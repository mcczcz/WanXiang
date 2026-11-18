#include "mod/ChatFormatter.h"

#ifdef LL_PLAT_S

#include "mod/FeatureDiagnostics.h"
#include "mod/Global.h"

#include "PA/PlaceholderAPI.h"
#include "ll/api/memory/Hook.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketPayload.h"
#include "mc/server/ServerPlayer.h"

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace my_mod::chat_formatter {
namespace {

std::atomic_bool enabled{false};

struct ChatRewriteContext {
    bool                       active{};
    std::string                originalMessage;
    std::string                formattedMessage;
    std::optional<std::string> formattedFilteredMessage;
    std::string                xuid;
    std::string                platformId;
};

thread_local ChatRewriteContext chatRewrite;

struct ScopedChatRewrite {
    ChatRewriteContext saved;

    explicit ScopedChatRewrite(ChatRewriteContext context) : saved(std::move(chatRewrite)) {
        chatRewrite        = std::move(context);
        chatRewrite.active = true;
    }

    ~ScopedChatRewrite() { chatRewrite = std::move(saved); }
};

void replaceAll(std::string& text, std::string_view token, std::string const& value) {
    size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        text.replace(pos, token.size(), value);
        pos += value.size();
    }
}

std::string formatMessage(std::string const& author, std::string const& message, Player& player) {
    std::string result = config.chatFormatter.format;
    replaceAll(result, "{author}", author);
    replaceAll(result, "{message}", message);
    auto context = PA::PlayerContext::factory(&player);
    return PA::PA_GetPlaceholderService()->replace(result, context.get());
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
LL_TYPE_STATIC_HOOK(
    CreateChatHook,
    ll::memory::HookPriority::Normal,
    TextPacketPayload,
    &TextPacketPayload::createChat,
    TextPacket,
    std::string const&         author,
    std::string const&         message,
    std::optional<std::string> filteredMessage,
    std::string const&         xuid,
    std::string const&         platformId
) {
    if (chatRewrite.active && xuid == chatRewrite.xuid && platformId == chatRewrite.platformId
        && message == chatRewrite.originalMessage) {
        return origin("", chatRewrite.formattedMessage, chatRewrite.formattedFilteredMessage, xuid, platformId);
    }
    return origin(author, message, filteredMessage, xuid, platformId);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerChatHook,
    ll::memory::HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::$handle,
    void,
    NetworkIdentifier const& identifier,
    TextPacket const&        packet
) {
    if (!enabled.load(std::memory_order_relaxed) || !config.chatFormatter.enable
        || std::visit([](auto const& body) { return body.mType; }, packet.mBody.get()) != TextPacketType::Chat) {
        return origin(identifier, packet);
    }

    // $handle 的 this 可能指向 NetEventCallback 子对象，先调整回 ServerNetworkHandler。
    auto* player = thisFor<NetEventCallback>()->_getServerPlayer(identifier, packet.mSenderSubId);
    if (!player) {
        return origin(identifier, packet);
    }

    std::string author;
    std::string message;
    std::visit(
        [&](auto const& body) {
            if constexpr (requires { body.mAuthor; }) {
                author = body.mAuthor;
            }
            message = body.mMessage;
        },
        packet.mBody.get()
    );
    if (!message.empty() && message.front() == '/') {
        return origin(identifier, packet);
    }
    if (author.empty()) {
        author = player->getNameTag();
    }

    std::optional<std::string> formattedFiltered;
    if (auto const& filtered = packet.mFilteredMessage.get(); filtered.has_value()) {
        formattedFiltered = formatMessage(author, *filtered, *player);
    }

    ScopedChatRewrite guard(ChatRewriteContext{
        .originalMessage          = message,
        .formattedMessage         = formatMessage(author, message, *player),
        .formattedFilteredMessage = std::move(formattedFiltered),
        .xuid                     = packet.mXuid.get(),
        .platformId               = packet.mPlatformId.get(),
    });
    origin(identifier, packet);
}
#pragma clang diagnostic pop

} // namespace

bool enable() {
    if (!config.chatFormatter.enable || enabled.load(std::memory_order_relaxed)) {
        return true;
    }
    if (!feature_diagnostics::checkHook("chat_formatter", "CreateChatHook", CreateChatHook::hook())) {
        return false;
    }
    if (!feature_diagnostics::checkHook("chat_formatter", "PlayerChatHook", PlayerChatHook::hook())) {
        CreateChatHook::unhook();
        return false;
    }
    enabled.store(true, std::memory_order_relaxed);
    return true;
}

void disable() {
    if (!enabled.exchange(false, std::memory_order_relaxed)) {
        return;
    }
    PlayerChatHook::unhook();
    CreateChatHook::unhook();
}

} // namespace my_mod::chat_formatter

#else

namespace my_mod::chat_formatter {

bool enable() { return true; }
void disable() {}

} // namespace my_mod::chat_formatter

#endif
