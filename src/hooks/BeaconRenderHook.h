#pragma once
#include <windows.h>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <glm/glm.hpp>

#include <ll/api/memory/Hook.h>
#include <mc/client/renderer/game/LevelRendererCamera.h>
#include <mc/client/renderer/blockactor/BlockActorRenderDispatcher.h>
#include <mc/client/renderer/blockactor/BeaconRenderer.h>
#include <mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h>
#include <mc/world/level/block/actor/BeaconBeamSection.h>
#include <mc/world/level/block/actor/BeaconBlockActor.h>
#include <mc/world/level/block/actor/component/IVanillaRenderBlockActorComponent.h>
#include <mc/world/actor/ActorTerrainInterlockData.h>
#include <mc/world/phys/AABB.h>
#include <mc/client/renderer/BaseActorRenderContext.h>
#include <mc/client/game/IClientInstance.h>
#include <mc/client/player/LocalPlayer.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/deps/core/math/Color.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/Tick.h>

#include "state/MapRenderState.h"
#include "state/WaypointManager.h"
#include "state/DeathPointManager.h"

#pragma warning(push)
#pragma warning(disable: 4265)
// 原版信标渲染适配组件: 桥接航点数据至原版方块实体渲染管线
class VirtualBeaconRenderComponent : public IVanillaRenderBlockActorComponent {
public:
    BlockPos mPos;
    BeaconBlockActor mBeaconActor;
    AABB mAABB;
    ActorTerrainInterlockData mInterlockData;

    VirtualBeaconRenderComponent(BlockPos const& pos)
        : mPos(pos),
          mBeaconActor(pos, true),
          mAABB(Vec3((float)pos.x - 4.0f, -128.0f, (float)pos.z - 4.0f),
                Vec3((float)pos.x + 4.0f, 2048.0f, (float)pos.z + 4.0f))
    {
        mInterlockData.mRenderVisibilityState = ActorTerrainInterlockData::VisibilityState::Visible;
    }

    ~VirtualBeaconRenderComponent() = default;

    virtual ::BlockActor& getBlockActor() override { return mBeaconActor; }
    virtual ::BlockActor const& getBlockActor() const override { return mBeaconActor; }
    virtual ::BlockPos const& getBlockActorPosition() const override { return mPos; }
    virtual ::BlockActorType getBlockActorType() const override { return ::BlockActorType::Beacon; }
    virtual ::BlockActorRendererId getRendererId() const override { return ::BlockActorRendererId::Beacon; }
    virtual bool isPermanentlyRendered() const override { return true; }
    virtual bool isWithinRenderDistance(::Vec3 const&) const override { return true; }
    virtual bool hasAlphaLayer() const override { return true; }
    virtual float getShadowRadius(::BlockSource&) const override { return 0.0f; }
    virtual bool isInWorld() const override { return true; }
    virtual ::IVanillaRenderBlockActorComponent* getCrackEntity(::BlockSource&, ::BlockPos const&) override { return nullptr; }
    virtual ::AABB const& getAABB() const override { return mAABB; }
    virtual void setAABB(::AABB const& value) override { mAABB = value; }
    virtual ::ActorTerrainInterlockData& getEntityTerrainInterlockData() override { return mInterlockData; }
    virtual ::ActorTerrainInterlockData const& getEntityTerrainInterlockDataConst() const override { return mInterlockData; }
    virtual void _resetAABB() override {}
};
#pragma warning(pop)

// 安全获取游戏时钟周期: 隔离 SEH 结构化异常处理，避免对象展开冲突
inline uint64_t SafeGetWorldTick(BlockSource& region) {
    uint64_t tickId = 0;
    __try {
        tickId = region.getLevel().getCurrentTick().tickID;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        tickId = 0;
    }
    if (tickId == 0) {
        __try {
            tickId = (uint64_t)region.getLevel().getTime();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            tickId = 0;
        }
    }
    return tickId;
}

// 内部信标渲染实现函数：包含局部析构逻辑
inline void RenderNativeBeaconsImpl(::BaseActorRenderContext& renderContext, bool renderAlphaLayer) {
    if (MapRenderState::g_isShuttingDown.load()) return;
    if (!MapRenderState::show3DWaypoints) return;
    if (!MapRenderState::useNativeBeaconBeams) return;

    LocalPlayer* localPlayer = renderContext.mClientInstance.getLocalPlayer();
    if (!localPlayer) return;

    BlockSource& region = localPlayer->getDimensionBlockSource();
    auto& dispatcher = renderContext.mBlockEntityRenderDispatcher;

    int currentDim = MapRenderState::currentDimensionId;
    std::string curSet = WaypointManager::GetCurrentWaypointSet();

    struct BeaconRenderEntry {
        BlockPos pos;
        mce::Color color;
    };
    std::vector<BeaconRenderEntry> entries;

    // 收集常规航点
    {
        std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
        entries.reserve(WaypointManager::g_waypoints.size() + 2);
        for (const auto& wp : WaypointManager::g_waypoints) {
            if (!wp.enabled) continue;
            if (MapRenderState::onlyShowCurrentWaypointSet && !curSet.empty() && wp.set != curSet) continue;
            bool isCrossDim = false;
            if (wp.dimId == currentDim) {
                if (currentDim == 0) {
                    bool wpInCave = (wp.isCave == 1) || (wp.isCave == -1 && wp.y < 55);
                    if (MapRenderState::g_caveModeActive && !wpInCave) continue;
                    if (!MapRenderState::g_caveModeActive && wpInCave) continue;
                }
            } else if (currentDim == 1 && wp.dimId == 0 && MapRenderState::preferOverworldWaypoints) {
                isCrossDim = true;
            } else {
                continue;
            }

            if (isCrossDim) {
                int nx = (int)std::floor((float)wp.x / 8.0f);
                int nz = (int)std::floor((float)wp.z / 8.0f);
                entries.push_back({ BlockPos(nx, wp.y, nz), mce::Color(wp.r, wp.g, wp.b, 1.0f) });
            } else {
                entries.push_back({ BlockPos(wp.x, wp.y, wp.z), mce::Color(wp.r, wp.g, wp.b, 1.0f) });
            }
        }
    }

    // 收集死亡点 (若已启用)
    if (MapRenderState::show3DDeathpoints) {
        std::lock_guard<std::mutex> lock(DeathPointManager::g_deathMutex);
        for (const auto& dp : DeathPointManager::g_deathPoints) {
            if (dp.dimensionId != currentDim) continue;
            entries.push_back({ BlockPos(dp.x, dp.y, dp.z), mce::Color(1.0f, 0.22f, 0.22f, 1.0f) });
        }
    }

    if (entries.empty()) return;

    Vec3 playerPos = localPlayer->getPosition();
    float maxDist = (float)MapRenderState::waypoint3DMaxDistance;
    float maxDistSq = maxDist * maxDist;

    alignas(8) static char s_emptyMat[16]{};
    mce::MaterialPtr const& emptyForcedMat = *reinterpret_cast<mce::MaterialPtr const*>(s_emptyMat);

    // 动态时钟同步机制: 驱动原版信标光柱自转与能量流动纹理
    uint64_t tickId = SafeGetWorldTick(region);
    if (tickId == 0) {
        static auto s_start = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::steady_clock::now() - s_start;
        tickId = (uint64_t)(std::chrono::duration<float>(elapsed).count() * 20.0f);
    }
    int safeTick = (int)(tickId % 65536);
    float totalTime = (float)safeTick + renderContext.mFrameAlpha;

    BlockPos playerBlockPos((int)std::floor(playerPos.x), (int)std::floor(playerPos.y), (int)std::floor(playerPos.z));
    Block const* fallbackBlock = nullptr;
    if (region.getChunkAt(playerBlockPos)) {
        fallbackBlock = &region.getBlock(playerBlockPos);
    }

    for (const auto& entry : entries) {
        float dx = (float)entry.pos.x - playerPos.x;
        float dz = (float)entry.pos.z - playerPos.z;
        float distSq = dx * dx + dz * dz;
        if (maxDist > 0.0f && distSq > maxDistSq) continue;

        BlockPos beaconPos(entry.pos.x, std::max(-64, entry.pos.y - 1), entry.pos.z);

        Block const* blockPtr = nullptr;
        if (region.getChunkAt(beaconPos)) {
            blockPtr = &region.getBlock(beaconPos);
        } else if (region.getChunkAt(entry.pos)) {
            blockPtr = &region.getBlock(entry.pos);
        } else if (fallbackBlock) {
            blockPtr = fallbackBlock;
        }
        if (!blockPtr) continue;

        VirtualBeaconRenderComponent comp(beaconPos);
        comp.mBeaconActor.mBeamSections->clear();
        BeaconBeamSection section;
        section.mHeight = 1024;
        section.mColor = entry.color;
        comp.mBeaconActor.mBeamSections->push_back(section);
        comp.mBeaconActor.mNumLevels = 4;
        comp.mBeaconActor.mNumLevelsSet = 4;
        comp.mBeaconActor.mPermanentlyRendered = true;
        comp.mBeaconActor.mIsBlockBaseLoaded = true;
        comp.mBeaconActor.mClientRenderingNeedsUpdate = false;
        comp.mBeaconActor.mTickCount = safeTick;
        comp.mBeaconActor.mBeamRot = totalTime * 0.05f;

        dispatcher.render(
            renderContext,
            region,
            comp,
            *blockPtr,
            renderAlphaLayer,
            emptyForcedMat,
            nullptr,
            0,
            std::nullopt
        );
    }
}

// 捕获与信标光柱渲染管线严格同步的原版世界摄像机参数
inline void CaptureWorldCamera(LevelRendererCamera& rendererCam) {
    __try {
        mce::Camera& cam = rendererCam.mWorldSpaceCamera;
        glm::vec3 pos = cam.mPosition;
        glm::vec3 fwd = cam.mForward;
        glm::vec3 rgt = cam.mRight;
        glm::vec3 up  = cam.mUp;

        if (glm::dot(fwd, fwd) > 0.01f && glm::dot(rgt, rgt) > 0.01f) {
            fwd = glm::normalize(fwd);
            rgt = glm::normalize(rgt);
            up = (glm::dot(up, up) > 0.01f) ? glm::normalize(up) : glm::normalize(glm::cross(rgt, fwd));

            MapRenderState::g_worldCameraCapture.posX = pos.x;
            MapRenderState::g_worldCameraCapture.posY = pos.y;
            MapRenderState::g_worldCameraCapture.posZ = pos.z;
            MapRenderState::g_worldCameraCapture.fwdX = fwd.x;
            MapRenderState::g_worldCameraCapture.fwdY = fwd.y;
            MapRenderState::g_worldCameraCapture.fwdZ = fwd.z;
            MapRenderState::g_worldCameraCapture.rgtX = rgt.x;
            MapRenderState::g_worldCameraCapture.rgtY = rgt.y;
            MapRenderState::g_worldCameraCapture.rgtZ = rgt.z;
            MapRenderState::g_worldCameraCapture.upX  = up.x;
            MapRenderState::g_worldCameraCapture.upY  = up.y;
            MapRenderState::g_worldCameraCapture.upZ  = up.z;

            if (cam.mAspectRatio > 0.01f) {
                MapRenderState::g_worldCameraCapture.aspect = (float)cam.mAspectRatio;
            }
            float fovVal = cam.mFov;
            if (fovVal > 3.14159f) {
                MapRenderState::g_worldCameraCapture.fovRad = fovVal * (3.141592653589793f / 180.0f);
            } else if (fovVal > 0.1f) {
                MapRenderState::g_worldCameraCapture.fovRad = fovVal;
            }

            if (!cam.projectionMatrixStack->stack->empty()) {
                glm::mat4x4 const& P = *cam.projectionMatrixStack->top()._m;
                float p00 = std::abs(P[0][0]);
                float p11 = std::abs(P[1][1]);
                if (p00 > 0.01f && p11 > 0.01f) {
                    MapRenderState::g_worldCameraCapture.fx = p00;
                    MapRenderState::g_worldCameraCapture.fy = p11;
                    MapRenderState::g_worldCameraCapture.hasProj = true;
                }
            }
            MapRenderState::g_worldCameraCapture.frameId++;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// 挂钩原版摄像机方块实体渲染流程：覆盖不透明核心层与透明光晕层
LL_TYPE_INSTANCE_HOOK(
    LevelRendererCameraRenderBlockEntitiesHook,
    ll::memory::HookPriority::Normal,
    LevelRendererCamera,
    &LevelRendererCamera::$renderBlockEntities,
    void,
    ::BaseActorRenderContext& renderContext,
    bool renderAlphaLayer
) {
    origin(renderContext, renderAlphaLayer);

    __try {
        CaptureWorldCamera(*this);
        RenderNativeBeaconsImpl(renderContext, renderAlphaLayer);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // 安全异常捕获: 0 卡死，0 崩溃
    }
}

namespace BeaconRenderHook {
    inline void hook() {
        LevelRendererCameraRenderBlockEntitiesHook::hook();
    }
    inline void unhook() {
        LevelRendererCameraRenderBlockEntitiesHook::unhook();
    }
}
