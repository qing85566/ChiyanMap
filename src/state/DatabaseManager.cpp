#include "state/DatabaseManager.h"
#include "state/MapRenderState.h"
#include "state/LanguageManager.h"
#include "state/WaypointManager.h"
#include "state/SpecialGridManager.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <imgui.h>

using json = nlohmann::json;

namespace DatabaseManager {

    sqlite3* g_db = nullptr;
    std::mutex g_dbMutex;
    std::string g_currentWorldId = "default";
    std::atomic<bool> g_isInitialized{ false };
    std::atomic<bool> g_dbConnected{ false };
    int g_schemaVersion = 0;
    std::string g_lastStatusMsg = "";

    static std::string s_lastBackupExportedPath = "";
    static std::string s_lastImportStatusMsg = "";
    static float s_lastImportStatusTime = 0.0f;

    void Init() {
        if (g_isInitialized.load()) return;
        try {
            std::filesystem::create_directories("mods/ChiyanMap/data");
            std::filesystem::create_directories("mods/ChiyanMap/backups");
        } catch (...) {}
        g_isInitialized.store(true);
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(g_dbMutex);
        if (g_db) {
            sqlite3_close(g_db);
            g_db = nullptr;
            g_dbConnected.store(false);
        }
    }

    void SwitchWorld(const std::string& worldId) {
        std::lock_guard<std::mutex> lock(g_dbMutex);
        if (g_db) {
            sqlite3_close(g_db);
            g_db = nullptr;
            g_dbConnected.store(false);
        }

        g_currentWorldId = worldId.empty() ? "default" : worldId;
        if (!MapRenderState::useSqliteDatabase) return;

        try {
            std::filesystem::create_directories("mods/ChiyanMap/data");
        } catch (...) {}

        std::string dbPath = "mods/ChiyanMap/data/" + g_currentWorldId + "_database.db";
        int rc = sqlite3_open(dbPath.c_str(), &g_db);
        if (rc != SQLITE_OK || !g_db) {
            g_dbConnected.store(false);
            if (g_db) {
                sqlite3_close(g_db);
                g_db = nullptr;
            }
            return;
        }

        // 启用 WAL 模式与内存优化配置，获得极致高并发与磁盘写入吞吐
        sqlite3_exec(g_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(g_db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(g_db, "PRAGMA temp_store = MEMORY;", nullptr, nullptr, nullptr);
        sqlite3_exec(g_db, "PRAGMA cache_size = -8000;", nullptr, nullptr, nullptr);

        g_dbConnected.store(true);

        // 执行版本迁移器 (DatabaseMigrator)
        MigrateDatabase(g_db, g_currentWorldId);
    }

    void SaveConfig(nlohmann::json& j) {
        j["useSqliteDatabase"] = MapRenderState::useSqliteDatabase;
        j["minimapFpsLimit"] = MapRenderState::minimapFpsLimit;
        j["bigMapCrossDimCoords"] = MapRenderState::bigMapCrossDimCoords;
        j["persistBigMapDimension"] = MapRenderState::persistBigMapDimension;
        j["lastViewedDimensionId"] = MapRenderState::lastViewedDimensionId;
        j["teleportFailNotifier"] = MapRenderState::teleportFailNotifier;
        j["fastWritesEnabled"] = MapRenderState::fastWritesEnabled;
        j["writeBatchIntervalMs"] = MapRenderState::writeBatchIntervalMs;
    }

    void LoadConfig(const nlohmann::json& j) {
        if (j.contains("useSqliteDatabase")) MapRenderState::useSqliteDatabase = j["useSqliteDatabase"].get<bool>();
        if (j.contains("minimapFpsLimit")) MapRenderState::minimapFpsLimit = j["minimapFpsLimit"].get<int>();
        if (j.contains("bigMapCrossDimCoords")) MapRenderState::bigMapCrossDimCoords = j["bigMapCrossDimCoords"].get<bool>();
        if (j.contains("persistBigMapDimension")) MapRenderState::persistBigMapDimension = j["persistBigMapDimension"].get<bool>();
        if (j.contains("lastViewedDimensionId")) MapRenderState::lastViewedDimensionId = j["lastViewedDimensionId"].get<int>();
        if (j.contains("teleportFailNotifier")) MapRenderState::teleportFailNotifier = j["teleportFailNotifier"].get<bool>();
        if (j.contains("fastWritesEnabled")) MapRenderState::fastWritesEnabled = j["fastWritesEnabled"].get<bool>();
        if (j.contains("writeBatchIntervalMs")) MapRenderState::writeBatchIntervalMs = j["writeBatchIntervalMs"].get<int>();
    }

    bool MigrateDatabase(sqlite3* db, const std::string& worldId) {
        if (!db) return false;

        // 1. 检查 schema_info 表
        const char* sqlSchema = "CREATE TABLE IF NOT EXISTS schema_info (version INTEGER PRIMARY KEY);";
        sqlite3_exec(db, sqlSchema, nullptr, nullptr, nullptr);

        int currentVer = 0;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT version FROM schema_info LIMIT 1;", -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                currentVer = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        // 2. 初始架构创建 (Version 1)
        if (currentVer < 1) {
            const char* sqlInit =
                "CREATE TABLE IF NOT EXISTS chunk_highlights ("
                "  chunk_x INT, chunk_z INT, dim_id INT,"
                "  flags INT, lava_height INT, portal_type INT, timestamp INT64,"
                "  PRIMARY KEY(chunk_x, chunk_z, dim_id)"
                ");"
                "CREATE TABLE IF NOT EXISTS breadcrumbs ("
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "  dim_id INT, x REAL, y REAL, z REAL, timestamp INT64"
                ");"
                "CREATE TABLE IF NOT EXISTS drawings ("
                "  id TEXT PRIMARY KEY, dim_id INT, layer_id TEXT, shape_type INT,"
                "  points_json TEXT, color INT, thickness REAL, text TEXT"
                ");"
                "CREATE TABLE IF NOT EXISTS waypoints ("
                "  id TEXT PRIMARY KEY, dim_id INT, name TEXT, x INT, y INT, z INT,"
                "  r REAL, g REAL, b REAL, enabled INT, pinned INT, folder TEXT, set_name TEXT, created_at INT64"
                ");"
                "CREATE INDEX IF NOT EXISTS idx_chunk_highlights_dim ON chunk_highlights(dim_id);"
                "CREATE INDEX IF NOT EXISTS idx_breadcrumbs_dim ON breadcrumbs(dim_id);";

            sqlite3_exec(db, sqlInit, nullptr, nullptr, nullptr);

            // 3. 数据自动平滑迁移：导入现存的 JSON / 二进制存档
            // 3.1 迁移历史足迹 (Breadcrumbs)
            std::string bcPath = "mods/ChiyanMap/breadcrumbs/" + worldId + "_breadcrumbs.json";
            if (std::filesystem::exists(bcPath)) {
                try {
                    std::ifstream in(bcPath);
                    if (in.is_open()) {
                        json j;
                        in >> j;
                        if (j.is_array()) {
                            sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
                            sqlite3_stmt* insertStmt = nullptr;
                            const char* sqlInsertBc = "INSERT INTO breadcrumbs (dim_id, x, y, z, timestamp) VALUES (?, ?, ?, ?, ?);";
                            if (sqlite3_prepare_v2(db, sqlInsertBc, -1, &insertStmt, nullptr) == SQLITE_OK) {
                                for (const auto& item : j) {
                                    int dim = item.value("dim", 0);
                                    double x = item.value("x", 0.0);
                                    double y = item.value("y", 0.0);
                                    double z = item.value("z", 0.0);
                                    int64_t ts = item.value("t", (int64_t)0);
                                    sqlite3_bind_int(insertStmt, 1, dim);
                                    sqlite3_bind_double(insertStmt, 2, x);
                                    sqlite3_bind_double(insertStmt, 3, y);
                                    sqlite3_bind_double(insertStmt, 4, z);
                                    sqlite3_bind_int64(insertStmt, 5, ts);
                                    sqlite3_step(insertStmt);
                                    sqlite3_reset(insertStmt);
                                }
                                sqlite3_finalize(insertStmt);
                            }
                            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
                        }
                    }
                } catch (...) {}
            }

            // 写入更新后的版本号
            sqlite3_exec(db, "INSERT OR REPLACE INTO schema_info (version) VALUES (1);", nullptr, nullptr, nullptr);
            currentVer = 1;
        }

        g_schemaVersion = currentVer;
        return true;
    }

    bool BeginTransaction() {
        if (!g_db) return false;
        return sqlite3_exec(g_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
    }

    bool CommitTransaction() {
        if (!g_db) return false;
        return sqlite3_exec(g_db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
    }

    bool RollbackTransaction() {
        if (!g_db) return false;
        return sqlite3_exec(g_db, "ROLLBACK;", nullptr, nullptr, nullptr) == SQLITE_OK;
    }

    bool SaveChunkHighlight(int chunkX, int chunkZ, int dimId, uint32_t flags, uint8_t lavaHeight, uint8_t portalType, uint64_t timestamp) {
        if (!g_db || !MapRenderState::useSqliteDatabase) return false;
        std::lock_guard<std::mutex> lock(g_dbMutex);

        const char* sql = "INSERT OR REPLACE INTO chunk_highlights (chunk_x, chunk_z, dim_id, flags, lava_height, portal_type, timestamp) VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, chunkX);
        sqlite3_bind_int(stmt, 2, chunkZ);
        sqlite3_bind_int(stmt, 3, dimId);
        sqlite3_bind_int(stmt, 4, (int)flags);
        sqlite3_bind_int(stmt, 5, (int)lavaHeight);
        sqlite3_bind_int(stmt, 6, (int)portalType);
        sqlite3_bind_int64(stmt, 7, (int64_t)timestamp);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    bool DeleteChunkHighlight(int chunkX, int chunkZ, int dimId) {
        if (!g_db || !MapRenderState::useSqliteDatabase) return false;
        std::lock_guard<std::mutex> lock(g_dbMutex);

        const char* sql = "DELETE FROM chunk_highlights WHERE chunk_x = ? AND chunk_z = ? AND dim_id = ?;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, chunkX);
        sqlite3_bind_int(stmt, 2, chunkZ);
        sqlite3_bind_int(stmt, 3, dimId);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    bool ClearChunkHighlights(int dimId) {
        if (!g_db || !MapRenderState::useSqliteDatabase) return false;
        std::lock_guard<std::mutex> lock(g_dbMutex);

        if (dimId >= 0 && dimId <= 2) {
            const char* sql = "DELETE FROM chunk_highlights WHERE dim_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, dimId);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                return true;
            }
            return false;
        } else {
            return sqlite3_exec(g_db, "DELETE FROM chunk_highlights;", nullptr, nullptr, nullptr) == SQLITE_OK;
        }
    }

    bool LoadAllChunkHighlights(std::function<void(int cx, int cz, int dimId, uint32_t flags, uint8_t lava, uint8_t portal, uint64_t ts)> callback) {
        if (!g_db || !MapRenderState::useSqliteDatabase) return false;
        std::lock_guard<std::mutex> lock(g_dbMutex);

        const char* sql = "SELECT chunk_x, chunk_z, dim_id, flags, lava_height, portal_type, timestamp FROM chunk_highlights;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int cx = sqlite3_column_int(stmt, 0);
            int cz = sqlite3_column_int(stmt, 1);
            int dim = sqlite3_column_int(stmt, 2);
            uint32_t flags = (uint32_t)sqlite3_column_int(stmt, 3);
            uint8_t lava = (uint8_t)sqlite3_column_int(stmt, 4);
            uint8_t portal = (uint8_t)sqlite3_column_int(stmt, 5);
            uint64_t ts = (uint64_t)sqlite3_column_int64(stmt, 6);
            if (callback) callback(cx, cz, dim, flags, lava, portal, ts);
        }
        sqlite3_finalize(stmt);
        return true;
    }

    bool SaveBreadcrumbsBatch(const std::vector<SpecialGridManager::BreadcrumbPoint>& points) {
        if (!g_db || !MapRenderState::useSqliteDatabase) return false;
        std::lock_guard<std::mutex> lock(g_dbMutex);

        sqlite3_exec(g_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        sqlite3_exec(g_db, "DELETE FROM breadcrumbs;", nullptr, nullptr, nullptr);

        const char* sql = "INSERT INTO breadcrumbs (dim_id, x, y, z, timestamp) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            for (const auto& pt : points) {
                sqlite3_bind_int(stmt, 1, pt.dimId);
                sqlite3_bind_double(stmt, 2, (double)pt.x);
                sqlite3_bind_double(stmt, 3, (double)pt.y);
                sqlite3_bind_double(stmt, 4, (double)pt.z);
                sqlite3_bind_int64(stmt, 5, (int64_t)pt.timestamp);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }
        sqlite3_exec(g_db, "COMMIT;", nullptr, nullptr, nullptr);
        return true;
    }

    bool LoadAllBreadcrumbs(std::vector<SpecialGridManager::BreadcrumbPoint>& outPoints) {
        if (!g_db || !MapRenderState::useSqliteDatabase) return false;
        std::lock_guard<std::mutex> lock(g_dbMutex);

        outPoints.clear();
        const char* sql = "SELECT dim_id, x, y, z, timestamp FROM breadcrumbs ORDER BY id ASC;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SpecialGridManager::BreadcrumbPoint pt{};
            pt.dimId = sqlite3_column_int(stmt, 0);
            pt.x = (float)sqlite3_column_double(stmt, 1);
            pt.y = (float)sqlite3_column_double(stmt, 2);
            pt.z = (float)sqlite3_column_double(stmt, 3);
            pt.timestamp = (uint64_t)sqlite3_column_int64(stmt, 4);
            outPoints.push_back(pt);
        }
        sqlite3_finalize(stmt);
        return true;
    }

    bool ClearBreadcrumbs(int dimId) {
        if (!g_db || !MapRenderState::useSqliteDatabase) return false;
        std::lock_guard<std::mutex> lock(g_dbMutex);

        if (dimId >= 0 && dimId <= 2) {
            const char* sql = "DELETE FROM breadcrumbs WHERE dim_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, dimId);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                return true;
            }
            return false;
        } else {
            return sqlite3_exec(g_db, "DELETE FROM breadcrumbs;", nullptr, nullptr, nullptr) == SQLITE_OK;
        }
    }

    std::string ExportWaypointsBackup(const std::string& customPath) {
        std::string exportPath = customPath;
        if (exportPath.empty()) {
            auto now = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
            localtime_s(&tm, &timeT);
            char timeBuf[64];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tm);
            exportPath = "mods/ChiyanMap/backups/waypoints_" + g_currentWorldId + "_" + timeBuf + ".json";
        }

        try {
            std::filesystem::create_directories("mods/ChiyanMap/backups");
            json root = json::object();
            root["format"] = "ChiyanMap_Waypoint_Backup_v1";
            root["worldId"] = g_currentWorldId;
            root["exportTime"] = (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            json arr = json::array();
            {
                std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                for (const auto& wp : WaypointManager::g_waypoints) {
                    if (wp.isTemporary) continue;
                    json w = json::object();
                    w["id"] = wp.id;
                    w["name"] = wp.name;
                    w["x"] = wp.x;
                    w["y"] = wp.y;
                    w["z"] = wp.z;
                    w["r"] = wp.r;
                    w["g"] = wp.g;
                    w["b"] = wp.b;
                    w["enabled"] = wp.enabled;
                    w["dimId"] = wp.dimId;
                    w["pinned"] = wp.pinned;
                    w["folder"] = wp.folder;
                    w["set"] = wp.set;
                    w["createdAt"] = wp.createdAt;
                    arr.push_back(w);
                }
            }
            root["waypoints"] = arr;

            std::ofstream out(exportPath);
            if (out.is_open()) {
                out << root.dump(2);
                s_lastBackupExportedPath = exportPath;
                return exportPath;
            }
        } catch (...) {}

        return "";
    }

    int ImportWaypointsBackup(const std::string& filePath) {
        if (!std::filesystem::exists(filePath)) return -1;
        int count = 0;
        try {
            std::ifstream in(filePath);
            if (!in.is_open()) return -1;
            json root;
            in >> root;

            json arr = json::array();
            if (root.is_object() && root.contains("waypoints") && root["waypoints"].is_array()) {
                arr = root["waypoints"];
            } else if (root.is_array()) {
                arr = root;
            } else {
                return -1;
            }

            for (const auto& w : arr) {
                std::string name = w.value("name", "Waypoint");
                int x = w.value("x", 0);
                int y = w.value("y", 64);
                int z = w.value("z", 0);
                float r = (float)w.value("r", 1.0);
                float g = (float)w.value("g", 1.0);
                float b = (float)w.value("b", 1.0);
                int dimId = w.value("dimId", 0);
                bool pinned = w.value("pinned", false);
                std::string folder = w.value("folder", "");
                std::string set = w.value("set", "gui.chiyan_default");

                WaypointManager::AddWaypoint(name, x, y, z, r, g, b, dimId, pinned, folder, -1, set);
                count++;
            }
            WaypointManager::SaveWaypoints();
        } catch (...) {
            return -1;
        }
        return count;
    }

    bool IsConnected() {
        return g_dbConnected.load() && g_db != nullptr;
    }

    int GetSchemaVersion() {
        return g_schemaVersion;
    }

    std::string GetDatabaseFilePath() {
        return "mods/ChiyanMap/data/" + g_currentWorldId + "_database.db";
    }

    int64_t GetDatabaseSizeBytes() {
        std::error_code ec;
        auto sz = std::filesystem::file_size(GetDatabaseFilePath(), ec);
        return ec ? 0 : (int64_t)sz;
    }

    static bool MatchSettingFilter(const char* filter, std::initializer_list<const char*> keywords) {
        if (!filter || filter[0] == '\0') return true;
        std::string lowerFilter;
        for (const char* p = filter; *p; ++p) {
            lowerFilter += (char)std::tolower((unsigned char)*p);
        }
        while (!lowerFilter.empty() && (lowerFilter.front() == ' ' || lowerFilter.front() == '\t')) lowerFilter.erase(lowerFilter.begin());
        while (!lowerFilter.empty() && (lowerFilter.back() == ' ' || lowerFilter.back() == '\t')) lowerFilter.pop_back();
        if (lowerFilter.empty()) return true;

        for (const char* kw : keywords) {
            if (!kw || kw[0] == '\0') continue;
            std::string lowerKw;
            for (const char* p = kw; *p; ++p) {
                lowerKw += (char)std::tolower((unsigned char)*p);
            }
            if (lowerKw.find(lowerFilter) != std::string::npos) return true;
        }
        return false;
    }

    void RenderSettingsCategory(int& matchCount, const char* searchFilter, float curScale) {
        bool m_sec = MatchSettingFilter(searchFilter, { LanguageManager::GetText("QOL_PERFORMANCE_SETTINGS"), "qol", "performance", "sqlite", "fps", "teleport", "backup", "持久化", "性能", "数据库", "备份" });
        bool m_db = MatchSettingFilter(searchFilter, { LanguageManager::GetText("USE_SQLITE_DATABASE"), LanguageManager::GetText("USE_SQLITE_DATABASE_DESC"), "sqlite", "database", "数据库" });
        bool m_fps = MatchSettingFilter(searchFilter, { LanguageManager::GetText("MINIMAP_FPS_LIMIT"), LanguageManager::GetText("MINIMAP_FPS_LIMIT_DESC"), "fps", "limit", "帧率", "限制" });
        bool m_cross = MatchSettingFilter(searchFilter, { LanguageManager::GetText("CROSS_DIM_COORDS"), LanguageManager::GetText("CROSS_DIM_COORDS_DESC"), "cross", "coords", "换算", "跨维度" });
        bool m_persistDim = MatchSettingFilter(searchFilter, { LanguageManager::GetText("PERSIST_BIGMAP_DIM"), LanguageManager::GetText("PERSIST_BIGMAP_DIM_DESC"), "persist", "dimension", "记住维度", "切换" });
        bool m_tpNotify = MatchSettingFilter(searchFilter, { LanguageManager::GetText("TELEPORT_FAIL_NOTIFIER"), LanguageManager::GetText("TELEPORT_FAIL_NOTIFIER_DESC"), "teleport", "fail", "传送", "提示" });
        bool m_fastW = MatchSettingFilter(searchFilter, { LanguageManager::GetText("FAST_WRITES"), LanguageManager::GetText("FAST_WRITES_DESC"), "fast", "writes", "节流", "写入" });
        bool m_offline = MatchSettingFilter(searchFilter, { LanguageManager::GetText("OFFLINE_MODE_BADGE"), LanguageManager::GetText("OFFLINE_MODE_DESC"), "offline", "离线", "网络", "隐私" });

        bool anyMatch = m_sec || m_db || m_fps || m_cross || m_persistDim || m_tpNotify || m_fastW || m_offline;
        if (!anyMatch) return;

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LanguageManager::GetText("QOL_PERFORMANCE_SETTINGS"));
        ImGui::Separator();

        // 1. SQLite 嵌入式结构化数据库
        if (m_sec || m_db) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("USE_SQLITE_DATABASE"), &MapRenderState::useSqliteDatabase)) {
                LanguageManager::SaveConfig();
                SwitchWorld(g_currentWorldId);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("USE_SQLITE_DATABASE_DESC"));
            }

            ImGui::Indent(15.0f * curScale);
            if (IsConnected()) {
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "%s: %s (v%d)", LanguageManager::GetText("DATABASE_STATUS"), LanguageManager::GetText("DATABASE_CONNECTED"), g_schemaVersion);
                int64_t sz = GetDatabaseSizeBytes();
                if (sz > 0) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("| %.2f MB", (double)sz / (1024.0 * 1024.0));
                }
            } else {
                ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.35f, 1.0f), "%s: %s", LanguageManager::GetText("DATABASE_STATUS"), LanguageManager::GetText("DATABASE_DISABLED"));
            }
            ImGui::Unindent(15.0f * curScale);
        }

        // 2. 小地图独立刷新率限制 (Minimap FPS Limiter)
        if (m_sec || m_fps) {
            matchCount++;
            const char* fpsModes[] = {
                LanguageManager::GetText("FPS_UNLIMITED"),
                "60 FPS",
                "30 FPS",
                "20 FPS",
                "15 FPS"
            };
            const int fpsValues[] = { 0, 60, 30, 20, 15 };
            int currentIdx = 0;
            for (int i = 0; i < 5; ++i) {
                if (MapRenderState::minimapFpsLimit == fpsValues[i]) {
                    currentIdx = i;
                    break;
                }
            }

            ImGui::SetNextItemWidth(160.0f * curScale);
            if (ImGui::Combo(LanguageManager::GetText("MINIMAP_FPS_LIMIT"), &currentIdx, fpsModes, 5)) {
                MapRenderState::minimapFpsLimit = fpsValues[currentIdx];
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("MINIMAP_FPS_LIMIT_DESC"));
            }
        }

        // 3. 大地图跨维度光标换算
        if (m_sec || m_cross) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("CROSS_DIM_COORDS"), &MapRenderState::bigMapCrossDimCoords)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("CROSS_DIM_COORDS_DESC"));
            }
        }

        // 4. 持久大地图查看维度
        if (m_sec || m_persistDim) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("PERSIST_BIGMAP_DIM"), &MapRenderState::persistBigMapDimension)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("PERSIST_BIGMAP_DIM_DESC"));
            }
        }

        // 5. 传送失败检测提示
        if (m_sec || m_tpNotify) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("TELEPORT_FAIL_NOTIFIER"), &MapRenderState::teleportFailNotifier)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("TELEPORT_FAIL_NOTIFIER_DESC"));
            }
        }

        // 6. 高性能写入节流控制
        if (m_sec || m_fastW) {
            matchCount++;
            if (ImGui::Checkbox(LanguageManager::GetText("FAST_WRITES"), &MapRenderState::fastWritesEnabled)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("FAST_WRITES_DESC"));
            }
        }

        // 7. 通用航点备份与导入导出
        {
            matchCount++;
            if (ImGui::Button(LanguageManager::GetText("WAYPOINT_BACKUP_EXPORT"))) {
                std::string path = ExportWaypointsBackup();
                if (!path.empty()) {
                    s_lastImportStatusMsg = LanguageManager::GetText("WAYPOINT_BACKUP_SUCCESS");
                    s_lastImportStatusTime = ImGui::GetTime();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(LanguageManager::GetText("WAYPOINT_BACKUP_IMPORT"))) {
                // 如果有上一次导出的备份文件，直接导入测试或导入指定路径
                if (!s_lastBackupExportedPath.empty() && std::filesystem::exists(s_lastBackupExportedPath)) {
                    int c = ImportWaypointsBackup(s_lastBackupExportedPath);
                    if (c >= 0) {
                        char buf[128];
                        snprintf(buf, sizeof(buf), LanguageManager::GetText("WAYPOINT_IMPORT_SUCCESS"), c);
                        s_lastImportStatusMsg = buf;
                    } else {
                        s_lastImportStatusMsg = LanguageManager::GetText("WAYPOINT_IMPORT_FAIL");
                    }
                    s_lastImportStatusTime = ImGui::GetTime();
                } else {
                    s_lastImportStatusMsg = LanguageManager::GetText("WAYPOINT_IMPORT_FAIL");
                    s_lastImportStatusTime = ImGui::GetTime();
                }
            }

            if (!s_lastImportStatusMsg.empty() && (ImGui::GetTime() - s_lastImportStatusTime < 5.0f)) {
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "%s", s_lastImportStatusMsg.c_str());
            }
        }

        // 8. 纯净离线安全状态标志
        if (m_sec || m_offline) {
            matchCount++;
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 0.85f), "%s", LanguageManager::GetText("OFFLINE_MODE_BADGE"));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", LanguageManager::GetText("OFFLINE_MODE_DESC"));
            }
        }

        ImGui::Spacing();
    }

} // namespace DatabaseManager
