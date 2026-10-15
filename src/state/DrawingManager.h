#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <imgui.h>

namespace DrawingManager {

    enum class DrawingToolType : int {
        Pen = 0,       // 自由画笔 (Polyline)
        Line = 1,      // 直线段 (Straight Line)
        Arrow = 2,     // 箭头射线 (Directed Arrow)
        Rect = 3,      // 矩形区域 (Rectangle)
        Circle = 4,    // 圆形/辐射圈 (Circle)
        Text = 5,      // 文字标注 (Text Annotation)
        Eraser = 6     // 橡皮擦 (Eraser)
    };

    struct WorldPoint2D {
        float x = 0.0f;
        float z = 0.0f;
    };

    struct DrawingElement {
        std::string id;
        DrawingToolType type = DrawingToolType::Pen;
        std::vector<WorldPoint2D> points; // Pen: 连续点集; Line/Arrow: [0]=起点, [1]=终点; Rect: [0]=角点1, [1]=角点2; Circle: [0]=圆心
        float radius = 0.0f;              // Circle 半径 (方块单位)
        float color[4] = { 1.0f, 0.25f, 0.25f, 0.90f }; // RGBA 颜色
        float thickness = 3.0f;           // 线条粗细 (像素)
        bool filled = false;              // Rect / Circle 是否填充内部
        float fillColor[4] = { 1.0f, 0.25f, 0.25f, 0.25f }; // 填充色
        std::string text;                 // 文字标注内容
        float fontSize = 16.0f;           // 文字字号
        uint64_t createdAt = 0;           // 创建时间戳 (ms)

        // AABB 边界盒 (世界坐标)，用于 O(1) 快速视口剔除与命中测试
        float minX = 0.0f;
        float maxX = 0.0f;
        float minZ = 0.0f;
        float maxZ = 0.0f;

        void RecomputeBounds();
        bool HitTest(float worldX, float worldZ, float toleranceWorld) const;
    };

    struct DrawingLayer {
        std::string id;
        std::string name;
        bool visible = true;
        bool locked = false;
        float opacity = 1.0f; // 0.05 ~ 1.0
        float themeColor[4] = { 0.20f, 0.70f, 1.0f, 1.0f };
        std::vector<DrawingElement> elements;
    };

    extern bool g_drawingModeActive;
    inline bool& s_drawingMode = g_drawingModeActive;

    extern DrawingToolType g_currentTool;
    extern float g_brushColor[4];
    extern float g_brushThickness;
    extern bool g_shapeFilled;
    extern float g_fillColor[4];
    extern float g_eraserRadius;
    extern float g_textFontSize;
    extern std::string g_activeLayerId;
    extern std::mutex g_drawingMutex;

    // 交互绘制状态
    extern bool g_isDrawingNow;
    extern WorldPoint2D g_dragStartWorld;
    extern WorldPoint2D g_dragCurrentWorld;
    extern std::vector<WorldPoint2D> g_currentPenStroke;
    extern bool g_showTextInputModal;
    extern WorldPoint2D g_pendingTextPos;
    extern char g_textInputBuffer[256];

    inline void ToggleDrawingMode() {
        g_drawingModeActive = !g_drawingModeActive;
        if (!g_drawingModeActive) {
            g_isDrawingNow = false;
            g_currentPenStroke.clear();
        }
    }

    // 初始化与持久化
    void Init();
    void SaveDrawings();
    void LoadDrawings();
    void SwitchWorld(const std::string& worldId, int dimensionId);
    void SwitchMap(const std::string& mapId);

    // 图层管理
    std::vector<DrawingLayer>& GetLayers();
    DrawingLayer* GetActiveLayer();
    DrawingLayer* FindLayer(const std::string& layerId);
    std::string AddLayer(const std::string& name);
    bool RemoveLayer(const std::string& layerId);
    bool RenameLayer(const std::string& layerId, const std::string& newName);
    void SetActiveLayer(const std::string& layerId);
    void ClearActiveLayer();

    // 撤销与重做
    void Undo();
    void Redo();
    bool CanUndo();
    bool CanRedo();

    // 元素增删
    void AddElementToActiveLayer(const DrawingElement& elem);
    void RemoveElement(const std::string& layerId, const std::string& elemId);
    bool EraseAtWorldPos(float worldX, float worldZ, float radiusWorld);

    // 渲染与交互接口
    void RenderBigMapDrawings(ImDrawList* drawList, const ImVec2& center, float zoom, float playerX, float playerZ, float offsetX, float offsetZ, const ImVec2& displaySize, int viewDim);
    void RenderMinimapDrawings(ImDrawList* drawList, float cx, float cy, float scale, float cosRot, float sinRot, float playerX, float playerZ, float zoomRadius, bool isSquare, const ImVec2& mapMin, const ImVec2& mapMax, int viewDim);
    bool HandleBigMapCanvasInput(bool isHoveringCanvas, float hoverWx, float hoverWz, float zoom, bool isShiftHeld);
    void RenderDrawingToolbar(const ImVec2& displaySize);
}

// 全局别名
using DrawingManager::DrawingToolType;
using DrawingManager::WorldPoint2D;
using DrawingManager::DrawingElement;
using DrawingManager::DrawingLayer;
