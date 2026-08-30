#include "mod/EntityCleaner.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorCategory.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/DimensionType.h"

#include <croncpp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace my_mod::entity_cleaner {
namespace {

// Scheduled 清理全部候选；Threshold 只清到目标数量为止。
enum class CleanupKind { None, Threshold, Scheduled };

using Clock = std::chrono::steady_clock;

// 倒计时用 steady_clock：NTP 校时或夏令时会让 system_clock 跳变，
// 让「10 秒后清理」变成立刻执行或永远不执行。cron 判定仍需墙钟时间。
constexpr int MaxConsecutiveErrors = 5;

ll::event::ListenerPtr tickListener;
bool                   active = false;

// --- enable() 时固化的过滤器 ---
bool                            filterIsWhitelist = false;
std::unordered_set<std::string> filterTypeNames;
std::vector<HashedString>       filterFamilies;

// --- enable() 时净化过的数值，后续代码可以直接信任 ---
int   thresholdMax          = 0;
int   thresholdTarget       = 0;
int   thresholdCheckTicks   = 1;
int   thresholdCooldown     = 0;
float protectRadiusSquared  = 0.0F;
bool  protectRadiusEnabled  = false;

// --- 触发器状态 ---
bool           cronEnabled = false;
cron::cronexpr cronExpression;
std::time_t    cronNextRun{};

std::vector<int> warningMarks; // 降序、去重、全为正数
int              warningLeadSeconds = 0;

std::uint64_t tickCounter = 0;
std::time_t   lastCheckedSecond{};

CleanupKind       pendingKind = CleanupKind::None;
Clock::time_point pendingDeadline{};
std::size_t       nextWarningIndex = 0;

Clock::time_point lastThresholdCleanup{};
bool              hasThresholdCleanup = false;

int consecutiveErrors = 0;

// ---------------------------------------------------------------------------
// 第 1 层：按实体类型保护
//
// 一律用 getEntityTypeId() 精确比较或 hasCategory()，不用 hasType()。
// hasType 是 MCAPI 游戏函数，头文件里看不到实现，掩码语义（(a&b)==b 还是 !=0）
// 无从验证；而 ActorCategory.h 里 hasCategory 是 inline 的
// (mCategories & category) == category，语义确定。这一层默认保护的都是删掉就找不回来的
// 东西，不能押在未经验证的语义上。每一组都允许管理员显式关闭。
// ---------------------------------------------------------------------------

bool isTransientTypeProtected(ActorType type, EntityCleanerProtectionConfig const& protection) {
    switch (type) {
    case ActorType::FallingBlock:       // 正在下落的沙子/砂砾/铁砧
        return protection.fallingBlocks;
    case ActorType::MovingBlock:        // 活塞推动中的方块
        return protection.movingBlocks;
    case ActorType::FishingHook:        // 删除会破坏本项目自己的 auto_fishing 模块
        return protection.fishingHooks;
    case ActorType::OminousItemSpawner: // 试炼密室战利品
        return protection.ominousItemSpawners;
    default:
        return false;
    }
}

bool isTypeProtected(Actor const& actor) {
    auto const& protection = config.entityCleaner.protection;
    ActorType const type   = actor.getEntityTypeId();

    if (isTransientTypeProtected(type, protection)) {
        return true;
    }

    // Bedrock 没有 boss family（末影龙是 dragon/monster/mob，凋灵是
    // wither/undead/monster/mob），所以 Boss 只能在这里按类型保护。默认白名单里的
    // "monster" 恰好会把末影龙选为候选，全靠这一条救回来。
    if (protection.bosses && (type == ActorType::Dragon || type == ActorType::WitherBoss)) {
        return true;
    }

    // 必须同时列 Villager 和 VillagerV2：现代版本只生成 _v2，漏掉它等于删光所有村民。
    // 这里刻意不用 hasCategory(ActorCategory::Villager)——EvocationIllagerMonster
    // 定义为 Monster|Humanoid|Villager，会把卫道士/唤魔者一并保护，导致袭击农场堆积。
    if (protection.villagers
        && (type == ActorType::Villager || type == ActorType::VillagerV2 || type == ActorType::WanderingTrader)) {
        return true;
    }

    if (protection.builds) {
        switch (type) {
        case ActorType::ArmorStand:
        case ActorType::Painting:
        case ActorType::EnderCrystal:
        case ActorType::Npc: // 管理员放置的对话 NPC，删掉对冒险服是灾难
        case ActorType::LeashKnot: // 拴绳结：删掉会让整段栅栏上的拴绳全部断开
            return true;
        default:
            break;
        }
        // 矿车/船用 category 判定，一次覆盖 chest_minecart、hopper_minecart、
        // chest_boat 等全部变体——漏掉任何一个都可能删掉玩家存在里面的物品。
        if (actor.hasCategory(ActorCategory::Minecart) || actor.hasCategory(ActorCategory::Boat)) {
            return true;
        }
    }

    return false;
}

// 第 2 层：按运行时状态保护，与类型无关，两种 mode 下都生效。
bool isStateProtected(Actor const& actor) {
    auto const& protection = config.entityCleaner.protection;
    return (protection.named && !actor.getNameTag().empty())      //
        || (protection.tamed && actor.isTame())                   //
        || (protection.leashed && actor.isLeashed())              //
        || (protection.vehicles && (actor.hasPassenger() || actor.isRiding()))
        || (protection.persistent && actor.isPersistent());
}

// 第 3 层：管理员名单。
bool matchesFilter(Actor const& actor) {
    bool listed = filterTypeNames.contains(actor.getTypeName());
    if (!listed) {
        for (HashedString const& family : filterFamilies) {
            if (actor.hasFamily(family)) {
                listed = true;
                break;
            }
        }
    }
    return filterIsWhitelist ? listed : !listed;
}

struct Candidate {
    Actor* actor;
    float  distanceSquared; // 到同维度最近玩家的距离平方；该维度无玩家时为 infinity
};

bool isFinitePosition(Vec3 const& position) {
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

// 收集本轮可清理的实体。必须在**执行时刻**调用：把候选列表缓存到预告期（10 秒前）
// 再用，会让其中每个 Actor* 都可能变成悬垂指针。
std::vector<Candidate> collectCandidates(Level& level) {
    // 玩家坐标按维度分桶，距离只在同维度内比较。
    std::unordered_map<DimensionType, std::vector<Vec3>> playersByDimension;
    level.forEachPlayer([&playersByDimension](Player& player) {
        if (!player.isRemoved() && isFinitePosition(player.getPosition())) {
            playersByDimension[player.getDimensionId()].emplace_back(player.getPosition());
        }
        return true;
    });

    std::vector<Candidate> candidates;
    for (Actor* actor : level.getRuntimeActorList()) {
        // 玩家始终不进入清理候选，不能通过 entityCleaner 配置关闭这项保护。
        if (actor == nullptr || actor->isRemoved() || actor->isPlayer()) {
            continue;
        }
        if (isTypeProtected(*actor) || isStateProtected(*actor) || !matchesFilter(*actor)) {
            continue;
        }

        Vec3 const& position = actor->getPosition();
        // 非有限坐标会让距离变成 NaN，破坏 std::sort 的严格弱序并导致内省排序越界。
        // 这是整个模块最容易踩出硬崩溃的地方，必须在进入排序前就挡掉。
        if (!isFinitePosition(position)) {
            continue;
        }

        float      nearestSquared = std::numeric_limits<float>::infinity();
        auto const players        = playersByDimension.find(actor->getDimensionId());
        if (players != playersByDimension.end()) {
            for (Vec3 const& playerPosition : players->second) {
                float const dx = position.x - playerPosition.x;
                float const dy = position.y - playerPosition.y;
                float const dz = position.z - playerPosition.z;
                nearestSquared = std::min(nearestSquared, (dx * dx) + (dy * dy) + (dz * dz));
            }
        }

        if (protectRadiusEnabled && nearestSquared < protectRadiusSquared) {
            continue;
        }

        candidates.emplace_back(Candidate{actor, nearestSquared});
    }

    return candidates;
}

void broadcast(Level& level, std::string const& message) {
    level.forEachPlayer([&message](Player& player) {
        player.sendMessage(message);
        return true;
    });
}

void executeCleanup(Level& level, CleanupKind kind) {
    auto const& cleanerConfig = config.entityCleaner;
    auto        candidates    = collectCandidates(level);

    std::size_t removeCount = candidates.size();
    if (kind == CleanupKind::Threshold) {
        auto const target = static_cast<std::size_t>(thresholdTarget);
        // 候选数可能少于 target，减法必须防下溢。
        removeCount = candidates.size() > target ? candidates.size() - target : 0;
        if (removeCount > 0 && removeCount < candidates.size()) {
            // 离玩家最远的先删，玩家眼前的实体尽量留下。只需前 removeCount 个有序。
            std::partial_sort(
                candidates.begin(),
                candidates.begin() + static_cast<std::ptrdiff_t>(removeCount),
                candidates.end(),
                [](Candidate const& lhs, Candidate const& rhs) { return lhs.distanceSquared > rhs.distanceSquared; }
            );
        }
    }

    std::map<std::string, int> removedByType;
    int                        removed = 0;
    for (std::size_t i = 0; i < removeCount; ++i) {
        Actor* actor = candidates[i].actor;
        // 重新确认：collect 与本循环之间，级联移除（例如载具连带乘客）可能已经删掉它。
        if (actor == nullptr || actor->isRemoved()) {
            continue;
        }
        ++removedByType[actor->getTypeName()]; // 必须在 remove() 之前取名字
        ++removed;
        if (!cleanerConfig.dryRun) {
            actor->remove();
        }
    }

    if (cleanerConfig.dryRun) {
        logger.info(tr("entity_cleaner.dry_run_result", removed, candidates.size()));
    } else {
        logger.info(tr("entity_cleaner.result", removed, candidates.size()));
        if (removed > 0) {
            // 玩家只看一行汇总，按类型分组的明细留在日志里。
            broadcast(level, tr("entity_cleaner.broadcast_done", removed));
        }
    }

    if (cleanerConfig.logResult) {
        for (auto const& [typeName, count] : removedByType) {
            logger.info("- {}: {}", typeName, count);
        }
    }
}

void requestCleanup(CleanupKind kind) {
    if (pendingKind == CleanupKind::Scheduled) {
        return; // 已是最高优先级
    }
    if (pendingKind == CleanupKind::Threshold && kind != CleanupKind::Scheduled) {
        return;
    }

    // Scheduled 优先级更高，可以就地升级一个已 pending 的 Threshold。此时保留原有
    // deadline——倒计时已经播报过，不该往后顺延。
    bool const upgrading = pendingKind != CleanupKind::None;
    pendingKind          = kind;
    if (!upgrading) {
        pendingDeadline  = Clock::now() + std::chrono::seconds(warningLeadSeconds);
        nextWarningIndex = 0;
    }
}

void updatePending(Level& level) {
    if (pendingKind == CleanupKind::None) {
        return;
    }

    auto const now = Clock::now();
    if (now >= pendingDeadline) {
        CleanupKind const kind = pendingKind;
        pendingKind            = CleanupKind::None;
        nextWarningIndex       = 0;
        if (kind == CleanupKind::Threshold) {
            lastThresholdCleanup = now;
            hasThresholdCleanup  = true;
        }
        executeCleanup(level, kind);
        return;
    }

    auto const remaining = std::chrono::duration_cast<std::chrono::seconds>(pendingDeadline - now).count();

    // 卡顿时可能一次跨过好几个刻度。只播报最近的那一个，避免刷出「10 秒」「5 秒」
    // 一串已经过时的数字。
    std::size_t announce = warningMarks.size();
    while (nextWarningIndex < warningMarks.size()
           && remaining <= static_cast<long long>(warningMarks[nextWarningIndex])) {
        announce = nextWarningIndex;
        ++nextWarningIndex;
    }
    if (announce < warningMarks.size()) {
        broadcast(level, tr("entity_cleaner.warning", warningMarks[announce]));
    }
}

void checkThreshold(Level& level) {
    if (pendingKind != CleanupKind::None) {
        return;
    }

    // 冷却：上一轮若因候选不足而没能清到目标，避免每个周期都重新触发并刷屏广播。
    if (hasThresholdCleanup) {
        auto const since =
            std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - lastThresholdCleanup).count();
        if (since < static_cast<long long>(thresholdCooldown)) {
            return;
        }
    }

    // 比较的是「候选数」而不是世界实体总数：后者含玩家和全部受保护实体，用它做判据
    // 会让「2500 个受保护村民 + 500 只怪」的服务器永远触发、永远清不到目标。
    if (collectCandidates(level).size() > static_cast<std::size_t>(thresholdMax)) {
        requestCleanup(CleanupKind::Threshold);
    }
}

void tickImpl(ll::event::ServerLevelTickEvent& event) {
    Level& level = event.level();

    if (config.entityCleaner.threshold.enable
        && (++tickCounter % static_cast<std::uint64_t>(thresholdCheckTicks)) == 0) {
        checkThreshold(level);
    }

    // cron 与倒计时每墙钟秒最多跑一次（照抄 ScheduledCommand.cpp 的门控）。
    auto const nowSeconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (nowSeconds == lastCheckedSecond) {
        return;
    }
    lastCheckedSecond = nowSeconds;

    if (cronEnabled && nowSeconds >= cronNextRun) {
        cronNextRun = cron::cron_next(cronExpression, nowSeconds);
        requestCleanup(CleanupKind::Scheduled);
    }

    updatePending(level);
}

void onServerTick(ll::event::ServerLevelTickEvent& event) {
    if (!active) {
        return;
    }

    try {
        tickImpl(event);
        consecutiveErrors = 0;
    } catch (std::exception const& exception) {
        // tr() 底层是 fmt::format(fmt::runtime(...))，语言文件里占位符数量不匹配就会
        // 抛 fmt::format_error。在每 tick 执行的回调里放任异常逃逸会直接拖垮服务器。
        //
        // 这里刻意不用 tr()：它自己就是最可能的异常来源，在 catch 里再抛一次会直接
        // terminate。logger 的格式串是编译期检查的字面量，安全。
        ++consecutiveErrors;
        if (consecutiveErrors >= MaxConsecutiveErrors) {
            active = false;
            logger.error("Entity cleaner disabled after {} consecutive failures: {}", consecutiveErrors, exception.what());
        } else {
            logger.error("Entity cleaner tick failed: {}", exception.what());
        }
    }
}

void resetState() {
    active               = false;
    filterIsWhitelist    = false;
    filterTypeNames.clear();
    filterFamilies.clear();
    warningMarks.clear();
    warningLeadSeconds   = 0;
    thresholdMax         = 0;
    thresholdTarget      = 0;
    thresholdCheckTicks  = 1;
    thresholdCooldown    = 0;
    protectRadiusSquared = 0.0F;
    protectRadiusEnabled = false;
    cronEnabled          = false;
    cronNextRun          = 0;
    tickCounter          = 0;
    lastCheckedSecond    = 0;
    pendingKind          = CleanupKind::None;
    pendingDeadline      = {};
    nextWarningIndex     = 0;
    lastThresholdCleanup = {};
    hasThresholdCleanup  = false;
    consecutiveErrors    = 0;
}

} // namespace

bool isTypeProtectedByConfig(::Actor const& actor) { return isTypeProtected(actor); }

bool matchesConfiguredFilter(::Actor const& actor) {
    auto const& filter = config.entityCleaner.filter;
    if (filter.mode != "whitelist" && filter.mode != "blacklist") {
        return false;
    }

    bool listed = false;
    for (std::string const& typeName : filter.typeNames) {
        if (!typeName.empty() && typeName == actor.getTypeName()) {
            listed = true;
            break;
        }
    }
    if (!listed) {
        for (std::string const& familyName : filter.families) {
            if (familyName.empty()) {
                continue;
            }
            HashedString const family(familyName);
            if (actor.hasFamily(family)) {
                listed = true;
                break;
            }
        }
    }
    return filter.mode == "whitelist" ? listed : !listed;
}

bool enable() {
    auto const& cleanerConfig = config.entityCleaner;
    if (!cleanerConfig.enable || tickListener) {
        return true;
    }

    resetState();

    // 校验失败一律「记录日志 + 保持功能关闭 + return true」。
    // 绝不能 return false：Entry.cpp 的级联会把它当成插件加载失败，一个配置笔误就
    // 让整个插件起不来。scheduled_command::enable() 就是这个先例。
    std::string const& mode = cleanerConfig.filter.mode;
    if (mode != "whitelist" && mode != "blacklist") {
        // 绝不回退到 blacklist 语义——那等于「删除一切未列出的」。
        logger.error(tr("entity_cleaner.invalid_mode", mode));
        return true;
    }
    filterIsWhitelist = (mode == "whitelist");

    if (!cleanerConfig.schedule.enable && !cleanerConfig.threshold.enable) {
        logger.warn(tr("entity_cleaner.no_trigger"));
        return true;
    }

    if (cleanerConfig.schedule.enable) {
        try {
            cronExpression = cron::make_cron(cleanerConfig.schedule.cron);
            cronNextRun    = cron::cron_next(
                cronExpression,
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
            );
            cronEnabled = true;
        } catch (std::exception const& exception) {
            logger.error(tr("entity_cleaner.invalid_cron", cleanerConfig.schedule.cron, exception.what()));
        }
    }
    if (!cronEnabled && !cleanerConfig.threshold.enable) {
        return true; // 没有任何可用触发器
    }

    filterTypeNames.reserve(cleanerConfig.filter.typeNames.size());
    for (std::string const& typeName : cleanerConfig.filter.typeNames) {
        if (!typeName.empty()) {
            filterTypeNames.insert(typeName);
        }
    }

    // HashedString 的构造函数会立即计算哈希，所以在这里一次性建好缓存，避免每个实体
    // 每轮都重新构造。先 reserve 再填充：它的移动构造会连带 mLastMatch 指针，扩容
    // 搬迁会留下悬垂指针。
    filterFamilies.reserve(cleanerConfig.filter.families.size());
    for (std::string const& family : cleanerConfig.filter.families) {
        if (!family.empty()) {
            filterFamilies.emplace_back(family);
        }
    }

    if (filterIsWhitelist && filterTypeNames.empty() && filterFamilies.empty()) {
        // 白名单为空 = 什么都不清，没必要注册监听器。
        logger.warn(tr("entity_cleaner.empty_whitelist"));
        return true;
    }

    // 数值净化，让后续代码可以直接信任这些值。
    thresholdMax        = std::max(cleanerConfig.threshold.maxEntities, 0);
    thresholdTarget     = std::clamp(cleanerConfig.threshold.targetEntities, 0, std::max(thresholdMax - 1, 0));
    thresholdCheckTicks = std::max(cleanerConfig.threshold.checkIntervalTicks, 1);
    thresholdCooldown   = std::max(cleanerConfig.threshold.cooldownSeconds, 0);

    float const radius = cleanerConfig.protection.playerRadius;
    if (std::isfinite(radius) && radius > 0.0F) {
        float const radiusSquared = radius * radius;
        // A very large finite float can overflow while squaring. Treat that as
        // disabled instead of accidentally protecting every entity near a player.
        protectRadiusEnabled = std::isfinite(radiusSquared);
        protectRadiusSquared = protectRadiusEnabled ? radiusSquared : 0.0F;
    } else {
        protectRadiusEnabled = false;
        protectRadiusSquared = 0.0F;
    }

    // 预告刻度净化：去掉非正数、去重、降序。空列表直接取 max() 是 UB，而负数会把
    // deadline 排到过去，导致「-5 秒后清理」并立刻执行。
    if (cleanerConfig.warning.enable) {
        for (int const seconds : cleanerConfig.warning.secondsBefore) {
            if (seconds > 0) {
                warningMarks.push_back(seconds);
            }
        }
        std::sort(warningMarks.begin(), warningMarks.end(), std::greater<int>{});
        warningMarks.erase(std::unique(warningMarks.begin(), warningMarks.end()), warningMarks.end());
    }
    // 关闭预告或刻度为空 => 提前量为 0 => 触发即执行。
    warningLeadSeconds = warningMarks.empty() ? 0 : warningMarks.front();

    active       = true;
    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(onServerTick);
    if (!tickListener) {
        resetState();
        return false;
    }

    logger.info(tr("entity_cleaner.registered", mode, cleanerConfig.dryRun ? "on" : "off"));
    return true;
}

void disable() {
    if (tickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::ServerLevelTickEvent>(tickListener);
        tickListener.reset();
    }
    // 必须把 pending 状态一并清干净：残留的 deadline 会让下次 enable() 立刻执行一次
    // 没有任何预告的清理。
    resetState();
}

} // namespace my_mod::entity_cleaner

#else

namespace my_mod::entity_cleaner {

bool enable() { return true; }
void disable() {}
bool isTypeProtectedByConfig(::Actor const&) { return false; }
bool matchesConfiguredFilter(::Actor const&) { return false; }

} // namespace my_mod::entity_cleaner

#endif
