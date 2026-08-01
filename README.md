# GenshinElement

仿《原神》元素反应机制的 ECS 架构游戏原型，使用 C++17 + WebAssembly 作为核心逻辑层，TypeScript + HTML5 Canvas 处理输入与渲染，严格遵循数据与逻辑分离的 ECS 设计原则。

## 技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| 游戏逻辑层（Core） | C++17 + Emscripten → WebAssembly | ECS 世界、组件存储、系统调度、元素反应计算、碰撞检测、输出指令打包 |
| 交互/渲染层 | TypeScript + HTML5 Canvas 2D | 键盘鼠标输入、玩家朝向指示、实体渲染、元素附着光环+量条、反应文字 label、左侧元素键位映射面板 |
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
3. 编译 9 个 C++ 源文件 → 链接为 `build/game.wasm` + `build/game.js`（通过 POST_BUILD 自动拷贝到 `dist/`）
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
| 鼠标左键 | 沿朝向发射元素子弹（有冷却，子弹元素=当前玩家元素） |
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

### 克制比规则

目标元素被 **触发元素（后手子弹）** 消耗时按以下比例：
- **克制方向 1:2**：1 单位触发实际量 = 2 单位目标实际量（水→火、火→冰）
- **被克方向 1:0.5**：1 单位触发实际量 = 0.5 单位目标实际量（火→水、冰→火）
- **对等方向 1:1**：1 单位触发实际量 = 1 单位目标实际量（其余反应）

消耗后目标 gauge 若 ≤ 0 则完全清除该附着元素，否则按比例扣减后保留。**所有反应的后手元素（子弹）都不残留**（`consumes_trigger = true`），符合"后手不残留"的简化规则。

### 已实现反应表（共 13 类 26 条规则，含克制方向变体）

| 反应名（ID） | 元素组合（后手 → 先手） | 消耗比 | 备注 |
|--------------|--------------------------|--------|------|
| **蒸发** Vaporize（1） | Hydro → Pyro | 1 : 2.0 | 水克火，目标火元素被双倍消耗 |
| **蒸发** ReverseVaporize（2） | Pyro → Hydro | 1 : 0.5 | 火被水克，半效率消耗水 |
| **融化** Melt（3） | Pyro → Cryo | 1 : 2.0 | 火克冰 |
| **融化** ReverseMelt（4） | Cryo → Pyro | 1 : 0.5 | 冰被火克 |
| **超载** Overload（5） | Pyro ↔ Electro | 1 : 1.0 | 双向对等 |
| **感电** ElectroCharged（6） | Electro ↔ Hydro | 1 : 1.0 | 双向对等（当前为一次性消耗，后续可拓展为双元素共存持续伤害） |
| **冻结** Frozen（7） | Cryo ↔ Hydro | 1 : 1.0 | 双向对等（后续可拓展硬直） |
| **超导** Superconduct（8） | Cryo ↔ Electro | 1 : 1.0 | 双向对等（后续可拓展减物抗 buff） |
| **扩散** Swirl（9） | Anemo → Pyro/Hydro/Electro/Cryo | 1 : 1.0 | 单向（Anemo 不可附着，无反向） |
| **结晶** Crystallize（10） | Geo → Pyro/Hydro/Electro/Cryo | 1 : 1.0 | 单向（Geo 不可附着，无反向） |
| **燃烧** Burning（11） | Pyro ↔ Dendro | 1 : 1.0 | 双向对等（Dendro 可附着） |
| **绽放** Bloom（12） | Hydro ↔ Dendro | 1 : 1.0 | 双向对等（Dendro 可附着） |
| **激化** Quicken（13） | Electro ↔ Dendro | 1 : 1.0 | 双向对等（Dendro 可附着） |

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
│   ├── core/                 # ECS 核心：实体、世界、组件池（Sparse Set 稀疏集）
│   │   ├── Entity.h          # Entity ID (uint16_t, 上限 2048)
│   │   ├── World.h/.cpp      # 实体创建/回收、组件 add/get/remove、query 匹配
│   │   └── ComponentRegistry.h
│   ├── components/           # 纯数据组件（单一职责，无成员函数业务逻辑）
│   │   ├── Transform.h Velocity.h Lifetime.h Collider.h
│   │   ├── ElementStatus.h   # 附着状态 + 强度表 + ReactionType 枚举
│   │   ├── ElementPayload.h  # 子弹载荷（元素类型 + ElementStrength 档位）
│   │   ├── FacingDirection.h # 玩家朝向 dx/dy
│   │   ├── IdentityTag.h     # 统一身份标签：Player / Enemy / Bullet / AoEZone
│   │   └── RenderInfo.h      # 视觉半径 + RGB + 引用 IdentityTag 枚举
│   ├── systems/              # 系统逻辑（无状态，只读/写组件）
│   │   ├── InputSystem       # 移动、朝向、发射子弹、数字键切玩家元素
│   │   ├── LifetimeSystem    # 子弹生命周期 countdown
│   │   ├── MovementSystem    # Transform += Velocity * dt
│   │   ├── CollisionSystem   # 子弹-敌人圆碰撞，写入 pending 元素槽
│   │   ├── ElementReactionSystem  # 衰减 + 查表反应 + 精细 gauge 扣减（含克制比）
│   │   └── OutputSystem      # 把 Transform/RenderInfo/ElementStatus 打包为 32B 对齐的 RenderCommand 数组供 TS 读取
│   ├── gameplay/
│   │   ├── SpawnPreset.h/.cpp# make_player / make_enemy 工厂函数 + EnemyPreset 配置表
│   │   └── wasm_api.cpp      # EMSCRIPTEN_BINDINGS：TS 侧 _wasm_init / _wasm_update / _wasm_set_input / _wasm_get_render_*
│   └── utils/
│       └── element_color.h   # 集中化 ElementType → RGB 映射，角色/子弹颜色全局统一
│
└── web/                      # TypeScript 交互/渲染层（浏览器执行）
    ├── index.html            # Canvas 容器 + import main.js
    ├── types.ts              # InputState、ElementType 枚举、ELEMENT_NAMES/COLORS、REACTION_NAMES
    ├── render.ts             # Renderer：实体圆绘制 + 元素光环 + 量条 + 反应文字 label + UI 面板
    └── main.ts               # 主循环 requestAnimationFrame + 键盘鼠标事件 + 桥接 TS ↔ WASM
```

## 设计约束与工程约定（来自项目 ECS 规范）

- **组件单一职责**：组件仅包含 POD 数据，无业务函数。逻辑完全在 System 中。
- **组合优于继承**：实体类型差异（Player / Enemy / Bullet）通过 `IdentityTag` + 组件组合区分，不用类继承。
- **子弹对象池**：超过上限 2048 时复用已死亡的 Bullet 实体，避免频繁创建/销毁。
- **RenderCommand 32 字节对齐**：C++ 与 TS 共享的帧数据内存布局严格一致，防止跨语言读错位。
- **组件存储 Sparse Set**：用稀疏索引数组 + 两张稠密数组实现高缓存局部性，`query<Component...>` 遍历速度极快。
- **实体 ID 栈式回收**：`create_entity` 从 `m_free_ids` 栈里复用已销毁的 ID，保证 ID 范围永远 ≤ 2048。
- **系统执行顺序固定**：Input → Lifetime → Movement → Collision → ElementReaction → Output，每帧严格按此顺序调度，避免时序依赖问题。
- **组件初始化风格**：显式声明局部变量，逐字段赋值后传入 `add_component`，**禁用大括号聚合初始化**。
- **元素→颜色集中化**：所有元素色值来自 `get_element_color()`（element_color.h），角色颜色、子弹颜色、UI 面板色块共用同一份定义。

## TODO

- [ ] 玩家/敌人生命值组件（Health）与反应伤害计算
- [ ] 超载范围爆炸 / 超导减物抗 / 感电持续掉血 / 冻结控制 等具体反应效果
- [ ] 敌人 AI 与自动生成波次
- [ ] 扩散 AoE、草原核、结晶护盾的实体实现（已预留 AoEZone 枚举位）
- [ ] 粒子特效（超载爆炸、燃烧火焰等）
- [ ] 响应式 UI 与伤害飘字

## License

学习项目，MIT。
