#include "state/WaypointManager.h"
#include "state/MultiworldManager.h"
#include "state/LanguageManager.h"
#include "state/MapRenderState.h"
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace WaypointManager {
    std::vector<Waypoint> g_waypoints;
    std::set<std::string> g_customFolders;
    std::set<std::string> g_customSets;
    std::mutex g_wpMutex;
    Waypoint g_lastDeletedWaypoint;
    bool g_hasDeletedWaypoint = false;
    std::vector<Waypoint> g_deletedWaypoints;
    constexpr size_t kMaxDeletedWaypoints = 64;
    
    std::string g_worldId = "";
    std::string g_currentMapId = "default";
    int g_currentDim = -999; // 玩家当前所处维度 (0=主世界 1=下界 2=末地)

    std::string GenerateID() {
        static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(0, (int)(sizeof(alphanum) - 2));
        std::string tmp_s;
        tmp_s.reserve(8);
        for (int i = 0; i < 8; ++i) {
            tmp_s += alphanum[dist(rng)];
        }
        return tmp_s;
    }

    // 生成某维度的路径点存档文件名
    std::string DimensionFile(int dim) {
        if (g_currentMapId.empty() || g_currentMapId == "default") {
            return "mods/ChiyanMap/waypoints/" + g_worldId + "_dim" + std::to_string(dim) + ".json";
        }
        return "mods/ChiyanMap/waypoints/" + g_worldId + "_" + g_currentMapId + "_dim" + std::to_string(dim) + ".json";
    }

    // 生成自定义文件夹集合存档文件名
    std::string FoldersFile() {
        if (g_currentMapId.empty() || g_currentMapId == "default") {
            return "mods/ChiyanMap/waypoints/" + g_worldId + "_folders.json";
        }
        return "mods/ChiyanMap/waypoints/" + g_worldId + "_" + g_currentMapId + "_folders.json";
    }

    // 生成路径点集合(Waypoint Sets)存档文件名
    std::string SetsFile() {
        if (g_currentMapId.empty() || g_currentMapId == "default") {
            return "mods/ChiyanMap/waypoints/" + g_worldId + "_sets.json";
        }
        return "mods/ChiyanMap/waypoints/" + g_worldId + "_" + g_currentMapId + "_sets.json";
    }

    // 读取单个维度的路径点并标记所属维度
    void LoadDimension(int dim) {
        std::string f = DimensionFile(dim);
        if (!std::filesystem::exists(f)) {
            std::string rawLevelId = g_worldId;
            size_t sPos = rawLevelId.find("_S");
            if (sPos != std::string::npos) rawLevelId = rawLevelId.substr(0, sPos);
            std::error_code ec;
            std::string wpDir = "mods/ChiyanMap/waypoints";
            if (std::filesystem::is_directory(wpDir, ec)) {
                for (auto const& entry : std::filesystem::directory_iterator(wpDir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    std::string fn = entry.path().filename().string();
                    std::string targetSuffix = "_dim" + std::to_string(dim) + ".json";
                    if (fn.rfind(rawLevelId, 0) == 0 && fn.ends_with(targetSuffix)) {
                        f = entry.path().string();
                        break;
                    }
                }
            }
        }
        if (!std::filesystem::exists(f)) return;
        std::ifstream in(f);
        if (!in.is_open()) return;
        try {
            json j;
            in >> j;
            for (const auto& item : j) {
                Waypoint wp;
                wp.id = item.value("id", GenerateID());
                wp.name = item.value("name", "New Waypoint");
                wp.x = item.value("x", 0);
                wp.y = item.value("y", 0);
                wp.z = item.value("z", 0);
                wp.r = item.value("r", 1.0f);
                wp.g = item.value("g", 1.0f);
                wp.b = item.value("b", 1.0f);
                wp.enabled = item.value("enabled", true);
                wp.dimId = item.value("dimId", dim); // 兼容旧存档
                wp.pinned = item.value("pinned", false);
                wp.isTemporary = item.value("isTemporary", false);
                wp.folder = item.value("folder", "");
                wp.set = item.value("set", "");
                if (wp.set.empty() || wp.set.find("_default") != std::string::npos) {
                    wp.set = "gui.chiyan_default";
                }
                wp.createdAt = item.value("createdAt", (uint64_t)0);
                wp.order = item.value("order", (int)g_waypoints.size());
                wp.isCave = item.value("isCave", (int8_t)-1);
                if (!wp.folder.empty()) {
                    g_customFolders.insert(wp.folder);
                }
                if (!wp.set.empty() && wp.set != "gui.chiyan_default") {
                    g_customSets.insert(wp.set);
                }
                g_waypoints.push_back(wp);
            }
        } catch (...) {
            // 防止因 json 损坏导致游戏崩溃
            printf("[ChiyanMap] Waypoints JSON 解析失败！\n");
        }
    }

    void SaveWaypoints() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        if (g_worldId.empty()) return;
        // 按维度分组后分别写入对应文件
        std::vector<json> groups(3);
        for (int d = 0; d < 3; ++d) groups[d] = json::array();
        for (const auto& wp : g_waypoints) {
            json obj;
            obj["id"] = wp.id;
            obj["name"] = wp.name;
            obj["x"] = wp.x;
            obj["y"] = wp.y;
            obj["z"] = wp.z;
            obj["r"] = wp.r;
            obj["g"] = wp.g;
            obj["b"] = wp.b;
            obj["enabled"] = wp.enabled;
            obj["pinned"] = wp.pinned;
            obj["isTemporary"] = wp.isTemporary;
            obj["folder"] = wp.folder;
            obj["set"] = wp.set;
            obj["createdAt"] = wp.createdAt;
            obj["order"] = wp.order;
            obj["isCave"] = (int)wp.isCave;
            int d = (wp.dimId >= 0 && wp.dimId < 3) ? wp.dimId : g_currentDim;
            if (d >= 0 && d < 3) {
                groups[d].push_back(obj);
            }
        }

        std::filesystem::create_directories("mods/ChiyanMap/waypoints");
        for (int d = 0; d < 3; ++d) {
            std::ofstream out(DimensionFile(d));
            if (out.is_open()) {
                out << groups[d].dump(4); // 格式化为带有 4 个空格缩进的漂亮 JSON
                out.close();
            }
        }

        // 保存独立文件夹配置
        std::set<std::string> allFolders = g_customFolders;
        for (const auto& wp : g_waypoints) {
            if (!wp.folder.empty()) allFolders.insert(wp.folder);
        }
        json fList = json::array();
        for (const auto& f : allFolders) {
            if (!f.empty()) fList.push_back(f);
        }
        std::ofstream fOut(FoldersFile());
        if (fOut.is_open()) {
            fOut << fList.dump(4);
            fOut.close();
        }

        // 保存路径点集合配置
        std::set<std::string> allSets = g_customSets;
        for (const auto& wp : g_waypoints) {
            if (!wp.set.empty() && wp.set != "gui.chiyan_default") allSets.insert(wp.set);
        }
        json sList = json::array();
        for (const auto& s : allSets) {
            if (!s.empty()) sList.push_back(s);
        }
        std::ofstream sOut(SetsFile());
        if (sOut.is_open()) {
            sOut << sList.dump(4);
            sOut.close();
        }
    }

    void LoadWaypoints() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        g_waypoints.clear();
        g_customFolders.clear();
        g_customSets.clear();

        // 加载文件夹元数据
        std::string fPath = FoldersFile();
        if (!std::filesystem::exists(fPath)) {
            std::string rawLevelId = g_worldId;
            size_t sPos = rawLevelId.find("_S");
            if (sPos != std::string::npos) rawLevelId = rawLevelId.substr(0, sPos);
            std::error_code ec;
            std::string wpDir = "mods/ChiyanMap/waypoints";
            if (std::filesystem::is_directory(wpDir, ec)) {
                for (auto const& entry : std::filesystem::directory_iterator(wpDir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    std::string fn = entry.path().filename().string();
                    if (fn.rfind(rawLevelId, 0) == 0 && fn.ends_with("_folders.json")) {
                        fPath = entry.path().string();
                        break;
                    }
                }
            }
        }
        if (std::filesystem::exists(fPath)) {
            try {
                std::ifstream fIn(fPath);
                if (fIn.is_open()) {
                    json fList;
                    fIn >> fList;
                    for (const auto& item : fList) {
                        if (item.is_string()) {
                            std::string s = item.get<std::string>();
                            if (!s.empty()) {
                                g_customFolders.insert(s);
                                g_customSets.insert(s);
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        // 加载集合元数据 (若存在则覆盖合并)
        std::string sPath = SetsFile();
        if (!std::filesystem::exists(sPath)) {
            std::string rawLevelId = g_worldId;
            size_t sPos = rawLevelId.find("_S");
            if (sPos != std::string::npos) rawLevelId = rawLevelId.substr(0, sPos);
            std::error_code ec;
            std::string wpDir = "mods/ChiyanMap/waypoints";
            if (std::filesystem::is_directory(wpDir, ec)) {
                for (auto const& entry : std::filesystem::directory_iterator(wpDir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    std::string fn = entry.path().filename().string();
                    if (fn.rfind(rawLevelId, 0) == 0 && fn.ends_with("_sets.json")) {
                        sPath = entry.path().string();
                        break;
                    }
                }
            }
        }
        if (std::filesystem::exists(sPath)) {
            try {
                std::ifstream sIn(sPath);
                if (sIn.is_open()) {
                    json sList;
                    sIn >> sList;
                    for (const auto& item : sList) {
                        if (item.is_string()) {
                            std::string s = item.get<std::string>();
                            if (!s.empty() && s.find("_default") == std::string::npos && s != "gui.chiyan_default") {
                                g_customSets.insert(s);
                                g_customFolders.insert(s);
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        // 一次性载入全部三个维度，由 UI 标签筛选显示
        for (int d = 0; d < 3; ++d) LoadDimension(d);
    }

    void Init() {
        std::filesystem::create_directories("mods/ChiyanMap/waypoints");
    }

    void SwitchWorld(const std::string& worldId, int dimensionId) {
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            std::filesystem::create_directories("mods/ChiyanMap/waypoints");
            if (g_worldId != worldId) {
                g_deletedWaypoints.clear();
                g_hasDeletedWaypoint = false;
            }
            g_worldId = worldId;
            g_currentMapId = MultiworldManager::GetActiveMapId();
            g_currentDim = dimensionId; // 记录“玩家当前所处维度”，用于新路径点归属
        }
        LoadWaypoints();
    }

    void SwitchMap(const std::string& mapId) {
        SaveWaypoints();
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            if (g_currentMapId != mapId) {
                g_deletedWaypoints.clear();
                g_hasDeletedWaypoint = false;
            }
            g_currentMapId = mapId;
        }
        LoadWaypoints();
    }

    std::string GetCurrentMapId() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        return g_currentMapId.empty() ? "default" : g_currentMapId;
    }

    std::string AddWaypoint(const std::string& name, int x, int y, int z, float r, float g, float b, int dimId, bool pinned, const std::string& folder, int8_t isCave, const std::string& set) {
        Waypoint wp;
        wp.id = GenerateID();
        wp.name = name;
        wp.x = x; wp.y = y; wp.z = z;
        wp.r = r; wp.g = g; wp.b = b;
        wp.enabled = true;
        wp.dimId = (dimId >= 0 && dimId < 3) ? dimId : g_currentDim; // 默认归属玩家当前维度
        wp.pinned = pinned;
        std::string finalSet = set;
        if (finalSet.empty() || finalSet.find("_default") != std::string::npos) {
            finalSet = "gui.chiyan_default";
        }
        wp.set = finalSet;
        wp.folder = folder;
        wp.isCave = isCave;
        wp.createdAt = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            wp.order = (int)g_waypoints.size();
            if (!wp.folder.empty()) g_customFolders.insert(wp.folder);
            if (!wp.set.empty() && wp.set != "gui.chiyan_default") g_customSets.insert(wp.set);
            g_waypoints.push_back(wp);
        }
        SaveWaypoints();
        return wp.id;
    }

    void RemoveWaypoint(const std::string& id) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::find_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) { return w.id == id; });
            if (it != g_waypoints.end()) {
                g_lastDeletedWaypoint = *it;
                g_deletedWaypoints.push_back(*it);
                if (g_deletedWaypoints.size() > kMaxDeletedWaypoints) {
                    g_deletedWaypoints.erase(g_deletedWaypoints.begin());
                }
                g_hasDeletedWaypoint = true;
                g_waypoints.erase(it);
                changed = true;
            }
        }
        // 在锁外执行持久化，避免底层死锁
        if (changed) SaveWaypoints();
    }

    void RemoveWaypoints(const std::set<std::string>& ids) {
        if (ids.empty()) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            for (const auto& w : g_waypoints) {
                if (ids.count(w.id) > 0) {
                    g_lastDeletedWaypoint = w;
                    g_deletedWaypoints.push_back(w);
                    if (g_deletedWaypoints.size() > kMaxDeletedWaypoints) {
                        g_deletedWaypoints.erase(g_deletedWaypoints.begin());
                    }
                    g_hasDeletedWaypoint = true;
                }
            }
            auto it = std::remove_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) {
                    return ids.count(w.id) > 0;
                });
            if (it != g_waypoints.end()) {
                g_waypoints.erase(it, g_waypoints.end());
                changed = true;
            }
        }
        if (changed) SaveWaypoints();
    }

    void ToggleWaypoint(const std::string& id) {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        for (auto& wp : g_waypoints) {
            if (wp.id == id) {
                wp.enabled = !wp.enabled;
                break;
            }
        }
    }

    void ToggleWaypointPin(const std::string& id) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            for (auto& wp : g_waypoints) {
                if (wp.id == id) {
                    wp.pinned = !wp.pinned;
                    changed = true;
                    break;
                }
            }
        }
        if (changed) SaveWaypoints();
    }

    void SetAllWaypointsEnabled(bool enabled, const std::vector<std::string>& targetIds) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            if (targetIds.empty()) {
                for (auto& wp : g_waypoints) {
                    if (wp.enabled != enabled) {
                        wp.enabled = enabled;
                        changed = true;
                    }
                }
            } else {
                std::unordered_set<std::string> idSet(targetIds.begin(), targetIds.end());
                for (auto& wp : g_waypoints) {
                    if (idSet.count(wp.id) > 0) {
                        if (wp.enabled != enabled) {
                            wp.enabled = enabled;
                            changed = true;
                        }
                    }
                }
            }
        }
        if (changed) SaveWaypoints();
    }

    void SetWaypointsEnabled(bool enabled, const std::set<std::string>& targetIds) {
        if (targetIds.empty()) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            for (auto& wp : g_waypoints) {
                if (targetIds.count(wp.id) > 0) {
                    if (wp.enabled != enabled) {
                        wp.enabled = enabled;
                        changed = true;
                    }
                }
            }
        }
        if (changed) SaveWaypoints();
    }

    void UpdateWaypoint(const std::string& id, const std::string& name, int x, int y, int z, float r, float g, float b, bool enabled, bool pinned, const std::string& folder, int8_t isCave, int dimId, const std::string& set) {
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::find_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) { return w.id == id; });
            if (it == g_waypoints.end()) return;
            it->name = name;
            it->x = x; it->y = y; it->z = z;
            it->r = r; it->g = g; it->b = b;
            it->enabled = enabled;
            it->pinned = pinned;
            std::string finalSet = set;
            if (finalSet.empty() || finalSet.find("_default") != std::string::npos) {
                finalSet = "gui.chiyan_default";
            }
            it->set = finalSet;
            it->folder = folder;
            it->isCave = isCave;
            if (dimId >= 0 && dimId < 3) {
                it->dimId = dimId;
            }
            if (!it->folder.empty()) g_customFolders.insert(it->folder);
            if (!it->set.empty() && it->set != "gui.chiyan_default") g_customSets.insert(it->set);
        }
        SaveWaypoints();
    }

    bool RestoreLastDeletedWaypoint() {
        bool restored = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            if (!g_deletedWaypoints.empty()) {
                Waypoint wp = g_deletedWaypoints.back();
                g_deletedWaypoints.pop_back();
                // 防止万一已有同 ID 的路径点
                auto dup = std::find_if(g_waypoints.begin(), g_waypoints.end(),
                    [&](const Waypoint& w) { return w.id == wp.id; });
                if (dup != g_waypoints.end()) {
                    wp.id = GenerateID();
                }
                if (!wp.folder.empty()) {
                    g_customFolders.insert(wp.folder);
                }
                g_waypoints.push_back(wp);
                if (!g_deletedWaypoints.empty()) {
                    g_lastDeletedWaypoint = g_deletedWaypoints.back();
                    g_hasDeletedWaypoint = true;
                } else {
                    g_hasDeletedWaypoint = false;
                }
                restored = true;
            } else if (g_hasDeletedWaypoint) {
                g_waypoints.push_back(g_lastDeletedWaypoint);
                g_hasDeletedWaypoint = false;
                restored = true;
            }
        }
        if (restored) SaveWaypoints();
        return restored;
    }

    int GetDeletedWaypointCount() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        if (!g_deletedWaypoints.empty()) {
            return (int)g_deletedWaypoints.size();
        }
        return g_hasDeletedWaypoint ? 1 : 0;
    }

    std::string GetLastDeletedWaypointName() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        if (!g_deletedWaypoints.empty()) {
            return g_deletedWaypoints.back().name;
        }
        return g_hasDeletedWaypoint ? g_lastDeletedWaypoint.name : "";
    }

    int GetWaypointCount() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        return (int)g_waypoints.size();
    }

    std::string SetTemporaryWaypoint(int x, int y, int z, int dimId, int8_t isCave) {
        int targetDim = (dimId >= 0 && dimId < 3) ? dimId : g_currentDim;
        std::string resId;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            for (auto& wp : g_waypoints) {
                if (wp.isTemporary) {
                    wp.x = x;
                    wp.y = y;
                    wp.z = z;
                    wp.dimId = targetDim;
                    wp.isCave = isCave;
                    wp.enabled = true;
                    wp.name = LanguageManager::GetText("TEMP_WAYPOINT_NAME");
                    wp.createdAt = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    resId = wp.id;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            Waypoint wp;
            wp.id = GenerateID();
            wp.name = LanguageManager::GetText("TEMP_WAYPOINT_NAME");
            wp.x = x; wp.y = y; wp.z = z;
            wp.r = 0.0f; wp.g = 0.9f; wp.b = 1.0f; // 鲜明醒目的天青色
            wp.enabled = true;
            wp.dimId = targetDim;
            wp.isCave = isCave;
            wp.pinned = false;
            wp.isTemporary = true;
            wp.folder = "";
            wp.createdAt = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            {
                std::lock_guard<std::mutex> lock(g_wpMutex);
                wp.order = (int)g_waypoints.size();
                g_waypoints.push_back(wp);
            }
            resId = wp.id;
        }
        SaveWaypoints();
        return resId;
    }

    void ClearTemporaryWaypoint() {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::remove_if(g_waypoints.begin(), g_waypoints.end(),
                [](const Waypoint& w) { return w.isTemporary; });
            if (it != g_waypoints.end()) {
                g_waypoints.erase(it, g_waypoints.end());
                changed = true;
            }
        }
        if (changed) SaveWaypoints();
    }

    bool HasTemporaryWaypoint() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        for (const auto& wp : g_waypoints) {
            if (wp.isTemporary) return true;
        }
        return false;
    }

    void SwapWaypointOrder(const std::string& id1, const std::string& id2) {
        if (id1.empty() || id2.empty() || id1 == id2) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it1 = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == id1; });
            auto it2 = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == id2; });
            if (it1 != g_waypoints.end() && it2 != g_waypoints.end()) {
                std::iter_swap(it1, it2);
                for (size_t i = 0; i < g_waypoints.size(); ++i) {
                    g_waypoints[i].order = (int)i;
                }
                changed = true;
            }
        }
        if (changed) SaveWaypoints();
    }

    void MoveWaypointToIndex(const std::string& srcId, const std::string& targetId) {
        if (srcId.empty() || targetId.empty() || srcId == targetId) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto itSrc = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == srcId; });
            auto itTgt = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == targetId; });
            if (itSrc != g_waypoints.end() && itTgt != g_waypoints.end()) {
                Waypoint item = *itSrc;
                g_waypoints.erase(itSrc);
                itTgt = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == targetId; });
                g_waypoints.insert(itTgt, item);
                for (size_t i = 0; i < g_waypoints.size(); ++i) {
                    g_waypoints[i].order = (int)i;
                }
                changed = true;
            }
        }
        if (changed) SaveWaypoints();
    }

    std::vector<std::string> GetFolders() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        std::set<std::string> folderSet = g_customFolders;
        for (const auto& wp : g_waypoints) {
            if (!wp.folder.empty()) folderSet.insert(wp.folder);
        }
        return std::vector<std::string>(folderSet.begin(), folderSet.end());
    }

    void AddFolder(const std::string& folderName) {
        if (folderName.empty()) return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customFolders.insert(folderName);
        }
        SaveWaypoints();
    }

    void RenameFolder(const std::string& oldName, const std::string& newName) {
        if (oldName.empty() || newName.empty() || oldName == newName) return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customFolders.erase(oldName);
            g_customFolders.insert(newName);
            for (auto& wp : g_waypoints) {
                if (wp.folder == oldName) {
                    wp.folder = newName;
                }
            }
        }
        SaveWaypoints();
    }

    void DeleteFolder(const std::string& folderName) {
        if (folderName.empty()) return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customFolders.erase(folderName);
            for (auto& wp : g_waypoints) {
                if (wp.folder == folderName) {
                    wp.folder = ""; // 移至未分类
                }
            }
        }
        SaveWaypoints();
    }

    // [P3] 路径点集合管理接口实现
    std::vector<std::string> GetWaypointSets() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        std::set<std::string> setPool = g_customSets;
        for (const auto& wp : g_waypoints) {
            if (!wp.set.empty() && wp.set != "gui.chiyan_default") {
                setPool.insert(wp.set);
            }
        }
        std::vector<std::string> result;
        result.push_back("gui.chiyan_default");
        for (const auto& s : setPool) {
            if (s != "gui.chiyan_default") {
                result.push_back(s);
            }
        }
        return result;
    }

    void AddWaypointSet(const std::string& setName) {
        if (setName.empty() || setName == "gui.chiyan_default") return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customSets.insert(setName);
            g_customFolders.insert(setName);
        }
        SaveWaypoints();
    }

    void RenameWaypointSet(const std::string& oldName, const std::string& newName) {
        if (oldName.empty() || newName.empty() || oldName == newName || oldName == "gui.chiyan_default") return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customSets.erase(oldName);
            g_customSets.insert(newName);
            g_customFolders.erase(oldName);
            g_customFolders.insert(newName);
            for (auto& wp : g_waypoints) {
                if (wp.set == oldName) {
                    wp.set = newName;
                    wp.folder = newName;
                }
            }
        }
        if (MapRenderState::currentWaypointSet == oldName) {
            MapRenderState::currentWaypointSet = newName;
            LanguageManager::SaveConfig();
        }
        SaveWaypoints();
    }

    void DeleteWaypointSet(const std::string& setName) {
        if (setName.empty() || setName == "gui.chiyan_default") return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customSets.erase(setName);
            for (auto& wp : g_waypoints) {
                if (wp.set == setName) {
                    wp.set = "gui.chiyan_default";
                }
            }
        }
        if (MapRenderState::currentWaypointSet == setName) {
            MapRenderState::currentWaypointSet = "gui.chiyan_default";
            LanguageManager::SaveConfig();
        }
        SaveWaypoints();
    }

    void CycleNextWaypointSet() {
        auto sets = GetWaypointSets();
        if (sets.empty()) return;
        std::string current = MapRenderState::currentWaypointSet;
        size_t nextIdx = 0;
        for (size_t i = 0; i < sets.size(); ++i) {
            if (sets[i] == current) {
                nextIdx = (i + 1) % sets.size();
                break;
            }
        }
        std::string nextSet = sets[nextIdx];
        MapRenderState::currentWaypointSet = nextSet;
        MapRenderState::TriggerWaypointSetToast(nextSet);
        LanguageManager::SaveConfig();
    }

    std::string GetCurrentWaypointSet() {
        return MapRenderState::currentWaypointSet;
    }

    void SetCurrentWaypointSet(const std::string& setName) {
        MapRenderState::currentWaypointSet = setName;
        LanguageManager::SaveConfig();
    }

    std::string GetWaypointSetDisplayName(const std::string& setName) {
        if (setName.empty() || setName == "gui.chiyan_default") {
            return LanguageManager::GetText("WP_SET_DEFAULT");
        }
        return setName;
    }
}

void MapRenderState::TriggerWaypointSetToast(const std::string& setName) {
    std::string disp = WaypointManager::GetWaypointSetDisplayName(setName);
    char buf[256];
    snprintf(buf, sizeof(buf), LanguageManager::GetText("WP_SET_TOAST"), disp.c_str());
    g_waypointSetToast = buf;
    g_waypointSetToastTimer = 2.5f;
}
