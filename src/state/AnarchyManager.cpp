#include "state/AnarchyManager.h"
#include "state/MapRenderState.h"
#include "state/LanguageManager.h"
#include "hooks/InputIMEHelper.h"

#include <mc/world/level/BlockSource.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/block/Block.h>
#include <generator.h>
#include <util.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace AnarchyManager {

    std::atomic<bool> g_anarchyRescanRequested{ false };

    namespace {
        std::mutex g_anarchyMutex;
        std::unordered_map<uint64_t, ChunkData> g_chunkHighlights[3]; // 0=Overworld, 1=Nether, 2=The End
        std::string g_currentWorldId = "default";

        // 通用主干道经典几何预设数据
        const int kRingRoads[] = {
            200, 500, 1000, 1500, 2000, 2500, 5000, 7500, 10000, 15000, 20000, 25000,
            50000, 55000, 62500, 75000, 100000, 125000, 250000, 500000, 750000, 1000000,
            1250000, 1568852, 1875000, 2500000, 3750000
        };
        constexpr size_t kRingRoadCount = sizeof(kRingRoads) / sizeof(kRingRoads[0]);

        const int kDiamonds[] = {
            2500, 5000, 25000, 50000, 125000, 250000, 500000, 3750000
        };
        constexpr size_t kDiamondCount = sizeof(kDiamonds) / sizeof(kDiamonds[0]);

        inline std::vector<int> ParseCustomDistances(const std::string& str) {
            std::vector<int> result;
            std::stringstream ss(str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.erase(item.begin());
                while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
                if (item.empty()) continue;
                try {
                    int val = std::stoi(item);
                    if (val > 0) result.push_back(val);
                } catch (...) {}
            }
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());
            return result;
        }

        // OldBiomes 1.12.2 生成器缓存
        Generator g_oldBiomeGen;
        bool g_oldBiomeGenInited = false;
        int64_t g_cachedOldBiomeSeed = 0;
        std::mutex g_oldBiomeGenMutex;

        inline uint64_t MakeChunkKey(int cx, int cz) {
            return ((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz;
        }

        inline void SplitChunkKey(uint64_t k, int& cx, int& cz) {
            cx = (int)(int32_t)(uint32_t)(k >> 32);
            cz = (int)(int32_t)(uint32_t)k;
        }

        std::filesystem::path GetSaveFilePath(int dimId) {
            std::filesystem::path dir = "mods/ChiyanMap/anarchy";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            std::string safeWorldId = g_currentWorldId;
            for (char& c : safeWorldId) {
                if (c == ':' || c == '/' || c == '\\' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                    c = '_';
                }
            }
            return dir / (safeWorldId + "_dim" + std::to_string(dimId) + ".bin");
        }

        void EnsureOldBiomeGenerator(int64_t seed) {
            std::lock_guard<std::mutex> lock(g_oldBiomeGenMutex);
            if (!g_oldBiomeGenInited || g_cachedOldBiomeSeed != seed) {
                setupGenerator(&g_oldBiomeGen, MC_1_12, 0);
                applySeed(&g_oldBiomeGen, DIM_OVERWORLD, static_cast<uint64_t>(seed));
                g_oldBiomeGenInited = true;
                g_cachedOldBiomeSeed = seed;
            }
        }

        // 辅助特征判断函数
        inline bool IsFlowingLiquid(const std::string& name, unsigned short data) {
            if (name.empty()) return false;
            if (name.find("flowing_water") != std::string::npos ||
                name.find("flowing_lava") != std::string::npos) {
                return true;
            }
            if (name.find("waterlily") != std::string::npos ||
                name.find("cauldron") != std::string::npos) {
                return false;
            }
            bool isWater = (name == "minecraft:water" || name == "water" || name.find("water") != std::string::npos);
            bool isLava  = (name == "minecraft:lava"  || name == "lava"  || name.find("lava") != std::string::npos);
            if ((isWater || isLava) && data > 0) {
                return true;
            }
            return false;
        }

        inline bool IsPortalBlock(const std::string& name, uint8_t& outType) {
            if (name.empty()) return false;
            if (name.find("end_portal") != std::string::npos || name.find("end_gateway") != std::string::npos) {
                outType = 2; // 末地传送门 / 折跃门
                return true;
            }
            if (name.find("portal") != std::string::npos) {
                outType = 1; // 下界传送门
                return true;
            }
            return false;
        }

        inline bool IsPlayerCraftedBlock(const std::string& name) {
            if (name.empty()) return false;
            // 1. 容器与功能工作台
            if (name.find("chest") != std::string::npos ||
                name.find("furnace") != std::string::npos ||
                name.find("barrel") != std::string::npos ||
                name.find("smoker") != std::string::npos ||
                name.find("blast_furnace") != std::string::npos ||
                name.find("crafting_table") != std::string::npos ||
                name.find("anvil") != std::string::npos ||
                name.find("enchanting_table") != std::string::npos ||
                name.find("brewing_stand") != std::string::npos ||
                name.find("hopper") != std::string::npos ||
                name.find("dispenser") != std::string::npos ||
                name.find("dropper") != std::string::npos ||
                name.find("shulker_box") != std::string::npos ||
                name.find("loom") != std::string::npos ||
                name.find("cartography_table") != std::string::npos ||
                name.find("smithing_table") != std::string::npos ||
                name.find("grindstone") != std::string::npos ||
                name.find("stonecutter") != std::string::npos ||
                name.find("cauldron") != std::string::npos) return true;

            // 2. 居所与装饰方块
            if (name.find("bed") != std::string::npos ||
                name.find("torch") != std::string::npos ||
                name.find("lantern") != std::string::npos ||
                name.find("campfire") != std::string::npos ||
                name.find("bookshelf") != std::string::npos ||
                name.find("banner") != std::string::npos ||
                name.find("sign") != std::string::npos ||
                name.find("item_frame") != std::string::npos ||
                name.find("flower_pot") != std::string::npos ||
                name.find("bell") != std::string::npos) return true;

            // 3. 红石与机械
            if (name.find("redstone") != std::string::npos ||
                name.find("repeater") != std::string::npos ||
                name.find("comparator") != std::string::npos ||
                name.find("piston") != std::string::npos ||
                name.find("observer") != std::string::npos ||
                name.find("lever") != std::string::npos ||
                name.find("button") != std::string::npos ||
                name.find("tripwire") != std::string::npos ||
                name.find("daylight_detector") != std::string::npos ||
                name.find("target") != std::string::npos ||
                name.find("note_block") != std::string::npos ||
                name.find("jukebox") != std::string::npos) return true;

            // 4. 交通与建筑加工材
            if (name.find("rail") != std::string::npos ||
                name.find("ladder") != std::string::npos ||
                name.find("scaffolding") != std::string::npos ||
                name.find("door") != std::string::npos ||
                name.find("trapdoor") != std::string::npos ||
                name.find("fence_gate") != std::string::npos ||
                name.find("planks") != std::string::npos ||
                name.find("cobblestone") != std::string::npos ||
                name.find("stonebrick") != std::string::npos ||
                name.find("stone_brick") != std::string::npos ||
                name.find("polished_") != std::string::npos ||
                name.find("smooth_") != std::string::npos ||
                name.find("chiseled_") != std::string::npos ||
                name.find("cut_") != std::string::npos ||
                name.find("glass") != std::string::npos ||
                name.find("concrete") != std::string::npos ||
                name.find("terracotta") != std::string::npos ||
                name.find("glazed_terracotta") != std::string::npos ||
                name.find("wool") != std::string::npos ||
                name.find("carpet") != std::string::npos) return true;

            return false;
        }

        inline bool IsModernBlock(const std::string& name, int dimId) {
            if (name.empty()) return false;
            if (dimId == 0) {
                return name.find("deepslate") != std::string::npos ||
                       name.find("copper") != std::string::npos ||
                       name.find("amethyst") != std::string::npos ||
                       name.find("tuff") != std::string::npos ||
                       name.find("dripstone") != std::string::npos ||
                       name.find("azalea") != std::string::npos ||
                       name.find("kelp") != std::string::npos ||
                       name.find("seagrass") != std::string::npos ||
                       name.find("campfire") != std::string::npos;
            } else if (dimId == 1) {
                return name.find("blackstone") != std::string::npos ||
                       name.find("basalt") != std::string::npos ||
                       name.find("soul_soil") != std::string::npos ||
                       name.find("ancient_debris") != std::string::npos ||
                       name.find("nylium") != std::string::npos ||
                       name.find("shroomlight") != std::string::npos;
            }
            return false;
        }

        // 原生基岩版 26.51 / 1.21 试炼遗迹与现代特征方块检测
        inline bool IsBedrock2651NativeBlock(const std::string& name, int dimId) {
            if (name.empty()) return false;
            // 1. 试炼遗迹结构方块 (Trial Chambers, 1.21 / 26.51)
            if (name.find("trial_spawner") != std::string::npos ||
                name.find("vault") != std::string::npos ||
                name.find("crafter") != std::string::npos ||
                name.find("copper_bulb") != std::string::npos ||
                name.find("heavy_core") != std::string::npos ||
                name.find("tuff_brick") != std::string::npos ||
                name.find("chiseled_tuff") != std::string::npos ||
                name.find("polished_tuff") != std::string::npos ||
                name.find("chiseled_copper") != std::string::npos ||
                name.find("copper_grate") != std::string::npos ||
                name.find("copper_door") != std::string::npos ||
                name.find("copper_trapdoor") != std::string::npos) {
                return true;
            }
            // 2. 苍白花园 (Pale Garden / 26.51 特征)
            if (name.find("pale_oak") != std::string::npos ||
                name.find("pale_moss") != std::string::npos ||
                name.find("pale_hanging_moss") != std::string::npos ||
                name.find("creaking_heart") != std::string::npos ||
                name.find("resin") != std::string::npos) {
                return true;
            }
            // 3. 樱花与考古新方块 (Cherry Grove / Archeology, 1.20)
            if (name.find("cherry_leaves") != std::string::npos ||
                name.find("cherry_log") != std::string::npos ||
                name.find("cherry_wood") != std::string::npos ||
                name.find("cherry_sapling") != std::string::npos ||
                name.find("pink_petals") != std::string::npos ||
                name.find("suspicious_sand") != std::string::npos ||
                name.find("suspicious_gravel") != std::string::npos ||
                name.find("sniffer_egg") != std::string::npos ||
                name.find("torchflower") != std::string::npos ||
                name.find("pitcher_plant") != std::string::npos ||
                name.find("pitcher_crop") != std::string::npos) {
                return true;
            }
            // 4. 深暗之域幽匿方块 (Deep Dark / Sculk, 1.19)
            if (name.find("sculk") != std::string::npos ||
                name.find("mangrove_roots") != std::string::npos ||
                name.find("mud_brick") != std::string::npos ||
                name.find("muddy_mangrove_roots") != std::string::npos ||
                name.find("reinforced_deepslate") != std::string::npos) {
                return true;
            }
            return false;
        }
    } // namespace

    void Init() {
        // 初始化经典旧生物群系生成器
        EnsureOldBiomeGenerator(MapRenderState::anarchyOldBiomesSeed);
        LoadHighlights();
    }

    void SwitchWorld(const std::string& worldId) {
        {
            std::lock_guard<std::mutex> lock(g_anarchyMutex);
            if (MapRenderState::anarchySaveLoadToDisk) {
                // 保存旧世界
                for (int dim = 0; dim < 3; ++dim) {
                    if (g_chunkHighlights[dim].empty()) continue;
                    auto path = GetSaveFilePath(dim);
                    std::ofstream out(path, std::ios::binary | std::ios::trunc);
                    if (out.is_open()) {
                        uint32_t magic = 0x4E415943; // 'CYAN'
                        uint32_t version = 1;
                        uint32_t count = static_cast<uint32_t>(g_chunkHighlights[dim].size());
                        out.write(reinterpret_cast<const char*>(&magic), 4);
                        out.write(reinterpret_cast<const char*>(&version), 4);
                        out.write(reinterpret_cast<const char*>(&count), 4);
                        for (const auto& [key, data] : g_chunkHighlights[dim]) {
                            int cx, cz;
                            SplitChunkKey(key, cx, cz);
                            out.write(reinterpret_cast<const char*>(&cx), 4);
                            out.write(reinterpret_cast<const char*>(&cz), 4);
                            out.write(reinterpret_cast<const char*>(&data.flags), 4);
                            out.write(reinterpret_cast<const char*>(&data.lavaHeight), 1);
                            out.write(reinterpret_cast<const char*>(&data.portalType), 1);
                            out.write(reinterpret_cast<const char*>(&data.timestamp), 8);
                        }
                    }
                }
            }
            g_currentWorldId = worldId.empty() ? "default" : worldId;
            for (int dim = 0; dim < 3; ++dim) {
                g_chunkHighlights[dim].clear();
            }
        }
        LoadHighlights();
    }

    void SaveHighlights() {
        if (!MapRenderState::anarchySaveLoadToDisk) return;
        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        for (int dim = 0; dim < 3; ++dim) {
            auto path = GetSaveFilePath(dim);
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) continue;

            uint32_t magic = 0x4E415943; // 'CYAN'
            uint32_t version = 1;
            uint32_t count = static_cast<uint32_t>(g_chunkHighlights[dim].size());
            out.write(reinterpret_cast<const char*>(&magic), 4);
            out.write(reinterpret_cast<const char*>(&version), 4);
            out.write(reinterpret_cast<const char*>(&count), 4);

            for (const auto& [key, data] : g_chunkHighlights[dim]) {
                int cx, cz;
                SplitChunkKey(key, cx, cz);
                out.write(reinterpret_cast<const char*>(&cx), 4);
                out.write(reinterpret_cast<const char*>(&cz), 4);
                out.write(reinterpret_cast<const char*>(&data.flags), 4);
                out.write(reinterpret_cast<const char*>(&data.lavaHeight), 1);
                out.write(reinterpret_cast<const char*>(&data.portalType), 1);
                out.write(reinterpret_cast<const char*>(&data.timestamp), 8);
            }
        }
    }

    void LoadHighlights() {
        if (!MapRenderState::anarchySaveLoadToDisk) return;
        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        for (int dim = 0; dim < 3; ++dim) {
            g_chunkHighlights[dim].clear();
            auto path = GetSaveFilePath(dim);
            if (!std::filesystem::exists(path)) continue;

            std::ifstream in(path, std::ios::binary);
            if (!in.is_open()) continue;

            uint32_t magic = 0, version = 0, count = 0;
            in.read(reinterpret_cast<char*>(&magic), 4);
            in.read(reinterpret_cast<char*>(&version), 4);
            in.read(reinterpret_cast<char*>(&count), 4);

            if (magic != 0x4E415943 || version != 1) continue;

            for (uint32_t i = 0; i < count; ++i) {
                int cx = 0, cz = 0;
                ChunkData cd{};
                in.read(reinterpret_cast<char*>(&cx), 4);
                in.read(reinterpret_cast<char*>(&cz), 4);
                in.read(reinterpret_cast<char*>(&cd.flags), 4);
                in.read(reinterpret_cast<char*>(&cd.lavaHeight), 1);
                in.read(reinterpret_cast<char*>(&cd.portalType), 1);
                in.read(reinterpret_cast<char*>(&cd.timestamp), 8);
                if (in.good()) {
                    g_chunkHighlights[dim][MakeChunkKey(cx, cz)] = cd;
                }
            }
        }
    }

    void ClearHighlights(int dimId) {
        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        if (dimId >= 0 && dimId < 3) {
            g_chunkHighlights[dimId].clear();
            std::error_code ec;
            std::filesystem::remove(GetSaveFilePath(dimId), ec);
        } else {
            for (int d = 0; d < 3; ++d) {
                g_chunkHighlights[d].clear();
                std::error_code ec;
                std::filesystem::remove(GetSaveFilePath(d), ec);
            }
        }
    }

    void DeleteHighlightAt(int chunkX, int chunkZ, int dimId) {
        if (dimId < 0 || dimId >= 3) return;
        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        g_chunkHighlights[dimId].erase(MakeChunkKey(chunkX, chunkZ));
    }

    size_t GetHighlightCount(int dimId) {
        if (dimId < 0 || dimId >= 3) return 0;
        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        return g_chunkHighlights[dimId].size();
    }

    bool HasHighlight(int chunkX, int chunkZ, int dimId, uint32_t flagMask) {
        if (dimId < 0 || dimId >= 3) return false;
        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        auto it = g_chunkHighlights[dimId].find(MakeChunkKey(chunkX, chunkZ));
        if (it == g_chunkHighlights[dimId].end()) return false;
        return (it->second.flags & flagMask) != 0;
    }

    ChunkData GetChunkData(int chunkX, int chunkZ, int dimId) {
        if (dimId < 0 || dimId >= 3) return ChunkData{};
        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        auto it = g_chunkHighlights[dimId].find(MakeChunkKey(chunkX, chunkZ));
        if (it != g_chunkHighlights[dimId].end()) {
            return it->second;
        }
        return ChunkData{};
    }

    std::string GetHighlightDescription(int chunkX, int chunkZ, int dimId) {
        ChunkData data = GetChunkData(chunkX, chunkZ, dimId);
        if (data.flags == 0) return "";

        std::string desc;
        auto append = [&](const char* text) {
            if (!desc.empty()) desc += " | ";
            desc += text;
        };

        if (data.flags & FLAG_LIQUID_NEW_CHUNK) append(LanguageManager::GetText("ANARCHY_LIQUID_NEW_CHUNK"));
        if (data.flags & FLAG_LIQUID_NEW_CHUNK_INV) append(LanguageManager::GetText("ANARCHY_LIQUID_NEW_CHUNK_INV"));
        if (data.flags & FLAG_PALETTE_NEW_CHUNK) append(LanguageManager::GetText("ANARCHY_PALETTE_NEW_CHUNK"));
        if (data.flags & FLAG_PALETTE_NEW_CHUNK_INV) append(LanguageManager::GetText("ANARCHY_PALETTE_NEW_CHUNK_INV"));
        if (data.flags & FLAG_OLD_CHUNK) append(LanguageManager::GetText("ANARCHY_OLD_CHUNK"));
        if (data.flags & FLAG_OLD_CHUNK_BLENDED) append(LanguageManager::GetText("ANARCHY_OLD_CHUNK_BLENDED"));
        if (data.flags & FLAG_OLD_CHUNK_INV) append(LanguageManager::GetText("ANARCHY_OLD_CHUNK_INV"));
        if (data.flags & FLAG_OLD_BIOME) append(LanguageManager::GetText("ANARCHY_OLD_BIOME"));
        if (data.flags & FLAG_PORTAL) {
            if (data.portalType == 2) append(LanguageManager::GetText("ANARCHY_PORTAL_END"));
            else append(LanguageManager::GetText("ANARCHY_PORTAL_NETHER"));
        }
        if (data.flags & FLAG_LAVA_COLUMN) {
            std::string s = LanguageManager::GetText("ANARCHY_LAVA_COLUMN");
            s += " (" + std::to_string(data.lavaHeight) + "m)";
            append(s.c_str());
        }
        if (data.flags & FLAG_PORTAL_SKIP) append(LanguageManager::GetText("ANARCHY_PORTAL_SKIP"));

        return desc;
    }

    // =========================================================================
    // 区块扫描检测与特征提取
    // =========================================================================
    void ProcessChunk(BlockSource& region, int chunkX, int chunkZ, int dimId, const std::string& currentBiomeName) {
        if (dimId < 0 || dimId >= 3) return;

        // 若全部探测模块均处于关闭状态，直接跳过
        bool anyEnabled = (
            MapRenderState::anarchyLiquidNewChunks || MapRenderState::anarchyLiquidNewChunksInverse ||
            MapRenderState::anarchyPaletteNewChunks || MapRenderState::anarchyPaletteNewChunksInverse ||
            MapRenderState::anarchyOldChunks || MapRenderState::anarchyOldChunksInverse ||
            MapRenderState::anarchyOldBiomes || MapRenderState::anarchyPortals ||
            MapRenderState::anarchyLavaColumns
        );
        if (!anyEnabled) return;

        uint32_t newFlags = 0;
        uint32_t removeFlags = 0;
        uint8_t  lavaHeight = 0;
        uint8_t  portalType = 0;

        int baseBx = chunkX * 16;
        int baseBz = chunkZ * 16;

        bool hasFlowingLiquid = false;
        bool hasAnySolidBlock = false;
        bool hasModernBlocks = false;
        bool has2651NativeBlocks = false;
        bool hasPlayerBlocks = false;
        bool hasDeepslateBelow0 = false;
        bool hasBedrockAt0 = false;
        bool hasPortalBlock = false;
        uint8_t foundPortalType = 0;
        int maxLavaCol = 0;

        // 1. 经典 OldBiomes 检测 (主世界专用)
        if (dimId == 0 && MapRenderState::anarchyOldBiomes) {
            EnsureOldBiomeGenerator(MapRenderState::anarchyOldBiomesSeed);
            int sampleX = baseBx + 8;
            int sampleZ = baseBz + 8;
            std::lock_guard<std::mutex> lock(g_oldBiomeGenMutex);
            int oldBiomeId = getBiomeAt(&g_oldBiomeGen, 4, sampleX >> 2, 0, sampleZ >> 2);
            const char* oldBiomeName = biome2str(MC_1_12, oldBiomeId);
            if (oldBiomeName != nullptr) {
                std::string obStr(oldBiomeName);
                std::string cbStr = currentBiomeName;
                std::transform(cbStr.begin(), cbStr.end(), cbStr.begin(), ::tolower);
                if (cbStr.find(obStr) != std::string::npos || obStr.find(cbStr) != std::string::npos) {
                    newFlags |= FLAG_OLD_BIOME;
                }
            }
        }

        // 2. 均匀覆盖的 4x4 采样网格 (16 列，覆盖整个 16x16 区块)
        const int sampleOffsets[16][2] = {
            { 2, 2 }, { 6, 2 }, { 10, 2 }, { 14, 2 },
            { 2, 6 }, { 6, 6 }, { 10, 6 }, { 14, 6 },
            { 2, 10 }, { 6, 10 }, { 10, 10 }, { 14, 10 },
            { 2, 14 }, { 6, 14 }, { 10, 14 }, { 14, 14 }
        };

        for (int i = 0; i < 16; ++i) {
            int bx = baseBx + sampleOffsets[i][0];
            int bz = baseBz + sampleOffsets[i][1];

            if (dimId == 0) {
                // 主世界 1.18 特征探测
                try {
                    Block const& bNeg = region.getBlock(BlockPos(bx, -10, bz));
                    std::string nNeg = bNeg.getTypeName();
                    if (nNeg.find("deepslate") != std::string::npos || nNeg.find("stone") != std::string::npos) {
                        hasDeepslateBelow0 = true;
                    }
                    if (IsBedrock2651NativeBlock(nNeg, 0)) {
                        has2651NativeBlocks = true;
                    }
                } catch (...) {}

                try {
                    Block const& bZero = region.getBlock(BlockPos(bx, 0, bz));
                    std::string nZero = bZero.getTypeName();
                    if (nZero.find("bedrock") != std::string::npos) {
                        hasBedrockAt0 = true;
                    }
                } catch (...) {}

                // 探测深层试炼遗迹 (Trial Chambers: 通常在 Y=-20 ~ Y=-40 生成)
                try {
                    Block const& bTrial = region.getBlock(BlockPos(bx, -28, bz));
                    std::string nTrial = bTrial.getTypeName();
                    if (IsBedrock2651NativeBlock(nTrial, 0)) {
                        has2651NativeBlocks = true;
                    }
                } catch (...) {}

                // 获取地表高度并探查地表附近方块
                short surfY = -32000;
                try {
                    surfY = region.getAboveTopSolidBlock(bx, bz, true, true);
                } catch (...) {}

                if (surfY > -64 && surfY < 320) {
                    hasAnySolidBlock = true;
                    int checkTop = std::min(surfY + 1, 319);
                    int checkBottom = std::max((int)surfY - 6, -60);
                    for (int y = checkTop; y >= checkBottom; --y) {
                        try {
                            Block const& blk = region.getBlock(BlockPos(bx, y, bz));
                            if (blk.isAir()) continue;

                            std::string name = blk.getTypeName();
                            unsigned short d = blk.getData();

                            if (IsFlowingLiquid(name, d)) {
                                if (!MapRenderState::anarchyLiquidNewChunksOnlyAboveY0 || y >= 0) {
                                    hasFlowingLiquid = true;
                                }
                            }
                            uint8_t pType = 0;
                            if (IsPortalBlock(name, pType)) {
                                hasPortalBlock = true;
                                foundPortalType = pType;
                            }
                            if (IsPlayerCraftedBlock(name)) {
                                hasPlayerBlocks = true;
                            }
                            if (IsModernBlock(name, 0)) {
                                hasModernBlocks = true;
                            }
                            if (IsBedrock2651NativeBlock(name, 0)) {
                                has2651NativeBlocks = true;
                            }
                        } catch (...) {}
                    }
                }
            } else if (dimId == 1) {
                // 下界：纵向扫描 Y=120 到 Y=32 (穿透顶棚黑曜石以下区域，捕捉传送门、岩浆柱与人工痕迹)
                int currentLavaStreak = 0;
                for (int y = 120; y >= 32; --y) {
                    try {
                        Block const& blk = region.getBlock(BlockPos(bx, y, bz));
                        if (!blk.isAir()) {
                            hasAnySolidBlock = true;
                            std::string name = blk.getTypeName();
                            unsigned short d = blk.getData();

                            if (IsFlowingLiquid(name, d)) {
                                hasFlowingLiquid = true;
                            }
                            uint8_t pType = 0;
                            if (IsPortalBlock(name, pType)) {
                                hasPortalBlock = true;
                                foundPortalType = pType;
                            }
                            if (IsPlayerCraftedBlock(name)) {
                                hasPlayerBlocks = true;
                            }
                            if (IsModernBlock(name, 1)) {
                                hasModernBlocks = true;
                            }
                            if (IsBedrock2651NativeBlock(name, 1)) {
                                has2651NativeBlocks = true;
                            }

                            if (name.find("lava") != std::string::npos) {
                                currentLavaStreak++;
                                if (currentLavaStreak > maxLavaCol) maxLavaCol = currentLavaStreak;
                            } else {
                                currentLavaStreak = 0;
                            }
                        } else {
                            currentLavaStreak = 0;
                        }
                    } catch (...) {}
                }
            } else {
                // 末地
                short surfY = -32000;
                try {
                    surfY = region.getAboveTopSolidBlock(bx, bz, true, true);
                } catch (...) {}
                if (surfY > 0 && surfY < 256) {
                    hasAnySolidBlock = true;
                    for (int y = surfY + 1; y >= surfY - 4; --y) {
                        try {
                            Block const& blk = region.getBlock(BlockPos(bx, y, bz));
                            if (blk.isAir()) continue;
                            std::string name = blk.getTypeName();
                            uint8_t pType = 0;
                            if (IsPortalBlock(name, pType)) {
                                hasPortalBlock = true;
                                foundPortalType = pType;
                            }
                            if (IsPlayerCraftedBlock(name)) {
                                hasPlayerBlocks = true;
                            }
                            if (IsBedrock2651NativeBlock(name, 2)) {
                                has2651NativeBlocks = true;
                            }
                        } catch (...) {}
                    }
                }
            }
        }

        // 标志综合结算
        if (hasFlowingLiquid) {
            newFlags |= FLAG_LIQUID_NEW_CHUNK;
            removeFlags |= FLAG_LIQUID_NEW_CHUNK_INV;
        } else if (hasAnySolidBlock) {
            newFlags |= FLAG_LIQUID_NEW_CHUNK_INV;
        }

        if (hasPlayerBlocks) {
            newFlags |= FLAG_PALETTE_NEW_CHUNK_INV;
            removeFlags |= FLAG_PALETTE_NEW_CHUNK;
        } else if (hasAnySolidBlock) {
            newFlags |= FLAG_PALETTE_NEW_CHUNK;
        }

        // 老区块判定 (模式 0=Classic 1.12, 1=Bedrock Pre-1.18, 2=Bedrock 26.51 Upgraded World, 3=Multi-Era Comprehensive)
        int oldMode = MapRenderState::anarchyOldChunksMode;

        if (oldMode == 0) { // 经典 1.12
            if (dimId == 0) {
                if (!hasModernBlocks && !hasDeepslateBelow0 && hasBedrockAt0) {
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else if (hasModernBlocks || hasDeepslateBelow0) {
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            } else if (dimId == 1) {
                if (!hasModernBlocks) {
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else {
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            }
        } else if (oldMode == 1) { // 基岩版 Pre-1.18 传承老区块
            if (dimId == 0) {
                if (!hasDeepslateBelow0 && hasBedrockAt0) {
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else {
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            } else {
                if (!hasModernBlocks) {
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else {
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            }
        } else if (oldMode == 2) { // 基岩版 26.51 升级存档探测模式
            if (dimId == 0) {
                if (!hasDeepslateBelow0 && hasBedrockAt0) {
                    // Pre-1.18 传承老区块
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else if (hasDeepslateBelow0 && !has2651NativeBlocks) {
                    // 基岩版 26.51 融合升级区块：有负层深板岩混合层，但缺乏 26.51 原生试炼遗迹与新方块
                    if (MapRenderState::anarchyOldChunksBedrockBlending) {
                        newFlags |= FLAG_OLD_CHUNK_BLENDED;
                    } else {
                        newFlags |= FLAG_OLD_CHUNK;
                    }
                    removeFlags |= FLAG_OLD_CHUNK_INV;
                } else if (has2651NativeBlocks) {
                    // 原生 26.51 新区块
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            } else {
                if (!hasModernBlocks && !has2651NativeBlocks) {
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else if (!has2651NativeBlocks) {
                    if (MapRenderState::anarchyOldChunksBedrockBlending) {
                        newFlags |= FLAG_OLD_CHUNK_BLENDED;
                    } else {
                        newFlags |= FLAG_OLD_CHUNK;
                    }
                    removeFlags |= FLAG_OLD_CHUNK_INV;
                } else {
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            }
        } else { // 模式 3: 全版本多代综合断层分析
            if (dimId == 0) {
                if (!hasDeepslateBelow0 && hasBedrockAt0) {
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else if (hasDeepslateBelow0 && !has2651NativeBlocks) {
                    newFlags |= FLAG_OLD_CHUNK_BLENDED;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_INV);
                } else {
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            } else {
                if (!hasModernBlocks) {
                    newFlags |= FLAG_OLD_CHUNK;
                    removeFlags |= (FLAG_OLD_CHUNK_INV | FLAG_OLD_CHUNK_BLENDED);
                } else if (!has2651NativeBlocks) {
                    newFlags |= FLAG_OLD_CHUNK_BLENDED;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_INV);
                } else {
                    newFlags |= FLAG_OLD_CHUNK_INV;
                    removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
                }
            }
        }

        // 传送门
        if (hasPortalBlock) {
            newFlags |= FLAG_PORTAL;
            portalType = foundPortalType;
        }

        // 下界岩浆柱
        if (dimId == 1 && maxLavaCol >= MapRenderState::anarchyLavaColumnsMinHeight) {
            newFlags |= FLAG_LAVA_COLUMN;
            lavaHeight = static_cast<uint8_t>(std::min(maxLavaCol, 255));
        }

        if (newFlags != 0 || removeFlags != 0) {
            uint64_t nowSec = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            );

            std::lock_guard<std::mutex> lock(g_anarchyMutex);
            uint64_t key = MakeChunkKey(chunkX, chunkZ);
            auto& data = g_chunkHighlights[dimId][key];
            data.flags = (data.flags & ~removeFlags) | newFlags;
            if (lavaHeight > 0) data.lavaHeight = lavaHeight;
            if (portalType > 0) data.portalType = portalType;
            data.timestamp = nowSec;
        }
    }

    void InspectColumnBlock(int blockX, int blockY, int blockZ, const std::string& blockName, unsigned short blockData, int dimId) {
        if (dimId < 0 || dimId >= 3 || blockName.size() < 4) return;

        bool anyEnabled = (
            MapRenderState::anarchyLiquidNewChunks || MapRenderState::anarchyLiquidNewChunksInverse ||
            MapRenderState::anarchyPaletteNewChunks || MapRenderState::anarchyPaletteNewChunksInverse ||
            MapRenderState::anarchyOldChunks || MapRenderState::anarchyOldChunksInverse ||
            MapRenderState::anarchyOldBiomes || MapRenderState::anarchyPortals ||
            MapRenderState::anarchyLavaColumns
        );
        if (!anyEnabled) return;

        // 极速跳过最常见自然地形方块 (单次比较跳过 95%+ 地形，零性能损耗)
        if (blockName == "minecraft:air" || blockName == "minecraft:stone" ||
            blockName == "minecraft:dirt" || blockName == "minecraft:grass_block" ||
            blockName == "minecraft:deepslate" || blockName == "minecraft:bedrock" ||
            blockName == "minecraft:netherrack" || blockName == "minecraft:end_stone" ||
            blockName == "minecraft:sand" || blockName == "minecraft:gravel" ||
            blockName == "minecraft:snow" || blockName == "minecraft:ice") {
            return;
        }

        int cx = blockX >> 4;
        int cz = blockZ >> 4;
        uint32_t addFlags = 0;
        uint32_t removeFlags = 0;
        uint8_t portalT = 0;

        // 1. 流动液体检查
        if (IsFlowingLiquid(blockName, blockData)) {
            if (!MapRenderState::anarchyLiquidNewChunksOnlyAboveY0 || blockY >= 0) {
                addFlags |= FLAG_LIQUID_NEW_CHUNK;
                removeFlags |= FLAG_LIQUID_NEW_CHUNK_INV;
            }
        }

        // 2. 传送门检查
        if (IsPortalBlock(blockName, portalT)) {
            addFlags |= FLAG_PORTAL;
        }

        // 3. 人工痕迹与调色板检查
        if (IsPlayerCraftedBlock(blockName)) {
            addFlags |= FLAG_PALETTE_NEW_CHUNK_INV; // 人工痕迹
            removeFlags |= FLAG_PALETTE_NEW_CHUNK;  // 移除原生新区块标记
        }

        // 4. 26.51 特征与现代方块检查
        if (IsBedrock2651NativeBlock(blockName, dimId)) {
            if (MapRenderState::anarchyOldChunksMode >= 2) {
                addFlags |= FLAG_OLD_CHUNK_INV;
                removeFlags |= (FLAG_OLD_CHUNK | FLAG_OLD_CHUNK_BLENDED);
            }
        }

        if (addFlags == 0 && removeFlags == 0) return;

        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        uint64_t key = MakeChunkKey(cx, cz);
        auto& cd = g_chunkHighlights[dimId][key];
        cd.flags = (cd.flags & ~removeFlags) | addFlags;
        if (portalT > 0) cd.portalType = portalT;
        cd.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    void RequestRescan() {
        g_anarchyRescanRequested.store(true);
        MapRenderState::forceRescan.store(true);
    }

    // =========================================================================
    // 传送门跳转检测 (PortalSkip 孤立加载岛屿分析)
    // =========================================================================
    void RunPortalSkipDetection(int centerChunkX, int centerChunkZ, int dimId) {
        if (!MapRenderState::anarchyPortalSkip || dimId != 0) return;

        int R = MapRenderState::anarchyPortalSkipRadius;
        if (R < 3) R = 7; // 默认 15x15 区块 (r=7)

        std::lock_guard<std::mutex> lock(g_anarchyMutex);
        const auto& dimMap = g_chunkHighlights[dimId];

        // 1. 检查中心 R 区域内部的区块密度 (必须有相当数量被识别过/加载过)
        int insideCount = 0;
        int totalInside = (2 * R + 1) * (2 * R + 1);

        for (int dz = -R; dz <= R; ++dz) {
            for (int dx = -R; dx <= R; ++dx) {
                uint64_t k = MakeChunkKey(centerChunkX + dx, centerChunkZ + dz);
                if (dimMap.find(k) != dimMap.end()) {
                    insideCount++;
                }
            }
        }

        // 内部加载率需要大于 40%
        if (insideCount < (totalInside * 2 / 5)) return;

        // 2. 检查外围一圈 (R+2 环带) 是否存在断崖式未加载边界
        int outerCount = 0;
        int totalOuter = 0;
        for (int r = R + 1; r <= R + 2; ++r) {
            for (int dx = -r; dx <= r; ++dx) {
                for (int dz = -r; dz <= r; ++dz) {
                    if (std::abs(dx) < r && std::abs(dz) < r) continue;
                    totalOuter++;
                    uint64_t k = MakeChunkKey(centerChunkX + dx, centerChunkZ + dz);
                    if (dimMap.find(k) != dimMap.end()) {
                        outerCount++;
                    }
                }
            }
        }

        // 外围未加载率需要大于 70% (孤立 15x15 岛屿特征)
        if (outerCount <= (totalOuter * 3 / 10)) {
            uint64_t nowSec = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            );

            // 将整片 15x15 标记为 PortalSkip
            for (int dz = -R; dz <= R; ++dz) {
                for (int dx = -R; dx <= R; ++dx) {
                    uint64_t k = MakeChunkKey(centerChunkX + dx, centerChunkZ + dz);
                    auto& data = g_chunkHighlights[dimId][k];
                    data.flags |= FLAG_PORTAL_SKIP;
                    data.timestamp = nowSec;
                }
            }
        }
    }

    // =========================================================================
    // 小地图渲染 (Overlays & Universal Highways)
    // =========================================================================
    void RenderMiniMapOverlays(ImDrawList* drawList, float cx, float cy, float scale,
                              float c_rot, float s_rot, float viewPX, float viewPZ,
                              float zoomRadius, bool isSquare, const ImVec2& mapMin, const ImVec2& mapMax,
                              int currentDim) {
        if (!drawList || currentDim < 0 || currentDim >= 3) return;

        constexpr float CHUNK_SIZE = 16.0f;
        float coverR = zoomRadius * (isSquare ? 1.42f : 1.0f);
        float screenR = zoomRadius * scale;
        float screenRSq = screenR * screenR;

        int firstChunkX = static_cast<int>(std::floor((viewPX - coverR) / CHUNK_SIZE));
        int lastChunkX  = static_cast<int>(std::ceil ((viewPX + coverR) / CHUNK_SIZE));
        int firstChunkZ = static_cast<int>(std::floor((viewPZ - coverR) / CHUNK_SIZE));
        int lastChunkZ  = static_cast<int>(std::ceil ((viewPZ + coverR) / CHUNK_SIZE));

        if (isSquare) drawList->PushClipRect(mapMin, mapMax, true);

        // 1. 渲染区块高亮 (按稀有度与关键度优先着色)
        {
            std::lock_guard<std::mutex> lock(g_anarchyMutex);
            const auto& dimMap = g_chunkHighlights[currentDim];

            for (int cz = firstChunkZ; cz <= lastChunkZ; ++cz) {
                for (int cxChunk = firstChunkX; cxChunk <= lastChunkX; ++cxChunk) {
                    uint64_t key = MakeChunkKey(cxChunk, cz);
                    auto it = dimMap.find(key);
                    if (it == dimMap.end()) continue;

                    const ChunkData& cd = it->second;
                    ImVec4 pickCol(0, 0, 0, 0);

                    // 优先级 1: 传送门区块 (传送门为极其罕见的人工/天然跃迁点，最高优先级)
                    if (MapRenderState::anarchyPortals && (cd.flags & FLAG_PORTAL)) {
                        pickCol = ImVec4(MapRenderState::anarchyPortalsColor[0], MapRenderState::anarchyPortalsColor[1],
                                         MapRenderState::anarchyPortalsColor[2], MapRenderState::anarchyPortalsColor[3]);
                    }
                    // 优先级 2: 传送门跳转疑似基地 (PortalSkip)
                    else if (MapRenderState::anarchyPortalSkip && (cd.flags & FLAG_PORTAL_SKIP)) {
                        pickCol = ImVec4(MapRenderState::anarchyPortalSkipColor[0], MapRenderState::anarchyPortalSkipColor[1],
                                         MapRenderState::anarchyPortalSkipColor[2], MapRenderState::anarchyPortalSkipColor[3]);
                    }
                    // 优先级 3: 下界垂直岩浆柱
                    else if (MapRenderState::anarchyLavaColumns && (cd.flags & FLAG_LAVA_COLUMN) && currentDim == 1) {
                        float a = std::clamp(MapRenderState::anarchyLavaColumnsAlphaShift + cd.lavaHeight * MapRenderState::anarchyLavaColumnsAlphaStep, 0.10f, 0.90f);
                        pickCol = ImVec4(MapRenderState::anarchyLavaColumnsColor[0], MapRenderState::anarchyLavaColumnsColor[1],
                                         MapRenderState::anarchyLavaColumnsColor[2], a);
                    }
                    // 优先级 4: 调色板反向 - 人工建造痕迹区块 (玩家基地 / 关键据点)
                    else if (MapRenderState::anarchyPaletteNewChunksInverse && (cd.flags & FLAG_PALETTE_NEW_CHUNK_INV)) {
                        pickCol = ImVec4(MapRenderState::anarchyPaletteNewChunksInverseColor[0], MapRenderState::anarchyPaletteNewChunksInverseColor[1],
                                         MapRenderState::anarchyPaletteNewChunksInverseColor[2], MapRenderState::anarchyPaletteNewChunksInverseColor[3]);
                    }
                    // 优先级 5: 液体流动新区块 (Liquid NewChunks)
                    else if (MapRenderState::anarchyLiquidNewChunks && (cd.flags & FLAG_LIQUID_NEW_CHUNK)) {
                        pickCol = ImVec4(MapRenderState::anarchyLiquidNewChunksColor[0], MapRenderState::anarchyLiquidNewChunksColor[1],
                                         MapRenderState::anarchyLiquidNewChunksColor[2], MapRenderState::anarchyLiquidNewChunksColor[3]);
                    }
                    // 优先级 6: 历史传承老区块 (OldChunks)
                    else if (MapRenderState::anarchyOldChunks && (cd.flags & FLAG_OLD_CHUNK)) {
                        pickCol = ImVec4(MapRenderState::anarchyOldChunksColor[0], MapRenderState::anarchyOldChunksColor[1],
                                         MapRenderState::anarchyOldChunksColor[2], MapRenderState::anarchyOldChunksColor[3]);
                    }
                    // 优先级 6.5: 基岩版 26.51 融合升级老区块
                    else if (MapRenderState::anarchyOldChunks && (cd.flags & FLAG_OLD_CHUNK_BLENDED)) {
                        pickCol = ImVec4(MapRenderState::anarchyOldChunksBlendedColor[0], MapRenderState::anarchyOldChunksBlendedColor[1],
                                         MapRenderState::anarchyOldChunksBlendedColor[2], MapRenderState::anarchyOldChunksBlendedColor[3]);
                    }
                    // 优先级 7: 经典旧生物群系
                    else if (MapRenderState::anarchyOldBiomes && (cd.flags & FLAG_OLD_BIOME) && currentDim == 0) {
                        pickCol = ImVec4(MapRenderState::anarchyOldBiomesColor[0], MapRenderState::anarchyOldBiomesColor[1],
                                         MapRenderState::anarchyOldBiomesColor[2], MapRenderState::anarchyOldBiomesColor[3]);
                    }
                    // 优先级 8: 液体反向 (已结算静态老区块)
                    else if (MapRenderState::anarchyLiquidNewChunksInverse && (cd.flags & FLAG_LIQUID_NEW_CHUNK_INV)) {
                        pickCol = ImVec4(MapRenderState::anarchyLiquidNewChunksInverseColor[0], MapRenderState::anarchyLiquidNewChunksInverseColor[1],
                                         MapRenderState::anarchyLiquidNewChunksInverseColor[2], MapRenderState::anarchyLiquidNewChunksInverseColor[3]);
                    }
                    // 优先级 9: 调色板原生新区块
                    else if (MapRenderState::anarchyPaletteNewChunks && (cd.flags & FLAG_PALETTE_NEW_CHUNK)) {
                        pickCol = ImVec4(MapRenderState::anarchyPaletteNewChunksColor[0], MapRenderState::anarchyPaletteNewChunksColor[1],
                                         MapRenderState::anarchyPaletteNewChunksColor[2], MapRenderState::anarchyPaletteNewChunksColor[3]);
                    }
                    // 优先级 10: 1.13+ 现代新区块
                    else if (MapRenderState::anarchyOldChunksInverse && (cd.flags & FLAG_OLD_CHUNK_INV)) {
                        pickCol = ImVec4(MapRenderState::anarchyOldChunksInverseColor[0], MapRenderState::anarchyOldChunksInverseColor[1],
                                         MapRenderState::anarchyOldChunksInverseColor[2], MapRenderState::anarchyOldChunksInverseColor[3]);
                    }

                    if (pickCol.w <= 0.01f) continue;

                    ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(pickCol);
                    ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(pickCol.x, pickCol.y, pickCol.z, std::min(1.0f, pickCol.w * 1.5f + 0.2f)));

                    float lx0 = ((float)(cxChunk * 16)      - viewPX) * scale;
                    float lx1 = ((float)(cxChunk * 16 + 16) - viewPX) * scale;
                    float lz0 = ((float)(cz * 16)      - viewPZ) * scale;
                    float lz1 = ((float)(cz * 16 + 16) - viewPZ) * scale;

                    ImVec2 p0(cx + lx0 * c_rot - lz0 * s_rot, cy + lx0 * s_rot + lz0 * c_rot);
                    ImVec2 p1(cx + lx1 * c_rot - lz0 * s_rot, cy + lx1 * s_rot + lz0 * c_rot);
                    ImVec2 p2(cx + lx1 * c_rot - lz1 * s_rot, cy + lx1 * s_rot + lz1 * c_rot);
                    ImVec2 p3(cx + lx0 * c_rot - lz1 * s_rot, cy + lx0 * s_rot + lz1 * c_rot);

                    if (isSquare) {
                        drawList->AddQuadFilled(p0, p1, p2, p3, fillCol);
                        drawList->AddLine(p0, p1, borderCol, 1.2f);
                        drawList->AddLine(p1, p2, borderCol, 1.2f);
                        drawList->AddLine(p2, p3, borderCol, 1.2f);
                        drawList->AddLine(p3, p0, borderCol, 1.2f);
                    } else {
                        // 圆形小地图：采用细分平滑贴合机制，解决边缘裁切
                        float d0 = (p0.x - cx) * (p0.x - cx) + (p0.y - cy) * (p0.y - cy);
                        float d1 = (p1.x - cx) * (p1.x - cx) + (p1.y - cy) * (p1.y - cy);
                        float d2 = (p2.x - cx) * (p2.x - cx) + (p2.y - cy) * (p2.y - cy);
                        float d3 = (p3.x - cx) * (p3.x - cx) + (p3.y - cy) * (p3.y - cy);

                        if (d0 <= screenRSq && d1 <= screenRSq && d2 <= screenRSq && d3 <= screenRSq) {
                            drawList->AddQuadFilled(p0, p1, p2, p3, fillCol);
                        } else {
                            constexpr int SUB = 4;
                            float stepX = (lx1 - lx0) / (float)SUB;
                            float stepZ = (lz1 - lz0) / (float)SUB;
                            for (int sz = 0; sz < SUB; ++sz) {
                                float slz0 = lz0 + sz * stepZ;
                                float slz1 = slz0 + stepZ;
                                for (int sx = 0; sx < SUB; ++sx) {
                                    float slx0 = lx0 + sx * stepX;
                                    float slx1 = slx0 + stepX;
                                    float midX = (slx0 + slx1) * 0.5f;
                                    float midZ = (slz0 + slz1) * 0.5f;
                                    float rmidx = midX * c_rot - midZ * s_rot;
                                    float rmidy = midX * s_rot + midZ * c_rot;
                                    if (rmidx * rmidx + rmidy * rmidy <= screenRSq) {
                                        ImVec2 sp0(cx + slx0 * c_rot - slz0 * s_rot, cy + slx0 * s_rot + slz0 * c_rot);
                                        ImVec2 sp1(cx + slx1 * c_rot - slz0 * s_rot, cy + slx1 * s_rot + slz0 * c_rot);
                                        ImVec2 sp2(cx + slx1 * c_rot - slz1 * s_rot, cy + slx1 * s_rot + slz1 * c_rot);
                                        ImVec2 sp3(cx + slx0 * c_rot - slz1 * s_rot, cy + slx0 * s_rot + slz1 * c_rot);
                                        drawList->AddQuadFilled(sp0, sp1, sp2, sp3, fillCol);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 通用坐标道路系统仅在全屏大地图显示 (RenderBigMapOverlays)，不应用到小地图

        if (isSquare) drawList->PopClipRect();
    }

    // =========================================================================
    // 全屏大地图渲染 (Overlays & Highways)
    // =========================================================================
    void RenderBigMapOverlays(ImDrawList* drawList, const ImVec2& canvasOrigin, float zm,
                             float smoothPX, float smoothPZ, float offX, float offZ,
                             const ImVec2& canvasSize, int viewDim) {
        if (!drawList || viewDim < 0 || viewDim >= 3) return;

        constexpr float CHUNK_SIZE = 16.0f;
        float wX0 = smoothPX - (canvasOrigin.x + offX) / zm;
        float wX1 = smoothPX + (canvasSize.x - canvasOrigin.x - offX) / zm;
        float wZ0 = smoothPZ - (canvasOrigin.y + offZ) / zm;
        float wZ1 = smoothPZ + (canvasSize.y - canvasOrigin.y - offZ) / zm;

        if (wX0 > wX1) std::swap(wX0, wX1);
        if (wZ0 > wZ1) std::swap(wZ0, wZ1);

        int minChunkX = static_cast<int>(std::floor(wX0 / CHUNK_SIZE));
        int maxChunkX = static_cast<int>(std::ceil (wX1 / CHUNK_SIZE));
        int minChunkZ = static_cast<int>(std::floor(wZ0 / CHUNK_SIZE));
        int maxChunkZ = static_cast<int>(std::ceil (wZ1 / CHUNK_SIZE));

        auto toScreen = [&](float wx, float wz) -> ImVec2 {
            float sx = canvasOrigin.x + (wx - smoothPX) * zm + offX;
            float sy = canvasOrigin.y + (wz - smoothPZ) * zm + offZ;
            return ImVec2(sx, sy);
        };

        // 1. 渲染区块高亮 (按稀有度与关键度优先着色)
        {
            std::lock_guard<std::mutex> lock(g_anarchyMutex);
            const auto& dimMap = g_chunkHighlights[viewDim];
            int64_t totalChunksInView = (int64_t)(maxChunkX - minChunkX + 1) * (maxChunkZ - minChunkZ + 1);

            auto renderOneChunk = [&](int cxChunk, int cz, const ChunkData& cd) {
                ImVec4 pickCol(0, 0, 0, 0);

                // 优先级 1: 传送门区块
                if (MapRenderState::anarchyPortals && (cd.flags & FLAG_PORTAL)) {
                    pickCol = ImVec4(MapRenderState::anarchyPortalsColor[0], MapRenderState::anarchyPortalsColor[1],
                                     MapRenderState::anarchyPortalsColor[2], MapRenderState::anarchyPortalsColor[3]);
                }
                // 优先级 2: 传送门跳转疑似基地 (PortalSkip)
                else if (MapRenderState::anarchyPortalSkip && (cd.flags & FLAG_PORTAL_SKIP)) {
                    pickCol = ImVec4(MapRenderState::anarchyPortalSkipColor[0], MapRenderState::anarchyPortalSkipColor[1],
                                     MapRenderState::anarchyPortalSkipColor[2], MapRenderState::anarchyPortalSkipColor[3]);
                }
                // 优先级 3: 下界垂直岩浆柱
                else if (MapRenderState::anarchyLavaColumns && (cd.flags & FLAG_LAVA_COLUMN) && viewDim == 1) {
                    float a = std::clamp(MapRenderState::anarchyLavaColumnsAlphaShift + cd.lavaHeight * MapRenderState::anarchyLavaColumnsAlphaStep, 0.10f, 0.90f);
                    pickCol = ImVec4(MapRenderState::anarchyLavaColumnsColor[0], MapRenderState::anarchyLavaColumnsColor[1],
                                     MapRenderState::anarchyLavaColumnsColor[2], a);
                }
                // 优先级 4: 人工建造痕迹区块 (调色板反向)
                else if (MapRenderState::anarchyPaletteNewChunksInverse && (cd.flags & FLAG_PALETTE_NEW_CHUNK_INV)) {
                    pickCol = ImVec4(MapRenderState::anarchyPaletteNewChunksInverseColor[0], MapRenderState::anarchyPaletteNewChunksInverseColor[1],
                                     MapRenderState::anarchyPaletteNewChunksInverseColor[2], MapRenderState::anarchyPaletteNewChunksInverseColor[3]);
                }
                // 优先级 5: 液体流动新区块 (Liquid NewChunks)
                else if (MapRenderState::anarchyLiquidNewChunks && (cd.flags & FLAG_LIQUID_NEW_CHUNK)) {
                    pickCol = ImVec4(MapRenderState::anarchyLiquidNewChunksColor[0], MapRenderState::anarchyLiquidNewChunksColor[1],
                                     MapRenderState::anarchyLiquidNewChunksColor[2], MapRenderState::anarchyLiquidNewChunksColor[3]);
                }
                // 优先级 6: 历史传承老区块 (OldChunks)
                else if (MapRenderState::anarchyOldChunks && (cd.flags & FLAG_OLD_CHUNK)) {
                    pickCol = ImVec4(MapRenderState::anarchyOldChunksColor[0], MapRenderState::anarchyOldChunksColor[1],
                                     MapRenderState::anarchyOldChunksColor[2], MapRenderState::anarchyOldChunksColor[3]);
                }
                // 优先级 6.5: 基岩版 26.51 融合升级老区块
                else if (MapRenderState::anarchyOldChunks && (cd.flags & FLAG_OLD_CHUNK_BLENDED)) {
                    pickCol = ImVec4(MapRenderState::anarchyOldChunksBlendedColor[0], MapRenderState::anarchyOldChunksBlendedColor[1],
                                     MapRenderState::anarchyOldChunksBlendedColor[2], MapRenderState::anarchyOldChunksBlendedColor[3]);
                }
                // 优先级 7: 经典旧生物群系
                else if (MapRenderState::anarchyOldBiomes && (cd.flags & FLAG_OLD_BIOME) && viewDim == 0) {
                    pickCol = ImVec4(MapRenderState::anarchyOldBiomesColor[0], MapRenderState::anarchyOldBiomesColor[1],
                                     MapRenderState::anarchyOldBiomesColor[2], MapRenderState::anarchyOldBiomesColor[3]);
                }
                // 优先级 8: 液体反向 (已结算老区块)
                else if (MapRenderState::anarchyLiquidNewChunksInverse && (cd.flags & FLAG_LIQUID_NEW_CHUNK_INV)) {
                    pickCol = ImVec4(MapRenderState::anarchyLiquidNewChunksInverseColor[0], MapRenderState::anarchyLiquidNewChunksInverseColor[1],
                                     MapRenderState::anarchyLiquidNewChunksInverseColor[2], MapRenderState::anarchyLiquidNewChunksInverseColor[3]);
                }
                // 优先级 9: 调色板原生新区块
                else if (MapRenderState::anarchyPaletteNewChunks && (cd.flags & FLAG_PALETTE_NEW_CHUNK)) {
                    pickCol = ImVec4(MapRenderState::anarchyPaletteNewChunksColor[0], MapRenderState::anarchyPaletteNewChunksColor[1],
                                     MapRenderState::anarchyPaletteNewChunksColor[2], MapRenderState::anarchyPaletteNewChunksColor[3]);
                }
                // 优先级 10: 现代版本新区块
                else if (MapRenderState::anarchyOldChunksInverse && (cd.flags & FLAG_OLD_CHUNK_INV)) {
                    pickCol = ImVec4(MapRenderState::anarchyOldChunksInverseColor[0], MapRenderState::anarchyOldChunksInverseColor[1],
                                     MapRenderState::anarchyOldChunksInverseColor[2], MapRenderState::anarchyOldChunksInverseColor[3]);
                }

                if (pickCol.w <= 0.01f) return;

                ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(pickCol);
                ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(pickCol.x, pickCol.y, pickCol.z, std::min(1.0f, pickCol.w * 1.5f + 0.25f)));

                ImVec2 p0 = toScreen(static_cast<float>(cxChunk * 16), static_cast<float>(cz * 16));
                ImVec2 p1 = toScreen(static_cast<float>(cxChunk * 16 + 16), static_cast<float>(cz * 16 + 16));

                // 极端远景子像素点状增强保护，确保点位清晰可见且不造成渲染断层
                if (p1.x - p0.x < 1.0f) p1.x = p0.x + 1.0f;
                if (p1.y - p0.y < 1.0f) p1.y = p0.y + 1.0f;

                drawList->AddRectFilled(p0, p1, fillCol);
                if (zm * 16.0f >= 6.0f) {
                    drawList->AddRect(p0, p1, borderCol, 0.0f, 0, 1.2f);
                }
            };

            if (totalChunksInView > (int64_t)dimMap.size() || totalChunksInView > 4096) {
                // 视野跨度极大时直接反向遍历稀疏存储的高亮数据表，从视口网格面积复杂度暴降至高亮记录数
                for (const auto& [key, cd] : dimMap) {
                    int cxChunk = 0, cz = 0;
                    SplitChunkKey(key, cxChunk, cz);
                    if (cxChunk >= minChunkX && cxChunk <= maxChunkX && cz >= minChunkZ && cz <= maxChunkZ) {
                        renderOneChunk(cxChunk, cz, cd);
                    }
                }
            } else {
                // 视野范围极小、高放大倍数时按视口网格遍历
                for (int cz = minChunkZ; cz <= maxChunkZ; ++cz) {
                    for (int cxChunk = minChunkX; cxChunk <= maxChunkX; ++cxChunk) {
                        uint64_t key = MakeChunkKey(cxChunk, cz);
                        auto it = dimMap.find(key);
                        if (it != dimMap.end()) {
                            renderOneChunk(cxChunk, cz, it->second);
                        }
                    }
                }
            }
        }

        // 2. 渲染通用坐标道路系统 (Universal Highways)
        bool highwayDimAllowed = (
            (viewDim == 0 && MapRenderState::anarchyHighwaysDimOverworld) ||
            (viewDim == 1 && MapRenderState::anarchyHighwaysDimNether) ||
            (viewDim == 2 && MapRenderState::anarchyHighwaysDimEnd)
        );

        if (MapRenderState::anarchyHighways && highwayDimAllowed) {
            ImU32 hwCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                MapRenderState::anarchyHighwaysColor[0], MapRenderState::anarchyHighwaysColor[1],
                MapRenderState::anarchyHighwaysColor[2], MapRenderState::anarchyHighwaysColor[3]
            ));
            ImU32 hwSecCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                MapRenderState::anarchyHighwaysSecondaryColor[0], MapRenderState::anarchyHighwaysSecondaryColor[1],
                MapRenderState::anarchyHighwaysSecondaryColor[2], MapRenderState::anarchyHighwaysSecondaryColor[3]
            ));
            float hwWidth = MapRenderState::anarchyHighwaysWidth;
            float maxHighwayCoord = static_cast<float>(MapRenderState::anarchyHighwaysMaxCoord);

            // 主坐标轴 X=0
            if (MapRenderState::anarchyHighwaysMainAxes && wX0 <= 0.0f && wX1 >= 0.0f) {
                float zMin = std::max(wZ0, -maxHighwayCoord);
                float zMax = std::min(wZ1,  maxHighwayCoord);
                if (zMin < zMax) {
                    drawList->AddLine(toScreen(0.0f, zMin), toScreen(0.0f, zMax), hwCol, hwWidth);
                    if (MapRenderState::anarchyHighwaysShowLabels && zm >= 0.04f) {
                        ImVec2 lblPos = toScreen(0.0f, (zMin + zMax) * 0.5f);
                        drawList->AddRectFilled(ImVec2(lblPos.x + 3, lblPos.y - 8), ImVec2(lblPos.x + 42, lblPos.y + 8), IM_COL32(0, 0, 0, 180), 2.0f);
                        drawList->AddText(ImVec2(lblPos.x + 5, lblPos.y - 7), IM_COL32(255, 255, 255, 230), "X = 0");
                    }
                }
            }
            // 主坐标轴 Z=0
            if (MapRenderState::anarchyHighwaysMainAxes && wZ0 <= 0.0f && wZ1 >= 0.0f) {
                float xMin = std::max(wX0, -maxHighwayCoord);
                float xMax = std::min(wX1,  maxHighwayCoord);
                if (xMin < xMax) {
                    drawList->AddLine(toScreen(xMin, 0.0f), toScreen(xMax, 0.0f), hwCol, hwWidth);
                    if (MapRenderState::anarchyHighwaysShowLabels && zm >= 0.04f) {
                        ImVec2 lblPos = toScreen((xMin + xMax) * 0.5f, 0.0f);
                        drawList->AddRectFilled(ImVec2(lblPos.x - 20, lblPos.y + 3), ImVec2(lblPos.x + 22, lblPos.y + 19), IM_COL32(0, 0, 0, 180), 2.0f);
                        drawList->AddText(ImVec2(lblPos.x - 18, lblPos.y + 4), IM_COL32(255, 255, 255, 230), "Z = 0");
                    }
                }
            }
            // 对角线道路 (X = ±Z)
            if (MapRenderState::anarchyHighwaysDiagAxes) {
                float tMin = std::max({ wX0, wZ0, -maxHighwayCoord });
                float tMax = std::min({ wX1, wZ1,  maxHighwayCoord });
                if (tMin < tMax) {
                    drawList->AddLine(toScreen(tMin, tMin), toScreen(tMax, tMax), hwCol, hwWidth);
                }
                float xMin = std::max({ wX0, -wZ1, -maxHighwayCoord });
                float xMax = std::min({ wX1, -wZ0,  maxHighwayCoord });
                if (xMin < xMax) {
                    drawList->AddLine(toScreen(xMin, -xMin), toScreen(xMax, -xMax), hwCol, hwWidth);
                }
            }

            // 坐标等距平行网格道路 (Grid Lines)
            int gStep = std::max(500, MapRenderState::anarchyHighwaysGridSpacing);
            if (MapRenderState::anarchyHighwaysGridRoads && ((float)gStep * zm >= 6.0f)) {
                int kMinX = static_cast<int>(std::floor(std::max(wX0, -maxHighwayCoord) / gStep));
                int kMaxX = static_cast<int>(std::ceil (std::min(wX1,  maxHighwayCoord) / gStep));
                float zMin = std::max(wZ0, -maxHighwayCoord);
                float zMax = std::min(wZ1,  maxHighwayCoord);
                if (zMin < zMax) {
                    for (int k = kMinX; k <= kMaxX; ++k) {
                        if (k == 0 && MapRenderState::anarchyHighwaysMainAxes) continue;
                        float gx = k * (float)gStep;
                        drawList->AddLine(toScreen(gx, zMin), toScreen(gx, zMax), hwSecCol, hwWidth * 0.8f);
                        if (MapRenderState::anarchyHighwaysShowLabels && zm >= 0.04f) {
                            ImVec2 lblPos = toScreen(gx, (zMin + zMax) * 0.5f);
                            std::string lbl = "X=" + std::to_string(static_cast<int>(gx));
                            drawList->AddRectFilled(ImVec2(lblPos.x + 2, lblPos.y - 7), ImVec2(lblPos.x + 8.0f * lbl.size() + 2, lblPos.y + 7), IM_COL32(0, 0, 0, 180), 2.0f);
                            drawList->AddText(ImVec2(lblPos.x + 4, lblPos.y - 6), IM_COL32(230, 230, 230, 220), lbl.c_str());
                        }
                    }
                }

                int kMinZ = static_cast<int>(std::floor(std::max(wZ0, -maxHighwayCoord) / gStep));
                int kMaxZ = static_cast<int>(std::ceil (std::min(wZ1,  maxHighwayCoord) / gStep));
                float xMin = std::max(wX0, -maxHighwayCoord);
                float xMax = std::min(wX1,  maxHighwayCoord);
                if (xMin < xMax) {
                    for (int k = kMinZ; k <= kMaxZ; ++k) {
                        if (k == 0 && MapRenderState::anarchyHighwaysMainAxes) continue;
                        float gz = k * (float)gStep;
                        drawList->AddLine(toScreen(xMin, gz), toScreen(xMax, gz), hwSecCol, hwWidth * 0.8f);
                        if (MapRenderState::anarchyHighwaysShowLabels && zm >= 0.04f) {
                            ImVec2 lblPos = toScreen((xMin + xMax) * 0.5f, gz);
                            std::string lbl = "Z=" + std::to_string(static_cast<int>(gz));
                            drawList->AddRectFilled(ImVec2(lblPos.x - 20, lblPos.y + 2), ImVec2(lblPos.x - 20 + 8.0f * lbl.size(), lblPos.y + 16), IM_COL32(0, 0, 0, 180), 2.0f);
                            drawList->AddText(ImVec2(lblPos.x - 18, lblPos.y + 3), IM_COL32(230, 230, 230, 220), lbl.c_str());
                        }
                    }
                }
            }

            // 同心方环道路 (Square Rings)
            if (MapRenderState::anarchyHighwaysRingRoads) {
                auto drawBigMapSquareRing = [&](float R) {
                    if (R > maxHighwayCoord || R <= 0.0f) return;
                    if (wX1 < -R || wX0 > R) return;
                    if (wZ1 < -R || wZ0 > R) return;

                    if (wZ0 <= -R && wZ1 >= -R) {
                        float x0 = std::max(wX0, -R);
                        float x1 = std::min(wX1,  R);
                        if (x0 < x1) {
                            drawList->AddLine(toScreen(x0, -R), toScreen(x1, -R), hwCol, hwWidth);
                            if (MapRenderState::anarchyHighwaysShowLabels && zm >= 0.04f) {
                                ImVec2 lblPos = toScreen((x0 + x1) * 0.5f, -R);
                                std::string lbl = "R=" + std::to_string(static_cast<int>(R));
                                drawList->AddRectFilled(ImVec2(lblPos.x - 15, lblPos.y - 15), ImVec2(lblPos.x - 15 + 8.0f * lbl.size(), lblPos.y - 1), IM_COL32(0, 0, 0, 180), 2.0f);
                                drawList->AddText(ImVec2(lblPos.x - 13, lblPos.y - 14), IM_COL32(255, 230, 100, 230), lbl.c_str());
                            }
                        }
                    }
                    if (wZ0 <= R && wZ1 >= R) {
                        float x0 = std::max(wX0, -R);
                        float x1 = std::min(wX1,  R);
                        if (x0 < x1) drawList->AddLine(toScreen(x0, R), toScreen(x1, R), hwCol, hwWidth);
                    }
                    if (wX0 <= -R && wX1 >= -R) {
                        float z0 = std::max(wZ0, -R);
                        float z1 = std::min(wZ1,  R);
                        if (z0 < z1) drawList->AddLine(toScreen(-R, z0), toScreen(-R, z1), hwCol, hwWidth);
                    }
                    if (wX0 <= R && wX1 >= R) {
                        float z0 = std::max(wZ0, -R);
                        float z1 = std::min(wZ1,  R);
                        if (z0 < z1) drawList->AddLine(toScreen(R, z0), toScreen(R, z1), hwCol, hwWidth);
                    }
                };

                if (MapRenderState::anarchyHighwaysMode == 0) {
                    for (size_t i = 0; i < kRingRoadCount; ++i) {
                        drawBigMapSquareRing(static_cast<float>(kRingRoads[i]));
                    }
                } else if (MapRenderState::anarchyHighwaysMode == 1) {
                    int rStep = std::max(500, MapRenderState::anarchyHighwaysRingInterval);
                    float minR = 0.0f;
                    if (wX0 > 0.0f) minR = std::max(minR, wX0);
                    else if (wX1 < 0.0f) minR = std::max(minR, -wX1);
                    if (wZ0 > 0.0f) minR = std::max(minR, wZ0);
                    else if (wZ1 < 0.0f) minR = std::max(minR, -wZ1);

                    float maxR = std::max({ std::abs(wX0), std::abs(wX1), std::abs(wZ0), std::abs(wZ1) });
                    maxR = std::min(maxR, maxHighwayCoord);
                    int kStart = std::max(1, static_cast<int>(std::floor(minR / rStep)));
                    int kEnd   = static_cast<int>(std::ceil(maxR / rStep));
                    for (int k = kStart; k <= kEnd; ++k) {
                        drawBigMapSquareRing(k * (float)rStep);
                    }
                } else {
                    auto customList = ParseCustomDistances(MapRenderState::anarchyHighwaysCustomRingsStr);
                    for (int r : customList) {
                        drawBigMapSquareRing(static_cast<float>(r));
                    }
                }
            }

            // 同心菱形道路 (Diamond Roads)
            if (MapRenderState::anarchyHighwaysDiamondRoads) {
                auto drawBigMapDiamond = [&](float R) {
                    if (R > maxHighwayCoord || R <= 0.0f) return;
                    if (wX1 < -R || wX0 > R) return;
                    if (wZ1 < -R || wZ0 > R) return;

                    ImVec2 ptN = toScreen(0.0f, -R);
                    ImVec2 ptE = toScreen( R, 0.0f);
                    ImVec2 ptS = toScreen(0.0f,  R);
                    ImVec2 ptW = toScreen(-R, 0.0f);

                    drawList->AddLine(ptN, ptE, hwSecCol, hwWidth);
                    drawList->AddLine(ptE, ptS, hwSecCol, hwWidth);
                    drawList->AddLine(ptS, ptW, hwSecCol, hwWidth);
                    drawList->AddLine(ptW, ptN, hwSecCol, hwWidth);

                    if (MapRenderState::anarchyHighwaysShowLabels && zm >= 0.04f) {
                        ImVec2 lblPos = toScreen(R * 0.5f, -R * 0.5f);
                        if (lblPos.x >= canvasOrigin.x && lblPos.x <= canvasOrigin.x + canvasSize.x &&
                            lblPos.y >= canvasOrigin.y && lblPos.y <= canvasOrigin.y + canvasSize.y) {
                            std::string lbl = "Dia=" + std::to_string(static_cast<int>(R));
                            drawList->AddRectFilled(ImVec2(lblPos.x + 3, lblPos.y - 7), ImVec2(lblPos.x + 3 + 8.0f * lbl.size(), lblPos.y + 7), IM_COL32(0, 0, 0, 180), 2.0f);
                            drawList->AddText(ImVec2(lblPos.x + 5, lblPos.y - 6), IM_COL32(100, 220, 255, 230), lbl.c_str());
                        }
                    }
                };

                if (MapRenderState::anarchyHighwaysMode == 0) {
                    for (size_t i = 0; i < kDiamondCount; ++i) {
                        drawBigMapDiamond(static_cast<float>(kDiamonds[i]));
                    }
                } else if (MapRenderState::anarchyHighwaysMode == 1) {
                    int dStep = std::max(500, MapRenderState::anarchyHighwaysDiamondInterval);
                    float minR = 0.0f;
                    if (wX0 > 0.0f) minR = std::max(minR, wX0);
                    else if (wX1 < 0.0f) minR = std::max(minR, -wX1);
                    if (wZ0 > 0.0f) minR = std::max(minR, wZ0);
                    else if (wZ1 < 0.0f) minR = std::max(minR, -wZ1);

                    float maxR = std::max({ std::abs(wX0), std::abs(wX1), std::abs(wZ0), std::abs(wZ1) });
                    maxR = std::min(maxR, maxHighwayCoord);
                    int kStart = std::max(1, static_cast<int>(std::floor(minR / dStep)));
                    int kEnd   = static_cast<int>(std::ceil(maxR / dStep));
                    for (int k = kStart; k <= kEnd; ++k) {
                        drawBigMapDiamond(k * (float)dStep);
                    }
                } else {
                    auto customList = ParseCustomDistances(MapRenderState::anarchyHighwaysCustomRingsStr);
                    for (int r : customList) {
                        drawBigMapDiamond(static_cast<float>(r));
                    }
                }
            }
        }
    }

    // =========================================================================
    // UI 设置面板
    // =========================================================================
    void RenderSettingsPanel() {
        if (!MapRenderState::showAnarchySettings) return;

        float curScale = MapRenderState::globalUIScale;
        ImGui::SetNextWindowSize(ImVec2(560.0f * curScale, 600.0f * curScale), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(LanguageManager::GetText("ANARCHY_SETTINGS_TITLE"), &MapRenderState::showAnarchySettings)) {
            ImGui::End();
            return;
        }

        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", LanguageManager::GetText("ANARCHY_SETTINGS_SUBTITLE"));
        ImGui::Separator();

        if (ImGui::BeginChild("AnarchyScrollArea", ImVec2(0, -42.0f * curScale), true)) {

            // 1. 液体流动新区块 (Liquid NewChunks)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_LIQUID_NEW_CHUNKS"), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_LIQUID_NEW_CHUNKS_ENABLE"), &MapRenderState::anarchyLiquidNewChunks)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_LIQUID_NEW_CHUNKS_INV"), &MapRenderState::anarchyLiquidNewChunksInverse)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_LIQUID_NEW_CHUNKS_Y0"), &MapRenderState::anarchyLiquidNewChunksOnlyAboveY0)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_NEW_CHUNKS"), MapRenderState::anarchyLiquidNewChunksColor);
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_NEW_CHUNKS_INV"), MapRenderState::anarchyLiquidNewChunksInverseColor);
                ImGui::Spacing();
            }

            // 2. 调色板/原生新区块 (Palette NewChunks)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_PALETTE_NEW_CHUNKS"))) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_PALETTE_NEW_CHUNKS_ENABLE"), &MapRenderState::anarchyPaletteNewChunks)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_PALETTE_NEW_CHUNKS_INV"), &MapRenderState::anarchyPaletteNewChunksInverse)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_PALETTE_NEW_CHUNKS"), MapRenderState::anarchyPaletteNewChunksColor);
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_PALETTE_NEW_CHUNKS_INV"), MapRenderState::anarchyPaletteNewChunksInverseColor);
                ImGui::Spacing();
            }

            // 3. 老区块探测 (OldChunks)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_OLD_CHUNKS"), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_OLD_CHUNKS_ENABLE"), &MapRenderState::anarchyOldChunks)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_OLD_CHUNKS_INV"), &MapRenderState::anarchyOldChunksInverse)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }

                // 探测模式切换 (4 种模式)
                const char* oldModes[] = {
                    LanguageManager::GetText("ANARCHY_OLD_CHUNKS_MODE_CLASSIC"),
                    LanguageManager::GetText("ANARCHY_OLD_CHUNKS_MODE_BEDROCK_LEGACY"),
                    LanguageManager::GetText("ANARCHY_OLD_CHUNKS_MODE_BEDROCK_2651"),
                    LanguageManager::GetText("ANARCHY_OLD_CHUNKS_MODE_MULTI_ERA")
                };
                int currentOldMode = std::clamp(MapRenderState::anarchyOldChunksMode, 0, 3);
                if (ImGui::Combo(LanguageManager::GetText("ANARCHY_OLD_CHUNKS_MODE"), &currentOldMode, oldModes, IM_ARRAYSIZE(oldModes))) {
                    MapRenderState::anarchyOldChunksMode = currentOldMode;
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }

                // 26.51 融合升级老区块选项
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_OLD_CHUNKS_BLENDED_ENABLE"), &MapRenderState::anarchyOldChunksBedrockBlending)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }

                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_OLD_CHUNKS"), MapRenderState::anarchyOldChunksColor);
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_OLD_CHUNKS_INV"), MapRenderState::anarchyOldChunksInverseColor);
                if (MapRenderState::anarchyOldChunksBedrockBlending) {
                    ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_OLD_CHUNKS_BLENDED"), MapRenderState::anarchyOldChunksBlendedColor);
                }
                ImGui::Spacing();
            }

            // 4. 经典旧生物群系 (OldBiomes)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_OLD_BIOMES"))) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_OLD_BIOMES_ENABLE"), &MapRenderState::anarchyOldBiomes)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_OLD_BIOMES"), MapRenderState::anarchyOldBiomesColor);
                static char seedBuf[64] = "-4172144997902289642";
                InputIMEHelper::InputTextWithIME(LanguageManager::GetText("ANARCHY_OLD_BIOMES_SEED"), seedBuf, sizeof(seedBuf));
                if (ImGui::Button(LanguageManager::GetText("ANARCHY_APPLY_SEED"))) {
                    try {
                        MapRenderState::anarchyOldBiomesSeed = std::stoll(seedBuf);
                        EnsureOldBiomeGenerator(MapRenderState::anarchyOldBiomesSeed);
                        RequestRescan();
                        LanguageManager::SaveConfig();
                    } catch (...) {}
                }
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("RESET_CLASSIC_SEED"))) {
                    MapRenderState::anarchyOldBiomesSeed = -4172144997902289642LL;
                    std::snprintf(seedBuf, sizeof(seedBuf), "-4172144997902289642");
                    EnsureOldBiomeGenerator(MapRenderState::anarchyOldBiomesSeed);
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::Spacing();
            }

            // 5. 传送门高亮 (Portals)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_PORTALS"), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_PORTALS_ENABLE"), &MapRenderState::anarchyPortals)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_PORTALS"), MapRenderState::anarchyPortalsColor);
                ImGui::Spacing();
            }

            // 6. 传送门跳转检测 (PortalSkip Detection)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_PORTAL_SKIP"))) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_PORTAL_SKIP_ENABLE"), &MapRenderState::anarchyPortalSkip)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::SliderInt(LanguageManager::GetText("ANARCHY_PORTAL_SKIP_RADIUS"), &MapRenderState::anarchyPortalSkipRadius, 3, 16);
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_PORTAL_SKIP"), MapRenderState::anarchyPortalSkipColor);
                ImGui::Spacing();
            }

            // 7. 通用坐标道路系统 (Universal Highways)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_HIGHWAYS"), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_ENABLE"), &MapRenderState::anarchyHighways)) {
                    LanguageManager::SaveConfig();
                }

                // 维度过滤
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_DIM_OVERWORLD"), &MapRenderState::anarchyHighwaysDimOverworld)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_DIM_NETHER"), &MapRenderState::anarchyHighwaysDimNether)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_DIM_END"), &MapRenderState::anarchyHighwaysDimEnd)) {
                    LanguageManager::SaveConfig();
                }

                // 道路系统模式 (0=经典预设, 1=等距通用系统, 2=自由距离列表)
                const char* hwModes[] = {
                    LanguageManager::GetText("ANARCHY_HIGHWAYS_MODE_PRESET"),
                    LanguageManager::GetText("ANARCHY_HIGHWAYS_MODE_SPACED"),
                    LanguageManager::GetText("ANARCHY_HIGHWAYS_MODE_CUSTOM")
                };
                int currentHwMode = std::clamp(MapRenderState::anarchyHighwaysMode, 0, 2);
                if (ImGui::Combo(LanguageManager::GetText("ANARCHY_HIGHWAYS_MODE"), &currentHwMode, hwModes, IM_ARRAYSIZE(hwModes))) {
                    MapRenderState::anarchyHighwaysMode = currentHwMode;
                    LanguageManager::SaveConfig();
                }

                // 轴线与道路开关
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_MAIN_AXES"), &MapRenderState::anarchyHighwaysMainAxes)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_DIAG_AXES"), &MapRenderState::anarchyHighwaysDiagAxes)) {
                    LanguageManager::SaveConfig();
                }

                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_RING_ROADS"), &MapRenderState::anarchyHighwaysRingRoads)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_DIAMOND_ROADS"), &MapRenderState::anarchyHighwaysDiamondRoads)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SameLine();
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_GRID_ROADS"), &MapRenderState::anarchyHighwaysGridRoads)) {
                    LanguageManager::SaveConfig();
                }

                // 模式专属参数
                if (MapRenderState::anarchyHighwaysMode == 1) { // 等距通用模式
                    if (MapRenderState::anarchyHighwaysRingRoads) {
                        if (ImGui::SliderInt(LanguageManager::GetText("ANARCHY_HIGHWAYS_RING_INTERVAL"), &MapRenderState::anarchyHighwaysRingInterval, 100, 20000)) {
                            LanguageManager::SaveConfig();
                        }
                    }
                    if (MapRenderState::anarchyHighwaysDiamondRoads) {
                        if (ImGui::SliderInt(LanguageManager::GetText("ANARCHY_HIGHWAYS_DIAMOND_INTERVAL"), &MapRenderState::anarchyHighwaysDiamondInterval, 100, 20000)) {
                            LanguageManager::SaveConfig();
                        }
                    }
                    if (MapRenderState::anarchyHighwaysGridRoads) {
                        if (ImGui::SliderInt(LanguageManager::GetText("ANARCHY_HIGHWAYS_GRID_SPACING"), &MapRenderState::anarchyHighwaysGridSpacing, 200, 20000)) {
                            LanguageManager::SaveConfig();
                        }
                    }
                } else if (MapRenderState::anarchyHighwaysMode == 2) { // 自由距离列表模式
                    if (InputIMEHelper::InputTextWithIME(LanguageManager::GetText("ANARCHY_HIGHWAYS_CUSTOM_RINGS"), MapRenderState::anarchyHighwaysCustomRingsStr, sizeof(MapRenderState::anarchyHighwaysCustomRingsStr))) {
                        LanguageManager::SaveConfig();
                    }
                }

                // 最大范围与线宽、标签
                if (ImGui::SliderInt(LanguageManager::GetText("ANARCHY_HIGHWAYS_MAX_COORD"), &MapRenderState::anarchyHighwaysMaxCoord, 1000, 30000000)) {
                    LanguageManager::SaveConfig();
                }
                if (ImGui::SliderFloat(LanguageManager::GetText("ANARCHY_HIGHWAYS_WIDTH"), &MapRenderState::anarchyHighwaysWidth, 1.0f, 6.0f, "%.1f px")) {
                    LanguageManager::SaveConfig();
                }
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_HIGHWAYS_SHOW_LABELS"), &MapRenderState::anarchyHighwaysShowLabels)) {
                    LanguageManager::SaveConfig();
                }

                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_HIGHWAYS_COLOR"), MapRenderState::anarchyHighwaysColor);
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_HIGHWAYS_SECONDARY_COLOR"), MapRenderState::anarchyHighwaysSecondaryColor);
                ImGui::Spacing();
            }

            // 8. 下界岩浆柱探测 (Lava Columns)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_LAVA_COLUMNS"))) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_LAVA_COLUMNS_ENABLE"), &MapRenderState::anarchyLavaColumns)) {
                    RequestRescan();
                    LanguageManager::SaveConfig();
                }
                ImGui::SliderInt(LanguageManager::GetText("ANARCHY_LAVA_COLUMNS_MIN_HEIGHT"), &MapRenderState::anarchyLavaColumnsMinHeight, 5, 60);
                ImGui::SliderFloat(LanguageManager::GetText("ANARCHY_LAVA_COLUMNS_ALPHA_SHIFT"), &MapRenderState::anarchyLavaColumnsAlphaShift, 0.0f, 0.5f, "%.2f");
                ImGui::SliderFloat(LanguageManager::GetText("ANARCHY_LAVA_COLUMNS_ALPHA_STEP"), &MapRenderState::anarchyLavaColumnsAlphaStep, 0.005f, 0.05f, "%.3f");
                ImGui::ColorEdit4(LanguageManager::GetText("ANARCHY_COLOR_LAVA_COLUMNS"), MapRenderState::anarchyLavaColumnsColor);
                ImGui::Spacing();
            }

            // 9. 下界黑曜石平台透视 (Transparent Obsidian Roof)
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_OBSIDIAN_ROOF"), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_OBSIDIAN_ROOF_ENABLE"), &MapRenderState::transparentObsidianRoof)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::SliderInt(LanguageManager::GetText("ANARCHY_OBSIDIAN_ROOF_Y"), &MapRenderState::transparentObsidianRoofY, 64, 256);
                ImGui::SliderFloat(LanguageManager::GetText("ANARCHY_OBSIDIAN_ROOF_DARKENING"), &MapRenderState::roofObsidianDarkening, 0.2f, 1.0f, "%.2f");
                ImGui::SliderFloat(LanguageManager::GetText("ANARCHY_OBSIDIAN_ROOF_SNOW"), &MapRenderState::roofSnowOpacity, 0.0f, 1.0f, "%.2f");
                ImGui::Spacing();
            }

            // 10. 数据持久化与存储管理
            if (ImGui::CollapsingHeader(LanguageManager::GetText("ANARCHY_HEADER_DATA_MGMT"), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Checkbox(LanguageManager::GetText("ANARCHY_SAVE_LOAD_DISK"), &MapRenderState::anarchySaveLoadToDisk)) {
                    LanguageManager::SaveConfig();
                }
                ImGui::Text(LanguageManager::GetText("ANARCHY_HIGHLIGHT_STATS"),
                    GetHighlightCount(0), GetHighlightCount(1), GetHighlightCount(2));

                if (ImGui::Button(LanguageManager::GetText("ANARCHY_RESCAN_NOW"))) {
                    RequestRescan();
                }
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("ANARCHY_SAVE_NOW"))) {
                    SaveHighlights();
                }
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("ANARCHY_CLEAR_CURRENT_DIM"))) {
                    ClearHighlights(MapRenderState::currentDimensionId);
                    RequestRescan();
                }
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("ANARCHY_CLEAR_ALL"))) {
                    ClearHighlights(-1);
                    RequestRescan();
                }
            }

            ImGui::EndChild();
        }

        if (ImGui::Button(LanguageManager::GetText("CLOSE"), ImVec2(100.0f * curScale, 0))) {
            MapRenderState::showAnarchySettings = false;
        }

        ImGui::End();
    }

} // namespace AnarchyManager
