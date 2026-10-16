# ChiyanMap 官方功能指南与使用教程

[English](./README.md) | **简体中文**

欢迎阅读 **ChiyanMap（赤焰地图）** 的全功能使用指南与配置手册。本文档库旨在帮助玩家与整合包作者深入了解本模组的每一项功能、快捷键操作逻辑、工作机制及高级调优选项。

---

## 📚 目录导航

| 章节 | 文档 | 核心内容概述 |
| :--- | :--- | :--- |
| **01** | [小地图与高级雷达系统](./zh/01-minimap.md) | 形状切换、平滑旋转、多层实体雷达、越界吸附边框、通配符过滤、药水避让及 HUD 仪表盘 |
| **02** | [全屏大地图与图像导出](./zh/02-worldmap.md) | 自由缩放漫游、坐标精准跳转、跨维度浏览、框选高清导出 PNG、实体头像显隐及路径点抽屉 |
| **03** | [路径点与死亡记录系统](./zh/03-waypoints-and-deathpoints.md) | 路径点集合管理、自定义分类文件夹、原版 3D 贯天信标光柱、死亡点自动记录/转换与撤销恢复 |
| **04** | [多世界与地图实例管理](./zh/04-multiworld.md) | 跨服与子服地图物理隔离、出生点自动识别绑定、地图实例创建/删除、级联清理与互联漫游 |
| **05** | [洞穴地图与无规则探测](./zh/05-cave-and-anarchy.md) | 垂直深度切片透视、多层地下空间识别、无规则区块玩家人工修改痕迹探测与敏感度调节 |
| **06** | [大地图自由涂鸦与标注](./zh/06-drawing-annotations.md) | 自由画笔、线条、箭头、几何图形、文本标注、多图层管理及完整撤销 (`Ctrl+Z`) / 重做 (`Ctrl+Y`) |
| **07** | [种子地图与结构预测](./zh/07-seedmap.md) | 世界种子捕获、全结构分布预测（村庄/要塞/远古城市等）、史莱姆区块计算与生物群系全景 |
| **08** | [快捷键、设置与性能调优](./zh/08-settings-and-hotkeys.md) | 单键/组合键重绑定、实时搜索过滤、一键出厂复位、SQLite WAL 存储模式与多线程写盘调优 |

---

## ⚡ 默认快捷键总览

| 按键 | 对应功能 | 默认上下文 |
| :--- | :--- | :--- |
| **`M`** | 打开 / 关闭全屏大地图 | 游戏任意时刻 |
| **`U`** | 打开 / 关闭路径点管理器 | 游戏任意时刻 |
| **`N`** | 切换小地图显示开关 | 游戏任意时刻 |
| **`Y`** | 切换小地图形状（圆形 ⟷ 方形） | 游戏任意时刻 |
| **`J`** | 切换小地图朝向（随玩家旋转 ⟷ 固定正北） | 游戏任意时刻 |
| **`Z`** | 临时切换小地图放大模式（Enlarge Minimap） | 游戏任意时刻 |
| **`[` / `]`** | 切换至上一个 / 下一个路径点集合（弹出 Toast） | 游戏任意时刻 |
| **`Tab` (长按)** | 临时显示 / 隐藏全屏大地图生物头像 | 全屏大地图处于激活状态 |
| **`Shift` + 鼠标拖拽** | 框选指定矩形范围并导出为高清 PNG | 全屏大地图处于激活状态 |
| **`Ctrl + Z`** | 撤销上一步操作（涂鸦笔迹 / 快捷键重绑 / 误删路径点恢复） | 全屏大地图 / 路径点管理器 / 设置面板 |
| **`Ctrl + Y`** | 重做上一步被撤销的操作（涂鸦绘制专用） | 涂鸦模式处于激活状态 |
| **`Esc`** | 关闭当前大地图界面 / 取消快捷键重绑监听 | 模组 UI 处于激活状态 |

> 💡 **提示**：所有快捷键均可在游戏内通过按下 **`M`** 打开大地图，点击顶部工具栏的 **⚙️（设置）** 图标进入快捷键面板进行自定义单键或多键组合绑定（支持 `Ctrl`、`Shift`、`Alt` 任意修饰键）。

---

## 📂 运行时目录架构与文件说明 (`mods/ChiyanMap`)

模组在客户端运行时，会在游戏根目录的 `mods/ChiyanMap/` 下维护高度模块化、多世界隔离的数据文件。以下为各目录与核心文件的完整结构图与职责说明：

```
mods/ChiyanMap/
├── ChiyanMap.dll                  # 模组核心二进制动态链接库 (LeviLamina 插件入口)
├── manifest.json                  # LeviLamina 模组配置清单与版本描述
├── tooth.json                     # Tooth 包管理器元数据与依赖定义
├── config.json                    # 全局运行配置 (小地图/大地图参数、快捷键及雷达规则)
│
├── lang/                          # 内置 16 种多语言本地化字典目录
│   ├── zh_CN.json                 # 简体中文 (Simplified Chinese)
│   ├── zh_TW.json                 # 繁体中文 (Traditional Chinese)
│   ├── en_US.json                 # 英语 (English)
│   ├── ja.json                    # 日语 (Japanese)
│   ├── ko.json                    # 韩语 (Korean)
│   ├── ru.json                    # 俄语 (Russian)
│   ├── de.json                    # 德语 (German)
│   ├── fr.json                    # 法语 (French)
│   ├── es.json                    # 西班牙语 (Spanish)
│   ├── pt_BR.json                 # 葡萄牙语 (Portuguese)
│   ├── it.json                    # 意大利语 (Italian)
│   ├── id.json                    # 印尼语 (Indonesian)
│   ├── th.json                    # 泰语 (Thai)
│   ├── tr.json                    # 土耳其语 (Turkish)
│   ├── uk.json                    # 乌克兰语 (Ukrainian)
│   └── vi.json                    # 越南语 (Vietnamese)
│
├── heads/                         # 生物/怪物雷达像素头像图标资源 (80+ PNG 图标)
│   ├── CowFace.png                # 牛微缩头像
│   ├── PigFace.png                # 猪微缩头像
│   ├── CreeperFace.png            # 苦力怕微缩头像
│   ├── ZombieFace.png             # 僵尸微缩头像
│   └── ...                        # 其他各类生物、掠夺者、Boss 图标
│
├── data/                          # 核心地形与地图数据储存根目录
│   ├── <World_ID>_database.db     # SQLite WAL 模式数据库 (开启 SQLite 模式时使用)
│   ├── <World_ID>_database.db-wal # SQLite 预写日志文件 (高并发写入缓存)
│   ├── <World_ID>_database.db-shm # SQLite 共享内存索引文件
│   │
│   ├── cache/                     # 二进制地图切片缓存目录
│   │   └── <World_ID>/            # 按世界 UUID 隔离的独立数据
│   │       ├── multiworld.json    # 当前世界下的多地图实例与互联清单
│   │       ├── dim_0/             # 主世界切片目录
│   │       │   ├── region_X_Z.bin # 512x512 区域高压缩二进制地貌数据
│   │       │   └── cave/          # 主世界地下洞穴切片数据目录
│   │       │       └── region_X_Z.bin
│   │       ├── dim_1/             # 下界切片目录
│   │       ├── dim_2/             # 末地切片目录
│   │       └── maps/              # 多世界/子服自定义实例切片目录
│   │           └── <Map_ID>/      # 专属地图实例 (如 spawn_0_0)
│   │               ├── dim_0/
│   │               ├── dim_1/
│   │               └── dim_2/
│   │
│   ├── export/                    # 全屏大地图 PNG 框选导出图片目录
│   │   └── map_export_*.png       # 高清拼接全景图
│   └── maps_backup/               # 地图删除时自动生成的安全恢复归档目录
│
├── waypoints/                     # 路径点存储目录
│   ├── <World_ID>_dim0.json       # 主世界默认路径点列表
│   ├── <World_ID>_dim1.json       # 下界路径点列表
│   ├── <World_ID>_dim2.json       # 末地路径点列表
│   ├── <World_ID>_folders.json    # 自定义路径点分类文件夹配置
│   ├── <World_ID>_sets.json       # 路径点集合 (Sets) 清单
│   └── <World_ID>_<Map_ID>_*.json # 多世界自定义地图实例的独立路径点与集合
│
├── deaths/                        # 玩家死亡点记录目录
│   └── <World_ID>.json            # 历史死亡记录与转换路径点关联映射表
│
├── drawings/                      # 大地图矢量涂鸦与标注存储目录
│   ├── <World_ID>_dim0.json       # 主世界矢量标注与图层数据
│   ├── <World_ID>_dim1.json       # 下界矢量标注
│   └── <World_ID>_<Map_ID>_*.json # 自定义地图实例的独立图层标注
│
├── anarchy/                       # 无规则区块探测数据目录
│   └── <World_ID>_anarchy.json    # 历史人为修改痕迹热度点位数据
│
├── breadcrumbs/                   # 玩家历史移动足迹目录
│   └── <World_ID>_breadcrumbs.json# 步数与轨迹记录 (用于足迹回溯)
│
└── backups/                       # 数据备份与导出目录
    └── waypoints_<World>_*.json   # 手动或定时导出的路径点全量备份
```

### 各核心目录与文件详细介绍

1. **核心程序与配置文件**
   - `ChiyanMap.dll`：由 C++23 编译而成的核心二进制模块，负责 MinHook 挂钩、DirectX 11 渲染注入、ImGui 界面绘制以及地图核心逻辑。
   - `config.json`：全局持久化配置，包括小地图与大地图的位置、缩放、透明度、HUD 信息开关、快捷键映射表及雷达黑白名单。
   - `manifest.json` / `tooth.json`：LeviLamina 插件包标准元数据，定义模组标识、前置依赖及安装目标路径。

2. **多语言与图集资源 (`lang/`, `heads/`)**
   - `lang/`：模组原生国际化语言文件，覆盖中、英、日、韩、德、法、俄等 16 种主流语言。玩家修改游戏语言或模组语言选项时，系统自动热重载对应 JSON 文件。
   - `heads/`：包含 80 余种原版生物（动物、怪物、灾厄村民、Boss 等）的 16x16 / 32x32 像素头部贴图，在雷达中用于高精度头像渲染。

3. **地图与缓存数据 (`data/`)**
   - `data/cache/<World_ID>/`：地貌数据切片缓存，按世界 UUID 进行严格物理隔离。每个切片文件 `region_<rx>_<rz>.bin` 包含 512x512 方块的色彩、高度、水体与群系信息，支持自愈校验。
   - `data/cache/<World_ID>/dim_0/cave/`：地下洞穴专属垂直深度切片，记录地底走廊与空腔结构。
   - `data/<World_ID>_database.db`：开启 SQLite 数据库模式时生成的高性能 ACID 数据库，包含 WAL 预写日志与共享内存缓存，专为大型服务器高速飞行跑图设计。
   - `data/export/`：全屏大地图使用 Shift 框选后导出的高清无损 PNG 图片默认储存目录。

4. **导航与标记数据 (`waypoints/`, `deaths/`, `drawings/`)**
   - `waypoints/`：按维度分别存储的路径点 JSON 数组，记录坐标、名称、RGB 颜色、所属集合及分类文件夹。
   - `deaths/`：记录玩家历次死亡的三维坐标、时间戳、维度以及是否已转换为路径点的关联 ID，支持误删还原联动。
   - `drawings/`：全屏大地图矢量涂鸦与标注文件，记录画笔线条、折线、箭头、几何形状、文字标注及多图层不透明度。

5. **探索辅助数据 (`anarchy/`, `breadcrumbs/`, `backups/`)**
   - `anarchy/`：无规则区块历史修改痕迹热点数据，记录被玩家破坏或人造方块集中的区块坐标。
   - `breadcrumbs/`：记录玩家探索时的移动轨迹点位，形成面包屑导航足迹。
   - `backups/`：路径点手动备份或版本升级时自动归档的快照文件，确保极端情况下数据万无一失。

