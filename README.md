# 万象（WanXiang）

万象是一个基于 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 的基岩版服务端原生模组，提供一组可以独立开关的服务端增强功能。配置文件为 JSON（支持 `//` 和 `/* ... */` 注释），所有功能都在模组加载时根据配置启用。

## 功能一览

| 配置项 | 功能 | 默认值 |
| --- | --- | --- |
| `trustedSkinEnable` | 将玩家皮肤标记为可信任皮肤 | `true` |
| `autoFishingEnable` | 钓鱼成功后自动收杆并重新抛竿 | `true` |
| `abilityCommandEnable` | 强制注册原版 `/ability` 命令 | `true` |
| `cleanUnknownBlocksEnable` | 区块加载时清理无法识别的方块 | `true` |
| `hideOpPermissionEnable` | 对非管理员隐藏客户端的 OP 权限信息 | `true` |
| `droppedItemLifetime` | 修改默认及指定掉落物的存在时间 | `enable=false` |
| `xpOrbMerge` | 合并范围内的经验球 | `enable=true` |
| `mobEffectBlocker` | 禁止指定状态效果触发伤害/死亡附带行为 | `enable=true` |
| `fastLeafDecay` | 原木或树叶被破坏后加速清理无支撑树叶 | `enable=true` |
| `scheduledCommands` | 按 Cron 表达式执行服务端命令 | `enable=false` |
| `commandPermissions` | 覆盖命令注册时的权限等级 | `enable=false` |
| `serverPong` | 修改服务器列表中的 MOTD 和人数 | `enable=false` |
| `clientLevelName` | 修改客户端进入世界时显示的存档名称 | `enable=false` |
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

下面是一份包含所有字段的示例。可以只保留需要修改的字段，缺失字段会使用代码中的默认值；缺失的顶层字段会在启动时自动补回原文件。

```json
{
    "version": 15,
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
        "radius": 8
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
