#pragma once

namespace my_mod::piston_control {

// 方块推动规则只在启动时应用；实体免疫支持运行时重载。
void applyBlockConfig();
bool enable();
void disable();

} // namespace my_mod::piston_control
