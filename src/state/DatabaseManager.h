#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include "state/SpecialGridManager.h"

namespace DatabaseManager {

    extern sqlite3* g_db;
    extern std::mutex g_dbMutex;
    extern std::string g_currentWorldId;
    extern std::atomic<bool> g_isInitialized;
    extern std::atomic<bool> g_dbConnected;
    extern int g_schemaVersion;
    extern std::string g_lastStatusMsg;

    void Init();
    void Shutdown();
    void SwitchWorld(const std::string& worldId);
    void SaveConfig(nlohmann::json& j);
    void LoadConfig(const nlohmann::json& j);

    // 数据库迁移器 (DatabaseMigrator)
    bool MigrateDatabase(sqlite3* db, const std::string& worldId);

    // 事务辅助
    bool BeginTransaction();
    bool CommitTransaction();
    bool RollbackTransaction();

    // 1. 区块高亮结构化存储 (Anarchy Highlights)
    bool SaveChunkHighlight(int chunkX, int chunkZ, int dimId, uint32_t flags, uint8_t lavaHeight, uint8_t portalType, uint64_t timestamp);
    bool DeleteChunkHighlight(int chunkX, int chunkZ, int dimId);
    bool ClearChunkHighlights(int dimId = -1);
    bool LoadAllChunkHighlights(std::function<void(int cx, int cz, int dimId, uint32_t flags, uint8_t lava, uint8_t portal, uint64_t ts)> callback);

    // 2. 足迹数据结构化存储 (Persistent Breadcrumbs)
    bool SaveBreadcrumbsBatch(const std::vector<SpecialGridManager::BreadcrumbPoint>& points);
    bool LoadAllBreadcrumbs(std::vector<SpecialGridManager::BreadcrumbPoint>& outPoints);
    bool ClearBreadcrumbs(int dimId = -1);

    // 3. 通用航点导入与导出备份 (Atlas & Waypoint Universal Backup)
    std::string ExportWaypointsBackup(const std::string& customPath = "");
    int ImportWaypointsBackup(const std::string& filePath);

    // 状态查询
    bool IsConnected();
    int GetSchemaVersion();
    std::string GetDatabaseFilePath();
    int64_t GetDatabaseSizeBytes();

    // 全屏大地图设置分类界面 (Category 6: QoL & Performance)
    void RenderSettingsCategory(int& matchCount, const char* searchFilter, float curScale);

} // namespace DatabaseManager
