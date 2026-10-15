#include "state/MultiworldManager.h"
#include "state/MapCacheManager.h"
#include "state/WaypointManager.h"
#include "state/DrawingManager.h"
#include "state/LanguageManager.h"
#include "mod/ChiyanMap.h"

#include <chrono>
#include <fstream>
#include <random>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace MultiworldManager {

    std::mutex g_mwMutex;
    std::string g_currentWorldId = "";
    std::string g_activeMapId = "default";
    std::string g_viewedMapId = "default";
    MapSelectionMode g_selectionMode = MapSelectionMode::Manual;
    std::vector<MapInstance> g_mapInstances;
    static std::vector<std::function<void(const std::string&)>> s_changeCallbacks;

    static uint64_t GetCurrentTimeMs() {
        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    static std::string GenerateUniqueId() {
        static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(0, (int)(sizeof(alphanum) - 2));
        std::string s = "map_";
        for (int i = 0; i < 8; ++i) {
            s += alphanum[dist(rng)];
        }
        return s;
    }

    static std::filesystem::path GetConfigFile(const std::string& worldId) {
        auto baseDataDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir();
        return baseDataDir / "cache" / worldId / "multiworld.json";
    }

    std::filesystem::path GetMapCacheDir(const std::string& worldId, const std::string& mapId, int dimId) {
        auto baseDataDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir();
        auto worldDir = baseDataDir / "cache" / worldId;
        if (mapId.empty() || mapId == "default") {
            return worldDir / ("dim_" + std::to_string(dimId));
        } else {
            return worldDir / "maps" / mapId / ("dim_" + std::to_string(dimId));
        }
    }

    void Init() {
        // 初始状态
    }

    void LoadConfig() {
        if (g_currentWorldId.empty()) return;
        auto cfgPath = GetConfigFile(g_currentWorldId);
        g_mapInstances.clear();

        if (std::filesystem::exists(cfgPath)) {
            try {
                std::ifstream f(cfgPath);
                if (f) {
                    json j;
                    f >> j;
                    g_selectionMode = (MapSelectionMode)j.value("selectionMode", (int)MapSelectionMode::Manual);
                    g_activeMapId = j.value("activeMapId", "default");

                    if (j.contains("maps") && j["maps"].is_array()) {
                        for (const auto& item : j["maps"]) {
                            MapInstance mi;
                            mi.id = item.value("id", "");
                            mi.name = item.value("name", "Map");
                            mi.spawnX = item.value("spawnX", 0);
                            mi.spawnZ = item.value("spawnZ", 0);
                            mi.createdAt = item.value("createdAt", 0ULL);
                            mi.lastUsedAt = item.value("lastUsedAt", 0ULL);
                            if (item.contains("connectedMaps") && item["connectedMaps"].is_array()) {
                                for (const auto& c : item["connectedMaps"]) {
                                    if (c.is_string()) mi.connectedMaps.insert(c.get<std::string>());
                                }
                            }
                            if (!mi.id.empty()) {
                                g_mapInstances.push_back(mi);
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        // 确保默认地图始终存在
        bool hasDefault = false;
        for (const auto& m : g_mapInstances) {
            if (m.id == "default") {
                hasDefault = true;
                break;
            }
        }

        if (!hasDefault) {
            MapInstance def;
            def.id = "default";
            def.name = LanguageManager::GetText("MAP_DEFAULT_NAME");
            def.createdAt = GetCurrentTimeMs();
            def.lastUsedAt = def.createdAt;
            def.connectedMaps.insert("default");
            g_mapInstances.insert(g_mapInstances.begin(), def);
        }

        // 检查 activeMapId 是否合法
        bool activeValid = false;
        for (auto& m : g_mapInstances) {
            if (m.id == g_activeMapId) {
                activeValid = true;
                m.lastUsedAt = GetCurrentTimeMs();
                break;
            }
        }
        if (!activeValid) {
            g_activeMapId = "default";
        }
        g_viewedMapId = g_activeMapId;
    }

    static bool IsBuiltinDefaultMapName(const std::string& name) {
        if (name.empty() || name == "MAP_DEFAULT_NAME") return true;
        static const char* const s_defaultNames[] = {
            "默认地图", "預設地圖", "Default Map", "Standardkarte",
            "Mapa predeterminado", "Carte par défaut", "Peta Bawaan", "Mappa predefinita",
            "デフォルトマップ", "기본 지도", "Mapa Padrão", "Карта по умолчанию",
            "แผนที่เริ่มต้น", "Varsayılan Harita", "Стандартна мапа", "Bản đồ mặc định"
        };
        for (const char* s : s_defaultNames) {
            if (name == s) return true;
        }
        return false;
    }

    static std::string ResolveMapDisplayName(const MapInstance& m) {
        if (m.id == "default" && IsBuiltinDefaultMapName(m.name)) {
            return LanguageManager::GetText("MAP_DEFAULT_NAME");
        }
        return m.name;
    }

    void SaveConfig() {
        if (g_currentWorldId.empty()) return;
        auto cfgPath = GetConfigFile(g_currentWorldId);
        try {
            std::filesystem::create_directories(cfgPath.parent_path());
            json j;
            j["selectionMode"] = (int)g_selectionMode;
            j["activeMapId"] = g_activeMapId;
            j["maps"] = json::array();

            for (const auto& m : g_mapInstances) {
                json item;
                item["id"] = m.id;
                item["name"] = m.name;
                item["spawnX"] = m.spawnX;
                item["spawnZ"] = m.spawnZ;
                item["createdAt"] = m.createdAt;
                item["lastUsedAt"] = m.lastUsedAt;
                item["connectedMaps"] = json::array();
                for (const auto& c : m.connectedMaps) {
                    item["connectedMaps"].push_back(c);
                }
                j["maps"].push_back(item);
            }

            std::ofstream f(cfgPath);
            if (f) {
                f << j.dump(2);
            }
        } catch (...) {}
    }

    void SwitchWorld(const std::string& worldId, int spawnX, int spawnZ) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        if (g_currentWorldId != worldId) {
            g_currentWorldId = worldId;
            LoadConfig();
            SaveConfig();
        }
        g_viewedMapId = g_activeMapId;

        if (g_selectionMode == MapSelectionMode::WorldSpawn && (spawnX != 0 || spawnZ != 0)) {
            CheckWorldSpawn(spawnX, spawnZ);
        }
    }

    std::string GetActiveMapId() {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        if (g_activeMapId.empty()) return "default";
        return g_activeMapId;
    }

    std::string GetActiveMapName() {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        for (const auto& m : g_mapInstances) {
            if (m.id == g_activeMapId) {
                return ResolveMapDisplayName(m);
            }
        }
        return LanguageManager::GetText("MAP_DEFAULT_NAME");
    }

    std::string GetViewedMapId() {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        if (g_viewedMapId.empty()) return g_activeMapId.empty() ? "default" : g_activeMapId;
        return g_viewedMapId;
    }

    std::string GetViewedMapName() {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        std::string targetId = g_viewedMapId.empty() ? g_activeMapId : g_viewedMapId;
        for (const auto& m : g_mapInstances) {
            if (m.id == targetId) {
                return ResolveMapDisplayName(m);
            }
        }
        return LanguageManager::GetText("MAP_DEFAULT_NAME");
    }

    void SetViewedMapId(const std::string& mapId) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        g_viewedMapId = mapId.empty() ? "default" : mapId;
    }

    bool SetActiveMapId(const std::string& mapId, bool notifyReload) {
        std::string oldId;
        std::string newId;
        {
            std::lock_guard<std::mutex> lock(g_mwMutex);
            bool found = false;
            for (auto& m : g_mapInstances) {
                if (m.id == mapId) {
                    found = true;
                    m.lastUsedAt = GetCurrentTimeMs();
                    break;
                }
            }
            if (!found) return false;

            oldId = g_activeMapId;
            g_activeMapId = mapId;
            g_viewedMapId = mapId;
            newId = mapId;
            SaveConfig();
        }

        if (notifyReload && oldId != newId) {
            // 通知缓存系统与路径点系统切换
            MapCacheManager::SwitchMap(newId);
            WaypointManager::SwitchMap(newId);
            DrawingManager::SwitchMap(newId);

            for (auto& cb : s_changeCallbacks) {
                if (cb) cb(newId);
            }
        }
        return true;
    }

    std::vector<MapInstance> GetMapList() {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        std::vector<MapInstance> list = g_mapInstances;
        for (auto& m : list) {
            m.name = ResolveMapDisplayName(m);
        }
        return list;
    }

    bool CreateMap(const std::string& name, const std::string& customId) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        if (name.empty()) return false;

        std::string id = customId.empty() ? GenerateUniqueId() : customId;
        // 校验唯一性
        for (const auto& m : g_mapInstances) {
            if (m.id == id) return false;
        }

        MapInstance mi;
        mi.id = id;
        mi.name = name;
        mi.createdAt = GetCurrentTimeMs();
        mi.lastUsedAt = mi.createdAt;
        mi.connectedMaps.insert(id); // 自身与自身互联
        // 默认与 activeMapId 互联，方便初始平滑过渡
        if (!g_activeMapId.empty()) {
            mi.connectedMaps.insert(g_activeMapId);
            for (auto& cur : g_mapInstances) {
                if (cur.id == g_activeMapId) {
                    cur.connectedMaps.insert(id);
                    break;
                }
            }
        }

        g_mapInstances.push_back(mi);
        SaveConfig();
        return true;
    }

    bool RenameMap(const std::string& mapId, const std::string& newName) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        if (newName.empty()) return false;
        for (auto& m : g_mapInstances) {
            if (m.id == mapId) {
                m.name = newName;
                SaveConfig();
                return true;
            }
        }
        return false;
    }

    bool DeleteMap(const std::string& mapId) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        if (mapId == "default" || mapId == g_activeMapId) {
            return false; // 禁止删除默认地图或当前活动地图
        }

        auto it = std::find_if(g_mapInstances.begin(), g_mapInstances.end(), [&](const MapInstance& m) {
            return m.id == mapId;
        });
        if (it == g_mapInstances.end()) return false;

        g_mapInstances.erase(it);

        // 清理其他地图对其的互联引用
        for (auto& m : g_mapInstances) {
            m.connectedMaps.erase(mapId);
        }

        SaveConfig();

        // 备份并删除地图磁盘数据
        try {
            auto baseDataDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir();
            auto mapDir = baseDataDir / "cache" / g_currentWorldId / "maps" / mapId;
            auto backupDir = baseDataDir / "cache" / g_currentWorldId / "maps_backup" / mapId;
            if (std::filesystem::exists(mapDir)) {
                std::error_code ec;
                std::filesystem::create_directories(backupDir.parent_path(), ec);
                std::filesystem::copy(mapDir, backupDir, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
                std::filesystem::remove_all(mapDir, ec);
            }
            // 同时清理对应地图独立的路径点文件
            for (int d = 0; d <= 2; ++d) {
                std::string wpFile = "mods/ChiyanMap/waypoints/" + g_currentWorldId + "_" + mapId + "_dim" + std::to_string(d) + ".json";
                std::error_code ec2;
                std::filesystem::remove(wpFile, ec2);
            }
        } catch (...) {}

        return true;
    }

    void ConnectMaps(const std::string& mapId1, const std::string& mapId2) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        for (auto& m : g_mapInstances) {
            if (m.id == mapId1) m.connectedMaps.insert(mapId2);
            if (m.id == mapId2) m.connectedMaps.insert(mapId1);
        }
        SaveConfig();
    }

    void DisconnectMaps(const std::string& mapId1, const std::string& mapId2) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        for (auto& m : g_mapInstances) {
            if (m.id == mapId1) m.connectedMaps.erase(mapId2);
            if (m.id == mapId2) m.connectedMaps.erase(mapId1);
        }
        SaveConfig();
    }

    bool AreMapsConnected(const std::string& mapId1, const std::string& mapId2) {
        if (mapId1 == mapId2) return true;
        std::lock_guard<std::mutex> lock(g_mwMutex);
        for (const auto& m : g_mapInstances) {
            if (m.id == mapId1) {
                return m.connectedMaps.count(mapId2) > 0;
            }
        }
        return false;
    }

    MapSelectionMode GetSelectionMode() {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        return g_selectionMode;
    }

    void SetSelectionMode(MapSelectionMode mode) {
        std::lock_guard<std::mutex> lock(g_mwMutex);
        g_selectionMode = mode;
        if (mode == MapSelectionMode::Single) {
            g_activeMapId = "default";
        }
        SaveConfig();
    }

    void CheckWorldSpawn(int spawnX, int spawnZ) {
        if (g_selectionMode != MapSelectionMode::WorldSpawn) return;
        if (spawnX == 0 && spawnZ == 0) return;

        std::string targetMapId = "";
        {
            std::lock_guard<std::mutex> lock(g_mwMutex);
            for (const auto& m : g_mapInstances) {
                if (m.spawnX == spawnX && m.spawnZ == spawnZ) {
                    targetMapId = m.id;
                    break;
                }
            }
        }

        if (targetMapId.empty()) {
            // 自动新建 Spawn 关联地图
            std::string autoName = "Spawn (" + std::to_string(spawnX) + ", " + std::to_string(spawnZ) + ")";
            std::string autoId = "spawn_" + std::to_string(spawnX) + "_" + std::to_string(spawnZ);
            CreateMap(autoName, autoId);
            {
                std::lock_guard<std::mutex> lock(g_mwMutex);
                for (auto& m : g_mapInstances) {
                    if (m.id == autoId) {
                        m.spawnX = spawnX;
                        m.spawnZ = spawnZ;
                        break;
                    }
                }
                SaveConfig();
            }
            targetMapId = autoId;
        }

        if (!targetMapId.empty() && targetMapId != g_activeMapId) {
            SetActiveMapId(targetMapId, true);
        }
    }

    int GetMapRegionCount(const std::string& mapId, int dimId) {
        if (g_currentWorldId.empty()) return 0;
        int total = 0;
        std::vector<int> dimsToCheck;
        if (dimId >= 0) dimsToCheck.push_back(dimId);
        else { dimsToCheck.push_back(0); dimsToCheck.push_back(1); dimsToCheck.push_back(2); }

        for (int d : dimsToCheck) {
            auto dir = GetMapCacheDir(g_currentWorldId, mapId, d);
            std::error_code ec;
            if (std::filesystem::is_directory(dir, ec)) {
                for (auto const& entry : std::filesystem::directory_iterator(dir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    std::string fn = entry.path().filename().string();
                    if (fn.rfind("region_", 0) == 0 && fn.ends_with(".bin")) {
                        total++;
                    }
                }
            }
            // 洞穴目录
            auto caveDir = dir / "cave";
            if (std::filesystem::is_directory(caveDir, ec)) {
                for (auto const& entry : std::filesystem::directory_iterator(caveDir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    std::string fn = entry.path().filename().string();
                    if (fn.rfind("region_", 0) == 0 && fn.ends_with(".bin")) {
                        total++;
                    }
                }
            }
        }
        return total;
    }

    void RegisterMapChangeCallback(std::function<void(const std::string&)> cb) {
        s_changeCallbacks.push_back(std::move(cb));
    }
}
