# 万象 (WanXiang)

基于 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 的基岩版服务端功能模组。支持通过 `config/config.json` 独立开关多项增强功能。

## 快速安装

1. 确认已安装与模组匹配的 LeviLamina（当前工程目标：`26.60.5`）。
2. 将发布包解压到服务端的 `plugins` 目录：
   ```text
   <服务器目录>/plugins/WanXiang/
   ├── WanXiang.dll
   ├── manifest.json
   └── lang/
   ```
3. 启动服务器生成 `plugins/WanXiang/config/config.json`。
4. 修改配置后，在控制台或游戏内执行 `/wanxiang reload` 应用大部分运行时功能。

> **注意**：命令注册、命令权限、未加载实体清理及活塞方块推动规则的修改需重启服务器生效。

---

## 功能一览

### 基础开关

| 配置项 | 功能描述 | 默认 |
| --- | --- | --- |
| `trustedSkinEnable` | 标记玩家皮肤为可信 | `true` |
| `autoFishingEnable` | 钓鱼成功后自动收杆并重新抛竿 | `false` |
| `autoReplenishEnable` | 主手物品耗尽或工具损坏后自动补充 | `true` |
| `containerPreviewEnable` | 右键受阻的箱子或陷阱箱时打开只读预览，支持双箱 | `false` |
| `quickHarvestEnable` | 右键成熟作物收割并原地复种，甘蔗、仙人掌、竹子和海带保留底部 | `false` |
| `abilityCommandEnable` | 强制注册原版 `/ability` 命令 | `true` |
| `msptCommandEnable` | 注册 `/mspt` MSPT/TPS 查询命令 | `true` |
| `vanishEnable` | 启用 OP 隐身命令 `/vanish` | `true` |
| `cleanUnknownBlocksEnable` | 区块加载时清理未知方块（不可恢复） | `false` |
| `hideOpPermissionEnable` | 对非 OP 隐藏客户端管理员权限显示 | `true` |
| `clientAchievementsEnable` | 向客户端隐藏作弊状态以允许成就判定 | `false` |

### 进阶模块

| 配置项 | 功能描述 | 默认 |
| --- | --- | --- |
| `sidebar` | 动态侧边栏，支持 Placeholder 与玩家开关持久化 | `enable=false` |
| `chatFormatter` | 自定义聊天格式，支持 `{author}`、`{message}`、Placeholder 和颜色代码 | `enable=false` |
| `entityCleaner` | 定时或超量清理已加载实体，支持保护规则与预告 | `enable=true` |
| `xpOrbMerge` | 合并指定半径内的经验球 | `enable=true` |
| `droppedItemLifetime` | 自定义默认及指定掉落物的存在时间 | `enable=false` |
| `customKnockback` | 分别调整玩家攻击玩家/生物的水平与垂直击退倍率 | `enable=false` |
| `customHurtInterval` | 分别设置玩家攻击玩家/非玩家生物的受击间隔，包含玩家投射物 | `enable=false` |
| `pistonControl` | 自定义方块推拉规则及实体活塞免疫 | `enable=false` |
| `mobEffectBlocker` | 阻止特定状态效果触发伤害/死亡附带行为 | `enable=false` |
| `fastLeafDecay` | 原木或树叶破坏后加速清理无支撑树叶 | `enable=false` |
| `scheduledCommands` | 按 6 段 Cron 表达式定时执行服务端命令 | `enable=false` |
| `commandPermissions` | 覆盖命令注册时的权限等级 | `enable=false` |
| `serverPong` | 修改服务器列表中的 MOTD 和人数显示 | `enable=false` |
| `clientLevelName` | 修改客户端进入世界时显示的存档名称 | `enable=false` |
| `coResourcePack` | 服务器材质包与玩家客户端材质包共存 | `enable=false` |
| `mobBlockDestruction` | 禁止凋灵和掠夺兽破坏方块 | `enable=false` |
| `unloadedEntitiesCleaner` | 开服时清理未加载的实体数据（仅执行一次） | `enable=false` |

---

## 常用配置示例

完整带注释配置请参考根目录的 [`config.jsonc`](config.jsonc)。以下为常见场景的精简配置片段（只需保留需要修改的字段，缺失字段会自动补全默认值）：

聊天格式配置示例：
```json
"chatFormatter": {
    "enable": true,
    "format": "<{author}> {message}"
}
```
`{author}` 为发送者名称，`{message}` 为聊天内容，支持 Placeholder 玩家占位符。

### 1. 动态侧边栏 (`sidebar`)
```json
"sidebar": {
    "enable": true,             // 总开关
    "defaultSidebarStatus": true,// 新玩家默认开启
    "updateInterval": 1000,      // 刷新间隔 (ms)，最小 50ms
    "dynamicTitles": ["§b万象"], // 轮播标题 (支持 Placeholder)
    "dynamicLines": [["§f欢迎来到服务器"]], // 显示内容 (支持 Placeholder)
    "lineGroupSwitchInterval": 5000 // 行组轮播间隔 (ms)
}
```
- 玩家可使用 `/sidebar on` 或 `/sidebar off` 控制显示（保存在 `sidebar.db`）。

### 2. 实体清理 (`entityCleaner`)
```json
"entityCleaner": {
    "enable": true,
    "dryRun": false,             // 测试模式 (true 时仅记录不清理)
    "threshold": {
        "enable": true,
        "maxEntities": 1000,     // 候选实体上限
        "targetEntities": 500,   // 清理后保留量
        "cooldownSeconds": 300   // 两次清理间隔 (s)
    },
    "filter": {
        "mode": "whitelist",     // 模式：whitelist(仅清名单内)/blacklist(保护名单内)
        "typeNames": ["minecraft:xp_orb", "minecraft:arrow"] // 候选类型
    },
    "protection": {
        "named": true,           // 保护命名实体
        "tamed": true,           // 保护已驯服实体
        "villagers": true,       // 保护村民
        "bosses": true,          // 保护 Boss
        "playerRadius": 32.0     // 保护玩家 32 格内实体
    }
}
```
- 手动管理：OP 可使用 `/entitycleaner` (`/eclean`) 执行面板。

### 3. 活塞控制 (`pistonControl`)
```json
"pistonControl": {
    "enable": true,
    "customPistonBlocks": {
        "minecraft:stone": { "movementType": 3, "stickyType": 0 }
    },
    "pistonImmuneEntities": ["minecraft:armor_stand", "minecraft:pig"]
}
```
- `movementType`：`0` 推拉、`1` 仅推、`2` 推毁、`3` 不可动。
- `stickyType`：`0` 无黏性、`1` 有黏性。
- 修改此功能需重启服务器。

### 4. 定时命令与权限覆盖
```json
"scheduledCommands": {
    "enable": true,
    "tasks": [
        { 
            "cron": "0 */5 * * * *", // 6 段：秒 分 时 日 月 星期
            "command": "list",       // 命令 (不带 /)
            "outputToConsole": true  // 是否输出至控制台
        }
    ]
},
"commandPermissions": {
    "enable": true,
    "rules": [
        { "command": "stop", "permissionLevel": "Admin" }
    ]
}
```

### 5. 击退、掉落物与经验球
```json
"customKnockback": {
    "enable": true,
    "player": { "horizontal": 1.2, "vertical": 0.8 }, // 击退倍率 (1.0 为原版)
    "mob": { "horizontal": 1.0, "vertical": 1.0 }
},
"customHurtInterval": {
    "enable": false,
    "player": 10, // 玩家受击后的 mInvulnerableTime，单位 tick，20 tick = 1 秒
    "mob": 10     // 非玩家生物受击后的 mInvulnerableTime，0 为无间隔，必须 >= 0
},                // 仅玩家近战及玩家发射的投射物生效
"droppedItemLifetime": {
    "enable": true,
    "defaultSeconds": 120,                           // 存活时间 (s)
    "items": {
        "minecraft:diamond_sword": 600,             // 精确 ID 优先，钻石剑保留 600 秒
        "minecraft:diamond_.*": 1200                // 其他以 minecraft:diamond_ 开头的物品保留 1200 秒
    }                                               // 正则匹配完整 ID，多条命中时按键名字典序取第一条
},
"xpOrbMerge": {
    "enable": true,
    "radius": 8.0,                                  // 合并半径
    "intervalTicks": 20                             // 检测间隔 (20 tick ≈ 1s)
}
```

### 6. 材质包共存 (`coResourcePack`)
```json
"coResourcePack": {
    "enable": true,
    "forceRequired": true,      // 强制下载
    "markAsAddonPack": true,    // 标记为扩展包
    "allowClientPacks": true    // 允许使用自身包
}
```


---

## 命令汇总

| 命令 | 权限 | 说明 |
| --- | --- | --- |
| `/wanxiang reload` | OP / 控制台 | 重载配置、日志等级与语言 |
| `/mspt` | 所有玩家 | 查看最近一次 tick 的 MSPT 与 TPS |
| `/vanish` | OP | 切换自身隐身状态 |
| `/sidebar <on\|off\|reload>` | 玩家 / OP | 开关个人侧边栏或重载配置 |
| `/entitycleaner` (`/eclean`) | OP | 打开手动实体清理面板 |

---

## 从源码构建

环境要求：Windows x64、[LLVM/Clang](https://llvm.org/) (`clang-cl`)、[xmake](https://xmake.io/)。

```powershell
xmake f -y -p windows -a x64 -m release --toolchain=clang-cl --target_type=server
xmake
```
构建产物位于 `bin/WanXiang/` 及 `bin/WanXiang.zip`。

## 许可

本项目采用 [CC0-1.0](LICENSE) 许可。
