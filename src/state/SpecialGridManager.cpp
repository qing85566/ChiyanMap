#include "state/SpecialGridManager.h"
#include "state/LanguageManager.h"
#include "state/MapRenderState.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <chrono>

using json = nlohmann::json;

extern int g_playerBlockX;
extern int g_playerBlockZ;

namespace SpecialGridManager {

    // 1. Region Grid (区域文件网格 512x512)
    bool showRegionGrid = false;
    bool showRegionGridLabel = true;
    float regionGridColor[4] = { 0.15f, 0.75f, 1.0f, 0.65f };
    float regionGridThickness = 1.5f;

    // 2. Map Art Grid (地图画网格 128x128)
    bool showMapArtGrid = false;
    bool showMapArtGridLabel = true;
    float mapArtGridColor[4] = { 1.0f, 0.35f, 0.85f, 0.65f };
    float mapArtGridThickness = 1.2f;

    // 3. Spawn Chunks (出生点区块模拟覆盖层)
    bool showSpawnChunks = false;
    bool spawnChunksAutoDetect = true;
    int customSpawnX = 0;
    int customSpawnZ = 0;
    int spawnEntityRadiusChunks = 4; // 9x9 区块 (中心 +/- 4)
    int spawnLazyRadiusChunks = 2;   // 13x13 区块 (+2)
    int spawnBorderRadiusChunks = 1; // 15x15 区块 (+1)
    bool showSpawnChunksOnMinimap = true;

    // 4. World Border (原版世界边界)
    bool showWorldBorder = true;
    float worldBorderCenterX = 0.0f;
    float worldBorderCenterZ = 0.0f;
    double worldBorderSize = 60000000.0; // 默认原版 6000 万方块 (正负 3000 万)
    float worldBorderColor[4] = { 0.95f, 0.18f, 0.18f, 0.85f };
    bool showWorldBorderWarning = true;

    // 5. Persistent Breadcrumbs (持久化历史足迹)
    bool persistentBreadcrumbsEnabled = true;
    int maxBreadcrumbPoints = 2500;
    float breadcrumbMinDistance = 6.0f;
    float breadcrumbColor[4] = { 0.20f, 0.85f, 1.0f, 0.85f };
    bool breadcrumbsShowLines = true;

    // 6. View Distance (视距覆盖层)
    bool showViewDistance = false;
    int viewDistanceChunks = 16;
    float viewDistanceColor[4] = { 0.25f, 0.70f, 1.0f, 0.75f };
    float viewDistanceFillAlpha = 0.08f;
    float viewDistanceThickness = 1.6f;
    int viewDistanceShape = 0; // 0 = 方形区块网格, 1 = 平滑径向圆形
    bool showViewDistanceOnMinimap = true;
    bool showViewDistanceLabel = true;

    // 7. Simulation Distance (模拟距离覆盖层)
    bool showSimulationDistance = false;
    int simulationDistanceChunks = 8;
    float simulationDistanceColor[4] = { 1.0f, 0.78f, 0.15f, 0.85f };
    float simulationDistanceFillAlpha = 0.12f;
    float simulationDistanceThickness = 1.8f;
    int simulationDistanceShape = 0; // 0 = 方形区块网格, 1 = 平滑径向圆形
    bool showSimulationDistanceOnMinimap = true;
    bool showSimulationDistanceLabel = true;

    std::vector<BreadcrumbPoint> g_breadcrumbs;
    std::mutex g_breadcrumbsMutex;
    std::atomic<bool> g_breadcrumbsDirty{ false };

    static std::string s_currentWorldId = "default";
    static uint64_t s_lastSaveTimestamp = 0;

    static std::string GetBreadcrumbsFilePath() {
        std::string safeWorld = s_currentWorldId.empty() ? "default" : s_currentWorldId;
        for (char& c : safeWorld) {
            if (c == ':' || c == '/' || c == '\\' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
        return "mods/ChiyanMap/breadcrumbs/" + safeWorld + "_breadcrumbs.json";
    }

    void Init() {
        try {
            std::filesystem::create_directories("mods/ChiyanMap/breadcrumbs");
        } catch (...) {}
    }

    void Shutdown() {
        SaveBreadcrumbs();
    }

    void SwitchWorld(const std::string& worldId) {
        if (g_breadcrumbsDirty.load()) {
            SaveBreadcrumbs();
        }
        {
            std::lock_guard<std::mutex> lock(g_breadcrumbsMutex);
            g_breadcrumbs.clear();
        }
        s_currentWorldId = worldId.empty() ? "default" : worldId;
        LoadBreadcrumbs();
    }

    void SaveConfig(json& j) {
        j["showRegionGrid"] = showRegionGrid;
        j["showRegionGridLabel"] = showRegionGridLabel;
        j["regionGridColor"] = { regionGridColor[0], regionGridColor[1], regionGridColor[2], regionGridColor[3] };
        j["regionGridThickness"] = regionGridThickness;

        j["showMapArtGrid"] = showMapArtGrid;
        j["showMapArtGridLabel"] = showMapArtGridLabel;
        j["mapArtGridColor"] = { mapArtGridColor[0], mapArtGridColor[1], mapArtGridColor[2], mapArtGridColor[3] };
        j["mapArtGridThickness"] = mapArtGridThickness;

        j["showSpawnChunks"] = showSpawnChunks;
        j["spawnChunksAutoDetect"] = spawnChunksAutoDetect;
        j["customSpawnX"] = customSpawnX;
        j["customSpawnZ"] = customSpawnZ;
        j["spawnEntityRadiusChunks"] = spawnEntityRadiusChunks;
        j["spawnLazyRadiusChunks"] = spawnLazyRadiusChunks;
        j["spawnBorderRadiusChunks"] = spawnBorderRadiusChunks;
        j["showSpawnChunksOnMinimap"] = showSpawnChunksOnMinimap;

        j["showWorldBorder"] = showWorldBorder;
        j["worldBorderCenterX"] = worldBorderCenterX;
        j["worldBorderCenterZ"] = worldBorderCenterZ;
        j["worldBorderSize"] = worldBorderSize;
        j["worldBorderColor"] = { worldBorderColor[0], worldBorderColor[1], worldBorderColor[2], worldBorderColor[3] };
        j["showWorldBorderWarning"] = showWorldBorderWarning;

        j["persistentBreadcrumbsEnabled"] = persistentBreadcrumbsEnabled;
        j["maxBreadcrumbPoints"] = maxBreadcrumbPoints;
        j["breadcrumbMinDistance"] = breadcrumbMinDistance;
        j["breadcrumbColor"] = { breadcrumbColor[0], breadcrumbColor[1], breadcrumbColor[2], breadcrumbColor[3] };
        j["breadcrumbsShowLines"] = breadcrumbsShowLines;

        j["showViewDistance"] = showViewDistance;
        j["viewDistanceChunks"] = viewDistanceChunks;
        j["viewDistanceColor"] = { viewDistanceColor[0], viewDistanceColor[1], viewDistanceColor[2], viewDistanceColor[3] };
        j["viewDistanceFillAlpha"] = viewDistanceFillAlpha;
        j["viewDistanceThickness"] = viewDistanceThickness;
        j["viewDistanceShape"] = viewDistanceShape;
        j["showViewDistanceOnMinimap"] = showViewDistanceOnMinimap;
        j["showViewDistanceLabel"] = showViewDistanceLabel;

        j["showSimulationDistance"] = showSimulationDistance;
        j["simulationDistanceChunks"] = simulationDistanceChunks;
        j["simulationDistanceColor"] = { simulationDistanceColor[0], simulationDistanceColor[1], simulationDistanceColor[2], simulationDistanceColor[3] };
        j["simulationDistanceFillAlpha"] = simulationDistanceFillAlpha;
        j["simulationDistanceThickness"] = simulationDistanceThickness;
        j["simulationDistanceShape"] = simulationDistanceShape;
        j["showSimulationDistanceOnMinimap"] = showSimulationDistanceOnMinimap;
        j["showSimulationDistanceLabel"] = showSimulationDistanceLabel;
    }

    void LoadConfig(const json& j) {
        showRegionGrid = j.value("showRegionGrid", false);
        showRegionGridLabel = j.value("showRegionGridLabel", true);
        if (j.contains("regionGridColor") && j["regionGridColor"].is_array() && j["regionGridColor"].size() == 4) {
            for (int i = 0; i < 4; ++i) regionGridColor[i] = j["regionGridColor"][i];
        }
        regionGridThickness = j.value("regionGridThickness", 1.5f);

        showMapArtGrid = j.value("showMapArtGrid", false);
        showMapArtGridLabel = j.value("showMapArtGridLabel", true);
        if (j.contains("mapArtGridColor") && j["mapArtGridColor"].is_array() && j["mapArtGridColor"].size() == 4) {
            for (int i = 0; i < 4; ++i) mapArtGridColor[i] = j["mapArtGridColor"][i];
        }
        mapArtGridThickness = j.value("mapArtGridThickness", 1.2f);

        showSpawnChunks = j.value("showSpawnChunks", false);
        spawnChunksAutoDetect = j.value("spawnChunksAutoDetect", true);
        customSpawnX = j.value("customSpawnX", 0);
        customSpawnZ = j.value("customSpawnZ", 0);
        spawnEntityRadiusChunks = std::clamp(j.value("spawnEntityRadiusChunks", 4), 1, 16);
        spawnLazyRadiusChunks = std::clamp(j.value("spawnLazyRadiusChunks", 2), 0, 8);
        spawnBorderRadiusChunks = std::clamp(j.value("spawnBorderRadiusChunks", 1), 0, 8);
        showSpawnChunksOnMinimap = j.value("showSpawnChunksOnMinimap", true);

        showWorldBorder = j.value("showWorldBorder", true);
        worldBorderCenterX = j.value("worldBorderCenterX", 0.0f);
        worldBorderCenterZ = j.value("worldBorderCenterZ", 0.0f);
        worldBorderSize = j.value("worldBorderSize", 60000000.0);
        if (j.contains("worldBorderColor") && j["worldBorderColor"].is_array() && j["worldBorderColor"].size() == 4) {
            for (int i = 0; i < 4; ++i) worldBorderColor[i] = j["worldBorderColor"][i];
        }
        showWorldBorderWarning = j.value("showWorldBorderWarning", true);

        persistentBreadcrumbsEnabled = j.value("persistentBreadcrumbsEnabled", true);
        maxBreadcrumbPoints = std::clamp(j.value("maxBreadcrumbPoints", 2500), 100, 10000);
        breadcrumbMinDistance = std::clamp(j.value("breadcrumbMinDistance", 6.0f), 2.0f, 32.0f);
        if (j.contains("breadcrumbColor") && j["breadcrumbColor"].is_array() && j["breadcrumbColor"].size() == 4) {
            for (int i = 0; i < 4; ++i) breadcrumbColor[i] = j["breadcrumbColor"][i];
        }
        breadcrumbsShowLines = j.value("breadcrumbsShowLines", true);

        showViewDistance = j.value("showViewDistance", false);
        viewDistanceChunks = std::clamp(j.value("viewDistanceChunks", 16), 2, 64);
        if (j.contains("viewDistanceColor") && j["viewDistanceColor"].is_array() && j["viewDistanceColor"].size() == 4) {
            for (int i = 0; i < 4; ++i) viewDistanceColor[i] = j["viewDistanceColor"][i];
        }
        viewDistanceFillAlpha = std::clamp(j.value("viewDistanceFillAlpha", 0.08f), 0.0f, 0.50f);
        viewDistanceThickness = std::clamp(j.value("viewDistanceThickness", 1.6f), 0.5f, 5.0f);
        viewDistanceShape = std::clamp(j.value("viewDistanceShape", 0), 0, 1);
        showViewDistanceOnMinimap = j.value("showViewDistanceOnMinimap", true);
        showViewDistanceLabel = j.value("showViewDistanceLabel", true);

        showSimulationDistance = j.value("showSimulationDistance", false);
        simulationDistanceChunks = std::clamp(j.value("simulationDistanceChunks", 8), 2, 32);
        if (j.contains("simulationDistanceColor") && j["simulationDistanceColor"].is_array() && j["simulationDistanceColor"].size() == 4) {
            for (int i = 0; i < 4; ++i) simulationDistanceColor[i] = j["simulationDistanceColor"][i];
        }
        simulationDistanceFillAlpha = std::clamp(j.value("simulationDistanceFillAlpha", 0.12f), 0.0f, 0.50f);
        simulationDistanceThickness = std::clamp(j.value("simulationDistanceThickness", 1.8f), 0.5f, 5.0f);
        simulationDistanceShape = std::clamp(j.value("simulationDistanceShape", 0), 0, 1);
        showSimulationDistanceOnMinimap = j.value("showSimulationDistanceOnMinimap", true);
        showSimulationDistanceLabel = j.value("showSimulationDistanceLabel", true);
    }

    void SaveBreadcrumbs() {
        if (!g_breadcrumbsDirty.load()) return;
        try {
            std::filesystem::create_directories("mods/ChiyanMap/breadcrumbs");
            std::string path = GetBreadcrumbsFilePath();
            json arr = json::array();
            {
                std::lock_guard<std::mutex> lock(g_breadcrumbsMutex);
                for (const auto& pt : g_breadcrumbs) {
                    arr.push_back({
                        {"x", std::round(pt.x * 10.0f) / 10.0f},
                        {"y", std::round(pt.y * 10.0f) / 10.0f},
                        {"z", std::round(pt.z * 10.0f) / 10.0f},
                        {"d", pt.dimId},
                        {"t", pt.timestamp}
                    });
                }
            }
            std::ofstream ofs(path);
            if (ofs.is_open()) {
                ofs << arr.dump();
                ofs.close();
                g_breadcrumbsDirty.store(false);
            }
        } catch (...) {}
    }

    void LoadBreadcrumbs() {
        std::string path = GetBreadcrumbsFilePath();
        if (!std::filesystem::exists(path)) return;
        try {
            std::ifstream ifs(path);
            if (!ifs.is_open()) return;
            json arr;
            ifs >> arr;
            ifs.close();
            if (arr.is_array()) {
                std::lock_guard<std::mutex> lock(g_breadcrumbsMutex);
                g_breadcrumbs.clear();
                g_breadcrumbs.reserve(arr.size());
                for (const auto& item : arr) {
                    BreadcrumbPoint pt;
                    pt.x = item.value("x", 0.0f);
                    pt.y = item.value("y", 64.0f);
                    pt.z = item.value("z", 0.0f);
                    pt.dimId = item.value("d", 0);
                    pt.timestamp = item.value("t", (uint64_t)0);
                    g_breadcrumbs.push_back(pt);
                }
                g_breadcrumbsDirty.store(false);
            }
        } catch (...) {}
    }

    void ClearBreadcrumbs() {
        {
            std::lock_guard<std::mutex> lock(g_breadcrumbsMutex);
            g_breadcrumbs.clear();
        }
        g_breadcrumbsDirty.store(true);
        SaveBreadcrumbs();
    }

    void AddBreadcrumbPoint(float x, float y, float z, int dimId) {
        if (!persistentBreadcrumbsEnabled) return;
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        {
            std::lock_guard<std::mutex> lock(g_breadcrumbsMutex);
            if (!g_breadcrumbs.empty()) {
                const auto& last = g_breadcrumbs.back();
                if (last.dimId == dimId) {
                    float dx = x - last.x;
                    float dy = y - last.y;
                    float dz = z - last.z;
                    float distSq = dx * dx + dy * dy + dz * dz;
                    if (distSq < breadcrumbMinDistance * breadcrumbMinDistance) {
                        return;
                    }
                }
            }
            g_breadcrumbs.push_back({ x, y, z, dimId, (uint64_t)now });
            if ((int)g_breadcrumbs.size() > maxBreadcrumbPoints) {
                size_t removeCount = g_breadcrumbs.size() - maxBreadcrumbPoints;
                g_breadcrumbs.erase(g_breadcrumbs.begin(), g_breadcrumbs.begin() + removeCount);
            }
        }
        g_breadcrumbsDirty.store(true);

        // 间隔 30 秒自动刷新存盘
        if (now - s_lastSaveTimestamp >= 30) {
            s_lastSaveTimestamp = now;
            SaveBreadcrumbs();
        }
    }

    // 辅助函数：绘制小地图裁剪线段 (支持方形与圆形剪裁)
    static void DrawClippedLine(ImDrawList* drawList, ImVec2 p0, ImVec2 p1, ImU32 col, float thickness, bool isSquare, ImVec2 mapMin, ImVec2 mapMax, ImVec2 center, float radius) {
        if (isSquare) {
            drawList->AddLine(p0, p1, col, thickness);
            return;
        }
        // 与圆求交
        float dx = p1.x - p0.x;
        float dy = p1.y - p0.y;
        float fx = p0.x - center.x;
        float fy = p0.y - center.y;
        float a = dx * dx + dy * dy;
        if (a < 0.0001f) return;
        float b = 2.0f * (fx * dx + fy * dy);
        float c = fx * fx + fy * fy - radius * radius;
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return;
        float sq = std::sqrt(disc);
        float t0 = std::clamp((-b - sq) / (2.0f * a), 0.0f, 1.0f);
        float t1 = std::clamp((-b + sq) / (2.0f * a), 0.0f, 1.0f);
        if (t0 < t1) {
            drawList->AddLine(ImVec2(p0.x + dx * t0, p0.y + dy * t0), ImVec2(p0.x + dx * t1, p0.y + dy * t1), col, thickness);
        }
    }

    void RenderMinimapOverlays(ImDrawList* drawList, ImVec2 center, float radius, float zoomScale, float angleRad, float playerX, float playerZ, bool isSquare, ImVec2 mapMin, ImVec2 mapMax, int dimId) {
        float c_rot = std::cos(angleRad);
        float s_rot = std::sin(angleRad);

        auto worldToMinimap = [&](float wx, float wz) -> ImVec2 {
            float dx = (wx - playerX) * zoomScale;
            float dz = (wz - playerZ) * zoomScale;
            float rx = dx * c_rot - dz * s_rot;
            float ry = dx * s_rot + dz * c_rot;
            return ImVec2(center.x + rx, center.y + ry);
        };

        if (isSquare) {
            drawList->PushClipRect(mapMin, mapMax, true);
        }

        float coverPx = radius * (isSquare ? 1.42f : 1.0f);
        float coverR = (zoomScale > 0.0001f) ? (coverPx / zoomScale) : radius;

        // 1. Region Grid (512x512)
        if (showRegionGrid) {
            ImU32 regCol = ImGui::ColorConvertFloat4ToU32(ImVec4(regionGridColor[0], regionGridColor[1], regionGridColor[2], regionGridColor[3]));
            int firstRx = (int)std::floor((playerX - coverR) / 512.0f) * 512;
            int lastRx  = (int)std::ceil((playerX + coverR) / 512.0f) * 512;
            for (int rx = firstRx; rx <= lastRx; rx += 512) {
                float lx = ((float)rx - playerX) * zoomScale;
                float r0x = lx * c_rot - (-coverPx) * s_rot;
                float r0y = lx * s_rot + (-coverPx) * c_rot;
                float r1x = lx * c_rot - coverPx * s_rot;
                float r1y = lx * s_rot + coverPx * c_rot;
                DrawClippedLine(drawList, ImVec2(center.x + r0x, center.y + r0y), ImVec2(center.x + r1x, center.y + r1y), regCol, regionGridThickness, isSquare, mapMin, mapMax, center, radius);
            }
            int firstRz = (int)std::floor((playerZ - coverR) / 512.0f) * 512;
            int lastRz  = (int)std::ceil((playerZ + coverR) / 512.0f) * 512;
            for (int rz = firstRz; rz <= lastRz; rz += 512) {
                float lz = ((float)rz - playerZ) * zoomScale;
                float r0x = (-coverPx) * c_rot - lz * s_rot;
                float r0y = (-coverPx) * s_rot + lz * c_rot;
                float r1x = coverPx * c_rot - lz * s_rot;
                float r1y = coverPx * s_rot + lz * c_rot;
                DrawClippedLine(drawList, ImVec2(center.x + r0x, center.y + r0y), ImVec2(center.x + r1x, center.y + r1y), regCol, regionGridThickness, isSquare, mapMin, mapMax, center, radius);
            }
        }

        // 2. Map Art Grid (128x128)
        if (showMapArtGrid) {
            ImU32 maCol = ImGui::ColorConvertFloat4ToU32(ImVec4(mapArtGridColor[0], mapArtGridColor[1], mapArtGridColor[2], mapArtGridColor[3]));
            int firstMx = (int)std::floor((playerX - coverR + 64.0f) / 128.0f) * 128 - 64;
            int lastMx  = (int)std::ceil((playerX + coverR + 64.0f) / 128.0f) * 128 - 64;
            for (int mx = firstMx; mx <= lastMx; mx += 128) {
                float lx = ((float)mx - playerX) * zoomScale;
                float r0x = lx * c_rot - (-coverPx) * s_rot;
                float r0y = lx * s_rot + (-coverPx) * c_rot;
                float r1x = lx * c_rot - coverPx * s_rot;
                float r1y = lx * s_rot + coverPx * c_rot;
                DrawClippedLine(drawList, ImVec2(center.x + r0x, center.y + r0y), ImVec2(center.x + r1x, center.y + r1y), maCol, mapArtGridThickness, isSquare, mapMin, mapMax, center, radius);
            }
            int firstMz = (int)std::floor((playerZ - coverR + 64.0f) / 128.0f) * 128 - 64;
            int lastMz  = (int)std::ceil((playerZ + coverR + 64.0f) / 128.0f) * 128 - 64;
            for (int mz = firstMz; mz <= lastMz; mz += 128) {
                float lz = ((float)mz - playerZ) * zoomScale;
                float r0x = (-coverPx) * c_rot - lz * s_rot;
                float r0y = (-coverPx) * s_rot + lz * c_rot;
                float r1x = coverPx * c_rot - lz * s_rot;
                float r1y = coverPx * s_rot + lz * c_rot;
                DrawClippedLine(drawList, ImVec2(center.x + r0x, center.y + r0y), ImVec2(center.x + r1x, center.y + r1y), maCol, mapArtGridThickness, isSquare, mapMin, mapMax, center, radius);
            }
        }

        // 3. Spawn Chunks (主世界)
        if (showSpawnChunks && showSpawnChunksOnMinimap && dimId == 0) {
            int spX = (spawnChunksAutoDetect && MapRenderState::hasWorldSpawn) ? MapRenderState::worldSpawnX : customSpawnX;
            int spZ = (spawnChunksAutoDetect && MapRenderState::hasWorldSpawn) ? MapRenderState::worldSpawnZ : customSpawnZ;
            int scX = (int)std::floor((float)spX / 16.0f);
            int scZ = (int)std::floor((float)spZ / 16.0f);

            int r1 = spawnEntityRadiusChunks;
            int r2 = r1 + spawnLazyRadiusChunks;
            int r3 = r2 + spawnBorderRadiusChunks;

            auto drawChunkRing = [&](int rad, ImU32 col, float thick) {
                float bx0 = (float)(scX - rad) * 16.0f;
                float bx1 = (float)(scX + rad + 1) * 16.0f;
                float bz0 = (float)(scZ - rad) * 16.0f;
                float bz1 = (float)(scZ + rad + 1) * 16.0f;

                ImVec2 p0 = worldToMinimap(bx0, bz0);
                ImVec2 p1 = worldToMinimap(bx1, bz0);
                ImVec2 p2 = worldToMinimap(bx1, bz1);
                ImVec2 p3 = worldToMinimap(bx0, bz1);

                DrawClippedLine(drawList, p0, p1, col, thick, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p1, p2, col, thick, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p2, p3, col, thick, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p3, p0, col, thick, isSquare, mapMin, mapMax, center, radius);
            };

            drawChunkRing(r3, IM_COL32(120, 180, 255, 170), 1.2f);
            drawChunkRing(r2, IM_COL32(255, 215, 50, 190), 1.6f);
            drawChunkRing(r1, IM_COL32(50, 240, 100, 220), 2.0f);
        }

        // 6. 视距覆盖层
        if (showViewDistance && showViewDistanceOnMinimap && dimId == MapRenderState::currentDimensionId) {
            int pcX = (int)std::floor(playerX / 16.0f);
            int pcZ = (int)std::floor(playerZ / 16.0f);
            int rad = viewDistanceChunks;

            ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(viewDistanceColor[0], viewDistanceColor[1], viewDistanceColor[2], viewDistanceColor[3]));
            ImU32 fillCol   = ImGui::ColorConvertFloat4ToU32(ImVec4(viewDistanceColor[0], viewDistanceColor[1], viewDistanceColor[2], viewDistanceFillAlpha));

            if (viewDistanceShape == 0) {
                // 方形区块网格
                float bx0 = (float)(pcX - rad) * 16.0f;
                float bx1 = (float)(pcX + rad + 1) * 16.0f;
                float bz0 = (float)(pcZ - rad) * 16.0f;
                float bz1 = (float)(pcZ + rad + 1) * 16.0f;

                ImVec2 p0 = worldToMinimap(bx0, bz0);
                ImVec2 p1 = worldToMinimap(bx1, bz0);
                ImVec2 p2 = worldToMinimap(bx1, bz1);
                ImVec2 p3 = worldToMinimap(bx0, bz1);

                if (viewDistanceFillAlpha > 0.001f) {
                    if (isSquare) {
                        drawList->AddQuadFilled(p0, p1, p2, p3, fillCol);
                    } else {
                        float rSq = radius * radius;
                        float minDist = std::min({ (playerX - bx0), (bx1 - playerX), (playerZ - bz0), (bz1 - playerZ) }) * zoomScale;
                        if (minDist >= radius) {
                            drawList->AddCircleFilled(center, radius, fillCol, 64);
                        } else {
                            float d0 = (p0.x - center.x) * (p0.x - center.x) + (p0.y - center.y) * (p0.y - center.y);
                            float d1 = (p1.x - center.x) * (p1.x - center.x) + (p1.y - center.y) * (p1.y - center.y);
                            float d2 = (p2.x - center.x) * (p2.x - center.x) + (p2.y - center.y) * (p2.y - center.y);
                            float d3 = (p3.x - center.x) * (p3.x - center.x) + (p3.y - center.y) * (p3.y - center.y);
                            if (d0 <= rSq && d1 <= rSq && d2 <= rSq && d3 <= rSq) {
                                drawList->AddQuadFilled(p0, p1, p2, p3, fillCol);
                            } else {
                                int cMinX = std::max(pcX - rad, (int)std::floor((playerX - coverR) / 16.0f));
                                int cMaxX = std::min(pcX + rad, (int)std::ceil((playerX + coverR) / 16.0f));
                                int cMinZ = std::max(pcZ - rad, (int)std::floor((playerZ - coverR) / 16.0f));
                                int cMaxZ = std::min(pcZ + rad, (int)std::ceil((playerZ + coverR) / 16.0f));
                                for (int cz = cMinZ; cz <= cMaxZ; ++cz) {
                                    for (int cx = cMinX; cx <= cMaxX; ++cx) {
                                        float cbx0 = (float)cx * 16.0f;
                                        float cbx1 = cbx0 + 16.0f;
                                        float cbz0 = (float)cz * 16.0f;
                                        float cbz1 = cbz0 + 16.0f;
                                        ImVec2 cp0 = worldToMinimap(cbx0, cbz0);
                                        ImVec2 cp1 = worldToMinimap(cbx1, cbz0);
                                        ImVec2 cp2 = worldToMinimap(cbx1, cbz1);
                                        ImVec2 cp3 = worldToMinimap(cbx0, cbz1);
                                        float cd0 = (cp0.x - center.x) * (cp0.x - center.x) + (cp0.y - center.y) * (cp0.y - center.y);
                                        float cd1 = (cp1.x - center.x) * (cp1.x - center.x) + (cp1.y - center.y) * (cp1.y - center.y);
                                        float cd2 = (cp2.x - center.x) * (cp2.x - center.x) + (cp2.y - center.y) * (cp2.y - center.y);
                                        float cd3 = (cp3.x - center.x) * (cp3.x - center.x) + (cp3.y - center.y) * (cp3.y - center.y);
                                        if (cd0 <= rSq && cd1 <= rSq && cd2 <= rSq && cd3 <= rSq) {
                                            drawList->AddQuadFilled(cp0, cp1, cp2, cp3, fillCol);
                                        } else {
                                            float midX = (cbx0 + cbx1) * 0.5f;
                                            float midZ = (cbz0 + cbz1) * 0.5f;
                                            ImVec2 cm = worldToMinimap(midX, midZ);
                                            float cmd = (cm.x - center.x) * (cm.x - center.x) + (cm.y - center.y) * (cm.y - center.y);
                                            if (cmd <= rSq) {
                                                drawList->AddQuadFilled(cp0, cp1, cp2, cp3, fillCol);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                DrawClippedLine(drawList, p0, p1, borderCol, viewDistanceThickness, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p1, p2, borderCol, viewDistanceThickness, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p2, p3, borderCol, viewDistanceThickness, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p3, p0, borderCol, viewDistanceThickness, isSquare, mapMin, mapMax, center, radius);
            } else {
                // 平滑径向圆形
                float radPx = (float)rad * 16.0f * zoomScale;
                if (isSquare) {
                    if (viewDistanceFillAlpha > 0.001f) {
                        drawList->AddCircleFilled(center, radPx, fillCol, 64);
                    }
                    drawList->AddCircle(center, radPx, borderCol, 64, viewDistanceThickness);
                } else {
                    if (viewDistanceFillAlpha > 0.001f) {
                        float fillR = std::min(radPx, radius);
                        drawList->AddCircleFilled(center, fillR, fillCol, 64);
                    }
                    if (radPx <= radius) {
                        drawList->AddCircle(center, radPx, borderCol, 64, viewDistanceThickness);
                    }
                }
            }
        }

        // 7. 模拟距离覆盖层
        if (showSimulationDistance && showSimulationDistanceOnMinimap && dimId == MapRenderState::currentDimensionId) {
            int pcX = (int)std::floor(playerX / 16.0f);
            int pcZ = (int)std::floor(playerZ / 16.0f);
            int rad = simulationDistanceChunks;

            ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(simulationDistanceColor[0], simulationDistanceColor[1], simulationDistanceColor[2], simulationDistanceColor[3]));
            ImU32 fillCol   = ImGui::ColorConvertFloat4ToU32(ImVec4(simulationDistanceColor[0], simulationDistanceColor[1], simulationDistanceColor[2], simulationDistanceFillAlpha));

            if (simulationDistanceShape == 0) {
                // 方形区块网格
                float bx0 = (float)(pcX - rad) * 16.0f;
                float bx1 = (float)(pcX + rad + 1) * 16.0f;
                float bz0 = (float)(pcZ - rad) * 16.0f;
                float bz1 = (float)(pcZ + rad + 1) * 16.0f;

                ImVec2 p0 = worldToMinimap(bx0, bz0);
                ImVec2 p1 = worldToMinimap(bx1, bz0);
                ImVec2 p2 = worldToMinimap(bx1, bz1);
                ImVec2 p3 = worldToMinimap(bx0, bz1);

                if (simulationDistanceFillAlpha > 0.001f) {
                    if (isSquare) {
                        drawList->AddQuadFilled(p0, p1, p2, p3, fillCol);
                    } else {
                        float rSq = radius * radius;
                        float minDist = std::min({ (playerX - bx0), (bx1 - playerX), (playerZ - bz0), (bz1 - playerZ) }) * zoomScale;
                        if (minDist >= radius) {
                            drawList->AddCircleFilled(center, radius, fillCol, 64);
                        } else {
                            float d0 = (p0.x - center.x) * (p0.x - center.x) + (p0.y - center.y) * (p0.y - center.y);
                            float d1 = (p1.x - center.x) * (p1.x - center.x) + (p1.y - center.y) * (p1.y - center.y);
                            float d2 = (p2.x - center.x) * (p2.x - center.x) + (p2.y - center.y) * (p2.y - center.y);
                            float d3 = (p3.x - center.x) * (p3.x - center.x) + (p3.y - center.y) * (p3.y - center.y);
                            if (d0 <= rSq && d1 <= rSq && d2 <= rSq && d3 <= rSq) {
                                drawList->AddQuadFilled(p0, p1, p2, p3, fillCol);
                            } else {
                                int cMinX = std::max(pcX - rad, (int)std::floor((playerX - coverR) / 16.0f));
                                int cMaxX = std::min(pcX + rad, (int)std::ceil((playerX + coverR) / 16.0f));
                                int cMinZ = std::max(pcZ - rad, (int)std::floor((playerZ - coverR) / 16.0f));
                                int cMaxZ = std::min(pcZ + rad, (int)std::ceil((playerZ + coverR) / 16.0f));
                                for (int cz = cMinZ; cz <= cMaxZ; ++cz) {
                                    for (int cx = cMinX; cx <= cMaxX; ++cx) {
                                        float cbx0 = (float)cx * 16.0f;
                                        float cbx1 = cbx0 + 16.0f;
                                        float cbz0 = (float)cz * 16.0f;
                                        float cbz1 = cbz0 + 16.0f;
                                        ImVec2 cp0 = worldToMinimap(cbx0, cbz0);
                                        ImVec2 cp1 = worldToMinimap(cbx1, cbz0);
                                        ImVec2 cp2 = worldToMinimap(cbx1, cbz1);
                                        ImVec2 cp3 = worldToMinimap(cbx0, cbz1);
                                        float cd0 = (cp0.x - center.x) * (cp0.x - center.x) + (cp0.y - center.y) * (cp0.y - center.y);
                                        float cd1 = (cp1.x - center.x) * (cp1.x - center.x) + (cp1.y - center.y) * (cp1.y - center.y);
                                        float cd2 = (cp2.x - center.x) * (cp2.x - center.x) + (cp2.y - center.y) * (cp2.y - center.y);
                                        float cd3 = (cp3.x - center.x) * (cp3.x - center.x) + (cp3.y - center.y) * (cp3.y - center.y);
                                        if (cd0 <= rSq && cd1 <= rSq && cd2 <= rSq && cd3 <= rSq) {
                                            drawList->AddQuadFilled(cp0, cp1, cp2, cp3, fillCol);
                                        } else {
                                            float midX = (cbx0 + cbx1) * 0.5f;
                                            float midZ = (cbz0 + cbz1) * 0.5f;
                                            ImVec2 cm = worldToMinimap(midX, midZ);
                                            float cmd = (cm.x - center.x) * (cm.x - center.x) + (cm.y - center.y) * (cm.y - center.y);
                                            if (cmd <= rSq) {
                                                drawList->AddQuadFilled(cp0, cp1, cp2, cp3, fillCol);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                DrawClippedLine(drawList, p0, p1, borderCol, simulationDistanceThickness, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p1, p2, borderCol, simulationDistanceThickness, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p2, p3, borderCol, simulationDistanceThickness, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p3, p0, borderCol, simulationDistanceThickness, isSquare, mapMin, mapMax, center, radius);
            } else {
                // 平滑径向圆形
                float radPx = (float)rad * 16.0f * zoomScale;
                if (isSquare) {
                    if (simulationDistanceFillAlpha > 0.001f) {
                        drawList->AddCircleFilled(center, radPx, fillCol, 64);
                    }
                    drawList->AddCircle(center, radPx, borderCol, 64, simulationDistanceThickness);
                } else {
                    if (simulationDistanceFillAlpha > 0.001f) {
                        float fillR = std::min(radPx, radius);
                        drawList->AddCircleFilled(center, fillR, fillCol, 64);
                    }
                    if (radPx <= radius) {
                        drawList->AddCircle(center, radPx, borderCol, 64, simulationDistanceThickness);
                    }
                }
            }
        }

        // 4. World Border
        if (showWorldBorder) {
            float halfSz = (float)(worldBorderSize * 0.5);
            float wbMinX = worldBorderCenterX - halfSz;
            float wbMaxX = worldBorderCenterX + halfSz;
            float wbMinZ = worldBorderCenterZ - halfSz;
            float wbMaxZ = worldBorderCenterZ + halfSz;

            ImU32 wbCol = ImGui::ColorConvertFloat4ToU32(ImVec4(worldBorderColor[0], worldBorderColor[1], worldBorderColor[2], worldBorderColor[3]));

            if (std::abs(playerX - wbMinX) < coverR || std::abs(playerX - wbMaxX) < coverR ||
                std::abs(playerZ - wbMinZ) < coverR || std::abs(playerZ - wbMaxZ) < coverR) {
                ImVec2 p0 = worldToMinimap(wbMinX, wbMinZ);
                ImVec2 p1 = worldToMinimap(wbMaxX, wbMinZ);
                ImVec2 p2 = worldToMinimap(wbMaxX, wbMaxZ);
                ImVec2 p3 = worldToMinimap(wbMinX, wbMaxZ);

                DrawClippedLine(drawList, p0, p1, wbCol, 2.5f, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p1, p2, wbCol, 2.5f, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p2, p3, wbCol, 2.5f, isSquare, mapMin, mapMax, center, radius);
                DrawClippedLine(drawList, p3, p0, wbCol, 2.5f, isSquare, mapMin, mapMax, center, radius);
            }
        }

        // 5. Persistent Breadcrumbs
        if (persistentBreadcrumbsEnabled && MapRenderState::showFootsteps) {
            std::lock_guard<std::mutex> lock(g_breadcrumbsMutex);
            size_t n = g_breadcrumbs.size();
            if (n >= 2) {
                ImU32 baseCol = ImGui::ColorConvertFloat4ToU32(ImVec4(breadcrumbColor[0], breadcrumbColor[1], breadcrumbColor[2], breadcrumbColor[3]));
                for (size_t i = 1; i < n; ++i) {
                    const auto& pA = g_breadcrumbs[i - 1];
                    const auto& pB = g_breadcrumbs[i];
                    if (pA.dimId != dimId || pB.dimId != dimId) continue;

                    float distSq = (pB.x - pA.x) * (pB.x - pA.x) + (pB.z - pA.z) * (pB.z - pA.z);
                    if (distSq > 64.0f * 64.0f) continue; // 传送或大跨度跳变不连线

                    float progress = (float)i / (float)n;
                    int alpha = (int)(40.0f + 215.0f * progress);
                    ImU32 col = (baseCol & 0x00FFFFFF) | ((uint32_t)alpha << 24);

                    ImVec2 sA = worldToMinimap(pA.x, pA.z);
                    ImVec2 sB = worldToMinimap(pB.x, pB.z);

                    if (breadcrumbsShowLines) {
                        DrawClippedLine(drawList, sA, sB, col, 1.8f, isSquare, mapMin, mapMax, center, radius);
                    }
                }
            }
        }

        if (isSquare) {
            drawList->PopClipRect();
        }
    }

    void RenderBigMapOverlays(ImDrawList* drawList, ImVec2 screenCenter, float zoom, float viewX, float viewZ, float offsetX, float offsetZ, ImVec2 screenSize, int dimId) {
        ImFont* font = ImGui::GetFont();
        float baseFontSize = ImGui::GetFontSize();

        float wX0 = viewX - (screenCenter.x + offsetX) / zoom;
        float wX1 = viewX + (screenSize.x - screenCenter.x - offsetX) / zoom;
        float wZ0 = viewZ - (screenCenter.y + offsetZ) / zoom;
        float wZ1 = viewZ + (screenSize.y - screenCenter.y - offsetZ) / zoom;

        // 1. Region Grid (512x512)
        if (showRegionGrid && (zoom * 512.0f >= 6.0f)) {
            ImU32 regCol = ImGui::ColorConvertFloat4ToU32(ImVec4(regionGridColor[0], regionGridColor[1], regionGridColor[2], regionGridColor[3]));
            int minRx = (int)std::floor(wX0 / 512.0f);
            int maxRx = (int)std::ceil(wX1 / 512.0f);
            int minRz = (int)std::floor(wZ0 / 512.0f);
            int maxRz = (int)std::ceil(wZ1 / 512.0f);

            // 绘制横纵网格线
            for (int rx = minRx; rx <= maxRx; ++rx) {
                float lineX = screenCenter.x + ((float)(rx * 512) - viewX) * zoom + offsetX;
                drawList->AddLine(ImVec2(lineX, 0.0f), ImVec2(lineX, screenSize.y), regCol, regionGridThickness);
            }
            for (int rz = minRz; rz <= maxRz; ++rz) {
                float lineY = screenCenter.y + ((float)(rz * 512) - viewZ) * zoom + offsetZ;
                drawList->AddLine(ImVec2(0.0f, lineY), ImVec2(screenSize.x, lineY), regCol, regionGridThickness);
            }

            // 网格中央标注区域文件名 (如 r.0.0)
            if (showRegionGridLabel && !MapRenderState::bigMapHideUI && zoom * 512.0f >= 38.0f) {
                float labelFontSize = std::clamp(baseFontSize * (zoom * 512.0f / 160.0f), 11.0f, 15.0f);
                for (int rx = minRx; rx < maxRx; ++rx) {
                    for (int rz = minRz; rz < maxRz; ++rz) {
                        float cx = screenCenter.x + ((float)(rx * 512 + 256) - viewX) * zoom + offsetX;
                        float cy = screenCenter.y + ((float)(rz * 512 + 256) - viewZ) * zoom + offsetZ;
                        if (cx < -60.0f || cx > screenSize.x + 60.0f || cy < -40.0f || cy > screenSize.y + 40.0f) continue;

                        char tagBuf[32];
                        snprintf(tagBuf, sizeof(tagBuf), "r.%d.%d", rx, rz);
                        ImVec2 txtSz = font->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, tagBuf);

                        ImVec2 pMin(cx - txtSz.x * 0.5f - 4.0f, cy - txtSz.y * 0.5f - 2.0f);
                        ImVec2 pMax(cx + txtSz.x * 0.5f + 4.0f, cy + txtSz.y * 0.5f + 2.0f);

                        drawList->AddRectFilled(pMin, pMax, IM_COL32(12, 18, 26, 195), 3.0f);
                        drawList->AddRect(pMin, pMax, regCol, 3.0f, 0, 1.0f);
                        drawList->AddText(font, labelFontSize, ImVec2(cx - txtSz.x * 0.5f, cy - txtSz.y * 0.5f), IM_COL32(230, 245, 255, 240), tagBuf);
                    }
                }
            }
        }

        // 2. Map Art Grid (128x128)
        if (showMapArtGrid && (zoom * 128.0f >= 4.0f)) {
            ImU32 maCol = ImGui::ColorConvertFloat4ToU32(ImVec4(mapArtGridColor[0], mapArtGridColor[1], mapArtGridColor[2], mapArtGridColor[3]));
            int minMx = (int)std::floor((wX0 + 64.0f) / 128.0f);
            int maxMx = (int)std::ceil((wX1 + 64.0f) / 128.0f);
            int minMz = (int)std::floor((wZ0 + 64.0f) / 128.0f);
            int maxMz = (int)std::ceil((wZ1 + 64.0f) / 128.0f);

            for (int mx = minMx; mx <= maxMx; ++mx) {
                float lineX = screenCenter.x + ((float)(mx * 128 - 64) - viewX) * zoom + offsetX;
                drawList->AddLine(ImVec2(lineX, 0.0f), ImVec2(lineX, screenSize.y), maCol, mapArtGridThickness);
            }
            for (int mz = minMz; mz <= maxMz; ++mz) {
                float lineY = screenCenter.y + ((float)(mz * 128 - 64) - viewZ) * zoom + offsetZ;
                drawList->AddLine(ImVec2(0.0f, lineY), ImVec2(screenSize.x, lineY), maCol, mapArtGridThickness);
            }

            if (showMapArtGridLabel && !MapRenderState::bigMapHideUI && zoom * 128.0f >= 36.0f) {
                float labelFontSize = std::clamp(baseFontSize * (zoom * 128.0f / 80.0f), 10.0f, 13.0f);
                for (int mx = minMx; mx < maxMx; ++mx) {
                    for (int mz = minMz; mz < maxMz; ++mz) {
                        float cx = screenCenter.x + ((float)(mx * 128) - viewX) * zoom + offsetX;
                        float cy = screenCenter.y + ((float)(mz * 128) - viewZ) * zoom + offsetZ;
                        if (cx < -50.0f || cx > screenSize.x + 50.0f || cy < -30.0f || cy > screenSize.y + 30.0f) continue;

                        char tagBuf[32];
                        snprintf(tagBuf, sizeof(tagBuf), "M:%d,%d", mx, mz);
                        ImVec2 txtSz = font->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, tagBuf);

                        ImVec2 pMin(cx - txtSz.x * 0.5f - 3.0f, cy - txtSz.y * 0.5f - 1.5f);
                        ImVec2 pMax(cx + txtSz.x * 0.5f + 3.0f, cy + txtSz.y * 0.5f + 1.5f);

                        drawList->AddRectFilled(pMin, pMax, IM_COL32(20, 10, 24, 185), 3.0f);
                        drawList->AddRect(pMin, pMax, maCol, 3.0f, 0, 1.0f);
                        drawList->AddText(font, labelFontSize, ImVec2(cx - txtSz.x * 0.5f, cy - txtSz.y * 0.5f), IM_COL32(255, 230, 245, 240), tagBuf);
                    }
                }
            }
        }

        // 3. Spawn Chunks (主世界)
        if (showSpawnChunks && dimId == 0) {
            int spX = (spawnChunksAutoDetect && MapRenderState::hasWorldSpawn) ? MapRenderState::worldSpawnX : customSpawnX;
            int spZ = (spawnChunksAutoDetect && MapRenderState::hasWorldSpawn) ? MapRenderState::worldSpawnZ : customSpawnZ;
            int scX = (int)std::floor((float)spX / 16.0f);
            int scZ = (int)std::floor((float)spZ / 16.0f);

            int r1 = spawnEntityRadiusChunks;
            int r2 = r1 + spawnLazyRadiusChunks;
            int r3 = r2 + spawnBorderRadiusChunks;

            auto renderSpawnBox = [&](int rad, ImU32 fillCol, ImU32 borderCol, float thick, const char* label) {
                float bx0 = (float)(scX - rad) * 16.0f;
                float bx1 = (float)(scX + rad + 1) * 16.0f;
                float bz0 = (float)(scZ - rad) * 16.0f;
                float bz1 = (float)(scZ + rad + 1) * 16.0f;

                float sx0 = screenCenter.x + (bx0 - viewX) * zoom + offsetX;
                float sx1 = screenCenter.x + (bx1 - viewX) * zoom + offsetX;
                float sz0 = screenCenter.y + (bz0 - viewZ) * zoom + offsetZ;
                float sz1 = screenCenter.y + (bz1 - viewZ) * zoom + offsetZ;

                ImVec2 bMin(std::min(sx0, sx1), std::min(sz0, sz1));
                ImVec2 bMax(std::max(sx0, sx1), std::max(sz0, sz1));

                drawList->AddRectFilled(bMin, bMax, fillCol);
                drawList->AddRect(bMin, bMax, borderCol, 0.0f, 0, thick);

                if (!MapRenderState::bigMapHideUI && label && label[0] && (bMax.x - bMin.x >= 70.0f)) {
                    float lFontSize = std::clamp(baseFontSize * 0.90f, 11.0f, 14.0f);
                    ImVec2 lSz = font->CalcTextSizeA(lFontSize, FLT_MAX, 0.0f, label);
                    ImVec2 tagPos(bMin.x + 6.0f, bMin.y + 6.0f);
                    if (tagPos.x >= 0.0f && tagPos.x + lSz.x <= screenSize.x && tagPos.y >= 0.0f && tagPos.y + lSz.y <= screenSize.y) {
                        drawList->AddRectFilled(ImVec2(tagPos.x - 3.0f, tagPos.y - 1.5f), ImVec2(tagPos.x + lSz.x + 3.0f, tagPos.y + lSz.y + 1.5f), IM_COL32(10, 14, 20, 210), 3.0f);
                        drawList->AddRect(ImVec2(tagPos.x - 3.0f, tagPos.y - 1.5f), ImVec2(tagPos.x + lSz.x + 3.0f, tagPos.y + lSz.y + 1.5f), borderCol, 3.0f, 0, 1.0f);
                        drawList->AddText(font, lFontSize, tagPos, IM_COL32(255, 255, 255, 235), label);
                    }
                }
            };

            // 外围加载区 -> 红石惰性区 -> 实体处理区 (由外到内分层绘制)
            char buf3[64]; snprintf(buf3, sizeof(buf3), "%s (%dx%d)", LanguageManager::GetText("SPAWN_CHUNKS_BORDER"), r3 * 2 + 1, r3 * 2 + 1);
            char buf2[64]; snprintf(buf2, sizeof(buf2), "%s (%dx%d)", LanguageManager::GetText("SPAWN_CHUNKS_LAZY"), r2 * 2 + 1, r2 * 2 + 1);
            char buf1[64]; snprintf(buf1, sizeof(buf1), "%s (%dx%d)", LanguageManager::GetText("SPAWN_CHUNKS_ENTITY"), r1 * 2 + 1, r1 * 2 + 1);

            renderSpawnBox(r3, IM_COL32(120, 180, 255, 15), IM_COL32(120, 180, 255, 180), 1.4f, buf3);
            renderSpawnBox(r2, IM_COL32(255, 215, 50, 22),  IM_COL32(255, 215, 50, 200), 1.8f, buf2);
            renderSpawnBox(r1, IM_COL32(50, 240, 100, 32),  IM_COL32(50, 240, 100, 230), 2.2f, buf1);

            // 中心十字锚点
            float spScreenX = screenCenter.x + ((float)spX - viewX) * zoom + offsetX;
            float spScreenY = screenCenter.y + ((float)spZ - viewZ) * zoom + offsetZ;
            drawList->AddCircleFilled(ImVec2(spScreenX, spScreenY), 4.5f, IM_COL32(255, 235, 80, 240), 16);
            drawList->AddCircle(ImVec2(spScreenX, spScreenY), 7.0f, IM_COL32(0, 0, 0, 200), 16, 1.5f);
        }

        // 6. 视距覆盖层
        if (showViewDistance && dimId == MapRenderState::currentDimensionId) {
            int pcX = (int)std::floor(viewX / 16.0f);
            int pcZ = (int)std::floor(viewZ / 16.0f);
            int rad = viewDistanceChunks;

            ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(viewDistanceColor[0], viewDistanceColor[1], viewDistanceColor[2], viewDistanceColor[3]));
            ImU32 fillCol   = ImGui::ColorConvertFloat4ToU32(ImVec4(viewDistanceColor[0], viewDistanceColor[1], viewDistanceColor[2], viewDistanceFillAlpha));

            float labelAnchorX = 0.0f;
            float labelAnchorY = 0.0f;
            bool isVisible = false;

            if (viewDistanceShape == 0) {
                // 方形区块网格
                float bx0 = (float)(pcX - rad) * 16.0f;
                float bx1 = (float)(pcX + rad + 1) * 16.0f;
                float bz0 = (float)(pcZ - rad) * 16.0f;
                float bz1 = (float)(pcZ + rad + 1) * 16.0f;

                float sx0 = screenCenter.x + (bx0 - viewX) * zoom + offsetX;
                float sx1 = screenCenter.x + (bx1 - viewX) * zoom + offsetX;
                float sz0 = screenCenter.y + (bz0 - viewZ) * zoom + offsetZ;
                float sz1 = screenCenter.y + (bz1 - viewZ) * zoom + offsetZ;

                if (sx1 >= 0.0f && sx0 <= screenSize.x && sz1 >= 0.0f && sz0 <= screenSize.y) {
                    isVisible = true;
                    if (viewDistanceFillAlpha > 0.001f) {
                        drawList->AddRectFilled(ImVec2(sx0, sz0), ImVec2(sx1, sz1), fillCol);
                    }
                    drawList->AddRect(ImVec2(sx0, sz0), ImVec2(sx1, sz1), borderCol, 0.0f, 0, viewDistanceThickness);
                    labelAnchorX = sx0 + 8.0f;
                    labelAnchorY = sz0 + 6.0f;
                }
            } else {
                // 平滑径向圆形
                float psx = screenCenter.x + offsetX;
                float psz = screenCenter.y + offsetZ;
                float radPx = (float)rad * 16.0f * zoom;

                if (psx + radPx >= 0.0f && psx - radPx <= screenSize.x && psz + radPx >= 0.0f && psz - radPx <= screenSize.y) {
                    isVisible = true;
                    if (viewDistanceFillAlpha > 0.001f) {
                        drawList->AddCircleFilled(ImVec2(psx, psz), radPx, fillCol, 64);
                    }
                    drawList->AddCircle(ImVec2(psx, psz), radPx, borderCol, 64, viewDistanceThickness);
                    labelAnchorX = psx - radPx * 0.707f + 6.0f;
                    labelAnchorY = psz - radPx * 0.707f + 4.0f;
                }
            }

            if (!MapRenderState::bigMapHideUI && isVisible && showViewDistanceLabel && (rad * 16.0f * zoom >= 24.0f)) {
                char labelBuf[128];
                snprintf(labelBuf, sizeof(labelBuf), "%s: %d %s · %dm",
                    LanguageManager::GetText("OVERLAY_TAG_VIEW_DIST"),
                    viewDistanceChunks,
                    LanguageManager::GetText("CHUNKS_UNIT"),
                    viewDistanceChunks * 16);
                float labelFontSize = std::clamp(baseFontSize * 0.95f, 11.0f, 15.0f);
                ImVec2 tSz = font->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, labelBuf);
                float lx = std::clamp(labelAnchorX, 10.0f, screenSize.x - tSz.x - 14.0f);
                float ly = std::clamp(labelAnchorY, 10.0f, screenSize.y - tSz.y - 10.0f);
                drawList->AddRectFilled(ImVec2(lx - 4.0f, ly - 2.0f), ImVec2(lx + tSz.x + 4.0f, ly + tSz.y + 2.0f), IM_COL32(18, 18, 22, 215), 3.0f);
                drawList->AddRect(ImVec2(lx - 4.0f, ly - 2.0f), ImVec2(lx + tSz.x + 4.0f, ly + tSz.y + 2.0f), borderCol, 3.0f, 0, 1.2f);
                drawList->AddText(font, labelFontSize, ImVec2(lx, ly), IM_COL32(255, 255, 255, 240), labelBuf);
            }
        }

        // 7. 模拟距离覆盖层
        if (showSimulationDistance && dimId == MapRenderState::currentDimensionId) {
            int pcX = (int)std::floor(viewX / 16.0f);
            int pcZ = (int)std::floor(viewZ / 16.0f);
            int rad = simulationDistanceChunks;

            ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(simulationDistanceColor[0], simulationDistanceColor[1], simulationDistanceColor[2], simulationDistanceColor[3]));
            ImU32 fillCol   = ImGui::ColorConvertFloat4ToU32(ImVec4(simulationDistanceColor[0], simulationDistanceColor[1], simulationDistanceColor[2], simulationDistanceFillAlpha));

            float labelAnchorX = 0.0f;
            float labelAnchorY = 0.0f;
            bool isVisible = false;

            if (simulationDistanceShape == 0) {
                // 方形区块网格
                float bx0 = (float)(pcX - rad) * 16.0f;
                float bx1 = (float)(pcX + rad + 1) * 16.0f;
                float bz0 = (float)(pcZ - rad) * 16.0f;
                float bz1 = (float)(pcZ + rad + 1) * 16.0f;

                float sx0 = screenCenter.x + (bx0 - viewX) * zoom + offsetX;
                float sx1 = screenCenter.x + (bx1 - viewX) * zoom + offsetX;
                float sz0 = screenCenter.y + (bz0 - viewZ) * zoom + offsetZ;
                float sz1 = screenCenter.y + (bz1 - viewZ) * zoom + offsetZ;

                if (sx1 >= 0.0f && sx0 <= screenSize.x && sz1 >= 0.0f && sz0 <= screenSize.y) {
                    isVisible = true;
                    if (simulationDistanceFillAlpha > 0.001f) {
                        drawList->AddRectFilled(ImVec2(sx0, sz0), ImVec2(sx1, sz1), fillCol);
                    }
                    drawList->AddRect(ImVec2(sx0, sz0), ImVec2(sx1, sz1), borderCol, 0.0f, 0, simulationDistanceThickness);
                    labelAnchorX = sx0 + 8.0f;
                    labelAnchorY = sz0 + 6.0f;
                }
            } else {
                // 平滑径向圆形
                float psx = screenCenter.x + offsetX;
                float psz = screenCenter.y + offsetZ;
                float radPx = (float)rad * 16.0f * zoom;

                if (psx + radPx >= 0.0f && psx - radPx <= screenSize.x && psz + radPx >= 0.0f && psz - radPx <= screenSize.y) {
                    isVisible = true;
                    if (simulationDistanceFillAlpha > 0.001f) {
                        drawList->AddCircleFilled(ImVec2(psx, psz), radPx, fillCol, 64);
                    }
                    drawList->AddCircle(ImVec2(psx, psz), radPx, borderCol, 64, simulationDistanceThickness);
                    labelAnchorX = psx - radPx * 0.707f + 6.0f;
                    labelAnchorY = psz - radPx * 0.707f + 4.0f;
                }
            }

            if (!MapRenderState::bigMapHideUI && isVisible && showSimulationDistanceLabel && (rad * 16.0f * zoom >= 24.0f)) {
                char labelBuf[128];
                snprintf(labelBuf, sizeof(labelBuf), "%s: %d %s · %dm",
                    LanguageManager::GetText("OVERLAY_TAG_SIM_DIST"),
                    simulationDistanceChunks,
                    LanguageManager::GetText("CHUNKS_UNIT"),
                    simulationDistanceChunks * 16);
                float labelFontSize = std::clamp(baseFontSize * 0.95f, 11.0f, 15.0f);
                ImVec2 tSz = font->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, labelBuf);
                float lx = std::clamp(labelAnchorX, 10.0f, screenSize.x - tSz.x - 14.0f);
                float ly = std::clamp(labelAnchorY, 10.0f, screenSize.y - tSz.y - 10.0f);
                drawList->AddRectFilled(ImVec2(lx - 4.0f, ly - 2.0f), ImVec2(lx + tSz.x + 4.0f, ly + tSz.y + 2.0f), IM_COL32(18, 18, 22, 215), 3.0f);
                drawList->AddRect(ImVec2(lx - 4.0f, ly - 2.0f), ImVec2(lx + tSz.x + 4.0f, ly + tSz.y + 2.0f), borderCol, 3.0f, 0, 1.2f);
                drawList->AddText(font, labelFontSize, ImVec2(lx, ly), IM_COL32(255, 255, 255, 240), labelBuf);
            }
        }

        // 4. World Border
        if (showWorldBorder) {
            float halfSz = (float)(worldBorderSize * 0.5);
            float wbMinX = worldBorderCenterX - halfSz;
            float wbMaxX = worldBorderCenterX + halfSz;
            float wbMinZ = worldBorderCenterZ - halfSz;
            float wbMaxZ = worldBorderCenterZ + halfSz;

            float sx0 = screenCenter.x + (wbMinX - viewX) * zoom + offsetX;
            float sx1 = screenCenter.x + (wbMaxX - viewX) * zoom + offsetX;
            float sz0 = screenCenter.y + (wbMinZ - viewZ) * zoom + offsetZ;
            float sz1 = screenCenter.y + (wbMaxZ - viewZ) * zoom + offsetZ;

            ImU32 wbCol = ImGui::ColorConvertFloat4ToU32(ImVec4(worldBorderColor[0], worldBorderColor[1], worldBorderColor[2], worldBorderColor[3]));

            // 绘制边界外暗红遮罩
            if (showWorldBorderWarning) {
                ImU32 warnCol = IM_COL32(180, 25, 25, 55);
                if (sz0 > 0.0f) drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screenSize.x, sz0), warnCol);
                if (sz1 < screenSize.y) drawList->AddRectFilled(ImVec2(0.0f, sz1), ImVec2(screenSize.x, screenSize.y), warnCol);
                if (sx0 > 0.0f) drawList->AddRectFilled(ImVec2(0.0f, std::max(0.0f, sz0)), ImVec2(sx0, std::min(screenSize.y, sz1)), warnCol);
                if (sx1 < screenSize.x) drawList->AddRectFilled(ImVec2(sx1, std::max(0.0f, sz0)), ImVec2(screenSize.x, std::min(screenSize.y, sz1)), warnCol);
            }

            // 绘制边界警戒框 (外层醒目红线 + 内层细虚线感)
            ImVec2 bMin(sx0, sz0);
            ImVec2 bMax(sx1, sz1);
            drawList->AddRect(bMin, bMax, wbCol, 0.0f, 0, 2.5f);
            drawList->AddRect(ImVec2(bMin.x + 2.0f, bMin.y + 2.0f), ImVec2(bMax.x - 2.0f, bMax.y - 2.0f), IM_COL32(255, 255, 255, 175), 0.0f, 0, 1.0f);

            // 边界文字标注
            if (!MapRenderState::bigMapHideUI && sx0 >= -100.0f && sx0 <= screenSize.x + 100.0f && sz0 >= 0.0f && sz0 <= screenSize.y) {
                char wbBuf[64];
                snprintf(wbBuf, sizeof(wbBuf), "%s", LanguageManager::GetText("WORLD_BORDER"));
                float wbFontSize = baseFontSize * 1.05f;
                ImVec2 tSz = font->CalcTextSizeA(wbFontSize, FLT_MAX, 0.0f, wbBuf);
                ImVec2 tPos(std::clamp(sx0 + 12.0f, 10.0f, screenSize.x - tSz.x - 10.0f), std::clamp(sz0 + 6.0f, 10.0f, screenSize.y - tSz.y - 10.0f));
                drawList->AddRectFilled(ImVec2(tPos.x - 4.0f, tPos.y - 2.0f), ImVec2(tPos.x + tSz.x + 4.0f, tPos.y + tSz.y + 2.0f), IM_COL32(18, 12, 14, 210), 3.0f);
                drawList->AddRect(ImVec2(tPos.x - 4.0f, tPos.y - 2.0f), ImVec2(tPos.x + tSz.x + 4.0f, tPos.y + tSz.y + 2.0f), wbCol, 3.0f, 0, 1.2f);
                drawList->AddText(font, wbFontSize, tPos, IM_COL32(255, 220, 220, 245), wbBuf);
            }
        }

        // 5. Persistent Breadcrumbs
        if (persistentBreadcrumbsEnabled && MapRenderState::showFootsteps) {
            std::lock_guard<std::mutex> lock(g_breadcrumbsMutex);
            size_t n = g_breadcrumbs.size();
            if (n >= 2) {
                ImU32 baseCol = ImGui::ColorConvertFloat4ToU32(ImVec4(breadcrumbColor[0], breadcrumbColor[1], breadcrumbColor[2], breadcrumbColor[3]));
                for (size_t i = 1; i < n; ++i) {
                    const auto& pA = g_breadcrumbs[i - 1];
                    const auto& pB = g_breadcrumbs[i];
                    if (pA.dimId != dimId || pB.dimId != dimId) continue;

                    float distSq = (pB.x - pA.x) * (pB.x - pA.x) + (pB.z - pA.z) * (pB.z - pA.z);
                    if (distSq > 64.0f * 64.0f) continue; // 跨度过大则断开

                    float sx0 = screenCenter.x + (pA.x - viewX) * zoom + offsetX;
                    float sz0 = screenCenter.y + (pA.z - viewZ) * zoom + offsetZ;
                    float sx1 = screenCenter.x + (pB.x - viewX) * zoom + offsetX;
                    float sz1 = screenCenter.y + (pB.z - viewZ) * zoom + offsetZ;

                    if ((sx0 < -50.0f && sx1 < -50.0f) || (sx0 > screenSize.x + 50.0f && sx1 > screenSize.x + 50.0f) ||
                        (sz0 < -50.0f && sz1 < -50.0f) || (sz0 > screenSize.y + 50.0f && sz1 > screenSize.y + 50.0f)) continue;

                    float progress = (float)i / (float)n;
                    int alpha = (int)(45.0f + 210.0f * progress);
                    ImU32 col = (baseCol & 0x00FFFFFF) | ((uint32_t)alpha << 24);

                    if (breadcrumbsShowLines) {
                        drawList->AddLine(ImVec2(sx0, sz0), ImVec2(sx1, sz1), col, 2.2f);
                    }
                    drawList->AddCircleFilled(ImVec2(sx1, sz1), 2.2f, col, 8);
                }
            }
        }
    }

    static bool MatchSettingFilter(const char* filter, std::initializer_list<const char*> keywords) {
        if (!filter || filter[0] == '\0') return true;
        std::string lowerFilter;
        for (const char* p = filter; *p; ++p) {
            lowerFilter += (char)std::tolower((unsigned char)*p);
        }
        while (!lowerFilter.empty() && (lowerFilter.front() == ' ' || lowerFilter.front() == '\t')) {
            lowerFilter.erase(lowerFilter.begin());
        }
        while (!lowerFilter.empty() && (lowerFilter.back() == ' ' || lowerFilter.back() == '\t')) {
            lowerFilter.pop_back();
        }
        if (lowerFilter.empty()) return true;

        for (const char* kw : keywords) {
            if (!kw || kw[0] == '\0') continue;
            std::string lowerKw;
            for (const char* p = kw; *p; ++p) {
                lowerKw += (char)std::tolower((unsigned char)*p);
            }
            if (lowerKw.find(lowerFilter) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    void RenderSettingsCategory(int& matchCount, const char* searchFilter, float curScale) {
        bool m_sec = MatchSettingFilter(searchFilter, { LanguageManager::GetText("SPECIAL_GRIDS_SETTINGS"), "grid", "region", "spawn", "worldborder", "breadcrumbs", "viewdistance", "simulationdistance", "特殊辅助网格", "网格", "区域", "出生点", "世界边界", "足迹", "视距", "模拟距离" });
        bool m_reg = MatchSettingFilter(searchFilter, { LanguageManager::GetText("REGION_GRID"), LanguageManager::GetText("REGION_GRID_DESC"), "region", "区域" });
        bool m_mapArt = MatchSettingFilter(searchFilter, { LanguageManager::GetText("MAP_ART_GRID"), LanguageManager::GetText("MAP_ART_GRID_DESC"), "mapart", "地图画" });
        bool m_spawn = MatchSettingFilter(searchFilter, { LanguageManager::GetText("SPAWN_CHUNKS"), LanguageManager::GetText("SPAWN_CHUNKS_DESC"), "spawn", "chunks", "出生点", "区块" });
        bool m_wb = MatchSettingFilter(searchFilter, { LanguageManager::GetText("WORLD_BORDER"), LanguageManager::GetText("WORLD_BORDER_DESC"), "worldborder", "世界边界", "边界" });
        bool m_bc = MatchSettingFilter(searchFilter, { LanguageManager::GetText("PERSISTENT_BREADCRUMBS"), LanguageManager::GetText("PERSISTENT_BREADCRUMBS_DESC"), "breadcrumbs", "footsteps", "足迹", "脚印" });
        bool m_vd = MatchSettingFilter(searchFilter, { LanguageManager::GetText("VIEW_DISTANCE_OVERLAY"), LanguageManager::GetText("VIEW_DISTANCE_OVERLAY_DESC"), "view", "distance", "render", "视距", "视野" });
        bool m_sd = MatchSettingFilter(searchFilter, { LanguageManager::GetText("SIMULATION_DISTANCE_OVERLAY"), LanguageManager::GetText("SIMULATION_DISTANCE_OVERLAY_DESC"), "simulation", "tick", "ticking", "模拟", "模拟距离" });

        bool anyMatch = m_sec || m_reg || m_mapArt || m_spawn || m_wb || m_bc || m_vd || m_sd;
        if (!anyMatch) return;

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LanguageManager::GetText("SPECIAL_GRIDS_SETTINGS"));
        ImGui::Separator();

        // 1. Region Grid
        if (m_sec || m_reg) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("REGION_GRID"), &showRegionGrid)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("REGION_GRID_DESC"));
            }
            if (showRegionGrid) {
                ImGui::Indent(15.0f * curScale);
                if (ImGui::Checkbox(LanguageManager::GetText("REGION_GRID_LABEL"), &showRegionGridLabel)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::ColorEdit4("##RegionGridCol", regionGridColor, ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("REGION_GRID_COLOR"));
                ImGui::Unindent(15.0f * curScale);
            }
        }

        // 2. Map Art Grid
        if (m_sec || m_mapArt) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("MAP_ART_GRID"), &showMapArtGrid)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("MAP_ART_GRID_DESC"));
            }
            if (showMapArtGrid) {
                ImGui::Indent(15.0f * curScale);
                if (ImGui::Checkbox(LanguageManager::GetText("MAP_ART_GRID_LABEL"), &showMapArtGridLabel)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::ColorEdit4("##MapArtGridCol", mapArtGridColor, ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("MAP_ART_GRID_COLOR"));
                ImGui::Unindent(15.0f * curScale);
            }
        }

        // 3. Spawn Chunks
        if (m_sec || m_spawn) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("SPAWN_CHUNKS"), &showSpawnChunks)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("SPAWN_CHUNKS_DESC"));
            }
            if (showSpawnChunks) {
                ImGui::Indent(15.0f * curScale);
                if (ImGui::Checkbox(LanguageManager::GetText("SPAWN_CHUNKS_AUTO"), &spawnChunksAutoDetect)) {
                    LanguageManager::SaveConfig();
                }
                if (!spawnChunksAutoDetect) {
                    ImGui::SetNextItemWidth(120.0f * curScale);
                    if (ImGui::InputInt("Spawn X", &customSpawnX)) LanguageManager::SaveConfig();
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120.0f * curScale);
                    if (ImGui::InputInt("Spawn Z", &customSpawnZ)) LanguageManager::SaveConfig();
                }
                if (ImGui::Button(LanguageManager::GetText("SPAWN_CHUNKS_SET_PLAYER"))) {
                    spawnChunksAutoDetect = false;
                    customSpawnX = g_playerBlockX;
                    customSpawnZ = g_playerBlockZ;
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("SPAWN_CHUNKS_RESET"))) {
                    spawnChunksAutoDetect = true;
                    if (MapRenderState::hasWorldSpawn) {
                        customSpawnX = MapRenderState::worldSpawnX;
                        customSpawnZ = MapRenderState::worldSpawnZ;
                    } else {
                        customSpawnX = 0;
                        customSpawnZ = 0;
                    }
                    LanguageManager::SaveConfig();
                }

                if (ImGui::Checkbox(LanguageManager::GetText("SPAWN_CHUNKS_MINIMAP"), &showSpawnChunksOnMinimap)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::Unindent(15.0f * curScale);
            }
        }

        // 4. World Border
        if (m_sec || m_wb) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("WORLD_BORDER"), &showWorldBorder)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("WORLD_BORDER_DESC"));
            }
            if (showWorldBorder) {
                ImGui::Indent(15.0f * curScale);
                if (ImGui::Checkbox(LanguageManager::GetText("WORLD_BORDER_WARNING"), &showWorldBorderWarning)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SetNextItemWidth(160.0f * curScale);
                int wbInt = (int)(worldBorderSize / 2.0);
                if (ImGui::InputInt("± Blocks##WorldBorderRad", &wbInt, 1000, 10000)) {
                    if (wbInt < 100) wbInt = 100;
                    if (wbInt > 30000000) wbInt = 30000000;
                    worldBorderSize = (double)wbInt * 2.0;
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("WORLD_BORDER_SIZE"));

                ImGui::ColorEdit4("##WorldBorderCol", worldBorderColor, ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("WORLD_BORDER_COLOR"));
                ImGui::Unindent(15.0f * curScale);
            }
        }

        // 5. Persistent Breadcrumbs
        if (m_sec || m_bc) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("PERSISTENT_BREADCRUMBS"), &persistentBreadcrumbsEnabled)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("PERSISTENT_BREADCRUMBS_DESC"));
            }
            if (persistentBreadcrumbsEnabled) {
                ImGui::Indent(15.0f * curScale);
                ImGui::SetNextItemWidth(160.0f * curScale);
                if (ImGui::SliderInt("##MaxBreadcrumbsPts", &maxBreadcrumbPoints, 200, 10000, "%d")) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("BREADCRUMBS_MAX_POINTS"));

                ImGui::SetNextItemWidth(160.0f * curScale);
                if (ImGui::SliderFloat("##BreadcrumbMinDist", &breadcrumbMinDistance, 2.0f, 32.0f, "%.1fm")) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("BREADCRUMBS_INTERVAL"));

                ImGui::ColorEdit4("##BreadcrumbCol", breadcrumbColor, ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("BREADCRUMBS_COLOR"));

                static bool s_showClearConfirm = false;
                if (ImGui::Button(LanguageManager::GetText("BREADCRUMBS_CLEAR"))) {
                    s_showClearConfirm = true;
                }
                if (s_showClearConfirm) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LanguageManager::GetText("BREADCRUMBS_CLEAR_CONFIRM"));
                    ImGui::SameLine();
                    if (ImGui::Button(LanguageManager::GetText("WP_CONFIRM"))) {
                        ClearBreadcrumbs();
                        s_showClearConfirm = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(LanguageManager::GetText("WP_CANCEL"))) {
                        s_showClearConfirm = false;
                    }
                }
                ImGui::Unindent(15.0f * curScale);
            }
        }

        // 6. View Distance Overlay
        if (m_sec || m_vd) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("VIEW_DISTANCE_OVERLAY"), &showViewDistance)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("VIEW_DISTANCE_OVERLAY_DESC"));
            }
            if (showViewDistance) {
                ImGui::Indent(15.0f * curScale);

                // 视距区块数调节滑块 + 重置按钮
                ImGui::SetNextItemWidth(170.0f * curScale);
                char vdFmt[64];
                snprintf(vdFmt, sizeof(vdFmt), "%%d %s", LanguageManager::GetText("CHUNKS_UNIT"));
                if (ImGui::SliderInt("##ViewDistChunks", &viewDistanceChunks, 2, 64, vdFmt)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("VIEW_DISTANCE_CHUNKS"));
                ImGui::SameLine();
                if (ImGui::SmallButton((std::string(LanguageManager::GetText("HOTKEY_RESET")) + "##ResetVD").c_str())) {
                    viewDistanceChunks = 16;
                    LanguageManager::SaveConfig();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s: 16 %s", LanguageManager::GetText("RESET"), LanguageManager::GetText("CHUNKS_UNIT"));
                }

                // 形状单选（方形区块网格 vs 平滑径向圆形）
                ImGui::Text("%s:", LanguageManager::GetText("VIEW_DISTANCE_SHAPE"));
                ImGui::SameLine();
                if (ImGui::RadioButton((std::string(LanguageManager::GetText("OVERLAY_SHAPE_SQUARE")) + "##VDSq").c_str(), viewDistanceShape == 0)) {
                    viewDistanceShape = 0;
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::RadioButton((std::string(LanguageManager::GetText("OVERLAY_SHAPE_CIRCLE")) + "##VDCir").c_str(), viewDistanceShape == 1)) {
                    viewDistanceShape = 1;
                    LanguageManager::SaveConfig();
                }

                // 颜色与透明度
                ImGui::ColorEdit4("##ViewDistCol", viewDistanceColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("VIEW_DISTANCE_COLOR"));

                ImGui::SetNextItemWidth(170.0f * curScale);
                if (ImGui::SliderFloat("##ViewDistFillAlpha", &viewDistanceFillAlpha, 0.0f, 0.8f, "%.2f")) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("OVERLAY_FILL_OPACITY"));

                ImGui::SetNextItemWidth(170.0f * curScale);
                if (ImGui::SliderFloat("##ViewDistThick", &viewDistanceThickness, 1.0f, 6.0f, "%.1fpx")) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("OVERLAY_BORDER_WIDTH"));

                // 小地图与文字标签开关
                if (ImGui::Checkbox(LanguageManager::GetText("VIEW_DISTANCE_MINIMAP"), &showViewDistanceOnMinimap)) {
                    LanguageManager::SaveConfig();
                }
                if (ImGui::Checkbox(LanguageManager::GetText("VIEW_DISTANCE_LABEL"), &showViewDistanceLabel)) {
                    LanguageManager::SaveConfig();
                }

                ImGui::Unindent(15.0f * curScale);
            }
        }

        // 7. Simulation Distance Overlay
        if (m_sec || m_sd) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("SIMULATION_DISTANCE_OVERLAY"), &showSimulationDistance)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("SIMULATION_DISTANCE_OVERLAY_DESC"));
            }
            if (showSimulationDistance) {
                ImGui::Indent(15.0f * curScale);

                // 模拟距离区块数调节滑块 + 重置按钮
                ImGui::SetNextItemWidth(170.0f * curScale);
                char sdFmt[64];
                snprintf(sdFmt, sizeof(sdFmt), "%%d %s", LanguageManager::GetText("CHUNKS_UNIT"));
                if (ImGui::SliderInt("##SimDistChunks", &simulationDistanceChunks, 2, 32, sdFmt)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("SIMULATION_DISTANCE_CHUNKS"));
                ImGui::SameLine();
                if (ImGui::SmallButton((std::string(LanguageManager::GetText("HOTKEY_RESET")) + "##ResetSD").c_str())) {
                    simulationDistanceChunks = 8;
                    LanguageManager::SaveConfig();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s: 8 %s", LanguageManager::GetText("RESET"), LanguageManager::GetText("CHUNKS_UNIT"));
                }

                // 形状单选（方形区块网格 vs 平滑径向圆形）
                ImGui::Text("%s:", LanguageManager::GetText("SIMULATION_DISTANCE_SHAPE"));
                ImGui::SameLine();
                if (ImGui::RadioButton((std::string(LanguageManager::GetText("OVERLAY_SHAPE_SQUARE")) + "##SDSq").c_str(), simulationDistanceShape == 0)) {
                    simulationDistanceShape = 0;
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::RadioButton((std::string(LanguageManager::GetText("OVERLAY_SHAPE_CIRCLE")) + "##SDCir").c_str(), simulationDistanceShape == 1)) {
                    simulationDistanceShape = 1;
                    LanguageManager::SaveConfig();
                }

                // 颜色与透明度
                ImGui::ColorEdit4("##SimDistCol", simulationDistanceColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("SIMULATION_DISTANCE_COLOR"));

                ImGui::SetNextItemWidth(170.0f * curScale);
                if (ImGui::SliderFloat("##SimDistFillAlpha", &simulationDistanceFillAlpha, 0.0f, 0.8f, "%.2f")) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("OVERLAY_FILL_OPACITY"));

                ImGui::SetNextItemWidth(170.0f * curScale);
                if (ImGui::SliderFloat("##SimDistThick", &simulationDistanceThickness, 1.0f, 6.0f, "%.1fpx")) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                ImGui::Text("%s", LanguageManager::GetText("OVERLAY_BORDER_WIDTH"));

                // 小地图与文字标签开关
                if (ImGui::Checkbox(LanguageManager::GetText("SIMULATION_DISTANCE_MINIMAP"), &showSimulationDistanceOnMinimap)) {
                    LanguageManager::SaveConfig();
                }
                if (ImGui::Checkbox(LanguageManager::GetText("SIMULATION_DISTANCE_LABEL"), &showSimulationDistanceLabel)) {
                    LanguageManager::SaveConfig();
                }

                ImGui::Unindent(15.0f * curScale);
            }
        }

        ImGui::Spacing();
    }

} // namespace SpecialGridManager
