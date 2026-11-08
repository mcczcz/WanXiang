#include "mod/PerformanceMonitor.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/command/Command.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/memory/Hook.h"
#include "mc/deps/ecs/systems/IEntitySystemsCollection.h"
#include "mc/deps/ecs/systems/ITickingSystem.h"
#include "mc/deps/ecs/systems/SystemInfo.h"
#include "mc/deps/ecs/systems/TickingSystemId.h"
#include "mc/deps/ecs/systems/TickingSystemIterator.h"
#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/deps/vanilla_components/MoveRequestComponent.h"
#include "mc/deps/vanilla_components/block_source_visitor/CollisionShape.h"
#include "mc/entity/components/ActorOwnerComponent.h"
#include "mc/entity/components_json_legacy/EntitySensorComponent.h"
#include "mc/entity/components_json_legacy/EntitySubsensor.h"
#include "mc/entity/components_json_legacy/MobEffectComponent.h"
#include "mc/entity/components_json_legacy/ProjectileComponent.h"
#include "mc/entity/systems/EnvironmentSensorSystem.h"
#include "mc/entity/systems/EntitySystems.h"
#include "mc/entity/systems/GoalSelectorSystem.h"
#include "mc/entity/systems/MobEffectSystem.h"
#include "mc/entity/systems/NavigationSystem.h"
#include "mc/entity/systems/ProjectileSystem.h"
#include "mc/entity/systems/move_collision_system/MoveCollisionSystem.h"
#include "mc/platform/brstd/function_ref.h"
#include "mc/profile/ProfilerLite.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorFilterGroup.h"
#include "mc/world/actor/DistanceSortedActor.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/ai/goal/MeleeAttackBaseGoal.h"
#include "mc/world/actor/ai/goal/MoveToPOIGoal.h"
#include "mc/world/actor/ai/goal/RandomStrollGoal.h"
#include "mc/world/actor/ai/goal/WorkGoal.h"
#include "mc/world/actor/ai/goal/target/NearestAttackableTargetGoal.h"
#include "mc/world/actor/ai/goal/target/TargetGoal.h"
#include "mc/world/actor/ai/navigation/PathNavigation.h"
#include "mc/world/actor/ai/village/VillageManager.h"
#include "mc/world/actor/item/ItemActor.h"
#include "mc/world/actor/npc/VillagerV2.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace my_mod::performance_monitor {
namespace {

using Clock                          = std::chrono::steady_clock;
constexpr std::size_t MaxTickSamples = 12000;

enum class Phase : std::size_t {
    World,
    Entities,
    EntitySystems,
    ActorTick,
    Goals,
    SelectTarget,
    Nearby,
    Reach,
    MeleeMove,
    Navigation,
    Push,
    AiStep,
    Stroll,
    ActorMove,
    CollisionShapes,
    EntitySensor,
    EnvironmentSensor,
    Filters,
    EffectEmitterSystem,
    EffectEmitter,
    EffectUpdate,
    ProjectileSystem,
    ProjectileMove,
    ProjectileHoming,
    ProjectileHit,
    ItemPostTick,
    PathTick,
    PathUpdate,
    PathMovePosition,
    PathMoveTarget,
    TargetCanUse,
    TargetContinue,
    SpatialQuery,
    EcsSystem,
    VillagerAi,
    WorkCanUse,
    WorkTick,
    PoiCanUse,
    PoiGet,
    PoiReach,
    VillageTick,
    Count
};
constexpr auto                                PhaseCount = static_cast<std::size_t>(Phase::Count);
constexpr std::array<char const*, PhaseCount> PhaseKeys  = {
    "perf.phase.world",
    "perf.phase.entities",
    "perf.phase.systems",
    "perf.phase.actor",
    "perf.phase.goals",
    "perf.phase.target",
    "perf.phase.nearby",
    "perf.phase.reach",
    "perf.phase.melee",
    "perf.phase.navigation",
    "perf.phase.push",
    "perf.phase.ai_step",
    "perf.phase.stroll",
    "perf.phase.move",
    "perf.phase.collision",
    "perf.phase.sensor",
    "perf.phase.environment",
    "perf.phase.filters",
    "perf.phase.emitter_system",
    "perf.phase.emitter",
    "perf.phase.effect_update",
    "perf.phase.projectiles",
    "perf.phase.projectile_move",
    "perf.phase.homing",
    "perf.phase.projectile_hit",
    "perf.phase.item",
    "perf.phase.path_tick",
    "perf.phase.path_update",
    "perf.phase.path_position",
    "perf.phase.path_target",
    "perf.phase.target_start",
    "perf.phase.target_continue",
    "perf.phase.spatial_query",
    "perf.phase.ecs_uncovered",
    "perf.phase.villager_ai",
    "perf.phase.work_start",
    "perf.phase.work_tick",
    "perf.phase.poi_start",
    "perf.phase.poi_get",
    "perf.phase.poi_reach",
    "perf.phase.village_tick"
};

struct Metric {
    std::uint64_t calls        = 0;
    std::uint64_t units        = 0;
    std::uint64_t maxUnits     = 0;
    std::uint64_t trueResults  = 0;
    std::uint64_t falseResults = 0;
    double        inclusive    = 0;
    double        self         = 0;
    double        maximum      = 0;

    void add(double elapsed, double exclusive, std::uint64_t amount, int result) {
        ++calls;
        units        += amount;
        maxUnits      = std::max(maxUnits, amount);
        trueResults  += result == 1;
        falseResults += result == 0;
        inclusive    += elapsed;
        self         += exclusive;
        maximum       = std::max(maximum, elapsed);
    }
};

struct ChunkKey {
    int  dimension;
    int  x;
    int  z;
    bool operator==(ChunkKey const&) const = default;
};
struct ChunkHash {
    std::size_t operator()(ChunkKey const& key) const noexcept {
        auto seed  = std::hash<int>{}(key.dimension);
        seed      ^= std::hash<int>{}(key.x) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed ^ (std::hash<int>{}(key.z) + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    }
};
struct ChunkStats {
    std::array<double, PhaseCount> self{};
    std::uint64_t                  calls = 0;
    double                         total() const { return std::accumulate(self.begin(), self.end(), 0.0); }
};
struct Spike {
    std::uint64_t                  tick    = 0;
    double                         elapsed = 0;
    std::array<double, PhaseCount> self{};
};
struct PeakCall {
    bool         hasEntity = false;
    std::int64_t id        = 0;
    int          dimension = 0;
    Vec3         position{};
    std::string  type;
};

struct EntityStats {
    std::int64_t                   id = 0;
    std::string                    type;
    int                            dimension = 0;
    Vec3                           position{};
    std::array<Metric, PhaseCount> phases{};
    std::uint64_t                  sampledTicks = 0;
    std::uint64_t                  lastTick     = 0;

    double selfTime() const {
        double result = 0;
        for (auto const& metric : phases) result += metric.self;
        return result;
    }
};

struct SystemKey {
    std::uintptr_t collection;
    unsigned int   id;
    bool operator==(SystemKey const&) const = default;
};
struct SystemHash {
    std::size_t operator()(SystemKey const& key) const noexcept {
        auto seed = std::hash<std::uintptr_t>{}(key.collection);
        return seed ^ (std::hash<unsigned int>{}(key.id) + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    }
};
struct SystemStats {
    std::string                   name;
    Metric                        metric;
    std::array<double, PhaseCount> self{};
};

struct Capture {
    bool                                                recording = true;
    Clock::time_point                                   deadline;
    std::vector<double>                                 ticks;
    std::array<Metric, PhaseCount>                      phases{};
    std::array<double, PhaseCount>                      slowSelf{};
    std::array<PeakCall, PhaseCount>                    peaks{};
    std::unordered_map<std::int64_t, EntityStats>       entities;
    std::unordered_map<ChunkKey, ChunkStats, ChunkHash> chunks;
    std::vector<Spike>                                  spikes;
    std::unordered_map<SystemKey, SystemStats, SystemHash> systems;
    std::uint64_t                                       systemOverflowCalls = 0;
    std::uint64_t                                       unsupportedCollections = 0;
    std::uint64_t                                       chunkOverflowCalls = 0;
    std::uint64_t                                       sensorDue          = 0;
    std::uint64_t                                       sensorVisits       = 0;
    // Origins of _selectTarget calls, rather than its parent's inclusive time.
    std::array<Metric, 3> targetSources{};
    std::uint64_t         slowTicks         = 0;
    std::uint64_t         unattributedCalls = 0;
    std::uint64_t         strollAllowed     = 0;
    std::uint64_t         strollSkipped     = 0;
};

struct Runtime {
    bool                                            active = true;
    PerformanceMonitorConfig                        settings;
    std::unordered_set<std::string>                 strollTypes;
    std::unordered_map<std::int64_t, std::uint64_t> nextStrollTick;
    std::uint64_t                                   tick          = 0;
    std::uint64_t                                   strollAllowed = 0;
    std::uint64_t                                   strollSkipped = 0;
    std::shared_ptr<Capture>                        capture;
};

std::atomic_bool enabled{false};
// Only the outer world tick and commands take this lock, never each entity/goal.
// Recursive because server commands may execute inside that same world tick.
std::recursive_mutex     stateMutex;
std::shared_ptr<Runtime> runtime;
bool                     commandRegistered = false;

class Scope;
struct TickContext {
    Runtime& state;
    Capture* capture;
    Scope*   top = nullptr;
    SystemStats* system = nullptr;
    // 0=canUse, 1=canContinueToUse, 2=other/unobserved caller.
    std::size_t targetSource = 2;
};
thread_local TickContext* tickContext = nullptr;

class ContextGuard {
    TickContext* previous;

public:
    explicit ContextGuard(TickContext& context) : previous(tickContext) { tickContext = &context; }
    ~ContextGuard() { tickContext = previous; }
    ContextGuard(ContextGuard const&)            = delete;
    ContextGuard& operator=(ContextGuard const&) = delete;
};

EntityStats* trackActor(TickContext& context, Actor const* actor) {
    if (!actor || actor->mRemoved || !actor->hasUniqueID()) return nullptr;
    auto&      capture = *context.capture;
    auto const id      = actor->getOrCreateUniqueID().rawID;
    auto       found   = capture.entities.find(id);
    if (found == capture.entities.end()) {
        if (capture.entities.size() >= static_cast<std::size_t>(context.state.settings.maxTrackedEntities)) {
            ++capture.unattributedCalls;
            return nullptr;
        }
        found              = capture.entities.try_emplace(id).first;
        found->second.id   = id;
        found->second.type = actor->getTypeName();
    }
    // Copy before origin: the actor or its goals can be removed by callbacks.
    found->second.dimension = static_cast<int>(actor->getDimensionId());
    found->second.position  = actor->getPosition();
    if (found->second.lastTick != context.state.tick) {
        ++found->second.sampledTicks;
        found->second.lastTick = context.state.tick;
    }
    return &found->second;
}

ChunkStats* trackChunk(TickContext& context, int dimension, Vec3 const& position) {
    double const x = std::floor(static_cast<double>(position.x) / 16.0);
    double const z = std::floor(static_cast<double>(position.z) / 16.0);
    if (!std::isfinite(x) || !std::isfinite(z) || x < std::numeric_limits<int>::min()
        || x > std::numeric_limits<int>::max() || z < std::numeric_limits<int>::min()
        || z > std::numeric_limits<int>::max())
        return nullptr;
    ChunkKey const key{dimension, static_cast<int>(x), static_cast<int>(z)};
    auto&          capture = *context.capture;
    auto           it      = capture.chunks.find(key);
    if (it != capture.chunks.end()) return &it->second;
    if (capture.chunks.size() >= static_cast<std::size_t>(context.state.settings.maxTrackedEntities)) {
        ++capture.chunkOverflowCalls;
        return nullptr;
    }
    return &capture.chunks.try_emplace(key).first->second;
}

SystemStats* trackSystem(IEntitySystemsCollection& collection, TickingSystemId id) {
    auto& capture = *tickContext->capture;
    SystemKey const key{reinterpret_cast<std::uintptr_t>(&collection), id.value};
    auto found = capture.systems.find(key);
    if (found != capture.systems.end()) return &found->second;
    if (capture.systems.size() >= 2048) {
        ++capture.systemOverflowCalls;
        return nullptr;
    }
    auto const* info = collection.getSystemInfoForTickingSystemId(id);
    auto& stats = capture.systems.try_emplace(key).first->second;
    stats.name = info && !info->mName.empty() ? info->mName : "ECS #" + std::to_string(id.value);
    return &stats;
}

class Scope {
    TickContext*      context = nullptr;
    Scope*            parent  = nullptr;
    EntityStats*      entity  = nullptr;
    ChunkStats*       chunk   = nullptr;
    std::size_t       phase   = 0;
    Clock::time_point started;
    double            children     = 0;
    std::uint64_t     units        = 0;
    int               result       = -1;
    std::size_t       targetSource = 2;
    int               dimension    = 0;
    Vec3              position{};
    SystemStats*      system = nullptr;
    SystemStats*      previousSystem = nullptr;
    bool              systemRoot = false;

public:
    explicit Scope(Phase value, Actor const* actor = nullptr, SystemStats* ecs = nullptr) {
        if (!tickContext || !tickContext->capture || !tickContext->state.active) return;
        context = tickContext;
        phase   = static_cast<std::size_t>(value);
        systemRoot = value == Phase::EcsSystem;
        previousSystem = context->system;
        system = systemRoot ? ecs : context->system;
        if (systemRoot) context->system = system;
        entity  = trackActor(*context, actor);
        if (entity) {
            dimension = entity->dimension;
            position  = entity->position;
            chunk     = trackChunk(*context, dimension, position);
        }
        targetSource = context->targetSource;
        parent       = context->top;
        context->top = this;
        started      = Clock::now();
    }
    ~Scope() {
        if (!context) return;
        double const elapsed   = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        double const exclusive = std::max(0.0, elapsed - children);
        context->top           = parent;
        if (parent) parent->children += elapsed;
        if (elapsed > context->capture->phases[phase].maximum) {
            auto& peak     = context->capture->peaks[phase];
            peak.hasEntity = entity != nullptr;
            if (entity) {
                peak.id        = entity->id;
                peak.type      = entity->type;
                peak.dimension = dimension;
                peak.position  = position;
            }
        }
        context->capture->phases[phase].add(elapsed, exclusive, units, result);
        if (system) {
            system->self[phase] += exclusive;
            if (systemRoot) system->metric.add(elapsed, exclusive, units, result);
        }
        if (systemRoot) context->system = previousSystem;
        if (entity) entity->phases[phase].add(elapsed, exclusive, units, result);
        if (chunk) {
            chunk->self[phase] += exclusive;
            ++chunk->calls;
        }
        if (phase == static_cast<std::size_t>(Phase::SelectTarget)) {
            context->capture->targetSources[targetSource].add(elapsed, exclusive, units, result);
        }
    }
    void setUnits(std::uint64_t value) { units = value; }
    bool returned(bool value) {
        result = value ? 1 : 0;
        return value;
    }
    void locate(int dimension, Vec3 const& position) {
        if (context && !entity) chunk = trackChunk(*context, dimension, position);
    }
    void inheritCaller() {
        if (!entity && parent) {
            entity    = parent->entity;
            chunk     = parent->chunk;
            dimension = parent->dimension;
            position  = parent->position;
        }
    }
    Scope(Scope const&)            = delete;
    Scope& operator=(Scope const&) = delete;
};

class TargetSourceGuard {
    TickContext* context  = nullptr;
    std::size_t  previous = 2;

public:
    explicit TargetSourceGuard(std::size_t source) : context(tickContext) {
        if (context) {
            previous              = context->targetSource;
            context->targetSource = source;
        }
    }
    ~TargetSourceGuard() {
        if (context) context->targetSource = previous;
    }
    TargetSourceGuard(TargetSourceGuard const&)            = delete;
    TargetSourceGuard& operator=(TargetSourceGuard const&) = delete;
};

bool profiling() { return tickContext && tickContext->capture && tickContext->state.active; }

std::uintptr_t ecsTickAddress = 0;
std::uintptr_t ecsSingleTickAddress = 0;
void installCollectionProbes(IEntitySystemsCollection& collection);

template <class Member>
std::optional<unsigned int> collectionVtableSlot(Member member) {
    if (auto slot = ll::memory::getVtableIndex(member)) return slot;
    if constexpr (sizeof(Member) == sizeof(ll::memory::FuncPtr)) {
        // Clang also emits mov rax,[rcx]; mov rax,[rax+disp8]; jmp rax.
        // LeviLamina 26.51.1 only decodes the disp32 variant of that thunk.
        auto const* code = static_cast<unsigned char const*>(ll::memory::toFuncPtr(member));
        constexpr std::array<unsigned char, 6> prefix{0x48, 0x8b, 0x01, 0x48, 0x8b, 0x40};
        constexpr std::array<unsigned char, 3> suffix{0x48, 0xff, 0xe0};
        if (std::equal(prefix.begin(), prefix.end(), code)
            && std::equal(suffix.begin(), suffix.end(), code + 7)
            && code[6] < 0x80 && code[6] % sizeof(void*) == 0) {
            return static_cast<unsigned int>(code[6] / sizeof(void*));
        }
    }
    return std::nullopt;
}

bool deferStroll(Mob& mob) {
    if (!tickContext || !tickContext->state.active) return false;
    auto& state = tickContext->state;
    if (!state.settings.optimization.enable || state.strollTypes.empty()) return false;
    if (!state.strollTypes.contains(mob.getTypeName()) || mob.mRemoved || mob.isPlayer() || !mob.hasUniqueID()
        || mob.mTargetId->rawID != ActorUniqueID::INVALID_ID().rawID || mob.isTame() || mob.isLeashed()
        || mob.hasPassenger() || mob.getVehicle() != nullptr || !mob.getNameTag().empty())
        return false;

    auto const id    = mob.getOrCreateUniqueID().rawID;
    auto       found = state.nextStrollTick.find(id);
    if (found == state.nextStrollTick.end()) {
        // Saturation fails open. No unbounded cache or stored Actor/Goal pointers.
        if (state.nextStrollTick.size() >= static_cast<std::size_t>(state.settings.maxTrackedEntities)) return false;
        found = state.nextStrollTick.try_emplace(id, 0).first;
    }
    if (state.tick < found->second) {
        ++state.strollSkipped;
        if (profiling()) ++tickContext->capture->strollSkipped;
        return true;
    }
    found->second = state.tick + static_cast<std::uint64_t>(state.settings.optimization.strollIntervalTicks);
    ++state.strollAllowed;
    if (profiling()) ++tickContext->capture->strollAllowed;
    return false;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

LL_TYPE_INSTANCE_HOOK(WorldTickHook, ll::memory::HookPriority::Normal, Level, &Level::$tick, void) {
    if (!enabled.load(std::memory_order_relaxed) || tickContext) {
        origin();
        return;
    }
    std::lock_guard lock(stateMutex);
    auto            state = runtime;
    if (!state || !state->active) {
        origin();
        return;
    }
    ++state->tick;
    if (state->tick % 1200 == 0) state->nextStrollTick.clear();
    // Shared ownership keeps in-flight scopes valid if reload/start/stop executes in origin.
    auto                           capture   = state->capture;
    bool const                     recording = capture && capture->recording;
    TickContext                    context{*state, recording ? capture.get() : nullptr};
    ContextGuard                   guard(context);
    std::array<double, PhaseCount> before{};
    if (recording) {
        auto& systems = this->getEntitySystems();
        if (systems.mSystems) installCollectionProbes(*systems.mSystems);
        for (std::size_t i = 0; i < PhaseCount; ++i) before[i] = capture->phases[i].self;
    }
    double const previousWorld = recording ? capture->phases[0].inclusive : 0;
    {
        Scope scope(Phase::World);
        origin();
    }
    if (!recording) return;
    double const elapsed = capture->phases[0].inclusive - previousWorld;
    capture->ticks.push_back(elapsed);
    if (elapsed >= state->settings.slowTickMs) {
        ++capture->slowTicks;
        Spike spike;
        spike.tick    = state->tick;
        spike.elapsed = elapsed;
        for (std::size_t i = 0; i < PhaseCount; ++i) {
            spike.self[i]         = capture->phases[i].self - before[i];
            capture->slowSelf[i] += spike.self[i];
        }
        if (capture->spikes.size() < 20) capture->spikes.push_back(spike);
        else {
            auto smallest =
                std::min_element(capture->spikes.begin(), capture->spikes.end(), [](auto const& a, auto const& b) {
                    return a.elapsed < b.elapsed;
                });
            if (spike.elapsed > smallest->elapsed) *smallest = spike;
        }
    }
    if (capture->recording && (Clock::now() >= capture->deadline || capture->ticks.size() >= MaxTickSamples)) {
        capture->recording = false;
        logger.info(tr("perf.finished", capture->ticks.size(), capture->slowTicks));
    }
}

LL_TYPE_INSTANCE_HOOK(EntitiesHook, ll::memory::HookPriority::Normal, Level, &Level::$tickEntities, void) {
    Scope scope(Phase::Entities);
    origin();
}
LL_TYPE_INSTANCE_HOOK(SystemsHook, ll::memory::HookPriority::Normal, Level, &Level::$tickEntitySystems, void) {
    Scope scope(Phase::EntitySystems);
    origin();
}

LL_TYPE_INSTANCE_HOOK(
    EcsDispatchHook,
    ll::memory::HookPriority::Normal,
    EntitySystems,
    &EntitySystems::tick,
    void,
    EntityRegistry& registry
) {
    if (profiling() && mSystems) installCollectionProbes(*mSystems);
    origin(registry);
}

using SystemCallback = brstd::function_ref<void(ITickingSystem&, TickingSystemId const&)>;

// 26.60 passes one iterator instead of (filter, std::function, captureTimings).
// The SDK leaves its fields untyped. BDS 1.26.62.1 ECS dispatch
// (0x14304B5C0) builds this layout; 0x143057C90 receives the system ID by reference.
// Keep the filter and optional single-tick callback opaque and unchanged.
struct TickingSystemIteratorAbi {
    std::array<std::byte, 8>  filter;
    SystemCallback            callback;
    std::array<std::byte, 24> singleTickCallback;
    bool                      captureTimings;
};
static_assert(sizeof(SystemCallback) == 16);
static_assert(sizeof(TickingSystemIteratorAbi) == sizeof(TickingSystemIterator));
static_assert(alignof(TickingSystemIteratorAbi) == alignof(TickingSystemIterator));
static_assert(offsetof(TickingSystemIteratorAbi, filter) == offsetof(TickingSystemIterator, mUnk3cc911));
static_assert(offsetof(TickingSystemIteratorAbi, callback) == offsetof(TickingSystemIterator, mUnke5e641));
static_assert(offsetof(TickingSystemIteratorAbi, singleTickCallback) == offsetof(TickingSystemIterator, mUnk29e8d6));
static_assert(offsetof(TickingSystemIteratorAbi, captureTimings) == offsetof(TickingSystemIterator, mUnkf6c4f3));

LL_TYPE_INSTANCE_HOOK(
    EcsForeachHook,
    ll::memory::HookPriority::Normal,
    IEntitySystemsCollection,
    ecsTickAddress,
    void,
    TickingSystemIteratorAbi const& iterator
) {
    if (!profiling()) {
        origin(iterator);
        return;
    }
    auto callback = [&](ITickingSystem& system, TickingSystemId const& id) {
        if (!profiling()) {
            iterator.callback(system, id);
            return;
        }
        Scope scope(Phase::EcsSystem, nullptr, trackSystem(*this, id));
        iterator.callback(system, id);
    };
    auto measured     = iterator;
    measured.callback = SystemCallback{callback};
    origin(measured);
}
LL_TYPE_INSTANCE_HOOK(
    EcsSingleForeachHook,
    ll::memory::HookPriority::Normal,
    IEntitySystemsCollection,
    ecsSingleTickAddress,
    void,
    TickingSystemIteratorAbi const& iterator
) {
    if (!profiling()) {
        origin(iterator);
        return;
    }
    auto callback = [&](ITickingSystem& system, TickingSystemId const& id) {
        if (!profiling()) {
            iterator.callback(system, id);
            return;
        }
        Scope scope(Phase::EcsSystem, nullptr, trackSystem(*this, id));
        iterator.callback(system, id);
    };
    auto measured     = iterator;
    measured.callback = SystemCallback{callback};
    origin(measured);
}
LL_TYPE_INSTANCE_HOOK(ActorTickHook, ll::memory::HookPriority::Normal, Actor, &Actor::tick, bool, BlockSource& region) {
    Scope scope(Phase::ActorTick, profiling() ? this : nullptr);
    return origin(region);
}
LL_TYPE_STATIC_HOOK(
    GoalsHook,
    ll::memory::HookPriority::Normal,
    GoalSelectorSystem,
    &GoalSelectorSystem::_tickGoalSelectorComponent,
    void,
    ActorOwnerComponent& owner
) {
    Scope scope(Phase::Goals, profiling() ? owner.mActor.get() : nullptr);
    origin(owner);
}
LL_TYPE_INSTANCE_HOOK(
    SelectTargetHook,
    ll::memory::HookPriority::Normal,
    NearestAttackableTargetGoal,
    &NearestAttackableTargetGoal::_selectTarget,
    bool
) {
    Scope scope(Phase::SelectTarget, profiling() ? &mMob : nullptr);
    return scope.returned(origin());
}

LL_TYPE_INSTANCE_HOOK(VillagerAiHook, ll::memory::HookPriority::Normal,
                      VillagerV2, &VillagerV2::$newServerAiStep, void) {
    Scope scope(Phase::VillagerAi, profiling() ? this : nullptr);
    origin();
}
LL_TYPE_INSTANCE_HOOK(WorkCanUseHook, ll::memory::HookPriority::Normal, WorkGoal, &WorkGoal::$canUse, bool) {
    Scope scope(Phase::WorkCanUse, profiling() ? &mMob : nullptr);
    return scope.returned(origin());
}
LL_TYPE_INSTANCE_HOOK(WorkTickHook, ll::memory::HookPriority::Normal, WorkGoal, &WorkGoal::$tick, void) {
    Scope scope(Phase::WorkTick, profiling() ? &mMob : nullptr);
    origin();
}
LL_TYPE_INSTANCE_HOOK(PoiCanUseHook, ll::memory::HookPriority::Normal, MoveToPOIGoal, &MoveToPOIGoal::$canUse, bool) {
    Scope scope(Phase::PoiCanUse, profiling() ? &mMob : nullptr);
    return scope.returned(origin());
}
LL_TYPE_INSTANCE_HOOK(PoiGetHook, ll::memory::HookPriority::Normal, MoveToPOIGoal, &MoveToPOIGoal::getPOI,
                      bool, POIType type) {
    Scope scope(Phase::PoiGet, profiling() ? &mMob : nullptr);
    return scope.returned(origin(type));
}
LL_TYPE_INSTANCE_HOOK(PoiReachHook, ll::memory::HookPriority::Normal, MoveToPOIGoal, &MoveToPOIGoal::_canReachPOI,
                      bool, Vec3 const& position, float radiusSquared, bool usingBoundingBox) {
    Scope scope(Phase::PoiReach, profiling() ? &mMob : nullptr);
    return scope.returned(origin(position, radiusSquared, usingBoundingBox));
}
LL_TYPE_INSTANCE_HOOK(VillageTickHook, ll::memory::HookPriority::Normal, VillageManager, &VillageManager::tickVillages,
                      void, Tick const& tick, Vec3 const& position, BlockSource& region) {
    Scope scope(Phase::VillageTick);
    if (profiling()) scope.locate(static_cast<int>(region.getDimensionId()), position);
    origin(tick, position, region);
}
using NearbyActors = std::vector<DistanceSortedActor>;
LL_TYPE_INSTANCE_HOOK(
    NearbyHook,
    ll::memory::HookPriority::Normal,
    Actor,
    &Actor::fetchNearbyActorsSorted,
    NearbyActors,
    Vec3 const& distance,
    ActorType   type
) {
    Scope scope(Phase::Nearby, profiling() ? this : nullptr);
    auto  result = origin(distance, type);
    scope.setUnits(static_cast<std::uint64_t>(result.size()));
    return result;
}
LL_TYPE_INSTANCE_HOOK(
    ReachHook,
    ll::memory::HookPriority::Normal,
    TargetGoal,
    &TargetGoal::_canReach,
    bool,
    Actor& target
) {
    Scope scope(Phase::Reach, profiling() ? &mMob : nullptr);
    return scope.returned(origin(target));
}
LL_TYPE_INSTANCE_HOOK(
    MeleeMoveHook,
    ll::memory::HookPriority::Normal,
    MeleeAttackBaseGoal,
    &MeleeAttackBaseGoal::_attemptMoveToTargetPosition,
    void,
    Actor const& target,
    bool         canReach,
    float        distanceSquared
) {
    Scope scope(Phase::MeleeMove, profiling() ? &mMob : nullptr);
    origin(target, canReach, distanceSquared);
}
LL_TYPE_INSTANCE_HOOK(
    NavigationHook,
    ll::memory::HookPriority::Normal,
    NavigationSystem,
    &NavigationSystem::$tick,
    void,
    EntityRegistry& registry
) {
    Scope scope(Phase::Navigation);
    origin(registry);
}
LL_TYPE_INSTANCE_HOOK(PushHook, ll::memory::HookPriority::Normal, Mob, &Mob::$pushActors, void) {
    Scope scope(Phase::Push, profiling() ? this : nullptr);
    origin();
}
LL_TYPE_INSTANCE_HOOK(AiStepHook, ll::memory::HookPriority::Normal, Mob, &Mob::$aiStep, void) {
    Scope scope(Phase::AiStep, profiling() ? this : nullptr);
    origin();
}
LL_TYPE_INSTANCE_HOOK(
    StrollHook,
    ll::memory::HookPriority::Normal,
    RandomStrollGoal,
    &RandomStrollGoal::$canUse,
    bool
) {
    if (deferStroll(mMob)) return false;
    Scope scope(Phase::Stroll, profiling() ? &mMob : nullptr);
    return origin();
}

LL_TYPE_INSTANCE_HOOK(ActorMoveHook, ll::memory::HookPriority::Normal, Actor, &Actor::move, void, Vec3 const& delta) {
    Scope scope(Phase::ActorMove, profiling() ? this : nullptr);
    origin(delta);
}

// This ECS function has no ActorOwnerComponent. Attribute it to the request's
// starting chunk, not to an arbitrary actor that happens to be on the stack.
using CollidableView = ViewT<StrictEntityContext, Include<CollidableMobFlagComponent>, AABBShapeComponent const>;
using StackableView  = ViewT<StrictEntityContext, AABBShapeComponent const, ActorDataFlagComponent const>;
using FallingView    = ViewT<StrictEntityContext, Include<FallingBlockFlagComponent>>;
LL_STATIC_HOOK(
    CollisionHook,
    ll::memory::HookPriority::Normal,
    &MoveCollisionSystem::fetchCollisionShapes,
    void,
    StrictEntityContext const&                       entity,
    AABBShapeComponent const&                        aabb,
    MaxAutoStepComponent const&                      step,
    Optional<CollidableMobNearFlagComponent const>   nearby,
    MoveRequestComponent&                            request,
    Optional<MinecartFlagComponent const>            minecart,
    CollidableView const&                            mobs,
    StackableView const&                             stackable,
    FallingView const&                               falling,
    IConstBlockSource const&                         region,
    LocalSpatialEntityFetcher&                       fetcher,
    GetCollisionShapeInterface const&                shapeInterface,
    std::vector<BlockSourceVisitor::CollisionShape>& shapes,
    std::vector<BlockSourceVisitor::CollisionShape>& scratch,
    std::vector<AABB>&                               temp
) {
    Scope scope(Phase::CollisionShapes);
    if (profiling()) scope.locate(static_cast<int>(region.getDimensionId()), request.mOriginalAABBShape->min);
    origin(
        entity,
        aabb,
        step,
        nearby,
        request,
        minecart,
        mobs,
        stackable,
        falling,
        region,
        fetcher,
        shapeInterface,
        shapes,
        scratch,
        temp
    );
    if (profiling()) scope.setUnits(static_cast<std::uint64_t>(request.mCollisionShapes->mShapes->size()));
}

LL_TYPE_INSTANCE_HOOK(
    SensorHook,
    ll::memory::HookPriority::Normal,
    EntitySensorComponent,
    &EntitySensorComponent::tick,
    void,
    ActorOwnerComponent& owner,
    Tick const&          currentTick
) {
    Scope scope(Phase::EntitySensor, profiling() ? owner.mActor.get() : nullptr);
    if (profiling()) {
        std::uint64_t due = 0;
        for (auto const& sensor : *mSubsensors) {
            auto const& next = *sensor.mNextSensingTick;
            if (!next || next->tickID <= currentTick.tickID) ++due;
        }
        scope.setUnits(due);
        tickContext->capture->sensorDue    += due;
        tickContext->capture->sensorVisits += static_cast<std::uint64_t>(mSubsensors->size());
    }
    origin(owner, currentTick);
}
LL_TYPE_INSTANCE_HOOK(
    EnvironmentHook,
    ll::memory::HookPriority::Normal,
    EnvironmentSensorSystem,
    &EnvironmentSensorSystem::$tick,
    void,
    EntityRegistry& registry
) {
    Scope scope(Phase::EnvironmentSensor);
    origin(registry);
}
LL_TYPE_INSTANCE_HOOK(
    FilterHook,
    ll::memory::HookPriority::Normal,
    ActorFilterGroup,
    &ActorFilterGroup::evaluateActor,
    bool,
    Actor const&                     actor,
    VariantParameterListConst const& params
) {
    Scope scope(Phase::Filters, profiling() ? &actor : nullptr);
    return scope.returned(origin(actor, params));
}
LL_TYPE_INSTANCE_HOOK(
    EmitterSystemHook,
    ll::memory::HookPriority::Normal,
    MobEffectSystem,
    &MobEffectSystem::$tick,
    void,
    EntityRegistry& registry
) {
    Scope scope(Phase::EffectEmitterSystem);
    origin(registry);
}
LL_TYPE_INSTANCE_HOOK(
    EmitterHook,
    ll::memory::HookPriority::Normal,
    MobEffectComponent,
    &MobEffectComponent::tick,
    void,
    Actor& actor
) {
    Scope scope(Phase::EffectEmitter, profiling() ? &actor : nullptr);
    origin(actor);
}
LL_TYPE_INSTANCE_HOOK(
    EffectUpdateHook,
    ll::memory::HookPriority::Normal,
    MobEffectInstance,
    &MobEffectInstance::updateEffects,
    void,
    Actor& actor
) {
    Scope scope(Phase::EffectUpdate, profiling() ? &actor : nullptr);
    origin(actor);
}
LL_TYPE_INSTANCE_HOOK(
    ProjectileSystemHook,
    ll::memory::HookPriority::Normal,
    ProjectileSystem,
    &ProjectileSystem::$tick,
    void,
    EntityRegistry& registry
) {
    Scope scope(Phase::ProjectileSystem);
    origin(registry);
}
LL_TYPE_INSTANCE_HOOK(
    ProjectileMoveHook,
    ll::memory::HookPriority::Normal,
    ProjectileComponent,
    &ProjectileComponent::handleMovementAndGravity,
    void,
    Actor& owner,
    bool   skipPositionUpdate
) {
    Scope scope(Phase::ProjectileMove, profiling() ? &owner : nullptr);
    origin(owner, skipPositionUpdate);
}
LL_TYPE_INSTANCE_HOOK(
    HomingHook,
    ll::memory::HookPriority::Normal,
    ProjectileComponent,
    &ProjectileComponent::handleMovementHoming,
    void,
    Actor& owner
) {
    Scope scope(Phase::ProjectileHoming, profiling() ? &owner : nullptr);
    origin(owner);
}
LL_TYPE_INSTANCE_HOOK(
    ProjectileHitHook,
    ll::memory::HookPriority::Normal,
    ProjectileComponent,
    &ProjectileComponent::onHit,
    void,
    Actor&           owner,
    HitResult const& hit
) {
    Scope scope(Phase::ProjectileHit, profiling() ? &owner : nullptr);
    origin(owner, hit);
}
LL_TYPE_INSTANCE_HOOK(ItemHook, ll::memory::HookPriority::Normal, ItemActor, &ItemActor::postNormalTick, void) {
    Scope scope(Phase::ItemPostTick, profiling() ? this : nullptr);
    origin();
}
LL_TYPE_INSTANCE_HOOK(
    PathTickHook,
    ll::memory::HookPriority::Normal,
    PathNavigation,
    &PathNavigation::$tick,
    void,
    NavigationComponent& navigation,
    Mob&                 mob
) {
    Scope scope(Phase::PathTick, profiling() ? &mob : nullptr);
    origin(navigation, mob);
}
LL_TYPE_INSTANCE_HOOK(
    PathUpdateHook,
    ll::memory::HookPriority::Normal,
    PathNavigation,
    &PathNavigation::$updatePath,
    void,
    NavigationComponent& navigation,
    Mob&                 mob
) {
    Scope scope(Phase::PathUpdate, profiling() ? &mob : nullptr);
    origin(navigation, mob);
}
auto const MoveToPosition =
    static_cast<bool (PathNavigation::*)(NavigationComponent&, Mob&, Vec3 const&, float)>(&PathNavigation::$moveTo);
auto const MoveToTarget =
    static_cast<bool (PathNavigation::*)(NavigationComponent&, Mob&, Actor const&, float)>(&PathNavigation::$moveTo);
LL_TYPE_INSTANCE_HOOK(
    PathPositionHook,
    ll::memory::HookPriority::Normal,
    PathNavigation,
    MoveToPosition,
    bool,
    NavigationComponent& navigation,
    Mob&                 mob,
    Vec3 const&          position,
    float                speed
) {
    Scope scope(Phase::PathMovePosition, profiling() ? &mob : nullptr);
    return scope.returned(origin(navigation, mob, position, speed));
}
LL_TYPE_INSTANCE_HOOK(
    PathTargetHook,
    ll::memory::HookPriority::Normal,
    PathNavigation,
    MoveToTarget,
    bool,
    NavigationComponent& navigation,
    Mob&                 mob,
    Actor const&         target,
    float                speed
) {
    Scope scope(Phase::PathMoveTarget, profiling() ? &mob : nullptr);
    return scope.returned(origin(navigation, mob, target, speed));
}
LL_TYPE_INSTANCE_HOOK(
    TargetCanUseHook,
    ll::memory::HookPriority::Normal,
    NearestAttackableTargetGoal,
    &NearestAttackableTargetGoal::$canUse,
    bool
) {
    TargetSourceGuard source(0);
    Scope             scope(Phase::TargetCanUse, profiling() ? &mMob : nullptr);
    return scope.returned(origin());
}
LL_TYPE_INSTANCE_HOOK(
    TargetContinueHook,
    ll::memory::HookPriority::Normal,
    NearestAttackableTargetGoal,
    &NearestAttackableTargetGoal::$canContinueToUse,
    bool
) {
    TargetSourceGuard source(1);
    Scope             scope(Phase::TargetContinue, profiling() ? &mMob : nullptr);
    return scope.returned(origin());
}
using ActorSpan = gsl::span<gsl::not_null<Actor*>>;
auto const FetchActors =
    static_cast<ActorSpan (BlockSource::*)(Actor const*, AABB const&, bool, bool)>(&BlockSource::$fetchEntities);
LL_TYPE_INSTANCE_HOOK(
    SpatialQueryHook,
    ll::memory::HookPriority::Normal,
    BlockSource,
    FetchActors,
    ActorSpan,
    Actor const* except,
    AABB const&  box,
    bool         hitbox,
    bool         display
) {
    Scope scope(Phase::SpatialQuery);
    scope.inheritCaller();
    auto result = origin(except, box, hitbox, display);
    scope.setUnits(static_cast<std::uint64_t>(result.size()));
    return result;
}

#pragma clang diagnostic pop

struct Probe {
    char const* name;
    int (*install)();
    bool (*remove)();
    bool installed = false;
    bool deferred = false;
    bool attempted = false;
    char const* failure = "perf.probe.not_attempted";
    int installResult = -1;
};
template <class Hook>
Probe probe(char const* name, bool deferred = false) {
    return {name, [] { return Hook::hook(); }, [] { return Hook::unhook(); }, false, deferred};
}
std::array probes = {
    probe<WorldTickHook>("Level::tick"),
    probe<EntitiesHook>("Level::tickEntities"),
    probe<SystemsHook>("Level::tickEntitySystems"),
    probe<EcsDispatchHook>("EntitySystems::tick"),
    probe<EcsForeachHook>("ECS::foreachTickingSystem", true),
    probe<EcsSingleForeachHook>("ECS::foreachSingleTickingSystem", true),
    probe<ActorTickHook>("Actor::tick"),
    probe<GoalsHook>("GoalSelector"),
    probe<SelectTargetHook>("SelectTarget"),
    probe<VillagerAiHook>("VillagerV2::newServerAiStep"),
    probe<WorkCanUseHook>("WorkGoal::canUse"),
    probe<WorkTickHook>("WorkGoal::tick"),
    probe<PoiCanUseHook>("MoveToPOIGoal::canUse"),
    probe<PoiGetHook>("MoveToPOIGoal::getPOI"),
    probe<PoiReachHook>("MoveToPOIGoal::_canReachPOI"),
    probe<VillageTickHook>("VillageManager::tickVillages"),
    probe<NearbyHook>("NearbyActorsSorted"),
    probe<ReachHook>("TargetReach"),
    probe<MeleeMoveHook>("MeleeMove"),
    probe<NavigationHook>("Navigation"),
    probe<PushHook>("PushActors"),
    probe<AiStepHook>("Mob::aiStep"),
    probe<ActorMoveHook>("Actor::move"),
    probe<CollisionHook>("MoveCollisionSystem::fetchCollisionShapes"),
    probe<SensorHook>("EntitySensorComponent::tick"),
    probe<EnvironmentHook>("EnvironmentSensorSystem::tick"),
    probe<FilterHook>("ActorFilterGroup::evaluateActor"),
    probe<EmitterSystemHook>("MobEffectSystem::tick"),
    probe<EmitterHook>("MobEffectComponent::tick"),
    probe<EffectUpdateHook>("MobEffectInstance::updateEffects"),
    probe<ProjectileSystemHook>("ProjectileSystem::tick"),
    probe<ProjectileMoveHook>("Projectile::movement"),
    probe<HomingHook>("Projectile::homing"),
    probe<ProjectileHitHook>("Projectile::onHit"),
    probe<ItemHook>("ItemActor::postNormalTick"),
    probe<PathTickHook>("PathNavigation::tick"),
    probe<PathUpdateHook>("PathNavigation::updatePath"),
    probe<PathPositionHook>("PathNavigation::moveTo(position)"),
    probe<PathTargetHook>("PathNavigation::moveTo(target)"),
    probe<TargetCanUseHook>("NearestTarget::canUse"),
    probe<TargetContinueHook>("NearestTarget::canContinueToUse"),
    probe<SpatialQueryHook>("BlockSource::fetchEntities(AABB)"),
    // Keep this last: enable() uses it to gate the optional stroll optimization.
    probe<StrollHook>("RandomStroll")
};

void installCollectionProbes(IEntitySystemsCollection& collection) {
    // Wrapping the engine callback preserves iteration order and filters.
    static auto const tickSlot = collectionVtableSlot(&IEntitySystemsCollection::foreachTickingSystem);
    static auto const singleSlot = collectionVtableSlot(&IEntitySystemsCollection::foreachSingleTickingSystem);
    auto const* table = ll::memory::dAccess<ll::memory::FuncPtr const*>(&collection, 0);
    auto install = [&](char const* name, auto slot, std::uintptr_t& address) {
        auto found = std::find_if(probes.begin(), probes.end(), [&](auto const& item) {
            return std::string_view(item.name) == name;
        });
        if (found == probes.end()) return;
        auto const candidate = slot ? reinterpret_cast<std::uintptr_t>(table[*slot]) : 0;
        if (found->installed) {
            if (candidate != address) ++tickContext->capture->unsupportedCollections;
            return;
        }
        if (found->attempted) return;
        found->attempted = true;
        found->failure = !slot ? "perf.probe.slot" : !candidate ? "perf.probe.address" : "perf.probe.changed";
        if (candidate && (!address || address == candidate)) {
            address = candidate;
            found->failure = "perf.probe.hook";
            try {
                found->installResult = found->install();
                found->installed = found->installResult == 0;
            } catch (std::exception const& exception) {
                found->failure = "perf.probe.exception";
                logger.warn(tr("perf.error", exception.what()));
            }
        }
        if (!found->installed)
            logger.warn(tr("perf.ecs_probe_error", name, tr(found->failure), found->installResult));
        else logger.info(tr("perf.ecs_probe_ready", name, *slot));
    };
    install("ECS::foreachTickingSystem", tickSlot, ecsTickAddress);
    install("ECS::foreachSingleTickingSystem", singleSlot, ecsSingleTickAddress);
}

bool permitted(CommandOrigin const& origin, CommandOutput& output) {
    // Keep profiling restricted even when commandPermissions lowers the registration level.
    if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors) {
        output.error(tr("perf.op_only"));
        return false;
    }
    if (!runtime || !runtime->active) {
        output.error(tr("perf.disabled"));
        return false;
    }
    return true;
}

void startCapture(CommandOrigin const& origin, CommandOutput& output, int seconds) {
    std::lock_guard lock(stateMutex);
    if (!permitted(origin, output)) return;
    if (seconds == 0) seconds = runtime->settings.captureSeconds;
    if (seconds < 1 || seconds > 600) {
        output.error(tr("perf.duration"));
        return;
    }
    if (runtime->capture && runtime->capture->recording) {
        output.error(tr("perf.already_recording"));
        return;
    }
    auto capture      = std::make_shared<Capture>();
    capture->deadline = Clock::now() + std::chrono::seconds(seconds);
    capture->ticks.reserve(MaxTickSamples);
    capture->spikes.reserve(20);
    capture->entities.reserve(static_cast<std::size_t>(runtime->settings.maxTrackedEntities));
    capture->chunks.reserve(static_cast<std::size_t>(runtime->settings.maxTrackedEntities));
    capture->systems.reserve(256);
    runtime->capture = std::move(capture);
    output.success(tr("perf.started", seconds));
}

double percentile(std::vector<double> const& sorted, double fraction) {
    auto const index = static_cast<std::size_t>(std::ceil(static_cast<double>(sorted.size()) * fraction)) - 1;
    return sorted[std::min(index, sorted.size() - 1)];
}

void printMetric(CommandOutput& output, std::string const& name, Metric const& metric, std::size_t ticks) {
    output.success(
        tr("perf.detail_metric",
           name,
           metric.calls,
           metric.inclusive,
           metric.self,
           metric.maximum,
           metric.calls ? metric.inclusive / static_cast<double>(metric.calls) : 0.0,
           ticks ? metric.self / static_cast<double>(ticks) : 0.0)
    );
    if (metric.trueResults || metric.falseResults) {
        output.success(tr("perf.results", metric.trueResults, metric.falseResults));
    }
}

void printWorkCounts(Capture const& capture, CommandOutput& output) {
    constexpr std::array WorkPhases{Phase::Nearby, Phase::SpatialQuery, Phase::CollisionShapes, Phase::EntitySensor};
    for (auto phase : WorkPhases) {
        auto const  index  = static_cast<std::size_t>(phase);
        auto const& metric = capture.phases[index];
        output.success(
            tr("perf.work_count",
               tr(PhaseKeys[index]),
               metric.units,
               metric.maxUnits,
               metric.calls ? static_cast<double>(metric.units) / static_cast<double>(metric.calls) : 0.0)
        );
    }
    output.success(tr("perf.sensor_due", capture.sensorDue, capture.sensorVisits));
    output.success(tr("perf.work_note"));
    for (std::size_t i = 0; i < capture.targetSources.size(); ++i) {
        constexpr std::array SourceKeys{"perf.target.initial", "perf.target.running", "perf.target.other"};
        printMetric(output, tr(SourceKeys[i]), capture.targetSources[i], capture.ticks.size());
    }
}

void printChunks(Capture const& capture, CommandOutput& output, std::size_t top) {
    std::vector<std::pair<ChunkKey, ChunkStats const*>> chunks;
    chunks.reserve(capture.chunks.size());
    for (auto const& [key, stats] : capture.chunks) chunks.emplace_back(key, &stats);
    std::sort(chunks.begin(), chunks.end(), [](auto const& a, auto const& b) {
        return a.second->total() > b.second->total();
    });
    output.success(tr("perf.chunk_title"));
    for (std::size_t i = 0; i < std::min(top, chunks.size()); ++i) {
        auto const& [key, stats] = chunks[i];
        auto const phase =
            static_cast<std::size_t>(std::max_element(stats->self.begin(), stats->self.end()) - stats->self.begin());
        output.success(
            tr("perf.chunk_row",
               i + 1,
               key.dimension,
               key.x,
               key.z,
               stats->calls,
               stats->total() / static_cast<double>(capture.ticks.size()),
               tr(PhaseKeys[phase]),
               stats->self[phase] / static_cast<double>(capture.ticks.size()))
        );
    }
    output.success(tr("perf.chunk_coverage", capture.chunks.size(), capture.chunkOverflowCalls));
}

void printSpikes(Capture const& capture, CommandOutput& output, std::size_t top) {
    auto spikes = capture.spikes;
    std::sort(spikes.begin(), spikes.end(), [](auto const& a, auto const& b) { return a.elapsed > b.elapsed; });
    output.success(tr("perf.spike_title", spikes.size()));
    for (std::size_t i = 0; i < std::min(top, spikes.size()); ++i) {
        auto const&                         spike = spikes[i];
        std::array<std::size_t, PhaseCount> order{};
        std::iota(order.begin(), order.end(), 0);
        std::partial_sort(order.begin(), order.begin() + 3, order.end(), [&](auto a, auto b) {
            return spike.self[a] > spike.self[b];
        });
        output.success(
            tr("perf.spike_row",
               spike.tick,
               spike.elapsed,
               tr(PhaseKeys[order[0]]),
               spike.self[order[0]],
               tr(PhaseKeys[order[1]]),
               spike.self[order[1]],
               tr(PhaseKeys[order[2]]),
               spike.self[order[2]])
        );
    }
}

void printPhases(Capture const& capture, CommandOutput& output) {
    std::array<std::size_t, PhaseCount> order{};
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](auto a, auto b) {
        return capture.phases[a].self > capture.phases[b].self;
    });
    double const worldTime = capture.phases[static_cast<std::size_t>(Phase::World)].inclusive;
    std::size_t inactive = 0;
    for (auto index : order) {
        auto const& metric = capture.phases[index];
        if (!metric.calls) {
            ++inactive;
            continue;
        }
        printMetric(output, tr(PhaseKeys[index]), metric, capture.ticks.size());
        output.success(
            tr("perf.share", worldTime > 0 ? 100.0 * metric.self / worldTime : 0.0, capture.slowSelf[index])
        );
        auto const& peak = capture.peaks[index];
        if (peak.hasEntity) {
            output.success(
                tr("perf.peak", peak.type, peak.id, peak.dimension, peak.position.x, peak.position.y, peak.position.z)
            );
        }
    }
    if (inactive) output.success(tr("perf.inactive_phases", inactive));
    printWorkCounts(capture, output);
}

void printHotspots(Capture const& capture, CommandOutput& output, std::size_t top) {
    struct Source {
        std::string name;
        Metric const* metric;
    };
    std::vector<Source> sources;
    for (std::size_t i = 0; i < PhaseCount; ++i) {
        if (i == static_cast<std::size_t>(Phase::EcsSystem) || !capture.phases[i].calls) continue;
        sources.push_back({tr(PhaseKeys[i]), &capture.phases[i]});
    }
    double attributed = 0;
    for (auto const& [key, stats] : capture.systems) {
        sources.push_back({tr("perf.system_remainder", stats.name), &stats.metric});
        attributed += stats.metric.self;
    }
    Metric overflow;
    overflow.self = std::max(0.0, capture.phases[static_cast<std::size_t>(Phase::EcsSystem)].self - attributed);
    if (overflow.self > 0) sources.push_back({tr("perf.phase.ecs_uncovered"), &overflow});
    std::sort(sources.begin(), sources.end(), [](auto const& a, auto const& b) {
        return a.metric->self > b.metric->self;
    });
    double const ticks = static_cast<double>(capture.ticks.size());
    double const world = capture.phases[0].inclusive;
    output.success(tr("perf.hot_title"));
    for (std::size_t i = 0; i < std::min(top, sources.size()); ++i) {
        auto const& source = sources[i];
        output.success(tr("perf.hot_row", i + 1, source.name, source.metric->self / ticks,
                          world > 0 ? 100.0 * source.metric->self / world : 0.0,
                          static_cast<double>(source.metric->calls) / ticks, source.metric->maximum));
    }
}

void printSystems(Capture const& capture, CommandOutput& output, std::size_t top) {
    std::vector<SystemStats const*> systems;
    for (auto const& [key, stats] : capture.systems) systems.push_back(&stats);
    std::sort(systems.begin(), systems.end(), [](auto a, auto b) {
        return a->metric.inclusive > b->metric.inclusive;
    });
    output.success(tr("perf.system_title", systems.size()));
    if (systems.empty()) output.success(tr("perf.system_no_data"));
    double const ticks = static_cast<double>(capture.ticks.size());
    for (std::size_t i = 0; i < std::min(top, systems.size()); ++i) {
        auto const& stats = *systems[i];
        auto const phase = static_cast<std::size_t>(
            std::max_element(stats.self.begin(), stats.self.end()) - stats.self.begin());
        output.success(tr("perf.system_row", i + 1, stats.name, stats.metric.inclusive / ticks,
                          stats.metric.self / ticks, static_cast<double>(stats.metric.calls) / ticks,
                          stats.metric.maximum, tr(PhaseKeys[phase]), stats.self[phase] / ticks));
    }
    output.success(tr("perf.system_coverage", capture.systemOverflowCalls, capture.unsupportedCollections));
}

void printTypes(Capture const& capture, CommandOutput& output, std::size_t top) {
    struct TypeStats {
        double time = 0;
        std::size_t count = 0;
        std::array<double, PhaseCount> self{};
    };
    std::unordered_map<std::string, TypeStats> byType;
    for (auto const& [id, entity] : capture.entities) {
        auto& stats = byType[entity.type];
        stats.time += entity.selfTime();
        ++stats.count;
        for (std::size_t i = 0; i < PhaseCount; ++i) stats.self[i] += entity.phases[i].self;
    }
    std::vector<std::pair<std::string, TypeStats>> types(byType.begin(), byType.end());
    std::sort(types.begin(), types.end(), [](auto const& a, auto const& b) {
        return a.second.time > b.second.time;
    });
    output.success(tr("perf.type_title"));
    double const ticks = static_cast<double>(capture.ticks.size());
    double const world = capture.phases[0].inclusive;
    for (std::size_t i = 0; i < std::min(top, types.size()); ++i) {
        auto const& [name, stats] = types[i];
        auto const phase = static_cast<std::size_t>(
            std::max_element(stats.self.begin(), stats.self.end()) - stats.self.begin());
        output.success(tr("perf.type_compact", i + 1, name, stats.time / ticks,
                          world > 0 ? 100.0 * stats.time / world : 0.0, stats.count,
                          tr(PhaseKeys[phase]), stats.self[phase] / ticks));
    }
}

void printEntities(Capture const& capture, CommandOutput& output, std::size_t top) {
    std::vector<EntityStats const*> entities;
    for (auto const& [id, entity] : capture.entities) entities.push_back(&entity);
    std::sort(entities.begin(), entities.end(), [](auto a, auto b) { return a->selfTime() > b->selfTime(); });
    output.success(tr("perf.entity_title"));
    double const ticks = static_cast<double>(capture.ticks.size());
    for (std::size_t i = 0; i < std::min(top, entities.size()); ++i) {
        auto const& entity = *entities[i];
        auto const worst = std::max_element(entity.phases.begin(), entity.phases.end(), [](auto const& a, auto const& b) {
            return a.self < b.self;
        });
        auto const phase = static_cast<std::size_t>(worst - entity.phases.begin());
        output.success(tr("perf.entity_compact", i + 1, entity.type, entity.id, entity.dimension,
                          entity.position.x, entity.position.y, entity.position.z, entity.selfTime() / ticks,
                          tr(PhaseKeys[phase]), worst->self / ticks));
    }
}

enum class ReportSection { Phases, Systems, Types, Entities, Chunks, Spikes };
void section(CommandOrigin const& origin, CommandOutput& output, ReportSection requested) {
    std::lock_guard lock(stateMutex);
    if (!permitted(origin, output)) return;
    auto const capture = runtime->capture;
    if (!capture || capture->ticks.empty()) {
        output.error(tr("perf.no_data"));
        return;
    }
    output.success(tr("perf.scope_note"));
    if (requested == ReportSection::Phases) printPhases(*capture, output);
    else if (requested == ReportSection::Systems) printSystems(*capture, output, capture->systems.size());
    else if (requested == ReportSection::Types) printTypes(*capture, output, capture->entities.size());
    else if (requested == ReportSection::Entities)
        printEntities(*capture, output, static_cast<std::size_t>(runtime->settings.topCount));
    else if (requested == ReportSection::Chunks)
        printChunks(*capture, output, static_cast<std::size_t>(runtime->settings.topCount));
    else printSpikes(*capture, output, static_cast<std::size_t>(runtime->settings.topCount));
}

void entityReport(CommandOrigin const& origin, CommandOutput& output, std::string const& textId) {
    std::lock_guard lock(stateMutex);
    if (!permitted(origin, output)) return;
    std::int64_t id{};
    auto const   parsed = std::from_chars(textId.data(), textId.data() + textId.size(), id);
    if (parsed.ec != std::errc{} || parsed.ptr != textId.data() + textId.size()) {
        output.error(tr("perf.invalid_id"));
        return;
    }
    auto const capture = runtime->capture;
    if (!capture) {
        output.error(tr("perf.no_data"));
        return;
    }
    auto const it = capture->entities.find(id);
    if (it == capture->entities.end()) {
        output.error(tr("perf.entity_missing", id));
        return;
    }
    auto const& entity = it->second;
    output.success(
        tr("perf.entity_detail",
           entity.type,
           id,
           entity.dimension,
           entity.position.x,
           entity.position.y,
           entity.position.z,
           entity.sampledTicks,
           entity.selfTime())
    );
    for (std::size_t i = 0; i < PhaseCount; ++i) {
        auto const& metric = entity.phases[i];
        if (!metric.calls) continue;
        printMetric(output, tr(PhaseKeys[i]), metric, capture->ticks.size());
        if (metric.units)
            output.success(
                tr("perf.work_count",
                   tr(PhaseKeys[i]),
                   metric.units,
                   metric.maxUnits,
                   static_cast<double>(metric.units) / static_cast<double>(metric.calls))
            );
    }
    output.success(tr("perf.detail_note"));
}

void report(CommandOrigin const& origin, CommandOutput& output) {
    std::lock_guard lock(stateMutex);
    if (!permitted(origin, output)) return;
    auto const capture = runtime->capture;
    if (!capture || capture->ticks.empty()) {
        output.error(tr("perf.no_data"));
        return;
    }
    auto sorted = capture->ticks;
    std::sort(sorted.begin(), sorted.end());
    double const average = std::accumulate(sorted.begin(), sorted.end(), 0.0) / static_cast<double>(sorted.size());
    output.success(
        tr("perf.summary",
           tr(capture->recording ? "perf.recording" : "perf.stopped"),
           sorted.size(),
           average,
           percentile(sorted, 0.95),
           percentile(sorted, 0.99),
           sorted.back(),
           capture->slowTicks,
           runtime->settings.slowTickMs)
    );
    output.success(
        tr("perf.median",
           percentile(sorted, 0.50),
           100.0 * static_cast<double>(capture->slowTicks) / static_cast<double>(sorted.size()))
    );
    char const* load = percentile(sorted, 0.50) >= runtime->settings.slowTickMs ? "perf.load.sustained"
        : capture->slowTicks ? "perf.load.spikes" : "perf.load.normal";
    output.success(tr("perf.assessment", tr(load), std::max(0.0, average - runtime->settings.slowTickMs)));
    output.success(tr("perf.compact_note"));
    auto const top = static_cast<std::size_t>(runtime->settings.topCount);
    printHotspots(*capture, output, std::min(top, std::size_t{8}));
    printSystems(*capture, output, std::min(top, std::size_t{5}));
    printTypes(*capture, output, std::min(top, std::size_t{3}));
    printEntities(*capture, output, std::min(top, std::size_t{3}));
    output.success(tr("perf.coverage", capture->entities.size(), capture->unattributedCalls));
    if (capture->strollAllowed || capture->strollSkipped)
        output.success(tr("perf.optimization_counts", capture->strollAllowed, capture->strollSkipped));
    printChunks(*capture, output, std::min(top, std::size_t{3}));
    if (!capture->spikes.empty()) printSpikes(*capture, output, 1);
    output.success(tr("perf.detail_help"));
    for (auto const& item : probes) {
        if (item.installed) continue;
        if (item.deferred && item.attempted)
            output.success(tr("perf.ecs_probe_error", item.name, tr(item.failure), item.installResult));
        else output.success(tr(item.deferred ? "perf.pending_probe" : "perf.missing_probe", item.name));
    }
}

void status(CommandOrigin const& origin, CommandOutput& output) {
    std::lock_guard lock(stateMutex);
    if (!permitted(origin, output)) return;
    auto const capture   = runtime->capture;
    auto const tickTime  = *ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime;
    auto const mspt      = std::max(0.0, std::chrono::duration<double, std::milli>(tickTime).count());
    auto const installed = std::count_if(probes.begin(), probes.end(), [](auto const& item) { return item.installed; });
    bool const optimizing = runtime->settings.optimization.enable && !runtime->strollTypes.empty();
    output.success(
        tr("perf.status",
           tr(capture && capture->recording ? "perf.recording" : "perf.stopped"),
           mspt,
           installed,
           probes.size(),
           tr(optimizing ? "perf.on" : "perf.off"))
    );
    output.success(tr("perf.optimization_counts", runtime->strollAllowed, runtime->strollSkipped));
    output.success(tr("perf.help"));
}

} // namespace

struct StartParams {
    int seconds = 0;
};
struct EntityParams {
    std::string id;
};

namespace {

void registerCommand() {
    if (commandRegistered) return;
    auto& command = ll::command::CommandRegistrar::getServerInstance()
                        .getOrCreateCommand("wxperf", tr("perf.description"), CommandPermissionLevel::GameDirectors);
    auto const showStatus = [](CommandOrigin const& origin, CommandOutput& output) { status(origin, output); };
    command.overload<ll::command::EmptyParam>().execute(showStatus);
    command.overload<ll::command::EmptyParam>().text("status").execute(showStatus);
    command.overload<StartParams>().text("start").optional("seconds").execute(
        [](CommandOrigin const& origin, CommandOutput& output, StartParams const& params) {
            startCapture(origin, output, params.seconds);
        }
    );
    command.overload<ll::command::EmptyParam>().text("report").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { report(origin, output); }
    );
    command.overload<ll::command::EmptyParam>().text("phases").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { section(origin, output, ReportSection::Phases); }
    );
    command.overload<ll::command::EmptyParam>().text("systems").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { section(origin, output, ReportSection::Systems); }
    );
    command.overload<ll::command::EmptyParam>().text("types").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { section(origin, output, ReportSection::Types); }
    );
    command.overload<ll::command::EmptyParam>().text("entities").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { section(origin, output, ReportSection::Entities); }
    );
    command.overload<ll::command::EmptyParam>().text("chunks").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { section(origin, output, ReportSection::Chunks); }
    );
    command.overload<ll::command::EmptyParam>().text("spikes").execute(
        [](CommandOrigin const& origin, CommandOutput& output) { section(origin, output, ReportSection::Spikes); }
    );
    command.overload<EntityParams>().text("entity").required("id").execute(
        [](CommandOrigin const& origin, CommandOutput& output, EntityParams const& params) {
            entityReport(origin, output, params.id);
        }
    );
    command.overload<ll::command::EmptyParam>().text("stop").execute([](CommandOrigin const& origin,
                                                                        CommandOutput&       output) {
        std::lock_guard lock(stateMutex);
        if (!permitted(origin, output)) return;
        if (runtime->capture) runtime->capture->recording = false;
        output.success(tr("perf.capture_stopped"));
    });
    // Keep registrations across config reloads; only their runtime state is replaced.
    commandRegistered = true;
}

} // namespace

bool enable() {
    std::lock_guard lock(stateMutex);
    if (runtime && runtime->active) return true;
    try {
        registerCommand();
        if (!config.performanceMonitor.enable) return true;
        auto state                  = std::make_shared<Runtime>();
        state->settings             = config.performanceMonitor;
        auto& settings              = state->settings;
        settings.captureSeconds     = std::clamp(settings.captureSeconds, 1, 600);
        settings.topCount           = std::clamp(settings.topCount, 1, 20);
        settings.maxTrackedEntities = std::clamp(settings.maxTrackedEntities, 128, 16384);
        settings.slowTickMs = std::isfinite(settings.slowTickMs) ? std::clamp(settings.slowTickMs, 1.0, 10000.0) : 50.0;
        settings.optimization.strollIntervalTicks = std::clamp(settings.optimization.strollIntervalTicks, 1, 200);
        state->strollTypes.insert(
            settings.optimization.strollTypeNames.begin(),
            settings.optimization.strollTypeNames.end()
        );
        for (auto& item : probes) {
            item.attempted = false;
            item.failure = "perf.probe.not_attempted";
            item.installResult = -1;
            if (item.deferred) continue;
            if (!item.installed) {
                try {
                    item.installed = item.install() == 0;
                } catch (std::exception const& exception) {
                    logger.warn(tr("perf.error", exception.what()));
                }
            }
            if (!item.installed) logger.warn(tr("perf.missing_probe", item.name));
            if (&item == &probes.front() && !item.installed) return true;
        }
        if (!probes.back().installed) settings.optimization.enable = false;
        runtime = std::move(state);
        enabled.store(true, std::memory_order_relaxed);
    } catch (std::exception const& exception) {
        logger.warn(tr("perf.error", exception.what()));
        disable();
    }
    // An unavailable diagnostic probe must not disable unrelated modules.
    return true;
}

void disable() {
    enabled.store(false, std::memory_order_relaxed);
    std::lock_guard lock(stateMutex);
    if (runtime) {
        runtime->active = false;
        if (runtime->capture) runtime->capture->recording = false;
    }
    runtime.reset();
    for (auto it = probes.rbegin(); it != probes.rend(); ++it) {
        if (it->installed) {
            if (it->remove()) it->installed = false;
            else logger.warn(tr("perf.unhook_failed", it->name));
        }
    }
}

} // namespace my_mod::performance_monitor

#else

namespace my_mod::performance_monitor {
bool enable() { return true; }
void disable() {}
} // namespace my_mod::performance_monitor

#endif
