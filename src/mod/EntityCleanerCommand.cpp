#include "mod/EntityCleanerCommand.h"

#ifdef LL_PLAT_S

#include "mod/EntityCleaner.h"
#include "mod/Global.h"

#include "ll/api/command/Command.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/form/CustomForm.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/deps/ecs/WeakEntityRef.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace my_mod::entity_cleaner_command {
namespace {

bool commandRegistered = false;

struct EntityTypeEntry {
    std::string typeName;
    std::string key;
};

struct ManualCleanupOptions {
    bool  keepTypeProtected           = true;
    bool  keepYoungDroppedItems       = true;
    bool  keepWhitelistedDroppedItems = true;
    bool  keepNamed                   = true;
    bool  keepTamed                   = true;
    bool  keepLeashed                 = true;
    bool  keepVehicles                = true;
    bool  keepPersistent              = true;
    bool  keepNearPlayers             = false;
    bool  applyConfigFilter           = false;
    float playerRadius                = 0.0F;
};

bool isFinitePosition(Vec3 const& position) {
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

std::vector<Actor*> collectLoadedActors(Level& level) {
    std::vector<Actor*> actors;
    std::unordered_set<Actor*> seen;
    // Each loaded dimension owns its entity map. Traverse all of them so the
    // manual panel and cleanup are independent of the command player's dimension.
    level.forEachDimension([&actors, &seen](Dimension& dimension) {
        auto& entityMap = dimension.getEntityIdMap();
        actors.reserve(actors.size() + entityMap.size());
        for (auto const& [actorId, entityRef] : entityMap) {
            (void)actorId;
            auto const actor = entityRef.tryUnwrap<Actor>();
            if (Actor* const ptr = actor.as_ptr(); ptr != nullptr && seen.insert(ptr).second) {
                actors.push_back(ptr);
            }
        }
        return true;
    });
    return actors;
}

std::map<std::string, std::size_t> collectEntityCounts(Level& level) {
    auto const actors = collectLoadedActors(level);
    std::map<std::string, std::size_t> counts;
    for (Actor* actor : actors) {
        // 手动面板展示所有已加载维度的完整实体列表，不套用自动清理的名单或保护规则。
        if (actor == nullptr || actor->isRemoved() || actor->isPlayer()) {
            continue;
        }
        std::string const& typeName = actor->getTypeName();
        if (!typeName.empty()) {
            ++counts[typeName];
        }
    }
    return counts;
}

bool getToggle(ll::form::CustomFormResult const& result, std::string const& key) {
    if (!result.has_value()) {
        return false;
    }
    auto const found = result->find(key);
    if (found == result->end()) {
        return false;
    }

    if (auto const value = std::get_if<uint64>(&found->second)) {
        return *value != 0;
    }
    if (auto const value = std::get_if<double>(&found->second)) {
        return *value != 0.0;
    }
    if (auto const value = std::get_if<std::string>(&found->second)) {
        return *value == "true" || *value == "1";
    }
    return false;
}

std::string radiusText(float radius) {
    if (!std::isfinite(radius) || radius <= 0.0F) {
        return tr("entity_cleaner_command.radius_disabled");
    }
    std::ostringstream stream;
    stream << radius;
    return stream.str();
}

void executeManualCleanup(
    Player&                         player,
    std::vector<std::string> const& selectedTypes,
    ManualCleanupOptions const&     options
) {
    Level& level = player.getLevel();
    auto const actors = collectLoadedActors(level);

    std::unordered_set<std::string> selectedSet;
    selectedSet.reserve(selectedTypes.size());
    for (std::string const& typeName : selectedTypes) {
        selectedSet.insert(typeName);
    }

    std::unordered_map<DimensionType, std::vector<Vec3>> playerPositions;
    float const       radiusSquared = options.playerRadius * options.playerRadius;
    bool const        useRadius     = options.keepNearPlayers && std::isfinite(options.playerRadius)
                                && options.playerRadius > 0.0F && std::isfinite(radiusSquared);
    if (useRadius) {
        level.forEachDimension([&playerPositions](Dimension& dimension) {
            dimension.forEachPlayer([&playerPositions](Player& other) {
                if (!other.isRemoved() && isFinitePosition(other.getPosition())) {
                    playerPositions[other.getDimensionId()].emplace_back(other.getPosition());
                }
                return true;
            });
            return true;
        });
    }

    std::map<std::string, int> removedByType;
    int                       removed = 0;
    for (Actor* actor : actors) {
        if (actor == nullptr || actor->isRemoved() || actor->isPlayer()) {
            continue;
        }

        std::string const& typeName = actor->getTypeName();
        if (selectedSet.find(typeName) == selectedSet.end()) {
            continue;
        }

        if (options.applyConfigFilter && !entity_cleaner::matchesConfiguredFilter(*actor)) {
            continue;
        }
        if (options.keepTypeProtected && entity_cleaner::isTypeProtectedByConfig(*actor)) {
            continue;
        }
        if (options.keepYoungDroppedItems && entity_cleaner::isDroppedItemAgeProtectedByConfig(*actor)) {
            continue;
        }
        if (options.keepWhitelistedDroppedItems && entity_cleaner::isDroppedItemWhitelistedByConfig(*actor)) {
            continue;
        }
        if ((options.keepNamed && !actor->getNameTag().empty()) || (options.keepTamed && actor->isTame())
            || (options.keepLeashed && actor->isLeashed())
            || (options.keepVehicles && (actor->hasPassenger() || actor->isRiding()))
            || (options.keepPersistent && actor->isPersistent())) {
            continue;
        }

        auto const players = playerPositions.find(actor->getDimensionId());
        if (useRadius && players != playerPositions.end() && !players->second.empty()
            && isFinitePosition(actor->getPosition())) {
            float nearestSquared = std::numeric_limits<float>::infinity();
            Vec3 const& position = actor->getPosition();
            for (Vec3 const& playerPosition : players->second) {
                float const dx = position.x - playerPosition.x;
                float const dy = position.y - playerPosition.y;
                float const dz = position.z - playerPosition.z;
                nearestSquared = std::min(nearestSquared, (dx * dx) + (dy * dy) + (dz * dz));
            }
            if (nearestSquared < radiusSquared) {
                continue;
            }
        }

        ++removedByType[typeName];
        ++removed;
        actor->remove();
    }

    if (removed == 0) {
        player.sendMessage(tr("entity_cleaner_command.no_candidates"));
        return;
    }

    player.sendMessage(tr("entity_cleaner_command.cleaned", removed));
    logger.info(tr("entity_cleaner_command.cleaned", removed));
    for (auto const& [typeName, count] : removedByType) {
        logger.info("- {}: {}", typeName, count);
    }
}

void showConfirmationForm(Player& player, std::vector<std::string> selectedTypes) {
    auto const counts = collectEntityCounts(player.getLevel());

    ll::form::CustomForm form(tr("entity_cleaner_command.confirm_title"));
    form.setSubmitButton(tr("entity_cleaner_command.confirm_button"));
    form.appendHeader(tr("entity_cleaner_command.confirm_header"));
    for (std::string const& typeName : selectedTypes) {
        auto const found = counts.find(typeName);
        auto const count = found == counts.end() ? 0U : found->second;
        form.appendLabel("- " + typeName + ": " + std::to_string(count));
    }

    auto const& protection = config.entityCleaner.protection;
    float const radius      = protection.playerRadius;
    form.appendHeader(tr("entity_cleaner_command.manual_filter_header"));
    form.appendToggle(
        "applyConfigFilter",
        tr("entity_cleaner_command.apply_config_filter"),
        false,
        tr("entity_cleaner_command.apply_config_filter_tooltip")
    );
    form.appendToggle(
        "keepTypeProtected",
        tr("entity_cleaner_command.keep_type_protected"),
        true,
        tr("entity_cleaner_command.keep_type_protected_tooltip")
    );
    form.appendToggle(
        "keepYoungDroppedItems",
        tr("entity_cleaner_command.keep_young_dropped_items"),
        true,
        tr("entity_cleaner_command.keep_young_dropped_items_tooltip")
    );
    form.appendToggle(
        "keepWhitelistedDroppedItems",
        tr("entity_cleaner_command.keep_whitelisted_dropped_items"),
        true,
        tr("entity_cleaner_command.keep_whitelisted_dropped_items_tooltip")
    );
    form.appendToggle("keepNamed", tr("entity_cleaner_command.keep_named"), protection.named);
    form.appendToggle("keepTamed", tr("entity_cleaner_command.keep_tamed"), protection.tamed);
    form.appendToggle("keepLeashed", tr("entity_cleaner_command.keep_leashed"), protection.leashed);
    form.appendToggle("keepVehicles", tr("entity_cleaner_command.keep_vehicles"), protection.vehicles);
    form.appendToggle("keepPersistent", tr("entity_cleaner_command.keep_persistent"), protection.persistent);
    form.appendToggle(
        "keepNearPlayers",
        tr("entity_cleaner_command.keep_near_players", radiusText(radius)),
        std::isfinite(radius) && radius > 0.0F
    );
    form.appendToggle("confirmCleanup", tr("entity_cleaner_command.confirm_cleanup_toggle"), false);

    form.sendTo(
        player,
        [selectedTypes = std::move(selectedTypes)](
            Player& player,
            ll::form::CustomFormResult const& result,
            ll::form::FormCancelReason
        ) {
            if (!result.has_value() || !getToggle(result, "confirmCleanup")) {
                return;
            }

            auto const& protection = config.entityCleaner.protection;
            ManualCleanupOptions options{
                getToggle(result, "keepTypeProtected"),
                getToggle(result, "keepYoungDroppedItems"),
                getToggle(result, "keepWhitelistedDroppedItems"),
                getToggle(result, "keepNamed"),
                getToggle(result, "keepTamed"),
                getToggle(result, "keepLeashed"),
                getToggle(result, "keepVehicles"),
                getToggle(result, "keepPersistent"),
                getToggle(result, "keepNearPlayers"),
                getToggle(result, "applyConfigFilter"),
                protection.playerRadius,
            };
            executeManualCleanup(player, selectedTypes, options);
        }
    );
}

void showSelectionForm(Player& player) {
    auto const counts = collectEntityCounts(player.getLevel());

    ll::form::CustomForm form(tr("entity_cleaner_command.form_title"));
    form.setSubmitButton(tr("entity_cleaner_command.submit"));
    form.appendLabel(tr("entity_cleaner_command.form_summary", counts.size()));
    form.appendLabel(tr("entity_cleaner_command.form_hint"));

    std::vector<EntityTypeEntry> entries;
    entries.reserve(counts.size());
    std::size_t index = 0;
    for (auto const& [typeName, count] : counts) {
        std::string key = "entity_" + std::to_string(index++);
        form.appendToggle(key, typeName + " (" + std::to_string(count) + ")", false);
        entries.push_back(EntityTypeEntry{typeName, std::move(key)});
    }
    if (entries.empty()) {
        form.appendLabel(tr("entity_cleaner_command.no_entities"));
    }

    form.sendTo(
        player,
        [entries = std::move(entries)](
            Player& player,
            ll::form::CustomFormResult const& result,
            ll::form::FormCancelReason
        ) {
            if (!result.has_value()) {
                return;
            }

            std::vector<std::string> selectedTypes;
            selectedTypes.reserve(entries.size());
            for (EntityTypeEntry const& entry : entries) {
                if (getToggle(result, entry.key)) {
                    selectedTypes.push_back(entry.typeName);
                }
            }
            if (selectedTypes.empty()) {
                player.sendMessage(tr("entity_cleaner_command.no_selection"));
                return;
            }

            showConfirmationForm(player, std::move(selectedTypes));
        }
    );
}

void executeCommand(CommandOrigin const& origin, CommandOutput& output) {
    Actor* const source = origin.getEntity();
    if (source == nullptr || !source->isPlayer()) {
        output.error(tr("entity_cleaner_command.not_player"));
        return;
    }

    auto& player = static_cast<Player&>(*source);
    if (!player.isOperator()) {
        player.sendMessage(tr("entity_cleaner_command.op_only"));
        return;
    }

    showSelectionForm(player);
    output.success();
}

bool registerCommand() {
    if (commandRegistered) {
        return true;
    }

    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "entitycleaner",
        tr("entity_cleaner_command.description"),
        CommandPermissionLevel::Any
    );
    command.alias("eclean");
    command.overload<ll::command::EmptyParam>().execute(
        [](CommandOrigin const& origin, CommandOutput& output) { executeCommand(origin, output); }
    );
    commandRegistered = true;
    logger.info(tr("entity_cleaner_command.registered"));
    return true;
}

} // namespace

bool enable() {
    if (commandRegistered) {
        return true;
    }

    try {
        return registerCommand();
    } catch (std::exception const& exception) {
        logger.error("Entity cleanup command registration failed: {}", exception.what());
        return false;
    }
}

void disable() {
    commandRegistered = false;
}

} // namespace my_mod::entity_cleaner_command

#else

namespace my_mod::entity_cleaner_command {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::entity_cleaner_command

#endif
