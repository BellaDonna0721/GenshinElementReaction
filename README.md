# GenshinElement

仿《原神》元素反应机制的 ECS 架构游戏原型，使用 C++17 + WebAssembly 作为核心逻辑层，TypeScript + HTML5 Canvas 处理输入与渲染，严格遵循数据与逻辑分离的 ECS 设计原则。

## 技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| 游戏逻辑层（Core） | C++17 + Emscripten → WebAssembly | ECS 世界、组件存储、系统调度、元素反应计算、碰撞检测、ICD 冷却、输出指令打包 |
| 交互/渲染层 | TypeScript + HTML5 Canvas 2D | 键盘鼠标输入、玩家朝向指示、实体渲染、元素附着光环+量条、多反应堆叠飘字、左侧元素键位映射面板 |
| 构建工具 | CMake + emcmake / mingw32-make + esbuild | 一键清理+重编 WASM，TypeScript 打包为 ESM 模块 |

## 前置条件

- **Emscripten SDK (emsdk)**，推荐安装路径 `D:\emsdk`，且已激活：
  ```bash
  emsdk activate latest
  ```
- **CMake ≥ 3.10**（在 emsdk 环境 PATH 内即可）
- **Node.js**（编译 TypeScript 用，esbuild 可通过 `npx` 直接调用）
- Windows 10/11（`build.bat` / `serve.bat` 为 PowerShell/cmd 批处理脚本）

## 快速开始

### 1. 一键构建

直接双击或在 PowerShell 中执行：
```powershell
.\build.bat
```

构建脚本会自动：
1. **强制清理** 旧的 `build/` 目录，确保 CMake 配置、源文件、编译选项永不过期
2. 使用 `emcmake` 重新生成 Release 模式 MinGW Makefiles
3. 编译 C++ 源文件 → 链接为 `build/game.wasm` + `build/game.js`（通过 POST_BUILD 自动拷贝到 `dist/`）
4. 使用 `esbuild` 打包 `web/main.ts` 为 ESM 模块 → `dist/main.js`

> 构建完成后 `dist/` 目录应包含：`game.js`、`game.wasm`、`main.js`。

### 2. 启动本地服务器

```powershell
.\serve.bat
```

脚本会启动 Python 3 内置 HTTP 服务器监听 3000 端口，浏览器打开：
```
http://localhost:3000/web/index.html
```

> 如果浏览器加载出来还是旧代码，请按 **Shift + F5（或 Ctrl + F5）** 强制刷新绕过 HTTP 缓存。

## 操作说明

画面左上角有完整操作提示 + 左侧实时元素映射面板。完整键位：

| 按键 | 功能 |
|------|------|
| `W` / `A` / `S` / `D`（或方向键） | 玩家 4 向移动 |
| 鼠标移动 | 改变玩家朝向（角色圆圈旁有白色小三角指向当前鼠标方向） |
| 鼠标左键 | 沿朝向发射 **弱** 元素子弹（有冷却） |
| 鼠标右键 | 沿朝向发射 **强** 元素子弹 |
| 数字键 `1` | 切换玩家元素为 **火（Pyro）** — 红色 |
| 数字键 `2` | 切换玩家元素为 **水（Hydro）** — 深蓝色 |
| 数字键 `3` | 切换玩家元素为 **雷（Electro）** — 紫色 |
| 数字键 `4` | 切换玩家元素为 **冰（Cryo）** — 蓝白色 |
| 数字键 `5` | 切换玩家元素为 **风（Anemo）** — 浅绿色（不可附着，仅作后手触发扩散） |
| 数字键 `6` | 切换玩家元素为 **岩（Geo）** — 土黄色（不可附着，仅作后手触发结晶） |
| 数字键 `7` | 切换玩家元素为 **草（Dendro）** — 绿色（可附着） |

左侧元素映射面板会用 **2px 白框 + 8% 白色半透明底色** 高亮当前选中的元素。

## 元素反应系统

### 元素强度分级（按原神官方拟合模型）

所有元素附着/子弹 payload 均按「理论 GU → 实际初始量 ×0.8 → 非线性持续时间 + 固定线性衰减」的原神模型统一查表，禁止手写 gauge/duration：

| 强度档位 | 理论 GU | 实际初始量 | 持续时间 | 衰减速率（/秒） |
|----------|---------|------------|----------|-----------------|
| Weak（弱） | 1.0 | 0.8 | 9.5 s | 0.8 / 9.5 ≈ 0.0842 |
| Medium（中） | 1.5 | 1.2 | 10.75 s | 1.2 / 10.75 ≈ 0.1116 |
| Strong（强） | 2.0 | 1.6 | 12.0 s | 1.6 / 12.0 ≈ 0.1333 |
| ExtraStrong（超强） | 4.0 | 3.2 | 17.0 s | 3.2 / 17.0 ≈ 0.1882 |

> 预设敌人：Pyro / Hydro 为 Weak（0.8 / 9.5s），Cryo 为 Medium（1.2 / 10.75s）。

### 同元素叠加规则

当命中元素与目标已有附着元素**相同时**，执行刷新而非覆盖：

- **元素量取较大者**：若补充元素的实际量 > 目标剩余量，则更新为补充量；否则保持不变（二者选其多，弱命中不稀释光环）。
- **衰减速度不变**：更新元素量**不会改变主元素的衰减速度**（`decay_rate` 保持首次附着时的值）。
- **持续时间反算**：仅按 `duration = gauge / decay_rate` 重新计算持续时间，保证 `gauge == decay_rate × duration` 不变量成立。
- **UI 基线同步**：`initial_gauge` 在量更新时同步刷新，进度条满格对应新的量。

示例：弱水附着（gauge=0.8, decay=0.0842, dur=9.5s）→ 衰减到 0.4 时强水补充 → gauge→1.6, decay 保持 0.0842, dur→1.6/0.0842≈19s。

### 克制比规则

目标元素被 **触发元素（后手子弹）** 消耗时按以下比例：
- **克制方向 1:2**：1 单位触发实际量 = 2 单位目标实际量（水→火、火→冰）
- **被克方向 1:0.5**：1 单位触发实际量 = 0.5 单位目标实际量（火→水、冰→火）
- **对等方向 1:1**：1 单位触发实际量 = 1 单位目标实际量（其余反应）

消耗后目标 gauge 若 ≤ 0 则完全清除该附着元素，否则按比例扣减后保留。

**后手元素残留规则**：
- 大多数反应 `consumes_trigger = true`，后手元素反应后不残留。
- **扩散例外**：Anemo 的 `consumes_trigger = false`——风元素本身不被扣减，从而可以链式消耗目标的多个附着槽（水雷共存下同时触发雷扩散 + 水扩散），但 Anemo 最终仍不残留（不可附着元素）。

### 水雷共存与链式反应

Hydro 与 Electro 相遇进入**共存态**（双槽同时存在），而非一次性消耗：
- 共存期间每 **1 秒** 触发一次感电 DoT，双方各扣 **0.4** gauge。
- 若有第三元素命中（如火），按**雷优先 → 水其次**的顺序链式消耗：先与雷反应（超载），剩余火再与水反应（蒸发），同一帧可触发多个反应。
- 反应过程中触发方（火）的剩余量直接丢弃，不残留。

### 元素附着 ICD（Internal Cooldown）

为避免高频攻击无限挂元素，每个目标实体独立维护 **7 元素 × 2 档位 = 14 组** 独立冷却计数器：

- **分组 key**：`(触发元素类型 × 攻击档位)`，Weak/Medium → 档 0，Strong/ExtraStrong → 档 1。
- **周期**：2.5 秒。首次命中启动新一轮（timer=2.5s, counter=0）。
- **放行条件**：`counter % 3 == 1`（即第 1、4、7、10… 次命中放行），其余命中被拦截（既不挂元素也不触发反应）。
- 每次命中 `counter += 1`（含被拦截的命中）；周期到点后 `counter` 归零、`timer` 停表，下次命中重启。

### 已实现反应表

`ReactionType` 枚举值同时作为**位掩码 index**（0~15），支持一帧同时标记多个反应：

| 反应名（ID=位索引） | 元素组合（后手 → 先手） | 消耗比 | 备注 |
|--------------|--------------------------|--------|------|
| 蒸发 Vaporize（1） | Hydro → Pyro | 1 : 2.0 | 水克火，目标火元素被双倍消耗 |
| 蒸发 ReverseVaporize（2） | Pyro → Hydro | 1 : 0.5 | 火被水克，半效率消耗水 |
| 融化 Melt（3） | Pyro → Cryo | 1 : 2.0 | 火克冰 |
| 融化 ReverseMelt（4） | Cryo → Pyro | 1 : 0.5 | 冰被火克 |
| 超载 Overload（5） | Pyro ↔ Electro | 1 : 1.0 | 双向对等 |
| 感电 ElectroCharged（6） | Electro ↔ Hydro | 共存 | 双元素共存，每 1s 各扣 0.4 DoT |
| 冻结 Frozen（7） | Cryo ↔ Hydro | 1 : 1.0 | 双向对等 |
| 超导 Superconduct（8） | Cryo ↔ Electro | 1 : 1.0 | 双向对等 |
| 火扩散 SwirlPyro（9） | Anemo → Pyro | 1 : 1.0 | 风不消耗，可链式多扩散 |
| 水扩散 SwirlHydro（10） | Anemo → Hydro | 1 : 1.0 | 同上 |
| 雷扩散 SwirlElectro（11） | Anemo → Electro | 1 : 1.0 | 同上 |
| 冰扩散 SwirlCryo（12） | Anemo → Cryo | 1 : 1.0 | 同上 |
| 结晶 Crystallize（13） | Geo → Pyro/Hydro/Electro/Cryo | 1 : 1.0 | 单向（Geo 不可附着） |
| 燃烧 Burning（14） | Pyro ↔ Dendro | 1 : 1.0 | 双向对等 |
| 绽放 Bloom（15） | Hydro ↔ Dendro | 1 : 1.0 | 双向对等 |

> 扩散拆分为 4 个独立反应类型，使水雷共存目标被风命中时可同时触发「雷扩散 + 水扩散」双飘字。

### 多反应同时显示

同一帧可发生多个反应（如感电 DoT + 超载 + 蒸发），通过 **uint16 位掩码** 传递：

- `ElementStatus::pending_reaction_mask` 每帧累积所有发生的反应（OR 叠加）。
- `RenderCommand` 用 `reaction_mask_lo`（byte[18]）+ `reaction_mask_hi`（byte[23]）拼成完整 16 位掩码。
- TS 侧解码位掩码，每个反应生成一条独立飘字，同帧多反应**上下堆叠**并各自渐隐。

### 不可附着元素（仅后手触发）
- **Anemo（风）、Geo（岩）**：永远不会作为附着状态挂在敌人身上，命中反应后不残留。扩散/结晶只在它们作后手子弹时触发。
- 其余 **Pyro / Hydro / Electro / Cryo / Dendro** 皆为可附着元素，命中无反应的目标会按元素强度表标准附着。

## 目录结构（严格 ECS 分层）

```
GenshinElement/
├── build.bat                 # 一键：清缓存 + 配置 + 编 WASM + 编 TS
├── serve.bat                 # 一键：Python HTTP 服务器 3000 端口
├── CMakeLists.txt            # Emscripten 工具链 + POST_BUILD 拷贝到 dist
├── dist/                     # 最终浏览器产物（game.js/game.wasm/main.js）
│
├── src/                      # C++ 游戏逻辑（ECS，编译为 WASM）
│   ├── core/                 # ECS 核心
│   │   ├── Entity.h          # Entity ID (uint32_t)
│   │   ├── ComponentRegistry.h  # ComponentTypeID / ComponentMask 位掩码
│   │   ├── ComponentPool.h   # IComponentPool + ComponentPool<T>（Sparse Set 稀疏集）
│   │   └── World.h/.cpp      # 协调器：实体生命周期、组件 add/get/remove、query、系统调度
│   ├── components/           # 纯数据组件（单一职责，组件间互不 include）
│   │   ├── Transform.h Velocity.h Lifetime.h Collider.h
│   │   ├── ElementStatus.h   # 附着光环（双槽 aura + gauge + 反应位掩码）
│   │   ├── ElementICDStatus.h # 元素命中冷却（7×2 组 counter 槽）
│   │   ├── ElementPayload.h  # 子弹载荷（元素类型 + ElementStrength 档位）
│   │   ├── FacingDirection.h # 玩家朝向 dx/dy
│   │   ├── IdentityTag.h     # 统一身份标签：Player / Enemy / Bullet
│   │   └── RenderInfo.h      # 视觉半径 + RGB
│   ├── systems/              # 系统逻辑（无状态，只读/写组件）
│   │   ├── System.h          # System 抽象基类
│   │   ├── InputSystem       # 移动、朝向、发射子弹、数字键切玩家元素
│   │   ├── LifetimeSystem    # 子弹生命周期 countdown
│   │   ├── MovementSystem    # Transform += Velocity * dt
│   │   ├── CollisionSystem   # 子弹-敌人圆碰撞 + ICD 判定，写入 pending 元素
│   │   ├── ElementReactionSystem  # 衰减 + 查表反应 + 感电 DoT + 链式消耗 + 反应位掩码
│   │   └── OutputSystem      # 打包为 32B RenderCommand 数组供 TS 读取
│   ├── gameplay/
│   │   ├── SpawnPreset.h/.cpp  # spawn_player / spawn_enemy 工厂 + EnemyPreset 配置表
│   │   ├── BulletPool.h/.cpp   # 子弹对象池（POOL_SIZE=40，acquire/release 复用）
│   │   └── wasm_api.cpp        # EMSCRIPTEN_BINDINGS：TS ↔ WASM 桥接
│   └── utils/
│       ├── ElementType.h     # 共享类型定义：ElementType/ElementStrength/ReactionType 枚举 + 强度参数表
│       └── element_color.h   # ElementType → RGB 映射，角色/子弹/UI 颜色统一
│
└── web/                      # TypeScript 交互/渲染层（浏览器执行）
    ├── index.html            # Canvas 容器 + import main.js
    ├── types.ts              # RenderCommand 接口、ElementType/Strength 枚举、REACTION_NAMES
    ├── render.ts             # Renderer：实体 + 元素光环 + 量条 + 多反应堆叠飘字 + UI 面板
    └── main.ts               # 主循环 requestAnimationFrame + 键盘鼠标事件 + TS↔WASM 桥接
```

## 设计约束与工程约定

- **组件单一职责**：组件仅包含 POD 数据 + 少量自身数据的查询/写入工具方法，业务逻辑完全在 System 中。
- **组件正交**：组件之间互不 include，共享类型（如 `ElementType`）抽到 `utils/`，避免循环依赖。
- **组合优于继承**：实体类型差异（Player / Enemy / Bullet）通过 `IdentityTag` + 组件组合区分，不用类继承。
- **子弹对象池**：预分配 40 个子弹实体，`acquire`/`release` 复用，避免频繁创建/销毁。
- **RenderCommand 32 字节对齐**：C++ 与 TS 共享的帧数据内存布局严格一致，字段增减需两端同步。
- **组件存储 Sparse Set**：稀疏索引数组 + 稠密数组实现高缓存局部性，`query<Component...>` 遍历高效。
- **实体 ID 栈式回收**：`create_entity` 从 `m_free_ids` 栈里复用已销毁的 ID。
- **系统执行顺序固定**：Input → Lifetime → Movement → Collision → ElementReaction → Output，每帧严格按此顺序调度。
- **元素→颜色集中化**：所有元素色值来自 `get_element_color()`（utils/element_color.h）。
- **类型定义集中化**：所有元素枚举/强度参数来自 `utils/ElementType.h`，组件和系统共享。

## TODO

- [ ] 玩家/敌人生命值组件（Health）与反应伤害计算
- [ ] 超载范围爆炸 / 超导减物抗 / 冻结控制 等具体反应效果
- [ ] 敌人 AI 与自动生成波次
- [ ] 扩散 AoE、草原核、结晶护盾的实体实现
- [ ] 粒子特效（超载爆炸、燃烧火焰等）
- [ ] 激化（Quicken）、烈绽放、超绽放等草系进阶反应

## License

学习项目，MIT。
