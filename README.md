# 万象（WanXiang）

万象是一个基于 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 的基岩版服务端原生模组，提供一组可以独立开关的服务端增强功能。配置文件为 JSON（支持 `//` 和 `/* ... */` 注释），所有功能都在模组加载时根据配置启用。

## 功能一览

| 配置项 | 功能 | 默认值 |
| --- | --- | --- |
| `trustedSkinEnable` | 将玩家皮肤标记为可信任皮肤 | `true` |
| `autoFishingEnable` | 钓鱼成功后自动收杆并重新抛竿 | `true` |
| `abilityCommandEnable` | 强制注册原版 `/ability` 命令 | `true` |
| `cleanUnknownBlocksEnable` | 区块加载时清理无法识别的方块 | `true` |
| `unloadedEntitiesCleaner` | 开服时清理未加载的实体数据（仅执行一次） | `enable=true` |
| `hideOpPermissionEnable` | 对非管理员隐藏客户端的 OP 权限信息 | `true` |
| `droppedItemLifetime` | 修改默认及指定掉落物的存在时间 | `enable=false` |
| `xpOrbMerge` | 合并范围内的经验球 | `enable=true` |
| `mobEffectBlocker` | 禁止指定状态效果触发伤害/死亡附带行为 | `enable=true` |
| `fastLeafDecay` | 原木或树叶被破坏后加速清理无支撑树叶 | `enable=true` |
| `scheduledCommands` | 按 Cron 表达式执行服务端命令 | `enable=false` |
| `commandPermissions` | 覆盖命令注册时的权限等级 | `enable=false` |
| `serverPong` | 修改服务器列表中的 MOTD 和人数 | `enable=false` |
| `clientLevelName` | 修改客户端进入世界时显示的存档名称 | `enable=false` |
| `entityCleaner` | 定时或超量时清理运行中的实体，清理前全服预告 | `enable=false` |
| `markPacksAsAddon` / `forceResourcePackRequired` | 调整服务端资源包的叠加和强制下载行为 | `true` / `false` |

## 安装

### 使用已编译文件

1. 确认服务端已安装与模组匹配的 LeviLamina（当前工程按 `26.20.7` 编译）。
2. 将发布包解压到服务端的 `plugins` 目录，保持目录结构。例如：

   ```text
   <服务器目录>/plugins/WanXiang/WanXiang.dll
   <服务器目录>/plugins/WanXiang/manifest.json
   <服务器目录>/plugins/WanXiang/lang/zh_CN.json
   <服务器目录>/plugins/WanXiang/lang/en_US.json
   ```

3. 启动一次服务器。模组会自动创建 `plugins/WanXiang/config/config.json`。
4. 修改配置后重启服务器。当前版本没有运行时重载命令，重启才能让 Hook 和事件监听按新配置重新注册。

如果使用源码构建，构建产物会在 `bin/` 目录生成，通常直接使用 `bin/WanXiang.zip` 安装即可。

## 配置文件

### 完整示例

下面是一份包含所有字段的示例。项目根目录的 [`config.example.json`](config.example.json) 提供了带中文注释、可直接复制的完整版本。可以只保留需要修改的字段，缺失字段会使用代码中的默认值；缺失的顶层字段会在启动时自动补回原文件。

```json
{
    "version": 19,
    "logLevel": 3,
    "language": "zh_CN",

    "trustedSkinEnable": true,
    "autoFishingEnable": true,
    "abilityCommandEnable": true,
    "cleanUnknownBlocksEnable": true,
    "hideOpPermissionEnable": true,

    "droppedItemLifetime": {
        "enable": false,
        "defaultSeconds": 300,
        "items": {}
    },

    "xpOrbMerge": {
        "enable": true,
        "radius": 8.0,
        "intervalTicks": 20
    },
    "mobEffectBlocker": {
        "enable": true,
        "weaving": true,
        "infested": true,
        "oozing": true,
        "windCharged": false
    },
    "fastLeafDecay": {
        "enable": true,
        "minDelayTicks": 2,
        "maxDelayTicks": 8
    },
    "unloadedEntitiesCleaner": {
        "enable": true,
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
    "entityCleaner": {
        "enable": false,
        "dryRun": false,
        "schedule": {
            "enable": false,
            "cron": "0 */10 * * * *"
        },
        "threshold": {
            "enable": false,
            "maxEntities": 3000,
            "targetEntities": 2000,
            "checkIntervalTicks": 100,
            "cooldownSeconds": 60,
            "maxMspt": 0.0
        },
        "droppedItem": {
            "protectAboveAge": 5900,
            "whitelist": [
                "minecraft:diamond",
                "minecraft:netherite_ingot",
                "minecraft:nether_star"
            ]
        },
        "filter": {
            "mode": "whitelist",
            "typeNames": ["minecraft:item", "minecraft:xp_orb", "minecraft:arrow"],
            "families": ["monster"]
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
            "secondsBefore": [10, 5, 3]
        },
        "logResult": true
    },
    "markPacksAsAddon": true,
    "forceResourcePackRequired": false
}
```

`logLevel` 是 LeviLamina 的整数枚举：`-1=Off`、`0=Fatal`、`1=Error`、`2=Warn`、`3=Info`、`4=Debug`、`5=Trace`。推荐使用默认值 `3`。

`language` 目前随包提供 `zh_CN` 和 `en_US`。找不到指定语言时会回退到中文；也可以在 `lang/` 目录放入同格式的 `<语言代码>.json` 扩展翻译。

### 单项开关功能

这些功能只有一个布尔开关，改为 `false` 即可关闭：

```json
{
    "trustedSkinEnable": false,
    "autoFishingEnable": false,
    "abilityCommandEnable": false,
    "cleanUnknownBlocksEnable": false,
    "hideOpPermissionEnable": false
}
```

- **可信任皮肤（`trustedSkinEnable`）**：将服务端判定的玩家皮肤标记为可信任皮肤。只影响皮肤信任标记，不会修改皮肤文件。
- **自动钓鱼（`autoFishingEnable`）**：鱼钩成功钓到物品后自动执行收杆/重新抛竿动作。玩家仍需正常抛出鱼竿，建议先在测试世界验证与其他钓鱼插件的兼容性。
- **Ability 命令（`abilityCommandEnable`）**：在服务端注册原版 `/ability` 命令，即使当前不是教育版模式也会尝试注册。启用后可按原版语法使用，例如 `/ability Steve mayfly true`。
- **未知方块清理（`cleanUnknownBlocksEnable`）**：区块加载时，将被游戏标记为“无法识别状态”的方块替换为空气，并在日志中记录清理数量。该操作不可恢复，建议先备份世界。
- **未加载实体清理（`unloadedEntitiesCleaner`）**：服务端启动完成后扫描世界数据库中的实体记录并清理，默认删除全部未加载实体，每次进程启动只执行一次。将 `entityTypeNamesToClean` 设置为非空数组可限制清理的实体类型（例如 `minecraft:item`）；该操作不可恢复，建议先备份世界。
- **隐藏 OP 权限（`hideOpPermissionEnable`）**：向非 OP 客户端发送能力数据包时，将玩家权限伪装为普通成员；OP 玩家收到原始权限数据。它只隐藏客户端显示，不会真正改变服务端权限。

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
| `enable` | `true` | 总开关 |
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

### 实体清理：`entityCleaner`

在服务器运行期间控制实体总量。与只在开服时扫描存档的 `unloadedEntitiesCleaner` 不同，本功能清理的是**已加载在世界里**的实体。默认 `enable=false`。

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

### 资源包叠加与强制下载

这两个字段位于配置顶层，不属于对象：

```json
{
    "markPacksAsAddon": true,
    "forceResourcePackRequired": false
}
```

- `markPacksAsAddon=true`：将服务端资源包标记为 Add-on，使客户端资源包与服务端资源包可以叠加。
- `forceResourcePackRequired=true`：要求客户端下载并使用服务端资源包。
- 两者都为 `false` 时，资源包数据包不会被修改。

如果服务器只想允许资源包共存而不强制下载，保持上面的默认组合即可；如果资源包必须生效，再将 `forceResourcePackRequired` 改为 `true`。修改后需要重启，客户端也可能需要重新连接才能看到效果。

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
- **改配置没有生效**：所有开关在加载/启用阶段读取，修改后必须完整重启服务器。
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
