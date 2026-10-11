#pragma once
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <ll/api/memory/Hook.h>
#include <mc/client/game/ClientInstance.h>
#include <mc/client/player/LocalPlayer.h>
#include <mc/world/actor/player/Player.h>
#include <mc/deps/core/math/Color.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/biome/Biome.h>
#include <mc/world/level/levelgen/synth/PerlinSimplexNoise.h>
#include <mc/world/level/material/Material.h>
#if __has_include(<mc/world/level/material/MaterialType.h>)
#include <mc/world/level/material/MaterialType.h>
using ChiyanMapMaterialType = ::MaterialType;
#else
#include <mc/deps/shared_types/v1_26_20/block/MaterialType.h>
using ChiyanMapMaterialType = ::SharedTypes::v1_26_20::MaterialType;
#endif
#include "state/WaypointManager.h"
#include "state/DeathPointManager.h"
#include "state/MultiworldManager.h"
#include <mc/world/level/BlockPos.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/world/level/Level.h>
#include <mc/client/multiplayer/ClientLevel.h>
#include <mc/world/actor/Actor.h>
#include <mc/legacy/ActorRuntimeID.h>
#include <mc/world/effect/MobEffectInstance.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/PlayerCommandOrigin.h>
#include <mc/server/commands/CurrentCmdVersion.h>
#include <mc/network/packet/CommandRequestPacket.h>
#include <mc/network/packet/CommandRequestPacketPayload.h>
#include <mc/network/Packet.h>
#include <mc/world/actor/ActorHurtResult.h>
#include <mc/world/actor/ActorCategory.h>
#include <mc/world/gamemode/GameMode.h>
#include <mc/world/gamemode/InteractionResult.h>
#include <mc/world/actor/Mob.h>
#include <mc/world/actor/ActorEvent.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/HandSlot.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/world/phys/HitResult.h>
#include <mc/world/actor/player/SerializedSkinRef.h>
#include <mc/world/actor/player/SerializedSkinImpl.h>
#include <mc/world/actor/player/SkinImage.h>
#include <mc/deps/core/image/Image.h>
#include <mc/platform/UUID.h>
#include "state/MapRenderState.h"
#include "state/MapCacheManager.h"
#include "state/LanguageManager.h"
#include "state/SeedMapManager.h"
#include "mod/ChiyanMap.h"
#include <mc/world/level/LevelSeed64.h>

struct CapturedLevelSeed final {
    ChiyanMap::WorldGen::SeedSnapshot seed = ChiyanMap::WorldGen::SeedSnapshot::Unavailable();
    std::optional<std::uint64_t> rawBits;
};

inline CapturedLevelSeed CaptureLevelSeedSnapshot(Level const& level) noexcept {
    try {
        const LevelSeed64 seed = level.getLevelSeed64();
        const auto seedBits = static_cast<std::uint64_t>(seed.mValue);
        if (ChiyanMap::WorldGen::IsUsableCapturedBedrockSeed(seedBits)) {
            return {ChiyanMap::WorldGen::SeedSnapshot::FromSeedBits(seedBits), seedBits};
        }
    } catch (...) {}

    try {
        const auto seed32 = const_cast<Level&>(level).getSeed();
        const auto seedBits = static_cast<std::uint64_t>(seed32);
        if (ChiyanMap::WorldGen::IsUsableCapturedBedrockSeed(seedBits)) {
            return {ChiyanMap::WorldGen::SeedSnapshot::FromSeedBits(seedBits), seedBits};
        }
    } catch (...) {}

    return {};
}

inline uint64_t HashPlayerSkinHead(const uint8_t* pixels, size_t size) {
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= pixels[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

inline void InvalidatePlayerSkinHead(const std::string& uuid) {
    if (uuid.empty()) return;
    std::lock_guard<std::mutex> lock(g_playerSkinMutex);
    auto it = g_playerSkinHeads.find(uuid);
    if (it != g_playerSkinHeads.end() && it->second.valid) {
        it->second.valid = false;
        ++it->second.revision;
    }
}

inline bool IsRadarPlayerLike(Actor const& actor) {
    const std::string typeName = actor.getTypeName();
    return actor.isPlayer()
        || actor.hasCategory(ActorCategory::Player)
        || typeName == "player"
        || typeName == "minecraft:player";
}

inline bool IsLocalRadarActor(ClientInstance const& clientInstance, LocalPlayer const& localPlayer, Actor const& actor) {
    if (&actor == static_cast<Actor const*>(&localPlayer)) return true;

    if (actor.hasRuntimeID() && localPlayer.hasRuntimeID()
        && actor.getRuntimeID() == localPlayer.getRuntimeID()) {
        return true;
    }

    if (!IsRadarPlayerLike(actor)) return false;

    const auto& actorUniqueId = actor.getOrCreateUniqueID();
    if (actorUniqueId == localPlayer.getOrCreateUniqueID()
        || clientInstance.isLocalSplitscreenWith(actorUniqueId)) {
        return true;
    }

    if (actor.isPlayer()) {
        auto const& candidatePlayer = static_cast<Player const&>(actor);
        if (candidatePlayer.getUuid() == localPlayer.getUuid()) return true;

        const auto localXuid = localPlayer.getXuid();
        const auto candidateXuid = candidatePlayer.getXuid();
        if (!localXuid.empty() && candidateXuid == localXuid) return true;
    }

    // Some servers mirror the local client as a separate network player entity.
    // That proxy has different actor IDs and UUID, but retains the player's name tag.
    auto const& localNameTag = localPlayer.getNameTag();
    return !localNameTag.empty() && actor.getNameTag() == localNameTag;
}

// 提取并合成玩家皮肤的 8x8 头部正面与外层（帽子/头盔层）
inline void ExtractPlayerSkinHead(class Player* player, const std::string& uuid) {
    if (!player || uuid.empty()) return;
    try {
        if (!player->mSkin) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        auto& skinRef = *player->mSkin;
        if (!skinRef.mSkinImpl) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        auto& threadOwner = *skinRef.mSkinImpl;
        auto& skinImpl = threadOwner.mObject;
        if (skinImpl.mIsPersona) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        auto& skinImage = skinImpl.mSkinImage.get();
        auto& img = static_cast<mce::Image&>(skinImage);
        if (img.imageFormat != mce::ImageFormat::RGBA8Unorm || img.mWidth < 8 || img.mHeight < 16 || img.mWidth % 8 != 0) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }
        const uint8_t* pixels = img.mImageBytes.data();
        if (!pixels) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }

        const int unit = static_cast<int>(img.mWidth) / 8;
        const int baseHeadX = unit;
        const int baseHeadY = unit;
        const int outerHeadX = unit * 5;
        if (unit < 8 || baseHeadY + unit > static_cast<int>(img.mHeight) || outerHeadX + unit > static_cast<int>(img.mWidth)) {
            InvalidatePlayerSkinHead(uuid);
            return;
        }

        PlayerSkinHead head;
        for (int y = 0; y < 8; ++y) {
            const int sourceY0 = baseHeadY + (y * unit) / 8;
            const int sourceY1 = baseHeadY + ((y + 1) * unit) / 8;
            for (int x = 0; x < 8; ++x) {
                const int sourceX0 = baseHeadX + (x * unit) / 8;
                const int sourceX1 = baseHeadX + ((x + 1) * unit) / 8;
                uint32_t totals[4]{};
                uint32_t sampleCount = 0;
                for (int sourceY = sourceY0; sourceY < sourceY1; ++sourceY) {
                    for (int sourceX = sourceX0; sourceX < sourceX1; ++sourceX) {
                        const uint8_t* base = pixels + (sourceY * static_cast<int>(img.mWidth) + sourceX) * 4;
                        const uint8_t* outer = pixels + (sourceY * static_cast<int>(img.mWidth) + outerHeadX + (sourceX - baseHeadX)) * 4;
                        const uint32_t outerAlpha = outer[3];
                        const uint32_t inverseOuterAlpha = 255 - outerAlpha;
                        const uint32_t baseAlpha = base[3];
                        const uint32_t outputAlpha = outerAlpha + (baseAlpha * inverseOuterAlpha + 127) / 255;
                        for (int channel = 0; channel < 3; ++channel) {
                            const uint32_t premultiplied = outer[channel] * outerAlpha
                                + (base[channel] * baseAlpha * inverseOuterAlpha + 127) / 255;
                            totals[channel] += outputAlpha == 0
                                ? 0
                                : (premultiplied + outputAlpha / 2) / outputAlpha;
                        }
                        totals[3] += outputAlpha;
                        ++sampleCount;
                    }
                }
                const int destination = (y * 8 + x) * 4;
                for (int channel = 0; channel < 4; ++channel) {
                    head.pixels[destination + channel] = static_cast<uint8_t>((totals[channel] + sampleCount / 2) / sampleCount);
                }
            }
        }
        head.fingerprint = HashPlayerSkinHead(head.pixels, sizeof(head.pixels));
        head.valid = true;
        std::lock_guard<std::mutex> lock(g_playerSkinMutex);
        auto& cachedHead = g_playerSkinHeads[uuid];
        if (cachedHead.valid && cachedHead.fingerprint == head.fingerprint
            && std::memcmp(cachedHead.pixels, head.pixels, sizeof(head.pixels)) == 0) {
            return;
        }
        head.revision = cachedHead.revision + 1;
        g_playerSkinHeads[uuid] = head;
    } catch (...) {}
}

extern float g_playerX;
extern float g_playerY;
extern float g_playerZ;
extern float g_playerYaw;
extern bool  g_hasPlayer;
extern LocalPlayer* g_localPlayer;
extern ClientInstance* g_clientInstance;
extern Vec3 g_prevPhysicsPos;
extern Vec3 g_currPhysicsPos;
extern std::chrono::steady_clock::time_point g_lastPhysicsTime;
extern int g_playerBlockX;
extern int g_playerBlockZ;

// 数据并发锁，消灭画面撕裂
inline std::mutex g_mapDataMutex;

// [性能] 持久化缓存写入线程：原实现每次扫描完成时 std::thread().detach() 创建新线程
// + new ColorGrid/HeightGrid (5MB 堆分配)。改为持久化 worker + 预分配缓冲 + 条件变量，
// 消除线程创建开销和 5MB 堆分配/释放开销。
using ColorGrid = mce::Color[MAP_DATA_SIZE][MAP_DATA_SIZE];
using HeightGrid = float[MAP_DATA_SIZE][MAP_DATA_SIZE];
inline std::mutex g_cacheWriteMutex;
inline std::condition_variable g_cacheWriteCV;
inline std::atomic<bool> g_cacheWritePending{false};
inline std::atomic<bool> g_cacheWriteExit{false};
inline std::thread* g_cacheWriteThread = nullptr;
// 预分配缓冲 (仅 worker 线程访问，无需加锁)
inline mce::Color (*g_cacheWriteColors)[MAP_DATA_SIZE] = nullptr;
inline float (*g_cacheWriteHeights)[MAP_DATA_SIZE] = nullptr;
inline uint8_t (*g_cacheWriteLights)[MAP_DATA_SIZE] = nullptr;
// 请求参数 (由扫描线程写入，worker 线程读取，通过 g_cacheWritePending 标志同步)
inline int g_cacheWriteX = 0;
inline int g_cacheWriteZ = 0;
inline int g_cacheWriteDim = 0;
inline bool g_cacheWriteIsCave = false;
inline std::vector<MapCacheManager::BiomeEntry> g_cacheWriteBiomes;

inline int g_currentScanX = -99999;
inline int g_currentScanZ = -99999;
inline bool g_isScanning = false;
inline int g_scanPhase = 0; // 0 = 核心视野同心环 [0, SCAN_INNER_RADIUS], 1 = 外围全图同心环 [SCAN_INNER_RADIUS+1, MAP_DATA_RADIUS]
inline int g_scanRadius = 0;
inline int g_scanPerimeterIndex = 0;
inline int g_ticksSinceScan = 0;
inline std::vector<MapCacheManager::BiomeEntry> g_scanBiomeEntries;
inline int g_caveDetectCooldown = 0;
inline int g_scanPrevDimension = 0;
inline bool g_scanPrevCaveActive = false;
inline int g_scanDimension = -999;

inline void AbortActiveScanAndPendingWrites() {
    g_cacheWritePending.store(false);
    g_cacheWriteBiomes.clear();
    g_cacheWriteCV.notify_one();

    g_isScanning = false;
    g_scanPhase = 0;
    g_scanRadius = 0;
    g_scanPerimeterIndex = 0;
    g_ticksSinceScan = 101;
    g_scanBiomeEntries.clear();
    // [防黑块核心] 严禁盲目置零后台缓冲！置零会导致跑图平移或模式重试期间出现数秒黑色空洞。
}

inline void CacheWriteWorkerFunc() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(g_cacheWriteMutex);
            g_cacheWriteCV.wait(lock, [] { return g_cacheWritePending.load() || g_cacheWriteExit.load(); });
            if (g_cacheWriteExit.load()) return;
        }

        if (MapRenderState::g_isShuttingDown.load()) {
            g_cacheWritePending.store(false);
            g_cacheWriteBiomes.clear();
            continue;
        }
        // 严格核验：待写维度必须与当前物理维度以及缓存加载维度完全匹配
        if (MapRenderState::currentDimensionId != g_cacheWriteDim ||
            MapCacheManager::GetLoadedDimensionId() != g_cacheWriteDim) {
            g_cacheWritePending.store(false);
            g_cacheWriteBiomes.clear();
            continue;
        }

        // [修复] 包裹 try-catch 防止 UpdateFromScan/UpdateBiomesFromScan 抛异常
        // 导致 std::terminate → 0xC0000409 FAST_FAIL_FATAL_APP_EXIT
        try {
            MapCacheManager::UpdateFromScan(g_cacheWriteDim, g_cacheWriteX, g_cacheWriteZ, g_cacheWriteColors, g_cacheWriteHeights, g_cacheWriteIsCave, g_cacheWriteLights);
            MapCacheManager::UpdateBiomesFromScan(g_cacheWriteDim, g_cacheWriteBiomes);
        } catch (...) {
            // 异常时仍需复位状态，避免 pending 永远卡住
        }
        g_cacheWriteBiomes.clear();
        g_cacheWritePending.store(false);
    }
}

// 提交缓存写入请求 (非阻塞，若 worker 仍在处理上一次请求则跳过)
inline void SubmitCacheWrite(int x, int z, int dim, bool isCave, std::vector<MapCacheManager::BiomeEntry>& biomes) {
    if (g_cacheWritePending.load()) return;  // worker 忙，跳过 (数据已在前台缓冲，下次扫描会再写)
    if (MapRenderState::currentDimensionId != dim) return; // 提交时校验物理维度
    if (MapCacheManager::GetLoadedDimensionId() != dim) return; // 提交时校验加载维度

    // 懒启动持久化 worker 线程
    if (!g_cacheWriteThread) {
        g_cacheWriteColors = new mce::Color[MAP_DATA_SIZE][MAP_DATA_SIZE];
        g_cacheWriteHeights = new float[MAP_DATA_SIZE][MAP_DATA_SIZE];
        g_cacheWriteLights = new uint8_t[MAP_DATA_SIZE][MAP_DATA_SIZE];
        g_cacheWriteThread = new std::thread(CacheWriteWorkerFunc);
    }

    {
        std::lock_guard<std::mutex> lock(g_mapDataMutex);
        std::memcpy(g_cacheWriteColors, g_mapColorsBack, sizeof(g_mapColorsBack));
        std::memcpy(g_cacheWriteHeights, g_mapHeightsBack, sizeof(g_mapHeightsBack));
        std::memcpy(g_cacheWriteLights, g_mapLightsBack, sizeof(g_mapLightsBack));
    }
    g_cacheWriteX = x;
    g_cacheWriteZ = z;
    g_cacheWriteDim = dim;
    g_cacheWriteIsCave = isCave;
    g_cacheWriteBiomes.swap(biomes);
    g_cacheWritePending.store(true);
    g_cacheWriteCV.notify_one();
}

inline void ShutdownCacheWriteThread() {
    if (g_cacheWriteThread) {
        g_cacheWriteExit.store(true);
        g_cacheWriteCV.notify_one();
        if (g_cacheWriteThread->joinable()) g_cacheWriteThread->join();
        delete g_cacheWriteThread;
        g_cacheWriteThread = nullptr;
        g_cacheWriteExit.store(false);
        g_cacheWritePending.store(false);
    }
    if (g_cacheWriteColors) { delete[] g_cacheWriteColors; g_cacheWriteColors = nullptr; }
    if (g_cacheWriteHeights) { delete[] g_cacheWriteHeights; g_cacheWriteHeights = nullptr; }
    if (g_cacheWriteLights) { delete[] g_cacheWriteLights; g_cacheWriteLights = nullptr; }
}

// ==========================================
// [跑图跟随] 扫描窗口平移工具
// 根因: 513×513 整幅扫描受每 tick 500μs 预算限制, 完整扫完需数十秒,
//   玩家持续移动时已完成扫描的中心大幅滞后 (跑图时可滞后 150~300 格),
//   小地图 UV 采样窗口越出纹理边界, POINT+CLAMP 寻址把纹理边缘行/列
//   拉伸成"东西向移动→横条纹、南北向移动→竖条纹"的整屏伪影。
// 方案: 扫描进行中玩家偏离扫描中心 ≥16 格时, 将后台缓冲整体平移(保留已扫数据),
//   仅新暴露的边缘条带置零等待后续扫描, 游标同步换算到新中心坐标系,
//   扫描无缝衔接, 扫描中心始终贴近玩家。
//   注: 平移后尾侧暴露条带 (约16格) 位于玩家行进反方向 240+ 格外,
//   超出小地图任何视野范围 (zoom 上限 200); 且 UpdateFromScan 跳过
//   alpha=0 列, 不会用缺口覆盖缓存中已有的已访问区域数据。
// ==========================================
template <typename T>
inline void ShiftScanGrid(T (&grid)[MAP_DATA_SIZE][MAP_DATA_SIZE], int shiftX, int shiftZ, int fillByte = 0) noexcept {
    constexpr int N = MAP_DATA_SIZE;
    // X 方向: 整行平移 (grid 按 [arrX][arrZ] 排布, arrX 是行索引)
    // 玩家向东移动 (shiftX>0): 新[arrX'] = 旧[arrX'+shiftX], 西侧数据移出, 东侧条带置零
    if (shiftX != 0) {
        if (shiftX > 0) {
            std::memmove(grid[0], grid[shiftX], (size_t)(N - shiftX) * sizeof(grid[0]));
            std::memset(grid[N - shiftX], fillByte, (size_t)shiftX * sizeof(grid[0]));
        } else {
            int s = -shiftX;
            std::memmove(grid[s], grid[0], (size_t)(N - s) * sizeof(grid[0]));
            std::memset(grid[0], fillByte, (size_t)s * sizeof(grid[0]));
        }
    }
    // Z 方向: 每行内部平移
    if (shiftZ != 0) {
        for (int x = 0; x < N; x++) {
            if (shiftZ > 0) {
                std::memmove(grid[x], grid[x] + shiftZ, (size_t)(N - shiftZ) * sizeof(grid[x][0]));
                std::memset(grid[x] + (N - shiftZ), fillByte, (size_t)shiftZ * sizeof(grid[x][0]));
            } else {
                int s = -shiftZ;
                std::memmove(grid[x] + s, grid[x], (size_t)(N - s) * sizeof(grid[x][0]));
                std::memset(grid[x], fillByte, (size_t)s * sizeof(grid[x][0]));
            }
        }
    }
}

// ==========================================
// [跑图跟随] GPU 纹理数据平移工具 (与 ShiftScanGrid 严格保持几何同步)
// ==========================================
inline void ShiftTextureData(uint8_t* tex, int shiftX, int shiftZ) noexcept {
    if (!tex || (shiftX == 0 && shiftZ == 0)) return;
    constexpr int N = MAP_DATA_SIZE;
    // g_textureData 排布: index = (z * N + x) * 4
    if (shiftZ != 0) {
        if (shiftZ > 0) {
            std::memmove(tex, tex + (size_t)shiftZ * N * 4, (size_t)(N - shiftZ) * N * 4);
            std::memset(tex + (size_t)(N - shiftZ) * N * 4, 0, (size_t)shiftZ * N * 4);
        } else {
            int s = -shiftZ;
            std::memmove(tex + (size_t)s * N * 4, tex, (size_t)(N - s) * N * 4);
            std::memset(tex, 0, (size_t)s * N * 4);
        }
    }
    if (shiftX != 0) {
        for (int z = 0; z < N; z++) {
            uint8_t* row = tex + (size_t)z * N * 4;
            if (shiftX > 0) {
                std::memmove(row, row + (size_t)shiftX * 4, (size_t)(N - shiftX) * 4);
                std::memset(row + (size_t)(N - shiftX) * 4, 0, (size_t)shiftX * 4);
            } else {
                int s = -shiftX;
                std::memmove(row + (size_t)s * 4, row, (size_t)(N - s) * 4);
                std::memset(row, 0, (size_t)s * 4);
            }
        }
    }
}

// ==========================================
// [同心方环扫描] 由中心向外辐射遍历方环周界坐标 (dx, dz)
// 半径 r=0 仅包含 1 个点 (0,0)；半径 r>0 包含 8r 个周界点
// ==========================================
inline void GetRingCoords(int r, int i, int& dx, int& dz) noexcept {
    if (r == 0) {
        dx = 0; dz = 0;
        return;
    }
    int K = 2 * r;
    int seg = i / K;
    int off = i % K;
    switch (seg) {
        case 0: dx = -r + off; dz = -r; break; // 北边缘：向东移动
        case 1: dx = r; dz = -r + off; break; // 东边缘：向南移动
        case 2: dx = r - off; dz = r; break;  // 南边缘：向西移动
        default: dx = -r; dz = r - off; break; // 西边缘：向北移动
    }
}

// ==========================================
// 生物群系中文翻译字典引擎
// ==========================================
    // [性能] FNV-1a 64 位字符串哈希，替代 std::hash<string>。扫描路径内每格调用 2 次，
// 实测比 MSVC std::hash<string> 快 2.5~3 倍，避免大量分配/析构临时 string 的开销。
inline uint64_t Fnv1aHash(const char* s, size_t len) noexcept {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint8_t)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}
inline uint64_t Fnv1aHash(const std::string& s) noexcept {
    return Fnv1aHash(s.data(), s.size());
}

inline std::string TranslateBiomeName(const std::string& rawName) {
    std::string cleanName = rawName;
    size_t colonPos = cleanName.find(":");
    if (colonPos != std::string::npos) cleanName = cleanName.substr(colonPos + 1);

    std::string lower = cleanName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;

    // [性能] 原实现每帧对 kBiomeRules 做 O(N) 线性 find 扫描（N=62），
    // 玩家 tick 调用一次，大地图悬停查询再调用一次。改用 (hash -> key) 直接查找，
    // 由于生物群系字符串是固定全集（62 条别名），无哈希冲突，复杂度 O(1)。
    // 为保持"别名优先于通用名"的正确匹配，原始具体别名（如 dark_forest、windswept_savanna）
    // 在初始化时全部登记到哈希表；通用 fallback（windswept / mushroom）仍然走线性扫描兜底。
    struct BiomeHashTable {
        std::unordered_map<uint64_t, const char*> map;
        BiomeHashTable() {
            static const std::pair<const char*, const char*> entries[] = {
                {"end_highlands", "BIOME_THE_END"},
                {"end_midlands", "BIOME_THE_END"},
                {"end_barrens", "BIOME_THE_END"},
                {"small_end_islands", "BIOME_THE_END"},
                {"the_end", "BIOME_THE_END"},
                {"crimson_forest", "BIOME_CRIMSON_FOREST"},
                {"warped_forest", "BIOME_WARPED_FOREST"},
                {"soulsand_valley", "BIOME_SOUL_SAND_VALLEY"},
                {"soul_sand_valley", "BIOME_SOUL_SAND_VALLEY"},
                {"basalt_deltas", "BIOME_BASALT_DELTAS"},
                {"nether_wastes", "BIOME_HELL"},
                {"hell", "BIOME_HELL"},
                {"deep_frozen_ocean", "BIOME_DEEP_FROZEN_OCEAN"},
                {"deep_cold_ocean", "BIOME_DEEP_COLD_OCEAN"},
                {"deep_lukewarm_ocean", "BIOME_DEEP_LUKEWARM_OCEAN"},
                {"deep_warm_ocean", "BIOME_DEEP_WARM_OCEAN"},
                {"deep_ocean", "BIOME_DEEP_OCEAN"},
                {"legacy_frozen_ocean", "BIOME_FROZEN_OCEAN"},
                {"frozen_ocean", "BIOME_FROZEN_OCEAN"},
                {"warm_ocean", "BIOME_WARM_OCEAN"},
                {"cold_ocean", "BIOME_COLD_OCEAN"},
                {"lukewarm_ocean", "BIOME_LUKEWARM_OCEAN"},
                {"ocean", "BIOME_OCEAN"},
                {"frozen_river", "BIOME_FROZEN_RIVER"},
                {"river", "BIOME_RIVER"},
                {"stone_beach", "BIOME_STONY_SHORE"},
                {"stony_shore", "BIOME_STONY_SHORE"},
                {"cold_beach", "BIOME_SNOWY_BEACH"},
                {"snowy_beach", "BIOME_SNOWY_BEACH"},
                {"beach", "BIOME_BEACH"},
                {"ice_plains_spikes", "BIOME_ICE_SPIKES"},
                {"ice_spikes", "BIOME_ICE_SPIKES"},
                {"ice_mountains", "BIOME_SNOWY_SLOPES"},
                {"snowy_slopes", "BIOME_SNOWY_SLOPES"},
                {"ice_plains", "BIOME_SNOWY_PLAINS"},
                {"snowy_plains", "BIOME_SNOWY_PLAINS"},
                {"snowy_tundra", "BIOME_SNOWY_PLAINS"},
                {"jagged_peaks", "BIOME_JAGGED_PEAKS"},
                {"frozen_peaks", "BIOME_FROZEN_PEAKS"},
                {"stony_peaks", "BIOME_STONY_PEAKS"},
                {"grove", "BIOME_GROVE"},
                {"extreme_hills_plus_trees", "BIOME_WINDSWEPT_FOREST"},
                {"windswept_forest", "BIOME_WINDSWEPT_FOREST"},
                {"extreme_hills_mutated", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
                {"windswept_gravelly_hills", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
                {"windswept_hills", "BIOME_EXTREME_HILLS"},
                {"windswept_savanna", "BIOME_WINDSWEPT_SAVANNA"},
                {"extreme_hills_edge", "BIOME_EXTREME_HILLS"},
                {"extreme_hills", "BIOME_EXTREME_HILLS"},
                {"windswept", "BIOME_EXTREME_HILLS"},
                {"mesa_bryce", "BIOME_ERODED_BADLANDS"},
                {"eroded_badlands", "BIOME_ERODED_BADLANDS"},
                {"mesa_plateau_stone", "BIOME_WOODED_BADLANDS"},
                {"wooded_badlands", "BIOME_WOODED_BADLANDS"},
                {"mesa_plateau", "BIOME_MESA"},
                {"mesa", "BIOME_MESA"},
                {"badlands", "BIOME_MESA"},
                {"cherry_grove", "BIOME_CHERRY"},
                {"cherry", "BIOME_CHERRY"},
                {"meadow", "BIOME_MEADOW"},
                {"savanna_mutated", "BIOME_WINDSWEPT_SAVANNA"},
                {"savanna_plateau", "BIOME_SAVANNA_PLATEAU"},
                {"savanna", "BIOME_SAVANNA"},
                {"bamboo_jungle", "BIOME_BAMBOO_JUNGLE"},
                {"jungle_edge", "BIOME_SPARSE_JUNGLE"},
                {"sparse_jungle", "BIOME_SPARSE_JUNGLE"},
                {"jungle", "BIOME_JUNGLE"},
                {"mangrove_swamp", "BIOME_MANGROVE_SWAMP"},
                {"swampland", "BIOME_SWAMP"},
                {"swamp", "BIOME_SWAMP"},
                {"mushroom_island", "BIOME_MUSHROOM"},
                {"mushroom_fields", "BIOME_MUSHROOM"},
                {"mushroom", "BIOME_MUSHROOM"},
                {"pale_garden", "BIOME_PALE_GARDEN"},
                {"lush_caves", "BIOME_LUSH_CAVES"},
                {"dripstone_caves", "BIOME_DRIPSTONE_CAVES"},
                {"sulfur_caves", "BIOME_SULFUR_CAVES"},
                {"dappled_forest", "SEED_MAP_LAYER_DAPPLED_FOREST"},
                {"cold_taiga", "BIOME_SNOWY_TAIGA"},
                {"snowy_taiga", "BIOME_SNOWY_TAIGA"},
                {"mega_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
                {"old_growth_pine_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
                {"redwood_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
                {"old_growth_spruce_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
                {"taiga", "BIOME_TAIGA"},
                {"dark_oak_forest", "BIOME_DARK_FOREST"},
                {"roofed_forest", "BIOME_DARK_FOREST"},
                {"dark_forest", "BIOME_DARK_FOREST"},
                {"birch_forest_mutated", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
                {"old_growth_birch_forest", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
                {"birch_forest_hills", "BIOME_BIRCH_FOREST"},
                {"birch_forest", "BIOME_BIRCH_FOREST"},
                {"flower_forest", "BIOME_FLOWER_FOREST"},
                {"forest", "BIOME_FOREST"},
                {"sunflower_plains", "BIOME_SUNFLOWER_PLAINS"},
                {"plains", "BIOME_PLAINS"},
                {"deep_dark", "BIOME_DEEP_DARK"},
                {"desert", "BIOME_DESERT"},
            };
            for (auto const& p : entries) {
                size_t l = std::strlen(p.first);
                map.emplace(Fnv1aHash(p.first, l), p.second);
            }
        }
    };
    static const BiomeHashTable s_table;

    std::string biomeKey = "BIOME_UNKNOWN";
    uint64_t lowerHash = Fnv1aHash(lower);
    auto it = s_table.map.find(lowerHash);
    if (it != s_table.map.end()) {
        biomeKey = it->second;
    } else {
        // 哈希未命中：降级为线性 find 子串扫描（处理某些不常见的命名组合）
        static const std::vector<std::pair<std::string, std::string>> kBiomeRules = {
            {"end_highlands", "BIOME_THE_END"}, {"end_midlands", "BIOME_THE_END"},
            {"end_barrens", "BIOME_THE_END"}, {"small_end_islands", "BIOME_THE_END"},
            {"the_end", "BIOME_THE_END"}, {"crimson_forest", "BIOME_CRIMSON_FOREST"},
            {"warped_forest", "BIOME_WARPED_FOREST"}, {"soulsand_valley", "BIOME_SOUL_SAND_VALLEY"},
            {"soul_sand_valley", "BIOME_SOUL_SAND_VALLEY"}, {"basalt_deltas", "BIOME_BASALT_DELTAS"},
            {"nether_wastes", "BIOME_HELL"}, {"hell", "BIOME_HELL"},
            {"deep_frozen_ocean", "BIOME_DEEP_FROZEN_OCEAN"}, {"deep_cold_ocean", "BIOME_DEEP_COLD_OCEAN"},
            {"deep_lukewarm_ocean", "BIOME_DEEP_LUKEWARM_OCEAN"}, {"deep_warm_ocean", "BIOME_DEEP_WARM_OCEAN"},
            {"deep_ocean", "BIOME_DEEP_OCEAN"}, {"legacy_frozen_ocean", "BIOME_FROZEN_OCEAN"},
            {"frozen_ocean", "BIOME_FROZEN_OCEAN"}, {"warm_ocean", "BIOME_WARM_OCEAN"},
            {"cold_ocean", "BIOME_COLD_OCEAN"}, {"lukewarm_ocean", "BIOME_LUKEWARM_OCEAN"},
            {"ocean", "BIOME_OCEAN"}, {"frozen_river", "BIOME_FROZEN_RIVER"},
            {"river", "BIOME_RIVER"}, {"stone_beach", "BIOME_STONY_SHORE"},
            {"stony_shore", "BIOME_STONY_SHORE"}, {"cold_beach", "BIOME_SNOWY_BEACH"},
            {"snowy_beach", "BIOME_SNOWY_BEACH"}, {"beach", "BIOME_BEACH"},
            {"ice_plains_spikes", "BIOME_ICE_SPIKES"}, {"ice_spikes", "BIOME_ICE_SPIKES"},
            {"ice_mountains", "BIOME_SNOWY_SLOPES"}, {"snowy_slopes", "BIOME_SNOWY_SLOPES"},
            {"ice_plains", "BIOME_SNOWY_PLAINS"}, {"snowy_plains", "BIOME_SNOWY_PLAINS"},
            {"snowy_tundra", "BIOME_SNOWY_PLAINS"}, {"jagged_peaks", "BIOME_JAGGED_PEAKS"},
            {"frozen_peaks", "BIOME_FROZEN_PEAKS"}, {"stony_peaks", "BIOME_STONY_PEAKS"},
            {"grove", "BIOME_GROVE"}, {"extreme_hills_plus_trees", "BIOME_WINDSWEPT_FOREST"},
            {"windswept_forest", "BIOME_WINDSWEPT_FOREST"}, {"extreme_hills_mutated", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
            {"windswept_gravelly_hills", "BIOME_WINDSWEPT_GRAVELLY_HILLS"}, {"windswept_hills", "BIOME_EXTREME_HILLS"},
            {"windswept_savanna", "BIOME_WINDSWEPT_SAVANNA"}, {"extreme_hills_edge", "BIOME_EXTREME_HILLS"},
            {"extreme_hills", "BIOME_EXTREME_HILLS"}, {"windswept", "BIOME_EXTREME_HILLS"},
            {"mesa_bryce", "BIOME_ERODED_BADLANDS"}, {"eroded_badlands", "BIOME_ERODED_BADLANDS"},
            {"mesa_plateau_stone", "BIOME_WOODED_BADLANDS"}, {"wooded_badlands", "BIOME_WOODED_BADLANDS"},
            {"mesa_plateau", "BIOME_MESA"}, {"mesa", "BIOME_MESA"},
            {"badlands", "BIOME_MESA"}, {"cherry_grove", "BIOME_CHERRY"},
            {"cherry", "BIOME_CHERRY"}, {"meadow", "BIOME_MEADOW"},
            {"savanna_mutated", "BIOME_WINDSWEPT_SAVANNA"}, {"savanna_plateau", "BIOME_SAVANNA_PLATEAU"},
            {"savanna", "BIOME_SAVANNA"}, {"bamboo_jungle", "BIOME_BAMBOO_JUNGLE"},
            {"jungle_edge", "BIOME_SPARSE_JUNGLE"}, {"sparse_jungle", "BIOME_SPARSE_JUNGLE"},
            {"jungle", "BIOME_JUNGLE"}, {"mangrove_swamp", "BIOME_MANGROVE_SWAMP"},
            {"swampland", "BIOME_SWAMP"}, {"swamp", "BIOME_SWAMP"},
            {"mushroom_island", "BIOME_MUSHROOM"}, {"mushroom_fields", "BIOME_MUSHROOM"},
            {"mushroom", "BIOME_MUSHROOM"}, {"pale_garden", "BIOME_PALE_GARDEN"},
            {"lush_caves", "BIOME_LUSH_CAVES"}, {"dripstone_caves", "BIOME_DRIPSTONE_CAVES"},
            {"sulfur_caves", "BIOME_SULFUR_CAVES"}, {"sulfur", "BIOME_SULFUR_CAVES"},
            {"dappled_forest", "SEED_MAP_LAYER_DAPPLED_FOREST"},
            {"cold_taiga", "BIOME_SNOWY_TAIGA"}, {"snowy_taiga", "BIOME_SNOWY_TAIGA"},
            {"mega_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"}, {"old_growth_pine_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
            {"redwood_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"}, {"old_growth_spruce_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
            {"taiga", "BIOME_TAIGA"}, {"dark_oak_forest", "BIOME_DARK_FOREST"},
            {"roofed_forest", "BIOME_DARK_FOREST"}, {"dark_forest", "BIOME_DARK_FOREST"},
            {"birch_forest_mutated", "BIOME_OLD_GROWTH_BIRCH_FOREST"}, {"old_growth_birch_forest", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
            {"birch_forest_hills", "BIOME_BIRCH_FOREST"}, {"birch_forest", "BIOME_BIRCH_FOREST"},
            {"flower_forest", "BIOME_FLOWER_FOREST"}, {"forest", "BIOME_FOREST"},
            {"sunflower_plains", "BIOME_SUNFLOWER_PLAINS"}, {"plains", "BIOME_PLAINS"},
            {"deep_dark", "BIOME_DEEP_DARK"}, {"desert", "BIOME_DESERT"},
        };
        for (const auto& rule : kBiomeRules) {
            if (lower.find(rule.first) != std::string::npos) {
                biomeKey = rule.second;
                break;
            }
        }
    }

        std::string result = LanguageManager::GetText(biomeKey);
        
        // 若找不到对应语言，回退将英文名首字母大写返回
        if (result == biomeKey) {
            std::string formattedName = cleanName;
            for (size_t i = 0; i < formattedName.length(); ++i) {
                if (formattedName[i] == '_') formattedName[i] = ' ';
                if (i == 0 || formattedName[i - 1] == ' ') formattedName[i] = (char)std::toupper(formattedName[i]);
            }
            return formattedName;
        }
        return result;
    }

// ==========================================
// 真彩自然光色彩引擎
// ==========================================
struct BiomeTintTriple {
        mce::Color grass;
        mce::Color foliage;
        mce::Color water;
        bool isSwampDualTone = false;
    };

// [步骤1排查打点] 记录未命中规则直接落入默认/哈希的群系和方块（每种仅记录一次）
inline void LogColorMiss(std::string const& tag, std::string const& name) {
    static std::unordered_set<std::string> s_loggedMisses;
    static std::mutex s_missLogMutex;
    std::lock_guard<std::mutex> lock(s_missLogMutex);
    std::string key = tag + ":" + name;
    if (s_loggedMisses.size() < 1000 && s_loggedMisses.insert(key).second) {
        try {
            auto dataDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir();
            std::ofstream log((dataDir / "color_miss_log.txt").string(), std::ios::app);
            if (log) {
                log << "[" << tag << "] " << name << std::endl;
            }
        } catch (...) {}
    }
}

inline void getBiomeTints(std::string const& biomeName, mce::Color& grass, mce::Color& foliage, mce::Color& water) {
    // [性能] 生物群系染色查找缓存：直接 key 原始 biomeName（含 namespace），
    // 避免每次查找 2~3 次 heap alloc（lower 转换 + 线性 find 子串）。
    // 典型世界内生物群系数目 <= 60，缓存几乎 100% 命中。
    static std::unordered_map<uint64_t, BiomeTintTriple> s_tintCache;
    if (!biomeName.empty()) {
        uint64_t h = Fnv1aHash(biomeName);
        auto it = s_tintCache.find(h);
        if (it != s_tintCache.end()) {
            grass   = it->second.grass;
            foliage = it->second.foliage;
            water   = it->second.water;
            return;
        }
    }

    // 默认基础色调（温带平原/森林）：草地匹配实际颜色 RGB(98,132,60)——基于截图采样，明亮翠绿
    grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
    foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    water   = mce::Color(0.18f, 0.38f, 0.85f, 1.0f);

    if (biomeName.empty()) return;

    std::string lower = biomeName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;

    // [性能] 包装剩余查找，结果写回缓存
    auto computeTints = [&]() {

    // === 平原：草地匹配实际颜色 RGB(98,132,60)——基于截图采样，明亮翠绿 ===
    if (lower.find("plains") != std::string::npos && lower.find("snow") == std::string::npos && lower.find("ice") == std::string::npos) {
        grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    }
    else if (lower.find("cherry") != std::string::npos) {
        grass   = mce::Color(0.44f, 0.56f, 0.26f, 1.0f);
        foliage = mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
    }
    else if (lower.find("eroded_badlands") != std::string::npos) {
        // eroded_badlands: grass tint -> dark taiga-green (user request)
        grass   = mce::Color(0.26f, 0.38f, 0.30f, 1.0f);
        foliage = mce::Color(0.15f, 0.28f, 0.25f, 1.0f);
    }
    else if (lower.find("desert") != std::string::npos || lower.find("mesa") != std::string::npos || lower.find("badlands") != std::string::npos) {
        grass   = mce::Color(0.45f, 0.42f, 0.20f, 1.0f);
        foliage = mce::Color(0.35f, 0.38f, 0.18f, 1.0f);
        water   = mce::Color(0.15f, 0.45f, 0.45f, 1.0f);
    } 
    else if (lower.find("savanna") != std::string::npos) {
        grass   = mce::Color(0.42f, 0.42f, 0.18f, 1.0f);
        foliage = mce::Color(0.375f, 0.345f, 0.105f, 1.0f); // 热带草原/风袭热带草原/热带高原：温润金橄榄黄褐树叶（实机采样RGB(94,89,28)精准校准，彻底消除冷暗灰绿失真）
    } 
    // === 丛林与竹林：草地匹配实际颜色 RGB(53,124,37) 鲜活翡翠碧绿（红分量由 0.32 降至 0.21，消除偏黄偏暗失真） ===
    else if (lower.find("jungle") != std::string::npos || lower.find("bamboo") != std::string::npos) {
        grass   = mce::Color(0.210f, 0.488f, 0.144f, 1.0f);
        foliage = mce::Color(0.22f, 0.45f, 0.15f, 1.0f);
    } 
    else if (lower.find("mangrove") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.38f, 0.18f, 1.0f);
        foliage = mce::Color(0.32f, 0.40f, 0.09f, 1.0f); // 原版专属色表 mangrove_swamp_foliage (#8db127) 鲜润暖青绿
        water   = mce::Color(0.15f, 0.25f, 0.22f, 1.0f);
    }
    else if (lower.find("swamp") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.30f, 0.13f, 1.0f); // 沼泽暖调橄榄草地色（实机采样调校）
        foliage = mce::Color(0.24f, 0.26f, 0.11f, 1.0f); // 沼泽树叶深暗暖橄榄黄褐色（消除冷暗灰绿感）
        water   = mce::Color(0.15f, 0.25f, 0.22f, 1.0f);
    }
    // === 蘑菇岛系列：极鲜亮翠绿草地 ===
    else if (lower.find("mushroom") != std::string::npos) {
        grass   = mce::Color(0.33f, 0.79f, 0.25f, 1.0f);
        foliage = mce::Color(0.30f, 0.70f, 0.22f, 1.0f);
    }
    // === 海洋与河流细分：草地与植被色调，水色统一继承经典群青蓝 ===
    // [温水海洋 / 深温水海洋] 必须在 warm_ocean 之前（因含 "warm_ocean" 子串）
    else if (lower.find("lukewarm_ocean") != std::string::npos) {
        grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    }
    // [暖水海洋 (珊瑚海)]
    else if (lower.find("warm_ocean") != std::string::npos) {
        grass   = mce::Color(0.56f, 0.73f, 0.44f, 1.0f);
        foliage = mce::Color(0.40f, 0.65f, 0.30f, 1.0f);
    }
    else if (lower.find("cold_ocean") != std::string::npos) {
        grass   = mce::Color(0.35f, 0.48f, 0.33f, 1.0f);
        foliage = mce::Color(0.20f, 0.32f, 0.18f, 1.0f);
    }
    else if (lower.find("frozen_river") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
    }
    else if (lower.find("frozen_ocean") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
    }
    else if (lower.find("ocean") != std::string::npos || lower.find("river") != std::string::npos) {
        grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    }
    // === 下界生物群系（必须在通用 forest 规则之前，避免 crimson/warped forest 误判为普通森林） ===
    else if (lower.find("crimson_forest") != std::string::npos) {
        grass   = mce::Color(0.70f, 0.06f, 0.06f, 1.0f);
        foliage = mce::Color(0.75f, 0.08f, 0.08f, 1.0f);
    }
    else if (lower.find("warped_forest") != std::string::npos) {
        grass   = mce::Color(0.08f, 0.61f, 0.52f, 1.0f);
        foliage = mce::Color(0.08f, 0.61f, 0.52f, 1.0f);
    }
    else if (lower.find("soul_sand_valley") != std::string::npos || lower.find("soulsand_valley") != std::string::npos) {
        grass   = mce::Color(0.35f, 0.40f, 0.40f, 1.0f);
        foliage = mce::Color(0.30f, 0.38f, 0.38f, 1.0f);
    }
    else if (lower.find("basalt_deltas") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.26f, 0.28f, 1.0f);
        foliage = mce::Color(0.24f, 0.22f, 0.24f, 1.0f);
    }
    else if (lower.find("nether") != std::string::npos || lower.find("hell") != std::string::npos) {
        grass   = mce::Color(0.60f, 0.45f, 0.30f, 1.0f);
        foliage = mce::Color(0.60f, 0.45f, 0.30f, 1.0f);
    }
    // === 末地群系 ===
    else if (lower.find("end") != std::string::npos) {
        grass   = mce::Color(0.50f, 0.50f, 0.50f, 1.0f);
        foliage = mce::Color(0.50f, 0.50f, 0.50f, 1.0f);
    }
    // === 针叶林与寒带：草地与树叶匹配实际颜色（草地 RGB(86,116,84)，树叶 RGB(45,66,45)） ===
    else if (lower.find("taiga") != std::string::npos || lower.find("snow") != std::string::npos || lower.find("ice") != std::string::npos || lower.find("frozen") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
    } 
    // === 风袭丘陵/山地系列：草地匹配实际颜色 RGB(78,102,76) ===
    else if ((lower.find("extreme_hills") != std::string::npos || lower.find("windswept") != std::string::npos)
             && lower.find("savanna") == std::string::npos) {
        grass   = mce::Color(0.306f, 0.400f, 0.298f, 1.0f);
        foliage = mce::Color(0.215f, 0.337f, 0.215f, 1.0f);
    }
    // === 山峰系列（1.18+ 山地峰顶与山坡） ===
    else if (lower.find("stony_peaks") != std::string::npos) {
        grass   = mce::Color(0.55f, 0.72f, 0.36f, 1.0f); // 暖性石峰，邻接温热带
        foliage = mce::Color(0.38f, 0.62f, 0.28f, 1.0f);
    }
    else if (lower.find("peaks") != std::string::npos || lower.find("slopes") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f); // 寒冷雪峰与雪坡
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
    }
    // === 海岸与沙滩 ===
    else if (lower.find("stony_shore") != std::string::npos || lower.find("stone_beach") != std::string::npos) {
        grass   = mce::Color(0.39f, 0.48f, 0.35f, 1.0f);
        foliage = mce::Color(0.25f, 0.38f, 0.25f, 1.0f);
    }
    else if (lower.find("beach") != std::string::npos || lower.find("shore") != std::string::npos) {
        grass   = mce::Color(0.57f, 0.74f, 0.35f, 1.0f);
        foliage = mce::Color(0.40f, 0.65f, 0.30f, 1.0f);
    }
    // === 白桦森林与原始桦木森林：草地匹配实际颜色 RGB(83,114,63) ===
    else if (lower.find("birch") != std::string::npos) {
        grass   = mce::Color(0.325f, 0.447f, 0.247f, 1.0f);
        foliage = mce::Color(0.263f, 0.341f, 0.173f, 1.0f);
    }
    else if (lower.find("dark_forest") != std::string::npos || lower.find("roofed_forest") != std::string::npos || lower.find("dark_oak_forest") != std::string::npos) {
        grass   = mce::Color(0.20f, 0.30f, 0.12f, 1.0f);
        foliage = mce::Color(0.235f, 0.471f, 0.118f, 1.0f);
    }
    // === 森林与繁花森林：草地匹配实际颜色 RGB(75,118,55) ===
    else if (lower.find("forest") != std::string::npos) {
        grass   = mce::Color(0.294f, 0.463f, 0.216f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    }
    // === 樱花树林：必须在通用 grove 之前（cherry_grove 含 grove 需优先拦截） ===
    else if (lower.find("cherry") != std::string::npos) {
        grass   = mce::Color(0.56f, 0.81f, 0.29f, 1.0f); // 鲜亮春意浅翠绿
        foliage = mce::Color(0.90f, 0.65f, 0.75f, 1.0f); // 标志性樱花粉红
    }
    else if (lower.find("meadow") != std::string::npos || lower.find("grove") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.18f, 0.32f, 0.20f, 1.0f);
    }
    else if (lower.find("lush_caves") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.45f, 0.20f, 1.0f);
        foliage = mce::Color(0.20f, 0.40f, 0.15f, 1.0f);
        water   = mce::Color(0.15f, 0.35f, 0.55f, 1.0f);
    }
    else if (lower.find("deep_dark") != std::string::npos) {
        grass   = mce::Color(0.18f, 0.22f, 0.20f, 1.0f);
        foliage = mce::Color(0.15f, 0.18f, 0.16f, 1.0f);
        water   = mce::Color(0.10f, 0.15f, 0.20f, 1.0f);
    }
    else if (lower.find("dripstone_caves") != std::string::npos) {
        grass   = mce::Color(0.48f, 0.64f, 0.38f, 1.0f);
        foliage = mce::Color(0.38f, 0.57f, 0.27f, 1.0f);
    }
    // === 苍白之园：草地匹配实际颜色 RGB(86,97,79) ===
    else if (lower.find("pale_garden") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.380f, 0.310f, 1.0f);
        foliage = mce::Color(0.337f, 0.380f, 0.310f, 1.0f);
    }
    // === 硫黄洞穴：原版 client_biome 定义水色 #34BF89, 草色 #ABA64F ===
    else if (lower.find("sulfur_caves") != std::string::npos || lower.find("sulfur") != std::string::npos) {
        grass   = mce::Color(0.671f, 0.651f, 0.310f, 1.0f);
        foliage = mce::Color(0.671f, 0.651f, 0.310f, 1.0f);
    }
    // === 原版 26.2 生物群系补充 ===
    else if (lower.find("flower_forest") != std::string::npos) {
        grass   = mce::Color(0.475f, 0.753f, 0.353f, 1.0f);
        foliage = mce::Color(0.349f, 0.682f, 0.188f, 1.0f);
    }
    else if (lower.find("sunflower_plains") != std::string::npos) {
        grass   = mce::Color(0.569f, 0.741f, 0.349f, 1.0f);
        foliage = mce::Color(0.467f, 0.671f, 0.184f, 1.0f);
    }
    else if (lower.find("old_growth_birch") != std::string::npos) {
        grass   = mce::Color(0.533f, 0.733f, 0.404f, 1.0f);
        foliage = mce::Color(0.420f, 0.663f, 0.255f, 1.0f);
    }
    else if (lower.find("old_growth_pine") != std::string::npos) {
        grass   = mce::Color(0.525f, 0.722f, 0.498f, 1.0f);
        foliage = mce::Color(0.408f, 0.647f, 0.373f, 1.0f);
    }
    else if (lower.find("old_growth_spruce") != std::string::npos) {
        grass   = mce::Color(0.525f, 0.718f, 0.514f, 1.0f);
        foliage = mce::Color(0.408f, 0.643f, 0.392f, 1.0f);
    }
    else if (lower.find("wooded_badlands") != std::string::npos) {
        grass   = mce::Color(0.565f, 0.506f, 0.302f, 1.0f);
        foliage = mce::Color(0.620f, 0.506f, 0.302f, 1.0f);
    }
    else if (lower.find("sparse_jungle") != std::string::npos) {
        grass   = mce::Color(0.392f, 0.780f, 0.247f, 1.0f);
        foliage = mce::Color(0.243f, 0.722f, 0.059f, 1.0f);
    }
    else if (lower.find("snowy_beach") != std::string::npos) {
        grass   = mce::Color(0.514f, 0.710f, 0.576f, 1.0f);
        foliage = mce::Color(0.392f, 0.635f, 0.471f, 1.0f);
    }
    else if (lower.find("ice_spikes") != std::string::npos) {
        grass   = mce::Color(0.502f, 0.706f, 0.592f, 1.0f);
        foliage = mce::Color(0.376f, 0.631f, 0.482f, 1.0f);
    }
    else if (lower.find("the_void") != std::string::npos || lower.find("void") != std::string::npos) {
        grass   = mce::Color(0.557f, 0.725f, 0.443f, 1.0f);
        foliage = mce::Color(0.443f, 0.655f, 0.302f, 1.0f);
    }
    else if (lower.find("deep_lukewarm_ocean") != std::string::npos) {
        grass   = mce::Color(0.557f, 0.725f, 0.443f, 1.0f);
        foliage = mce::Color(0.443f, 0.655f, 0.302f, 1.0f);
    }
    else if (lower.find("deep_cold_ocean") != std::string::npos) {
        grass   = mce::Color(0.557f, 0.725f, 0.443f, 1.0f);
        foliage = mce::Color(0.443f, 0.655f, 0.302f, 1.0f);
    }
    else if (lower.find("deep_frozen_ocean") != std::string::npos) {
        grass   = mce::Color(0.557f, 0.725f, 0.443f, 1.0f);
        foliage = mce::Color(0.443f, 0.655f, 0.302f, 1.0f);
    }
    else {
        LogColorMiss("BiomeMiss", biomeName);
    }
    }; // [性能] lambda 闭合
    computeTints();
    // 写回缓存（上层已对 biomeName 为空做 return）
    BiomeTintTriple tri{grass, foliage, water};
    s_tintCache.emplace(Fnv1aHash(biomeName), tri);
}

// =========================================================================
// [群系地面与水体平滑过渡算法 (Biome Color Blending)]
// 基于连续双线性插值消除 4x4 生物群系单元阶梯锯齿
// =========================================================================

inline bool IsBiomeTintedBlock(std::string const& name) noexcept {
    if (name.find("grass") != std::string::npos ||
        name.find("fern") != std::string::npos ||
        name.find("double_plant") != std::string::npos ||
        name.find("bush") != std::string::npos ||
        name.find("vine") != std::string::npos ||
        name.find("leaf") != std::string::npos ||
        name.find("leaves") != std::string::npos) {
        if (name.find("dry_grass") != std::string::npos) return false;
        if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos) return false;
        if (name.find("cherry_leaves") != std::string::npos ||
            name.find("mangrove_leaves") != std::string::npos ||
            name.find("pale_oak_leaves") != std::string::npos ||
            name.find("pale_leaves") != std::string::npos) return false;
        return true;
    }
    return false;
}

// [沼泽双色草地噪声生成]
// 原版沼泽 (swamp / swampland，不含红树林沼泽 mangrove_swamp) 的草地并非单一枯萎橄榄色，
// 而是基于原版 PerlinSimplexNoise(2345u, 1) 在 (worldX * 0.0225f, worldZ * 0.0225f) 采样，
// 精确对应实机 swamp_grass.png 调色板 (128/129 分界点 noiseVal ≈ -0.096f / -0.1f)：
// noiseVal < -0.096f 为【深邃沼泽绿 (#4C763C)】，noiseVal >= -0.096f 为【枯萎橄榄褐 (#6A7039)】
inline bool IsSwampDualToneBiome(std::string const& rawName) noexcept {
    if (rawName.empty()) return false;
    std::string lower = rawName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    return lower.find("swamp") != std::string::npos && lower.find("mangrove") == std::string::npos;
}

inline float HashGrid2D(int gx, int gz, uint32_t seed) noexcept {
    uint32_t h = (uint32_t)gx * 374761393u + (uint32_t)gz * 668265263u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return (float)(h & 0xFFFFu) / 65535.0f;
}

inline float SmoothValueNoise2D(float fx, float fz, uint32_t seed) noexcept {
    int x0 = (int)std::floor(fx);
    int z0 = (int)std::floor(fz);
    float tx = fx - (float)x0;
    float tz = fz - (float)z0;
    float ux = tx * tx * (3.0f - 2.0f * tx);
    float uz = tz * tz * (3.0f - 2.0f * tz);

    float v00 = HashGrid2D(x0,     z0,     seed);
    float v10 = HashGrid2D(x0 + 1, z0,     seed);
    float v01 = HashGrid2D(x0,     z0 + 1, seed);
    float v11 = HashGrid2D(x0 + 1, z0 + 1, seed);

    float vx0 = v00 + (v10 - v00) * ux;
    float vx1 = v01 + (v11 - v01) * ux;
    return vx0 + (vx1 - vx0) * uz;
}

inline mce::Color ComputeSwampGrassColor(int worldX, int worldZ, bool smoothEdge = true) noexcept {
    // 直接调用游戏引擎自身的 PerlinSimplexNoise(2345u, 1)，与实机 GrassTessellationPolicy / getMapGrassColor 1:1 对齐
    static const PerlinSimplexNoise s_swampGrassNoise(2345u, 1);
    float noiseVal = s_swampGrassNoise.getValue((float)worldX * 0.0225f, (float)worldZ * 0.0225f);

    float t = 0.0f;
    if (smoothEdge) {
        // 对应实机 GrassTessellationPolicy 5x5 邻域平滑采样，在 -0.096f (swamp_grass.png 128/129 分界) 处做 ~2 格抗锯齿过渡
        t = std::clamp((noiseVal - (-0.126f)) / 0.060f, 0.0f, 1.0f);
        t = t * t * (3.0f - 2.0f * t);
    } else {
        t = (noiseVal < -0.096f) ? 0.0f : 1.0f;
    }

    // 深绿色 (实机沼泽深绿草地采样校准) vs 枯萎暖橄榄色
    constexpr float kDeepGreenR = 0.205f, kDeepGreenG = 0.365f, kDeepGreenB = 0.172f;
    constexpr float kWitheredR  = 0.285f, kWitheredG  = 0.300f, kWitheredB  = 0.130f;
    return mce::Color(
        kDeepGreenR + (kWitheredR - kDeepGreenR) * t,
        kDeepGreenG + (kWitheredG - kDeepGreenG) * t,
        kDeepGreenB + (kWitheredB - kDeepGreenB) * t,
        1.0f
    );
}

// [熔岩动态纹理色调校准] 还原实机熔岩池橙黄亮脉与暖橙基底的熔融层次，消除死板纯红失真
inline mce::Color ApplyLavaPatternTint(int worldX, int worldZ) noexcept {
    float n1 = SmoothValueNoise2D((float)worldX * 0.32f, (float)worldZ * 0.32f, 0x4C415641u);
    float n2 = HashGrid2D(worldX, worldZ, 0x9E3779B9u);
    float t  = std::clamp(n1 * 0.65f + n2 * 0.35f, 0.0f, 1.0f);

    // 在暖橙基底 RGB(206, 89, 15) 与亮金橙脉络 RGB(232, 124, 36) 之间自然过渡（均值约 RGB(218, 105, 23)）
    return mce::Color(
        0.808f + 0.102f * t,
        0.349f + 0.137f * t,
        0.059f + 0.082f * t,
        1.0f
    );
}

inline void GetCellBiomeTint(BlockSource& region, int cellX, int cellZ, short sampleY,
                             BiomeTintTriple& outTriple, const BiomeTintTriple& fallbackTriple) {
    static thread_local std::unordered_map<uint32_t, BiomeTintTriple> s_cellMap;
    if (s_cellMap.size() > 4096) s_cellMap.clear();

    uint32_t cellKey = ((uint32_t)(cellX & 0xFFFF) << 16) | ((uint32_t)(cellZ & 0xFFFF));
    auto it = s_cellMap.find(cellKey);
    if (it != s_cellMap.end()) {
        outTriple = it->second;
        return;
    }

    int sampleWorldX = (cellX << 2) + 2;
    int sampleWorldZ = (cellZ << 2) + 2;
    try {
        auto const& b = region.getBiome(BlockPos(sampleWorldX, (int)sampleY, sampleWorldZ));
        std::string bName = b.mHash->getString();
        if (!bName.empty()) {
            getBiomeTints(bName, outTriple.grass, outTriple.foliage, outTriple.water);
            outTriple.isSwampDualTone = IsSwampDualToneBiome(bName);
            s_cellMap[cellKey] = outTriple;
            return;
        }
    } catch (...) {}

    outTriple = fallbackTriple;
    s_cellMap[cellKey] = outTriple;
}

inline void GetBlendedBiomeTints(BlockSource& region, int targetX, int targetZ, short sampleY,
                                const BiomeTintTriple& fallbackTriple,
                                mce::Color& grass, mce::Color& foliage, mce::Color& water) {
    if (!MapRenderState::biomeBlending) {
        BiomeTintTriple t00;
        GetCellBiomeTint(region, targetX >> 2, targetZ >> 2, sampleY, t00, fallbackTriple);
        grass   = t00.isSwampDualTone ? ComputeSwampGrassColor(targetX, targetZ, false) : t00.grass;
        foliage = t00.foliage;
        water   = t00.water;
        return;
    }

    float ux = ((float)targetX - 1.5f) * 0.25f;
    float uz = ((float)targetZ - 1.5f) * 0.25f;
    int cx0 = (int)std::floor(ux);
    int cz0 = (int)std::floor(uz);
    int cx1 = cx0 + 1;
    int cz1 = cz0 + 1;
    float tx = ux - (float)cx0;
    float tz = uz - (float)cz0;

    BiomeTintTriple t00, t10, t01, t11;
    GetCellBiomeTint(region, cx0, cz0, sampleY, t00, fallbackTriple);
    GetCellBiomeTint(region, cx1, cz0, sampleY, t10, fallbackTriple);
    GetCellBiomeTint(region, cx0, cz1, sampleY, t01, fallbackTriple);
    GetCellBiomeTint(region, cx1, cz1, sampleY, t11, fallbackTriple);

    // 沼泽群系草地直接按当前真实世界坐标 (targetX, targetZ) 逐格采样原版 PerlinSimplexNoise，杜绝 4x4 网格偏移与量化失真
    if (t00.isSwampDualTone || t10.isSwampDualTone || t01.isSwampDualTone || t11.isSwampDualTone) {
        mce::Color swampGrass = ComputeSwampGrassColor(targetX, targetZ, true);
        if (t00.isSwampDualTone) t00.grass = swampGrass;
        if (t10.isSwampDualTone) t10.grass = swampGrass;
        if (t01.isSwampDualTone) t01.grass = swampGrass;
        if (t11.isSwampDualTone) t11.grass = swampGrass;
    }

    // 快速分支：四角群系色完全相同时(>95%情形)，直接返回无插值开销
    if (t00.grass.r == t10.grass.r && t00.grass.r == t01.grass.r && t00.grass.r == t11.grass.r &&
        t00.grass.g == t10.grass.g && t00.grass.g == t01.grass.g && t00.grass.g == t11.grass.g &&
        t00.foliage.r == t10.foliage.r && t00.foliage.r == t01.foliage.r && t00.foliage.r == t11.foliage.r &&
        t00.water.r == t10.water.r && t00.water.r == t01.water.r && t00.water.r == t11.water.r) {
        grass   = t00.grass;
        foliage = t00.foliage;
        water   = t00.water;
        return;
    }

    float w00 = (1.0f - tx) * (1.0f - tz);
    float w10 = tx * (1.0f - tz);
    float w01 = (1.0f - tx) * tz;
    float w11 = tx * tz;

    grass = mce::Color(
        w00 * t00.grass.r + w10 * t10.grass.r + w01 * t01.grass.r + w11 * t11.grass.r,
        w00 * t00.grass.g + w10 * t10.grass.g + w01 * t01.grass.g + w11 * t11.grass.g,
        w00 * t00.grass.b + w10 * t10.grass.b + w01 * t01.grass.b + w11 * t11.grass.b,
        1.0f
    );

    foliage = mce::Color(
        w00 * t00.foliage.r + w10 * t10.foliage.r + w01 * t01.foliage.r + w11 * t11.foliage.r,
        w00 * t00.foliage.g + w10 * t10.foliage.g + w01 * t01.foliage.g + w11 * t11.foliage.g,
        w00 * t00.foliage.b + w10 * t10.foliage.b + w01 * t01.foliage.b + w11 * t11.foliage.b,
        1.0f
    );

    water = mce::Color(
        w00 * t00.water.r + w10 * t10.water.r + w01 * t01.water.r + w11 * t11.water.r,
        w00 * t00.water.g + w10 * t10.water.g + w01 * t01.water.g + w11 * t11.water.g,
        w00 * t00.water.b + w10 * t10.water.b + w01 * t01.water.b + w11 * t11.water.b,
        1.0f
    );
}


#include "render/BlockColorMap.h"

// [辅助] 判断方块是否为隐形/技术性覆盖方块（地图扫描应穿透保留底层方块，不渲染为黑洞或杂色）
inline bool IsInvisibleOrTechnicalOverlay(std::string const& rawName) noexcept {
    return BlockColorMap::IsInvisibleOrTechnicalOverlay(rawName);
}

// [原木与菌柄判断] 检测方块是否为有年轮/树皮朝向区分的原木或菌柄
inline bool IsPillarLogOrStem(std::string const& rawName) noexcept {
    return BlockColorMap::IsPillarLogOrStem(rawName);
}

// [水平放置原木转树皮名称] 水平放置时映射为对应树皮材质 (_log -> _wood, _stem -> _hyphae)
inline std::string GetHorizontalWoodName(std::string const& name) {
    return BlockColorMap::GetHorizontalWoodName(name);
}

// [原木朝向判断] 判断原木或菌柄是否水平放置 (X轴或Z轴，顶面显示树皮侧面而非年轮横截面)
inline bool IsBlockPillarHorizontal(Block const& block) noexcept {
    try {
        static const HashedString s_axisKey("pillar_axis");
        auto const* stateMap = block.getBlockType().mStateNameMap.operator->();
        if (stateMap) {
            auto it = stateMap->find(s_axisKey);
            if (it != stateMap->end()) {
                auto val = block.getState<int>(it->second);
                if (val.has_value()) {
                    return *val != 0; // 0 = Y (垂直，年轮向上), 1 = X (水平，树皮向上), 2 = Z (水平，树皮向上)
                }
            }
        }
    } catch (...) {}

    // 兜底回退：基岩版传统方块数据位 2-3（0=Y, 1=X, 2=Z, 3=全树皮）
    ushort d = block.getData();
    return ((d >> 2) & 3) != 0;
}

inline mce::Color getBlockColor(std::string const& rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol) {
    return BlockColorMap::GetBlockColor(rawName, grassCol, foliageCol, waterCol);
}

// ==========================================
// [传送日志器] 文件级日志，便于调试与问题追踪
// 日志路径：<mod_data_dir>/teleport_log.txt，按行追加
// ==========================================
inline void LogTeleport(const std::string& message) {
    try {
        static std::string logPath;
        if (logPath.empty()) {
            auto dataDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir();
            logPath = (dataDir / "teleport_log.txt").string();
        }
        std::ofstream log(logPath, std::ios::app);
        if (log) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf;
            localtime_s(&tm_buf, &time);
            char timeBuf[64];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm_buf);
            log << "[" << timeBuf << "] " << message << std::endl;
        }
    } catch (...) {
        // 日志失败绝不影响传送主流程
    }
}

// ==========================================
// [服务器指令发送器] 通过 CommandRequestPacket 直发指令
// 等价于聊天栏输入指令，服务器端执行，避免客户端回弹
// ==========================================
inline void SendServerCommand(Player& player, std::string const& cmd) {
    auto origin = std::make_unique<PlayerCommandOrigin>(player.getLevel(), player.getOrCreateUniqueID());
    CommandContext ctx(cmd, std::move(origin), (int)CurrentCmdVersion::Latest);
    CommandRequestPacketPayload payload(ctx, false);
    CommandRequestPacket packet(std::move(payload));
    packet.sendToServer();
}

// ==========================================
// [安全地表高度查询] SEH 包装，防止部分加载区块的 NULL subchunk 解引用崩溃
// 崩溃根因：Phase 0 传送到未访问区域后，区块处于部分加载状态（部分 subchunk 指针为 NULL）
//   getAboveTopSolidBlock 遍历 subchunk 时执行 mov rax,[rax+0x08] (RAX=0) → 0xC0000005
// 解决方案：__try/__except 捕获 AV，返回 -32000 哨兵表示"区块未就绪"，调用方重试
// 注意：__try/__except 不能与 C++ 对象析构共存，故本函数仅使用 POD 类型
// ==========================================
inline short SafeGetSurfaceY(BlockSource& region, int x, int z) noexcept {
    short result = -32000;
    __try {
        // includeUnloaded=true 保留原行为（对已加载区块返回真实 Y，对完全未加载返回 ≤-64）
        // 部分加载区块的 NULL subchunk AV 由 __except 兜底
        result = region.getAboveTopSolidBlock(x, z, true, true);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // 部分加载区块触发 AV → 视为"未就绪"，调用方下次重试
        result = -32000;
    }
    return result;
}

// [安全光照查询] try/catch 包装的 getBrightnessPair 查询 (/EHa 模式下可捕获 SEH AV)，返回打包字节 (skyLight << 4) | blockLight
inline uint8_t SafeGetPackedLight(BlockSource& region, int x, int y, int z, int intrinsicBlockLight = 0, bool defaultFullSky = true) noexcept {
    uint8_t defSky = defaultFullSky ? 15 : 0;
    uint8_t defBlk = (uint8_t)std::clamp(intrinsicBlockLight, 0, 15);
    uint8_t result = (uint8_t)((defSky << 4) | defBlk);
    try {
        int clampedY = (y < -64) ? -64 : ((y > 319) ? 319 : y);
        BrightnessPair bp = region.getBrightnessPair(BlockPos(x, clampedY, z));
        uint8_t sl = *(reinterpret_cast<const uint8_t*>(&bp.sky));
        uint8_t bl = *(reinterpret_cast<const uint8_t*>(&bp.block));
        if (sl > 15) sl = 15;
        if (bl > 15) bl = 15;
        if ((int)bl < intrinsicBlockLight) bl = defBlk;
        result = (uint8_t)((sl << 4) | bl);
    } catch (...) {
    }
    return result;
}

// ==========================================
// [安全生物群系名查询] try/catch 包装，防止部分加载区块 AV
// 与 SafeGetSurfaceY 的区别：getBiome 返回 Biome const& 并需 std::string 赋值（含析构），
//   故不能用 __try/__except（不能与 C++ 对象析构共存），改用 try/catch(...)
//   在 /EHa 模式下 catch(...) 可捕获 SEH AV，与 PlayerHook.h:616,773 现有调用风格一致
// ==========================================
inline bool SafeGetBiomeName(BlockSource& region, int x, int y, int z,
                             std::string& outRawName) noexcept {
    try {
        auto const& biome = region.getBiome(BlockPos(x, y, z));
        outRawName = biome.mHash->getString();
        return !outRawName.empty();
    } catch (...) {
        // 部分加载区块触发 AV/其他异常 → 视为"未就绪"，调用方下次重试
        return false;
    }
}

// [辅助] 判断方块名是否为液体（水/熔岩）— 传送安全共用
inline bool IsLiquidBlockName(std::string const& name) noexcept {
    return name.find("water") != std::string::npos ||
           name.find("lava") != std::string::npos;
}

// [辅助] 判断方块名是否为空气/可穿透（玩家可站立其中）— 传送安全共用
inline bool IsAirLikeName(std::string const& name) noexcept {
    if (name.empty()) return false;
    if (name.find("air") != std::string::npos) return true;
    if (name.find("barrier") != std::string::npos) return true;
    if (name.find("light_block") != std::string::npos) return true;
    if (name.find("structure_void") != std::string::npos) return true;
    if (name.find("placeholder") != std::string::npos) return true;
    if (name.find("info_update") != std::string::npos) return true;
    if (name.find("invisible_bedrock") != std::string::npos || name.find("invisiblebedrock") != std::string::npos) return true;
    if (name.find("moving_block") != std::string::npos || name.find("movingblock") != std::string::npos) return true;
    if (name.find("piston_arm_collision") != std::string::npos || name.find("pistonarmcollision") != std::string::npos) return true;
    if (name.find("border_block") != std::string::npos) return true;
    return false;
}

// [辅助] 判断方块名是否为"石头类"灰色污染候选
// 地表扫描中, 部分加载区块 SafeGetSurfaceY 可能返回洞穴天花板Y, 读取到石头/深板岩等,
// 写入缓存造成地表大地图灰色块状污染。检测到此类方块时回退查缓存地表Y重新读取。
inline bool IsStoneLikeBlock(std::string const& name) noexcept {
    // stone 必须先于 sandstone/redstone 等含 "stone" 子串的非石头方块排除
    if (name.find("sandstone") != std::string::npos) return false;
    if (name.find("redstone") != std::string::npos) return false;
    if (name.find("glowstone") != std::string::npos) return false;
    if (name.find("lodestone") != std::string::npos) return false;
    if (name.find("end_stone") != std::string::npos) return false;
    if (name.find("blackstone") != std::string::npos) return false;
    if (name.find("dripstone") != std::string::npos) return false;
    if (name.find("moss_block") != std::string::npos) return false;
    // 真正的石头类: stone/cobblestone/andesite/diorite/granite/deepslate/tuff/bedrock/gravel
    return name.find("stone") != std::string::npos ||
           name.find("cobble") != std::string::npos ||
           name.find("andesite") != std::string::npos ||
           name.find("diorite") != std::string::npos ||
           name.find("granite") != std::string::npos ||
           name.find("deepslate") != std::string::npos ||
           name.find("tuff") != std::string::npos ||
           name.find("bedrock") != std::string::npos ||
           name.find("gravel") != std::string::npos;
}

// ==========================================
// [安全方块查询] try/catch 包装的 getBlock 查询
// 与 SafeGetBiomeName 同模式：在 /EHa 模式下 catch(...) 可捕获 SEH AV 并保证 C++ 栈正确展开，
// 避免 std::string 析构与 SEH 冲突（防止部分加载 chunk 的 NULL subchunk 引发堆损坏 0xC0000374）
// ==========================================
inline bool SafeGetBlockName(BlockSource& region, int x, int y, int z, std::string& outName) noexcept {
    try {
        auto const& block = region.getBlock(BlockPos(x, y, z));
        std::string name = block.getTypeName();
        if (!name.empty()) {
            outName = std::move(name);
            return true;
        }
    } catch (...) {
    }
    outName.clear();
    return false;
}

// ==================== 洞穴材质判定 (汲取 0.3.4 优势: Material枚举精准识别通道/水体) ====================
// 洞穴分层投影：所有列从同一个 Top Y 向下解析，避免相邻列跳到不同高度层。
inline constexpr int kCaveLayerTopOffset = 3;
inline constexpr int kCaveLayerAirSearchDepth = 64;
inline constexpr int kCaveLayerFloorSearchDepth = 64;

inline ChiyanMapMaterialType GetCaveMaterialType(Block const& block) {
    return block.getBlockType().mMaterial.mType;
}

inline bool IsCaveWaterBlock(Block const& block) {
    if (block.isAir()) return false;
    ChiyanMapMaterialType material = GetCaveMaterialType(block);
    return material == ChiyanMapMaterialType::Water || material == ChiyanMapMaterialType::Bubble;
}

inline constexpr float kWaterOverlayAlpha = 0.65f;
inline constexpr mce::Color kDefaultWaterTint(0.18f, 0.38f, 0.85f, 1.0f);

// 水色叠加在洞底颜色之上；避免液体单独饱和色显得突兀。
inline mce::Color BlendWaterOverFloor(mce::Color floorColor, mce::Color waterTint) {
    if (waterTint.a <= 0.01f) waterTint = kDefaultWaterTint;
    return mce::Color(
        floorColor.r + (waterTint.r - floorColor.r) * kWaterOverlayAlpha,
        floorColor.g + (waterTint.g - floorColor.g) * kWaterOverlayAlpha,
        floorColor.b + (waterTint.b - floorColor.b) * kWaterOverlayAlpha,
        1.0f
    );
}

// 洞穴投影和自动判定共享此规则：流体、植被与透明方块都不形成洞穴墙体。
// 相比字符串匹配，Material 枚举更精准，避免漏判新方块导致黑灰/红块。
inline bool IsCavePassableBlock(Block const& block) {
    if (block.isAir()) return true;
    ChiyanMapMaterialType material = GetCaveMaterialType(block);
    switch (material) {
    case ChiyanMapMaterialType::Air:
    case ChiyanMapMaterialType::Water:
    case ChiyanMapMaterialType::Bubble:
    case ChiyanMapMaterialType::Plant:
    case ChiyanMapMaterialType::SolidPlant:
    case ChiyanMapMaterialType::Leaves:
    case ChiyanMapMaterialType::Glass:
    case ChiyanMapMaterialType::Ice:
    case ChiyanMapMaterialType::PowderSnow:
    case ChiyanMapMaterialType::Cactus:
    case ChiyanMapMaterialType::Fire:
    case ChiyanMapMaterialType::Portal:
    case ChiyanMapMaterialType::Grate:
    case ChiyanMapMaterialType::StoneDecoration:
    case ChiyanMapMaterialType::DecorationSolid:
    case ChiyanMapMaterialType::NonSolid:
    case ChiyanMapMaterialType::StructureVoid:
        return true;
    default:
        break;
    }
    if (material == ChiyanMapMaterialType::Wood) {
        std::string const& name = block.getTypeName();
        return name.find("log") != std::string::npos || name.find("stem") != std::string::npos;
    }
    return false;
}

// ==================== 洞穴地图系统 ====================

// [洞穴辅助] 判断方块是否为"覆盖层"（透明/非实心），在洞穴列扫描中跳过
// 忽略空气、液体、玻璃等不可见/穿透方块: air, liquid, glass, torch, grass, flowers, leaves
inline bool IsCaveOverlayBlockName(std::string const& rawName) noexcept {
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    if (IsAirLikeName(name)) return true;
    if (IsLiquidBlockName(name)) return true;  // 液体在洞穴扫描中视为可穿透层
    if (name.find("glass") != std::string::npos) return true;
    if (name.find("torch") != std::string::npos || name.find("lantern") != std::string::npos) return true;
    if (name.find("chain") != std::string::npos) return true;
    if (name.find("pale_hanging_moss") != std::string::npos) return true;
    if (name.find("pointed_dripstone") != std::string::npos) return true;
    if (name.find("amethyst_cluster") != std::string::npos || name.find("amethyst_bud") != std::string::npos) return true;
    if ((name.find("grass") != std::string::npos && name.find("grass_block") == std::string::npos) || name.find("fern") != std::string::npos) return true;
    if (name.find("flower") != std::string::npos || name.find("bush") != std::string::npos) return true;
    if (name.find("leaves") != std::string::npos) return true;
    if (name.find("vine") != std::string::npos || name.find("lichen") != std::string::npos) return true;
    if (name.find("roots") != std::string::npos || name.find("dripleaf") != std::string::npos) return true;
    if (name.find("sculk_vein") != std::string::npos) return true;
    if (name.find("sapling") != std::string::npos) return true;
    if (name.find("mushroom") != std::string::npos || name.find("spore") != std::string::npos) return true;
    if (name.find("seagrass") != std::string::npos || name.find("kelp") != std::string::npos) return true;
    if (name.find("snow") != std::string::npos && name.find("snow_block") == std::string::npos) return true;
    if (name.find("rail") != std::string::npos) return true;
    if (name.find("ladder") != std::string::npos) return true;
    if (name.find("sign") != std::string::npos) return true;
    if (name.find("banner") != std::string::npos) return true;
    if (name.find("button") != std::string::npos) return true;
    if (name.find("pressure_plate") != std::string::npos) return true;
    if (name.find("lever") != std::string::npos) return true;
    if (name.find("door") != std::string::npos) return true;
    if (name.find("trapdoor") != std::string::npos) return true;
    if (name.find("fence") != std::string::npos) return true;
    if (name.find("wall") != std::string::npos && name.find("cobblestone_wall") == std::string::npos) return true;
    if (name.find("carpet") != std::string::npos) return true;
    if (name.find("web") != std::string::npos) return true;
    if (name.find("lily") != std::string::npos) return true;
    if (name.find("candle") != std::string::npos) return true;
    if (name.find("wheat") != std::string::npos || name.find("carrot") != std::string::npos ||
        name.find("potato") != std::string::npos || name.find("beetroot") != std::string::npos ||
        name.find("crop") != std::string::npos) return true;
    return false;
}

// [辅助] 安全判断指定坐标是否为可穿透/非实心天花板阻挡方块
inline bool IsPassableCeilingBlock(BlockSource& region, int x, int y, int z) noexcept {
    std::string name;
    if (!SafeGetBlockName(region, x, y, z, name)) return true;
    if (name.empty()) return true;
    if (IsAirLikeName(name) || IsCaveOverlayBlockName(name) || IsLiquidBlockName(name)) return true;
    if (name.find("log") != std::string::npos || name.find("stem") != std::string::npos) return true;
    return false;
}

// [洞穴检测] 智能检测玩家是否身处真正的地下洞穴中
// 彻底解决巨型洞穴误显地表、深层洞穴变黑、以及浮空岛/大树冠/屋檐误判为洞穴的问题
// 返回值: true=在洞穴中, caveStartY=洞穴起始Y
inline bool DetectCaveStart(BlockSource& region, int playerX, int playerY, int playerZ, int& outCaveStartY) noexcept {
    if (playerY <= -64 || playerY >= 315) return false;

    // 1. 获取玩家所在列的最高地表高度
    short surfaceY = SafeGetSurfaceY(region, playerX, playerZ);

    // 如果玩家自身高度已经等于或超过最高地表高度 (例如站在开阔地表、山顶、高台或悬崖顶)，直接为地表
    if (surfaceY > -64 && surfaceY != -32000 && surfaceY <= playerY + 2) return false;

    // 2. 深层地下快速通道 (playerY < 55):
    // 主世界海平面为 62，地下 Y < 55 绝无自然浮空岛、树冠或人工遮阳棚。
    // 在负高度 (如 Y = -9, Y = -46) 及海平面以下，只要处于封闭岩体或深裂谷中，100% 为地下洞穴。
    if (playerY < 55) {
        // 向上寻找首个实心岩石/深板岩/泥土天花板 (搜索上限放宽至 80 格或 surfaceY，覆盖 1.18+ 巨型穹顶洞穴)
        int ceilingY = -1;
        int maxScan = (surfaceY > playerY + 2 && surfaceY != -32000) ? std::min((int)surfaceY, playerY + 80) : std::min(319, playerY + 80);
        for (int y = playerY + 2; y <= maxScan; y++) {
            if (!IsPassableCeilingBlock(region, playerX, y, playerZ)) {
                ceilingY = y;
                break;
            }
        }
        if (ceilingY != -1) {
            // 命中实心天花板：无需做多层空腔 airAbove 检查 (真实洞穴常有上下多层空腔/废弃矿井/裂隙)
            outCaveStartY = ceilingY;
            return true;
        }

        // 头顶未在 80 格内探测到实心天花板 (超巨型开敞裂谷/天坑深渊):
        // 若地表高度远高于玩家 (surfaceY > playerY + 8)，玩家身处深渊裂谷底部，依然应显示裂谷而非高空地表
        if (surfaceY > playerY + 8 && surfaceY != -32000) {
            // 排除开阔海洋游泳 (若从脚下到地表全为水体且无天花板，则为海洋地表)
            std::string currentBlock;
            if (SafeGetBlockName(region, playerX, playerY, playerZ, currentBlock) &&
                IsLiquidBlockName(currentBlock)) {
                return false;
            }
            outCaveStartY = std::min((int)surfaceY - 1, playerY + 40);
            return true;
        }

        // 负高度极其深层 (如 Y < 0 或 Y < 30): 在主世界绝不可能为露天地表，即使区块加载延迟也视为地下
        if (playerY < 30) {
            outCaveStartY = playerY + 24;
            return true;
        }

        return false;
    }

    // 3. 高空与近地表环境检测 (playerY >= 55):
    // 此时玩家可能处于山峰、森林、房屋遮阳棚、悬崖突出岩壁或半山腰洞穴中。
    // 需严格过滤浮空岛、树冠、建筑屋檐，避免误判。
    if (surfaceY <= -64 || surfaceY == -32000) return false;  // 未加载区块

    // 向上探测玩家头顶的第一个实心天花板 (非空气、非植被、非树叶/原木、非透明方块)
    int ceilingY = -1;
    int maxScanCeilingY = std::min((int)surfaceY, playerY + 60);
    for (int y = playerY + 2; y <= maxScanCeilingY; y++) {
        if (!IsPassableCeilingBlock(region, playerX, y, playerZ)) {
            ceilingY = y;
            break;
        }
    }
    if (ceilingY == -1) return false;

    // 检查天花板厚度: 若天花板极薄 (<= 2 格实心方块，如木板挑檐/遮阳板/薄石拱桥)
    int solidThickness = 0;
    for (int y = ceilingY; y <= (int)surfaceY && solidThickness <= 4; y++) {
        if (IsPassableCeilingBlock(region, playerX, y, playerZ)) {
            break;
        }
        solidThickness++;
    }
    if (solidThickness <= 2) return false;

    // 头顶净空高度检测 (针对高空浮空岛/高空突出悬崖):
    int clearance = ceilingY - (playerY + 2);
    if (clearance > 8) {
        std::string floorName;
        if (SafeGetBlockName(region, playerX, playerY - 1, playerZ, floorName)) {
            if (floorName.find("grass_block") != std::string::npos ||
                floorName.find("dirt_with_roots") != std::string::npos ||
                floorName.find("podzol") != std::string::npos ||
                floorName.find("mycelium") != std::string::npos) {
                return false;
            }
        }
    }

    // 四周地表采样判决:
    static const struct { int dx, dz; } kSampleOffsets[] = {
        { -8,   0 }, {  8,   0 }, {  0,  -8 }, {  0,   8 },
        { -14,  0 }, { 14,   0 }, {  0, -14 }, {  0,  14 },
        { -10, -10 }, { 10, -10 }, { -10, 10 }, { 10, 10 }
    };

    int openSurfaceCount = 0;
    int validSampleCount = 0;
    for (const auto& offset : kSampleOffsets) {
        short sY = SafeGetSurfaceY(region, playerX + offset.dx, playerZ + offset.dz);
        if (sY <= -64 || sY == -32000) continue;
        validSampleCount++;
        if (sY <= playerY + 5) {
            openSurfaceCount++;
        }
    }

    if (validSampleCount >= 4 && openSurfaceCount >= 2) {
        return false;
    }
    if (validSampleCount > 0 && openSurfaceCount >= 1 && clearance > 6) {
        return false;
    }

    // 周边有露天采样点且脚下为草方块，直接排除洞穴
    if (openSurfaceCount > 0) {
        std::string floorName;
        if (SafeGetBlockName(region, playerX, playerY - 1, playerZ, floorName)) {
            if (floorName.find("grass_block") != std::string::npos) {
                return false;
            }
        }
    }

    // 针对特大浮空岛 (半径超过 14 格): 扩展至 24 格采样
    if (clearance > 8 || playerY >= 62) {
        static const struct { int dx, dz; } kFarOffsets[] = {
            { -24, 0 }, { 24, 0 }, { 0, -24 }, { 0, 24 },
            { -18, -18 }, { 18, -18 }, { -18, 18 }, { 18, 18 }
        };
        int farOpenCount = 0;
        int farValidCount = 0;
        for (const auto& offset : kFarOffsets) {
            short sY = SafeGetSurfaceY(region, playerX + offset.dx, playerZ + offset.dz);
            if (sY <= -64 || sY == -32000) continue;
            farValidCount++;
            if (sY <= playerY + 6) {
                farOpenCount++;
            }
        }
        if (farValidCount >= 4 && farOpenCount >= 2) {
            return false;
        }
        if (farValidCount > 0 && farOpenCount >= 1 && clearance > 10) {
            return false;
        }
    }

    outCaveStartY = ceilingY;
    return true;
}

// [自动洞穴天花板矩阵检测] 支持按用户配置的尺寸检测头顶实心顶棚:
//   autoCaveMode == -1: Smart (智能自适应多特征防误判检测)
//   autoCaveMode ==  0: OFF   (关闭自动进入洞穴)
//   autoCaveMode ==  1: 1x1   (半径 0，仅检测玩家正上方 1x1 柱)
//   autoCaveMode ==  2: 3x3   (半径 1，检测玩家头顶 3x3 实心天花板矩阵)
//   autoCaveMode ==  3: 5x5   (半径 2，检测玩家头顶 5x5 实心天花板矩阵)
//   autoCaveMode ==  4: 7x7   (半径 3，检测玩家头顶 7x7 实心天花板矩阵)
//   autoCaveMode ==  5: 9x9   (半径 4，检测玩家头顶 9x9 实心天花板矩阵)
inline bool DetectCaveStartWithRoofSize(BlockSource& region, int playerX, int playerY, int playerZ,
                                        int autoCaveMode, int& outCaveStartY) noexcept {
    if (autoCaveMode == 0) return false;
    if (autoCaveMode < 0) {
        return DetectCaveStart(region, playerX, playerY, playerZ, outCaveStartY);
    }
    if (playerY <= -64 || playerY >= 315) return false;

    int radius = std::clamp(autoCaveMode - 1, 0, 4);
    int centerCeilingY = -1;

    for (int dx = -radius; dx <= radius; dx++) {
        for (int dz = -radius; dz <= radius; dz++) {
            int cx = playerX + dx;
            int cz = playerZ + dz;
            short sY = SafeGetSurfaceY(region, cx, cz);
            if (sY > -64 && sY != -32000 && sY <= playerY + 2) {
                return false;
            }
            int maxScan = (sY > playerY + 2 && sY != -32000) ? std::min((int)sY, playerY + 80) : std::min(319, playerY + 80);
            int colCeilingY = -1;
            for (int y = playerY + 2; y <= maxScan; y++) {
                if (!IsPassableCeilingBlock(region, cx, y, cz)) {
                    colCeilingY = y;
                    break;
                }
            }
            if (colCeilingY == -1) {
                return false;
            }
            if (dx == 0 && dz == 0) {
                centerCeilingY = colCeilingY;
            }
        }
    }

    outCaveStartY = (centerCeilingY != -1) ? centerCeilingY : (playerY + 4);
    return true;
}

// [洞穴亮度计算] 深度衰减亮度公式
// 深度衰减亮度参数:
//   legible=false (非清晰): finalBrightness = 0.375 + 0.625 * (1 - depth/caveDepth)
//   legible=true (清晰): finalBrightness = 1.0 (全亮, 由深度因子直接乘RGB)
// 参数: depth = 从 caveStart 向下的层数, caveDepth = 最大扫描深度
inline float ComputeCaveBrightness(int depth, int caveDepth) noexcept {
    if (caveDepth <= 0) return 1.0f;
    float t = 1.0f - std::clamp((float)depth / (float)caveDepth, 0.0f, 1.0f);
    if (MapRenderState::g_legibleCaveMaps) {
        return 1.0f;  // 清晰模式: 全亮, 深度通过 RGB 乘子控制
    }
    return 0.375f + 0.625f * t;  // 非清晰模式: 0.375(底) ~ 1.0(顶)
}

// [洞穴列扫描] 扫描单个 x,z 列, 从 startY 向下查找第一个紧邻空气的实心方块
// 从 caveStart 向下扫描到 caveStart - caveDepth (或 fullDepthMode 全深度扫描至世界底部)
// 核心逻辑: 只渲染洞穴空腔下方的方块 (洞穴地板/墙壁), 纯石头区域返回 false (透明)
// 液体方块 (熔岩/水) 在空气下方时返回, 由 GetCaveLiquidColor 应用饱和色 (不受深度衰减)
// 参数: region, x, z, startY(扫描起点Y), caveDepth(扫描深度), fullDepthMode(是否为 Full 全深度洞穴模式)
// 输出: outBlockName(方块名), outBlockY(方块Y), outDepth(深度), outHasWater(是否含水)
// 返回值: true=找到洞穴方块, false=列内无洞穴 (纯实心石头, 渲染为透明)
inline bool ScanColumnCave(BlockSource& region, int x, int z, int startY, int caveDepth,
                           std::string& outBlockName, int& outBlockY, int& outDepth, bool& outHasWater,
                           bool fullDepthMode = false) noexcept {
    outHasWater = false;
    int topY = startY;
    if (topY < -64) return false;

    int searchStartY = topY;
    if (fullDepthMode && MapRenderState::currentDimensionId != 1) {
        // Full 全深度模式 (主世界/末地):
        // 先向下穿透地表表层 (跳过天空空气、树冠树叶、地表水体，并穿过至少 3 格实心岩层/土层)，
        // 确保只抓取地壳下方的地下洞穴空腔，绝不把露天海床或普通地表误判为洞穴。
        int solidRoofCount = 0;
        int roofBottomY = -99999;
        for (int y = topY; y >= -64; y--) {
            try {
                Block const& block = region.getBlock(BlockPos(x, y, z));
                if (!IsCavePassableBlock(block)) {
                    solidRoofCount++;
                    if (solidRoofCount >= 3) {
                        roofBottomY = y - 1;
                        break;
                    }
                } else {
                    solidRoofCount = 0;
                }
            } catch (...) {
                continue;
            }
        }
        if (roofBottomY == -99999 || roofBottomY < -64) return false;
        searchStartY = roofBottomY;
    }

    // 阶段一: 从 searchStartY 向下找第一个"通道"方块(空气/水/植被等可穿透)
    int channelY = -99999;
    int airSearchBottom = fullDepthMode ? (MapRenderState::currentDimensionId == 1 ? 0 : -64)
                                        : std::max(-64, searchStartY - 64);
    for (int y = searchStartY; y >= airSearchBottom; y--) {
        try {
            Block const& block = region.getBlock(BlockPos(x, y, z));
            if (IsCavePassableBlock(block)) {
                channelY = y;
                break;
            }
        } catch (...) {
            continue;
        }
    }
    if (channelY == -99999) return false;  // 纯实体岩石列 = 透明(黑色背景)

    // 阶段二: 从通道Y向下找第一个非穿透方块(地板)
    // 深度至少允许向下探测 64 格 (Full 模式下直达世界底端 -64)
    int maxFloorDepth = fullDepthMode ? 384 : std::max(64, caveDepth);
    int floorSearchBottom = fullDepthMode ? -64 : std::max(-64, channelY - maxFloorDepth);
    int depth = 0;
    bool hasWater = false;
    for (int y = channelY; y >= floorSearchBottom; y--, depth++) {
        try {
            Block const& block = region.getBlock(BlockPos(x, y, z));
            if (IsCaveWaterBlock(block)) {
                hasWater = true;
                continue;  // 记录含水并继续向下寻找水下海床/基岩
            }
            if (IsCavePassableBlock(block)) continue;  // 通道内的空气或可穿透方块
            // 命中地板/墙壁方块
            outBlockName = block.getTypeName();
            if (IsPillarLogOrStem(outBlockName) && IsBlockPillarHorizontal(block)) {
                outBlockName = GetHorizontalWoodName(outBlockName);
            }
            outBlockY = y;
            outDepth = fullDepthMode ? (depth + std::max(0, (searchStartY - channelY) / 6)) : depth;
            outHasWater = hasWater;
            return true;
        } catch (...) {
            continue;
        }
    }

    if (hasWater) {
        // 纯水体直到底部仍未找到实心方块: 以水方块本身作为表面
        outBlockName = "minecraft:water";
        outBlockY = floorSearchBottom;
        outDepth = depth;
        outHasWater = true;
        return true;
    }

    // 有通道但 maxFloorDepth 内无地板 = 深洞，渲染深灰而非纯黑背景
    outBlockName = "__CAVE_DEEPHOLE__";
    outBlockY = std::max(channelY - maxFloorDepth, -64);
    outDepth = maxFloorDepth;
    outHasWater = false;
    return true;
}

// [洞穴特殊方块颜色] 液体方块在洞穴中使用饱和颜色 (不受深度衰减)
// 液体通过转换保持饱和色彩
inline mce::Color GetCaveLiquidColor(std::string const& name) noexcept {
    if (name.find("lava") != std::string::npos) return mce::Color(0.855f, 0.412f, 0.090f, 1.0f);
    if (name.find("water") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.90f, 1.0f);
    return mce::Color(0, 0, 0, 0);  // 非液体
}

// [洞穴方块专用颜色] 比通用 getBlockColor 更精细, 识别洞穴常见方块并赋予准确颜色
// 使洞穴地图色彩丰富: 矿石有对应颜色, 石头变种有区分, 深板岩为冷蓝灰
inline mce::Color GetCaveBlockColor(std::string const& rawName) noexcept {
    if (MapRenderState::blockColorMode == 1) {
        return BlockColorMap::GetVanillaBlockColor(
            rawName,
            mce::Color(0.50f, 0.70f, 0.30f, 1.0f),
            mce::Color(0.40f, 0.65f, 0.22f, 1.0f),
            kDefaultWaterTint,
            MapRenderState::biomeColorsInVanilla
        );
    }

    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    // [矿石] 必须在 deepslate 检查之前，确保 deepslate_diamond_ore / deepslate_gold_ore 等深层矿石不会被误判为深板岩石材
    if (name.find("diamond") != std::string::npos) return mce::Color(0.40f, 0.85f, 0.85f, 1.0f);
    if (name.find("gold") != std::string::npos) return mce::Color(0.95f, 0.82f, 0.25f, 1.0f);
    if (name.find("iron") != std::string::npos) return mce::Color(0.78f, 0.58f, 0.42f, 1.0f);
    if (name.find("coal") != std::string::npos) return mce::Color(0.18f, 0.18f, 0.18f, 1.0f);
    if (name.find("redstone") != std::string::npos) return mce::Color(0.78f, 0.15f, 0.15f, 1.0f);
    if (name.find("lapis") != std::string::npos) return mce::Color(0.20f, 0.42f, 0.88f, 1.0f);
    if (name.find("emerald") != std::string::npos) return mce::Color(0.20f, 0.80f, 0.38f, 1.0f);
    if (name.find("debris") != std::string::npos) return mce::Color(0.48f, 0.36f, 0.30f, 1.0f);
    if (name.find("netherite") != std::string::npos) return mce::Color(0.22f, 0.20f, 0.22f, 1.0f);
    if (name.find("quartz") != std::string::npos) return mce::Color(0.88f, 0.85f, 0.78f, 1.0f);

    // [铜与氧化阶段] 必须涵盖所有变体
    if (name.find("copper") != std::string::npos) {
        if (name.find("oxidized") != std::string::npos) return mce::Color(0.30f, 0.55f, 0.50f, 1.0f);
        if (name.find("weathered") != std::string::npos) return mce::Color(0.35f, 0.50f, 0.40f, 1.0f);
        if (name.find("exposed") != std::string::npos) return mce::Color(0.55f, 0.45f, 0.35f, 1.0f);
        return mce::Color(0.78f, 0.52f, 0.35f, 1.0f);
    }

    // [基岩与深板岩]
    if (name.find("bedrock") != std::string::npos) return mce::Color(0.12f, 0.12f, 0.14f, 1.0f);
    if (name.find("reinforced_deepslate") != std::string::npos) return mce::Color(0.28f, 0.28f, 0.30f, 1.0f);
    if (name.find("deepslate") != std::string::npos) return mce::Color(0.28f, 0.28f, 0.31f, 1.0f);

    // [试炼大厅方块 (1.21)]
    if (name.find("trial_spawner") != std::string::npos) return mce::Color(0.63f, 0.39f, 0.24f, 1.0f);
    if (name.find("crafter") != std::string::npos) return mce::Color(0.42f, 0.40f, 0.40f, 1.0f);
    if (name.find("vault") != std::string::npos) return mce::Color(0.38f, 0.36f, 0.38f, 1.0f);
    if (name.find("heavy_core") != std::string::npos) return mce::Color(0.24f, 0.25f, 0.29f, 1.0f);

    // 石头变种 (暖→冷渐变区分)
    if (name.find("granite") != std::string::npos) return mce::Color(0.58f, 0.42f, 0.35f, 1.0f);
    if (name.find("diorite") != std::string::npos) return mce::Color(0.72f, 0.70f, 0.68f, 1.0f);
    if (name.find("andesite") != std::string::npos) return mce::Color(0.50f, 0.48f, 0.46f, 1.0f);
    if (name.find("tuff") != std::string::npos) return mce::Color(0.45f, 0.43f, 0.41f, 1.0f);
    if (name.find("calcite") != std::string::npos) return mce::Color(0.82f, 0.80f, 0.78f, 1.0f);
    if (name.find("dripstone") != std::string::npos) return mce::Color(0.525f, 0.424f, 0.365f, 1.0f);
    if (name.find("amethyst") != std::string::npos) return mce::Color(0.62f, 0.42f, 0.82f, 1.0f);
    // [化石与骨块]
    if (name.find("bone_block") != std::string::npos) return mce::Color(0.88f, 0.86f, 0.78f, 1.0f);
    // [嗅探兽蛋]
    if (name.find("sniffer_egg") != std::string::npos) return mce::Color(0.78f, 0.38f, 0.22f, 1.0f);
    // [末地石与末石砖]
    if (name.find("end_stone") != std::string::npos || name.find("end_brick") != std::string::npos) return mce::Color(0.539f, 0.633f, 0.422f, 1.0f);
    // [紫珀块系列]
    if (name.find("purpur") != std::string::npos) return mce::Color(0.60f, 0.42f, 0.68f, 1.0f);

    // 泥土/沙石 (soul_sand/soul_soil 必须在 sand 之前, 因 "soul_sand" 含子串 "sand")
    if (name.find("soul_sand") != std::string::npos) return mce::Color(0.330f, 0.250f, 0.200f, 1.0f);
    if (name.find("soul_soil") != std::string::npos) return mce::Color(0.265f, 0.200f, 0.160f, 1.0f);
    if (name.find("dirt") != std::string::npos) return mce::Color(0.53f, 0.38f, 0.26f, 1.0f);
    // [红沙与红砂岩] 必须在通用 sand 之前
    if (name.find("red_sandstone") != std::string::npos) return mce::Color(0.711f, 0.384f, 0.123f, 1.0f);
    if (name.find("red_sand") != std::string::npos) return mce::Color(0.748f, 0.404f, 0.130f, 1.0f);
    if (name.find("sandstone") != std::string::npos) return mce::Color(0.890f, 0.840f, 0.680f, 1.0f);
    if (name.find("sand") != std::string::npos) return mce::Color(0.880f, 0.830f, 0.660f, 1.0f);
    if (name.find("gravel") != std::string::npos) return mce::Color(0.52f, 0.47f, 0.44f, 1.0f);
    if (name.find("clay") != std::string::npos && name.find("hardened_clay") == std::string::npos) return mce::Color(0.48f, 0.51f, 0.55f, 1.0f);

    // [陶瓦与硬化粘土] (恶地地下层及洞穴出露陶瓦层)
    if (name.find("terracotta") != std::string::npos || name.find("hardened_clay") != std::string::npos) {
        if (name.find("white") != std::string::npos) return mce::Color(0.822f, 0.698f, 0.633f, 1.0f);
        if (name.find("orange") != std::string::npos) return mce::Color(0.634f, 0.329f, 0.148f, 1.0f);
        if (name.find("magenta") != std::string::npos) return mce::Color(0.587f, 0.345f, 0.426f, 1.0f);
        if (name.find("light_blue") != std::string::npos) return mce::Color(0.445f, 0.426f, 0.541f, 1.0f);
        if (name.find("yellow") != std::string::npos) return mce::Color(0.730f, 0.522f, 0.139f, 1.0f);
        if (name.find("lime") != std::string::npos) return mce::Color(0.406f, 0.461f, 0.207f, 1.0f);
        if (name.find("pink") != std::string::npos) return mce::Color(0.635f, 0.307f, 0.309f, 1.0f);
        if (name.find("light_gray") != std::string::npos || name.find("silver") != std::string::npos) return mce::Color(0.530f, 0.420f, 0.382f, 1.0f);
        if (name.find("gray") != std::string::npos) return mce::Color(0.227f, 0.166f, 0.139f, 1.0f);
        if (name.find("cyan") != std::string::npos) return mce::Color(0.340f, 0.357f, 0.357f, 1.0f);
        if (name.find("purple") != std::string::npos) return mce::Color(0.464f, 0.276f, 0.338f, 1.0f);
        if (name.find("blue") != std::string::npos) return mce::Color(0.291f, 0.234f, 0.357f, 1.0f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.303f, 0.201f, 0.140f, 1.0f);
        if (name.find("green") != std::string::npos) return mce::Color(0.298f, 0.327f, 0.166f, 1.0f);
        if (name.find("red") != std::string::npos) return mce::Color(0.561f, 0.239f, 0.184f, 1.0f);
        if (name.find("black") != std::string::npos) return mce::Color(0.146f, 0.090f, 0.064f, 1.0f);
        return mce::Color(0.597f, 0.369f, 0.266f, 1.0f);
    }

    // [植被与繁茂洞穴]
    if (name.find("pale_moss") != std::string::npos || name.find("pale_hanging_moss") != std::string::npos) return mce::Color(0.42f, 0.44f, 0.41f, 1.0f);
    if (name.find("moss") != std::string::npos) return mce::Color(0.35f, 0.43f, 0.18f, 1.0f);
    if (name.find("flowering_azalea") != std::string::npos) return mce::Color(0.48f, 0.46f, 0.32f, 1.0f);
    if (name.find("azalea") != std::string::npos) return mce::Color(0.40f, 0.49f, 0.19f, 1.0f);
    if (name.find("dripleaf") != std::string::npos) return mce::Color(0.44f, 0.56f, 0.20f, 1.0f);
    if (name.find("cave_vines") != std::string::npos || (name.find("glow") != std::string::npos && name.find("berry") != std::string::npos)) {
        if (name.find("berries") != std::string::npos || name.find("berry") != std::string::npos) {
            return mce::Color(0.78f, 0.55f, 0.18f, 1.0f);
        }
        return mce::Color(0.35f, 0.40f, 0.15f, 1.0f);
    }
    if (name.find("spore") != std::string::npos) return mce::Color(0.81f, 0.38f, 0.62f, 1.0f);
    if (name.find("glow_lichen") != std::string::npos || name.find("lichen") != std::string::npos) return mce::Color(0.44f, 0.51f, 0.48f, 1.0f);
    if (name.find("dirt_with_roots") != std::string::npos || (name.find("rooted") != std::string::npos && name.find("dirt") != std::string::npos)) {
        return mce::Color(0.56f, 0.41f, 0.30f, 1.0f);
    }
    if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos) return mce::Color(0.421f, 0.309f, 0.159f, 1.0f);
    if (name.find("tall_dry_grass") != std::string::npos) return mce::Color(0.771f, 0.674f, 0.482f, 1.0f);
    if (name.find("dry_grass") != std::string::npos) return mce::Color(0.733f, 0.622f, 0.424f, 1.0f);
    if (name.find("cactus_flower") != std::string::npos) return mce::Color(0.822f, 0.473f, 0.531f, 1.0f);
    if (name.find("firefly_bush") != std::string::npos || name.find("firefly") != std::string::npos) return mce::Color(0.58f, 0.50f, 0.22f, 1.0f);
    if (name.find("leaf_litter") != std::string::npos || name.find("litter") != std::string::npos || name.find("fallen_leaf") != std::string::npos || name.find("fallen_leaves") != std::string::npos) return mce::Color(0.50f, 0.35f, 0.24f, 1.0f);

    // [硫黄洞穴系列方块 (1.26 硫黄与辰砂)]
    if (name.find("cinnabar") != std::string::npos) return mce::Color(0.68f, 0.22f, 0.20f, 1.0f);
    if (name.find("sulfur") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.22f, 1.0f);

    // [幽匿系列 (深暗之域)] 标志性深暗青翠/荧光青蓝，彻底消除黑色空洞误解
    if (name.find("sculk_catalyst") != std::string::npos) return mce::Color(0.20f, 0.32f, 0.35f, 1.0f);
    if (name.find("sculk_shrieker") != std::string::npos) return mce::Color(0.48f, 0.45f, 0.38f, 1.0f);
    if (name.find("calibrated_sculk_sensor") != std::string::npos) return mce::Color(0.35f, 0.30f, 0.50f, 1.0f);
    if (name.find("sculk_sensor") != std::string::npos) return mce::Color(0.07f, 0.38f, 0.42f, 1.0f);
    if (name.find("sculk_vein") != std::string::npos) return mce::Color(0.08f, 0.32f, 0.36f, 1.0f);
    if (name.find("sculk") != std::string::npos) return mce::Color(0.07f, 0.28f, 0.32f, 1.0f);

    // [冰系列]
    if (name.find("blue_ice") != std::string::npos) return mce::Color(0.45f, 0.65f, 0.95f, 1.0f);
    if (name.find("packed_ice") != std::string::npos) return mce::Color(0.55f, 0.70f, 0.92f, 1.0f);
    if (name.find("ice") != std::string::npos || name.find("frosted") != std::string::npos) return mce::Color(0.44f, 0.57f, 0.80f, 1.0f);

    // 普通石头
    if (name.find("stone") != std::string::npos) return mce::Color(0.42f, 0.42f, 0.44f, 1.0f);

    // 下界方块 (Nether blocks — 用于下界地图渲染)
    if (name.find("netherrack") != std::string::npos) return mce::Color(0.45f, 0.12f, 0.12f, 1.0f);
    if (name.find("nether_wart") != std::string::npos) return mce::Color(0.60f, 0.14f, 0.14f, 1.0f);
    if (name.find("shroomlight") != std::string::npos) return mce::Color(1.0f, 0.60f, 0.20f, 1.0f);
    if (name.find("glowstone") != std::string::npos) return mce::Color(1.0f, 0.85f, 0.30f, 1.0f);
    if (name.find("magma") != std::string::npos) return mce::Color(0.60f, 0.20f, 0.08f, 1.0f);
    if (name.find("nether_brick") != std::string::npos) return mce::Color(0.25f, 0.10f, 0.15f, 1.0f);
    if (name.find("crimson_nylium") != std::string::npos) return mce::Color(0.55f, 0.15f, 0.15f, 1.0f);
    if (name.find("warped_nylium") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);
    if (name.find("warped_wart") != std::string::npos) return mce::Color(0.20f, 0.50f, 0.42f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.25f, 0.22f, 0.22f, 1.0f);
    if (name.find("gilded") != std::string::npos) return mce::Color(0.45f, 0.38f, 0.22f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.20f, 0.18f, 0.20f, 1.0f);
    if (name.find("crying_obsidian") != std::string::npos) return mce::Color(0.25f, 0.12f, 0.35f, 1.0f);
    if (name.find("respawn_anchor") != std::string::npos) return mce::Color(0.32f, 0.18f, 0.38f, 1.0f);
    if (name.find("obsidian") != std::string::npos) return mce::Color(0.12f, 0.08f, 0.18f, 1.0f);

    // 废弃矿井/要塞/遗迹结构
    if (name.find("spawner") != std::string::npos) return mce::Color(0.22f, 0.26f, 0.30f, 1.0f);
    if (name.find("rail") != std::string::npos) return mce::Color(0.55f, 0.48f, 0.38f, 1.0f);
    if (name.find("web") != std::string::npos) return mce::Color(0.85f, 0.85f, 0.85f, 0.7f);
    if (name.find("chest") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    if (name.find("chain") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    if (name.find("planks") != std::string::npos || name.find("fence") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);

    // 默认: 中性灰
    return mce::Color(0.35f, 0.35f, 0.37f, 1.0f);
}

// ==========================================
// [安全传送增强·核心函数群] 解决"传送到地下/水中"问题
// 设计目标：99.9%+ 传送准确落在地表表面，无地下/水中传送
// 五道防线：①区块就绪检查 ②水面/液体检测 ③落脚点验证 ④稳定性确认 ⑤附近点回退
// ==========================================

// [防线①] 区块就绪检查：通过 hasChunksAt 验证目标区块已完全加载
// ignoreClientChunk = true: 忽略客户端空区块占位，仅当真实数据存在时返回 true
inline bool IsChunkReady(BlockSource& region, int x, int y, int z) noexcept {
    try {
        return region.hasChunksAt(BlockPos(x, y, z), 0, true);
    } catch (...) {
        return false;
    }
}

// [辅助] 判断方块名是否为岩浆（熔岩流体或岩浆方块）
inline bool IsLavaBlockName(std::string const& name) noexcept {
    return name.find("lava") != std::string::npos;
}

// [安全落脚点判定] 判断方块是否可作为脚下的有效支撑
// 规则1 (主世界/下界): 除了岩浆，其它任何方块均可作为安全落脚点（包括水、固体、植物等）
// 规则2 (末地): 除了虚空(空气)，任何位置/方块均作为安全落脚点
inline bool IsValidGroundName(std::string const& name, int dimId = 0) noexcept {
    if (name.empty()) return false;
    if (IsAirLikeName(name)) return false;
    if (dimId == 2) {
        return true; // 末地：非空气/非虚空即可站立
    }
    if (IsLavaBlockName(name)) return false;
    if (name.find("fire") != std::string::npos) return false;
    if (name.find("campfire") != std::string::npos) return false;
    if (name.find("magma") != std::string::npos) return false;
    if (name.find("powder_snow") != std::string::npos) return false;
    if (name.find("cactus") != std::string::npos) return false;
    return true; // 主世界与下界：非危险方块即可站立
}

// [无法承重的非实体/装饰方块判定]
// 仅判定确实无法作为脚下站立支撑的虚体/细长植物/附着物
// 注意：water(水体)、leaves(树叶)、snow(雪层)、carpet(地毯)、lily(荷叶)、sculk_vein(幽匿脉络)等均允许站立
inline bool IsNonSupportingPassable(std::string const& name) noexcept {
    if (name.empty()) return true;
    if (IsAirLikeName(name)) return true;
    if (name.find("torch") != std::string::npos) return true;
    if (name.find("short_grass") != std::string::npos || 
        name.find("tallgrass") != std::string::npos || 
        name.find("tall_grass") != std::string::npos ||
        (name.find("grass") != std::string::npos && name.find("grass_block") == std::string::npos && name.find("path") == std::string::npos && name.find("seagrass") == std::string::npos)) {
        return true;
    }
    if (name.find("flower") != std::string::npos && name.find("chorus_flower") == std::string::npos) return true;
    if (name.find("tulip") != std::string::npos || name.find("rose") != std::string::npos || name.find("dandelion") != std::string::npos ||
        name.find("orchid") != std::string::npos || name.find("allium") != std::string::npos || name.find("bluet") != std::string::npos ||
        name.find("poppy") != std::string::npos || name.find("daisy") != std::string::npos || name.find("cornflower") != std::string::npos ||
        name.find("sunflower") != std::string::npos || name.find("lilac") != std::string::npos || name.find("peony") != std::string::npos) {
        return true;
    }
    if (name.find("fern") != std::string::npos) return true;
    if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos) return true;
    if (name.find("sapling") != std::string::npos) return true;
    if (name.find("rail") != std::string::npos) return true;
    if (name.find("button") != std::string::npos || name.find("lever") != std::string::npos || name.find("tripwire") != std::string::npos) return true;
    if (name.find("pressure_plate") != std::string::npos) return true;
    if (name.find("sign") != std::string::npos || name.find("banner") != std::string::npos) return true;
    if (name.find("firefly") != std::string::npos) return true;
    if (name.find("crop") != std::string::npos || name.find("wheat") != std::string::npos || name.find("carrots") != std::string::npos ||
        name.find("potatoes") != std::string::npos || name.find("beetroot") != std::string::npos) return true;
    return false;
}

// [安全站立空间判定] 判断玩家脚部/头部所处空间是否通畅且安全（非窒息、非岩浆）
// 严禁将草方块(grass_block)、巨型蘑菇方块(mushroom_block)等实体方块判定为可站立空间
inline bool IsBreathableSpaceName(std::string const& name, int dimId = 0) noexcept {
    if (name.empty()) return false;
    // 空间内绝不能是岩浆、火焰、营火或有害植物
    if (IsLavaBlockName(name)) return false;
    if (name.find("fire") != std::string::npos) return false;
    if (name.find("campfire") != std::string::npos) return false;
    if (name.find("wither_rose") != std::string::npos) return false;
    if (name.find("sweet_berry") != std::string::npos) return false;
    if (name.find("powder_snow") != std::string::npos) return false;
    if (name.find("cactus") != std::string::npos) return false;
    // 空气类方块
    if (IsAirLikeName(name)) return true;
    // 水体（允许在水中/水面站立）
    if (name.find("water") != std::string::npos) return true;
    // 各种无窒息碰撞/可穿透的覆盖物（植物、火把、告示牌、红石、梯子等）
    if (name.find("torch") != std::string::npos) return true;
    if (name.find("short_grass") != std::string::npos || 
        name.find("tallgrass") != std::string::npos || 
        name.find("tall_grass") != std::string::npos ||
        (name.find("grass") != std::string::npos && name.find("grass_block") == std::string::npos && name.find("path") == std::string::npos)) {
        return true;
    }
    if (name.find("fern") != std::string::npos) return true;
    if (name.find("flower") != std::string::npos && name.find("chorus_flower") == std::string::npos) return true;
    if (name.find("tulip") != std::string::npos || name.find("rose") != std::string::npos || name.find("dandelion") != std::string::npos ||
        name.find("orchid") != std::string::npos || name.find("allium") != std::string::npos || name.find("bluet") != std::string::npos ||
        name.find("poppy") != std::string::npos || name.find("daisy") != std::string::npos || name.find("cornflower") != std::string::npos ||
        name.find("sunflower") != std::string::npos || name.find("lilac") != std::string::npos || name.find("peony") != std::string::npos) {
        return true;
    }
    if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos || name.find("firefly") != std::string::npos || name.find("litter") != std::string::npos) return true;
    if (name.find("vine") != std::string::npos) return true;
    if (name.find("sapling") != std::string::npos) return true;
    if (name.find("mushroom") != std::string::npos && name.find("mushroom_block") == std::string::npos && name.find("mushroom_stem") == std::string::npos) return true;
    if (name.find("fungus") != std::string::npos) return true;
    if (name.find("seagrass") != std::string::npos) return true;
    if (name.find("kelp") != std::string::npos) return true;
    if (name.find("rail") != std::string::npos) return true;
    if (name.find("ladder") != std::string::npos) return true;
    if (name.find("sign") != std::string::npos) return true;
    if (name.find("banner") != std::string::npos) return true;
    if (name.find("button") != std::string::npos) return true;
    if (name.find("lever") != std::string::npos) return true;
    if (name.find("pressure_plate") != std::string::npos) return true;
    if (name.find("tripwire") != std::string::npos) return true;
    if (name.find("carpet") != std::string::npos) return true;
    if (name.find("web") != std::string::npos) return true;
    if (name.find("lily") != std::string::npos) return true;
    if (name.find("crop") != std::string::npos || name.find("wheat") != std::string::npos || name.find("carrots") != std::string::npos ||
        name.find("potatoes") != std::string::npos || name.find("beetroot") != std::string::npos) return true;
    if (name.find("snow") != std::string::npos && name.find("snow_block") == std::string::npos) return true;
    if (name.find("sugar_cane") != std::string::npos || name.find("reeds") != std::string::npos) return true;
    if (name.find("spore_blossom") != std::string::npos || 
        (name.find("roots") != std::string::npos && name.find("mangrove") == std::string::npos) || 
        name.find("sprouts") != std::string::npos) return true;
    return false;
}

// [最终防线] 检查目标点是否 100% 安全（脚部与头部两格通畅无窒息，下方为合法支撑）
inline bool IsTeleportSpotSafe(BlockSource& region, int blockX, int blockY, int blockZ, int dimId = 0) noexcept {
    try {
        std::string feetName, headName, groundName;
        if (!SafeGetBlockName(region, blockX, blockY, blockZ, feetName) || feetName.empty()) return false;
        if (!SafeGetBlockName(region, blockX, blockY + 1, blockZ, headName) || headName.empty()) return false;
        if (!SafeGetBlockName(region, blockX, blockY - 1, blockZ, groundName) || groundName.empty()) return false;

        if (!IsBreathableSpaceName(feetName, dimId)) return false;
        if (!IsBreathableSpaceName(headName, dimId)) return false;
        if (!IsValidGroundName(groundName, dimId)) return false;
        if (IsNonSupportingPassable(groundName)) return false;

        // [水域安全强化] 传送到水域时必须落在水面，头部不能浸没在水中（水底），确保呼吸与安全
        if (dimId == 0) {
            if (feetName.find("water") != std::string::npos || groundName.find("water") != std::string::npos) {
                if (headName.find("water") != std::string::npos) {
                    return false; // 头部在水里 = 水底浸没，拒绝！
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

// [邻域危险检测] 主世界/下界检测周围是否有岩浆；末地无危险方块检测
inline bool HasAdjacentHazard(BlockSource& region, int x, int y, int z, int radius = 1, int dimId = 0) noexcept {
    if (dimId == 2) return false;
    try {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0) continue;
                std::string name;
                if (SafeGetBlockName(region, x + dx, y, z + dz, name)) {
                    if (IsLavaBlockName(name)) return true;
                }
            }
        }
        return false;
    } catch (...) {
        return true;
    }
}

// [地表露天验证] 验证目标落脚点之上无实心岩石/深板岩/地底天花板阻挡
// 杜绝两阶段探测传送到地下洞穴天花板下方或未加载完整的下层岩石中窒息
inline bool HasOpenSkyAbove(BlockSource& region, int x, short standY, int z, int dimId = 0) noexcept {
    if (dimId == 1) return true; // 下界为封闭洞穴结构，由专用洞穴净空逻辑检测
    try {
        short surfY = SafeGetSurfaceY(region, x, z);
        // 1. 如果 standY 远低于 surfY (如落差超过 3 格)，说明在洞穴或天花板下方，绝对不是露天！
        if (surfY > -64 && surfY < 319 && standY < surfY - 3) {
            return false;
        }

        // 2. 主世界海平面保护：如果 standY < 62 且不是水体，拒绝露天判定（防止未加载完高层 subchunk 的地底假象）
        if (dimId == 0 && standY < 62) {
            std::string ground;
            if (!SafeGetBlockName(region, x, standY - 1, z, ground) || ground.find("water") == std::string::npos) {
                return false;
            }
        }

        // 3. 向上真实露天扫描：从 standY + 2 扫描到 standY + 32 (且至少达到 Y=128)
        short scanLimit = (dimId == 2) ? 256 : 319;
        short maxCheck = std::min((short)scanLimit, (short)std::max((short)128, (short)(standY + 32)));
        for (short y = standY + 2; y <= maxCheck; ++y) {
            std::string name;
            if (!SafeGetBlockName(region, x, y, z, name)) {
                // 读取失败：如果在 128 以下读取失败，说明 subchunk 尚未下发完成，绝不能当作露天空气！
                if (y <= 128) return false;
                // 128 以上若无 subchunk，且 standY 已高于海平面，视为高空虚空
                if (standY >= 62) continue;
                return false;
            }
            if (name.empty()) continue;
            if (IsAirLikeName(name)) continue;
            // 允许树木枝叶、树冠、雪层、藤蔓等自然地表覆盖物
            if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos) continue;
            if (name.find("wood") != std::string::npos || name.find("log") != std::string::npos) continue;
            if (name.find("vine") != std::string::npos || name.find("lichen") != std::string::npos) continue;
            if (name.find("snow") != std::string::npos && name.find("snow_block") == std::string::npos) continue;
            if (IsBreathableSpaceName(name, dimId)) continue;

            // 遇到任何其他非透气实体方块（岩石/深板岩/泥土/矿石/砂岩等天花板）→ 绝对不是露天！
            return false;
        }

        // 4. 高空抽样检查（Y=160, 220, 280），防止高空悬浮实心结构或未下发的巨型山体
        static const short kHighSamples[] = { 160, 220, 280 };
        for (short hy : kHighSamples) {
            if (hy > standY + 2 && hy <= scanLimit) {
                std::string hName;
                if (SafeGetBlockName(region, x, hy, z, hName) && !hName.empty()) {
                    if (!IsAirLikeName(hName) && !IsBreathableSpaceName(hName, dimId) &&
                        hName.find("leaf") == std::string::npos && hName.find("leaves") == std::string::npos &&
                        hName.find("vine") == std::string::npos && hName.find("snow") == std::string::npos) {
                        return false; // 高空存在实心方块，不是真正露天
                    }
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

// [下界高品质安全落脚点评分查找] 在单个列内寻找最佳下界落脚点
// 规则：
// 1. 严格限制脚部Y在 [33, 100] 区间（避开Y<=31岩浆海与Y>=105天花板基岩缝隙）
// 2. 支撑方块为非岩浆、非空气、非火焰、非岩浆块的实心支撑
// 3. 脚部(y)、头部(y+1)以及头部上方(y+2)全部通畅，至少保证 3 格垂直净空，杜绝起跳卡头窒息
// 4. 水平四周检查：脚部四周至少有 2 个方向通畅，且 3x3 邻域至少 3 方向通畅，杜绝 1x1 嵌岩缝隙导致窒息
// 5. 按开阔度与净空评分，优先选择最开阔、最安全的洞穴平坦地面
inline short FindBestNetherSpawnInColumn(BlockSource& region, int x, int z, int preferredY, int& outScore, bool strict = true) noexcept {
    outScore = -1;
    short bestY = -32000;

    constexpr int kNetherMinY = 33;  // 脚部Y=33, 支撑地面Y=32 (高于岩浆海31)
    constexpr int kNetherMaxY = 100; // 脚部Y<=100 (避开天花板裂隙)

    for (int y = kNetherMaxY; y >= kNetherMinY; --y) {
        // 1. 支撑方块 (y - 1) 校验
        std::string groundName;
        if (!SafeGetBlockName(region, x, y - 1, z, groundName) || groundName.empty()) continue;
        if (!IsValidGroundName(groundName, 1)) continue;
        if (IsNonSupportingPassable(groundName)) continue; // 支撑方块不能是空气/植物/透光虚体
        if (groundName.find("magma") != std::string::npos) continue;

        // 2. 脚部 (y) 与 头部 (y + 1) 校验
        std::string feetName, headName;
        if (!SafeGetBlockName(region, x, y, z, feetName) || !IsBreathableSpaceName(feetName, 1)) continue;
        if (!SafeGetBlockName(region, x, y + 1, z, headName) || !IsBreathableSpaceName(headName, 1)) continue;

        // 3. 头部上方空间 (y + 2, y + 3) 校验：至少 4 格连续垂直净空，杜绝起跳卡头窒息
        std::string aboveName, above2Name;
        if (!SafeGetBlockName(region, x, y + 2, z, aboveName) || !IsBreathableSpaceName(aboveName, 1)) continue;
        if (!SafeGetBlockName(region, x, y + 3, z, above2Name) || !IsBreathableSpaceName(above2Name, 1)) continue;

        // 4. 水平四周开阔度校验 (杜绝 1x1 夹缝穿模窒息)
        static const struct { int dx, dz; } kAdj[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        int openSides = 0;
        for (const auto& adj : kAdj) {
            std::string adjFeet, adjHead;
            if (SafeGetBlockName(region, x + adj.dx, y, z + adj.dz, adjFeet) && IsBreathableSpaceName(adjFeet, 1) &&
                SafeGetBlockName(region, x + adj.dx, y + 1, z + adj.dz, adjHead) && IsBreathableSpaceName(adjHead, 1)) {
                openSides++;
            }
        }
        if (strict && openSides < 2) continue; // 严格模式下必须至少两面开阔
        if (!strict && openSides < 1) continue;

        // 3x3 邻域开阔度 (8 邻居)
        int openSurr = 0;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dz == 0) continue;
                std::string sFeet;
                if (SafeGetBlockName(region, x + dx, y, z + dz, sFeet) && IsBreathableSpaceName(sFeet, 1)) {
                    openSurr++;
                }
            }
        }
        if (strict && openSurr < 3) continue;

        // 5. 危险物检查 (避开相邻岩浆或火)
        if (HasAdjacentHazard(region, x, y, z, 1, 1)) continue;

        // 6. 测量实际净空高度
        int headroom = 3;
        for (int h = y + 3; h <= std::min(y + 16, 120); ++h) {
            std::string hName;
            if (SafeGetBlockName(region, x, h, z, hName) && IsBreathableSpaceName(hName, 1)) {
                headroom++;
            } else {
                break;
            }
        }

        // 7. 综合评分：开阔度与净空加权，优先选择大溶洞开阔地面
        int score = headroom * 12 + openSides * 25 + openSurr * 10;
        int targetRef = (preferredY >= 33 && preferredY <= 100) ? preferredY : 64;
        score -= std::abs(y - targetRef) * 2;

        if (score > outScore) {
            outScore = score;
            bestY = (short)y;
        }
    }

    return bestY;
}

// [下界大范围开阔安全点搜索] 遍历一片区域，寻找最开阔安全的洞穴地面 (x, z 可自由变化)
inline bool FindBestSafeSpawnNether(BlockSource& region, int& x, int& z, short& outY, int preferredY = 64, int maxRadius = 24) noexcept {
    int bestScore = -1;
    short bestY = -32000;
    int bestX = x, bestZ = z;

    // 第一轮：严格模式 (openSides >= 2, openSurr >= 3, 搜索整片区域，轻微距离衰减)
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                int colScore = -1;
                short colY = FindBestNetherSpawnInColumn(region, testX, testZ, preferredY, colScore, true);
                if (colY > -64) {
                    colScore -= (r / 2);  // 极轻微距离惩罚，鼓励选择更远的大溶洞开阔平地
                    if (colScore > bestScore) {
                        bestScore = colScore;
                        bestY = colY;
                        bestX = testX;
                        bestZ = testZ;
                    }
                }
            }
        }
        // 若在 r >= 8 之后已找到综合得分 >= 120 的极高品质开阔大溶洞，可提前采纳
        if (bestScore >= 120 && r >= 8) {
            x = bestX;
            z = bestZ;
            outY = bestY;
            return true;
        }
    }

    if (bestY > -64 && bestScore > 0) {
        x = bestX;
        z = bestZ;
        outY = bestY;
        return true;
    }

    // 第二轮：宽松模式 (openSides >= 1, 扩大至 maxRadius + 8)
    int fallbackLimit = std::min(32, maxRadius + 8);
    for (int r = 0; r <= fallbackLimit; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                int colScore = -1;
                short colY = FindBestNetherSpawnInColumn(region, testX, testZ, preferredY, colScore, false);
                if (colY > -64) {
                    colScore -= r;
                    if (colScore > bestScore) {
                        bestScore = colScore;
                        bestY = colY;
                        bestX = testX;
                        bestZ = testZ;
                    }
                }
            }
        }
    }

    if (bestY > -64 && bestScore > -50) {
        x = bestX;
        z = bestZ;
        outY = bestY;
        return true;
    }
    return false;
}

// [防线②③·地表安全落脚点查找] 给定 (x,z)，返回玩家可安全站立的实际 Y 坐标
// [洞穴/下界传送·安全落脚点查找] 给定 (x,z) 和参考 Y, 在 [minY, maxY] 范围内 refY 附近搜索可安全站立的 Y
// 用于洞穴/下界传送: 从参考高度向上下扫描, 找到通畅且下方为有效落脚点的位置
// 主世界/下界：除岩浆外任何方块均可落脚；末地：除虚空外任何位置均可落脚
inline short SafeFindSafeSpawnYNearY(BlockSource& region, int x, int z, int refY, int minY = -64, int maxY = 319, int dimId = 0) noexcept {
    if (refY < minY) refY = minY;
    if (refY > maxY) refY = maxY;
    try {
        if (dimId == 1) {
            int score = -1;
            int prefY = (refY >= 33 && refY <= 100) ? refY : 64;
            short bestY = FindBestNetherSpawnInColumn(region, x, z, prefY, score);
            if (bestY > -64) return bestY;
            return -32000;
        }

        // 常规/已保存坐标区域: 优先在 refY 附近查找
        // 先从 refY 向上扫描 (优先传送到同高度或略高), 上限 maxY
        for (int y = refY; y <= maxY; ++y) {
            std::string feetName, headName, belowName;
            if (!SafeGetBlockName(region, x, y, z, feetName) || feetName.empty()) continue;
            if (IsBreathableSpaceName(feetName, dimId)) {
                if (y + 1 > maxY) break;
                if (!SafeGetBlockName(region, x, y + 1, z, headName) || headName.empty()) continue;
                if (IsBreathableSpaceName(headName, dimId)) {
                    if (y - 1 < minY) continue;
                    if (!SafeGetBlockName(region, x, y - 1, z, belowName) || belowName.empty()) continue;
                    if (IsValidGroundName(belowName, dimId)) {
                        // [水域安全强化] 若下方或脚部是水，头部必须不能浸在水中（水面判定）
                        if (dimId == 0 && (feetName.find("water") != std::string::npos || belowName.find("water") != std::string::npos)) {
                            if (headName.find("water") != std::string::npos) continue;
                        }
                        return (short)y;
                    }
                }
            }
        }
        // 再从 refY-1 向下扫描, 下限 minY
        for (int y = refY - 1; y >= minY; --y) {
            std::string feetName, headName, belowName;
            if (!SafeGetBlockName(region, x, y, z, feetName) || feetName.empty()) continue;
            if (IsBreathableSpaceName(feetName, dimId)) {
                if (y + 1 > maxY) continue;
                if (!SafeGetBlockName(region, x, y + 1, z, headName) || headName.empty()) continue;
                if (IsBreathableSpaceName(headName, dimId)) {
                    if (y - 1 < minY) continue;
                    if (!SafeGetBlockName(region, x, y - 1, z, belowName) || belowName.empty()) continue;
                    if (IsValidGroundName(belowName, dimId)) {
                        // [水域安全强化] 若下方或脚部是水，头部必须不能浸在水中（水面判定）
                        if (dimId == 0 && (feetName.find("water") != std::string::npos || belowName.find("water") != std::string::npos)) {
                            if (headName.find("water") != std::string::npos) continue;
                        }
                        return (short)y;
                    }
                }
            }
        }
        return -32000;
    } catch (...) {
        return -32000;
    }
}

// [洞穴/下界传送·附近安全点搜索] 螺旋搜索附近列, 在 refY 附近 [minY,maxY] 范围找安全落脚点
inline bool FindNearestSafeSpawnNearY(BlockSource& region, int& x, int& z, short& outY, int refY, int maxRadius = 16, int minY = -64, int maxY = 319, int dimId = 0) noexcept {
    if (dimId == 1) {
        int prefY = (refY >= 33 && refY <= 100) ? refY : 64;
        return FindBestSafeSpawnNether(region, x, z, outY, prefY, maxRadius);
    }
    // 第一轮: 严格 (邻居无岩浆)
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    if (!HasAdjacentHazard(region, testX, testY, testZ, 1, dimId)) {
                        x = testX;
                        z = testZ;
                        outY = testY;
                        return true;
                    }
                }
            }
        }
    }
    // 第二轮: 宽松 (仅本列安全)
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }
    // 第三轮: 超大范围宽松 (±17~±32)
    for (int r = maxRadius + 1; r <= 32; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }
    return false;
}

// [防线②③·地表安全落脚点查找] 给定 (x,z)，返回玩家可安全站立的实际 Y 坐标
// 规则：
// 1. 主世界与下界：除岩浆外任何方块均为安全落脚点（包括水体、草地、冰面等）
// 2. 末地：除虚空外任何位置均为安全落脚点
// 3. 下界：在 [33, 100] 洞穴区间查找
// 4. 主世界/末地：通过引擎 SafeGetSurfaceY 定位真实地表站立高度，快速验证落脚点及露天净空
// 返回值：安全 Y（玩家脚部位置），或 -32000 表示该列无安全落脚点
inline short SafeFindSafeSpawnY(BlockSource& region, int x, int z, int dimId = 0) noexcept {
    try {
        if (dimId == 1) {
            // 下界传送:
            // 1. 若大地图已保存具体坐标点的高度 (已探索区域), 优先在保存高度附近查找
            int16_t cachedNetherY = MapCacheManager::GetCachedSurfaceHeight(x, z, true);
            int prefY = 64;
            if (cachedNetherY != MapCacheManager::HEIGHT_UNKNOWN && cachedNetherY >= 33 && cachedNetherY <= 100) {
                prefY = (int)cachedNetherY + 1;
            }
            int outScore = -1;
            short bestY = FindBestNetherSpawnInColumn(region, x, z, prefY, outScore, true);
            if (bestY >= 33 && bestY <= 100) return bestY;
            bestY = FindBestNetherSpawnInColumn(region, x, z, prefY, outScore, false);
            if (bestY >= 33 && bestY <= 100) return bestY;
            return -32000;
        }

        // 主世界 (dimId == 0) 与 末地 (dimId == 2):
        // 使用引擎内置 SafeGetSurfaceY (已包裹 __try/__except SEH 防止 NULL subchunk AV)
        short surfY = SafeGetSurfaceY(region, x, z);
        if (surfY <= -64 || surfY >= 319 || surfY == -32000) {
            return -32000;
        }

        // 主世界海平面防窒息安全线：若检测到的 surfY < 62，检查地表是否为水体。
        // 若为岩石/泥土等干地，极大概率是地表高层 subchunk (Y=64+) 尚未下发完成的假象，坚决拒绝采纳！
        if (dimId == 0 && surfY < 62) {
            std::string surfGround;
            if (SafeGetBlockName(region, x, surfY - 1, z, surfGround)) {
                if (surfGround.find("water") == std::string::npos) {
                    return -32000;
                }
            } else {
                return -32000;
            }
        }

        int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(x, z);

        // surfY 是引擎最高实体方块上方的格点 (站立高度)
        // 在 [surfY + 2, surfY - 4] 范围内精准定位真实支撑地面与站立空间
        short startY = std::min((short)318, (short)(surfY + 2));
        short endY = std::max((short)-60, (short)(surfY - 4));
        for (short standY = startY; standY >= endY; --standY) {
            std::string groundName;
            if (!SafeGetBlockName(region, x, standY - 1, z, groundName) || groundName.empty()) continue;
            if (!IsValidGroundName(groundName, dimId)) continue;
            if (IsNonSupportingPassable(groundName)) continue; // 支撑物不能是空气或无法站立的植物/小部件
            if (dimId == 0 && standY < 62 && groundName.find("water") == std::string::npos) continue; // 主世界干地 standY 必须 >= 62

            std::string feetName, headName, aboveHeadName;
            if (!SafeGetBlockName(region, x, standY, z, feetName) || feetName.empty()) continue;
            if (!IsBreathableSpaceName(feetName, dimId)) continue;

            if (!SafeGetBlockName(region, x, standY + 1, z, headName) || headName.empty()) continue;
            if (!IsBreathableSpaceName(headName, dimId)) continue;

            // 头部上方空间 (standY + 2) 校验：允许树叶/藤蔓等自然树冠；若为实体方块（如岩石洞穴天花板），跳过
            if (SafeGetBlockName(region, x, standY + 2, z, aboveHeadName) && !aboveHeadName.empty()) {
                if (!IsBreathableSpaceName(aboveHeadName, dimId) &&
                    aboveHeadName.find("leaf") == std::string::npos &&
                    aboveHeadName.find("leaves") == std::string::npos &&
                    aboveHeadName.find("vine") == std::string::npos) {
                    continue;
                }
            }

            // 水域安全：脚部或下方是水时，头部必须绝对处于空气中（不能浸没在水底），保证呼吸与水面安全
            if (dimId == 0 && (feetName.find("water") != std::string::npos || groundName.find("water") != std::string::npos)) {
                if (headName.find("water") != std::string::npos) continue;
            }

            // 检查历史缓存落差：如果历史缓存存在且落差过大 (>15)，说明可能正在向下加载，继续等待
            if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -60) {
                if (standY < cachedY - 15) {
                    return -32000;
                }
            }

            // 周围危险检测
            if (HasAdjacentHazard(region, x, standY, z, 1, dimId)) continue;

            // 露天与天花板阻挡校验
            if (!HasOpenSkyAbove(region, x, standY, z, dimId)) continue;

            return standY;
        }

        return -32000;
    } catch (...) {
        return -32000;
    }
}

// [防线⑤·附近安全点搜索] 目标列无安全落脚点时，螺旋搜索附近列
// 主世界/下界：避开岩浆；末地：寻找最近的非虚空空岛
inline bool FindNearestSafeSpawn(BlockSource& region, int& x, int& z, short& outY, int maxRadius = 16, int dimId = 0) noexcept {
    if (dimId == 1) {
        return FindBestSafeSpawnNether(region, x, z, outY, 64, maxRadius);
    }
    // 第一轮：严格搜索（主世界/下界 3x3 邻居无岩浆，且必须露天）
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId) && HasOpenSkyAbove(region, testX, testY, testZ, dimId)) {
                    if (!HasAdjacentHazard(region, testX, testY, testZ, 1, dimId)) {
                        x = testX;
                        z = testZ;
                        outY = testY;
                        return true;
                    }
                }
            }
        }
    }

    // 第二轮：宽松搜索（仅本列安全，且必须露天）
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId) && HasOpenSkyAbove(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }

    // 第三轮：超大范围宽松搜索（半径 ±(maxRadius+1)~±32，且必须露天）
    for (int r = maxRadius + 1; r <= 32; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId) && HasOpenSkyAbove(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }
    return false;
}

// 线程安全/SEH保护的游戏刻时间查询函数 (仅在主线程调用)
inline int SafeQueryGameTime(ClientInstance* ci) noexcept {
    if (!ci) return -1;
    __try {
        if (ci->hasLevel()) {
            auto* cl = ci->getLevel();
            if (cl) {
                int t = cl->getTime();
                if (t >= 0) return t;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    __try {
        auto* region = ci->getRegion();
        if (region) {
            return region->getLevel().getTime();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    return -1;
}

LL_TYPE_INSTANCE_HOOK(
    ClientInstanceUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool a1
) {
    MapRenderState::lastFrameTotalCalls = MapRenderState::frameCallCount.load();
    if (MapRenderState::lastFrameTotalCalls < 1) MapRenderState::lastFrameTotalCalls = 1;
    MapRenderState::frameCallCount.store(0);

    bool result = origin(a1);

    // [关闭期安全] disable() 已启动资源释放流程（D3D/ImGui/缓存均可能已失效）。
    // 此时进入复杂玩家状态/区块/缓存逻辑会访问已释放对象导致 0xC0000005 退出崩溃。
    // 直接放行原函数结果，让 update 链路继续往下走（游戏自身的退出流程）。
    if (MapRenderState::g_isShuttingDown.load()) return result;

    g_clientInstance = this;

    auto* player = this->getLocalPlayer();
    if (player && this->isWorldActive()) {
        try {
            g_localPlayerUuid = static_cast<std::string>(player->getUuid());
            static int localSkinDelay = 29;
            if (++localSkinDelay >= 30) {
                localSkinDelay = 0;
                ExtractPlayerSkinHead(player, g_localPlayerUuid);
            }
        } catch (...) {}
        Vec3 pos;
        try {
            pos = player->getFeetPos();

            if (g_prevPhysicsPos.x == 0.0f && g_prevPhysicsPos.z == 0.0f) {
                g_prevPhysicsPos = pos;
            } else {
                g_prevPhysicsPos = g_currPhysicsPos;
            }
            g_currPhysicsPos  = pos;
            g_lastPhysicsTime = std::chrono::steady_clock::now();

            g_playerBlockX = (int)std::floor(pos.x);
            g_playerBlockZ = (int)std::floor(pos.z);
            g_playerX = pos.x; g_playerY = pos.y; g_playerZ = pos.z;
            g_playerYaw = player->getRotation().y;
            g_hasPlayer   = true;
            g_localPlayer = player;

            if (MapRenderState::showFootsteps) {
                static auto s_startEpoch = std::chrono::steady_clock::now();
                float curTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - s_startEpoch).count();
                MapRenderState::AddFootstep(pos.x, pos.y, pos.z, MapRenderState::currentDimensionId, curTime);
            }

            int gameTicks = SafeQueryGameTime(this);
            if (gameTicks >= 0) {
                MapRenderState::g_gameTimeTicks.store(gameTicks);
            }

            // [HUD 实时光照等级] 当开启光照显示时，安全采样脚部方块光照
            if (MapRenderState::infoShowLight) {
                try {
                    BlockSource* region = this->getRegion();
                    if (region) {
                        BlockPos pPos((int)std::floor(pos.x), (int)std::floor(pos.y), (int)std::floor(pos.z));
                        BrightnessPair bp = region->getBrightnessPair(pPos);
                        unsigned char sl = *(reinterpret_cast<const unsigned char*>(&bp.sky));
                        unsigned char bl = *(reinterpret_cast<const unsigned char*>(&bp.block));
                        MapRenderState::g_skyLight.store((int)sl);
                        MapRenderState::g_blockLight.store((int)bl);
                    }
                } catch (...) {}
            }

            // 靠近 6 个方块内自动清除到达过的死亡地点 (5秒免删安全期)
            if (MapRenderState::autoRemoveDeathpoints) {
                long long nowSec = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                std::string toRemoveId;
                {
                    std::lock_guard<std::mutex> lock(DeathPointManager::g_deathMutex);
                    for (const auto& dp : DeathPointManager::g_deathPoints) {
                        if (dp.dimensionId != MapRenderState::currentDimensionId) continue;
                        if (nowSec - dp.timestamp < 5) continue; // 死亡 5 秒内免删缓冲期
                        float dx = ((float)dp.x + 0.5f) - pos.x;
                        float dy = (float)dp.y - pos.y;
                        float dz = ((float)dp.z + 0.5f) - pos.z;
                        if (dx * dx + dy * dy + dz * dz <= 36.0f) { // 6方块以内
                            toRemoveId = dp.id;
                            break;
                        }
                    }
                }
                if (!toRemoveId.empty()) {
                    DeathPointManager::RemoveDeathPoint(toRemoveId);
                }
            }
        } catch (...) {
            // player 指针可能失效(维度切换/区块卸载/退出过程), 跳过本帧
            return result;
        }

        // ==========================================
        // [地表直达传送·增强版] 三级地表Y识别 + 两阶段探测 + 五道安全防线
        // ==========================================
        if (MapRenderState::triggerTeleport.load()) {
            float targetX = MapRenderState::tpTargetX;
            float targetY = MapRenderState::tpTargetY;
            float targetZ = MapRenderState::tpTargetZ;
            int targetDim = MapRenderState::tpTargetDim;
            int currentDim = MapRenderState::currentDimensionId;
            if (targetDim < 0) {
                targetDim = currentDim;
            }

            // [跨维度传送] 若目标维度与当前维度不同，使用 Bedrock 规范指令 /execute in <dim> run tp @s 执行跨界传送
            if (targetDim != currentDim) {
                // 立即丢弃当前正在进行的异界扫描与待写缓冲，彻底防止跨维度写入污染
                AbortActiveScanAndPendingWrites();
                g_currentScanX = -99999;
                g_currentScanZ = -99999;
                g_scanDimension = -999;

                std::string dimName;
                if (targetDim == 1) dimName = "nether";
                else if (targetDim == 2) dimName = "the_end";
                else dimName = "overworld";

                float finalY = targetY;
                bool needNetherCrossProbe = false;
                bool needOverworldCrossProbe = false;
                if (targetDim == 1) {
                    // 跨维度传送至下界：若 finalY 在基岩顶 (>=125) 或岩浆海 (<33) 或未指定 (<-500/==320)
                    // 严禁直接传送进实心地狱岩中窒息！
                    // 先传送到 Y=128 基岩顶层绝对安全暂存区触发下界区块加载，并启动下界两阶段探测
                    if (finalY >= 125.0f || finalY < 33.0f || finalY < -500.0f || std::abs(finalY - 320.0f) < 0.1f) {
                        finalY = 128.0f;
                        needNetherCrossProbe = true;
                    }
                } else if (targetDim == 0 && (finalY < -500.0f || std::abs(finalY - 320.0f) < 0.1f)) {
                    int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight((int)std::floor(targetX), (int)std::floor(targetZ), false);
                    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -64 && cachedY < 319) {
                        finalY = (float)cachedY + 1.0f;
                    } else {
                        // 主世界未探索黑色区域：传送到 Y=320 高空启动两阶段地表探测
                        finalY = 320.0f;
                        needOverworldCrossProbe = true;
                    }
                } else if (finalY < -500.0f || std::abs(finalY - 320.0f) < 0.1f) {
                    if (targetDim == 2) finalY = 65.0f;  // 末地岛屿表面
                    else finalY = 70.0f;                // 默认海平面附近
                }

                char coordBuf[160];
                std::snprintf(coordBuf, sizeof(coordBuf), "/execute in %s run tp @s %.2f %.2f %.2f",
                              dimName.c_str(), targetX, finalY, targetZ);
                SendServerCommand(*player, coordBuf);

                if (needNetherCrossProbe || needOverworldCrossProbe) {
                    // 启动向下探测安全落脚点 (Phase 1)
                    MapRenderState::probeMode = (targetDim == 1) ? 1 : 0;
                    MapRenderState::probeMinY = (targetDim == 1) ? 2 : -64;
                    MapRenderState::probeMaxY = (targetDim == 1) ? 125 : 319;
                    MapRenderState::probeRefY = 64;
                    MapRenderState::probeIsNether = (targetDim == 1);
                    MapRenderState::probeOriginalX = g_playerX;
                    MapRenderState::probeOriginalY = g_playerY;
                    MapRenderState::probeOriginalZ = g_playerZ;
                    MapRenderState::probeTargetX.store((int)std::floor(targetX));
                    MapRenderState::probeTargetZ.store((int)std::floor(targetZ));
                    MapRenderState::probeStartTime = std::chrono::steady_clock::now();
                    MapRenderState::probeLastY = -32000;
                    MapRenderState::probeLastX = 0;
                    MapRenderState::probeLastZ = 0;
                    MapRenderState::probeStableCount = 0;
                    MapRenderState::pendingSurfaceProbe.store(true);
                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Loading);
                    MapRenderState::teleportStatusMsg = LanguageManager::GetText("TELEPORT_LOADING");
                } else {
                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                    MapRenderState::teleportStatusMsg.clear();
                }

                LogTeleport("tp cross-dimension from dim=" + std::to_string(currentDim) +
                            " to dim=" + std::to_string(targetDim) + " (" + dimName +
                            ") coords=(" + std::to_string((int)targetX) + "," +
                            std::to_string((int)finalY) + "," + std::to_string((int)targetZ) + ")");

                MapRenderState::tpTargetDim = -1;
                MapRenderState::triggerTeleport.store(false);
            } else {
                MapRenderState::tpTargetDim = -1;
                std::string detectMethod = "user-specified";

                int dimId = MapRenderState::currentDimensionId;
                // targetY < -500（哨兵）或 == 320（未加载默认值）→ 系统决定地表
                bool needSurfaceDetect = (targetY < -500.0f) || (std::abs(targetY - 320.0f) < 0.1f);
                if (dimId == 1) {
                    // 下界环境防线：
                    // 1. 若 targetY >= 125 (在顶部基岩层上或之上) 强制必须往下探测安全落脚点，绝不允许留在基岩层之上！
                    // 2. 若 targetY < 33 (在底部岩浆海中或虚空) 强制探测安全落脚点
                    if (targetY >= 125.0f || targetY < 33.0f) {
                        needSurfaceDetect = true;
                    } else {
                        // 3. 即使指定了 Y，若目标区块已就绪且该点是实心地狱岩/岩浆（不安全），强制重探测，杜绝窒息
                        BlockSource* chkRegion = this->getRegion();
                        int bx = (int)std::floor(targetX);
                        int by = (int)std::floor(targetY);
                        int bz = (int)std::floor(targetZ);
                        if (chkRegion && IsChunkReady(*chkRegion, bx, by, bz) && !IsTeleportSpotSafe(*chkRegion, bx, by, bz, 1)) {
                            needSurfaceDetect = true;
                        }
                    }
                }

            if (needSurfaceDetect) {
                int blockX = (int)std::floor(targetX);
                int blockZ = (int)std::floor(targetZ);
                BlockSource* region = this->getRegion();

                // [维度感知] 确定传送模式
                // 0=主世界地表, 1=下界/主世界洞穴, 2=末地开阔空岛
                int teleportMode = 0;
                if (dimId == 1) {
                    teleportMode = 1;  // 下界: 洞穴模式传送
                } else if (dimId == 2) {
                    teleportMode = 2;  // 末地: 开阔空岛地表传送 (避免虚空)
                } else if (MapRenderState::g_caveModeActive) {
                    teleportMode = 1;  // 主世界洞穴: 洞穴模式传送
                }
                int refY = (int)g_playerY;  // 洞穴传送的参考Y (玩家当前高度)
                if (dimId == 1) {
                    int16_t cachedNetherY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ, true);
                    if (cachedNetherY != MapCacheManager::HEIGHT_UNKNOWN && cachedNetherY >= 33 && cachedNetherY <= 100) {
                        refY = (int)cachedNetherY + 1; // 已保存具体坐标点的位置: 优先在保存高度附近查找
                    } else if (targetY >= 33.0f && targetY <= 100.0f) {
                        refY = (int)targetY;
                    } else {
                        refY = (g_playerY >= 33.0f && g_playerY <= 100.0f) ? (int)g_playerY : 64; // 避开基岩顶层，默认参考高度64
                    }
                } else if (MapRenderState::g_caveModeActive) {
                    int16_t cachedCaveY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ, true);
                    if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > -64) {
                        refY = (int)cachedCaveY + 1;
                    }
                }

                // [Y范围限制] 按维度计算扫描范围
                // 下界 [2,125] 避开基岩层; 洞穴 [refY-48, refY+48] 避免穿过天花板到地表; 末地不限
                int tpMinY = -64, tpMaxY = 319;
                if (dimId == 1) {
                    tpMinY = 2; tpMaxY = 125;  // 下界: 避开底部/顶部基岩层
                } else if (teleportMode == 1) {
                    tpMinY = std::max(-64, refY - 48);  // 洞穴: 限制在玩家Y±48
                    tpMaxY = std::min(319, refY + 48);
                }

                // [防线①] 区块就绪检查：仅当区块已完全加载时才信任实时/缓存 Y
                bool chunkReady = false;
                if (region) {
                    int checkY = (teleportMode == 1) ? ((dimId == 1 && refY >= 120) ? 64 : refY) : 64;
                    chunkReady = IsChunkReady(*region, blockX, checkY, blockZ);
                }

                bool needProbe = false;

                if (teleportMode == 0 || teleportMode == 2) {
                    // === 地表/末地传送: 缓存/实时高度 → SafeFindSafeSpawnY → FindNearestSafeSpawn ===
                    int16_t surfaceY = MapCacheManager::HEIGHT_UNKNOWN;

                    // [优先级1] 缓存高度图：可识别已扫描但已卸载的区域
                    int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ);
                    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -64 && cachedY < 319) {
                        surfaceY = cachedY;
                        detectMethod = "cache";
                    }

                    // 未探索黑色区域：绝不盲目瞬移到默认高度，必须传送到 Y=320 高空启动两阶段地表探测！
                    if (cachedY == MapCacheManager::HEIGHT_UNKNOWN) {
                        needProbe = true;
                    } else {
                        // [优先级2] 实时 BlockSource：仅对已加载区块有效
                        if (surfaceY == MapCacheManager::HEIGHT_UNKNOWN && region && chunkReady) {
                            short liveY = SafeGetSurfaceY(*region, blockX, blockZ);
                            if (liveY > -64 && liveY < 319) {
                                surfaceY = liveY;
                                detectMethod = "live";
                            }
                        }

                        if (surfaceY != MapCacheManager::HEIGHT_UNKNOWN && surfaceY > -64 && surfaceY < 319) {
                            if (region && chunkReady) {
                                // [防线②③] 命中缓存/实时且区块就绪 → 用 SafeFindSafeSpawnY 验证落脚点
                                short safeY = SafeFindSafeSpawnY(*region, blockX, blockZ, dimId);
                                if (safeY > -64 && safeY < 319 && IsTeleportSpotSafe(*region, blockX, safeY, blockZ, dimId)) {
                                    // 落脚点验证通过 → 直接传送
                                    float finalY = (float)safeY;
                                    LogTeleport("tp instant (" + std::to_string(blockX) + "," +
                                                std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                                ") dim=" + std::to_string(dimId) + " method=" + detectMethod + "/safe-spawn");
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                  (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                    SendServerCommand(*player, coordBuf);
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                    MapRenderState::teleportStatusMsg.clear();
                                } else {
                                    // [防线⑤] 目标列为岩浆/虚空/不安全 → 螺旋搜索周围安全落脚点 (±16~±32)
                                    int searchX = blockX, searchZ = blockZ;
                                    short nearbyY = -32000;
                                    if (FindNearestSafeSpawn(*region, searchX, searchZ, nearbyY, 16, dimId) &&
                                        IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, dimId)) {
                                        LogTeleport("tp nearby-fallback (" + std::to_string(searchX) + "," +
                                                    std::to_string((int)nearbyY) + "," + std::to_string(searchZ) +
                                                    ") [original (" + std::to_string(blockX) + "," +
                                                    std::to_string(blockZ) + ") lava/void, landed on safe ground, dim=" + std::to_string(dimId) + "]");
                                        char coordBuf[128];
                                        std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                      (float)searchX + 0.5f, (float)nearbyY, (float)searchZ + 0.5f);
                                        SendServerCommand(*player, coordBuf);
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                        MapRenderState::teleportStatusMsg.clear();
                                    } else if (surfaceY != MapCacheManager::HEIGHT_UNKNOWN && surfaceY > -64 && surfaceY < 319) {
                                        // [解除误判驳回] 实时落脚点未就绪，使用已探索地图缓存中的真实地表Y高度进行精准传送
                                        float finalY = (float)surfaceY;
                                        if (dimId == 0 && MapCacheManager::IsCachedWater(blockX, blockZ)) {
                                            if (finalY < 63.0f) finalY = 63.0f;
                                        }
                                        LogTeleport("tp instant cached-fallback (" + std::to_string(blockX) + "," +
                                                    std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                                    ") dim=" + std::to_string(dimId) + " method=" + detectMethod + "/direct-cache-fallback");
                                        char coordBuf[128];
                                        std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                      (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                        SendServerCommand(*player, coordBuf);
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                        MapRenderState::teleportStatusMsg.clear();
                                    } else {
                                        needProbe = true;
                                    }
                                }
                            } else {
                                // [解除强行区块就绪校验] 区块未就绪，但已探索缓存中有地表Y记录 → 直接精准传送到地表
                                float finalY = (float)surfaceY;
                                if (dimId == 0 && MapCacheManager::IsCachedWater(blockX, blockZ)) {
                                    if (finalY < 63.0f) finalY = 63.0f;
                                }
                                LogTeleport("tp instant cached (" + std::to_string(blockX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                            ") dim=" + std::to_string(dimId) + " method=" + detectMethod + "/direct-cache");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            }
                        } else {
                            needProbe = true;
                        }
                    }
                } else {
                    // === 洞穴/下界传送: 在参考Y附近搜索安全落脚点 ===
                    bool isCave = (dimId == 1) || MapRenderState::g_caveModeActive;
                    int16_t cachedCaveY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ, isCave);

                    if (dimId == 1) {
                        // [下界传送]
                        // 未探索黑色区域：绝不单列瞬移，强制传送到 Y=128 基岩顶层之上启动两阶段范围探测！
                        if (cachedCaveY == MapCacheManager::HEIGHT_UNKNOWN) {
                            needProbe = true;
                        } else if (region && chunkReady) {
                            short safeY = SafeFindSafeSpawnY(*region, blockX, blockZ, 1);
                            if (safeY >= 33 && safeY <= 100 && IsTeleportSpotSafe(*region, blockX, safeY, blockZ, 1)) {
                                LogTeleport("tp nether-instant (" + std::to_string(blockX) + "," +
                                            std::to_string((int)safeY) + "," + std::to_string(blockZ) +
                                            ") [refY=" + std::to_string(refY) + "]");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, (float)safeY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            } else {
                                // 目标点为岩浆或不安全 → 搜索周围安全落脚点 (±16~±32)
                                int searchX = blockX, searchZ = blockZ;
                                short nearbyY = -32000;
                                if ((FindNearestSafeSpawnNearY(*region, searchX, searchZ, nearbyY, refY, 16, tpMinY, tpMaxY, 1) ||
                                     FindNearestSafeSpawnNearY(*region, searchX, searchZ, nearbyY, refY, 32, tpMinY, tpMaxY, 1)) &&
                                    nearbyY >= 33 && nearbyY <= 100 &&
                                    IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, 1)) {
                                    LogTeleport("tp nether-nearby (" + std::to_string(searchX) + "," +
                                                std::to_string((int)nearbyY) + "," + std::to_string(searchZ) +
                                                ") [refY=" + std::to_string(refY) + "]");
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                  (float)searchX + 0.5f, (float)nearbyY, (float)searchZ + 0.5f);
                                    SendServerCommand(*player, coordBuf);
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                    MapRenderState::teleportStatusMsg.clear();
                                } else {
                                    // 目标点及周围 32 格内暂无已验证的安全点 → 转入两阶段探测！
                                    needProbe = true;
                                }
                            }
                        } else {
                            // 下界区块未就绪：转入两阶段探测
                            needProbe = true;
                        }
                    } else {
                        // [主世界洞穴传送]
                        if (region && chunkReady) {
                            short safeY = SafeFindSafeSpawnYNearY(*region, blockX, blockZ, refY, tpMinY, tpMaxY, 0);
                            if (safeY > -64 && safeY < 319 && IsTeleportSpotSafe(*region, blockX, safeY, blockZ, 0)) {
                                LogTeleport("tp cave-instant (" + std::to_string(blockX) + "," +
                                            std::to_string((int)safeY) + "," + std::to_string(blockZ) +
                                            ") [refY=" + std::to_string(refY) + "]");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, (float)safeY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            } else if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > -64 && cachedCaveY < 319) {
                                float finalY = (float)cachedCaveY + 1.0f;
                                LogTeleport("tp cave cached-fallback (" + std::to_string(blockX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                            ") dim=0 method=cache/direct-cave-fallback");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            } else {
                                needProbe = true;
                            }
                        } else if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > -64 && cachedCaveY < 319) {
                            // [解除强行区块就绪校验] 洞穴区块未就绪，但缓存中有洞穴Y高度记录 → 直接精准传送
                            float finalY = (float)cachedCaveY + 1.0f;
                            LogTeleport("tp cave cached (" + std::to_string(blockX) + "," +
                                        std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                        ") dim=0 method=cache/direct-cave");
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                            MapRenderState::teleportStatusMsg.clear();
                        } else {
                            needProbe = true;
                        }
                    }
                }

                if (needProbe) {
                    // [两阶段探测] Phase 0 tp 触发区块加载, Phase 1 轮询找安全Y
                    MapRenderState::probeMode = teleportMode;
                    MapRenderState::probeMinY = tpMinY;
                    MapRenderState::probeMaxY = tpMaxY;
                    MapRenderState::probeRefY = refY;
                    MapRenderState::probeIsNether = (dimId == 1);
                    MapRenderState::probeOriginalX = g_playerX;
                    MapRenderState::probeOriginalY = g_playerY;
                    MapRenderState::probeOriginalZ = g_playerZ;
                    MapRenderState::probeTargetX.store(blockX);
                    MapRenderState::probeTargetZ.store(blockZ);
                    MapRenderState::probeStartTime = std::chrono::steady_clock::now();
                    MapRenderState::probeLastY = -32000;
                    MapRenderState::probeLastX = 0;
                    MapRenderState::probeLastZ = 0;
                    MapRenderState::probeStableCount = 0;
                    MapRenderState::pendingSurfaceProbe.store(true);
                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Loading);
                    MapRenderState::teleportStatusMsg = LanguageManager::GetText("TELEPORT_LOADING");
                    MapRenderState::teleportFailReason.clear();

                    // Phase 0 探测传送：先传送到安全高度触发区块加载
                    // 下界: Y=128 (基岩顶层之上, 玩家安全等待区块加载)
                    // 其他维度: Y=320 (地表/空岛之上)
                    float probeY = (dimId == 1) ? 128.0f : 320.0f;
                    char probeBuf[128];
                    std::snprintf(probeBuf, sizeof(probeBuf), "/tp @s %.2f %.2f %.2f", targetX, probeY, targetZ);
                    SendServerCommand(*player, probeBuf);

                    LogTeleport("probe START (" + std::to_string(blockX) + "," +
                                std::to_string((int)probeY) + "," + std::to_string(blockZ) +
                                ") [mode=" + std::to_string(teleportMode) + " dim=" + std::to_string(dimId) +
                                " waiting chunk load, chunkReady=" + (chunkReady ? "yes" : "no") + "]");
                }
            } else {
                // 用户/路径点显式指定 Y（非 -500/非 320）→ 直接传送，尊重原意
                LogTeleport("tp user-y (" + std::to_string((int)targetX) + "," +
                            std::to_string((int)targetY) + "," + std::to_string((int)targetZ) +
                            ") method=" + detectMethod);
                char coordBuf[128];
                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f", targetX, targetY, targetZ);
                SendServerCommand(*player, coordBuf);
                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                MapRenderState::teleportStatusMsg.clear();
            }
                MapRenderState::triggerTeleport.store(false);
            }
        }

        // ==========================================
        // [两阶段探测·Phase 1 轮询] 等待目标区块加载后传送到地表/落脚点
        // ==========================================
        if (MapRenderState::pendingSurfaceProbe.load()) {
            int probeX = MapRenderState::probeTargetX.load();
            int probeZ = MapRenderState::probeTargetZ.load();
            bool probeDone = false;

            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - MapRenderState::probeStartTime).count();

            // [防线①] Phase 0 tp 后 400ms 宽限期，留出远端区块建立连接与初始包下发时间
            if (elapsedMs >= 400) {
                BlockSource* region = this->getRegion();
                if (region) {
                    int probeMode = MapRenderState::probeMode;
                    int dimId = (int)player->getDimensionId();
                    bool isNether = (dimId == 1 || MapRenderState::probeIsNether);

                    // 跨维度传送防护：等待客户端真正切换维度后再探测方块
                    if (MapRenderState::probeIsNether && dimId != 1) {
                        // 正在跨维度进入下界，继续等待
                    } else if (!MapRenderState::probeIsNether && dimId == 1 && MapRenderState::currentDimensionId != 1) {
                        // 正在跨维度退出下界，继续等待
                    } else if (isNether) {
                        // ==========================================
                        // [下界·Phase 1] 确认下界区块方块数据加载到达客户端后，寻找最佳开阔安全落脚点
                        // ==========================================
                        bool netherDataLoaded = false;
                        std::string testBlock;
                        // 检查下界顶部 Y=127 或底部 Y=0 基岩层是否已下发（保证全高 subchunk 已完整到达客户端）
                        if (SafeGetBlockName(*region, probeX, 127, probeZ, testBlock) && 
                            !testBlock.empty() && testBlock.find("bedrock") != std::string::npos) {
                            netherDataLoaded = true;
                        } else if (SafeGetBlockName(*region, probeX, 0, probeZ, testBlock) && 
                            !testBlock.empty() && testBlock.find("bedrock") != std::string::npos) {
                            netherDataLoaded = true;
                        } else if (SafeGetBlockName(*region, probeX, 32, probeZ, testBlock) && !testBlock.empty()) {
                            netherDataLoaded = true;
                        } else if (SafeGetBlockName(*region, probeX, 64, probeZ, testBlock) && !testBlock.empty()) {
                            netherDataLoaded = true;
                        }

                        int searchX = probeX;
                        int searchZ = probeZ;
                        short netherY = -32000;
                        int prefY = (MapRenderState::probeRefY >= 33 && MapRenderState::probeRefY <= 100) ? MapRenderState::probeRefY : 64;

                        // 仅当区块数据确已加载到达时，范围搜索整片区域 (r=32) 内的最佳开阔安全点，允许 x, z 自由变化
                        bool found = false;
                        if (netherDataLoaded) {
                            found = FindBestSafeSpawnNether(*region, searchX, searchZ, netherY, prefY, 32);
                        }

                        if (found && netherY >= 33 && netherY <= 100 && IsTeleportSpotSafe(*region, searchX, netherY, searchZ, 1)) {
                            // 连续 6 帧且至少 800ms 宽限期确认安全点坐标与高度稳定
                            if (netherY == MapRenderState::probeLastY && searchX == MapRenderState::probeLastX && searchZ == MapRenderState::probeLastZ) {
                                MapRenderState::probeStableCount++;
                            } else {
                                MapRenderState::probeLastY = netherY;
                                MapRenderState::probeLastX = searchX;
                                MapRenderState::probeLastZ = searchZ;
                                MapRenderState::probeStableCount = 1;
                            }

                            if (MapRenderState::probeStableCount >= 6 && elapsedMs >= 800) {
                                float finalY = (float)netherY;
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)searchX + 0.5f, finalY, (float)searchZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                LogTeleport("probe SUCCESS-nether (" + std::to_string(searchX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(searchZ) +
                                            ") [prefY=" + std::to_string(prefY) + ", safe open cavern ground]");
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                                probeDone = true;
                            }
                        } else {
                            MapRenderState::probeStableCount = 0;
                            MapRenderState::probeLastY = -32000;
                            // 超过 6 秒未在下方找到足够开阔的安全溶洞 → 绝不驳回！安全驻留在 Y=128 基岩顶层完成传送
                            if (elapsedMs >= 6000 && netherDataLoaded) {
                                LogTeleport("probe SUCCESS-nether-roof (" + std::to_string(probeX) + ",128," +
                                            std::to_string(probeZ) + ") [stationed safely on bedrock roof above target]");
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                                probeDone = true;
                            }
                        }
                    } else if (probeMode == 1) {
                        // [主世界洞穴·Phase 1] 稳定性检查 + 纵向安全落脚点搜索
                        int refY = MapRenderState::probeRefY;
                        short liveY = SafeFindSafeSpawnYNearY(*region, probeX, probeZ, refY, MapRenderState::probeMinY, MapRenderState::probeMaxY, dimId);
                        int targetX = probeX;
                        int targetZ = probeZ;
                        short targetY = liveY;

                        if (targetY <= -64 || targetY >= 319 || !IsTeleportSpotSafe(*region, targetX, targetY, targetZ, dimId)) {
                            // 目标列为岩石/岩浆/不安全 → 搜索周围安全落脚点
                            short nearbyY = -32000;
                            int searchX = probeX, searchZ = probeZ;
                            if (FindNearestSafeSpawnNearY(*region, searchX, searchZ, nearbyY, refY, 16, MapRenderState::probeMinY, MapRenderState::probeMaxY, dimId) &&
                                IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, dimId)) {
                                targetX = searchX;
                                targetZ = searchZ;
                                targetY = nearbyY;
                            }
                        }

                        if (targetY > -64 && targetY < 319) {
                            if (targetY == MapRenderState::probeLastY && targetX == MapRenderState::probeLastX && targetZ == MapRenderState::probeLastZ) {
                                MapRenderState::probeStableCount++;
                            } else {
                                MapRenderState::probeLastY = targetY;
                                MapRenderState::probeLastX = targetX;
                                MapRenderState::probeLastZ = targetZ;
                                MapRenderState::probeStableCount = 1;
                            }
                        } else {
                            MapRenderState::probeStableCount = 0;
                            MapRenderState::probeLastY = -32000;
                        }

                        if (MapRenderState::probeStableCount >= 2) {
                            float finalY = (float)targetY;
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)targetX + 0.5f, finalY, (float)targetZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            LogTeleport("probe SUCCESS-cave (" + std::to_string(targetX) + "," +
                                        std::to_string((int)finalY) + "," + std::to_string(targetZ) +
                                        ") [refY=" + std::to_string(refY) + ", safe cave ground]");
                            MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                            MapRenderState::teleportStatusMsg.clear();
                            probeDone = true;
                        } else if (targetY <= -64 && elapsedMs >= 4000) {
                            // 超过 4 秒未在洞穴深度找到落脚点 → 优雅降级至对应地表安全位置，绝不失败驳回！
                            short surfY = SafeFindSafeSpawnY(*region, probeX, probeZ, 0);
                            if (surfY <= -64 || surfY >= 319) surfY = SafeGetSurfaceY(*region, probeX, probeZ);
                            if (surfY > -64 && surfY < 319) {
                                float finalY = (float)surfY;
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)probeX + 0.5f, finalY, (float)probeZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                LogTeleport("probe CAVE-FALLBACK-SURFACE (" + std::to_string(probeX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(probeZ) +
                                            ") [cave unsafe/solid rock, landed on surface]");
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                                probeDone = true;
                            }
                        }
                    } else {
                        // ==========================================
                        // [主世界地表 / 末地·Phase 1] 稳定性检查：自顶向下获取地表安全落脚点
                        // ==========================================
                        int targetX = probeX;
                        int targetZ = probeZ;
                        short targetY = -32000;
                        bool found = false;

                        // 阶段 1: 精准查找安全落脚点（含水面、雪原、植被与开阔陆地）
                        short liveY = SafeFindSafeSpawnY(*region, probeX, probeZ, dimId);
                        if (liveY > -64 && liveY < 319 && 
                            IsTeleportSpotSafe(*region, probeX, liveY, probeZ, dimId) &&
                            HasOpenSkyAbove(*region, probeX, liveY, probeZ, dimId)) {
                            targetX = probeX;
                            targetZ = probeZ;
                            targetY = liveY;
                            found = true;
                        } else if (elapsedMs >= 1500) {
                            // 留出 1500ms 保证中心区块完整生成，再搜索周围安全露天落脚点 (r=16, 必要时扩展至 r=32)
                            int searchX = probeX, searchZ = probeZ;
                            short nearbyY = -32000;
                            if (FindNearestSafeSpawn(*region, searchX, searchZ, nearbyY, 16, dimId) &&
                                nearbyY > -64 && nearbyY < 319 &&
                                IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, dimId) &&
                                HasOpenSkyAbove(*region, searchX, nearbyY, searchZ, dimId)) {
                                targetX = searchX;
                                targetZ = searchZ;
                                targetY = nearbyY;
                                found = true;
                            } else if (FindNearestSafeSpawn(*region, searchX, searchZ, nearbyY, 32, dimId) &&
                                       nearbyY > -64 && nearbyY < 319 &&
                                       IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, dimId) &&
                                       HasOpenSkyAbove(*region, searchX, nearbyY, searchZ, dimId)) {
                                targetX = searchX;
                                targetZ = searchZ;
                                targetY = nearbyY;
                                found = true;
                            }
                        }

                        // 阶段 2: 优雅降级（若经过 4.0 秒仍未精准命中，尝试 SafeFindSafeSpawnY 严格验证）
                        if (!found && elapsedMs >= 4000) {
                            short directSurfY = SafeFindSafeSpawnY(*region, probeX, probeZ, dimId);
                            if (directSurfY > -64 && directSurfY < 319 && 
                                IsTeleportSpotSafe(*region, probeX, directSurfY, probeZ, dimId) &&
                                HasOpenSkyAbove(*region, probeX, directSurfY, probeZ, dimId)) {
                                targetX = probeX;
                                targetZ = probeZ;
                                targetY = directSurfY;
                                found = true;
                            }
                        }

                        // 高空滞空防坠落保护：如果区块尚未就绪仍在等待，每 1.2 秒刷新一次玩家于 320 高度
                        static auto s_lastHoverHoldTime = std::chrono::steady_clock::now();
                        if (!found && elapsedMs >= 1500) {
                            auto holdDiff = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - s_lastHoverHoldTime).count();
                            if (holdDiff >= 1200) {
                                s_lastHoverHoldTime = std::chrono::steady_clock::now();
                                char hoverBuf[128];
                                std::snprintf(hoverBuf, sizeof(hoverBuf), "/tp @s %.2f 320.00 %.2f",
                                              (float)probeX + 0.5f, (float)probeZ + 0.5f);
                                SendServerCommand(*player, hoverBuf);
                            }
                        }

                        if (found && targetY > -64 && targetY < 319) {
                            if (targetY == MapRenderState::probeLastY && targetX == MapRenderState::probeLastX && targetZ == MapRenderState::probeLastZ) {
                                MapRenderState::probeStableCount++;
                            } else {
                                MapRenderState::probeLastY = targetY;
                                MapRenderState::probeLastX = targetX;
                                MapRenderState::probeLastZ = targetZ;
                                MapRenderState::probeStableCount = 1;
                            }

                            // 稳定判定：至少 6 帧连续稳定且达到基础等待时间 (800ms)；
                            // 主世界干地若 < 62，必须至少等待 5000ms 宽限期以确保绝非未完成下发的假象
                            bool meetsWaitTime = (elapsedMs >= 800);
                            if (dimId == 0 && targetY < 62) {
                                std::string tg;
                                bool isWater = (SafeGetBlockName(*region, targetX, targetY - 1, targetZ, tg) && tg.find("water") != std::string::npos);
                                if (!isWater && elapsedMs < 5000) {
                                    meetsWaitTime = false;
                                }
                            }

                            if (MapRenderState::probeStableCount >= 6 && meetsWaitTime) {
                                float finalY = (float)targetY;
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf),
                                              "/tp @s %.2f %.2f %.2f", (float)targetX + 0.5f, finalY, (float)targetZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                LogTeleport("probe SUCCESS-surface (" + std::to_string(targetX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(targetZ) +
                                            ") dim=" + std::to_string(dimId) + " method=stable-safe");
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                                probeDone = true;
                            }
                        } else {
                            MapRenderState::probeStableCount = 0;
                            MapRenderState::probeLastY = -32000;
                        }
                    }
                }
            }

            if (!probeDone) {
                // [超时安全保障] 给予远距离未探索区块生成与下发充裕时间 (25 秒)
                if (elapsedMs >= 25000) {
                    BlockSource* reg = this->getRegion();
                    bool fallbackDone = false;
                    if (reg) {
                        int dimId = (int)player->getDimensionId();
                        if (dimId == 1 || MapRenderState::probeIsNether) {
                            // 下界超时保底：安居基岩顶层 Y=128
                            LogTeleport("probe SUCCESS-nether-roof (" + std::to_string(probeX) + ",128," +
                                        std::to_string(probeZ) + ") [timeout safe: stationed on bedrock roof]");
                            fallbackDone = true;
                        } else {
                            short surfY = SafeFindSafeSpawnY(*reg, probeX, probeZ, 0);
                            if (surfY <= -64 || surfY >= 319) surfY = SafeGetSurfaceY(*reg, probeX, probeZ);
                            // 超时安全防线：主世界传送绝对不允许落在 Y < 63 的地下岩石/泥土中窒息！
                            if (surfY < 63) surfY = 64;
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)probeX + 0.5f, (float)surfY, (float)probeZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            LogTeleport("probe TIMEOUT-FALLBACK-SURFACE (" + std::to_string(probeX) + "," +
                                        std::to_string((int)surfY) + "," + std::to_string(probeZ) + ") [safe height >= 64]");
                            fallbackDone = true;
                        }
                    }
                    if (fallbackDone) {
                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                        MapRenderState::teleportStatusMsg.clear();
                        probeDone = true;
                    } else {
                        // 仅当经过 25 秒仍未收到任何有效区块数据（如网络断开/离线异常）时，才回退原位防止永久卡在虚空
                        char abortBuf[128];
                        std::snprintf(abortBuf, sizeof(abortBuf), "/tp @s %.2f %.2f %.2f",
                                      MapRenderState::probeOriginalX,
                                      MapRenderState::probeOriginalY,
                                      MapRenderState::probeOriginalZ);
                        SendServerCommand(*player, abortBuf);
                        LogTeleport("probe TIMEOUT abort→original (" +
                                    std::to_string((int)MapRenderState::probeOriginalX) + "," +
                                    std::to_string((int)MapRenderState::probeOriginalY) + "," +
                                    std::to_string((int)MapRenderState::probeOriginalZ) +
                                    ") [server stopped streaming chunks for 25s]");
                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Failed);
                        MapRenderState::teleportStatusMsg.clear();
                        MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_TIMEOUT_MSG");
                        probeDone = true;
                    }
                }
            }

            if (probeDone) {
                MapRenderState::pendingSurfaceProbe.store(false);
                MapRenderState::probeStableCount = 0;
                MapRenderState::probeLastY = -32000;
                MapRenderState::probeLastX = 0;
                MapRenderState::probeLastZ = 0;
            }
        }

        // ==========================================
        // [分层地图系统] 跨维度与跨地下层级监听器
        // [性能] 原实现每帧调用 getLevelId/getSeed/getDefaultSpawn + 构造 worldId 字符串
        // (约 8-10 次堆分配) + 每帧 getBiome + biome.mHash->getString (2-3 次堆分配)。
        // 改为 30 帧 (0.5s) 节流：世界 ID 在游戏中几乎不变，生物群系名也只需半秒级刷新。
        // ==========================================
        int currentLiveDim = (int)player->getDimensionId();
        bool dimChanged = (MapRenderState::currentDimensionId != -999 && currentLiveDim != MapRenderState::currentDimensionId);
        static int s_worldBiomeCheckCounter = 30;
        s_worldBiomeCheckCounter++;
        if (dimChanged || s_worldBiomeCheckCounter >= 30) {
            s_worldBiomeCheckCounter = 0;
            try {
                int dimId = currentLiveDim;
                std::string rawLevelId = "UnknownWorld";
                std::string seedStr = "0";
                std::string spawnStr = "0_0";

                try {
                    std::string lid = player->getLevel().getLevelId();
                    if (!lid.empty()) rawLevelId = lid;
                } catch(...) {}

                try {
                    unsigned int seed = player->getLevel().getSeed();
                    seedStr = std::to_string(seed);
                } catch(...) {}

                try {
                    BlockPos spawn = player->getLevel().getDefaultSpawn();
                    spawnStr = std::to_string(spawn.x) + "_" + std::to_string(spawn.z);
                    MapRenderState::worldSpawnX = spawn.x;
                    MapRenderState::worldSpawnY = spawn.y;
                    MapRenderState::worldSpawnZ = spawn.z;
                    MapRenderState::hasWorldSpawn = true;
                } catch(...) {}

                if (rawLevelId.empty() || rawLevelId == "UnknownWorld") {
                    rawLevelId = "RemoteServer";
                }

                std::string finalWorldId = rawLevelId + "_S" + seedStr + "_P" + spawnStr;

                for (char& c : finalWorldId) {
                    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
                }

                CapturedLevelSeed seedCapture = CaptureLevelSeedSnapshot(player->getLevel());
                if (!seedCapture.seed.HasSeed() && seedStr != "0") {
                    try {
                        uint64_t s32 = std::stoull(seedStr);
                        if (ChiyanMap::WorldGen::IsUsableCapturedBedrockSeed(s32)) {
                            seedCapture.rawBits = s32;
                            seedCapture.seed = ChiyanMap::WorldGen::SeedSnapshot::FromSeedBits(s32);
                        }
                    } catch(...) {}
                }

                if (MapRenderState::currentWorldId != finalWorldId || MapRenderState::currentDimensionId != dimId) {
                    // 维度/世界切换：立即中止当前扫描并清除待写缓存，绝不允许旧维度残留数据写入新维度缓存
                    AbortActiveScanAndPendingWrites();

                    if (MapRenderState::currentWorldId != finalWorldId) {
                        MapRenderState::ResetDimCameraStates();
                    }
                    MapRenderState::currentWorldId = finalWorldId;
                    MapRenderState::currentDimensionId = dimId;
                    MapRenderState::bigMapViewDimensionId = -999;

                    MultiworldManager::SwitchWorld(finalWorldId, MapRenderState::worldSpawnX, MapRenderState::worldSpawnZ);
                    MapCacheManager::SwitchWorld(finalWorldId, dimId);
                    WaypointManager::SwitchWorld(finalWorldId, dimId);
                    DeathPointManager::SwitchWorld(finalWorldId);
                    SeedMapManager::NotifyWorldChanged(finalWorldId, seedCapture.seed, seedCapture.rawBits, dimId);

                    MapRenderState::clearGPUCache.store(true);

                    int px = g_playerBlockX;
                    int pz = g_playerBlockZ;
                    bool isCave = (dimId == 1) || MapRenderState::g_caveModeActive;

                    // [极速加载核心] 进世界/切维度第 0 帧：立即从缓存预填当前位置小地图与直接推送纹理，杜绝黑块！
                    MapCacheManager::PrefillMapGrid(px, pz, g_mapColorsBack, g_mapHeightsBack, isCave, false, g_textureData, g_mapLightsBack);
                    {
                        std::lock_guard<std::mutex> lock(g_mapDataMutex);
                        std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                        std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                        std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                        g_lastRenderX = px;
                        g_lastRenderZ = pz;
                        g_textureCenterX = (float)px;
                        g_textureCenterZ = (float)pz;
                        g_textureReadyToUpload.store(true);
                        g_mapDataUpdated.store(true);
                    }

                    g_currentScanX = px;
                    g_currentScanZ = pz;
                    g_scanPrevDimension = dimId;
                    g_scanPrevCaveActive = isCave;
                    g_scanDimension = dimId;
                    g_isScanning = true;
                    g_scanPhase = 0;
                    g_scanRadius = 0;
                    g_scanPerimeterIndex = 0;
                } else {
                    // 处于同一世界中，但若种子地图尚未有可用种子，且当前成功捕获到可用种子，实时刷新
                    const auto smStatus = SeedMapManager::GetStatus();
                    if (!smStatus.seedAvailable && seedCapture.seed.HasSeed()) {
                        SeedMapManager::NotifyWorldChanged(finalWorldId, seedCapture.seed, seedCapture.rawBits, dimId);
                    }
                }
            } catch(...) {}

            try {
                BlockSource* regionPtr = this->getRegion();
                if (regionPtr) {
                    auto const& biome = regionPtr->getBiome(BlockPos(g_playerBlockX, (int)g_playerY, g_playerBlockZ));
                    {
                        std::string rawName = biome.mHash->getString();
                        std::string displayRawName = rawName;

                        // 剥离命名空间前缀（例如将 minecraft:plains 转为 plains）
                        size_t colonPos = displayRawName.find(":");
                        if (colonPos != std::string::npos) {
                            displayRawName = displayRawName.substr(colonPos + 1);
                        }

                        MapRenderState::rawBiomeName = displayRawName;
                        MapRenderState::translatedBiomeName = TranslateBiomeName(rawName);
                    }
                }
            } catch (...) {}
        }

        static bool wasPlayerAlive = true;
        static bool hasLastAlivePos = false;
        static int lastAliveX = 0;
        static int lastAliveY = 0;
        static int lastAliveZ = 0;
        static int lastAliveDimensionId = 0;

        bool isPlayerAlive = true;
        try {
            if (player) {
                isPlayerAlive = player->isAlive();
            }
        } catch (...) {
            isPlayerAlive = true;
        }

        if (isPlayerAlive) {
            wasPlayerAlive = true;
            hasLastAlivePos = true;
            lastAliveX = g_playerBlockX;
            lastAliveY = (int)std::floor(g_playerY);
            lastAliveZ = g_playerBlockZ;
            lastAliveDimensionId = MapRenderState::currentDimensionId;
        } else if (wasPlayerAlive && hasLastAlivePos && !MapRenderState::currentWorldId.empty()) {
            DeathPointManager::AddDeathPoint(lastAliveX, lastAliveY, lastAliveZ, lastAliveDimensionId);
            wasPlayerAlive = false;
        }

        static bool lastUIState = false;
        bool currentUIState = MapRenderState::IsUIActive();
        if (currentUIState != lastUIState) {
            lastUIState = currentUIState;
            if (currentUIState) {
                this->releaseMouse();
                ClipCursor(NULL);
            } else {
                bool nativeScreenOpen = g_clientInstance && !g_clientInstance->isInGameInputEnabled();
                if (!nativeScreenOpen) {
                    this->grabMouse();
                }
            }
        }

        int px = g_playerBlockX;
        int pz = g_playerBlockZ;
        g_ticksSinceScan++;

        if (MapRenderState::forceRescan.exchange(false)) {
            g_currentScanX = px;
            g_currentScanZ = pz;
            g_ticksSinceScan = 0;
            g_isScanning = true;
            g_scanDimension = MapRenderState::currentDimensionId;
            g_scanPhase = 0;
            g_scanRadius = 0;
            g_scanPerimeterIndex = 0;
            g_scanBiomeEntries.clear();
            g_caveDetectCooldown = 0;
        }

        if (!g_isScanning && (std::abs(px - g_currentScanX) >= 8 || std::abs(pz - g_currentScanZ) >= 8 || g_ticksSinceScan > 50)) {
            int moveDistX = px - g_currentScanX;
            int moveDistZ = pz - g_currentScanZ;
            bool isCaveMode = (MapRenderState::currentDimensionId == 1) || MapRenderState::g_caveModeActive;

            if (std::abs(moveDistX) >= 64 || std::abs(moveDistZ) >= 64) {
                // 远距离跳转/传送：重新整幅预填
                g_currentScanX = px;
                g_currentScanZ = pz;
                MapCacheManager::PrefillMapGrid(px, pz, g_mapColorsBack, g_mapHeightsBack, isCaveMode, false, g_textureData, g_mapLightsBack);
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                    std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                    std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                    g_lastRenderX = px;
                    g_lastRenderZ = pz;
                    g_textureCenterX = (float)px;
                    g_textureCenterZ = (float)pz;
                    g_textureReadyToUpload.store(true);
                    g_mapDataUpdated.store(true);
                }
            } else if (moveDistX != 0 || moveDistZ != 0) {
                // 平移现有网格与GPU纹理，并用缓存填补边缘空缺 (onlyIfMissing = true)
                ShiftScanGrid(g_mapColorsBack, moveDistX, moveDistZ);
                ShiftScanGrid(g_mapHeightsBack, moveDistX, moveDistZ);
                ShiftScanGrid(g_mapLightsBack, moveDistX, moveDistZ, 0xF0);
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    ShiftTextureData(g_textureData, moveDistX, moveDistZ);
                    g_textureCenterX += (float)moveDistX;
                    g_textureCenterZ += (float)moveDistZ;
                }
                g_currentScanX += moveDistX;
                g_currentScanZ += moveDistZ;
                MapCacheManager::PrefillMapGrid(g_currentScanX, g_currentScanZ, g_mapColorsBack, g_mapHeightsBack, isCaveMode, true, g_textureData, g_mapLightsBack);
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                    std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                    std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                    g_lastRenderX = g_currentScanX;
                    g_lastRenderZ = g_currentScanZ;
                    g_textureReadyToUpload.store(true);
                    g_mapDataUpdated.store(true);
                }
            }

            g_ticksSinceScan = 0;
            g_isScanning = true;
            g_scanDimension = MapRenderState::currentDimensionId;
            g_scanPhase = 0;
            g_scanRadius = 0;
            g_scanPerimeterIndex = 0;
            g_scanBiomeEntries.clear();
            g_caveDetectCooldown = 0;
        }

        // [跑图跟随] 扫描进行中玩家继续移动时, 平移扫描窗口跟随玩家 (详见 ShiftScanGrid 注释)。
        // 使扫描中心始终贴近玩家, 避免小地图纹理中心大幅滞后导致 UV 越界采样条纹。
        if (g_isScanning && (std::abs(px - g_currentScanX) >= 16 || std::abs(pz - g_currentScanZ) >= 16)) {
            int shiftX = px - g_currentScanX;
            int shiftZ = pz - g_currentScanZ;
            bool isCaveMode = (MapRenderState::currentDimensionId == 1) || MapRenderState::g_caveModeActive;

            if (std::abs(shiftX) >= 64 || std::abs(shiftZ) >= 64) {
                // 异常跳转/传送 (平移量超过缓冲区): 重新整幅预填
                g_currentScanX = px;
                g_currentScanZ = pz;
                MapCacheManager::PrefillMapGrid(px, pz, g_mapColorsBack, g_mapHeightsBack, isCaveMode, false, g_textureData, g_mapLightsBack);
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                    std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                    std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                    g_lastRenderX = px;
                    g_lastRenderZ = pz;
                    g_textureCenterX = (float)px;
                    g_textureCenterZ = (float)pz;
                    g_textureReadyToUpload.store(true);
                    g_mapDataUpdated.store(true);
                }
                g_scanPhase = 0;
                g_scanRadius = 0;
                g_scanPerimeterIndex = 0;
            } else {
                // 平移后台缓冲与GPU纹理
                ShiftScanGrid(g_mapColorsBack, shiftX, shiftZ);
                ShiftScanGrid(g_mapHeightsBack, shiftX, shiftZ);
                ShiftScanGrid(g_mapLightsBack, shiftX, shiftZ, 0xF0);
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    ShiftTextureData(g_textureData, shiftX, shiftZ);
                    g_textureCenterX += (float)shiftX;
                    g_textureCenterZ += (float)shiftZ;
                }
                g_currentScanX += shiftX;
                g_currentScanZ += shiftZ;

                // [防黑条核心] 立即从缓存填补平移露出的边缘条带 (onlyIfMissing = true) 并写入纹理
                MapCacheManager::PrefillMapGrid(g_currentScanX, g_currentScanZ, g_mapColorsBack, g_mapHeightsBack, isCaveMode, true, g_textureData, g_mapLightsBack);

                // 同步前台缓冲，使小地图纹理中心紧密贴合玩家，彻底杜绝跑图边缘 UV 越界条纹
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                    std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                    std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                    g_lastRenderX = g_currentScanX;
                    g_lastRenderZ = g_currentScanZ;
                    g_textureReadyToUpload.store(true);
                    g_mapDataUpdated.store(true);
                }

                // 优先以玩家新位置为中心重启 Phase 0 核心视野同心环扫描
                g_scanPhase = 0;
                g_scanRadius = 0;
                g_scanPerimeterIndex = 0;
            }
        }

        // ==================== 洞穴地图检测 ====================
        // 维度切换时同步该维度对应的洞穴模式类型 (若未单独覆盖则使用 Default Cave Mode Type)
        if (MapRenderState::g_lastCaveDimensionId != MapRenderState::currentDimensionId) {
            MapRenderState::g_lastCaveDimensionId = MapRenderState::currentDimensionId;
            MapRenderState::SyncCaveModeTypeForDimension(MapRenderState::currentDimensionId);
            MapRenderState::g_caveHasPendingToggle = false;
        }

        // 检查当前维度是否在洞穴模式允许的维度白名单内
        bool dimAllowed = MapRenderState::IsCaveModeAllowedInDimension(MapRenderState::currentDimensionId);
        int effectiveCaveType = dimAllowed ? MapRenderState::g_caveModeType : 0;

        // 当洞穴模式类型在 Layered 与 Full 之间切换时，若当前已在洞穴模式中则立即重启全图扫描
        static int s_prevEffectiveCaveType = -1;
        if (s_prevEffectiveCaveType != effectiveCaveType) {
            bool shouldRestartCaveScan = (s_prevEffectiveCaveType > 0 && effectiveCaveType > 0 && MapRenderState::g_caveModeActive);
            s_prevEffectiveCaveType = effectiveCaveType;
            if (shouldRestartCaveScan) {
                g_currentScanX = px;
                g_currentScanZ = pz;
                g_isScanning = true;
                g_scanPhase = 0;
                g_scanRadius = 0;
                g_scanPerimeterIndex = 0;
            }
        }

        // [下界地图] 下界无天空光照 (hasSkylight=false), 在允许下界洞穴且未关闭时视为洞穴模式
        if (MapRenderState::currentDimensionId == 1) {
            MapRenderState::g_caveHasPendingToggle = false;
            if (effectiveCaveType != 0) {
                MapRenderState::g_caveModeActive = true;
                MapRenderState::g_caveStartY = (effectiveCaveType == (int)MapRenderState::CaveModeType::Full) ? 122 : 120;
            } else {
                MapRenderState::g_caveModeActive = false;
            }
        } else if (g_caveDetectCooldown > 0) {
            g_caveDetectCooldown--;
        }

        if (effectiveCaveType != 0 && MapRenderState::currentDimensionId != 1 && g_caveDetectCooldown == 0) {
            try {
                if (MapRenderState::g_caveTopYAuto) {
                    // Auto 模式: 按配置的自动天花板检测尺寸 (Smart / OFF / 1x1..9x9) 检测洞穴，并应用防闪烁计时器
                    BlockSource* regionPtr = this->getRegion();
                    if (regionPtr) {
                        int caveStartY = 0;
                        bool inCave = DetectCaveStartWithRoofSize(
                            *regionPtr, px, (int)g_playerY, pz, MapRenderState::g_autoCaveMode, caveStartY);
                        if (inCave != MapRenderState::g_caveModeActive) {
                            int toggleDelayMs = std::clamp(MapRenderState::g_caveModeToggleTimer, 0, 5000);
                            bool canToggleNow = (toggleDelayMs <= 0);
                            if (!canToggleNow) {
                                auto nowTp = std::chrono::steady_clock::now();
                                if (!MapRenderState::g_caveHasPendingToggle || MapRenderState::g_cavePendingTargetState != inCave) {
                                    MapRenderState::g_caveHasPendingToggle = true;
                                    MapRenderState::g_cavePendingTargetState = inCave;
                                    MapRenderState::g_cavePendingSinceTime = nowTp;
                                } else {
                                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        nowTp - MapRenderState::g_cavePendingSinceTime).count();
                                    if (elapsedMs >= toggleDelayMs) {
                                        canToggleNow = true;
                                    }
                                }
                            }
                            if (canToggleNow) {
                                MapRenderState::g_caveHasPendingToggle = false;
                                // [防残留] 洞穴→地表切换时强制触发重扫, 清除缓存中的旧洞穴数据
                                if (MapRenderState::g_caveModeActive && !inCave) {
                                    g_ticksSinceScan = 101;
                                }
                                MapRenderState::g_caveModeActive = inCave;
                                if (inCave) {
                                    MapRenderState::g_caveStartY = caveStartY;
                                }
                            }
                        } else {
                            MapRenderState::g_caveHasPendingToggle = false;
                            if (inCave) {
                                MapRenderState::g_caveStartY = caveStartY;
                            }
                        }
                    }
                    g_caveDetectCooldown = MapRenderState::g_caveHasPendingToggle ? 1 : 5;
                } else {
                    // 手动 Top Y 模式: 始终启用洞穴, 使用用户指定的 Top Y
                    MapRenderState::g_caveHasPendingToggle = false;
                    MapRenderState::g_caveModeActive = true;
                    MapRenderState::g_caveStartY = MapRenderState::g_caveTopY;
                    g_caveDetectCooldown = 5;
                }
            } catch (...) {
                MapRenderState::g_caveModeActive = false;
                g_caveDetectCooldown = 5;
            }
        } else if (effectiveCaveType == 0 && MapRenderState::currentDimensionId != 1) {
            // Off 模式或当前维度未列入洞穴白名单: 不启用洞穴渲染
            MapRenderState::g_caveHasPendingToggle = false;
            MapRenderState::g_caveModeActive = false;
            // [防Off模式地表污染] 仍需检测玩家是否在地下洞穴中
            // 原因: SafeGetSurfaceY 在地下时可能返回洞穴天花板 Y (区块部分加载),
            //       导致地表扫描读取石头方块, 扫描完成后 UpdateFromScan 永久写入缓存 → 灰石污染
            // 修复: 检测到地下时中止地表扫描, 保留缓存中的纯净地表数据
            if (g_caveDetectCooldown == 0) {
                try {
                    BlockSource* regionPtr = this->getRegion();
                    if (regionPtr) {
                        int detectY = 0;
                        bool inCave = DetectCaveStart(*regionPtr, px, (int)g_playerY, pz, detectY);
                        if (inCave && g_isScanning) {
                            // 玩家在地下但洞穴模式关闭: 中止地表扫描, 防止读取洞穴天花板石头
                            AbortActiveScanAndPendingWrites();
                            g_ticksSinceScan = 60;  // ~3秒后重试, 等玩家可能离开洞穴
                        }
                    }
                } catch (...) {}
                g_caveDetectCooldown = 5;
            }
        }

        // [维度切换] 维度变化时立即彻底中止当前扫描与待写缓冲, 避免跨维度数据混合
        if (g_scanPrevDimension != MapRenderState::currentDimensionId || (g_isScanning && g_scanDimension != MapRenderState::currentDimensionId)) {
            g_scanPrevDimension = MapRenderState::currentDimensionId;
            g_scanPrevCaveActive = MapRenderState::g_caveModeActive;  // 同步, 避免下帧再次触发模式切换中止
            AbortActiveScanAndPendingWrites();
        }

        // [防地表污染核心修复] 洞穴模式切换时, 立即中止当前进行中的扫描
        // 原因: 洞穴扫描与地表扫描是 if/else if 互斥分支。若切换发生在扫描中途,
        //   已扫部分是旧模式数据(如洞穴灰石 alpha=1), 后半段是新模式数据,
        //   完成后整份混合数据被 UpdateFromScan 写入缓存 → 永久灰黑块污染。
        // 修复: 检测到 g_caveModeActive 翻转时, 丢弃当前半成品扫描, 下帧重新开始纯模式扫描。
        if (g_scanPrevCaveActive != MapRenderState::g_caveModeActive) {
            g_scanPrevCaveActive = MapRenderState::g_caveModeActive;
            AbortActiveScanAndPendingWrites();

            // [洞穴/地表极速平滑切换] 从对应缓存预填并立即更新前台与纹理，彻底杜绝切换瞬间的黑块与延迟
            bool newIsCave = MapRenderState::g_caveModeActive;
            MapCacheManager::PrefillMapGrid(px, pz, g_mapColorsBack, g_mapHeightsBack, newIsCave, false, g_textureData, g_mapLightsBack);
            {
                std::lock_guard<std::mutex> lock(g_mapDataMutex);
                std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                g_lastRenderX = px;
                g_lastRenderZ = pz;
                g_textureCenterX = (float)px;
                g_textureCenterZ = (float)pz;
                g_textureReadyToUpload.store(true);
                g_mapDataUpdated.store(true);
            }
            g_currentScanX = px;
            g_currentScanZ = pz;
            g_isScanning = true;
            g_scanPhase = 0;
            g_scanRadius = 0;
            g_scanPerimeterIndex = 0;
        }

        // [洞穴扫描] 玩家在地下且洞穴模式启用时, 执行洞穴列扫描代替地表扫描
        // 从 caveStart 向下扫描 caveDepth 格 (或 Full 全深度模式扫描全列)
        if (g_isScanning && MapRenderState::g_caveModeActive && effectiveCaveType != 0) {
            try {
                BlockSource* regionPtr = this->getRegion();
                if (regionPtr) {
                    BlockSource& region = *regionPtr;
                    auto scanStartTime = std::chrono::high_resolution_clock::now();
                    bool timeBudgetExceeded = false;

                    // 洞穴扫描深度与是否 Full 全深度模式
                    int caveDepth = MapRenderState::g_caveDepth;
                    bool isFullCaveMode = (effectiveCaveType == (int)MapRenderState::CaveModeType::Full);

                    // 统一 Top Y 投影:
                    // Layered 模式下所有列从同一高度向下解析, 避免相邻列跳到不同高度层产生杂色条纹;
                    // Full 模式下忽略 Top Y，从地壳下方 (或下界顶棚 122) 贯穿扫描至世界底端 (-64)。
                    int currentCaveTopY;
                    int currentCaveScanDepth;
                    if (isFullCaveMode) {
                        currentCaveTopY = (MapRenderState::currentDimensionId == 1) ? 122 : 319;
                        currentCaveScanDepth = (MapRenderState::currentDimensionId == 1) ? 80 : std::max(30, caveDepth);
                    } else if (MapRenderState::currentDimensionId == 1) {
                        currentCaveTopY = std::clamp((int)g_playerY + 16, -64, 120);
                        currentCaveScanDepth = 80;
                    } else if (!MapRenderState::g_caveTopYAuto) {
                        currentCaveTopY = std::clamp(MapRenderState::g_caveTopY, -64, 319);
                        currentCaveScanDepth = caveDepth;
                    } else {
                        // Auto 模式: 紧随玩家所在洞穴层
                        // 上限以 playerY + 16 为基准；若局部通道天花板更低，则以天花板为界避开岩石层
                        int headroom = 16;
                        int targetTop = (int)g_playerY + headroom;
                        if (MapRenderState::g_caveStartY > (int)g_playerY && MapRenderState::g_caveStartY < targetTop) {
                            targetTop = MapRenderState::g_caveStartY;
                        }
                        currentCaveTopY = std::clamp(targetTop, -60, 319);
                        currentCaveScanDepth = caveDepth;
                    }

                    int minR = (g_scanPhase == 0) ? 0 : (SCAN_INNER_RADIUS + 1);
                    int maxR = (g_scanPhase == 0) ? SCAN_INNER_RADIUS : MAP_DATA_RADIUS;
                    if (g_scanRadius < minR) g_scanRadius = minR;

                    while (g_scanRadius <= maxR && !timeBudgetExceeded) {
                        int r = g_scanRadius;
                        int perimeter = (r == 0) ? 1 : (8 * r);

                        while (g_scanPerimeterIndex < perimeter) {
                            int dx = 0, dz = 0;
                            GetRingCoords(r, g_scanPerimeterIndex, dx, dz);

                            int targetX = g_currentScanX + dx;
                            int targetZ = g_currentScanZ + dz;
                            int arrX    = dx + MAP_DATA_RADIUS;
                            int arrZ    = dz + MAP_DATA_RADIUS;

                            int startY = currentCaveTopY;
                            if (isFullCaveMode && MapRenderState::currentDimensionId != 1) {
                                short surfY = SafeGetSurfaceY(region, targetX, targetZ);
                                startY = (surfY > -64 && surfY != -32000) ? (int)surfY : 319;
                            }
                            int scanDepth = currentCaveScanDepth;

                            std::string blockName;
                            int blockY = 0, depth = 0;
                            bool hasWater = false;

                            if (startY > -64 && ScanColumnCave(region, targetX, targetZ, startY, scanDepth, blockName, blockY, depth, hasWater, isFullCaveMode)) {
                                g_mapHeightsBack[arrX][arrZ] = (float)blockY;

                                // 深洞: 有空气通道但指定深度内无地板，渲染深灰避免泄露地表色
                                if (blockName == "__CAVE_DEEPHOLE__") {
                                    g_mapColorsBack[arrX][arrZ] = mce::Color(0.08f, 0.08f, 0.08f, 1.0f);
                                    g_mapLightsBack[arrX][arrZ] = 0x00;
                                } else {
                                    int intrinsicLight = BlockColorMap::GetIntrinsicBlockLight(blockName);
                                    g_mapLightsBack[arrX][arrZ] = SafeGetPackedLight(region, targetX, blockY + 1, targetZ, intrinsicLight, false);

                                    // 液体方块: 使用饱和颜色, 不应用深度衰减
                                    mce::Color liquidCol = GetCaveLiquidColor(blockName);
                                    if (liquidCol.a > 0.0f) {
                                        if (blockName.find("lava") != std::string::npos && MapRenderState::blockColorMode == 0) {
                                            liquidCol = ApplyLavaPatternTint(targetX, targetZ);
                                        }
                                        g_mapColorsBack[arrX][arrZ] = liquidCol;
                                    } else {
                                        // 实心方块: 使用洞穴专用颜色表 (含矿石/石头变种/基岩等)
                                        mce::Color baseColor = GetCaveBlockColor(blockName);

                                        // 水色叠加: 若列内含水体，叠加半透明水色
                                        if (hasWater) {
                                            baseColor = BlendWaterOverFloor(baseColor, mce::Color(0.15f, 0.45f, 0.90f, 1.0f));
                                        }

                                        // 应用深度亮度衰减
                                        float brightness = ComputeCaveBrightness(depth, scanDepth);
                                        g_mapColorsBack[arrX][arrZ] = mce::Color(
                                            baseColor.r * brightness,
                                            baseColor.g * brightness,
                                            baseColor.b * brightness,
                                            1.0f  // 洞穴模式 alpha=1 (完全不透明)
                                        );
                                    }
                                }
                            } else {
                                // 关键防黑块策略：若已存在有效像素（来自缓存预填或上一轮扫描），绝不覆盖为纯黑！
                                if (g_mapColorsBack[arrX][arrZ].a <= 0.01f) {
                                    g_mapColorsBack[arrX][arrZ] = mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
                                    g_mapHeightsBack[arrX][arrZ] = 0.0f;
                                    g_mapLightsBack[arrX][arrZ] = 0x00;
                                }
                            }

                            g_scanPerimeterIndex++;

                            if ((g_scanPerimeterIndex & 31) == 0) {
                                auto now = std::chrono::high_resolution_clock::now();
                                int maxMicroSec = (g_scanPhase == 0) ? 750 : 500;
                                if (std::chrono::duration_cast<std::chrono::microseconds>(now - scanStartTime).count() > maxMicroSec) {
                                    timeBudgetExceeded = true;
                                    break;
                                }
                            }
                        }

                        if (timeBudgetExceeded) break;

                        if (g_scanPerimeterIndex >= perimeter) {
                            g_scanPerimeterIndex = 0;
                            g_scanRadius++;
                        }
                    }

                    // 增量周期性提交 (每 80ms)，实现每秒 12 次高频平滑渲染，杜绝任何可见延迟
                    static auto s_lastCaveCommit = std::chrono::steady_clock::now();
                    auto nowCave = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(nowCave - s_lastCaveCommit).count() >= 80) {
                        s_lastCaveCommit = nowCave;
                        std::lock_guard<std::mutex> lock(g_mapDataMutex);
                        std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                        std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                        std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                        g_lastRenderX = g_currentScanX;
                        g_lastRenderZ = g_currentScanZ;
                        g_mapDataUpdated.store(true);
                    }

                    if (g_scanRadius > maxR) {
                        if (g_scanPhase == 0) {
                            // Phase 0 核心视野扫描完成：立即提交前台，切换至 Phase 1 外围扫描
                            {
                                std::lock_guard<std::mutex> lock(g_mapDataMutex);
                                std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                                std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                                std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                                g_lastRenderX = g_currentScanX;
                                g_lastRenderZ = g_currentScanZ;
                                g_mapDataUpdated.store(true);
                            }
                            // [极速优化] Phase 0 核心视野完成后，立即异步写入缓存，使大地图周边区块无需等待数十秒
                            if (g_scanDimension == MapRenderState::currentDimensionId &&
                                g_scanDimension == MapCacheManager::GetLoadedDimensionId()) {
                                SubmitCacheWrite(g_currentScanX, g_currentScanZ, g_scanDimension, true, g_scanBiomeEntries);
                            }
                            g_scanPhase = 1;
                            g_scanRadius = SCAN_INNER_RADIUS + 1;
                            g_scanPerimeterIndex = 0;
                        } else {
                            // Phase 1 全图扫描完成
                            g_isScanning = false;
                            g_scanPhase = 0;
                            g_scanRadius = 0;
                            g_scanPerimeterIndex = 0;
                            {
                                std::lock_guard<std::mutex> lock(g_mapDataMutex);
                                std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                                std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                                std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                                g_lastRenderX = g_currentScanX;
                                g_lastRenderZ = g_currentScanZ;
                                g_mapDataUpdated.store(true);
                            }

                            // [持久化] 异步写入缓存 (含下界/洞穴数据)
                            // [维度安全核验] 扫描完成提交前，严格确认维度未发生改变，杜绝跨维度提交
                            if (g_scanDimension == MapRenderState::currentDimensionId &&
                                g_scanDimension == MapCacheManager::GetLoadedDimensionId()) {
                                SubmitCacheWrite(g_currentScanX, g_currentScanZ, g_scanDimension, true, g_scanBiomeEntries);
                            }
                        }
                    }
                }
            } catch (...) {}
        } else if (g_isScanning && !MapRenderState::g_caveModeActive) {
        // ==================== 地表扫描 (原有逻辑) ====================
        // 玩家不在洞穴时执行正常地表扫描
        try {
                BlockSource* regionPtr = this->getRegion();
                if (regionPtr) {
                    BlockSource& region = *regionPtr;
                    int rowsThisFrame = 0;

                    static std::string s_lastBlockName = "";
                    static mce::Color s_lastBlockColor(0, 0, 0, 0);
                    static int s_biomeCellX = -99999;
                    static int s_biomeCellZ = -99999;
                    static std::string s_biomeName = "";
                    // [性能] 记录当前生物群系名称 hash，后续每格计算 cacheKey 时
                    // 直接复用该值，避免重复对 s_biomeName 做 hash（s_biomeName 每 4x4 格才变化一次）
                    static uint64_t s_biomeNameHash = 0;
                    static mce::Color s_cachedGrass(0,0,0,0), s_cachedFoliage(0,0,0,0), s_cachedWater(0,0,0,0);

                    auto scanStartTime = std::chrono::high_resolution_clock::now();
                    bool timeBudgetExceeded = false;

                    int minR = (g_scanPhase == 0) ? 0 : (SCAN_INNER_RADIUS + 1);
                    int maxR = (g_scanPhase == 0) ? SCAN_INNER_RADIUS : MAP_DATA_RADIUS;
                    if (g_scanRadius < minR) g_scanRadius = minR;

                    while (g_scanRadius <= maxR && !timeBudgetExceeded) {
                        int r = g_scanRadius;
                        int perimeter = (r == 0) ? 1 : (8 * r);

                        while (g_scanPerimeterIndex < perimeter) {
                            int dx = 0, dz = 0;
                            GetRingCoords(r, g_scanPerimeterIndex, dx, dz);

                            int targetX = g_currentScanX + dx;
                            int targetZ = g_currentScanZ + dz;
                            int arrX    = dx + MAP_DATA_RADIUS;
                            int arrZ    = dz + MAP_DATA_RADIUS;

                            // [安全] 使用 SafeGetSurfaceY (SEH 包装)，防止部分加载区块 AV 崩溃
                            short topY = SafeGetSurfaceY(region, targetX, targetZ);

                            // [防黑块核心] 只有在合法世界高度区间才更新高度与方块采样；区块未就绪时绝不置零或损坏已有地形
                            if (topY > -64 && topY < 320) {
                                int highestLightSampleY = (int)topY;
                                int maxIntrinsicLight = 0;

                                g_mapHeightsBack[arrX][arrZ] = (float)topY;
                                Block const& block = region.getBlock(BlockPos(targetX, topY - 1, targetZ));
                                Block const* currentBlockPtr = &block;
                                std::string blockName = currentBlockPtr->getTypeName();

                                // [屏障/隐形方块/禁用染色玻璃/禁用花朵向下穿透]
                                while (topY > -64 && (
                                    (currentBlockPtr && currentBlockPtr->isAir()) ||
                                    IsInvisibleOrTechnicalOverlay(blockName) ||
                                    (!MapRenderState::loadFlowers && BlockColorMap::IsFlowerBlock(blockName)) ||
                                    (!MapRenderState::displayStainedGlass && BlockColorMap::IsGlassOrStainedGlassBlock(blockName)))) {
                                    topY--;
                                    try {
                                        currentBlockPtr = &region.getBlock(BlockPos(targetX, topY - 1, targetZ));
                                        blockName = currentBlockPtr->getTypeName();
                                    } catch (...) {
                                        break;
                                    }
                                }
                                g_mapHeightsBack[arrX][arrZ] = (float)topY;

                                // [防地表灰石污染] 检测到石头类方块时, 回退查缓存地表Y重新读取
                                if (IsStoneLikeBlock(blockName)) {
                                    int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(targetX, targetZ);
                                    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > (int16_t)topY + 15) {
                                        std::string surfName;
                                        if (SafeGetBlockName(region, targetX, cachedY - 1, targetZ, surfName) && !surfName.empty()) {
                                            topY = cachedY;
                                            g_mapHeightsBack[arrX][arrZ] = (float)topY;
                                            blockName = surfName;
                                            try {
                                                currentBlockPtr = &region.getBlock(BlockPos(targetX, cachedY - 1, targetZ));
                                            } catch (...) {}
                                        }
                                    }
                                }

                                struct OverlayLayer {
                                    mce::Color color;
                                };
                                OverlayLayer overlays[8];
                                int overlayCount = 0;
                                int waterDepth = 0;
                                std::string lastOverlayName = "";

                                // 1. 检查 topY 位置的上层附着方块 (雪片、花朵、草丛、地毯、玻璃板等)
                                if (topY < 320 && blockName.find("snow") == std::string::npos) {
                                    try {
                                        Block const& blockAbove = region.getBlock(BlockPos(targetX, topY, targetZ));
                                        std::string aboveName = blockAbove.getTypeName();
                                        if (blockAbove.isAir() ||
                                            IsInvisibleOrTechnicalOverlay(aboveName) ||
                                            (!MapRenderState::loadFlowers && BlockColorMap::IsFlowerBlock(aboveName)) ||
                                            (!MapRenderState::displayStainedGlass && BlockColorMap::IsGlassOrStainedGlassBlock(aboveName))) {
                                            // 忽略隐形方块或已关闭显示的花朵/染色玻璃
                                        } else {
                                            int abvLight = BlockColorMap::GetIntrinsicBlockLight(aboveName);
                                            if (abvLight > maxIntrinsicLight) maxIntrinsicLight = abvLight;

                                            mce::Color aboveOvCol;
                                            float aboveOvAlpha = 0.0f;
                                            if (BlockColorMap::GetTranslucentOverlayColor(aboveName, aboveOvCol, aboveOvAlpha)) {
                                                aboveOvCol.a = aboveOvAlpha;
                                                overlays[overlayCount++] = { aboveOvCol };
                                                lastOverlayName = aboveName;
                                            } else {
                                                blockName = aboveName;
                                                currentBlockPtr = &blockAbove;
                                            }
                                        }
                                    } catch (...) {}
                                }

                                // 2. 多层半透明方块叠加 (Overlay) 与水层深度穿透系统：
                                // 当遇到半透明方块 (染色玻璃/冰/黏液块/蜂蜜块等) 或水体时，收集半透明层并继续向下寻找实心底板
                                int floorY = (int)topY - 1;
                                while (floorY > -64) {
                                    int curIntrinsic = BlockColorMap::GetIntrinsicBlockLight(blockName);
                                    if (curIntrinsic > maxIntrinsicLight) maxIntrinsicLight = curIntrinsic;

                                    mce::Color ovCol;
                                    float ovAlpha = 0.0f;

                                    if ((currentBlockPtr && currentBlockPtr->isAir()) ||
                                        IsInvisibleOrTechnicalOverlay(blockName) ||
                                        (!MapRenderState::loadFlowers && BlockColorMap::IsFlowerBlock(blockName)) ||
                                        (!MapRenderState::displayStainedGlass && BlockColorMap::IsGlassOrStainedGlassBlock(blockName))) {
                                        floorY--;
                                    } else if (currentBlockPtr && IsCaveWaterBlock(*currentBlockPtr)) {
                                        waterDepth++;
                                        floorY--;
                                    } else if (BlockColorMap::GetTranslucentOverlayColor(blockName, ovCol, ovAlpha)) {
                                        ovCol.a = ovAlpha;
                                        if (blockName != lastOverlayName && overlayCount < 8) {
                                            overlays[overlayCount++] = { ovCol };
                                            lastOverlayName = blockName;
                                        }
                                        floorY--;
                                    } else if (waterDepth > 0 && currentBlockPtr && IsCavePassableBlock(*currentBlockPtr)) {
                                        floorY--;
                                    } else if (overlayCount > 0 && currentBlockPtr && (
                                               GetCaveMaterialType(*currentBlockPtr) == ChiyanMapMaterialType::Air ||
                                               GetCaveMaterialType(*currentBlockPtr) == ChiyanMapMaterialType::Plant ||
                                               GetCaveMaterialType(*currentBlockPtr) == ChiyanMapMaterialType::NonSolid ||
                                               GetCaveMaterialType(*currentBlockPtr) == ChiyanMapMaterialType::StructureVoid ||
                                               GetCaveMaterialType(*currentBlockPtr) == ChiyanMapMaterialType::Fire)) {
                                        // 半透明玻璃下方的空气、矮草/藤蔓、火把/按钮/拉杆/绊线等非实心附着物继续向下穿透，同时保留下方的树冠、原木与地毯
                                        floorY--;
                                    } else {
                                        break;
                                    }
                                    try {
                                        currentBlockPtr = &region.getBlock(BlockPos(targetX, floorY, targetZ));
                                        blockName = currentBlockPtr->getTypeName();
                                    } catch (...) {
                                        break;
                                    }
                                }

                                // 更新用于地貌阴影计算的高度值
                                if (waterDepth > 0) {
                                    g_mapHeightsBack[arrX][arrZ] = (float)floorY;
                                } else if (MapRenderState::adjustHeightForShortBlocks && MapRenderState::IsShortBlock(blockName)) {
                                    g_mapHeightsBack[arrX][arrZ] = (float)floorY;
                                } else {
                                    g_mapHeightsBack[arrX][arrZ] = (float)(floorY + 1);
                                }

                                // 更新生物群系染色缓存 (4x4 网格)
                                int cellX = targetX >> 2;
                                int cellZ = targetZ >> 2;
                                if (cellX != s_biomeCellX || cellZ != s_biomeCellZ) {
                                    s_biomeCellX = cellX; s_biomeCellZ = cellZ;
                                    try {
                                        auto const& biome = region.getBiome(BlockPos(targetX, std::clamp(floorY, -64, 319), targetZ));
                                        std::string newBiomeName = biome.mHash->getString();
                                        if (s_biomeName != newBiomeName) {
                                            s_biomeName = newBiomeName;
                                            s_biomeNameHash = Fnv1aHash(s_biomeName);
                                            getBiomeTints(s_biomeName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                        }
                                        g_scanBiomeEntries.push_back({cellX, cellZ, newBiomeName});
                                    } catch (...) {
                                        if (!s_biomeName.empty()) { s_biomeName = ""; s_biomeNameHash = 0; }
                                    }
                                }

                                // 3. 计算底层实心方块的基础颜色
                                mce::Color finalColor(0.08f, 0.08f, 0.08f, 1.0f);
                                bool curIsSwamp = IsSwampDualToneBiome(s_biomeName);
                                BiomeTintTriple curFallbackTriple{s_cachedGrass, s_cachedFoliage, s_cachedWater, curIsSwamp};
                                if (curIsSwamp) {
                                    curFallbackTriple.grass = ComputeSwampGrassColor(targetX, targetZ, MapRenderState::biomeBlending);
                                }
                                mce::Color bGrass = curFallbackTriple.grass, bFoliage = s_cachedFoliage, bWater = s_cachedWater;
                                bool blendedComputed = false;

                                if (blockName.find("snow") != std::string::npos && MapRenderState::blockColorMode == 0) {
                                    finalColor = mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
                                } else if (blockName.find("lava") != std::string::npos &&
                                           blockName.find("cauldron") == std::string::npos &&
                                           MapRenderState::blockColorMode == 0) {
                                    finalColor = ApplyLavaPatternTint(targetX, targetZ);
                                } else if (!blockName.empty()) {
                                    if (currentBlockPtr && IsPillarLogOrStem(blockName)) {
                                        if (IsBlockPillarHorizontal(*currentBlockPtr)) {
                                            blockName = GetHorizontalWoodName(blockName);
                                        }
                                    }

                                    if (IsBiomeTintedBlock(blockName)) {
                                        GetBlendedBiomeTints(region, targetX, targetZ, (short)std::clamp(floorY, -64, 319), curFallbackTriple, bGrass, bFoliage, bWater);
                                        blendedComputed = true;
                                        finalColor = getBlockColor(blockName, bGrass, bFoliage, bWater);
                                    } else {
                                        static std::unordered_map<uint64_t, mce::Color> s_globalColorCache;
                                        static int s_lastColorMode = -1;
                                        static bool s_lastVanillaBiome = true;
                                        if (s_lastColorMode != MapRenderState::blockColorMode ||
                                            s_lastVanillaBiome != MapRenderState::biomeColorsInVanilla ||
                                            s_globalColorCache.size() > 65536) {
                                            s_globalColorCache.clear();
                                            s_lastColorMode = MapRenderState::blockColorMode;
                                            s_lastVanillaBiome = MapRenderState::biomeColorsInVanilla;
                                        }

                                        uint64_t blockHash = Fnv1aHash(blockName);
                                        auto it = s_globalColorCache.find(blockHash);
                                        if (it != s_globalColorCache.end()) {
                                            finalColor = it->second;
                                        } else {
                                            mce::Color calculatedColor = getBlockColor(blockName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                            s_globalColorCache[blockHash] = calculatedColor;
                                            finalColor = calculatedColor;
                                        }
                                    }
                                }

                                // 4. 水体叠加 (支持水深透明度动态渐变 + 生物群系水色混合 + 原版模式固定水色)
                                if (waterDepth > 0) {
                                    mce::Color waterCol = s_cachedWater;
                                    if (MapRenderState::blockColorMode == 1 && !MapRenderState::biomeColorsInVanilla) {
                                        waterCol = mce::Color(64.0f / 255.0f, 64.0f / 255.0f, 1.0f, 1.0f);
                                    } else if (MapRenderState::biomeBlending) {
                                        if (!blendedComputed) {
                                            GetBlendedBiomeTints(region, targetX, targetZ, (short)std::clamp(floorY, -64, 319), curFallbackTriple, bGrass, bFoliage, bWater);
                                            blendedComputed = true;
                                        }
                                        waterCol = bWater;
                                    }
                                    float waterAlpha = std::clamp(0.48f + (float)waterDepth * 0.022f, 0.50f, 0.86f);
                                    finalColor = mce::Color(
                                        finalColor.r * (1.0f - waterAlpha) + waterCol.r * waterAlpha,
                                        finalColor.g * (1.0f - waterAlpha) + waterCol.g * waterAlpha,
                                        finalColor.b * (1.0f - waterAlpha) + waterCol.b * waterAlpha,
                                        1.0f
                                    );
                                }

                                // 5. 多层半透明方块由下至上逐层 Alpha 混合 (染色玻璃、冰、黏液块、蜂蜜块等)
                                if (overlayCount > 0 && finalColor.a <= 0.01f) {
                                    // 若底层一直到世界底部均为空气，使用柔和深色基底而非纯黑，确保半透明玻璃质感可见
                                    finalColor = mce::Color(0.22f, 0.25f, 0.22f, 1.0f);
                                }
                                float remainingBgWeight = 1.0f;
                                for (int ovIdx = overlayCount - 1; ovIdx >= 0; --ovIdx) {
                                    const mce::Color& ov = overlays[ovIdx].color;
                                    float a = std::clamp(ov.a, 0.0f, 0.45f);
                                    // 多层半透明块叠加时限制累积不透明度上限 (最高不超过 52%，始终保留至少 48% 底层地形细节)
                                    if (remainingBgWeight * (1.0f - a) < 0.48f) {
                                        a = std::max(0.0f, 1.0f - (0.48f / std::max(remainingBgWeight, 0.01f)));
                                    }
                                    remainingBgWeight *= (1.0f - a);
                                    finalColor.r = finalColor.r * (1.0f - a) + ov.r * a;
                                    finalColor.g = finalColor.g * (1.0f - a) + ov.g * a;
                                    finalColor.b = finalColor.b * (1.0f - a) + ov.b * a;
                                    finalColor.a = 1.0f;
                                }

                                g_mapColorsBack[arrX][arrZ] = finalColor;

                                // 6. 采样方块光照与天空光照 (供大地图/小地图光照与昼夜系统使用)
                                int floorIntrinsic = BlockColorMap::GetIntrinsicBlockLight(blockName);
                                if (floorIntrinsic > maxIntrinsicLight) maxIntrinsicLight = floorIntrinsic;
                                uint8_t lightFloor = SafeGetPackedLight(region, targetX, std::clamp(floorY + 1, -64, 319), targetZ, maxIntrinsicLight, true);
                                if (waterDepth > 0 || overlayCount > 0) {
                                    uint8_t lightTop = SafeGetPackedLight(region, targetX, std::clamp(highestLightSampleY, -64, 319), targetZ, maxIntrinsicLight, true);
                                    int skyMax = std::max((lightFloor >> 4) & 0x0F, (lightTop >> 4) & 0x0F);
                                    int blkMax = std::max(lightFloor & 0x0F, lightTop & 0x0F);
                                    g_mapLightsBack[arrX][arrZ] = (uint8_t)((skyMax << 4) | blkMax);
                                } else {
                                    g_mapLightsBack[arrX][arrZ] = lightFloor;
                                }
                            } else {
                                // topY <= -64 或 -32000 (区块未就绪/加载中):
                                // 关键防黑块策略：绝不抹杀已有的有效像素！保留预填充或上一轮扫描的地表数据！
                            }

                            g_scanPerimeterIndex++;

                            if ((g_scanPerimeterIndex & 31) == 0) {
                                auto now = std::chrono::high_resolution_clock::now();
                                int maxMicroSec = (g_scanPhase == 0) ? 750 : 500;
                                if (std::chrono::duration_cast<std::chrono::microseconds>(now - scanStartTime).count() > maxMicroSec) {
                                    timeBudgetExceeded = true;
                                    break;
                                }
                            }
                        }

                        if (timeBudgetExceeded) break;

                        if (g_scanPerimeterIndex >= perimeter) {
                            g_scanPerimeterIndex = 0;
                            g_scanRadius++;
                        }
                    }

                    // 增量周期性提交 (每 80ms)，实现每秒 12 次高频平滑渲染，杜绝任何可见延迟
                    static auto s_lastSurfaceCommit = std::chrono::steady_clock::now();
                    auto nowSurface = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(nowSurface - s_lastSurfaceCommit).count() >= 80) {
                        s_lastSurfaceCommit = nowSurface;
                        std::lock_guard<std::mutex> lock(g_mapDataMutex);
                        std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                        std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                        std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                        g_lastRenderX = g_currentScanX;
                        g_lastRenderZ = g_currentScanZ;
                        g_mapDataUpdated.store(true);
                    }

                    if (g_scanRadius > maxR) {
                        if (g_scanPhase == 0) {
                            // Phase 0 核心视野扫描完成：立即原子提交前台，切换至 Phase 1 外围扫描
                            {
                                std::lock_guard<std::mutex> lock(g_mapDataMutex);
                                std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                                std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                                std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                                g_lastRenderX = g_currentScanX;
                                g_lastRenderZ = g_currentScanZ;
                                g_mapDataUpdated.store(true);
                            }

                            // [极速优化] Phase 0 核心视野完成后，防污染核验后立即异步提交写入缓存，大地图秒级同步
                            bool playerUndergroundP0 = false;
                            if (MapRenderState::currentDimensionId != 1) {
                                try {
                                    int detectY = 0;
                                    playerUndergroundP0 = DetectCaveStart(region, px, (int)g_playerY, pz, detectY);
                                } catch (...) {}
                            }
                            if (!playerUndergroundP0) {
                                if (g_scanDimension == MapRenderState::currentDimensionId &&
                                    g_scanDimension == MapCacheManager::GetLoadedDimensionId()) {
                                    SubmitCacheWrite(g_currentScanX, g_currentScanZ, g_scanDimension, false, g_scanBiomeEntries);
                                }
                            }

                            g_scanPhase = 1;
                            g_scanRadius = SCAN_INNER_RADIUS + 1;
                            g_scanPerimeterIndex = 0;
                        } else {
                            // Phase 1 全图扫描完成
                            g_isScanning = false;
                            g_scanPhase = 0;
                            g_scanRadius = 0;
                            g_scanPerimeterIndex = 0;

                            // [防污染最终防线] 扫描完成时再次检测玩家是否在地下
                            // 若在地下, 跳过本次扫描数据的提交 (g_mapColors 和缓存), 保留之前的地表数据
                            // 原因: 即使 Off 模式有中途中止检测 (每10帧), 扫描仍可能在检测冷却期内完成,
                            //       将洞穴天花板石头数据写入缓存造成永久污染
                            bool playerUnderground = false;
                            if (MapRenderState::currentDimensionId != 1) {
                                try {
                                    int detectY = 0;
                                    playerUnderground = DetectCaveStart(region, px, (int)g_playerY, pz, detectY);
                                } catch (...) {}
                            }

                            if (!playerUnderground) {
                                // [跨维度安全检查] 必须确保扫描开始时的维度与当前维度以及缓存加载的维度完全一致
                                if (g_scanDimension == MapRenderState::currentDimensionId &&
                                    g_scanDimension == MapCacheManager::GetLoadedDimensionId()) {
                                    {
                                        std::lock_guard<std::mutex> lock(g_mapDataMutex);
                                        std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                                        std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                                        std::memcpy(g_mapLights, g_mapLightsBack, sizeof(g_mapLights));
                                        g_lastRenderX = g_currentScanX;
                                        g_lastRenderZ = g_currentScanZ;
                                        g_mapDataUpdated.store(true);
                                    }

                                    // [性能] 使用持久化 worker 线程，避免每次扫描完成时创建线程 + 5MB 堆分配
                                    SubmitCacheWrite(g_currentScanX, g_currentScanZ, g_scanDimension, false, g_scanBiomeEntries);
                                }
                            } else {
                                // 玩家在地下: 丢弃本次地表扫描数据, 防止洞穴天花板石头污染地表缓存;
                                // 同时若洞穴模式开启，立即激活洞穴模式并强制下帧启动洞穴扫描
                                if (effectiveCaveType != 0) {
                                    MapRenderState::g_caveModeActive = true;
                                    g_ticksSinceScan = 101;
                                }
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        static int entityDelay = 0;
        if (++entityDelay >= 4) { // 提高刷新率至约 5Hz (原60为3秒/次，移动严重跳变)，近乎零开销实现丝滑雷达
            entityDelay = 0;
            bool anyRadarCategory = MapRenderState::radarShowPlayers || MapRenderState::radarShowHostile ||
                                    MapRenderState::radarShowFriendly || MapRenderState::radarShowItems;
            if ((MapRenderState::showRadar || g_tabHeld || MapRenderState::showBigMap || MapRenderState::bigMapShowEntities) && anyRadarCategory) {
                std::vector<RadarEntity> tempEntities;
                auto& level = player->getLevel();
                const auto& entities = level.getRuntimeActorList();
                
                for (auto* actor : entities) {
                    if (!actor || IsLocalRadarActor(*this, *player, *actor)) continue;
                    if (!actor->isAlive()) continue;
                    if (actor->getDimensionId() != player->getDimensionId()) continue;

                    const Vec3& ePos = actor->getPosition();
                    float dx = ePos.x - pos.x;
                    float dz = ePos.z - pos.z;

                    if (dx * dx + dz * dz > MAP_DATA_RADIUS * MAP_DATA_RADIUS) continue;

                    int type = 2; 
                    std::string entityType;
                    std::string uuid;
                    if (IsRadarPlayerLike(*actor)) {
                        if (!MapRenderState::radarShowPlayers) continue;
                        type = 0;
                        entityType = "player";
                        if (actor->isPlayer()) {
                            auto* p = static_cast<class Player*>(actor);
                            uuid = static_cast<std::string>(p->getUuid());
                            ExtractPlayerSkinHead(p, uuid);
                        }
                    } else if (actor->hasCategory(ActorCategory::Item)) {
                        if (!MapRenderState::radarShowItems) continue;
                        type = 3;
                        entityType = "item";
                    } else if (actor->hasCategory(ActorCategory::Monster)) {
                        if (!MapRenderState::radarShowHostile) continue;
                        type = 1;
                        try { entityType = actor->getTypeName(); } catch (...) {}
                    } else {
                        if (!MapRenderState::radarShowFriendly) continue;
                        type = 2;
                        try { entityType = actor->getTypeName(); } catch (...) {}
                    }
                    
                    std::string nameTag;
                    try {
                        nameTag = actor->getNameTag();
                    } catch (...) {}

                    tempEntities.push_back({ePos.x, ePos.y, ePos.z, type, std::move(entityType), std::move(nameTag), std::move(uuid)});
                }
                {
                    std::lock_guard<std::mutex> lock(g_radarMutex);
                    g_radarEntities = std::move(tempEntities);
                }
                g_radarUpdated.store(true);
                g_radarGeneration.fetch_add(1, std::memory_order_release);
            } else {
                bool hadEntities = false;
                {
                    std::lock_guard<std::mutex> lock(g_radarMutex);
                    if (!g_radarEntities.empty()) {
                        g_radarEntities.clear();
                        hadEntities = true;
                    }
                }
                if (hadEntities) {
                    g_radarUpdated.store(true);
                    g_radarGeneration.fetch_add(1, std::memory_order_release);
                }
            }
        }

    } else {
        g_hasPlayer   = false;
        g_localPlayer = nullptr;
        g_localPlayerUuid.clear();
        {
            std::lock_guard<std::mutex> lock(g_playerSkinMutex);
            g_playerSkinHeads.clear();
        }
        clearPlayerHeadTextures.store(true);
        MapRenderState::showSeedMap = false;
        SeedMapManager::ClearWorldContext();
    }

    return result;
}

// ==========================================
// [大地图模式] 视角与交互底层物理锁死模块
// ==========================================

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerApplyTurnDeltaHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::_applyTurnDelta,
    void,
    Vec2 const& rotationDelta
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(rotationDelta); return; }
    if (MapRenderState::IsUIActive()) return;
    origin(rotationDelta);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeStartDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$startDestroyBlock,
    bool,
    BlockPos const& pos,
    unsigned char face,
    bool& isDestroyed
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(pos, face, isDestroyed);
    if (MapRenderState::IsUIActive()) return false;
    return origin(pos, face, isDestroyed);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItem,
    bool,
    ItemStack& item,
    HandSlot handSlot
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item, handSlot);
    if (MapRenderState::IsUIActive()) return false;
    return origin(item, handSlot);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerPickBlockHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::pickBlock,
    void,
    HitResult const& hitResult,
    bool withData
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(hitResult, withData); return; }
    if (MapRenderState::IsUIActive()) return;
    origin(hitResult, withData);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerInventorySelectSlotHook,
    ll::memory::HookPriority::Normal,
    PlayerInventory,
    &PlayerInventory::selectSlot,
    bool,
    int slot,
    ContainerID containerId
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(slot, containerId);
    if (MapRenderState::IsUIActive()) return false;
    return origin(slot, containerId);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerInventorySetItemHook,
    ll::memory::HookPriority::Normal,
    Inventory,
    &Inventory::$setItem,
    void,
    int slot,
    ItemStack const& item
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(slot, item); return; }
    if (MapRenderState::IsUIActive()) return;
    origin(slot, item);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeAttackHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$attack,
    bool,
    Actor& actor,
    Vec3 const& hitPosition
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(actor, hitPosition);
    if (MapRenderState::IsUIActive()) return false;
    return origin(actor, hitPosition);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerSwingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$swing,
    bool,
    ActorSwingSource swingSource,
    HandSlot handSlot
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(swingSource, handSlot);
    if (MapRenderState::IsUIActive()) return false;
    return origin(swingSource, handSlot);
}

// 【彻底阻断打空气音效】当模组 UI 激活时，拦截打空/未击中逻辑，消除 AttackNoDamage 音效与发包
LL_TYPE_INSTANCE_HOOK(
    LocalPlayerMissedSwingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::missedSwing,
    void
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(); return; }
    if (MapRenderState::IsUIActive()) return;
    origin();
}

LL_TYPE_INSTANCE_HOOK(
    GameModeInteractHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$interact,
    bool,
    Actor& entity,
    Vec3 const& location,
    HandSlot handSlot
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(entity, location, handSlot);
    if (MapRenderState::IsUIActive()) return false;
    return origin(entity, location, handSlot);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemAsAttackHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItemAsAttack,
    bool,
    ItemStack& item,
    Vec3 const& aimDirection,
    HandSlot handSlot
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item, aimDirection, handSlot);
    if (MapRenderState::IsUIActive()) return false;
    return origin(item, aimDirection, handSlot);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemOnHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItemOn,
    InteractionResult,
    ItemStack& item,
    BlockPos const& pos,
    unsigned char face,
    Vec3 const& hitPos,
    HandSlot handSlot,
    Block const* block,
    bool isFirstEvent
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item, pos, face, hitPos, handSlot, block, isFirstEvent);
    if (MapRenderState::IsUIActive()) return InteractionResult{false, false};
    return origin(item, pos, face, hitPos, handSlot, block, isFirstEvent);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeContinueDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$continueDestroyBlock,
    bool,
    BlockPos const& pos,
    unsigned char face,
    Vec3 const& playerPos,
    bool& hasDestroyedBlock
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(pos, face, playerPos, hasDestroyedBlock);
    if (MapRenderState::IsUIActive()) return false;
    return origin(pos, face, playerPos, hasDestroyedBlock);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerJumpFromGroundHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::canJump,
    bool
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin();
    if (MapRenderState::IsUIActive() && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return false;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerSetSneakingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$setSneaking,
    void,
    bool isSneaking
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(isSneaking); return; }
    if (MapRenderState::IsUIActive() && g_localPlayer && this == g_localPlayer) return;
    origin(isSneaking);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerIsImmobileHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::$isImmobile,
    bool
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin();
    if (MapRenderState::IsUIActive() && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return true;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    MobSetCarriedItemHook,
    ll::memory::HookPriority::Normal,
    Actor,
    &Actor::$setCarriedItem,
    void,
    ItemStack const& item
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(item); return; }
    if (MapRenderState::IsUIActive() && g_localPlayer && this == (Actor*)g_localPlayer) return;
    origin(item);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeBaseUseItemHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::baseUseItem,
    bool,
    ItemStack const& item,
    HandSlot handSlot
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item, handSlot);
    if (MapRenderState::IsUIActive()) return false;
    return origin(item, handSlot);
}