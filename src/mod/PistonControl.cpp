#include "mod/PistonControl.h"

#ifdef LL_PLAT_S
#include "ll/api/memory/Hook.h"
#include "mc/common/Globals.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/utility/optional_ref.h"
#include "mc/deps/vanilla_components/IConstBlockSource.h"
#include "mc/deps/vanilla_components/block_source_visitor/CollisionShape.h"
#include "mc/entity/systems/MoveTowardsClosestSpaceSystemImpl.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/ActorTypeNamespaceRules.h"
#include "mc/world/actor/GetCollisionShapeActorProxy.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/GetCollisionShapeInterface.h"
#include "mc/world/level/block/PistonArmBlock.h"
#include "mc/world/level/block/actor/MovingBlock.h"
#include "mc/world/level/block/actor/PistonBlockActor.h"
#include "mc/world/level/block/components/MovementType.h"
#include "mc/world/level/block/components/StickyType.h"
#include "mc/world/phys/AABB.h"
#include "mod/Global.h"
#include "mod/PistonCollisionBlockSource.h"
#include <atomic>
#include <cmath>
#include <set>
#include <vector>

namespace my_mod::piston_control {
namespace {

// 配置里的名字预解析成 ActorType，避免在碰撞热路径里做字符串查表。
// 在启用或重载实体免疫功能时刷新。
std::set<::ActorType> gImmuneTypes;
bool                  gImmuneAll = false;
std::atomic_bool      enabled{false};

// Actor 路径可取回实体名；ECS 的 GetCollisionShapeEntityProxy 则使用接口的 ActorType。
::Actor const* tryGetActor(::optional_ref<::GetCollisionShapeInterface const> entity) {
    auto const* base = entity.as_ptr();
    if (base == nullptr) {
        return nullptr;
    }
    if (*reinterpret_cast<void* const* const*>(base) != ::GetCollisionShapeActorProxy::$vftable()) {
        return nullptr;
    }
    return &static_cast<::GetCollisionShapeActorProxy const*>(base)->mActor;
}

// 该实体是否被配置为无视活塞。
bool isPistonImmune(::Actor const& entity) {
    auto const& immune = config.pistonControl.pistonImmuneEntities;
    return enabled.load(std::memory_order_relaxed) && !immune.empty()
        && (immune.contains("*") || immune.contains(entity.getTypeName()));
}

bool isPistonImmune(::optional_ref<::GetCollisionShapeInterface const> entity) {
    if (!enabled.load(std::memory_order_relaxed) || !entity.has_value()) {
        return false;
    }
    if (gImmuneAll) {
        return true;
    }
    // 主判定：接口自带的 ActorType，不需要任何指针转换，原版实体都能覆盖。
    if (gImmuneTypes.contains(entity->getEntityTypeId())) {
        return true;
    }
    // 兜底：类型未识别时，仅 Actor 代理可以取回实体名进行精确比较。
    auto const* actor = tryGetActor(entity);
    return actor != nullptr && isPistonImmune(*actor);
}

// 被活塞推动中的方块 + 伸出的活塞臂。这两类才需要对免疫实体隐形，
// 活塞本体保持实心，免得实体掉进活塞里。
bool isPistonCollisionBlock(::Block const& block) {
    if (block.getBlockType().isMovingBlock()) {
        return true;
    }
    return block.getTypeName().find("piston_arm") != std::string::npos;
}

// 实体的碰撞箱是否正压在活塞方块上。
// 用来把"取消去穿透"限定在活塞场景，别的情况（比如卡进墙里）仍然照常脱困。
bool overlapsPistonBlock(::AABB const& aabb, ::IConstBlockSource const& region) {
    int const minX = static_cast<int>(std::floor(aabb.min.x));
    int const minY = static_cast<int>(std::floor(aabb.min.y));
    int const minZ = static_cast<int>(std::floor(aabb.min.z));
    int const maxX = static_cast<int>(std::floor(aabb.max.x));
    int const maxY = static_cast<int>(std::floor(aabb.max.y));
    int const maxZ = static_cast<int>(std::floor(aabb.max.z));

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                if (isPistonCollisionBlock(region.getBlock(::BlockPos{x, y, z}))) {
                    return true;
                }
            }
        }
    }
    return false;
}

void rebuildPistonImmuneCache() {
    auto const& immune = config.pistonControl.pistonImmuneEntities;

    gImmuneTypes.clear();
    gImmuneAll = immune.contains("*");

    for (auto const& name : immune) {
        if (name == "*") {
            continue;
        }
        auto const type = ::EntityTypeFromString(name);
        // 未识别名称不能让所有 Undefined 类型的附加包实体一起免疫。
        if (type != ::ActorType::Undefined) {
            gImmuneTypes.insert(type);
        }
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

// 1) 取消活塞主动施加给实体的位移。
// moveEntityLastProgress 是活塞（含 MovingBlockActor 路径）位移单个实体的唯一入口。
LL_TYPE_INSTANCE_HOOK(
    PistonIgnoreEntityHook,
    ll::memory::HookPriority::Normal,
    PistonBlockActor,
    &PistonBlockActor::moveEntityLastProgress,
    void,
    ::Actor& entity,
    ::Vec3   delta
) {
    if (isPistonImmune(entity)) {
        return;
    }

    origin(entity, delta);
}

// 2) 真正的"穿透"：实体移动时收集方块碰撞箱的地方，把活塞相关的盒子剔掉。
// MoveCollisionSystem::fetchCollisionShapes 走的就是这个接口，
// 而且 CollisionShape 里带了 mMaybeBlock，能判断盒子来自哪个方块。
LL_TYPE_INSTANCE_HOOK(
    PistonPassThroughHook,
    ll::memory::HookPriority::Normal,
    BlockSource,
    &BlockSource::$fetchCollisionShapesAndBlocks,
    void,
    ::std::vector<::BlockSourceVisitor::CollisionShape>& shapes,
    ::AABB const&                                        intersectTestBox,
    bool                                                 withUnloadedChunks,
    ::optional_ref<::GetCollisionShapeInterface const>   entity,
    ::std::vector<::AABB>*                               tempShapes
) {
    origin(shapes, intersectTestBox, withUnloadedChunks, entity, tempShapes);

    if (!isPistonImmune(entity)) {
        return;
    }

    std::erase_if(shapes, [](::BlockSourceVisitor::CollisionShape const& shape) {
        ::Block const* block = shape.mMaybeBlock;
        return block != nullptr && isPistonCollisionBlock(*block);
    });
}

// 免疫实体与活塞重叠时，跳过原版的挤出方块位移。
LL_TYPE_INSTANCE_HOOK(
    PistonPushOutOfBlocksHook,
    ll::memory::HookPriority::Normal,
    Actor,
    &Actor::pushOutOfBlocks,
    void,
    ::Vec3 const& vec
) {
    if (isPistonImmune(*this)) {
        if (overlapsPistonBlock(this->getAABB(), this->getDimensionBlockSource())) {
            return;
        }
    }

    origin(vec);
}

// 26.32 把 moveTowardsClosestSpace 内联进 doTick。保留原版计算，
// 通过局部方块源适配器过滤活塞碰撞，不依赖 LocalConstBlockSource 的私有布局。
LL_STATIC_HOOK(
    PistonDepenetrationHook,
    ll::memory::HookPriority::Normal,
    &MoveTowardsClosestSpaceSystemImpl::doTick,
    void,
    ::StrictEntityContext const&                      entity,
    ::ActorDataFlagComponent const&                   synchedActorData,
    ::AABBShapeComponent const&                       aabbShape,
    ::DepenetrationComponent const&                   depenetration,
    ::Optional<::MovementInterpolatorComponent const> moveInterpolator,
    ::StateVectorComponent&                           stateVector,
    bool                                              isClientSide,
    ::GetCollisionShapeViews const&                   collisionViews,
    ::IConstBlockSource const&                        region
) {
    if (!enabled.load(std::memory_order_relaxed)) {
        origin(
            entity,
            synchedActorData,
            aabbShape,
            depenetration,
            moveInterpolator,
            stateVector,
            isClientSide,
            collisionViews,
            region
        );
        return;
    }
    PistonCollisionBlockSource filteredRegion(region, &isPistonImmune, [](::Block const* block) {
        return isPistonCollisionBlock(*block);
    });
    origin(
        entity,
        synchedActorData,
        aabbShape,
        depenetration,
        moveInterpolator,
        stateVector,
        isClientSide,
        collisionViews,
        filteredRegion
    );
}

// 2) 让活塞推动中的方块对这些实体没有碰撞箱 —— 这一步才是"穿透"。
// MovingBlock 没有重写 addCollisionShapes，走的是 BlockType 的实现，
// 而后者会分发到这个虚函数，所以这里返回空盒即可去掉碰撞。
LL_TYPE_INSTANCE_HOOK(
    MovingBlockNoCollisionHook,
    ll::memory::HookPriority::Normal,
    MovingBlock,
    &MovingBlock::$getCollisionShape,
    ::AABB,
    ::Block const&                                     block,
    ::IConstBlockSource const&                         region,
    ::BlockPos const&                                  pos,
    ::optional_ref<::GetCollisionShapeInterface const> entity
) {
    if (isPistonImmune(entity)) {
        return ::AABB{}; // 零体积，等价于无碰撞
    }

    return origin(block, region, pos, entity);
}

// 3) 活塞臂（伸出的推头）本身也会挡住/推动实体。
// PistonArmBlock 自己重写了 addCollisionShapes，不会经过 getCollisionShape，
// 所以两个都要拦。
LL_TYPE_INSTANCE_HOOK(
    PistonArmNoCollisionShapeHook,
    ll::memory::HookPriority::Normal,
    PistonArmBlock,
    &PistonArmBlock::$getCollisionShape,
    ::AABB,
    ::Block const&                                     block,
    ::IConstBlockSource const&                         region,
    ::BlockPos const&                                  pos,
    ::optional_ref<::GetCollisionShapeInterface const> entity
) {
    if (isPistonImmune(entity)) {
        return ::AABB{};
    }

    return origin(block, region, pos, entity);
}

LL_TYPE_INSTANCE_HOOK(
    PistonArmNoAddCollisionHook,
    ll::memory::HookPriority::Normal,
    PistonArmBlock,
    &PistonArmBlock::$addCollisionShapes,
    bool,
    ::Block const&                                     block,
    ::IConstBlockSource const&                         region,
    ::BlockPos const&                                  pos,
    ::AABB const*                                      intersectTestBox,
    ::std::vector<::AABB>&                             inoutBoxes,
    ::optional_ref<::GetCollisionShapeInterface const> entity
) {
    if (isPistonImmune(entity)) {
        return false; // 不往 inoutBoxes 里加任何盒子
    }

    return origin(block, region, pos, intersectTestBox, inoutBoxes, entity);
}

#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (enabled.load(std::memory_order_relaxed) || !config.pistonControl.enable
        || config.pistonControl.pistonImmuneEntities.empty()) {
        return true;
    }
    rebuildPistonImmuneCache();
    if (PistonIgnoreEntityHook::hook() != 0) {
        return false;
    }
    if (PistonPassThroughHook::hook() != 0) {
        PistonIgnoreEntityHook::unhook();
        return false;
    }
    if (PistonPushOutOfBlocksHook::hook() != 0) {
        PistonPassThroughHook::unhook();
        PistonIgnoreEntityHook::unhook();
        return false;
    }
    if (PistonDepenetrationHook::hook() != 0) {
        PistonPushOutOfBlocksHook::unhook();
        PistonPassThroughHook::unhook();
        PistonIgnoreEntityHook::unhook();
        return false;
    }
    if (MovingBlockNoCollisionHook::hook() != 0) {
        PistonDepenetrationHook::unhook();
        PistonPushOutOfBlocksHook::unhook();
        PistonPassThroughHook::unhook();
        PistonIgnoreEntityHook::unhook();
        return false;
    }
    if (PistonArmNoCollisionShapeHook::hook() != 0) {
        MovingBlockNoCollisionHook::unhook();
        PistonDepenetrationHook::unhook();
        PistonPushOutOfBlocksHook::unhook();
        PistonPassThroughHook::unhook();
        PistonIgnoreEntityHook::unhook();
        return false;
    }
    if (PistonArmNoAddCollisionHook::hook() != 0) {
        PistonArmNoCollisionShapeHook::unhook();
        MovingBlockNoCollisionHook::unhook();
        PistonDepenetrationHook::unhook();
        PistonPushOutOfBlocksHook::unhook();
        PistonPassThroughHook::unhook();
        PistonIgnoreEntityHook::unhook();
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    PistonArmNoAddCollisionHook::unhook();
    PistonArmNoCollisionShapeHook::unhook();
    MovingBlockNoCollisionHook::unhook();
    PistonDepenetrationHook::unhook();
    PistonPushOutOfBlocksHook::unhook();
    PistonPassThroughHook::unhook();
    PistonIgnoreEntityHook::unhook();
    gImmuneTypes.clear();
    gImmuneAll = false;
}

} // namespace my_mod::piston_control

#else

namespace my_mod::piston_control {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::piston_control

#endif
