#include "mod/PistonControl.h"

#ifdef LL_PLAT_S
#include "ll/api/base/ScopedValue.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/components/BlockMovableComponent.h"
#include "mc/world/level/block/components/BlockMovableDescription.h"
#include "mod/Global.h"

namespace my_mod::piston_control {

// Called from enable(), after the server has built the block registry. Applying
// to live states also covers initialization calls inlined by the Windows build.
void applyBlockConfig() {
    if (!config.pistonControl.enable) {
        return;
    }
    auto const& blocks = config.pistonControl.customPistonBlocks;
    if (blocks.empty()) {
        return;
    }

    for (auto const& [name, settings] : blocks) {
        auto blockType = ::BlockType::tryGetFromRegistry(::HashedString{name});
        if (!blockType) {
            logger.error(tr("piston_control.block_not_found", name));
            continue;
        }

        size_t applied = 0;
        size_t created = 0;
        size_t failed  = 0;
        auto   apply   = [&](::Block const& block) {
            auto* movable = const_cast<::BlockMovableComponent*>(block.tryGetComponent<::BlockMovableComponent>());
            if (movable == nullptr) {
                // No explicit movable component means PushPull/None in the
                // piston walker. Use the game's initializer to register the
                // correct type ID and allocate the shared component.
                ::BlockMovableDescription description{};
                description.mMovementType = ::MovementType::PushPull;
                description.mStickyType   = ::StickyType::None;
                // The registry exposes a const view; configuration deliberately
                // updates the live component storage during enable().
                auto& storage = const_cast<::BlockComponentStorage&>(blockType->mComponents.get());
                {
                    ll::ScopedValue allowModification(storage.mAllowModifyingComponents, true);
                    description.$initializeComponentFromCode(storage);
                }
                movable = const_cast<::BlockMovableComponent*>(block.tryGetComponent<::BlockMovableComponent>());
                if (movable == nullptr) {
                    ++failed;
                    return;
                }
                ++created;
            }
            movable->mMovementType = settings.movementType;
            movable->mStickyType   = settings.stickyType;

            // Read through the same accessor used by the piston walker, rather
            // than assuming a successful write means this state uses it.
            auto const* actual = block.tryGetComponent<::BlockMovableComponent>();
            if (actual != nullptr && actual->mMovementType == settings.movementType
                && actual->mStickyType == settings.stickyType) {
                ++applied;
            } else {
                ++failed;
            }
        };

        auto const* defaultState = blockType->mDefaultState;
        if (defaultState != nullptr) {
            apply(*defaultState);
        }
        for (auto const& state : *blockType->mBlockPermutations) {
            if (state && state.get() != defaultState) {
                apply(*state);
            }
        }

        if (applied == 0 || failed != 0) {
            logger.error(tr("piston_control.block_failed", name, applied, failed));
        } else {
            logger.info(
                tr("piston_control.block_applied",
                   name,
                   applied,
                   created,
                   static_cast<int>(settings.movementType),
                   static_cast<int>(settings.stickyType))
            );
        }
    }
}

} // namespace my_mod::piston_control

#else

namespace my_mod::piston_control {

void applyBlockConfig() {}

} // namespace my_mod::piston_control

#endif
