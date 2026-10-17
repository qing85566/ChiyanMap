# 配置文件注释与参数全解指南 (config.json)

[English](../en/09-config-json.md) | **简体中文** | [返回文档首页](../README_zh.md)

`mods/ChiyanMap/config.json` 是本模组的全局持久化配置文件。游戏启动、在界面中调节任何设置，或使用快捷键变更选项时，系统均会自动同步读写此文件。

本文档提供 `config.json` 的**完整参数结构图、带注释的 JSONC 样例、各字段数据类型、取值区间与功能详述**，以便玩家和整合包作者进行批量预设、故障排查与性能微调。

---

## 1. 完整带注释 JSON 模板 (JSONC)

> 💡 **提示**：真正的 `config.json` 文件遵循标准 JSON 规范（不支持 `//` 注释）。在手动编辑时请勿包含注释字符，或参照下文表格核对字段。

```jsonc
{
    // ==========================================
    // 1. 语言与全局视觉设置 (Language & Global UI)
    // ==========================================
    "language": "zh_CN",                // 当前界面语言代码 (zh_CN, zh_TW, en_US, ja, ko, ru, de, fr, es, pt_BR, it, id, th, tr, uk, vi)
    "globalUIScale": 1.0,               // 全局 UI 缩放比率 (浮点数, 0.5 ~ 2.5)
    "uiTextScale": 1.0,                 // 全局字体缩放基准
    "openingAnimation": true,           // 大地图呼出时是否播放平滑淡入展开动画

    // ==========================================
    // 2. 小地图外观与布局 (MiniMap Display & Layout)
    // ==========================================
    "showMiniMap": true,                // 小地图总开关 (快捷键 N)
    "isSquareMap": false,               // 小地图形状 (false: 圆形, true: 方形, 快捷键 Y)
    "rotateMiniMap": true,              // 小地图旋转模式 (true: 随玩家旋转, false: 锁定正北, 快捷键 J)
    "miniMapScale": 1.0,                // 小地图缩放比率 (0.5 ~ 2.5)
    "miniMapZoomRadius": 64,            // 小地图可视扫描方块半径 (16 ~ 128 格)
    "minimapDoubleMinZoom": false,      // 小地图最小缩放等级翻倍开关
    "transparentMinimapBackground": false, // 小地图未探索迷雾区域是否使用完全透明背景
    "miniMapOffsetX": 16,               // 小地图相对于屏幕锚点的水平像素偏移
    "miniMapOffsetY": 16,               // 小地图相对于屏幕锚点的垂直像素偏移
    "miniMapFrameStyle": 0,             // 边框样式索引 (0: 原版质感金属框, 1: 极简纯色边框, 2: 无边框)
    "miniMapFrameColor": [0.0, 0.0, 0.0, 0.8], // 自定义边框 RGBA 颜色数组 (各通道 0.0 ~ 1.0)
    "minimapFpsLimit": 0,               // 小地图独立渲染帧率限制 (0 为不限制跟随主循环, 可设为 30, 60 降耗)
    "enlargeMinimapToggle": false,      // 快捷键 X 临时放大模式是否改为“按下切换”而非“长按维持”
    "keepEnlargedMinimapRotation": false, // 放大模式下是否强制保持地图旋转属性

    // ==========================================
    // 3. 罗盘与玩家箭头 (Compass & Player Indicator)
    // ==========================================
    "showCompass": true,                // 是否在小地图边框周围渲染罗盘方位字母 (N, E, S, W)
    "compassScale": 1.0,                // 罗盘刻度文字与指针尺寸缩放
    "compassLocation": 0,               // 罗盘停靠位置 (0: 顶部内嵌, 1: 外部环绕)
    "northCompassColorMode": 0,         // 正北指北针颜色模式 (0: 经典鲜红色, 1: 自定义 RGBA)
    "northCompassCustomColor": [1.0, 0.0, 0.0, 1.0], // 指北针自定义 RGBA 颜色
    "playerArrowColor": 0,              // 玩家中心自身箭头颜色模式 (0: 纯白色, 1: 随队伍, 2: 鲜红)
    "playerArrowScale": 1.0,            // 玩家自身箭头尺寸大小

    // ==========================================
    // 4. 仪表盘信息项 (HUD Telemetry)
    // ==========================================
    "infoShowCoords": true,             // 显示物理 X, Y, Z 三维坐标
    "infoShowNetherCoords": true,       // 位于主世界/下界时同步换算另一维度的对应门坐标 (1:8)
    "infoShowFacing": true,             // 显示文字朝向方位 (North, South, East, West 等)
    "infoShowAngles": false,            // 显示高精度偏航角 (Yaw) 与俯仰角 (Pitch) 度数
    "infoShowBiome": true,              // 显示脚下生物群系本地化名称
    "infoShowWeather": true,            // 显示晴天/降雨/雷暴等气象动态图标
    "infoShowTime": true,               // 显示游戏内时间 (如 12:30)
    "timeFormat24h": true,              // 时间格式 (true: 24小时制, false: 12小时 AM/PM)
    "infoShowLight": true,              // 显示天顶光与方块光照强度 (Sky / Block)
    "infoShowChunkCoords": false,       // 显示当前处于区块内的相对坐标 (0~15) 与区块 ID
    "infoShowBackground": true,         // 信息文本背后是否渲染半透明深色保护底板
    "pushboxBossHealth": true,          // 出现凋灵/末影龙血条时小地图是否智能下移避让
    "pushboxPotionEffects": true,       // 右上角激活药水效果图标时小地图是否智能避让

    // ==========================================
    // 5. 实体雷达系统 (Entity Radar)
    // ==========================================
    "showRadar": true,                  // 实体雷达总开关
    "radarShowPlayers": true,           // 雷达是否探测其他玩家
    "radarShowHostile": true,           // 雷达是否探测敌对型怪物 (僵尸/骷髅/苦力怕等)
    "radarShowFriendly": true,          // 雷达是否探测被动/友好生物 (牛/羊/村民等)
    "radarShowItems": false,            // 雷达是否探测地表掉落物实体
    "radarOverFrameMode": 1,            // 实体超出视野边缘吸附模式 (0: 禁用, 1: 箭头吸附, 2: 圆点滑行等)
    "radarHighlightTamed": true,        // 已驯服生物额外渲染双层荧光绿光环与爱心标记
    "radarDepthFadeStart": 8.0,         // 实体高度差景深渐隐起始距离 (格数)
    "radarHeightIndicators": true,      // 实体偏高或偏低时显示上下指示小三角 (▲ / ▼)
    "entityDepth": true,                // 开启垂直距离感应
    "entityHeightLimit": 30.0,          // 实体超出该垂直落差后完全隐去 (格数)
    "alwaysShowNametags": false,        // 忽略距离始终在雷达上绘制生物头顶名字
    "radarFilterInclude": "",           // 通配符白名单 (留空表示不限制, 例如: "*skeleton*,*zombie*")
    "radarFilterExclude": "",           // 通配符黑名单 (过滤不重要生物, 例如: "bat,squid")

    // ==========================================
    // 6. 全屏大地图与交互 (World Map)
    // ==========================================
    "bigMapShowEntities": true,         // 大地图是否绘制生物与玩家头像
    "bigMapShowMarkers": true,          // 大地图是否绘制路径点图标
    "showChunkGrid": false,             // 大地图是否绘制 F3+G 样式的 16x16 区块边界网格
    "bigMapShowHoverBox": true,         // 鼠标悬停于地貌或图标上时是否弹出信息气泡
    "bigMapWaypointDrawerOpen": false,  // 大地图左侧路径点折叠抽屉默认展开状态
    "bigMapCloseDrawerAfterHop": true,  // 在抽屉中点击路径点居中对齐后是否自动收起抽屉
    "bigMapFollowPlayer": false,        // 大地图视角是否始终以玩家物理坐标为中心自动平移
    "bigMapWaypointScale": 1.0,         // 大地图路径点图标缩放大小
    "minZoomForLocalWaypoints": 0.5,    // 仅当缩放高于此倍数时才渲染密集的近程路径点
    "renderWaypointBackgrounds": true,  // 大地图路径点文字铭牌是否附带背景底色
    "declutterWaypointLabels": true,    // 路径点密集重叠时是否智能避让抽稀文字
    "autoHideLabelsOnFarZoom": true,    // 视角拉得极远时自动隐藏小路径点文字以保整洁
    "bigMapEntityScale": 1.0,           // 大地图实体头像尺寸缩放
    "bigMapShowDisabledWaypoints": false, // 是否在大地图中灰度显示已被关闭停用的路径点
    "showWaypointDistance": true,       // 路径点下方是否标明物理直线距离 (米)
    "showSlimeChunks": false,           // 是否高亮覆盖基岩版史莱姆区块网格 (半透明翠绿)
    "bigMapCrossDimCoords": true,       // 大地图是否在右下角联动折算主世界与下界门坐标
    "persistBigMapDimension": false,    // 是否在关闭大地图后记住最后浏览的维度
    "lastViewedDimensionId": 0,         // 上次浏览的维度 ID (0: 主世界, 1: 下界, 2: 末地)
    "showZoomButtons": true,            // 大地图角落是否显示浮动放大/缩小按钮

    // ==========================================
    // 7. 路径点与 3D 光柱 (Waypoints & 3D Beams)
    // ==========================================
    "showWaypointsOnMinimap": true,     // 小地图是否渲染路径点
    "preferOverworldWaypoints": false,  // 在下界漫游时优先显示对应位置的主世界路径点
    "showDrawingsOnMinimap": true,      // 小地图是否同步显示大地图的矢量标绘笔迹
    "onlyShowCurrentWaypointSet": false, // 是否仅显示当前选中的路径点集合 (屏蔽其它集合)
    "currentWaypointSet": "default",    // 当前激活的路径点集合名称 (默认: default)
    "autoRemoveDeathpoints": false,     // 玩家靠近死亡点遗址 6 格内捡包后是否自动销毁记录
    "show3DWaypoints": true,            // 是否在主游戏三维世界中渲染悬浮路径点标牌
    "show3DWaypointBeams": true,        // 是否在主游戏三维世界中投射冲天光柱
    "useNativeBeaconBeams": true,       // 是否挂钩原版方块实体着色器管线渲染真实自转能量信标光柱
    "show3DWaypointLabels": true,       // 3D 标牌是否显示路径点名称文字
    "show3DWaypointDistance": true,     // 3D 标牌是否显示距离米数 (如 [120m])
    "waypoint3DClampToScreen": true,    // 视角背对或偏离航点时是否将指示器贴附在屏幕边缘
    "waypoint3DMaxDistance": 2048.0,    // 3D 悬浮指示器最远渲染距离 (格数)
    "waypoint3DBeamOpacity": 1.0,       // 3D 信标光柱不透明度 (0.1 ~ 1.0)
    "waypoint3DBeamRadius": 1.0,        // 3D 信标光柱粗细半径
    "waypoint3DScale": 1.0,             // 3D 悬浮文字标牌整体尺寸大小
    "show3DDeathpoints": true,          // 是否在 3D 世界中为最近一次死亡点单独投射红色光柱

    // ==========================================
    // 8. 洞穴模式参数 (Cave Mode)
    // ==========================================
    "caveModeType": 0,                  // 当前维度洞穴模式 (0: 关闭, 1: 分层扫描, 2: 全深度透视)
    "defaultCaveModeType": 1,           // 进入未独立配置维度时采用的默认洞穴模式
    "dimCaveModeType0": 0,              // 主世界独立洞穴模式
    "dimCaveModeType1": 1,              // 下界独立洞穴模式
    "dimCaveModeType2": 0,              // 末地独立洞穴模式
    "caveTopYAuto": true,               // 洞穴顶层 Y 轴是否随玩家站立高度自适应动态计算
    "caveTopY": 64,                     // 洞穴模式手动固定的顶层绝对 Y 坐标
    "caveDepth": 32,                    // 洞穴模式向下扫描渲染的垂直切片深度 (格数)
    "legibleCaveMaps": true,            // 洞穴模式地下未知方块高对比度易读性增强
    "autoCaveMode": 1,                  // 自动进入洞穴模式所需头顶实心岩石网格尺寸 (0 为禁用)
    "caveModeToggleTimer": 1000,        // 自动出入洞穴防频繁闪烁切换的冷却等待时长 (毫秒)
    "caveAllowedOverworld": true,       // 主世界是否允许启用洞穴透视
    "caveAllowedNether": true,          // 下界是否允许启用洞穴透视
    "caveAllowedEnd": true,             // 末地是否允许启用洞穴透视

    // ==========================================
    // 9. 图像导出设置 (PNG Export)
    // ==========================================
    "exportForceFullMap": false,        // 强制导出已探索的完整世界所有区块而非仅视口
    "exportMultipleImages": false,      // 超大选区时是否自动按 Region 切片分割导出多张图
    "exportOpenFolder": true,           // PNG 导出完成后自动在 Windows 资源管理器中打开目标目录
    "exportScaleDownSquare": 1,         // 导出像素降采样抗锯齿倍率 (1 为原始 1:1 无损)
    "exportNighttime": false,           // 是否以黑夜光照效果渲染导出图片

    // ==========================================
    // 10. 地形着色与切片烘焙 (Terrain & Shading)
    // ==========================================
    "terrainSlopes": true,              // 开启地形倾斜度高程阴影 (产生山脉立体浮雕效果)
    "terrainDepth": true,               // 开启水体与透明流体深度分层渐变着色
    "adjustHeightForShortBlocks": true, // 半砖、台阶、地毯等非全包方块是否修正其阴影高度
    "blockColorMode": 0,                // 方块色彩提取模式 (0: 基于材质均值色彩, 1: 纯色图表)
    "biomeColorsInVanilla": true,       // 草方块与树叶是否应用原版群系温度与湿度色彩渐变
    "mapLighting": true,                // 地图是否烘焙方块光照与天空自然阴影
    "displayStainedGlass": true,        // 彩色染色玻璃是否以对应半透明 RGBA 色彩渲染
    "loadFlowers": true,                // 小花、草丛、枯灌木等植物是否呈现在地图上
    "biomeBlending": true,              // 群系边界处是否执行 3x3 柔和色彩混合平滑过渡
    "loadNewChunks": true,              // 是否持续扫描并保存新加载的未知区块
    "updateChunks": true,               // 已探索过的区块在方块被玩家修改后是否实时更新
    "mapWritingDistance": 5,            // 地图后台磁盘写盘检测距离半径 (区块数)
    "ignoreHeightmaps": false,          // 是否忽略原版高度图强制全层精确扫描
    "multiLevelPyramid": true,          // 是否在内存中维护多级金字塔 MIP 纹理以加速远距缩放
    "maxLoadedRegions": 64,             // 内存中同时常驻的最大 Region 区域数量上限 (LRU 淘汰)

    // ==========================================
    // 11. 辅助网格与边界线 (Special Grids & Borders)
    // ==========================================
    "showRegionGrid": false,            // 是否在大地图上显示 512x512 方块的 Region 区域红线
    "showRegionGridLabel": true,        // 是否在区域网格中心标注 r.X.Z 文件编号
    "regionGridColor": [1.0, 0.0, 0.0, 0.8], // 区域网格线 RGBA 颜色
    "regionGridThickness": 1.5,         // 区域网格线粗细
    "showMapArtGrid": false,            // 是否显示 128x128 方块的地图画 (Map Art) 边框
    "showMapArtGridLabel": true,        // 是否标注地图画编号
    "mapArtGridColor": [1.0, 1.0, 0.0, 0.8], // 地图画网格 RGBA 颜色
    "mapArtGridThickness": 1.0,         // 地图画网格线粗细
    "showSpawnChunks": false,           // 是否高亮世界常驻出生点区块 (Spawn Chunks)
    "spawnChunksAutoDetect": true,      // 自动监听底层世界出生点坐标变更
    "customSpawnX": 0,                  // 手动指定的出生点 X 坐标
    "customSpawnZ": 0,                  // 手动指定的出生点 Z 坐标
    "spawnEntityRadiusChunks": 9,       // 核心实体活动出生区块半径 (默认 9 区块)
    "spawnLazyRadiusChunks": 12,        // 懒惰加载区块半径 (默认 12 区块)
    "spawnBorderRadiusChunks": 16,      // 边缘卸载区块半径
    "showSpawnChunksOnMinimap": false,  // 小地图上是否同步呈现出生点区块边界
    "showWorldBorder": false,           // 是否渲染游戏世界边界警示红线
    "worldBorderCenterX": 0,            // 世界边界中心点 X
    "worldBorderCenterZ": 0,            // 世界边界中心点 Z
    "worldBorderSize": 60000000,        // 世界边界正方形边长大小 (格数)
    "worldBorderColor": [0.0, 0.8, 1.0, 0.8], // 世界边界线 RGBA 颜色
    "showWorldBorderWarning": true,     // 接近世界边界时是否在 HUD 弹出警告
    "persistentBreadcrumbsEnabled": false, // 是否开启玩家移动足迹 (面包屑) 持续记录
    "maxBreadcrumbPoints": 1000,        // 内存中保留的最大足迹点数量上限
    "breadcrumbMinDistance": 8.0,       // 记录新足迹点的最小移动步长距离 (格数)
    "breadcrumbColor": [1.0, 0.5, 0.0, 0.8], // 足迹轨迹线 RGBA 颜色
    "showFootsteps": false,             // 小地图上是否以微小脚印符号渲染移动路径

    // ==========================================
    // 12. 无规则区块探测与高速公路 (Anarchy & Highways)
    // ==========================================
    "anarchyLiquidNewChunks": false,     // 检测非自然流动液体以识别新加载区块
    "anarchyLiquidNewChunksInverse": false, // 反向滤除
    "anarchyLiquidNewChunksOnlyAboveY0": true, // 仅在 Y > 0 层以上检测新区块
    "anarchyLiquidNewChunksColor": [0.0, 1.0, 1.0, 0.5], // 新区块高亮 RGBA 颜色
    "anarchyLiquidNewChunksInverseColor": [1.0, 0.0, 0.0, 0.5],
    "anarchyPaletteNewChunks": false,    // 基于区块调色板格式版本判定新区块
    "anarchyPaletteNewChunksInverse": false,
    "anarchyPaletteNewChunksColor": [0.2, 0.8, 0.2, 0.5],
    "anarchyPaletteNewChunksInverseColor": [0.8, 0.2, 0.2, 0.5],
    "anarchyOldChunks": false,          // 识别并高亮 1.18 地形更新前的历史旧区块
    "anarchyOldChunksInverse": false,
    "anarchyOldChunksMode": 0,          // 旧区块判定算法 (0: 默认, 1: 严格)
    "anarchyOldChunksBedrockBlending": true, // 识别新旧交界处的基岩混合缝隙
    "anarchyOldChunksColor": [1.0, 0.8, 0.0, 0.5],
    "anarchyOldChunksInverseColor": [0.5, 0.5, 0.5, 0.5],
    "anarchyOldChunksBlendedColor": [1.0, 0.2, 0.8, 0.5],
    "anarchyOldBiomes": false,          // 识别历史旧版本生物群系
    "anarchyOldBiomesColor": [0.6, 0.3, 0.8, 0.5],
    "anarchyOldBiomesSeed": 0,          // 用于比对历史群系的基准种子
    "anarchyPortals": false,            // 探测并标注已激活的下界传送门位置
    "anarchyPortalsColor": [0.8, 0.1, 1.0, 0.8],
    "anarchyPortalSkip": false,         // 忽略特定密集范围内的传送门
    "anarchyPortalSkipRadius": 16,
    "anarchyPortalSkipColor": [0.4, 0.4, 0.4, 0.5],
    "anarchyHighways": false,           // 开启无规则服务器主干公路 (Highway) 自动投影
    "anarchyHighwaysMode": 0,           // 公路投影模式 (0: 正交与对角轴, 1: 完整环线)
    "anarchyHighwaysMainAxes": true,    // 绘制 X/Z 正交主轴公路 (X=0, Z=0)
    "anarchyHighwaysDiagAxes": true,    // 绘制 45 度对角主轴公路 (X=Z, X=-Z)
    "anarchyHighwaysRingRoads": false,  // 绘制方形环线公路 (如 1k, 5k, 10k, 25k, 50k)
    "anarchyHighwaysDiamondRoads": false, // 绘制菱形对角环线
    "anarchyHighwaysGridRoads": false,  // 绘制网格辅助支线
    "anarchyHighwaysRingInterval": 5000,// 环线默认间隔距离 (格数)
    "anarchyHighwaysDiamondInterval": 5000,
    "anarchyHighwaysGridSpacing": 1000,
    "anarchyHighwaysMaxCoord": 30000000,// 公路最大延伸渲染终点
    "anarchyHighwaysCustomRingsStr": "1000,5000,10000,25000,50000,100000", // 自定义环线半径列表
    "anarchyHighwaysDimOverworld": true,// 主世界是否显示公路
    "anarchyHighwaysDimNether": true,   // 下界是否显示公路
    "anarchyHighwaysDimEnd": false,     // 末地是否显示公路
    "anarchyHighwaysWidth": 3,          // 公路标线宽度 (格数)
    "anarchyHighwaysColor": [0.2, 0.5, 1.0, 0.8], // 主干公路 RGBA 颜色
    "anarchyHighwaysSecondaryColor": [0.5, 0.8, 1.0, 0.5], // 支线公路颜色
    "anarchyHighwaysShowLabels": true,  // 公路上是否渲染坐标里程碑文字 (如 "+X 10k")
    "anarchyLavaColumns": false,        // 探测高耸岩浆柱 (Lava Cast / 废墟刷怪塔)
    "anarchyLavaColumnsMinHeight": 20,  // 岩浆柱最小高度阈值
    "anarchyLavaColumnsAlphaShift": 0.2,
    "anarchyLavaColumnsAlphaStep": 0.1,
    "anarchyLavaColumnsColor": [1.0, 0.3, 0.0, 0.8],
    "transparentObsidianRoof": false,   // 下界顶层基岩之上的黑曜石屋顶透明化
    "transparentObsidianRoofY": 128,    // 开始透明化的起始 Y 坐标
    "roofObsidianDarkening": 0.5,       // 黑曜石屋顶压暗系数
    "roofSnowOpacity": 0.5,             // 屋顶雪层不透明度
    "anarchySaveLoadToDisk": true,      // 无规则探测出的历史修改点是否落盘持久化

    // ==========================================
    // 13. 种子地图内置引擎 (Seed Map)
    // ==========================================
    "seedMap": {
        "enabled": false,               // 种子地图总开关
        "searchMode": 0,                // 演算范围模式 (0: 跟随玩家视口, 1: 固定中心坐标)
        "dimension": 0,                 // 目标演算维度 (0: 主世界, 1: 下界, 2: 末地)
        "manualCenterX": 0,             // 手动模式中心 X 坐标
        "manualCenterZ": 0,             // 手动模式中心 Z 坐标
        "manualRadius": 2048,           // 手动演算半径 (格数)
        "enabledLayers": [              // 各结构图标图层启用状态布尔数组
            true,                       // 0: 村庄 (Villages)
            true,                       // 1: 掠夺者前哨站 (Pillager Outposts)
            true,                       // 2: 远古城市 (Ancient Cities)
            true,                       // 3: 末地要塞 (Strongholds)
            true,                       // 4: 林地府邸 (Woodland Mansions)
            true,                       // 5: 海底神殿 (Ocean Monuments)
            true,                       // 6: 试炼密室 (Trial Chambers)
            true,                       // 7: 沙漠/丛林神殿 (Temples)
            true,                       // 8: 下界要塞 (Nether Fortresses)
            true,                       // 9: 堡垒残骸 (Bastions)
            true                        // 10: 末地城 (End Cities)
        ]
    },
    "manualSeeds": {                    // 针对各个世界的独立手动输入种子缓存映射
        // "<World_UUID>": "123456789012345"
    },

    // ==========================================
    // 14. 数据库引擎与高性能写盘 (Database & I/O)
    // ==========================================
    "useSqliteDatabase": true,          // 是否启用 SQLite WAL 数据库作为主要缓存格式 (推荐 true)
    "fastWritesEnabled": true,          // 启用内存批量合并队列写入
    "writeBatchIntervalMs": 500,        // 批量刷盘间隔周期 (毫秒, 100 ~ 2000)
    "teleportFailNotifier": true,       // 跨服/未互联跨图传送被拒绝或超时时弹出告警

    // ==========================================
    // 15. 快捷键映射表 (Hotkeys)
    // 每个按键包含虚拟键码 key、修饰键掩码 modifiers 以及布尔标识
    // ==========================================
    "hotkeys": {
        "openBigMap": { "key": 77, "modifiers": 0 },                 // M 键: 打开/关闭大地图
        "openWaypointMgr": { "key": 85, "modifiers": 0 },            // U 键: 打开路径点管理器
        "openDeathPointMgr": { "key": 0, "modifiers": 0 },           // 0 (未绑定): 打开死亡点列表
        "toggleMinimap": { "key": 78, "modifiers": 0 },              // N 键: 显隐小地图
        "toggleMinimapShape": { "key": 89, "modifiers": 0 },         // Y 键: 切换圆形/方形
        "toggleMinimapRot": { "key": 74, "modifiers": 0 },           // J 键: 切换随玩家旋转/锁正北
        "holdEntities": { "key": 9, "modifiers": 0 },                // Tab 键: 大地图临时显示实体头像
        "toggleSeedMap": { "key": 0, "modifiers": 0 },               // 种子地图开关
        "enlargeMinimap": { "key": 88, "modifiers": 0 },             // X 键: 临时放大模式
        "centerCamera": { "key": 32, "modifiers": 0 },               // 空格键: 大地图镜头居中玩家
        "selectExportArea": { "key": 16, "modifiers": 2, "shift": true }, // Shift 键: 框选导出 PNG
        "toggleSlimeChunks": { "key": 0, "modifiers": 0 },           // 切换史莱姆区块
        "switchWaypointSet": { "key": 221, "modifiers": 0 },         // ] 键: 切换下一个路径点集合
        "toggleHighways": { "key": 0, "modifiers": 0 },              // 公路投影开关
        "toggleAnarchySettings": { "key": 0, "modifiers": 0 },       // 无规则设置面板
        "toggleDrawingMode": { "key": 0, "modifiers": 0 },           // 大地图涂鸦模式开关
        "toggleDoubleMinZoom": { "key": 0, "modifiers": 0 },         // 最小缩放翻倍
        "toggleTransBg": { "key": 0, "modifiers": 0 },               // 透明背景切换
        "minimapZoomIn": { "key": 187, "modifiers": 0 },             // = 键: 小地图放大
        "minimapZoomOut": { "key": 189, "modifiers": 0 },            // - 键: 小地图缩小
        "togglePreferOwWaypoints": { "key": 0, "modifiers": 0 },     // 优先主世界路径点切换
        "quickCreateWaypoint": { "key": 66, "modifiers": 0 },        // B 键: 在原地快速创建路径点
        "toggleViewDistance": { "key": 0, "modifiers": 0 },          // 视野距离开关
        "toggleSimulationDistance": { "key": 0, "modifiers": 0 },    // 模拟距离开关
        "toggleBigMapUI": { "key": 70, "modifiers": 0 },             // F 键: 大地图纯净全屏沉浸模式
        "toggleFollowPlayer": { "key": 0, "modifiers": 0 }           // 视角跟随玩家开关
    }
}
```

---

## 2. 核心参数速查分类表

### 2.1 性能与渲染优化参数

| 参数名 | 类型 | 默认值 | 推荐配置 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| `useSqliteDatabase` | `boolean` | `true` | **`true`** | 启用 SQLite WAL 高并发存储，杜绝高速跑图掉帧卡顿。 |
| `fastWritesEnabled` | `boolean` | `true` | **`true`** | 启用内存批量写盘队列，保护 SSD 寿命。 |
| `writeBatchIntervalMs` | `integer` | `500` | `200`~`1000` | 批量落盘时间间隔（毫秒）。性能紧张时可设为 `1000`。 |
| `minimapFpsLimit` | `integer` | `0` | `0` / `60` | 小地图帧率限制。笔记本核显建议设为 `60` 显著降低功耗发热。 |
| `maxLoadedRegions` | `integer` | `64` | `32`~`128` | 内存中常驻的最大 512x512 区域切片数量。16GB 内存可设为 `128`。 |
| `multiLevelPyramid` | `boolean` | `true` | **`true`** | 预生成多级 MIP 地图纹理金字塔，使超大尺度缩放极其丝滑。 |

### 2.2 视觉与小地图控制参数

| 参数名 | 类型 | 默认值 | 说明 |
| :--- | :--- | :--- | :--- |
| `miniMapScale` | `float` | `1.0` | 小地图尺寸缩放，支持 `0.5` 到 `2.5` 线性缩放。 |
| `miniMapZoomRadius` | `integer` | `64` | 小地图方块可视范围半径（`16` ~ `128` 格）。 |
| `isSquareMap` | `boolean` | `false` | `false` 为圆形小地图，`true` 为方形小地图。 |
| `rotateMiniMap` | `boolean` | `true` | `true` 随玩家视线朝向平滑旋转，`false` 锁定正北。 |
| `transparentMinimapBackground` | `boolean` | `false` | 为 `true` 时，小地图未探测的虚空和迷雾完全透明。 |

### 2.3 导航与 3D 光柱参数

| 参数名 | 类型 | 默认值 | 说明 |
| :--- | :--- | :--- | :--- |
| `useNativeBeaconBeams` | `boolean` | `true` | 使用原版方块实体渲染管线绘制真实信标光柱。 |
| `show3DWaypoints` | `boolean` | `true` | 在 3D 世界中渲染路径点悬浮文字指示牌。 |
| `waypoint3DMaxDistance` | `float` | `2048.0` | 3D 指示牌最远渲染距离（格）。超出后自动隐藏。 |
| `autoRemoveDeathpoints` | `boolean` | `false` | 拾回掉落物（靠近死亡点 6 格内）后自动清理该死亡记录。 |

---

## 3. 手动编辑与备份建议

1. **游戏运行时热重载**：在游戏内修改设置时，系统会自动将变动持久化保存至 `config.json`；若使用文本编辑器手动编辑，请在**游戏完全关闭**时修改并保存，以防内存状态覆盖磁盘。
2. **格式安全核对**：编辑后请确保 JSON 语法合法（无多余逗号，闭合花括号正确），若语法损坏，系统在下次启动时会自动将其备份并回退生成一套全新出厂配置。

---

[← 上一章：08 - 快捷键、设置与性能调优](./08-settings-and-hotkeys.md) | [返回文档首页](../README_zh.md)
