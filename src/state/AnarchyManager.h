#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <imgui.h>
#include <mc/deps/core/math/Color.h>

class BlockSource;

namespace AnarchyManager {

    extern std::atomic<bool> g_anarchyRescanRequested;

    // 区块高亮特征标志位 (Bitmask)
    enum HighlightFlag : uint32_t {
        FLAG_NONE                  = 0,
        FLAG_LIQUID_NEW_CHUNK      = 1 << 0, // 液体流动新区块
        FLAG_LIQUID_NEW_CHUNK_INV  = 1 << 1, // 液体流动反向 (已结算老区块)
        FLAG_PALETTE_NEW_CHUNK     = 1 << 2, // 调色板/原生新区块
        FLAG_PALETTE_NEW_CHUNK_INV = 1 << 3, // 调色板反向 (人工痕迹区块)
        FLAG_OLD_CHUNK             = 1 << 4, // 1.12 / Pre-1.18 传承老区块
        FLAG_OLD_CHUNK_INV         = 1 << 5, // 现代新版本区块 (含 26.51 原生新区块)
        FLAG_OLD_BIOME             = 1 << 6, // 经典旧生物群系
        FLAG_PORTAL                = 1 << 7, // 传送门区块 (下界/末地)
        FLAG_LAVA_COLUMN           = 1 << 8, // 下界岩浆柱
        FLAG_PORTAL_SKIP           = 1 << 9, // 传送门跳转疑似基地区块群
        FLAG_OLD_CHUNK_BLENDED     = 1 << 10,// 原生基岩版 26.51 升级融合老区块
    };

    struct ChunkData {
        uint32_t flags = 0;
        uint8_t  lavaHeight = 0;       // 岩浆柱垂直高度
        uint8_t  portalType = 0;       // 1 = Nether Portal, 2 = End Portal
        uint64_t timestamp = 0;        // 发现时间戳 (秒)
    };

    // 基础生命周期与存储
    void Init();
    void SwitchWorld(const std::string& worldId);
    void SaveHighlights();
    void LoadHighlights();
    void ClearHighlights(int dimId = -1);
    void DeleteHighlightAt(int chunkX, int chunkZ, int dimId);

    // 区块扫描检测与特征提取 (由 PlayerHook 调用)
    void ProcessChunk(BlockSource& region, int chunkX, int chunkZ, int dimId, const std::string& currentBiomeName);
    void InspectColumnBlock(int blockX, int blockY, int blockZ, const std::string& blockName, unsigned short blockData, int dimId);
    void RequestRescan();

    // 传送门跳转检测 (PortalSkip 孤立加载岛屿分析)
    void RunPortalSkipDetection(int centerChunkX, int centerChunkZ, int dimId);

    // 状态查询
    bool HasHighlight(int chunkX, int chunkZ, int dimId, uint32_t flagMask = 0xFFFFFFFF);
    ChunkData GetChunkData(int chunkX, int chunkZ, int dimId);
    std::string GetHighlightDescription(int chunkX, int chunkZ, int dimId);
    size_t GetHighlightCount(int dimId);

    // 渲染叠加层 (由 DX11Hook 调用)
    void RenderMiniMapOverlays(ImDrawList* drawList, float cx, float cy, float scale,
                              float c_rot, float s_rot, float viewPX, float viewPZ,
                              float zoomRadius, bool isSquare, const ImVec2& mapMin, const ImVec2& mapMax,
                              int currentDim);

    void RenderBigMapOverlays(ImDrawList* drawList, const ImVec2& canvasOrigin, float zm,
                             float smoothPX, float smoothPZ, float offX, float offZ,
                             const ImVec2& canvasSize, int viewDim);

    // 设置面板与交互
    void RenderSettingsPanel();
}
