#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <mc/deps/core/math/Color.h>

inline std::atomic<bool> g_mapDataUpdated{true};

namespace MapRenderState {
    inline std::atomic<int> frameCallCount{0};
    inline int lastFrameTotalCalls{1};
    inline bool showBigMap = false; // 大地图开启状态
    inline float bigMapOffsetX = 0.0f;
    inline float bigMapOffsetZ = 0.0f;
    inline float bigMapZoom = 3.0f; // 默认放大 3 倍
    inline bool bigMapShowEntities = false; // 全屏大地图生物头像显示开关 (直观图标按钮切换)
    inline bool bigMapShowMarkers = true;  // 全屏大地图标记显示开关 (路径点 + 死亡点，图标按钮切换，持久化)
    inline bool bigMapShowDisabledWaypoints = true; // 全屏大地图显示已禁用的路径点 (半透明，持久化)
    inline bool showChunkGrid = false;     // 区块网格显示开关 (小地图 + 大地图共用，图标按钮切换，持久化)
    inline bool bigMapShowHoverBox = true; // 全屏大地图鼠标悬停区块选框显示开关 (持久化)
    inline bool bigMapHideUI = false;      // 大地图全套界面隐藏状态 (按 F1 切换)
    inline bool bigMapFollowPlayer = true; // [锁定跟随玩家] 大地图锁定跟随玩家视角 (持久化，默认开)

    // [P1] 内嵌折叠路径点抽屉与镜头对齐状态 (持久化)
    inline bool bigMapWaypointDrawerOpen = false;  // 大地图路径点抽屉是否展开 (持久化)
    inline bool bigMapCloseDrawerAfterHop = false; // 镜头跳转后是否自动关闭抽屉，持久化

    // [P1] 镜头平滑跳转与平移对齐状态 (Camera Hopping / Pan Animation)
    inline bool cameraHopActive = false;
    inline float cameraHopStartOffsetX = 0.0f;
    inline float cameraHopStartOffsetZ = 0.0f;
    inline float cameraHopTargetOffsetX = 0.0f;
    inline float cameraHopTargetOffsetZ = 0.0f;
    inline std::chrono::steady_clock::time_point cameraHopStartTime;
    inline float cameraHopDuration = 0.35f;

    // [光标位置记忆与恢复] 记录离开全屏大地图时的光标屏幕坐标，在切回大地图时精准恢复，确保光标静止不动
    struct SavedCursorPos { long x = -1; long y = -1; };
    inline SavedCursorPos g_lastBigMapCursorPos = { -1, -1 };
    inline bool g_hasSavedBigMapCursor = false;

    // [原生界面与模组弹窗光标记忆] 记录离开物品栏、容器、暂停菜单等界面时的光标相对坐标，在重新打开时精准还原原版行为
    inline float g_lastScreenNormX = 0.5f;
    inline float g_lastScreenNormY = 0.5f;
    inline bool g_hasSavedScreenPos = false;

    // 当前玩家所处生物群系名称 (拆分为原始命名空间ID与本地化名称)
    inline std::string rawBiomeName = "minecraft:unknown";
    inline std::string translatedBiomeName = "Unknown Biome";

    // [大地图鼠标悬停生物群系] 独立于玩家当前生物群系，不影响小地图
    inline std::string hoverBiomeRawName = "";          // 鼠标位置原始生物群系名
    inline std::string hoverBiomeTranslatedName = "";   // 鼠标位置翻译后生物群系名
    inline float hoverBiomeAlpha = 0.0f;                 // 当前显示透明度 (0=隐藏, 1=完全显示)
    inline float hoverBiomeTargetAlpha = 0.0f;           // 目标透明度
    inline int hoverBiomeFrameCounter = 0;               // 节流帧计数器
    inline float hoverBiomeLastQueryX = 0.0f;            // 上次查询的世界 X
    inline float hoverBiomeLastQueryZ = 0.0f;            // 上次查询的世界 Z
    inline bool hoverBiomeHasValidResult = false;        // 是否有有效查询结果

    // [新增] 世界与维度隔离系统状态
    inline std::string currentWorldId = "";
    inline int currentDimensionId = -999;
    inline std::atomic<bool> clearGPUCache{false}; // 用于通知 GPU 清理旧世界的贴图残留
    inline std::atomic<uint32_t> g_spatialCacheGen{0}; // 空间视口缓存代数计数器

    // [维度切换] 全屏大地图浏览维度 (-999 表示跟随玩家物理维度，0=主世界, 1=下界, 2=末地)
    inline int bigMapViewDimensionId = -999;
    inline bool bigMapOverworldCave = false; // 全屏大地图主世界洞穴视图开关 (true=主世界洞穴, false=主世界地表)

    inline int GetEffectiveViewDimensionId() {
        if (bigMapViewDimensionId >= 0 && bigMapViewDimensionId <= 2) {
            return bigMapViewDimensionId;
        }
        return currentDimensionId;
    }

    // [世界出生点坐标]
    inline int worldSpawnX = 0;
    inline int worldSpawnY = 64;
    inline int worldSpawnZ = 0;
    inline bool hasWorldSpawn = false;

    inline void GetWorldSpawnCoords(int& outX, int& outZ) {
        if (hasWorldSpawn) {
            outX = worldSpawnX;
            outZ = worldSpawnZ;
            return;
        }
        size_t pPos = currentWorldId.rfind("_P");
        if (pPos != std::string::npos) {
            std::string s = currentWorldId.substr(pPos + 2);
            size_t under = s.find('_');
            if (under != std::string::npos) {
                try {
                    outX = std::stoi(s.substr(0, under));
                    outZ = std::stoi(s.substr(under + 1));
                    return;
                } catch (...) {}
            }
        }
        outX = 0;
        outZ = 0;
    }

    struct DimCameraState {
        float centerWx = 0.0f;
        float centerWz = 0.0f;
        float zoom = 3.0f;
        bool initialized = false;
    };
    inline DimCameraState dimCameraStates[3];

    inline void ResetDimCameraStates() {
        for (int i = 0; i < 3; ++i) {
            dimCameraStates[i] = DimCameraState{};
        }
    }

    inline void SaveDimCamera(int dim, float smoothPX, float smoothPZ) {
        if (dim >= 0 && dim < 3) {
            dimCameraStates[dim].centerWx = smoothPX - bigMapOffsetX / bigMapZoom;
            dimCameraStates[dim].centerWz = smoothPZ - bigMapOffsetZ / bigMapZoom;
            dimCameraStates[dim].zoom = bigMapZoom;
            dimCameraStates[dim].initialized = true;
        }
    }

    inline void RestoreDimCamera(int targetDim, int fromDim, float smoothPX, float smoothPZ) {
        if (targetDim < 0 || targetDim >= 3) return;
        if (dimCameraStates[targetDim].initialized) {
            bigMapZoom = std::clamp(dimCameraStates[targetDim].zoom, 0.01f, 50.0f);
            float targetWx = dimCameraStates[targetDim].centerWx;
            float targetWz = dimCameraStates[targetDim].centerWz;
            bigMapOffsetX = (smoothPX - targetWx) * bigMapZoom;
            bigMapOffsetZ = (smoothPZ - targetWz) * bigMapZoom;
        } else {
            bigMapZoom = 3.0f;
            float targetWx = 0.0f;
            float targetWz = 0.0f;
            if (targetDim == currentDimensionId) {
                targetWx = smoothPX;
                targetWz = smoothPZ;
            } else if (targetDim == 0) {
                if (currentDimensionId == 1) {
                    targetWx = smoothPX * 8.0f;
                    targetWz = smoothPZ * 8.0f;
                } else {
                    int spX = 0, spZ = 0;
                    GetWorldSpawnCoords(spX, spZ);
                    targetWx = (float)spX;
                    targetWz = (float)spZ;
                }
            } else if (targetDim == 1) {
                if (currentDimensionId == 0) {
                    targetWx = smoothPX / 8.0f;
                    targetWz = smoothPZ / 8.0f;
                } else {
                    targetWx = 0.0f;
                    targetWz = 0.0f;
                }
            } else if (targetDim == 2) {
                targetWx = 0.0f;
                targetWz = 0.0f;
            } else {
                targetWx = smoothPX;
                targetWz = smoothPZ;
            }
            bigMapOffsetX = (smoothPX - targetWx) * bigMapZoom;
            bigMapOffsetZ = (smoothPZ - targetWz) * bigMapZoom;
            dimCameraStates[targetDim].centerWx = targetWx;
            dimCameraStates[targetDim].centerWz = targetWz;
            dimCameraStates[targetDim].zoom = bigMapZoom;
            dimCameraStates[targetDim].initialized = true;
        }
    }

    inline void CenterCameraOnViewDimension(float smoothPX, float smoothPZ) {
        bigMapFollowPlayer = true;
        int viewDim = GetEffectiveViewDimensionId();
        if (viewDim == currentDimensionId) {
            bigMapOffsetX = 0.0f;
            bigMapOffsetZ = 0.0f;
        } else {
            float targetWx = 0.0f;
            float targetWz = 0.0f;
            if (currentDimensionId == 0 && viewDim == 1) {
                targetWx = smoothPX / 8.0f;
                targetWz = smoothPZ / 8.0f;
            } else if (currentDimensionId == 1 && viewDim == 0) {
                targetWx = smoothPX * 8.0f;
                targetWz = smoothPZ * 8.0f;
            } else if (viewDim == 0) {
                int spX = 0, spZ = 0;
                GetWorldSpawnCoords(spX, spZ);
                targetWx = (float)spX;
                targetWz = (float)spZ;
            } else if (viewDim == 1) {
                targetWx = 0.0f;
                targetWz = 0.0f;
            } else if (viewDim == 2) {
                targetWx = 0.0f;
                targetWz = 0.0f;
            }
            bigMapOffsetX = (smoothPX - targetWx) * bigMapZoom;
            bigMapOffsetZ = (smoothPZ - targetWz) * bigMapZoom;
        }
        SaveDimCamera(viewDim, smoothPX, smoothPZ);
    }

    inline void CenterBigMapOn(int targetDim, float targetWx, float targetWz, float smoothPX, float smoothPZ) {
        if (targetDim < 0 || targetDim > 2) return;
        int oldViewDim = GetEffectiveViewDimensionId();
        if (oldViewDim != targetDim) {
            SaveDimCamera(oldViewDim, smoothPX, smoothPZ);
            clearGPUCache.store(true);
        }
        bigMapViewDimensionId = targetDim;
        bigMapOffsetX = -(targetWx - smoothPX) * bigMapZoom;
        bigMapOffsetZ = -(targetWz - smoothPZ) * bigMapZoom;
        SaveDimCamera(targetDim, smoothPX, smoothPZ);
        showBigMap = true;
        hoverBiomeAlpha = 0.0f;
        hoverBiomeTargetAlpha = 0.0f;
        hoverBiomeHasValidResult = false;
    }

    // [新增] 路径点 UI 开启状态
    inline bool showWaypointUI = false;
    inline bool showDeathPointUI = false;
    inline int waypointSortMode = 0; // 0=时间(最近), 1=时间(最远), 2=名称A-Z, 3=名称Z-A, 4=距离近-远, 5=距离远-近, 6=手动排序
    inline std::string waypointFolderFilter = ""; // ""=全部文件夹, "__ROOT__"=未分类, 其他=文件夹名

    // [新增] 小地图路径点显示开关 (独立于路径点 enabled 属性，控制是否在小地图上绘制)
    inline bool showWaypointsOnMinimap = true;

    // [优先主世界路径点] 身处下界时跨维度渲染主世界路径点，且默认创建和管理主世界路径点
    inline bool preferOverworldWaypoints = false;

    // [新增] 小地图雷达显示开关 (控制是否扫描并显示周围生物/实体)
    inline bool showRadar = true;

    // [新增] 快捷键设置面板开启状态
    inline bool showHotkeySettings = false;
    inline bool showCaveSettings = false; // [洞穴地图] 洞穴设置面板
    inline bool showExportPNGScreen = false; // [PNG导出] World Map PNG Export 全屏面板开启状态
    inline bool showSeedMap = false; // [种子全知地图] 结构预测/群系查找/Slime区块面板开启状态

    // [PNG导出] PNG 导出面板状态与配置 (全部持久化保存于 config.json)
    inline bool exportForceFullMap = false;     // 强制全图导出 (持久化，默认关)
    inline bool exportMultipleImages = false;   // 多张无缩放图像 (持久化，默认关)
    inline bool exportNighttime = false;        // 夜间模式导出 (持久化，默认关)
    inline bool exportOpenFolder = true;        // 导出后自动打开所在文件夹 (持久化，默认开)
    inline int  exportScaleDownSquare = 20;     // 单张图像最大尺寸 (0=不限制, 1..90=NxN 区域，持久化，默认20)
    inline std::atomic<int> exportStage{0};     // 0=Idle, 1=Exporting, 2=Finished
    inline std::atomic<int> exportResultType{-1}; // -1=None, 0=SUCCESS, 1=EMPTY, 2=NOT_PREPARED, 3=TOO_BIG, 4=OUT_OF_MEMORY, 5=IO_EXCEPTION, 6=CANCELED
    inline std::atomic<bool> exportCancelRequested{false}; // 用户请求终止导出并清空已导出图片
    inline std::atomic<int>  exportTotalTiles{0};          // 多张分片导出总片数
    inline std::atomic<int>  exportCompletedTiles{0};      // 多张分片已完成片数
    inline std::string exportResultPath = "";   // 导出结果路径展示
    inline std::mutex exportResultMutex;

    // [PNG导出] 当前大地图视野范围与框选区域 (用于区分 强制全图导出: 关 vs 开)
    inline int  exportViewMinX = -256;
    inline int  exportViewMaxX = 256;
    inline int  exportViewMinZ = -256;
    inline int  exportViewMaxZ = 256;
    inline bool hasExportSelection = false;
    inline int  exportSelMinX = 0;
    inline int  exportSelMaxX = 0;
    inline int  exportSelMinZ = 0;
    inline int  exportSelMaxZ = 0;

    inline void OpenExportPNGScreen() {
        showExportPNGScreen = true;
        if (exportStage.load() != 1) {
            exportStage.store(0);
            exportResultType.store(-1);
            exportCancelRequested.store(false);
            exportTotalTiles.store(0);
            exportCompletedTiles.store(0);
            std::lock_guard<std::mutex> lk(exportResultMutex);
            exportResultPath.clear();
        }
    }

    // [快捷键增强] 单键或多按键组合结构 (支持 Ctrl / Shift / Alt 修饰键)
    // 默认值: M=0x4D, U=0x55, N=0x4E, Y=0x59, J=0x4A, Tab=0x09
    // key=0 表示已禁用 (清除设置)，不匹配任何按键事件
    // openBigMap 支持自定义按键，但不可清除 (严禁设为 0)，防止玩家因误清除导致无法呼出操作面板
    struct Hotkey {
        int key = 0;              // 虚拟键码 (Win32 VK_*)，0 表示已禁用 (清除)
        uint8_t modifiers = 0;    // 修饰键掩码 (MOD_CTRL, MOD_SHIFT, MOD_ALT)

        static constexpr uint8_t HK_MOD_CTRL  = 1 << 0;
        static constexpr uint8_t HK_MOD_SHIFT = 1 << 1;
        static constexpr uint8_t HK_MOD_ALT   = 1 << 2;

        constexpr bool IsEmpty() const { return key == 0; }
        void Clear() { key = 0; modifiers = 0; }

        constexpr bool operator==(const Hotkey& o) const {
            return key == o.key && modifiers == o.modifiers;
        }
        constexpr bool operator!=(const Hotkey& o) const {
            return !(*this == o);
        }
    };

    struct HotkeyBindings {
        Hotkey openBigMap        = { 0x4D, 0 }; // M: 切换大地图 (可自定义，不可清除)
        Hotkey openWaypointMgr   = { 0x55, 0 }; // U: 切换路径点管理器
        Hotkey openDeathPointMgr = { 0x49, 0 }; // I: 切换死亡记录管理器
        Hotkey toggleMinimap     = { 0x4E, 0 }; // N: 切换小地图显示
        Hotkey toggleMinimapShape= { 0x59, 0 }; // Y: 切换小地图形状
        Hotkey toggleMinimapRot  = { 0x4A, 0 }; // J: 切换小地图旋转
        Hotkey holdEntities      = { 0x09, 0 }; // Tab: 放大/显示生物头像（小地图/大地图）
        Hotkey toggleSeedMap     = { 0x4B, 0 }; // K: 切换种子全知地图
        Hotkey enlargeMinimap    = { 0x58, 0 }; // X: 放大小地图 (默认 X 键，避免与原版 Z 键状态效果界面冲突)
        Hotkey centerCamera      = { 0x20, 0 }; // Space: 大地图视角居中对齐玩家
        Hotkey selectExportArea  = { 0x10, 0 }; // Shift: 框选导出高清 PNG (默认 Shift 键)
        Hotkey toggleSlimeChunks = { 0, 0 };    // 切换小地图史莱姆区块显示 (默认未绑定)
        Hotkey switchWaypointSet = { 0, 0 };    // 切换路径点集合 (默认未绑定)
        Hotkey toggleHighways    = { 0, 0 };    // 切换通用主干道覆盖层 (默认未绑定)
        Hotkey toggleAnarchySettings = { 0, 0 }; // 切换无规则/区块探测设置面板 (默认未绑定)
        Hotkey toggleDrawingMode = { 0x50, 0 }; // P: 切换地图涂鸦模式 (默认 P 键)
        Hotkey toggleDoubleMinZoom = { 0, 0 };   // 切换小地图最小变焦翻倍 (默认未绑定)
        Hotkey toggleTransBg       = { 0, 0 };   // 切换未加载区块透明化 (默认未绑定)
        Hotkey minimapZoomIn       = { 0, 0 };   // 小地图视野放大 (默认未绑定)
        Hotkey minimapZoomOut      = { 0, 0 };   // 小地图视野缩小 (默认未绑定)
        Hotkey togglePreferOwWaypoints = { 0, 0 }; // 切换优先主世界路径点 (默认未绑定)
        Hotkey quickCreateWaypoint     = { 0, 0 }; // 快捷新建路径点 (默认未绑定)
        Hotkey toggleViewDistance       = { 0, 0 }; // 切换视距覆盖层 (默认未绑定)
        Hotkey toggleSimulationDistance = { 0, 0 }; // 切换模拟距离覆盖层 (默认未绑定)
        Hotkey toggleBigMapUI          = { 0x70, 0 }; // F1: 切换大地图全套界面隐藏 (默认 F1 键)
        Hotkey toggleFollowPlayer      = { 0, 0 };    // 切换大地图锁定跟随 (默认未绑定)

        // 系统预设默认值 (单一来源，供"单行重置"与"全部重置"共用)
        static constexpr HotkeyBindings Defaults() {
            return {
                { 0x4D, 0 },
                { 0x55, 0 },
                { 0x49, 0 },
                { 0x4E, 0 },
                { 0x59, 0 },
                { 0x4A, 0 },
                { 0x09, 0 },
                { 0x4B, 0 },
                { 0x58, 0 },
                { 0x20, 0 },
                { 0x10, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0x50, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0, 0 },
                { 0x70, 0 },
                { 0, 0 }
            };
        }
    };
    inline HotkeyBindings g_hotkeys;
    // 正在监听按键的重绑目标 (nullptr=未在监听)
    inline Hotkey* g_listeningHotkey = nullptr;
    // 监听状态下实时记录的修饰键组合 (Ctrl / Shift / Alt)
    inline uint8_t g_listeningModifiers = 0;
    // [快捷键增强] Ctrl+Z 撤销请求标志 (由 WndProc 设置，渲染线程消费)
    inline std::atomic<bool> g_hotkeyUndoRequested{false};
    // [路径点交互增强] Ctrl+Z 误删路径点恢复请求标志 (由 WndProc 设置，渲染线程消费)
    inline std::atomic<bool> g_waypointRestoreRequested{false};

    // [P3] 路径点集合系统
    inline std::string currentWaypointSet = "gui.chiyan_default"; // 当前激活集合 (默认 gui.chiyan_default)
    inline bool onlyShowCurrentWaypointSet = false;              // 仅在地图显示当前集合 (默认关)
    inline std::string g_waypointSetToast = "";                 // 集合切换 Toast 通知文本
    inline float g_waypointSetToastTimer = 0.0f;                // Toast 动画计时器 (秒)
    void TriggerWaypointSetToast(const std::string& setName);

    inline std::string g_minimapToast = "";                     // 小地图 Toast 通知文本
    inline float g_minimapToastTimer = 0.0f;                    // Toast 动画计时器 (秒)
    inline void TriggerMinimapToast(const std::string& msg) {
        g_minimapToast = msg;
        g_minimapToastTimer = 2.0f;
    }

    // ==================== 航点 3D 渲染与增强 ====================
    inline bool show3DWaypoints = true;            // 启用游戏内 3D 航点渲染
    inline bool show3DWaypointBeams = true;        // 渲染 3D 贯天信标光柱
    inline bool useNativeBeaconBeams = true;       // 调用原版游戏内置信标光柱
    inline bool show3DWaypointLabels = true;       // 悬浮显示航点名称标签
    inline bool show3DWaypointDistance = true;     // 悬浮显示距离数值
    inline bool waypoint3DClampToScreen = true;    // 屏幕边缘方位指示与贴边吸附
    inline int  waypoint3DMaxDistance = 5000;      // 最大渲染距离 (格)，0 为无限制
    inline float waypoint3DBeamOpacity = 0.65f;    // 光柱不透明度 (0.10f ~ 1.00f)
    inline float waypoint3DBeamRadius = 0.40f;     // 光柱宽度比例 (0.10f ~ 1.50f)
    inline float waypoint3DScale = 1.0f;           // 3D 航点图标与文字整体缩放 (0.50f ~ 2.00f)
    inline bool show3DDeathpoints = true;          // 游戏内 3D 渲染上次死亡点

    // 原版世界摄像机参数捕获: 与原版信标渲染管线严格同步
    struct WorldCameraCapture {
        float posX = 0.0f;
        float posY = 0.0f;
        float posZ = 0.0f;
        float fwdX = 0.0f;
        float fwdY = 0.0f;
        float fwdZ = 1.0f;
        float rgtX = 1.0f;
        float rgtY = 0.0f;
        float rgtZ = 0.0f;
        float upX = 0.0f;
        float upY = 1.0f;
        float upZ = 0.0f;
        float fx = 0.0f;
        float fy = 0.0f;
        float fovRad = 1.2217f;
        float aspect = 16.0f / 9.0f;
        bool hasProj = false;
        uint64_t frameId = 0;
    };
    inline WorldCameraCapture g_worldCameraCapture;


    // [P3] 小地图边框样式与罗盘个性化 (Minimap Frame Styling & Color)
    inline int   miniMapFrameStyle = 0;                          // 边框样式: 0=经典默认, 1=纯色粗框(4px), 2=纯色细框(2px), 3=无边框
    inline float miniMapFrameColor[4] = { 0.12f, 0.12f, 0.12f, 1.0f }; // 边框自定义 RGBA 颜色
    inline int   compassLocation = 1;                            // 罗盘字标位置: 0=地图内侧, 1=边框圆环上
    inline int   northCompassColorMode = 0;                      // 北向标颜色模式: 0=经典鲜红, 1=跟随边框色, 2=自定义颜色
    inline float northCompassCustomColor[4] = { 1.0f, 0.29f, 0.29f, 1.0f }; // 北向标自定义颜色
    inline bool  keepEnlargedMinimapRotation = false;            // 临时放大小地图时保持视角旋转 (默认关)

    // [P1] 史莱姆区块覆盖层与 HUD 推挤避让系统
    inline bool showSlimeChunks = true;        // 小地图史莱姆区块覆盖层 (默认开)
    inline int  pushboxBossHealth = 1;         // Boss 血条避让推挤框 (0=关闭, 1=常规避让, 2=整屏推挤)
    inline int  pushboxPotionEffects = 1;      // 药水状态避让推挤框 (0=关闭, 1=常规避让, 2=整屏推挤)
    inline float g_currentPushOffsetY = 0.0f;  // 运行时小地图平滑推挤 Y 偏移量
    inline float g_currentPushOffsetX = 0.0f;  // 运行时小地图平滑推挤 X 偏移量
    inline std::atomic<bool> g_hasActiveBoss{false}; // 是否激活 Boss 血条
    inline std::atomic<int>  g_activeEffectCount{0}; // 玩家当前活跃状态效果数量

    // [新增] 小地图偏移与位置设置
    inline float miniMapOffsetX = 0.0f; // 小地图 X 偏移
    inline float miniMapOffsetY = 0.0f; // 小地图 Y 偏移
    inline bool showMiniMapPosSettings = false; // 小地图位置设置面板 (切换至小地图界面调整)
    inline bool showMiniMapSettings = false;    // 小地图设置面板 (在大地图内渲染)
    inline bool showBigMapSettings = false;     // 全屏大地图设置面板 (在大地图内渲染)

    // [新增] 实体雷达高低差指示与探索增强配置
    inline bool radarHeightIndicators = true;   // 实体雷达高低差指示箭头 (▲/▼)
    inline bool entityDepth = true;             // 显示实体深度 (按相对高低差渐变暗化)
    inline int  entityHeightLimit = 0;          // 实体垂直高度限制 (0=不限)
    inline bool autoRemoveDeathpoints = true;   // 靠近6方块内自动清除死亡点
    inline bool enlargeMinimapToggle = false;   // 放大小地图按键视为切换 (false=按住, true=切换)
    inline std::atomic<bool> g_enlargeHeld{false}; // 放大小地图按住状态
    inline bool g_enlargeToggled = false;       // 放大小地图切换状态

    // [实体雷达分类过滤] (持久化保存)
    inline bool radarShowPlayers = true;   // 显示玩家
    inline bool radarShowHostile = true;   // 显示敌对生物
    inline bool radarShowFriendly = true;  // 显示友好/被动生物
    inline bool radarShowItems = false;    // 显示掉落物与载具 (默认关，防止杂乱)

    // [P4] 高级实体雷达系统 (Advanced Radar System)
    inline int   radarOverFrameMode = 0;       // 越界吸附边框: 0=关闭, 1=全部实体, 2=仅敌对生物, 3=仅其他玩家, 4=仅按住Tab时
    inline bool  radarHighlightTamed = true;   // 已驯服生物高亮显示 (默认开)
    inline float radarDepthFadeStart = 1.5f;   // 景深淡出起始高度差 (默认 1.5 格, 1.0~30.0)
    inline char  radarFilterInclude[128] = ""; // 自定义包含规则 (逗号分隔通配符, 非空则仅显示匹配项)
    inline char  radarFilterExclude[128] = ""; // 自定义排除规则 (逗号分隔通配符, 匹配项被隐藏)

    // [大地图路径点缩放] (持久化保存)
    inline float bigMapWaypointScale = 1.0f; // 0.5x ~ 2.5x
    // [密集路径点最小缩放阈值] 当大地图缩放低于此阈值时自动隐藏普通局部路径点，仅显示置顶(★)与临时路径点 (0.0=关闭，持久化)
    inline float minZoomForLocalWaypoints = 0.0f; // 0.0f ~ 2.0f
    // [路径点文字背景底板开关] 控制是否为路径点名称文字渲染半透明背景底板框 (默认开启，持久化)
    inline bool renderWaypointBackgrounds = true;
    // [路径点智能防遮挡] 当多个路径点文字在屏幕上发生矩形碰撞时，智能隐藏次要文字仅保留图标 (默认开启，持久化)
    inline bool declutterWaypointLabels = true;
    // [远景自动隐藏普通标签] 当大地图缩小到远景全局俯瞰视角(Zoom < 0.35x)时，普通路径点自动只保留图标点，隐藏长名称以防遮挡地图 (默认开启，持久化)
    inline bool autoHideLabelsOnFarZoom = true;

    // [大地图生物图标缩放] (持久化保存)
    inline float bigMapEntityScale = 1.0f; // 0.5x ~ 2.5x

    // [小地图指南针方位标] (持久化保存)
    inline bool showCompass = true;       // 显示小地图东南西北方位标
    inline float compassScale = 1.0f;     // 方位标字号缩放比例 (0.8x ~ 2.0x)

    // [传送状态机] 用于 UI 加载提示与异常反馈
    // Idle: 无传送 / Loading: 等待区块加载 / Validating: 验证地表 / Failed: 异常回退
    enum class TeleportState : int { Idle = 0, Loading = 1, Validating = 2, Done = 3, Failed = 4 };
    inline std::atomic<int> teleportState{(int)TeleportState::Idle};
    inline std::string teleportStatusMsg;     // 给 UI 显示的简短状态文本（如"加载地形中..."）
    inline std::string teleportFailReason;    // 失败原因（供日志与调试）
    // [多世界 / 子世界地图实例管理] UI 面板状态
    inline bool showMapSelectionUI = false; // 地图选择与管理模态面板
    inline bool showNewMapModal = false;    // 新建地图模态窗口
    inline bool showRenameMapModal = false; // 重命名地图模态窗口
    inline bool showConnectMapModal = false;// 互联管理模态窗口
    inline std::string selectedConfigMapId = ""; // 正在配置互联的地图 ID
    inline std::string renamingMapId = "";       // 正在重命名的地图 ID
    inline char newMapNameInput[128] = "";       // 新建地图名称输入缓冲
    inline char renameMapNameInput[128] = "";    // 重命名输入缓冲
    inline bool showTeleportNotConnectedWarning = false; // 传送未连接警告

    inline bool showAnarchySettings = false; // 是否显示无规则/区块探测设置窗口

    // [统一拦截枢纽] 判断是否有任何全屏 UI 处于活动状态
    inline bool IsUIActive() {
        int tp = teleportState.load();
        bool isTeleportUIActive = (tp == (int)TeleportState::Loading || 
                                   tp == (int)TeleportState::Validating || 
                                   tp == (int)TeleportState::Failed);
        return showBigMap || showWaypointUI || showDeathPointUI || showMiniMapPosSettings || showMiniMapSettings || showBigMapSettings || showHotkeySettings || showCaveSettings || showAnarchySettings ||
               showExportPNGScreen || showSeedMap || showMapSelectionUI || showNewMapModal || showRenameMapModal || showConnectMapModal || showTeleportNotConnectedWarning || isTeleportUIActive;
    }

    // [新增] 跨菜单桥接：大地图右键唤起新建地标的预设坐标与归属维度
    inline bool triggerAddWaypoint = false;
    inline bool addWaypointFromBigMap = false; // 是否直接从大地图唤起创建地标窗口
    inline int addWaypointX = -999999;
    inline int addWaypointY = -999999;
    inline int addWaypointZ = -999999;
    inline int addWaypointDim = -1; // -1=当前维度, 0=主世界, 1=下界, 2=末地
    inline int addWaypointIsCave = -1; // -1=自动, 0=地表, 1=洞穴

    // [新增] 原生瞬间传送信号器
    inline std::atomic<bool> triggerTeleport{false};
    inline float tpTargetX = 0.0f;
    inline float tpTargetY = 0.0f;
    inline float tpTargetZ = 0.0f;
    inline int tpTargetDim = -1; // 目标维度: 0=主世界 1=下界 2=末地; -1=未指定(默认当前维度)

    // [两阶段地表探测] 未访问区域的分阶段传送状态
    // Phase 0: 已 tp 到 Y=320 触发服务器下发目标区块，等待客户端区块就绪
    // Phase 1: 区块就绪后 re-tp 到实际地表（/tp 重置坠落距离，无需缓降）
    // 仅在缓存与实时探测均未命中（玩家从未到访）时启用
    inline std::atomic<bool> pendingSurfaceProbe{false};
    inline std::atomic<int> probeTargetX{0};
    inline std::atomic<int> probeTargetZ{0};
    inline float probeOriginalX = 0.0f;  // 探测超时回退坐标
    inline float probeOriginalY = 0.0f;
    inline float probeOriginalZ = 0.0f;
    inline std::chrono::steady_clock::time_point probeStartTime;

    // [维度感知传送] 探测模式: 0=地表(主世界非洞穴), 1=洞穴/下界, 2=末地
    // Phase 1 轮询时根据此值选择 SafeFindSafeSpawnY(地表) 或 SafeFindSafeSpawnYNearY(洞穴/下界/末地)
    inline int probeMode = 0;

    // [Y范围限制] 探测时 SafeFindSafeSpawnYNearY 的扫描范围
    // 下界 [2,125] 避开基岩层; 洞穴 [refY-48, refY+48] 避免穿过天花板到地表
    inline int probeMinY = -64;
    inline int probeMaxY = 319;

    // [探测参考Y] Phase 1 搜索的起始Y参考值
    // 下界=125(从顶部往下搜); 洞穴/末地=玩家原始Y
    inline int probeRefY = 64;

    // [下界探测标志] 标记当前探测是否为下界传送, 用于延长超时时间
    inline bool probeIsNether = false;

    // [安全传送增强] 探测稳定性检查：连续 N 帧返回相同 Y 才视为"区块加载完成"
    // 防止部分加载区块返回临时错误 Y（如地下结构）导致地下传送
    // v2: 从 2 帧升级到 3 帧，进一步过滤部分加载区块的"稳定但错误"Y 值
    inline short probeLastY = -32000;         // 上一帧探测到的 Y
    inline int  probeLastX = 0;               // 上一帧探测到的 X
    inline int  probeLastZ = 0;               // 上一帧探测到的 Z
    inline int  probeStableCount = 0;         // 连续一致帧数
    constexpr static int kProbeStableThreshold = 8;  // 需要的连续一致帧数（6→8，约133~160ms）


    inline bool showMiniMap = true;  // 是否显示小地图
    inline bool isSquareMap = false; // 是否为方形小地图
    inline bool rotateMiniMap = false; // 小地图跟随视角旋转
    inline float globalUIScale = 1.0f; // 全局整体 UI 缩放比例 (默认 1.0x，支持 0.5x~2.5x 调节)
    inline float& uiTextScale = globalUIScale; // 向后兼容旧字段别名引用

    // [UI 自适应缩放] 根据屏幕/窗口纵向分辨率计算推荐的最佳 UI 缩放比例
    // 1080p及以下: 1.00x | 1200p: 1.10x | 1440p(2K): 1.25x | 1600p: 1.50x | 1800p: 1.65x | 2160p(4K): 2.00x | 2880p(5K)+: 2.50x
    float CalculateOptimalUIScale(float displayH);
    float GetOptimalUIScale(float displayH = 0.0f);

    inline float miniMapScale = 1.0f; // 小地图本身大小缩放
    inline float miniMapZoomRadius = 50.0f; // 小地图可视范围（玩家周围方块半径，10-200）
    inline bool minimapDoubleMinZoom = false; // 小地图最小变焦翻倍 (开启后视野缩放范围翻倍放大，展示 2 倍半径宏观地形)
    inline bool transparentMinimapBackground = false; // 小地图未加载区块透明化 (未扫描/未加载区域完全透明透出游戏画面)

    // [底层数据持久化与细节体验配置]
    inline bool useSqliteDatabase = true;          // 启用 SQLite 嵌入式结构化数据库
    inline int  minimapFpsLimit = 0;               // 小地图独立刷新率限制 (0=跟随主帧率, 60, 30, 20, 15)
    inline bool bigMapCrossDimCoords = true;       // 大地图光标跨维度换算显示 (主世界 ↔ 下界 8:1)
    inline bool persistBigMapDimension = true;     // 持久大地图查看维度 (记住上次查看维度)
    inline int  lastViewedDimensionId = 0;         // 上次查看的大地图维度
    inline bool teleportFailNotifier = true;       // 缺少权限时的传送失败检测提示
    inline bool fastWritesEnabled = true;          // 高性能写入节流控制 (合并高频磁盘事务)
    inline int  writeBatchIntervalMs = 500;        // 写入批处理节流间隔 (毫秒)

    // [小地图 HUD 实时信息栏配置]
    inline bool infoShowCoords = true;       // 显示 XYZ 坐标，第1项默认开
    inline bool infoShowNetherCoords = false; // 显示下界/主世界换算坐标 (第2项: 默认关)
    inline bool infoShowFacing = false;       // 显示朝向与偏航角 (第3项: 默认关)
    inline bool infoShowAngles = false;       // 显示精准视角角度 (默认关)
    inline bool infoShowBiome = true;        // 显示当前生物群系 (第4项: 默认开)
    inline bool infoShowWeather = false;      // 显示天气状态 (默认关)
    inline bool infoShowTime = false;         // 显示游戏时间与现实时间 (第5项: 默认关)
    inline bool infoShowLight = false;        // 显示方块光照等级 (默认关)
    inline bool infoShowChunkCoords = false;  // 显示区块局部坐标与索引 (默认关)
    inline bool infoShowBackground = true;    // 显示小地图信息栏深色背景底板 (默认开)
    inline bool alwaysShowNametags = true;    // 始终显示已命名实体名称 (默认开)
    inline std::atomic<int> g_blockLight{-1}; // 当前方块光照 (0-15)
    inline std::atomic<int> g_skyLight{-1};   // 当前天空光照 (0-15)
    inline std::atomic<int> g_gameTimeTicks{-1}; // 游戏刻实时同步 (主线程 -> 渲染线程)
    inline std::atomic<float> g_weatherRainLevel{0.0f};      // 降雨强度 (0.0~1.0)
    inline std::atomic<float> g_weatherLightningLevel{0.0f}; // 雷暴强度 (0.0~1.0)
    inline std::atomic<bool>  g_isCurrentPosRainAllowed{true}; // 当前群系是否允许降水
    inline std::atomic<bool>  g_isCurrentPosSnowing{false};     // 当前位置群系/高度是否降雪

    // [关闭期安全标志] disable() 入口处置 true，所有钩子和后台线程在入口检查此标志并提前返回，
    // 防止进程退出阶段访问已释放的 D3D/ImGui 资源导致 0xC0000005 退出崩溃
    inline std::atomic<bool> g_isShuttingDown{false};

    // ==================== 洞穴地图系统 ====================
    // 洞穴模式类型
    // 0=Off(关闭), 1=Layered(分层), 2=Full(全深度)
    enum class CaveModeType : int { Off = 0, Layered = 1, Full = 2 };

    // 洞穴模式设置 (持久化到 config)
    inline int g_caveModeType = (int)CaveModeType::Layered;         // 当前维度洞穴模式类型 (0=Off, 1=Layered, 2=Full)
    inline int g_defaultCaveModeType = (int)CaveModeType::Layered;  // 各维度默认洞穴类型 (Default Cave Mode Type)
    inline int g_dimCaveModeTypes[3] = { -1, -1, -1 };              // 各维度独立洞穴模式覆盖 (-1=跟随默认, 0=Off, 1=Layered, 2=Full)
    inline bool g_caveTopYAuto = true;                              // Cave Mode Top Y: auto=自动检测, false=手动指定
    inline int g_caveTopY = 64;                                     // Top Y: 手动模式下的洞穴顶层Y (默认 64=海平面, 有效范围 [-64,320])
    inline int g_caveDepth = 30;                                    // 洞穴扫描深度 (1-64, 默认 30)
    inline bool g_legibleCaveMaps = false;                          // 清晰洞穴地图 (默认 false)
    inline int g_autoCaveMode = -1;                                 // 自动洞穴天花板检测尺寸 (-1=智能自适应, 0=关闭自动进入, 1=1x1, 2=3x3, 3=5x5, 4=7x7, 5=9x9)
    inline int g_caveModeToggleTimer = 1000;                        // 防闪烁切换冷却计时器 (毫秒, 0~5000, 默认 1000ms)
    inline bool g_caveAllowedOverworld = true;                      // 允许主世界启用洞穴模式 (默认 true)
    inline bool g_caveAllowedNether = true;                         // 允许下界启用洞穴模式 (默认 true)
    inline bool g_caveAllowedEnd = false;                           // 允许末地启用洞穴模式 (默认 false)

    // 洞穴模式运行时状态 (每帧计算)
    inline bool g_caveModeActive = false;       // 当前是否处于洞穴模式 (玩家在地下)
    inline int g_caveStartY = 0;                // 当前洞穴起始 Y (扫描顶部)
    inline int g_lastCaveDimensionId = -999;    // 上次同步洞穴模式类型的维度 ID
    inline bool g_caveHasPendingToggle = false; // 是否存在处于防闪烁计时中的状态切换请求
    inline bool g_cavePendingTargetState = false;
    inline std::chrono::steady_clock::time_point g_cavePendingSinceTime = std::chrono::steady_clock::now();

    // [辅助] 判断指定维度是否在洞穴模式白名单内
    inline bool IsCaveModeAllowedInDimension(int dimId) noexcept {
        if (dimId == 0) return g_caveAllowedOverworld;
        if (dimId == 1) return g_caveAllowedNether;
        if (dimId == 2) return g_caveAllowedEnd;
        return false;
    }

    // [辅助] 获取指定维度的有效洞穴模式类型 (若未单独覆盖则返回 g_defaultCaveModeType)
    inline int GetDimensionCaveModeType(int dimId) noexcept {
        if (dimId >= 0 && dimId < 3 && g_dimCaveModeTypes[dimId] >= 0 && g_dimCaveModeTypes[dimId] <= 2) {
            return g_dimCaveModeTypes[dimId];
        }
        return std::clamp(g_defaultCaveModeType, 0, 2);
    }

    // [辅助] 同步指定维度的洞穴模式类型到 g_caveModeType
    inline void SyncCaveModeTypeForDimension(int dimId) noexcept {
        g_caveModeType = GetDimensionCaveModeType(dimId);
    }

    // [辅助] 设置指定维度的洞穴模式类型并同步当前值
    inline void SetDimensionCaveModeType(int dimId, int type) noexcept {
        int clamped = std::clamp(type, 0, 2);
        if (dimId >= 0 && dimId < 3) {
            g_dimCaveModeTypes[dimId] = clamped;
        }
        g_caveModeType = clamped;
    }


    // ==================== 地形坡度、光影与色彩系统 ====================
    // 0=Default 2D, 1=Legacy, 2=Default 3D (浮雕立体阴影)
    inline int terrainSlopes = 2;
    inline bool terrainDepth = true;
    inline bool adjustHeightForShortBlocks = true;

    // 0=Accurate (精确纹理色), 1=Vanilla (原版地图色)
    inline int blockColorMode = 0;
    inline bool biomeColorsInVanilla = true;   // 原版模式下群系染色
    inline bool mapLighting = true;            // 地图光照渲染 (记录并动态应用方块光照与昼夜明暗)
    inline bool displayStainedGlass = true;    // 显示染色玻璃与半透明覆盖层 (Overlay)
    inline bool loadFlowers = true;            // 加载花朵 (渲染地表花朵色彩)
    inline bool biomeBlending = true;          // 生物群系颜色平滑混合
    inline bool openingAnimation = true;       // 打开全屏大地图平滑缩放过渡动画
    inline std::chrono::steady_clock::time_point g_bigMapOpenTime = std::chrono::steady_clock::now();
    inline std::atomic<bool> forceRescan{false}; // 渲染取色/过滤设置变更时触发立即重扫

    // ==================== 地图数据源、读写控制与多级缓存架构 ====================
    inline bool g_loadNewChunks = true;        // 加载新区块 (Load New Chunks): 是否记录尚未探索过的新区块/新像素
    inline bool g_updateChunks = true;         // 更新已探索区块 (Update Chunks): 是否更新已存在缓存的区块 (关闭后对已探索区域只读保护)
    inline int  g_mapWritingDistance = 0;      // 地图写入最大距离限制 (Map Writing Distance): 0 = 无限制(跟随完整扫描半径), 1 ~ 32 区块
    inline bool g_ignoreHeightmaps = false;    // 忽略服务端高度图 (Ignore Server Heightmaps): 开启后强制从高处向下逐格扫描真实顶面方块
    inline int  g_maxLoadedRegions = 512;      // 内存最大驻留区域数 (Max Loaded Regions): 64 ~ 4096 (超出后自动按 LRU 淘汰落盘释放内存)
    inline bool g_multiLevelPyramid = true;    // 四叉树多级金字塔纹理 (BranchLeveledRegion LOD): 大幅缩小时自动合并 2x2 / 4x4 区域降采样渲染

    // 当前全屏大地图视口覆盖的区域范围 (供 Reload Viewed Regions 使用)
    inline int g_viewedMinRx = 0;
    inline int g_viewedMaxRx = 0;
    inline int g_viewedMinRz = 0;
    inline int g_viewedMaxRz = 0;

    // 区域重载/转换操作状态反馈
    inline std::string g_regionOpStatusText = "";
    inline std::chrono::steady_clock::time_point g_regionOpStatusTime = std::chrono::steady_clock::now();

    // ==================== 无规则与区块历史探测 ====================
    // 1. 液体流动新区块
    inline bool anarchyLiquidNewChunks = false;
    inline bool anarchyLiquidNewChunksInverse = false;
    inline bool anarchyLiquidNewChunksOnlyAboveY0 = false;
    inline float anarchyLiquidNewChunksColor[4] = { 1.0f, 0.15f, 0.15f, 0.45f };
    inline float anarchyLiquidNewChunksInverseColor[4] = { 0.15f, 0.90f, 0.35f, 0.30f };

    // 2. 调色板与原生新区块
    inline bool anarchyPaletteNewChunks = false;
    inline bool anarchyPaletteNewChunksInverse = false;
    inline float anarchyPaletteNewChunksColor[4] = { 1.0f, 0.45f, 0.05f, 0.45f };
    inline float anarchyPaletteNewChunksInverseColor[4] = { 0.25f, 0.70f, 0.95f, 0.30f };

    // 3. 老区块探测
    inline bool anarchyOldChunks = false;
    inline bool anarchyOldChunksInverse = false;
    inline int  anarchyOldChunksMode = 2; // 0=经典1.12, 1=基岩版Pre-1.18, 2=基岩版26.51升级存档探测, 3=全代综合断层分析
    inline bool anarchyOldChunksBedrockBlending = true; // 启用 26.51 融合升级老区块高亮
    inline float anarchyOldChunksColor[4] = { 1.0f, 0.90f, 0.10f, 0.45f }; // 老区块高亮颜色 (金黄)
    inline float anarchyOldChunksInverseColor[4] = { 0.20f, 0.50f, 1.0f, 0.35f }; // 现代新区块高亮颜色 (天蓝)
    inline float anarchyOldChunksBlendedColor[4] = { 0.95f, 0.55f, 0.15f, 0.40f }; // 融合升级老区块高亮颜色 (暖橙)

    // 4. 经典旧生物群系
    inline bool anarchyOldBiomes = false;
    inline float anarchyOldBiomesColor[4] = { 0.10f, 0.85f, 0.85f, 0.45f };
    inline int64_t anarchyOldBiomesSeed = -4172144997902289642LL;

    // 5. 传送门高亮
    inline bool anarchyPortals = false;
    inline float anarchyPortalsColor[4] = { 0.80f, 0.15f, 0.95f, 0.60f };

    // 6. 传送门跳转检测
    inline bool anarchyPortalSkip = false;
    inline int anarchyPortalSkipRadius = 7;
    inline float anarchyPortalSkipColor[4] = { 0.10f, 0.95f, 0.95f, 0.50f };

    // 7. 通用坐标道路系统
    inline bool anarchyHighways = false;
    inline int  anarchyHighwaysMode = 0;              // 0=经典预设体系, 1=等距通用系统, 2=自由距离列表
    inline bool anarchyHighwaysMainAxes = true;       // 主坐标轴 X=0, Z=0
    inline bool anarchyHighwaysDiagAxes = true;       // 对角线轴 X=±Z
    inline bool anarchyHighwaysRingRoads = true;      // 同心方环
    inline bool anarchyHighwaysDiamondRoads = true;   // 同心菱形道路
    inline bool anarchyHighwaysGridRoads = false;     // 平行等距网格道路
    inline int  anarchyHighwaysRingInterval = 1000;   // 方环间距 (格)
    inline int  anarchyHighwaysDiamondInterval = 2500;// 菱形环间距 (格)
    inline int  anarchyHighwaysGridSpacing = 1000;    // 平行网格道路间距 (格)
    inline int  anarchyHighwaysMaxCoord = 3750000;    // 最大绘制范围 (格)
    inline char anarchyHighwaysCustomRingsStr[256] = "200, 500, 1000, 1500, 2000, 2500, 5000, 10000, 25000, 50000";
    inline bool anarchyHighwaysDimOverworld = true;   // 主世界显示
    inline bool anarchyHighwaysDimNether = true;      // 下界显示
    inline bool anarchyHighwaysDimEnd = false;        // 末地显示
    inline float anarchyHighwaysWidth = 2.0f;         // 道路线宽
    inline float anarchyHighwaysColor[4] = { 0.10f, 0.65f, 1.0f, 0.80f }; // 主轴/主环路颜色
    inline float anarchyHighwaysSecondaryColor[4] = { 0.15f, 0.85f, 0.60f, 0.60f }; // 次级网格/环路颜色
    inline bool  anarchyHighwaysShowLabels = false;   // 道路坐标距离标注

    // 8. 下界岩浆柱探测
    inline bool anarchyLavaColumns = false;
    inline int anarchyLavaColumnsMinHeight = 15;
    inline float anarchyLavaColumnsAlphaShift = 0.10f;
    inline float anarchyLavaColumnsAlphaStep = 0.02f;
    inline float anarchyLavaColumnsColor[4] = { 1.0f, 0.40f, 0.0f, 0.55f };

    // 9. 下界黑曜石平台透视
    inline bool transparentObsidianRoof = false;
    inline int transparentObsidianRoofY = 125;
    inline float roofObsidianDarkening = 0.75f;
    inline float roofSnowOpacity = 0.25f;

    // 10. 磁盘持久化存储
    inline bool anarchySaveLoadToDisk = true;

    // ==================== 探索足迹与个性化 ====================
    inline bool showFootsteps = true;
    inline int playerArrowColor = 0; // 0=Red, 1=White, 2=Green, 3=Blue, 4=Yellow, 5=Purple, 6=Black, 7=Cyan
    inline float playerArrowScale = 1.0f; // 玩家箭头大小缩放 (0.50x ~ 2.00x)
    inline bool timeFormat24h = true;     // 24小时制时间格式 (true=24h, false=12h AM/PM)
    inline bool showWaypointDistance = false; // 小地图路径点显示直线距离
    inline bool showZoomButtons = true; // 大地图角落缩放按钮
    inline bool showDrawingsOnMinimap = true; // 小地图显示涂鸦标注 (默认开)

    struct FootstepPoint {
        float x;
        float y;
        float z;
        int dimId;
        float timestamp;
    };
    inline std::vector<FootstepPoint> g_footsteps;
    inline std::mutex g_footstepsMutex;

    inline void AddFootstep(float x, float y, float z, int dimId, float currentTime) {
        std::lock_guard<std::mutex> lock(g_footstepsMutex);
        if (!g_footsteps.empty()) {
            const auto& last = g_footsteps.back();
            if (last.dimId == dimId) {
                float dx = x - last.x;
                float dz = z - last.z;
                if (dx * dx + dz * dz < 6.25f) return; // 移动距离小于 2.5 格则不新增
            }
        }
        g_footsteps.push_back({x, y, z, dimId, currentTime});
        if (g_footsteps.size() > 200) {
            g_footsteps.erase(g_footsteps.begin(), g_footsteps.begin() + 40);
        }
    }

    inline bool IsShortBlock(const std::string& name) {
        if (name.empty()) return false;
        if (name.find("carpet") != std::string::npos) return true;
        if (name.find("snow") != std::string::npos && name.find("block") == std::string::npos) return true;
        if (name.find("lily_pad") != std::string::npos || name.find("waterlily") != std::string::npos) return true;
        if (name.find("trapdoor") != std::string::npos) return true;
        if (name.find("repeater") != std::string::npos || name.find("comparator") != std::string::npos) return true;
        if (name.find("rail") != std::string::npos) return true;
        if (name.find("pressure_plate") != std::string::npos) return true;
        if (name.find("button") != std::string::npos) return true;
        if (name.find("redstone_wire") != std::string::npos) return true;
        if (name.find("candle") != std::string::npos) return true;
        if (name.find("flower_pot") != std::string::npos) return true;
        return false;
    }

    inline float ComputeTerrainShading(float currentY, float northY, float westY, float northWestY, int slopeMode, bool enableDepth, bool isCave, uint8_t packedLight = 0x00) {
        float shade = 1.0f;
        float verticalSlope = currentY - northY;

        if (slopeMode == 1) { // Legacy
            if (verticalSlope > 0.0f) shade = 1.15f;
            else if (verticalSlope < 0.0f) shade = 0.85f;
        } else if (slopeMode == 2) { // Default 3D (浮雕立体阴影)
            float diagonalSlope = currentY - northWestY;
            if (verticalSlope == 0.0f && diagonalSlope == 0.0f) {
                shade = 0.9714138f;
            } else {
                float crossX = verticalSlope - diagonalSlope;
                float crossZ = -verticalSlope;
                float crossMagnitude = std::sqrt(crossX * crossX + 1.0f + crossZ * crossZ);
                float cosAngle = (1.0f - crossZ) / (crossMagnitude * 1.41421356f);
                if (cosAngle > 1.0f) cosAngle = 1.0f;
                if (cosAngle < 0.0f) cosAngle = 0.0f;
                float directLight = (cosAngle >= 0.999f) ? 0.6667f : (std::ceil(cosAngle * 10.0f) / 10.0f) * 0.6667f * 0.88388f;
                shade = 0.50f + directLight;
            }
        } else { // Default 2D
            if (verticalSlope == 0.0f) {
                shade = 0.9714138f;
            } else {
                float surfaceDirectionMagnitude = std::sqrt(verticalSlope * verticalSlope + 1.0f);
                float cosAngle = (verticalSlope + 1.0f) / (surfaceDirectionMagnitude * 1.41421356f);
                if (cosAngle > 1.0f) cosAngle = 1.0f;
                if (cosAngle < 0.0f) cosAngle = 0.0f;
                float directLight = (cosAngle >= 0.999f) ? 0.6667f : (std::ceil(cosAngle * 10.0f) / 10.0f) * 0.6667f * 0.88388f;
                shade = 0.50f + directLight;
            }
        }

        // 强自发光方块 (如熔岩/萤石/蛙明灯，blockLight >= 14) 凹陷于地面时，消除 0.50x 深褐色陡坡暗影，保留明亮熔融质感
        if ((packedLight & 0x0F) >= 14 && shade < 1.0f) {
            shade = 0.90f + 0.10f * shade;
        }

        if (enableDepth && !isCave) {
            float depthFactor = std::clamp(1.0f + (currentY - 64.0f) / 450.0f, 0.70f, 1.15f);
            shade *= depthFactor;
        }

        return std::clamp(shade, 0.45f, 1.40f);
    }

    // 计算当前维度的天空光亮度比例 (0.22 = 深夜, 1.0 = 白昼)
    inline float ComputeSkyLightBrightness(int gameTimeTicks, int dimId, bool forceNighttime = false) noexcept {
        if (forceNighttime) return 0.22f;
        if (dimId == 1) return 0.0f;  // 下界无天空光
        if (dimId == 2) return 1.0f;  // 末地恒定环境天光
        if (gameTimeTicks < 0) return 1.0f;

        int dayTick = gameTimeTicks % 24000;
        if (dayTick < 0) dayTick += 24000;

        // Minecraft 昼夜曲线:
        // 0..11800: 白天全亮 (1.0)
        // 11800..13800: 黄昏日落平滑过渡 (1.0 -> 0.22)
        // 13800..22200: 夜晚月光 (0.22)
        // 22200..24000: 黎明日出平滑过渡 (0.22 -> 1.0)
        if (dayTick <= 11800) {
            return 1.0f;
        } else if (dayTick < 13800) {
            float t = (float)(dayTick - 11800) / 2000.0f;
            return 1.0f - t * (1.0f - 0.22f);
        } else if (dayTick <= 22200) {
            return 0.22f;
        } else {
            float t = (float)(dayTick - 22200) / 1800.0f;
            return 0.22f + t * (1.0f - 0.22f);
        }
    }

    // 获取量化后的当前光照阶梯值 (0..32)，供渲染线程检测昼夜渐变并刷新贴图
    inline int GetCurrentLightStep(int dimId) noexcept {
        if (!mapLighting) return -1;
        float sb = ComputeSkyLightBrightness(g_gameTimeTicks.load(), dimId, false);
        return (int)std::round(sb * 32.0f);
    }

    // 使用预计算天空亮度的快速光照应用方法（高频循环优化）
    inline void ApplyMapLightingPrecomputed(float& r, float& g, float& b, uint8_t packedLight, bool isCave, int dimId, bool enableLighting, float skyBright) noexcept {
        if (!enableLighting) return;

        int skyL   = (packedLight >> 4) & 0x0F;
        int blockL = packedLight & 0x0F;

        // 洞穴/下界若读取到旧缓存默认 0xF0，视为无天空光 + 基础环境光
        if ((isCave || dimId == 1) && packedLight == 0xF0) {
            skyL = 0;
            blockL = 7;
        }

        float skyContrib = (skyL * (1.0f / 15.0f)) * skyBright;
        float blockContrib = (blockL * (1.0f / 15.0f));
        float maxLight = std::max(skyContrib, blockContrib);

        float ambient = (isCave || dimId == 1) ? 0.36f : ((dimId == 2) ? 0.48f : 0.26f);
        float lightMult = ambient + (1.0f - ambient) * maxLight;

        // 火把/熔岩等方块光源主导时赋予微暖色调，夜间月光主导时赋予微冷夜色
        float torchDom = std::max(0.0f, blockContrib - skyContrib);
        float nightMoon = (!isCave && dimId == 0 && skyBright < 0.70f)
            ? std::max(0.0f, (0.70f - skyBright) * (1.0f - blockContrib))
            : 0.0f;

        float rMult = lightMult * (1.0f + 0.08f * torchDom - 0.05f * nightMoon);
        float gMult = lightMult * (1.0f + 0.02f * torchDom - 0.02f * nightMoon);
        float bMult = lightMult * (1.0f - 0.08f * torchDom + 0.08f * nightMoon);

        r *= rMult;
        g *= gMult;
        b *= bMult;
    }

    // 将方块光照与天空昼夜明暗动态应用到像素 RGB (0..255 浮点)
    inline void ApplyMapLighting(float& r, float& g, float& b, uint8_t packedLight, bool isCave, int dimId, bool enableLighting, bool forceNighttime = false) noexcept {
        if (!enableLighting && !forceNighttime) return;
        float skyBright = ComputeSkyLightBrightness(g_gameTimeTicks.load(), dimId, forceNighttime);
        ApplyMapLightingPrecomputed(r, g, b, packedLight, isCave, dimId, true, skyBright);
    }

    // ==================== 全局与分面板默认设置重置 ====================
    inline void ResetMiniMapSettingsToDefaults() {
        showMiniMap = true;
        isSquareMap = false;
        rotateMiniMap = false;
        miniMapScale = 1.0f;
        miniMapOffsetX = 0.0f;
        miniMapOffsetY = 0.0f;
        miniMapZoomRadius = 50.0f;
        minimapDoubleMinZoom = false;
        transparentMinimapBackground = false;
        showWaypointsOnMinimap = true;
        preferOverworldWaypoints = false;
        showRadar = true;
        showCompass = true;
        compassScale = 1.0f;
        playerArrowScale = 1.0f;
        infoShowBackground = true;
        infoShowCoords = true;
        infoShowNetherCoords = false;
        infoShowFacing = false;
        infoShowAngles = false;
        infoShowBiome = true;
        infoShowWeather = false;
        infoShowTime = false;
        timeFormat24h = true;
        infoShowLight = false;
        infoShowChunkCoords = false;
        radarHeightIndicators = true;
        entityDepth = true;
        alwaysShowNametags = true;
        entityHeightLimit = 0;
        radarShowPlayers = true;
        radarShowHostile = true;
        radarShowFriendly = true;
        radarShowItems = false;
        autoRemoveDeathpoints = true;
        enlargeMinimapToggle = false;
        showWaypointDistance = false;
        renderWaypointBackgrounds = true;
        declutterWaypointLabels = true;
        showSlimeChunks = true;
        pushboxBossHealth = 1;
        pushboxPotionEffects = 1;
        g_currentPushOffsetY = 0.0f;
        g_currentPushOffsetX = 0.0f;
        miniMapFrameStyle = 0;
        miniMapFrameColor[0] = 0.12f; miniMapFrameColor[1] = 0.12f; miniMapFrameColor[2] = 0.12f; miniMapFrameColor[3] = 1.0f;
        compassLocation = 1;
        northCompassColorMode = 0;
        northCompassCustomColor[0] = 1.0f; northCompassCustomColor[1] = 0.29f; northCompassCustomColor[2] = 0.29f; northCompassCustomColor[3] = 1.0f;
        keepEnlargedMinimapRotation = false;
        onlyShowCurrentWaypointSet = false;
        currentWaypointSet = "gui.chiyan_default";
        show3DWaypoints = true;
        show3DWaypointBeams = true;
        useNativeBeaconBeams = true;
        show3DWaypointLabels = true;
        show3DWaypointDistance = true;
        waypoint3DClampToScreen = true;

        waypoint3DMaxDistance = 5000;
        waypoint3DBeamOpacity = 0.65f;
        waypoint3DBeamRadius = 0.40f;
        waypoint3DScale = 1.0f;
        show3DDeathpoints = true;
        radarOverFrameMode = 0;
        radarHighlightTamed = true;
        radarDepthFadeStart = 1.5f;
        radarFilterInclude[0] = '\0';
        radarFilterExclude[0] = '\0';
        ::g_mapDataUpdated.store(true);
    }

    inline void ResetBigMapSettingsToDefaults() {
        bigMapZoom = 3.0f;
        bigMapHideUI = false;
        bigMapFollowPlayer = true;
        bigMapShowHoverBox = true;
        bigMapShowEntities = false;
        bigMapEntityScale = 1.0f;
        bigMapShowMarkers = true;
        preferOverworldWaypoints = false;
        bigMapShowDisabledWaypoints = true;
        bigMapWaypointScale = 1.0f;
        minZoomForLocalWaypoints = 0.0f;
        renderWaypointBackgrounds = true;
        declutterWaypointLabels = true;
        autoHideLabelsOnFarZoom = true;
        showChunkGrid = false;
        showFootsteps = true;
        showZoomButtons = true;
        playerArrowColor = 0;
        playerArrowScale = 1.0f;
        terrainSlopes = 2;
        terrainDepth = true;
        adjustHeightForShortBlocks = true;
        blockColorMode = 0;
        biomeColorsInVanilla = true;
        mapLighting = true;
        displayStainedGlass = true;
        loadFlowers = true;
        biomeBlending = true;
        openingAnimation = true;
        g_loadNewChunks = true;
        g_updateChunks = true;
        g_mapWritingDistance = 0;
        g_ignoreHeightmaps = false;
        g_multiLevelPyramid = true;
        g_maxLoadedRegions = 512;
        clearGPUCache.store(true);
        forceRescan.store(true);
        ::g_mapDataUpdated.store(true);
    }

    inline void ResetAllSettingsToDefaults() {
        ResetMiniMapSettingsToDefaults();
        ResetBigMapSettingsToDefaults();
        globalUIScale = GetOptimalUIScale();
        bigMapWaypointDrawerOpen = false;
        bigMapCloseDrawerAfterHop = false;
        g_hotkeys = HotkeyBindings::Defaults();
    }
}

// 【全球探索级】：匹配 16 区块能见度的究极扫描半径（513x513个方块）！
constexpr int MAP_DATA_RADIUS = 256; 
constexpr int MAP_DATA_SIZE   = 513;

struct RadarEntity {
    float x;
    float y;
    float z;
    int type; 
    std::string typeName;
    std::string nameTag;
    std::string uuid;
    bool isTamed = false; // [P4] 是否为已驯服生物
};

namespace RadarUtils {
    inline bool MatchesWildcardPattern(std::string_view pattern, std::string_view str) {
        size_t p = 0, s = 0;
        size_t starP = std::string_view::npos, starS = 0;
        while (s < str.size()) {
            if (p < pattern.size() && (std::tolower(static_cast<unsigned char>(pattern[p])) == std::tolower(static_cast<unsigned char>(str[s])) || pattern[p] == '?')) {
                ++p;
                ++s;
            } else if (p < pattern.size() && pattern[p] == '*') {
                starP = p++;
                starS = s;
            } else if (starP != std::string_view::npos) {
                p = starP + 1;
                s = ++starS;
            } else {
                return false;
            }
        }
        while (p < pattern.size() && pattern[p] == '*') ++p;
        return p == pattern.size();
    }

    inline bool MatchesCommaPatterns(const char* patternList, const std::string& typeName, const std::string& nameTag) {
        if (!patternList || patternList[0] == '\0') return false;

        std::string shortName = typeName;
        size_t colon = shortName.find(':');
        if (colon != std::string::npos) shortName = shortName.substr(colon + 1);

        const char* ptr = patternList;
        while (*ptr) {
            while (*ptr == ' ' || *ptr == ',' || *ptr == ';') ++ptr;
            if (!*ptr) break;
            const char* start = ptr;
            while (*ptr && *ptr != ',' && *ptr != ';') ++ptr;
            const char* end = ptr - 1;
            while (end > start && *end == ' ') --end;
            std::string token(start, end + 1);

            if (token.empty()) continue;
            if (MatchesWildcardPattern(token, typeName) ||
                MatchesWildcardPattern(token, shortName) ||
                (!nameTag.empty() && MatchesWildcardPattern(token, nameTag))) {
                return true;
            }
        }
        return false;
    }

    inline bool PassesRadarFilter(const std::string& typeName, const std::string& nameTag, const char* includeList, const char* excludeList) {
        if (excludeList && excludeList[0] != '\0') {
            if (MatchesCommaPatterns(excludeList, typeName, nameTag)) {
                return false;
            }
        }
        if (includeList && includeList[0] != '\0') {
            if (!MatchesCommaPatterns(includeList, typeName, nameTag)) {
                return false;
            }
        }
        return true;
    }
}

struct PlayerSkinHead {
    uint8_t pixels[8 * 8 * 4]{}; // 8x8 RGBA
    uint64_t fingerprint = 0;
    uint64_t revision = 0;
    bool valid = false;
};

extern std::unordered_map<std::string, PlayerSkinHead> g_playerSkinHeads; // keyed by UUID
inline std::mutex g_playerSkinMutex;
extern std::string g_localPlayerUuid;
inline std::atomic<bool> clearPlayerHeadTextures{false};

inline std::mutex g_radarMutex;
extern std::atomic<bool> g_radarUpdated;
inline std::atomic<uint64_t> g_radarGeneration{1};
inline std::atomic<bool> g_tabHeld{false};
extern std::vector<RadarEntity> g_radarEntities; 

// 前台缓冲（仅供显卡渲染读取，绝不闪烁）
extern mce::Color g_mapColors[MAP_DATA_SIZE][MAP_DATA_SIZE];
extern float g_mapHeights[MAP_DATA_SIZE][MAP_DATA_SIZE];
inline uint8_t g_mapLights[MAP_DATA_SIZE][MAP_DATA_SIZE];

// 后台缓冲（供 CPU 高速扫描写入）
inline mce::Color g_mapColorsBack[MAP_DATA_SIZE][MAP_DATA_SIZE];
inline float g_mapHeightsBack[MAP_DATA_SIZE][MAP_DATA_SIZE];
inline uint8_t g_mapLightsBack[MAP_DATA_SIZE][MAP_DATA_SIZE];

// [洞穴地图] 洞穴模式前台缓冲 (渲染线程读取)
extern mce::Color g_caveColors[MAP_DATA_SIZE][MAP_DATA_SIZE];
extern float g_caveHeights[MAP_DATA_SIZE][MAP_DATA_SIZE];

// 记录最后一次生成贴图时的绝对中心坐标
inline int g_lastRenderX = 0;
inline int g_lastRenderZ = 0;

// [小地图极速装载核心] 核心视野半径 (80 格已完整覆盖所有缩放下的小地图圆形视口)
constexpr int SCAN_INNER_RADIUS = 80;

// 小地图烘焙与推送到 GPU 的全局纹理字节缓冲及中心坐标
inline uint8_t g_textureData[MAP_DATA_SIZE * MAP_DATA_SIZE * 4];
inline float g_textureCenterX = 0.0f;
inline float g_textureCenterZ = 0.0f;
inline std::atomic<bool> g_textureReadyToUpload{false};

