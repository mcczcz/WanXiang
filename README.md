# 万象（WanXiang）

万象是一个基于 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 的基岩版服务端原生模组，提供一组可以独立开关的服务端增强功能。配置文件为 JSON（支持 `//` 和 `/* ... */` 注释），所有功能都在模组加载时根据配置启用。

## 功能一览

| 配置项 | 功能 | 默认值 |
| --- | --- | --- |
| `trustedSkinEnable` | 将玩家皮肤标记为可信任皮肤 | `true` |
| `autoFishingEnable` | 钓鱼成功后自动收杆并重新抛竿 | `false` |
| `abilityCommandEnable` | 强制注册原版 `/ability` 命令 | `true` |
| `msptCommandEnable` | 注册 `/mspt` MSPT/TPS 查询命令 | `true` |
| `vanishEnable` | 启用仅 OP 可用的 `/vanish` 隐身命令，支持重载 | `true` |
| `cleanUnknownBlocksEnable` | 区块加载时清理无法识别的方块 | `false` |
| `unloadedEntitiesCleaner` | 开服时清理未加载的实体数据（仅执行一次） | `enable=false` |
| `hideOpPermissionEnable` | 对非管理员隐藏客户端的 OP 权限信息 | `true` |
| `droppedItemLifetime` | 修改默认及指定掉落物的存在时间 | `enable=false` |
| `xpOrbMerge` | 合并范围内的经验球 | `enable=true` |
| `mobEffectBlocker` | 禁止指定状态效果触发伤害/死亡附带行为 | `enable=false` |
| `fastLeafDecay` | 原木或树叶被破坏后加速清理无支撑树叶 | `enable=false` |
| `scheduledCommands` | 按 Cron 表达式执行服务端命令 | `enable=false` |
| `commandPermissions` | 覆盖命令注册时的权限等级 | `enable=false` |
| `serverPong` | 修改服务器列表中的 MOTD 和人数 | `enable=false` |
| `clientLevelName` | 修改客户端进入世界时显示的存档名称 | `enable=false` |
| `coResourcePack` | 服务器材质包与玩家自己的材质包共存 | `enable=false` |
| `mobBlockDestruction` | 禁止凋灵和掠夺兽破坏方块 | `enable=false` |
| `customKnockback` | 分别调整玩家攻击玩家和其他生物时的水平、垂直击退倍率 | `enable=false` |
| `pistonControl` | 自定义方块推拉规则及实体活塞免疫 | `enable=false` |
| `entityCleaner` | 超量时清理远离玩家的经验球和箭矢，清理前全服预告 | `enable=true` |
| `sidebar` | 动态侧边栏，支持 Placeholder 占位符和玩家显示开关持久化 | `enable=false` |

## 侧边栏


在万象的 `config/config.json` 中配置 `sidebar`，设置 `enable=true` 后执行 `/wanxiang reload` 即可开启：

```json
"sidebar": {
    "enable": true,
    "defaultSidebarStatus": true,
    "updateInterval": 1000,
    "dynamicTitles": ["§b万象"],
    "dynamicLines": [["§f欢迎来到服务器"]],
    "currentLineGroupIndex": 0,
    "lineGroupSwitchInterval": 5000
}
```

- `/sidebar on`、`/sidebar off`：玩家开启或立即关闭自己的侧边栏。
- `/sidebar reload`：管理员或控制台重载万象配置，等同于 `/wanxiang reload`。
- `updateInterval`、`lineGroupSwitchInterval` 的单位均为毫秒；刷新间隔最小为 50 毫秒。
- `dynamicLines` 中的每个数组是一组显示行；多组按间隔轮播，标题按当前行组索引循环选取。标题和显示行均支持 Placeholder 占位符。
- 玩家开关保存在万象数据目录的 `sidebar.db`，重新登录或重载后保留。`defaultSidebarStatus` 只影响没有保存过开关的新玩家。

迁移旧配置时，将原 `sidebar` 中的标题、行组和切换字段复制到万象的 `sidebar`，再将原 `enablePlugin`、`defaultSidebarStatus`、`update_interval` 分别对应填入 `sidebar.enable`、`sidebar.defaultSidebarStatus`、`sidebar.updateInterval`。原 `threadPoolSize` 不再使用，显示刷新沿用服务端线程调度。若要保留原玩家开关，停服后将原插件数据目录的 `sidebar.db` 复制到万象数据目录。

## 安装

### 使用已编译文件

1. 确认服务端已安装与模组匹配的 LeviLamina（当前工程服务端目标按 `26.60.5` 编译）。
2. 将发布包解压到服务端的 `plugins` 目录，保持目录结构。例如：

   ```text
   <服务器目录>/plugins/WanXiang/WanXiang.dll
   <服务器目录>/plugins/WanXiang/manifest.json
   <服务器目录>/plugins/WanXiang/lang/zh_CN.json
   <服务器目录>/plugins/WanXiang/lang/en_US.json
   ```

3. 启动一次服务器。模组会自动创建 `plugins/WanXiang/config/config.json`。
4. 修改配置后执行 `/wanxiang reload` 重载运行时功能；命令注册、命令权限、未加载实体清理和活塞方块推动规则的修改仍需重启服务器。

如果使用源码构建，构建产物会在 `bin/` 目录生成，通常直接使用 `bin/WanXiang.zip` 安装即可。

## 配置文件

### 完整示例

下面是适合原版生存服的默认配置：保留原版钓鱼、状态效果、树叶腐烂和生物破坏方块规则，关闭开服数据库清理和未知方块清理，开启经验球合并与保守的超量清理。默认语言为中文。项目根目录的 [`config.example.json`](config.example.json) 提供了带中文注释、可直接使用的完整版本。可以只保留需要修改的字段，缺失字段会使用代码中的默认值并在启动时自动补回原文件。

默认值只影响新生成的配置和旧配置中缺失的字段，不会覆盖已有配置中的设置；已有服务器可参考完整示例调整后重载配置。

```json
{
    "version": 24,
    "logLevel": 3,
    "language": "zh_CN",

    "trustedSkinEnable": true,
    "autoFishingEnable": false,
    "abilityCommandEnable": true,
    "msptCommandEnable": true,
    "vanishEnable": true,
    "cleanUnknownBlocksEnable": false,
    "hideOpPermissionEnable": true,
    "sidebar": {
        "enable": false,
        "defaultSidebarStatus": true,
        "updateInterval": 1000,
        "dynamicTitles": ["§b万象"],
        "dynamicLines": [["§f欢迎来到服务器"]],
        "currentLineGroupIndex": 0,
        "lineGroupSwitchInterval": 5000
    },

    "droppedItemLifetime": {
        "enable": false,
        "defaultSeconds": 300,
        "items": {}
    },
    "customKnockback": {
        "enable": false,
        "player": { "horizontal": 1.0, "vertical": 1.0 },
        "mob": { "horizontal": 1.0, "vertical": 1.0 }
    },

    "xpOrbMerge": {
        "enable": true,
        "radius": 8.0,
        "intervalTicks": 20
    },
    "mobEffectBlocker": {
        "enable": false,
        "weaving": true,
        "infested": true,
        "oozing": true,
        "windCharged": false
    },
    "fastLeafDecay": {
        "enable": false,
        "minDelayTicks": 2,
        "maxDelayTicks": 8
    },
    "unloadedEntitiesCleaner": {
        "enable": false,
        "entityTypeNamesToClean": [],
        "logCleanedEntityTypeCounts": true,
        "logSkippedEntityTypeCounts": true,
        "writeCleanupResultToTextLog": false,
        "cleanupResultTextLogFile": "cleanup-result.log"
    },
    "scheduledCommands": {
        "enable": false,
        "tasks": []
    },
    "commandPermissions": {
        "enable": false,
        "rules": []
    },
    "serverPong": {
        "enable": false,
        "motdEnable": false,
        "motd": "",
        "playerCountEnable": false,
        "playerCount": 0,
        "maxPlayerCountEnable": false,
        "maxPlayerCount": 0
    },
    "clientLevelName": {
        "enable": false,
        "name": ""
    },
    "coResourcePack": {
        "enable": false,
        "forceRequired": true,
        "markAsAddonPack": true,
        "allowClientPacks": true
    },
    "pistonControl": {
        "enable": false,
        "customPistonBlocks": {},
        "pistonImmuneEntities": []
    },
    "mobBlockDestruction": {
        "enable": false,
        "wither": true,
        "ravager": true
    },
    "explosionControl": {
        "enable": false,
        "rules": []
    },
    "entityCleaner": {
        "enable": true,
        "dryRun": false,
        "schedule": {
            "enable": false,
            "cron": "0 */10 * * * *"
        },
        "threshold": {
            "enable": true,
            "maxEntities": 1000,
            "targetEntities": 500,
            "checkIntervalTicks": 100,
            "cooldownSeconds": 300,
            "maxMspt": 0.0
        },
        "droppedItem": {
            "protectAboveAge": 5900,
            "whitelist": [
                "minecraft:netherite_helmet", "minecraft:netherite_sword", "minecraft:netherite_chestplate",
                "minecraft:diamond_helmet", "minecraft:netherite_leggings", "minecraft:undyed_shulker_box",
                "minecraft:netherite_boots", "minecraft:shulker_box", "minecraft:elytra",
                "minecraft:dragon_egg", "minecraft:nether_star", "minecraft:diamond_sword",
                "minecraft:diamond_chestplate", "minecraft:diamond_boots", "minecraft:diamond_leggings",
                "minecraft:diamond_hoe", "minecraft:netherite_hoe", "minecraft:diamond_axe",
                "minecraft:netherite_axe", "minecraft:netherite_pickaxe", "minecraft:diamond_pickaxe"
            ]
        },
        "filter": {
            "mode": "whitelist",
            "typeNames": ["minecraft:xp_orb", "minecraft:arrow"],
            "families": []
        },
        "protection": {
            "named": true,
            "tamed": true,
            "leashed": true,
            "vehicles": true,
            "persistent": true,
            "villagers": true,
            "builds": true,
            "bosses": true,
            "fallingBlocks": true,
            "movingBlocks": true,
            "fishingHooks": true,
            "ominousItemSpawners": true,
            "playerRadius": 32.0
        },
        "warning": {
            "enable": true,
            "secondsBefore": [15],
            "message": "",
            "broadcastResult": true,
            "resultMessage": ""
        },
        "logResult": true
    }
}
```

### 自定义击退：`customKnockback`

调整玩家攻击造成的击退，按受击对象分开配置：`player` 用于受击玩家，`mob` 用于其他受击生物。默认关闭，修改开关或倍率后执行 `/wanxiang reload` 生效。

```json
"customKnockback": {
    "enable": true,
    "player": { "horizontal": 1.2, "vertical": 0.8 },
    "mob": { "horizontal": 1.0, "vertical": 1.0 }
}
```

`horizontal` 控制水平倍率，`vertical` 控制垂直倍率。`1` 保留原版，`0` 表示这次击退不改变对应方向的速度，`2` 表示两倍速度变化；数值必须为非负有限数。倍率作用于原版计算出的本次速度变化，保留原版击退抗性、附魔和垂直限速计算，不会额外放大受击前已有的速度。

仅处理击退来源实体为玩家的调用；生物攻击、无玩家来源的击退以及爆炸控制仍使用原来的逻辑。

### 生物方块破坏：`mobBlockDestruction`

默认关闭，保留凋灵和掠夺兽破坏方块的原版行为。启用下面的配置后会阻止这两类生物破坏方块；总开关或单个实体开关设为 `false` 后，重载配置即可恢复对应行为。

```json
"mobBlockDestruction": {
    "enable": true,
    "wither": true,
    "ravager": true
}
```

`logLevel` 是 LeviLamina 的整数枚举：`-1=Off`、`0=Fatal`、`1=Error`、`2=Warn`、`3=Info`、`4=Debug`、`5=Trace`。推荐使用默认值 `3`。

`language` 目前随包提供 `zh_CN` 和 `en_US`。找不到指定语言时会回退到中文；也可以在 `lang/` 目录放入同格式的 `<语言代码>.json` 扩展翻译。

### 活塞控制：`pistonControl`

整合 PistonControl 的方块推动规则和实体活塞免疫功能，默认关闭。将原插件的 `customPistonBlocks` 和 `pistonImmuneEntities` 移到 `pistonControl` 内，并将 `enable` 设为 `true` 即可使用。

```json
"pistonControl": {
    "enable": true,
    "customPistonBlocks": {
        "minecraft:stone": { "movementType": 3, "stickyType": 0 },
        "minecraft:glass": { "movementType": 1, "stickyType": 0 }
    },
    "pistonImmuneEntities": ["minecraft:armor_stand", "minecraft:pig"]
}
```

| 字段 | 说明 |
| --- | --- |
| `enable` | 活塞控制总开关，默认 `false` |
| `customPistonBlocks` | 方块完整 ID 到推动规则的映射；空对象表示不修改 |
| `movementType` | `0` 可推动和拉回，`1` 只能推动，`2` 推动时破坏，`3` 不可移动 |
| `stickyType` | `0` 无黏性，`1` 黏性 |
| `pistonImmuneEntities` | 实体完整 ID 数组；空数组表示不启用免疫，`["*"]` 匹配所有实体 |

免疫名单中的实体不会被活塞推动，并可穿过移动方块及伸出的活塞臂，活塞本体仍保持碰撞。实体免疫名单及其开关可通过 `/wanxiang reload` 生效。方块规则在启动时应用到注册表中的方块状态；修改、删除规则或关闭已经应用的方块规则后，需要重启服务器。

### 单项开关功能

这些功能只有一个布尔开关，改为 `false` 即可关闭：

```json
{
    "trustedSkinEnable": false,
    "autoFishingEnable": false,
    "abilityCommandEnable": false,
    "msptCommandEnable": false,
    "cleanUnknownBlocksEnable": false,
    "hideOpPermissionEnable": false
}
```

- **可信任皮肤（`trustedSkinEnable`）**：将服务端判定的玩家皮肤标记为可信任皮肤。只影响皮肤信任标记，不会修改皮肤文件。
- **自动钓鱼（`autoFishingEnable`）**：鱼钩成功钓到物品后自动执行收杆/重新抛竿动作。玩家仍需正常抛出鱼竿，建议先在测试世界验证与其他钓鱼插件的兼容性。
- **Ability 命令（`abilityCommandEnable`）**：在服务端注册原版 `/ability` 命令，即使当前不是教育版模式也会尝试注册。启用后可按原版语法使用，例如 `/ability Steve mayfly true`。
- **MSPT/TPS 命令（`msptCommandEnable`）**：注册 `/mspt` 查询命令。关闭后不会注册该命令。
- **隐身（`vanishEnable`）**：仅 OP 可用，执行 `/vanish` 切换自身隐身。修改后执行 `/wanxiang reload` 生效；重载会结束所有玩家当前的隐身状态。关闭后命令保留，但无法使用。
- **未知方块清理（`cleanUnknownBlocksEnable`）**：区块加载时，将被游戏标记为“无法识别状态”的方块替换为空气，并在日志中记录清理数量。该操作不可恢复，建议先备份世界。
- **未加载实体清理（`unloadedEntitiesCleaner`）**：默认关闭。启用后在服务端启动完成时扫描世界数据库中的实体记录并清理，每次进程启动只执行一次。`entityTypeNamesToClean=[]` 表示删除全部未加载实体，非空数组可限制清理的实体类型（例如 `minecraft:item`）；该操作不可恢复，建议先备份世界。
- **隐藏 OP 权限（`hideOpPermissionEnable`）**：向非 OP 客户端发送能力数据包时，将玩家权限伪装为普通成员；OP 玩家收到原始权限数据。它只隐藏客户端显示，不会真正改变服务端权限。

### MSPT/TPS 查询命令：`/mspt`

玩家执行 `/mspt` 可查看最近一次服务端 tick 的耗时（MSPT）和按该耗时计算的 TPS。TPS 最高显示为 20；服务器尚未完成 tick 统计时显示为 20 TPS、0 MSPT。

### 配置重载命令：`/wanxiang reload`

OP 玩家执行 `/wanxiang reload`，或在服务端控制台执行 `wanxiang reload`，可重新读取 `config/config.json` 并应用运行时功能开关和参数，同时更新日志等级和当前语言。无需额外配置开关。

配置文件无法读取或 JSON 解析失败时，当前配置和功能保持不变；功能重新启用失败时，会尝试恢复旧配置并在日志中记录原因。字段类型错误仍沿用配置加载规则，回退到该字段的默认值。

重载会取消尚未执行的树叶腐烂回调、实体清理倒计时，并重新计算定时任务的下一次执行时间。未加载实体数据库清理不会因重载再次执行。客户端存档名称和资源包选项在后续发送对应数据包时生效，已在线玩家通常需要重新加入。

`abilityCommandEnable`、`msptCommandEnable`、`commandPermissions`、`unloadedEntitiesCleaner` 和 `pistonControl.customPistonBlocks` 的修改仍需重启服务器；重载不会重新注册已有命令或修改其权限要求，也不会重新应用或恢复活塞方块推动规则。语言文件内容的修改也需重启，重载只切换 `language` 指定的已加载语言。

### 掉落物存在时间：`droppedItemLifetime`

修改新生成掉落物的存在时间。未列入 `items` 的掉落物使用 `defaultSeconds`，指定物品使用自身的覆盖值；物品名必须填写包含命名空间的完整标识。秒数小于 `0` 时按 `0` 处理，掉落物会在下一次更新时消失。

```json
"droppedItemLifetime": {
    "enable": true,
    "defaultSeconds": 120,
    "items": {
        "minecraft:diamond": 600,
        "minecraft:nether_star": 1800
    }
}
```

此示例让普通掉落物存在 2 分钟、钻石存在 10 分钟、下界之星存在 30 分钟。该设置只影响启用功能后新生成的掉落物。

### 经验球合并：`xpOrbMerge`

在同一维度内，将距离不超过 `radius` 的经验球合并为一个，经验总量保持不变，并移动到参与合并经验球的中心位置。

| 字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `enable` | 布尔 | `true` | 是否启用 |
| `radius` | 数字 | `8.0` | 合并半径，必须为非负数；非法或负数时本功能跳过 |
| `intervalTicks` | 整数 | `20` | 检查间隔（游戏刻）；小于 `1` 会按 `1` 处理，20 刻约等于 1 秒 |

示例：每 10 刻合并半径 5 格内的经验球。

```json
"xpOrbMerge": {
    "enable": true,
    "radius": 5.0,
    "intervalTicks": 10
}
```

### 状态效果阻止：`mobEffectBlocker`

用于阻止特定状态效果在实体受伤或死亡时触发的附带行为：`infested` 控制受伤触发，其他三个字段控制死亡触发。

| 字段 | 默认值 | 被阻止的效果 |
| --- | --- | --- |
| `enable` | `false` | 总开关；默认保留原版效果行为 |
| `weaving` | `true` | Weaving（编织） |
| `infested` | `true` | Infested（寄生）受伤触发 |
| `oozing` | `true` | Oozing（渗浆） |
| `windCharged` | `false` | Wind Charged（风袭） |

示例：保留编织效果，允许渗浆和风袭的原版行为。

```json
"mobEffectBlocker": {
    "enable": true,
    "weaving": true,
    "infested": true,
    "oozing": false,
    "windCharged": false
}
```

### 树叶快速腐烂：`fastLeafDecay`

原木或树叶被移除后，模组会在附近扫描并立即破坏没有原木/木头支撑的非持久树叶。`radius` 会被限制在 `1` 到 `16` 之间；支撑搜索最多继续检查 6 格，且不会为了本功能加载未加载区块。

```json
"fastLeafDecay": {
    "enable": true,
    "radius": 10
}
```

半径越大，单次树叶更新需要检查的方块越多。大型服务器建议从默认值 `8` 开始，根据性能逐步调整。

### 定时命令：`scheduledCommands`

启用后，模组以服务端 Owner 权限执行任务。每个任务使用 6 段 Cron 表达式，顺序为：

```text
秒 分 时 月内日期 月 星期
```

`command` 填命令内容，建议不要带开头的 `/`；`outputToConsole=true` 时保留命令输出，设为 `false` 可抑制输出。任务从模组启动后的下一次匹配时间开始执行。

示例：每 5 分钟执行一次 `list`，每天 04:00:00 保存世界，并且不把保存命令输出到控制台。

```json
"scheduledCommands": {
    "enable": true,
    "tasks": [
        {
            "cron": "0 */5 * * * *",
            "command": "list",
            "outputToConsole": true
        }
    ]
}
```

空命令会被跳过，Cron 解析失败的任务会记录错误但不会阻止其他任务注册。命令由服务端执行，不代表某个玩家，因此不能使用需要玩家上下文的语法。

### 命令权限覆盖：`commandPermissions`

此功能拦截命令注册过程，并按命令名替换权限等级。命令名不区分大小写，可带或不带 `/`；同名规则以后出现的值为准。它只影响命令注册时的要求，修改后需要重启服务器。

支持的权限等级：`Any`、`GameDirectors`、`Admin`、`Host`、`Owner`、`Internal`（大小写不敏感）。

示例：允许所有玩家执行 `/gamemode`，并要求管理员权限执行 `/ability`。

```json
"commandPermissions": {
    "enable": true,
    "rules": [
        {
            "command": "stop",
            "permissionLevel": "Admin"
        }
    ]
}
```

空命令名或无效权限等级的规则会被跳过；如果没有有效规则，Hook 不会注册。

### 服务器列表信息：`serverPong`

修改客户端服务器列表中收到的 MOTD、当前人数和最大人数。总开关和三个字段开关需要分别启用；未启用的字段保持服务端原值。

```json
"serverPong": {
    "enable": true,
    "motdEnable": true,
    "motd": "§a生存服务器 | WanXiang",
    "playerCountEnable": true,
    "playerCount": 12,
    "maxPlayerCountEnable": true,
    "maxPlayerCount": 100
}
```

### 客户端存档名称：`clientLevelName`

启用后，服务端发送 `StartGamePacket` 时只修改发给客户端的存档名称，世界实际保存名称和服务端路径不变。名称为空时不会注册 Hook。

```json
"clientLevelName": {
    "enable": true,
    "name": "我的服务器"
}
```

### 资源包共存：`coResourcePack`

让服务器下发的材质包和玩家自己在客户端启用的材质包同时生效。

原版协议里，「客户端必须下载服务器材质包」和「进服后禁用玩家自己的材质包」共用同一个来源（`server.properties` 的 `texturepack-required`），开一个就等于开另一个。本功能在封包序列化的瞬间把这两件事拆开，一共只改三个字段：

| 开关 | 改动的字段 | 作用 |
| --- | --- | --- |
| `forceRequired` | `ResourcePacksInfoPacket` 的 `mResourcePackRequired` 置 `true` | 客户端必须下载服务器的包，没有跳过选项 |
| `markAsAddonPack` | 每个 `PackInfoData` 的 `mIsAddonPack` 置 `true` | 客户端把这些包当作服务器附带内容，而不是玩家自选的材质包 |
| `allowClientPacks` | `StartGamePacket` 中 `LevelSettings` 的 `mTexturePacksRequired` 置 `false` | 客户端不再因为「世界要求材质包」而停用玩家自己的包 |

三个开关默认全开，通常保持默认即可；单独关掉任意一个只影响对应的那一条。全部关掉时不会注册任何 Hook。

```json
"coResourcePack": {
    "enable": true,
    "forceRequired": true,
    "markAsAddonPack": true,
    "allowClientPacks": true
}
```

注意事项：

- `forceRequired=true` 会覆盖 `server.properties` 里的 `texturepack-required=false`，所有玩家都会被要求下载服务器材质包。不想强制下载就把它设为 `false`。
- 改动只作用于发给客户端的封包，服务端自身的材质包配置和存档不受影响；写完封包后字段会立即还原，关闭功能后无残留。
- 最终表现取决于客户端如何处理 `isAddonPack`，不同客户端版本可能有差异，建议上线前实测。

### 实体清理：`entityCleaner`

在服务器运行期间控制实体总量。与只在开服时扫描存档的 `unloadedEntitiesCleaner` 不同，本功能清理的是**已加载在世界里**的实体。默认 `enable=true`、`dryRun=false`，仅开启阈值触发：远离玩家至少 32 格且通过保护规则的经验球和箭矢超过 1000 个时，提前 15 秒预告后清理到最多 500 个，两次阈值清理至少间隔 300 秒。默认关闭定时清理和 MSPT 触发。

默认名单不含掉落物或生物 family，死亡掉落物按原版时间消失，村民、动物及刷怪农场中的生物不会被自动清理。需要清理掉落物或怪物时，可自行向 `filter.typeNames` 添加 `minecraft:item` 或向 `filter.families` 添加 `monster`。

#### 触发方式

两种触发可以同时开启：

- **定时（`schedule`）**：按 Cron 表达式触发，语法与 `scheduledCommands` 相同（6 位，含秒）。清理**全部**候选实体。
- **超量（`threshold`）**：每 `checkIntervalTicks` 个游戏刻检查一次，候选实体数超过 `maxEntities`，或最近一次服务端 tick 的 MSPT 超过 `maxMspt`（毫秒，设为 `0` 关闭）时触发；按「离玩家最远的先删」只删到 `targetEntities` 为止，玩家眼前的实体尽量保留。`cooldownSeconds` 是两次阈值清理的最小间隔，避免候选不足或 MSPT 持续偏高时反复触发刷屏。

掉落物候选还会应用 `droppedItem` 保护：掉落物的 `age` 从 `6000` 向下递减，`age > protectAboveAge` 时不会被自动清理。默认 `protectAboveAge=5900`，即保护刚生成后的约 100 tick（5 秒）；设为 `0` 可关闭年龄保护。白名单 `whitelist` 中的完整物品标识始终不会被自动清理。`/entitycleaner` 手动清理的确认页提供两个独立开关，可分别选择是否保留新掉落物、是否保留白名单掉落物。

> `maxEntities` 比较的是**候选实体数**（即真正会被删的那些），不是世界里的实体总数。否则一个有 2500 个受保护村民的服务器会永远触发、永远清不到目标。

#### `protection` 保护项详解

一个实体必须**同时通过以下四层筛选**才会被删除：

玩家始终不会进入清理候选，不受以下配置开关影响。

1. **实体类型保护（`protection`）**：按实体类型跳过清理，两种名单模式下都生效。以下开关默认保护那些删掉后可能破坏机制或造成资产损失的实体：

   | 开关 | 默认值 | 保护对象 | 关闭后的行为 |
   | --- | --- | --- | --- |
   | `villagers` | `true` | `Villager`、`VillagerV2`、`WanderingTrader` | 村民和流浪商人仍可能按名单被清理；卫道士、唤魔者等不会因为此开关自动受保护 |
   | `builds` | `true` | 盔甲架、画、末影水晶、NPC、拴绳结，以及所有矿车和船的变体 | 这些实体只要通过其他筛选，就可能被清理；矿车/船内的物品也可能因此丢失 |
   | `bosses` | `true` | 末影龙、凋灵 | Boss 只要通过其他筛选，就可能被清理 |
   | `fallingBlocks` | `true` | 正在下落的方块实体（沙子、砂砾、铁砧等） | 可能打断方块落地或相关机制 |
   | `movingBlocks` | `true` | 活塞推动中的方块实体 | 可能打断活塞运动和红石机制 |
   | `fishingHooks` | `true` | 鱼钩实体 | 可能打断正常钓鱼，也可能影响本插件的自动钓鱼 |
   | `ominousItemSpawners` | `true` | 试炼密室的不祥之瓶战利品生成器 | 可能导致试炼密室战利品生成流程异常 |

2. **运行时状态保护（`protection`）**：与实体类型无关，两种名单模式下都生效。

   | 开关 | 默认值 | 判断条件 | 关闭后的行为 |
   | --- | --- | --- | --- |
   | `named` | `true` | 名称标签不为空的实体（通常是命名牌命名过的实体） | 命名实体只要通过其他筛选，就可能被清理 |
   | `tamed` | `true` | `isTame()` 为真的已驯服实体 | 已驯服宠物或坐骑只要通过其他筛选，就可能被清理 |
   | `leashed` | `true` | 当前被拴绳连接的实体 | 被拴住的实体只要通过其他筛选，就可能被清理 |
   | `vehicles` | `true` | 载有乘客，或正在骑乘另一个实体的实体 | 载具、乘客或骑乘中的实体只要通过其他筛选，就可能被清理 |
   | `persistent` | `true` | 带有持久化标记、不会自然消失的实体；刷怪蛋或召唤命令生成的生物通常属于此类 | 持久化实体只要通过其他筛选，就可能被清理 |

3. **玩家距离保护（`playerRadius`）**：同一维度内，实体到最近玩家的距离小于该值时跳过清理。`0`、负数或非有限数值表示关闭；没有玩家的维度不会触发这项保护。玩家本身仍始终不会进入清理候选。

4. **管理员名单（`filter`）**：
   - `"mode": "whitelist"`（默认）——**只清理**名单内的类型/family。
   - `"mode": "blacklist"`——名单内**受保护**，其余全部清理。

   `typeNames` 匹配完整实体标识（如 `minecraft:zombie`），`families` 匹配实体的 family（如 `monster`、`animal`）。

> 默认取 `whitelist`：名单写错或清空只会导致「什么都不删」（服务器变卡，可恢复）；而 `blacklist` 的同类错误意味着「删除一切未列出的」（玩家资产没了，不可恢复）。
>
> `mode` 拼错（例如 `blacklsit`）时本功能会直接关闭并报错，**不会**回退到任一模式。

#### 清理预告

`warning.secondsBefore` 里的每个秒数都会在清理前向全服广播一次倒计时。列表会自动去重、降序排序并丢弃非正数；设为 `[]` 或 `warning.enable=false` 表示触发即清理，不预告。服务器无人在线时照常清理，只是跳过广播。

默认提前 15 秒预告一次；例如 `[15, 5, 3]` 表示提前 15、5、3 秒各提醒一次。`warning.message` 可自定义预告文案，`{seconds}` 替换为剩余秒数；`warning.resultMessage` 可自定义完成文案，`{count}` 替换为实际清理数量。两者均支持 Minecraft 的 `§` 颜色代码，也兼容单个 `{}` 占位符；空字符串使用当前语言文件的默认提示。格式错误会记录警告并回退到默认提示。

`warning.broadcastResult` 独立控制完成广播，不受 `warning.enable` 影响；没有删除实体或 `dryRun=true` 时不广播完成消息。这些配置用于定时/阈值自动清理；OP 手动清理仍即时执行并向操作者反馈结果。修改配置后执行 `/wanxiang reload` 生效，旧配置的缺失字段会在加载时自动补齐，原有倒计时秒数会保留。

例如，下面的配置会显示 15 秒预告和清理数量提示：

```json
"warning": {
    "enable": true,
    "secondsBefore": [15],
    "message": "§e[清理] §f{seconds} 秒后将清理多余实体。",
    "broadcastResult": true,
    "resultMessage": "§e[清理] §f已清理 {count} 个实体。"
}
```

#### 上线前请先演练

`dryRun` 设为 `true` 时会完整跑一遍筛选流程并在日志里输出按类型分组的统计，但**不实际删除**任何实体。由于清理不可恢复，建议先这样验证一轮：

```json
"entityCleaner": {
    "enable": true,
    "dryRun": true,
    "schedule": { "enable": true, "cron": "*/30 * * * * *" }
}
```

确认日志里的清理清单符合预期后，再把 `dryRun` 改回 `false`。

#### OP 手动管理命令：`/entitycleaner`

拥有 OP 权限的玩家可以使用 `/entitycleaner`（别名 `/eclean`）打开实体管理面板。面板会统计**所有已加载维度**内的非玩家实体，每种实体类型显示一个复选开关，支持一次选择多种类型；这里不会因为自动清理的 `whitelist` / `blacklist` 隐藏实体。

提交后会打开第二个确认表单，显示即将处理的实体类型和实时数量，并允许设置以下本次过滤/保留项：

- 本次是否应用自动清理的 `whitelist` / `blacklist`（默认关闭；不会修改配置文件）；
- 配置中定义的类型保护（Boss、村民、建筑、矿车、船和机制实体）；
- 新掉落物年龄保护和掉落物白名单保护（两个独立开关，默认均开启）；
- 命名、驯服、拴绳、载具/乘客和持久化实体；
- 玩家附近实体（使用 `protection.playerRadius`）。

刷怪蛋生成的苦力怕通常带有持久化标记；要清理它们，请取消勾选“保留持久化实体”，或在配置中将 `protection.persistent` 设为 `false`：

```json
"persistent": false
```

只有在第二个表单点击“确定清理”后才会执行。玩家始终不会被清理。此命令针对所有已加载维度的实体，且是明确确认的手动操作，不受自动清理的 `enable` 或 `dryRun` 开关影响。

## 从源码构建

构建环境：Windows x64、[LLVM/Clang](https://llvm.org/)（提供 `clang-cl`）、[xmake](https://xmake.io/)。依赖会由 xmake 从工程配置的仓库自动获取。

```powershell
# 服务端版本（默认目标）
xmake f -y -p windows -a x64 -m release --toolchain=clang-cl --target_type=server
xmake

# 如需生成客户端目标
xmake f -y -p windows -a x64 -m release --toolchain=clang-cl --target_type=client
xmake
```

构建完成后：

- DLL 和调试符号位于 `bin/WanXiang/`；
- 可分发压缩包为 `bin/WanXiang.zip`；
- `lang/` 会在构建后自动复制到产物目录。

本项目的功能实现均使用服务端 Hook/事件；客户端目标主要用于满足 LeviLamina 的构建矩阵，实际部署建议使用 `server` 目标。

## 故障排查

- **模组无法加载**：确认 `manifest.json`、`WanXiang.dll` 位于同一目录，并检查 LeviLamina 版本和平台是否为 Windows x64。
- **配置解析失败**：检查 JSON 逗号、引号和注释，日志会给出解析错误位置。可备份后删除 `config.json`，重启生成默认文件。
- **改配置没有生效**：运行时功能修改后执行 `/wanxiang reload`；命令注册、命令权限和未加载实体清理仍需完整重启服务器。
- **定时任务不执行**：确认 `scheduledCommands.enable=true`、Cron 为 6 段格式、命令非空，并查看启动日志中的注册数量和无效表达式提示。
- **权限规则无效**：确认命令名拼写正确、权限等级属于支持列表；规则只对命令注册过程生效，需重启后验证。

## 目录结构

```text
src/Config/       配置结构、JSON 序列化和配置读写
src/I18n/         多语言加载与文本回退
src/mod/Entry.cpp 模组生命周期和各功能模块注册
src/mod/          各独立功能模块
lang/             zh_CN.json、en_US.json
xmake.lua         构建和 LeviLamina 依赖配置
tooth.json        Tooth 发布包描述
```

## 许可

本项目沿用仓库中的 `CC0-1.0` 许可。详见 [LICENSE](LICENSE)。
