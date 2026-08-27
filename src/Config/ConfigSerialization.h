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
#include <string_view>
#include <type_traits>
#include <utility> // for std::index_sequence


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

// 通用序列化模板，适用于所有聚合类型 (C++17 或更高版本)
template <typename T>
    requires std::is_aggregate_v<T>
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
     */
    static void from_json(const json& j, T& value) {
        const T        default_value{};
        constexpr auto field_count = boost::pfr::tuple_size_v<T>;

        // 同样使用编译时展开技术
        [&]<size_t... I>(std::index_sequence<I...>) {
            (([&] {
                 // 获取编译时成员名称
                 constexpr std::string_view name = boost::pfr::get_name<I, T>();

                 if (j.contains(name)) {
                     // 从 JSON 中获取值并赋给对应成员
                     j.at(name).get_to(boost::pfr::get<I>(value));
                 } else {
                     // 从默认构造的对象中获取默认值
                     boost::pfr::get<I>(value) = boost::pfr::get<I>(default_value);
                 }
             }()),
             ...); // 立即调用内部 lambda
        }(std::make_index_sequence<field_count>{});
    }
};

} // namespace nlohmann

