#pragma once

class Actor;

namespace my_mod::entity_cleaner {

bool enable();
void disable();

// 管理员手动清理需要复用自动清理的实体类型保护判断。
bool isTypeProtectedByConfig(::Actor const& actor);

// 手动清理可选择是否套用自动清理的 whitelist/blacklist；该判断不会修改配置。
bool matchesConfiguredFilter(::Actor const& actor);

} // namespace my_mod::entity_cleaner
