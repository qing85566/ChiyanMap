# ChiyanMap

[English](./README.md) | **简体中文**

[![Release](https://img.shields.io/badge/Release-v0.5.6--mc26.51-red)](https://github.com/qing85566/ChiyanMap/releases)
[![Platform](https://img.shields.io/badge/Platform-Minecraft%20Bedrock%20(Windows)-green)](https://www.minecraft.net/)
[![Framework](https://img.shields.io/badge/Loader-LeviLamina-blue)](https://github.com/LiteLDev/LeviLamina)
[![Language](https://img.shields.io/badge/C%2B%2B-23-orange)](https://en.cppreference.com/w/cpp/23)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](./LICENSE)

**ChiyanMap** 是一款专为 **Minecraft 基岩版 (Windows)** 打造的高性能客户端渲染向地图与雷达模组。基于 **LeviLamina** 原生框架与 DirectX 11 / 11On12 渲染管线深度集成，纯客户端运行，无需服务器安装任何插件即可享受丝滑的 HUD 实时地图体验。

> ⚠️ **声明**：本模组全部代码均由 **AI（人工智能）编写生成**。使用即代表知情并自行承担相关风险。

> 📖 **全功能使用指南与教程**：欢迎查阅仓库根目录下的 [详细中文文档库 (docs/)](./docs/README_zh.md)。

![ChiyanMap 全屏大地图效果预览](./assets/worldmap.jpg)

---

## 🌟 核心特性

- **🗺️ 实时小地图 (MiniMap)**
  - 支持圆形 / 方形无缝切换、自由拖拽定位、缩放与显示半径调节。
  - 玩家朝向平滑旋转或固定正北；智能药水效果避让与 HUD 堆叠防护。
  - 内置多距离分层实体雷达（生物、玩家、怪物、已驯服宠物专属光环）。

- **🌍 全屏大地图 (World Map)**
  - 高流畅度平滑缩放、坐标即时跳转、多维度视角无缝漫游。
  - 框选高清导出地图 PNG 图片（支持自定义快捷键）。
  - 内置种子地图（Seed Map），支持结构分布、史莱姆区块与群系实时预测。

- **📍 路径点与死亡点系统 (Waypoints & Death Points)**
  - 支持路径点集合（Sets）管理、分类抽屉筛选、自定义颜色与图标。
  - 原版 3D 贯天信标光柱与屏幕距离指示器；支持快捷键切换集合并弹出平滑 Toast。
  - 死亡位置自动高精度记录，支持一键转为永久路径点，误删支持 `Ctrl+Z` 撤销恢复。

- **🌌 多世界与实例漫游 (Multiworld)**
  - 支持跨服/子服地图实例独立隔离与互联漫游，自动感知世界出生点变更。
  - 实例删除级联清理，不留磁盘孤儿文件。

- **⛏️ 洞穴地图与无规则探测 (Cave & Anarchy)**
  - 垂直深度切片洞穴透视，地下结构一目了然。
  - 无规则（Anarchy）区块历史修改痕迹探测，协助识别建筑痕迹与地形变动。

- **✏️ 涂鸦与标注系统 (Drawing & Annotations)**
  - 大地图自由涂鸦，支持画笔、折线、箭头、矩形、圆形、文本标记及图层管理。
  - 完整支持撤销 (`Ctrl+Z`) 与重做 (`Ctrl+Y`)。

- **⚡ 极致性能与稳定性**
  - 多线程分层异步磁盘写盘，内存缓存自愈管理。
  - DXGI Present 呈现层全链路异常容灾屏障，0 崩溃、0 掉帧。
  - 内置 16 种语言本地化支持与原生独立输入法 (IME) 输入支持。

---

## ⌨️ 常用快捷键（默认）

| 按键 | 功能 | 按键 | 功能 |
| :--- | :--- | :--- | :--- |
| **`M`** | 打开 / 关闭全屏大地图 | **`U`** | 打开路径点管理器 |
| **`N`** | 切换小地图显示开关 | **`Y`** | 切换小地图形状（圆形 / 方形） |
| **`J`** | 切换小地图朝向（旋转 / 锁定北） | **`Shift` + 拖拽** | 大地图框选区域导出 PNG |
| **`Tab` (长按)** | 大地图临时显隐实体头像 | **`Ctrl + Z`** | 撤销上一步操作 / 恢复已删除路径点 |

> 💡 所有快捷键均可在设置面板（大地图齿轮图标）中自由重绑定与组合绑定。

---

## 📦 安装方法

1. **下载本模组**：前往 [ChiyanMap Releases](https://github.com/qing85566/ChiyanMap/releases) 下载最新版本的模组发布包（`.zip` 或 `.dll`）。
2. **下载并安装启动器**：前往 [LeviLauncher Releases](https://github.com/LiteLDev/LeviLauncher/releases) 下载并安装 LeviLauncher 启动器。
3. **下载适配游戏版本**：打开 LeviLauncher，点击左侧导航栏的 **“下载”** 按钮，选择并安装与本模组相适配且支持 LeviLamina 的 Minecraft 基岩版版本。
4. **选择实例**：点击左侧导航栏的 **“启动”** 按钮返回主界面，在实例列表中找到并选中刚刚安装好的 Minecraft 版本。
5. **导入模组**：点击 **“管理模组”**，随后点击界面右上角的 **“导入 .zip/dll”** 按钮，选择第一步中下载好的本模组文件并完成导入。
6. **启动游戏**：返回启动界面，点击启动游戏即可畅玩。

---

## 🛠️ 源码构建

### 环境要求
- **操作系统**：Windows 10 / 11 (x64)
- **编译工具**：[Visual Studio 2022](https://visualstudio.microsoft.com/)（C++ 桌面开发环境，MSVC v143+）
- **构建工具**：[XMake](https://xmake.io/) (3.0.0+)
- **语言标准**：C++23

### 构建步骤
```powershell
# 1. 克隆仓库
git clone https://github.com/qing85566/ChiyanMap.git
cd ChiyanMap

# 2. 配置并构建（Release）
xmake f -m release
xmake

# 编译产物位于 bin/ChiyanMap/
```

---

## 📂 仓库架构与目录说明

```text
ChiyanMap/
├── .github/                 # GitHub CI/CD 工作流与 Issue/PR 模板
├── assets/                  # 宣传预览图与界面截图资源
├── docs/                    # 完整用户手册与详细使用指南（中英双语完整文档）
├── heads/                   # 原版实体与生物雷达高分辨率头像素材（PNG）
├── lang/                    # 16 国语言本地化国际化 JSON 翻译字典
├── src/                     # C++23 核心源代码
│   ├── hooks/               # LeviLamina / MinHook 渲染与游戏底层 Hook 拦截器
│   │   ├── BeaconRenderHook.h   # 3D 贯天信标与三维场景渲染拦截
│   │   ├── DX11Hook.h           # DX11/11On12 SwapChain Present 呈现与 ImGui 渲染屏障
│   │   ├── HookRegistry.h/.cpp  # 统一 Hook 注册、初始化与卸载安全管理
│   │   ├── PlayerHook.h         # 玩家 Tick、坐标旋转、跨维度与死亡拦截
│   │   └── UIRenderHook.h       # 游戏 UI 状态协同与输入冲突仲裁
│   ├── mod/                 # 模组生命周期与运行时安全入口
│   │   ├── ChiyanMap.h/.cpp     # 模组初始化、配置热加载、输入监听与全局派发
│   │   └── MemoryOperators.cpp  # 跨 DLL 内存分配/释放安全重载算子
│   ├── render/              # 贴图、着色与图集渲染层
│   │   ├── BlockColorMap.h/.cpp # 方块 ID/State 转地图着色与高程阴影算法
│   │   ├── EntityIconManager.h  # 实体头像纹理加载、缓存与雷达标记渲染
│   │   └── MapGuiAtlas.h        # 矢量与程序化 GUI 图集渲染
│   ├── state/               # 核心状态机、数据模型与业务管理器
│   │   ├── AnarchyManager.h/.cpp     # 无规则区块历史变动与建筑痕迹探测
│   │   ├── DatabaseManager.h/.cpp    # 本地数据持久化存储底层
│   │   ├── DeathPointManager.h/.cpp  # 死亡记录追踪、历史归档与 Ctrl+Z 恢复
│   │   ├── DrawingManager.h/.cpp     # 大地图矢量标绘涂鸦与图层撤销/重做
│   │   ├── LanguageManager.h/.cpp    # 16 种多语言本地化加载与实时切换
│   │   ├── MapCacheManager.h/.cpp    # 多线程分层异步磁盘写盘与 LRU 缓存自愈
│   │   ├── MapRenderState.h/.cpp     # 小地图与大地图 ImGui 渲染驱动核心
│   │   ├── MultiworldManager.h/.cpp  # 多世界/多维度实例隔离与级联文件清理
│   │   ├── SeedMapIconAtlas.h        # 种子地图群系与结构图标图集
│   │   ├── SeedMapManager.h/.cpp     # 种子地图预测、结构分布与异步生成
│   │   ├── SpecialGridManager.h/.cpp # 坐标网格、区块边界与史莱姆区块计算
│   │   └── WaypointManager.h/.cpp    # 路径点集合、分类抽屉筛选与光柱管理
│   └── worldgen/            # 基岩版世界生成算法封装
│       ├── BedrockBiomeValidator.h/.cpp # 基岩版群系有效性校验与推导
│       ├── Random.h/.cpp                # 基岩版 PRNG 伪随机数发生器
│       ├── StructureCatalog.h/.cpp      # 全结构生成特征与索引编目
│       ├── TileQuery.h/.cpp             # 瓦片空间坐标与区块计算管道
│       └── WorldGenTypes.h              # 世界生成基础类型与几何结构体
├── third_party/             # 第三方依赖库
│   └── cubiomes-bedrock/    # 高性能 C 语言基岩版群系与结构算法库
├── .clang-format            # 代码格式化规范配置
├── .clang-tidy              # 静态代码检查规则配置
├── LICENSE                  # GNU GPL-3.0 开源协议文本
├── manifest.json            # LeviLamina 模组元数据清单
├── tooth.json               # LeviLamina 包管理器配置文件
└── xmake.lua                # XMake 构建自动化脚本
```

### 核心模块职责说明

- **`src/mod/`（模组生命周期）**：模组主入口，负责在 LeviLamina 加载阶段初始化各子系统、注册命令与事件回调、监听键盘热键，并提供跨模块的安全内存算子。
- **`src/hooks/`（游戏钩子）**：基于 MinHook 拦截 DirectX 11 `Present` / `ResizeBuffers`，挂载 ImGui 渲染管线；拦截玩家 tick 与视角变换，实时同步玩家移动与死亡事件。
- **`src/render/`（渲染层）**：负责方块材质色彩提取、高程渲染阴影、实体头像贴图加载与雷达渲染，以及 GUI 图标图集绘制。
- **`src/state/`（状态与业务逻辑）**：模组的核心业务引擎。涵盖小地图与大地图绘制状态机、异步多线程瓦片磁盘缓存、路径点/死亡点持久化、跨维度多世界识别、矢量标注撤销重做、种子地图计算与 16 国语言本地化。
- **`src/worldgen/` & `third_party/`（世界生成与算法）**：基于移植优化的 C 语言 `cubiomes-bedrock` 库，在纯客户端环境下高效预测基岩版群系分布、史莱姆区块与各种自然生成结构。
- **`docs/`（文档库）**：包含 9 篇全方位功能说明书与用户教程，涵盖小地图、大地图、路径点、多世界漫游、洞穴透视、矢量标绘、种子地图与快捷键设置。
- **`heads/` & `lang/`（资源与资产）**：包含全套原版生物高分辨率雷达头像与 16 国语言 JSON 字典。

---

## 📄 开源许可证

本项目基于 [GNU General Public License v3.0 (GPL-3.0)](./LICENSE) 协议开源。
