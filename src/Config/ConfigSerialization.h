#pragma once

// clang-cl defines _MSC_VER, which makes Boost.PFR 2.1 select the MSVC field-name parser.
#if defined(__clang__) && defined(_MSC_VER)
#define BOOST_PFR_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#define BOOST_PFR_CORE_NAME_PARSING                                                                                   \
    (sizeof("auto boost::pfr::detail::name_of_field_impl() [MsvcWorkaround = ") - 1, sizeof("}]") - 1, backward("."))
#endif

#include "Config.h"
#include "ll/api/io/LogLevel.h"
#include <nlohmann/json.hpp>

#include <boost/pfr.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility> // for std::index_sequence
#include <vector>


namespace my_mod::detail {

// Per-field deserialization failures, drained by ConfigManager::load once parsing finishes.
//
// The serializer below lives in namespace nlohmann and is pulled in by Config.h consumers, so it
// cannot reach for the mod logger without a circular include. Buffering the messages here keeps
// the header free of that dependency.
inline std::vector<std::string>& configFieldErrors() {
    static std::vector<std::string> errors;
    return errors;
}

// Opt-in list for the generic aggregate serializer defined below.
//
// The serializer is a *partial* specialization of nlohmann::adl_serializer. Constraining it on
// std::is_aggregate_v alone would hijack JSON serialization for every aggregate in the whole
// program -- std::array, MC/LeviLamina structs, anything a future header happens to drag in --
// silently overriding more correct handling. Listing our own types keeps the blast radius at
// exactly the config tree.
//
// Adding a struct to Config.h without listing it here is a compile error at its first use, not a
// silent fallback, so this cannot rot quietly.
template <typename T>
inline constexpr bool kIsSerializableConfig = false;

#define WANXIANG_SERIALIZABLE_CONFIG(TYPE)                                                                             \
    template <>                                                                                                        \
    inline constexpr bool kIsSerializableConfig<TYPE> = true

// ExplosionRuleConfig is deliberately absent: it has a hand-written full specialization below
// that handles its std::optional members.
WANXIANG_SERIALIZABLE_CONFIG(XpOrbMergeConfig);
WANXIANG_SERIALIZABLE_CONFIG(DroppedItemLifetimeConfig);
WANXIANG_SERIALIZABLE_CONFIG(MobEffectBlockerConfig);
WANXIANG_SERIALIZABLE_CONFIG(FastLeafDecayConfig);
WANXIANG_SERIALIZABLE_CONFIG(UnloadedEntitiesCleanerConfig);
WANXIANG_SERIALIZABLE_CONFIG(ScheduledCommandTaskConfig);
WANXIANG_SERIALIZABLE_CONFIG(ScheduledCommandsConfig);
WANXIANG_SERIALIZABLE_CONFIG(CommandPermissionRuleConfig);
WANXIANG_SERIALIZABLE_CONFIG(CommandPermissionsConfig);
WANXIANG_SERIALIZABLE_CONFIG(ServerPongConfig);
WANXIANG_SERIALIZABLE_CONFIG(ClientLevelNameConfig);
WANXIANG_SERIALIZABLE_CONFIG(CoResourcePackConfig);
WANXIANG_SERIALIZABLE_CONFIG(ExplosionControlConfig);
WANXIANG_SERIALIZABLE_CONFIG(MobBlockDestructionConfig);
WANXIANG_SERIALIZABLE_CONFIG(PistonBlockConfig);
WANXIANG_SERIALIZABLE_CONFIG(PistonControlConfig);
WANXIANG_SERIALIZABLE_CONFIG(EntityCleanerScheduleConfig);
WANXIANG_SERIALIZABLE_CONFIG(EntityCleanerThresholdConfig);
WANXIANG_SERIALIZABLE_CONFIG(EntityCleanerDroppedItemConfig);
WANXIANG_SERIALIZABLE_CONFIG(EntityCleanerFilterConfig);
WANXIANG_SERIALIZABLE_CONFIG(EntityCleanerProtectionConfig);
WANXIANG_SERIALIZABLE_CONFIG(EntityCleanerWarningConfig);
WANXIANG_SERIALIZABLE_CONFIG(EntityCleanerConfig);
WANXIANG_SERIALIZABLE_CONFIG(Config);

#undef WANXIANG_SERIALIZABLE_CONFIG

} // namespace my_mod::detail


namespace nlohmann {

template <>
struct adl_serializer<my_mod::ExplosionRuleConfig> {
private:
    template <typename T>
    static void writeOptional(json& j, char const* name, std::optional<T> const& value) {
        if (value.has_value()) {
            j[name] = *value;
        }
    }

    template <typename T>
    static void readOptional(
        json const&       j,
        char const*       name,
        char const*       legacyOverrideName,
        std::optional<T>& value
    ) {
        auto const legacyOverride = j.find(legacyOverrideName);
        if (legacyOverride != j.end() && !legacyOverride->template get<bool>()) {
            value.reset();
            return;
        }

        auto const entry = j.find(name);
        if (entry == j.end() || entry->is_null()) {
            value.reset();
            return;
        }
        value = entry->template get<T>();
    }

public:
    static void to_json(json& j, my_mod::ExplosionRuleConfig const& value) {
        j = json{{"sourceType", value.sourceType}, {"cancel", value.cancel}};
        writeOptional(j, "radius", value.radius);
        writeOptional(j, "fire", value.fire);
        writeOptional(j, "breaking", value.breaking);
        writeOptional(j, "allowUnderwater", value.allowUnderwater);
        writeOptional(j, "canToggleBlocks", value.canToggleBlocks);
        writeOptional(j, "damageScaling", value.damageScaling);
        writeOptional(j, "ignoreBlockResistance", value.ignoreBlockResistance);
        writeOptional(j, "maxResistance", value.maxResistance);
        writeOptional(j, "inWater", value.inWater);
        writeOptional(j, "totalDamage", value.totalDamage);
        writeOptional(j, "knockbackScaling", value.knockbackScaling);
    }

    static void from_json(json const& j, my_mod::ExplosionRuleConfig& value) {
        value.sourceType = j.value("sourceType", "minecraft:tnt");
        value.cancel     = j.value("cancel", false);
        readOptional(j, "radius", "overrideRadius", value.radius);
        readOptional(j, "fire", "overrideFire", value.fire);
        readOptional(j, "breaking", "overrideBreaking", value.breaking);
        readOptional(j, "allowUnderwater", "overrideAllowUnderwater", value.allowUnderwater);
        readOptional(j, "canToggleBlocks", "overrideCanToggleBlocks", value.canToggleBlocks);
        readOptional(j, "damageScaling", "overrideDamageScaling", value.damageScaling);
        readOptional(
            j,
            "ignoreBlockResistance",
            "overrideIgnoreBlockResistance",
            value.ignoreBlockResistance
        );
        readOptional(j, "maxResistance", "overrideMaxResistance", value.maxResistance);
        readOptional(j, "inWater", "overrideInWater", value.inWater);
        readOptional(j, "totalDamage", "overrideTotalDamage", value.totalDamage);
        readOptional(j, "knockbackScaling", "overrideKnockbackScaling", value.knockbackScaling);
    }
};

// 通用序列化模板，适用于 my_mod::detail 中显式登记的配置聚合类型
template <typename T>
    requires std::is_aggregate_v<T> && my_mod::detail::kIsSerializableConfig<T>
struct adl_serializer<T> {
    /**
     * @brief 将聚合类型 T 序列化为 JSON 对象。
     */
    static void to_json(json& j, const T& value) {
        j = json::object();
        // 获取结构体成员的数量
        constexpr auto field_count = boost::pfr::tuple_size_v<T>;

        // 使用 C++17 的模板 lambda 和折叠表达式在编译时遍历所有成员
        [&]<size_t... I>(std::index_sequence<I...>) {
            // ( expression, ... ) 是折叠表达式的语法
            // 它会为 I 的每一个值（0, 1, 2, ...）展开内部的表达式
            ((j[boost::pfr::get_name<I, T>()] = boost::pfr::get<I>(value)), ...);
        }(std::make_index_sequence<field_count>{});
    }

    /**
     * @brief 从 JSON 对象反序列化为聚合类型 T。
     *
     * 单个字段解析失败只影响该字段：记录一条错误、回退到默认值，其余字段照常读取。
     * 早先的实现让异常一路抛到 ConfigManager::load，一个手滑的类型错误
     * （例如 "radius": "8"）就会导致整个插件加载失败。
     */
    static void from_json(const json& j, T& value) {
        // 函数内静态：只构造一次，避免每次反序列化都重建整棵默认配置。
        static const T default_value{};
        constexpr auto field_count = boost::pfr::tuple_size_v<T>;

        // 同样使用编译时展开技术
        [&]<size_t... I>(std::index_sequence<I...>) {
            (([&] {
                 // 获取编译时成员名称
                 constexpr std::string_view name = boost::pfr::get_name<I, T>();

                 // find 一次即可，contains + at 是两次哈希查找。
                 const auto found = j.find(name);
                 if (found == j.end() || found->is_null()) {
                     boost::pfr::get<I>(value) = boost::pfr::get<I>(default_value);
                     return;
                 }

                 try {
                     found->get_to(boost::pfr::get<I>(value));
                 } catch (const json::exception& e) {
                     boost::pfr::get<I>(value) = boost::pfr::get<I>(default_value);
                     my_mod::detail::configFieldErrors().emplace_back(
                         std::string{name} + ": " + e.what()
                     );
                 }
             }()),
             ...); // 立即调用内部 lambda
        }(std::make_index_sequence<field_count>{});
    }
};

} // namespace nlohmann
