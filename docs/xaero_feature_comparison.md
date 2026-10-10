# Xaero's World Map 与 ChiyanMap 功能特性对比及差异备忘

> **对比目标**：`D:\Project\xaero\xaeroworldmap-fabric-26.2-1.41.3.jar` (Minecraft 1.26.2 / Fabric)  
> **本项目**：`ChiyanMap` (Minecraft 基岩版 / LeviLamina 原生 Mod / DirectX 11)  
> **记录日期**：2026-10-07  

---

## 目录

- [一、背景与底层架构差异](#一背景与底层架构差异)
- [二、Xaero's World Map 独有功能（本项目未实现）](#二xaeros-world-map-独有功能本项目未实现)
  - [1. 多世界 / 子世界与地图实例管理（Multiworld & Map Selection）](#1-多世界--子世界与地图实例管理multiworld--map-selection)
  - [2. 离线世界存档直读与全图转换（World Save & Region Converter）](#2-离线世界存档直读与全图转换world-save--region-converter)
  - [3. 服务端通信协议与远距离追踪（Server Protocol & Tracked Players）](#3-服务端通信协议与远距离追踪server-protocol--tracked-players)
  - [4. 第三方 Mod 深度生态集成（Mod Integrations）](#4-第三方-mod-深度生态集成mod-integrations)
  - [5. 渲染细节与图形管线特性（Rendering & Visual Features）](#5-渲染细节与图形管线特性rendering--visual-features)
  - [6. 路径点交互与便利功能（Waypoint Interactions）](#6-路径点交互与便利功能waypoint-interactions)
  - [7. 地图写入限制与界面操作（Map Controls & UX）](#7-地图写入限制与界面操作map-controls--ux)
- [三、ChiyanMap 独有优势（Xaero's World Map 无此功能）](#三chiyanmap-独有优势xaeros-world-map-无此功能)
- [四、功能汇总对比表](#四功能汇总对比表)
- [五、未来移植与开发可行性参考](#五未来移植与开发可行性参考)

---

## 一、背景与底层架构差异

| 维度 | Xaero's World Map (Fabric) | ChiyanMap (赤焰地图) |
| :--- | :--- | :--- |
| **运行平台** | Minecraft Java Edition (Fabric Loader) | Minecraft Bedrock Edition (LeviLamina Native Mod) |
| **开发语言 / 图形API** | Java / OpenGL / LWJGL | C++20 / DirectX 11 / ImGui |
| **架构设计** | **仅为全屏大地图**（雷达和小地图被拆分为独立的 `xaerominimap` 模组） | **一体化集成**（小地图 + 全屏大地图 + 实体雷达 + 种子地图一体化） |
| **数据源** | 内存已加载区块 + 本地 Anvil Region 存档文件 (`.mca`) + 服务端网络包 | 客户端渲染线程 Hook + 内存区块指针查询 + 本地缓存 (`dim_<n>/region_*.bin`) |

---

## 二、Xaero's World Map 独有功能（本项目未实现）

### 1. 多世界 / 子世界与地图实例管理（Multiworld & Map Selection）
在大型群组服（BungeeCord / Velocity / Paper）或多世界插件（Multiverse）环境下，不同子服经常共用相同的维度 ID（如均为主世界 ID 0），Xaero 提供了完善的多实例隔离体系：
- **多地图实例选择（Map Selection）**：
  - 支持同一个维度下创建并管理多个独立的地图实例。
  - 支持 `Manual`（手动确认）、`Single`（单一地图）、`World Spawn`（依据世界出生点区分）、`Server`（依据服务端包区分）四种选择模式。
- **地图连接与断开（Connect / Disconnect Map）**：
  - 允许玩家在多地图界面中将属于同一个大世界但被识别为不同实例的地图“连接”（Connect）起来，支持跨实例地图连续渲染与安全传送。
- **地图删除保护与备份**：
  - 删除地图时自动备份最近一次删除的地图数据，防止误删整个区域。
- **按服务器地址区分（`DIFFERENTIATE_BY_SERVER_ADDRESS`）**：
  - 严格按服务器 Host / Port 隔离不同的地图数据与路径点。

### 2. 离线世界存档直读与全图转换（World Save & Region Converter）
- **直接读取单人世界存档（World Save）**：
  - 内置 `xaero/map/file/worldsave/WorldDataReader`，单人模式下直接解析磁盘上的 `.mca` Region 文件。**玩家无需亲自跑图，即可在地图上渲染出已被世界生成器生成过的全部地形**。
- **全地图后台重载（Reload All Regions / Reload Viewed Regions）**：
  - 支持后台多线程重新解析并刷新全部或当前视野内的已缓存 Region，修复方块变更或版本升级后的地貌过期问题。
- **世界存档转换（Convert All Regions）**：
  - 将单人 World Save 烘焙的地图转存为通用的 `Converted World Save`（保存在 `cm$converted` 目录下），可直接通过文件管理器迁移至多人服务器。

### 3. 服务端通信协议与远距离追踪（Server Protocol & Tracked Players）
Xaero's World Map 包含服务端网络同步组件（`xaero/map/server/`）：
- **远距离玩家同步追踪系统（Tracked Players）**：
  - 服务端通过自定义数据包（`ClientboundTrackedPlayerPacket`）跨越常规客户端实体视距（甚至跨越数千方块），实时向大地图下发同服务器玩家的位置。
  - 大地图支持将远距离玩家按队伍（`Team Color`）、同阵营（`Same Party`）、盟友（`Ally Party`）进行颜色标注，并支持右键直接传送到追踪玩家。
- **服务端规则与公平竞争限制（Fair-play & Server Rules Enforcement）**：
  - 服务端可以通过 `ClientboundRulesPacket` 强制限制客户端功能，例如在纯净生存或竞技服中强制关闭洞穴视图、关闭雷达等。
- **药水状态效果联动（Status Effects）**：
  - 注册了原生状态效果：
    - `effect.xaeroworldmap.no_world_map`：获得该效果时完全禁用大地图界面。
    - `effect.xaeroworldmap.no_cave_maps`：获得该效果时禁用洞穴分层透视。
- **必须携带物品限制（Fullscreen Map Item）**：
  - 可配置必须在快捷栏手持或装备指定物品（如原版空地图、指南针或特定道具）才允许打开全屏大地图（`gui.xaero_map_item`）。

### 4. 第三方 Mod 深度生态集成（Mod Integrations）
- **Open Parties and Claims (OPAC) 领地系统集成**：
  - 在大地图上渲染领地区块（Chunk Claims）覆盖层，可调节边框不透明度（`pac_claims_border_opacity`）和填充不透明度（`pac_claims_fill_opacity`）。
  - 右键领地玩家直接发起传送命令。
- **FTB Teams & Argonauts 组队系统集成**：
  - 自动获取队伍成员名字、颜色并在大地图统一高亮标注。
- **渲染优化模组与异形方块兼容**：
  - `SupportIris`：光影开启时的渲染兼容层。
  - `SupportFramedBlocks`：各类自定义斜面/异形方块材质的映射渲染支持。
  - `SupportAmecs`：修饰键与复杂快捷键拓展支持。

### 5. 渲染细节与图形管线特性（Rendering & Visual Features）
- **生物群系色彩平滑混合（Biome Blending）**：
  - 在群系边界处对草方块、树叶、水体进行类似原版的色彩渐变混合（`gui.xaero_wm_biome_blending`），避免生硬的区块/群系颜色突变切割。
- **原版地图调色板模式（Vanilla Map Colours）**：
  - 提供了原版物品地图（Filled Map）的 64 色调色板模式（内嵌 `vanilla_states.dat`），可切换 `Accurate`（精确方块原色）、`Vanilla`（原版物品地图质感）与 `Legacy`（旧版兼容）。
- **半透明多层渲染（Display Stained Glass）**：
  - 支持多层染色玻璃（Stained Glass）与半透明方块的混合透视渲染（透过滤镜看到下方的真实地形）。
- **光照与夜晚渲染模式（Lighting & Nighttime Simulation）**：
  - 完整模拟方块光与天空光的阴影光照；在导出 PNG 时支持夜晚模拟渲染（`export_option_nighttime`）。
- **显存纹理压缩与分级细节（LOD / Mipmap 金字塔）**：
  - 支持 OpenGL 硬件纹理压缩（`gui.xaero_texture_compression`）。
  - 具备多级 Branch Region（下采样 LOD 区域金字塔），在大地图大幅缩小（远景）时仅加载低分辨率的分级贴图，显存占用远低于全量贴图。
  - 支持设置最大常驻内存的 Region 上限（`gui.xaero_wm_max_loaded_regions`）。
- **忽略服务端错误高度图（Ignore Server Heightmaps）**：
  - 专门用于纠正由于服务端模组/插件修改 heightmap 导致的地表高度计算错误。

### 6. 路径点交互与便利功能（Waypoint Interactions）
- **聊天框一键分享路径点（Share Waypoint in Chat）**：
  - 大地图右键路径点可直接将带有坐标的格式化链接/文本发送到游戏公屏或队伍聊天。
- **路径点误删恢复（Restore Waypoint）**：
  - 类似回收站机制，删除后可以在右键菜单或设置中一键恢复最近删除的路径点。
- **密集路径点最小缩放阈值（Min Zoom For Local Waypoints）**：
  - 当地图缩小到一定比例时，自动隐藏密集的局部路径点，仅显示全局重要地标，防止地图缩小时图标被挤爆遮挡。
- **路径点文字阴影/背景框开关（Render Waypoint Backgrounds）**：
  - 可配置是否为路径点名称文字渲染半透明背景底板。

### 7. 地图写入限制与界面操作（Map Controls & UX）
- **地图写入距离限制（Map Writing Distance）**：
  - 可限制地图仅记录玩家周围特定区块半径（如 2~32 区块或 Unlimited），防止玩家探图过快或限制 I/O 读写量。
- **冻结/暂停更新选项（Load New Chunks / Update Chunks 开关）**：
  - 在查看大地图时可选择暂停更新或仅浏览已有区域。
- **设置面板内置正则/关键词搜索栏（Settings Search）**：
  - 设置界面顶部自带 Search 输入框，支持普通文本与正则表达式实时过滤配置项。
- **打开地图平滑过渡动画（Opening Animation）**：
  - 打开全屏地图时的平滑放大展开动画。
- **在线更新提示（Update Notifications）**：
  - 启动时异步请求网络检查更新并弹出通知。

---

## 三、ChiyanMap 独有优势（Xaero's World Map 无此功能）

1. **一体化集成（All-in-One）**：
   - Xaero's World Map **不包含**小地图和雷达（必须额外装 `xaerominimap`）；而 ChiyanMap 原生自带完整的小地图、实体雷达、朝向旋转与 HUD 信息面板。
2. **种子全知地图（SeedMap）**：
   - 内置基岩版世界生成算法与结构预测引擎（村庄、要塞、远古城市、海底神殿、林地府邸、史莱姆区块及群系反查），Xaero 完全不具备此功能。
3. **死亡点管理系统（DeathPointManager）**：
   - 记录死亡历史、记录死亡点坐标与维度、一键导航、靠近自动清除、死亡点转路径点。
4. **游戏内 3D 信标光束（Beacon Beam Hook）**：
   - 直接在 3D 游戏画面中渲染贯穿天际的彩色信标光束引导玩家前往路径点或死亡点。
5. **极速 Native C++ 性能**：
   - 基于 DirectX 11 与 ImGui 原生渲染，无 JVM GC 停顿与内存膨胀，并支持跨线程原子操作撤销（Ctrl+Z）。

---

## 四、功能汇总对比表

| 功能模块 | 功能特性 | Xaero's World Map | ChiyanMap |
| :--- | :--- | :---: | :---: |
| **基础地图** | 全屏大地图浏览与平移缩放 | ✅ | ✅ |
| | 维度切换 (主世界/下界/末地) | ✅ | ✅ |
| | 右键传送 (自动安全高度探测) | ✅ | ✅ |
| | 洞穴分层地图 (Cave Mode) | ✅ | ✅ |
| | 地形阴影与坡度 (2D/3D/Legacy) | ✅ | ✅ |
| | PNG 高清全图导出 | ✅ | ✅ |
| **小地图 & 雷达** | 内置小地图 (圆形/方形/旋转) | ❌ (需Minimap模组) | ✅ |
| | 实体雷达 (生物高低差/头部贴图) | ❌ (需Minimap模组) | ✅ |
| | 3D 世界信标光束 (Beacon Beam) | ❌ (需Minimap模组) | ✅ |
| **高级扩展** | 种子全知地图 (结构/群系/史莱姆) | ❌ | ✅ |
| | 死亡点记录与距离追踪 | ❌ | ✅ |
| **地图管理** | 多世界/子地图实例切换 (Multiworld) | ✅ | ✅ (已实现 MultiworldManager) |
| | 地图连接 (Connect Maps) | ✅ | ✅ (已实现地图互联与安全保护) |
| | 本地离线存档直读 (World Save) | ✅ | ❌ |
| | 全图后台重载 (Reload Regions) | ✅ | ❌ |
| **渲染细节** | 生物群系渐变混合 (Biome Blending) | ✅ | ❌ |
| | 原版 64 色调色板模式 (Vanilla States) | ✅ | ❌ |
| | 染色玻璃多层半透明渲染 | ✅ | ❌ |
| | LOD 纹理分级金字塔 (Branch Regions) | ✅ | ❌ |
| **交互 & 生态** | 聊天框分享路径点 (Share in Chat) | ✅ | ✅ (大地图菜单 & 管理器一键分享+复制) |
| | 路径点回收站 (Restore Waypoint) | ✅ | ✅ (多级回收站栈 & Ctrl+Z 恢复) |
| | 密集路径点缩放自适应隐藏 | ✅ | ✅ (局部路径点最小缩放阈值) |
| | 路径点名称背景底板开关 | ✅ | ✅ (Render Waypoint Backgrounds) |
| | 领地插件联动 (OPAC Claims) | ✅ | ❌ |
| | 服务端远距离玩家追踪协议 | ✅ | ❌ |
| | 药水状态效果与公平模式禁用 | ✅ | ❌ |
| | 设置界面正则搜索过滤 | ✅ | ❌ |

---

## 五、未来移植与开发可行性参考

若未来计划为 ChiyanMap 补充上述特性，可参考以下可行性评估：

1. **高价值且易实现（建议优先考虑）**：
   - **密集路径点缩放阈值（Min Zoom for Waypoints）**：在 ImGui 渲染大地图路径点时，根据当前 `bigMapZoom` 过滤局部路径点，大幅改善地图缩小时的混乱遮挡。
   - **设置面板关键词搜索（Settings Search）**：在 ImGui 设置窗口顶部加一个搜索输入框，按文本快速过滤设置选项。
   - **路径点误删恢复（Restore Waypoint）**：维护一个简单的 `m_recentlyDeletedWaypoints` 栈，提供一键撤回。
   - **生物群系色彩平滑混合（Biome Blending）**：在写入颜色缓冲时，对 3x3 邻域的草方块/叶子颜色做均值滤波，消除群系交界处的明显色块分界。

2. **中等复杂度（视需求扩展）**：
   - **多世界/子地图实例管理**：支持用户手动为当前维度指定不同的子地图名称并独立存储 Region，解决部分基岩版大型服务器跨服共用维度的问题。
   - **LOD 纹理金字塔（Branch Regions）**：生成 2x/4x 降采样的缩略贴图，当缩放级别很远时减少显存绑定开销。

3. **受基岩版限制（暂不推荐或难度极大）**：
   - **离线存档直读**：基岩版存档基于 LevelDB，在游戏客户端运行时直接跨进程读取 LevelDB 极易引发文件锁冲突或损坏风险。
   - **服务端网络协议（Tracked Players / Fairplay）**：ChiyanMap 为纯客户端渲染模组，若无配套 BDS 服务端插件支持，无法主动接收远距离玩家封包。
