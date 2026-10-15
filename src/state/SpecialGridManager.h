#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace SpecialGridManager {

    struct BreadcrumbPoint {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        int dimId = 0;
        uint64_t timestamp = 0;
    };

    // 1. Region Grid (区域文件网格 512x512)
    extern bool showRegionGrid;
    extern bool showRegionGridLabel;
    extern float regionGridColor[4];
    extern float regionGridThickness;

    // 2. Map Art Grid (地图画网格 128x128)
    extern bool showMapArtGrid;
    extern bool showMapArtGridLabel;
    extern float mapArtGridColor[4];
    extern float mapArtGridThickness;

    // 3. Spawn Chunks (出生点区块模拟覆盖层)
    extern bool showSpawnChunks;
    extern bool spawnChunksAutoDetect;
    extern int customSpawnX;
    extern int customSpawnZ;
    extern int spawnEntityRadiusChunks;
    extern int spawnLazyRadiusChunks;
    extern int spawnBorderRadiusChunks;
    extern bool showSpawnChunksOnMinimap;

    // 4. World Border (原版世界边界)
    extern bool showWorldBorder;
    extern float worldBorderCenterX;
    extern float worldBorderCenterZ;
    extern double worldBorderSize;
    extern float worldBorderColor[4];
    extern bool showWorldBorderWarning;

    // 5. Persistent Breadcrumbs (持久化历史足迹)
    extern bool persistentBreadcrumbsEnabled;
    extern int maxBreadcrumbPoints;
    extern float breadcrumbMinDistance;
    extern float breadcrumbColor[4];
    extern bool breadcrumbsShowLines;

    // 6. View Distance (视距覆盖层)
    extern bool showViewDistance;
    extern int viewDistanceChunks;
    extern float viewDistanceColor[4];
    extern float viewDistanceFillAlpha;
    extern float viewDistanceThickness;
    extern int viewDistanceShape; // 0 = 方形区块网格, 1 = 平滑径向圆形
    extern bool showViewDistanceOnMinimap;
    extern bool showViewDistanceLabel;

    // 7. Simulation Distance (模拟距离覆盖层)
    extern bool showSimulationDistance;
    extern int simulationDistanceChunks;
    extern float simulationDistanceColor[4];
    extern float simulationDistanceFillAlpha;
    extern float simulationDistanceThickness;
    extern int simulationDistanceShape; // 0 = 方形区块网格, 1 = 平滑径向圆形
    extern bool showSimulationDistanceOnMinimap;
    extern bool showSimulationDistanceLabel;

    extern std::vector<BreadcrumbPoint> g_breadcrumbs;
    extern std::mutex g_breadcrumbsMutex;
    extern std::atomic<bool> g_breadcrumbsDirty;

    void Init();
    void Shutdown();
    void SwitchWorld(const std::string& worldId);
    void SaveConfig(nlohmann::json& j);
    void LoadConfig(const nlohmann::json& j);

    // 足迹持久化与生命周期管理
    void SaveBreadcrumbs();
    void LoadBreadcrumbs();
    void ClearBreadcrumbs();
    void AddBreadcrumbPoint(float x, float y, float z, int dimId);

    // 渲染方法：小地图与全屏大地图
    void RenderMinimapOverlays(ImDrawList* drawList, ImVec2 center, float radius, float zoomScale, float angleRad, float playerX, float playerZ, bool isSquare, ImVec2 mapMin, ImVec2 mapMax, int dimId);
    void RenderBigMapOverlays(ImDrawList* drawList, ImVec2 screenCenter, float zoom, float viewX, float viewZ, float offsetX, float offsetZ, ImVec2 screenSize, int dimId);

    // 设置面板 UI (嵌入全屏大地图设置)
    void RenderSettingsCategory(int& matchCount, const char* searchFilter, float curScale);

} // namespace SpecialGridManager
