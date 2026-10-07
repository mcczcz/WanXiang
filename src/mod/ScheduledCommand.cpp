#include "mod/ScheduledCommand.h"

#ifdef LL_PLAT_S

#include "mod/Global.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/deps/core/utility/MCRESULT.h"
#include "mc/server/ServerLevel.h"
#include "mc/server/commands/CommandContext.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/CurrentCmdVersion.h"
#include "mc/server/commands/MinecraftCommands.h"
#include "mc/server/commands/ServerCommandOrigin.h"
#include "mc/world/Minecraft.h"
#include "mc/world/level/dimension/DimensionType.h"

#include <croncpp.h>

#include <chrono>
#include <ctime>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace my_mod::scheduled_command {
namespace {

struct ScheduledTask {
    std::string    command;
    cron::cronexpr expression;
    std::time_t    nextRun;
    bool           outputToConsole;
};

ll::event::ListenerPtr     tickListener;
std::vector<ScheduledTask> scheduledTasks;
std::time_t                lastCheckedSecond{};
std::uint64_t              taskGeneration{};

void execute(ScheduledTask const& task, ServerLevel& level) {
    auto minecraft = ll::service::getMinecraft();
    if (!minecraft || !minecraft->mCommands) {
        logger.error(tr("scheduled_command.minecraft_unavailable", task.command));
        return;
    }

    auto origin = std::make_unique<ServerCommandOrigin>(
        "ScheduledCommand",
        level,
        CommandPermissionLevel::Owner,
        DimensionType{0}
    );
    CommandContext context(task.command, std::move(origin), static_cast<int>(CurrentCmdVersion::Latest));
    bool const     suppressOutput = !task.outputToConsole;
    auto const     result         = minecraft->mCommands->executeCommand(context, suppressOutput);
    (void)result;
}

void onServerTick(ll::event::ServerLevelTickEvent& event) {
    auto const now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (now == lastCheckedSecond) {
        return;
    }
    lastCheckedSecond = now;

    auto& level = static_cast<ServerLevel&>(event.level());
    for (auto& task : scheduledTasks) {
        if (now < task.nextRun) {
            continue;
        }

        auto const generation = taskGeneration;
        execute(task, level);
        // A scheduled reload can replace this vector while execute() is running.
        if (generation != taskGeneration) {
            return;
        }
        task.nextRun = cron::cron_next(task.expression, now);
    }
}

} // namespace

bool enable() {
    if (!config.scheduledCommands.enable || tickListener) {
        return true;
    }

    scheduledTasks.clear();
    auto const now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    for (auto const& taskConfig : config.scheduledCommands.tasks) {
        if (taskConfig.command.empty()) {
            logger.warn(tr("scheduled_command.empty_command"));
            continue;
        }

        try {
            auto expression = cron::make_cron(taskConfig.cron);
            scheduledTasks.emplace_back(
                ScheduledTask{
                    taskConfig.command,
                    expression,
                    cron::cron_next(expression, now),
                    taskConfig.outputToConsole,
                }
            );
        } catch (std::exception const& exception) {
            logger.error(tr("scheduled_command.invalid_cron", taskConfig.cron, taskConfig.command, exception.what()));
        }
    }

    if (scheduledTasks.empty()) {
        logger.warn(tr("scheduled_command.no_valid_tasks"));
        return true;
    }

    lastCheckedSecond = 0;
    tickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(onServerTick);
    if (!tickListener) {
        scheduledTasks.clear();
        return false;
    }

    logger.info(tr("scheduled_command.registered", scheduledTasks.size()));
    return true;
}

void disable() {
    ++taskGeneration;
    if (tickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::ServerLevelTickEvent>(tickListener);
        tickListener.reset();
    }
    scheduledTasks.clear();
    lastCheckedSecond = 0;
}

} // namespace my_mod::scheduled_command

#else

namespace my_mod::scheduled_command {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::scheduled_command

#endif
