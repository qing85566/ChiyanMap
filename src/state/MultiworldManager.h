#pragma once

#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <cstdint>
#include <filesystem>
#include <functional>

namespace MultiworldManager {

    enum class MapSelectionMode : int {
        Single = 0,     // 单张地图模式：固定使用 default 地图实例
        Manual = 1,     // 手动选择模式：玩家可在地图列表中自由选择、新建、切换地图实例 (默认)
        WorldSpawn = 2  // 出生点自动模式：根据世界出生点坐标变动自动切分/切换地图实例
    };

    struct MapInstance {
        std::string id;              // 唯一 ID（如 "default", "map_1728300000"）
        std::string name;            // 显示名称（如 "默认地图", "主城大厅", "生存一区"）
        int spawnX = 0;              // 关联的世界出生点 X
        int spawnZ = 0;              // 关联的世界出生点 Z
        uint64_t createdAt = 0;      // 创建时间戳（毫秒）
        uint64_t lastUsedAt = 0;     // 最后使用时间戳（毫秒）
        std::set<std::string> connectedMaps; // 互联的地图实例集合
    };

    extern std::mutex g_mwMutex;
    extern std::string g_currentWorldId;
    extern std::string g_activeMapId;
    extern std::string g_viewedMapId;
    extern MapSelectionMode g_selectionMode;
    extern std::vector<MapInstance> g_mapInstances;

    void Init();
    void SwitchWorld(const std::string& worldId, int spawnX = 0, int spawnZ = 0);
    
    // 查询与设置当前活动地图 (玩家正在游玩的实际世界实例)
    std::string GetActiveMapId();
    std::string GetActiveMapName();
    bool SetActiveMapId(const std::string& mapId, bool notifyReload = true);

    // 查询与设置大地图浏览地图 (全屏大地图正在查看的地图实例，不改变玩家活动地图)
    std::string GetViewedMapId();
    std::string GetViewedMapName();
    void SetViewedMapId(const std::string& mapId);

    // 实例管理
    std::vector<MapInstance> GetMapList();
    bool CreateMap(const std::string& name, const std::string& customId = "");
    bool RenameMap(const std::string& mapId, const std::string& newName);
    bool DeleteMap(const std::string& mapId);

    // 互联管理
    void ConnectMaps(const std::string& mapId1, const std::string& mapId2);
    void DisconnectMaps(const std::string& mapId1, const std::string& mapId2);
    bool AreMapsConnected(const std::string& mapId1, const std::string& mapId2);

    // 模式管理
    MapSelectionMode GetSelectionMode();
    void SetSelectionMode(MapSelectionMode mode);
    void CheckWorldSpawn(int spawnX, int spawnZ);

    // 统计与路径
    int GetMapRegionCount(const std::string& mapId, int dimId = -1);
    std::filesystem::path GetMapCacheDir(const std::string& worldId, const std::string& mapId, int dimId);

    void SaveConfig();
    void LoadConfig();

    // 监听地图变更的回调注册
    void RegisterMapChangeCallback(std::function<void(const std::string& newMapId)> cb);
}
