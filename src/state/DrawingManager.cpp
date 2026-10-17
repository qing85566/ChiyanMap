#include "DrawingManager.h"
#include "LanguageManager.h"
#include "MapRenderState.h"
#include "hooks/InputIMEHelper.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>

namespace {
    inline bool IsPhysicalKeyDown(int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0 || (GetKeyState(vk) & 0x8000) != 0;
    }
}

namespace DrawingManager {

    bool g_drawingModeActive = false;
    DrawingToolType g_currentTool = DrawingToolType::Pen;
    float g_brushColor[4] = { 1.0f, 0.25f, 0.25f, 0.90f }; // 鲜红
    float g_brushThickness = 3.0f;
    bool g_shapeFilled = false;
    float g_fillColor[4] = { 1.0f, 0.25f, 0.25f, 0.25f };
    float g_eraserRadius = 16.0f; // 像素
    float g_textFontSize = 16.0f;
    std::string g_activeLayerId = "layer_default";
    std::mutex g_drawingMutex;

    bool g_isDrawingNow = false;
    WorldPoint2D g_dragStartWorld = { 0.0f, 0.0f };
    WorldPoint2D g_dragCurrentWorld = { 0.0f, 0.0f };
    std::vector<WorldPoint2D> g_currentPenStroke;
    bool g_showTextInputModal = false;
    WorldPoint2D g_pendingTextPos = { 0.0f, 0.0f };
    char g_textInputBuffer[256] = "";

    static std::string s_currentWorldId = "";
    static std::string s_currentMapId = "default";
    static int s_currentDim = 0;
    static std::vector<DrawingLayer> s_layers;

    // 撤销/重做系统
    enum class ActionKind { AddElem, EraseElem, ClearLayer };
    struct UndoAction {
        ActionKind kind;
        std::string layerId;
        DrawingElement element;
        std::vector<DrawingElement> clearedElements;
    };
    static std::vector<UndoAction> s_undoStack;
    static std::vector<UndoAction> s_redoStack;
    constexpr size_t kMaxUndoHistory = 60;

    static std::string GenerateUUID() {
        static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(0, 35);
        std::string res = "elem_";
        for (int i = 0; i < 8; ++i) res += alphanum[dist(rng)];
        return res;
    }

    static std::string GetDrawingsFilePath(int dim) {
        std::string mapPart = (s_currentMapId.empty() || s_currentMapId == "default") ? "" : ("_" + s_currentMapId);
        return "mods/ChiyanMap/drawings/" + s_currentWorldId + mapPart + "_dim" + std::to_string(dim) + ".json";
    }

    static float DistToSegmentSq(float px, float pz, float ax, float az, float bx, float bz) {
        float dx = bx - ax;
        float dz = bz - az;
        float l2 = dx * dx + dz * dz;
        if (l2 < 1e-6f) {
            float ex = px - ax, ez = pz - az;
            return ex * ex + ez * ez;
        }
        float t = ((px - ax) * dx + (pz - az) * dz) / l2;
        t = std::clamp(t, 0.0f, 1.0f);
        float projX = ax + t * dx;
        float projZ = az + t * dz;
        float rx = px - projX;
        float rz = pz - projZ;
        return rx * rx + rz * rz;
    }

    void EnsureDefaultLayer() {
        if (s_layers.empty()) {
            DrawingLayer def;
            def.id = "layer_default";
            def.name = LanguageManager::GetText("DRAWING_LAYER_DEFAULT");
            def.visible = true;
            def.locked = false;
            def.opacity = 1.0f;
            def.themeColor[0] = 0.20f; def.themeColor[1] = 0.70f; def.themeColor[2] = 1.0f; def.themeColor[3] = 1.0f;
            s_layers.push_back(def);
            g_activeLayerId = def.id;
        }
    }

    void Init() {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        EnsureDefaultLayer();
    }

    std::vector<DrawingLayer>& GetLayers() {
        EnsureDefaultLayer();
        return s_layers;
    }

    DrawingLayer* FindLayer(const std::string& layerId) {
        for (auto& l : s_layers) {
            if (l.id == layerId) return &l;
        }
        return nullptr;
    }

    DrawingLayer* GetActiveLayer() {
        DrawingLayer* lay = FindLayer(g_activeLayerId);
        if (!lay) {
            EnsureDefaultLayer();
            lay = &s_layers.front();
            g_activeLayerId = lay->id;
        }
        return lay;
    }

    std::string AddLayer(const std::string& name) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        DrawingLayer lay;
        lay.id = "layer_" + GenerateUUID();
        lay.name = name.empty() ? (LanguageManager::GetText("DRAWING_LAYER_DEFAULT") + std::to_string(s_layers.size() + 1)) : name;
        lay.visible = true;
        lay.locked = false;
        lay.opacity = 1.0f;
        // 自动轮换配色
        static const float kThemeCols[6][4] = {
            { 0.20f, 0.70f, 1.0f, 1.0f },
            { 1.0f, 0.40f, 0.20f, 1.0f },
            { 0.30f, 0.90f, 0.40f, 1.0f },
            { 0.95f, 0.80f, 0.20f, 1.0f },
            { 0.80f, 0.30f, 0.95f, 1.0f },
            { 0.10f, 0.90f, 0.85f, 1.0f }
        };
        const float* tc = kThemeCols[s_layers.size() % 6];
        for (int i = 0; i < 4; ++i) lay.themeColor[i] = tc[i];

        s_layers.push_back(lay);
        g_activeLayerId = lay.id;
        SaveDrawings();
        return lay.id;
    }

    bool RemoveLayer(const std::string& layerId) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        if (s_layers.size() <= 1) return false; // 至少保留一个图层
        auto it = std::find_if(s_layers.begin(), s_layers.end(), [&](const DrawingLayer& l) { return l.id == layerId; });
        if (it != s_layers.end()) {
            s_layers.erase(it);
            if (g_activeLayerId == layerId) {
                g_activeLayerId = s_layers.front().id;
            }
            SaveDrawings();
            return true;
        }
        return false;
    }

    bool RenameLayer(const std::string& layerId, const std::string& newName) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        DrawingLayer* l = FindLayer(layerId);
        if (l && !newName.empty()) {
            l->name = newName;
            SaveDrawings();
            return true;
        }
        return false;
    }

    void SetActiveLayer(const std::string& layerId) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        if (FindLayer(layerId)) {
            g_activeLayerId = layerId;
        }
    }

    void ClearActiveLayer() {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        DrawingLayer* lay = GetActiveLayer();
        if (!lay || lay->locked || lay->elements.empty()) return;

        UndoAction act;
        act.kind = ActionKind::ClearLayer;
        act.layerId = lay->id;
        act.clearedElements = lay->elements;
        s_undoStack.push_back(act);
        s_redoStack.clear();

        lay->elements.clear();
        SaveDrawings();
    }

    void AddElementToActiveLayer(const DrawingElement& elem) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        DrawingLayer* lay = GetActiveLayer();
        if (!lay || lay->locked) return;

        lay->elements.push_back(elem);

        UndoAction act;
        act.kind = ActionKind::AddElem;
        act.layerId = lay->id;
        act.element = elem;
        s_undoStack.push_back(act);
        if (s_undoStack.size() > kMaxUndoHistory) s_undoStack.erase(s_undoStack.begin());
        s_redoStack.clear();

        SaveDrawings();
    }

    void RemoveElement(const std::string& layerId, const std::string& elemId) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        DrawingLayer* lay = FindLayer(layerId);
        if (!lay || lay->locked) return;

        auto it = std::find_if(lay->elements.begin(), lay->elements.end(), [&](const DrawingElement& e) { return e.id == elemId; });
        if (it != lay->elements.end()) {
            UndoAction act;
            act.kind = ActionKind::EraseElem;
            act.layerId = lay->id;
            act.element = *it;
            s_undoStack.push_back(act);
            if (s_undoStack.size() > kMaxUndoHistory) s_undoStack.erase(s_undoStack.begin());
            s_redoStack.clear();

            lay->elements.erase(it);
            SaveDrawings();
        }
    }

    bool EraseAtWorldPos(float worldX, float worldZ, float radiusWorld) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        bool erasedAny = false;
        for (auto& lay : s_layers) {
            if (!lay.visible || lay.locked) continue;
            for (auto it = lay.elements.begin(); it != lay.elements.end(); ) {
                if (it->HitTest(worldX, worldZ, radiusWorld)) {
                    UndoAction act;
                    act.kind = ActionKind::EraseElem;
                    act.layerId = lay.id;
                    act.element = *it;
                    s_undoStack.push_back(act);
                    if (s_undoStack.size() > kMaxUndoHistory) s_undoStack.erase(s_undoStack.begin());
                    s_redoStack.clear();

                    it = lay.elements.erase(it);
                    erasedAny = true;
                } else {
                    ++it;
                }
            }
        }
        if (erasedAny) SaveDrawings();
        return erasedAny;
    }

    bool CanUndo() { return !s_undoStack.empty(); }
    bool CanRedo() { return !s_redoStack.empty(); }

    void Undo() {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        if (s_undoStack.empty()) return;
        UndoAction act = s_undoStack.back();
        s_undoStack.pop_back();

        DrawingLayer* lay = FindLayer(act.layerId);
        if (!lay) return;

        if (act.kind == ActionKind::AddElem) {
            // 撤销添加 -> 删除该元素
            auto it = std::find_if(lay->elements.begin(), lay->elements.end(), [&](const DrawingElement& e) { return e.id == act.element.id; });
            if (it != lay->elements.end()) {
                lay->elements.erase(it);
            }
            s_redoStack.push_back(act);
        } else if (act.kind == ActionKind::EraseElem) {
            // 撤销擦除 -> 加回该元素
            lay->elements.push_back(act.element);
            s_redoStack.push_back(act);
        } else if (act.kind == ActionKind::ClearLayer) {
            // 撤销清空 -> 恢复原所有元素
            lay->elements = act.clearedElements;
            s_redoStack.push_back(act);
        }
        SaveDrawings();
    }

    void Redo() {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        if (s_redoStack.empty()) return;
        UndoAction act = s_redoStack.back();
        s_redoStack.pop_back();

        DrawingLayer* lay = FindLayer(act.layerId);
        if (!lay) return;

        if (act.kind == ActionKind::AddElem) {
            lay->elements.push_back(act.element);
            s_undoStack.push_back(act);
        } else if (act.kind == ActionKind::EraseElem) {
            auto it = std::find_if(lay->elements.begin(), lay->elements.end(), [&](const DrawingElement& e) { return e.id == act.element.id; });
            if (it != lay->elements.end()) {
                lay->elements.erase(it);
            }
            s_undoStack.push_back(act);
        } else if (act.kind == ActionKind::ClearLayer) {
            lay->elements.clear();
            s_undoStack.push_back(act);
        }
        SaveDrawings();
    }

    void SwitchWorld(const std::string& worldId, int dimensionId) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        if (s_currentWorldId == worldId && s_currentDim == dimensionId) return;
        s_currentWorldId = worldId;
        s_currentDim = dimensionId;
        s_undoStack.clear();
        s_redoStack.clear();
        LoadDrawings();
    }

    void SwitchMap(const std::string& mapId) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);
        if (s_currentMapId == mapId) return;
        s_currentMapId = mapId;
        s_undoStack.clear();
        s_redoStack.clear();
        LoadDrawings();
    }

    void SaveDrawings() {
        if (s_currentWorldId.empty()) return;
        std::string filePath = GetDrawingsFilePath(s_currentDim);
        try {
            std::filesystem::path p(filePath);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }

            nlohmann::json root = nlohmann::json::object();
            root["activeLayerId"] = g_activeLayerId;
            nlohmann::json layersArr = nlohmann::json::array();

            for (const auto& lay : s_layers) {
                nlohmann::json layObj = nlohmann::json::object();
                layObj["id"] = lay.id;
                layObj["name"] = lay.name;
                layObj["visible"] = lay.visible;
                layObj["locked"] = lay.locked;
                layObj["opacity"] = lay.opacity;
                layObj["themeColor"] = { lay.themeColor[0], lay.themeColor[1], lay.themeColor[2], lay.themeColor[3] };

                nlohmann::json elemArr = nlohmann::json::array();
                for (const auto& elem : lay.elements) {
                    nlohmann::json eObj = nlohmann::json::object();
                    eObj["id"] = elem.id;
                    eObj["type"] = static_cast<int>(elem.type);
                    eObj["color"] = { elem.color[0], elem.color[1], elem.color[2], elem.color[3] };
                    eObj["thickness"] = elem.thickness;
                    eObj["filled"] = elem.filled;
                    eObj["fillColor"] = { elem.fillColor[0], elem.fillColor[1], elem.fillColor[2], elem.fillColor[3] };
                    eObj["radius"] = elem.radius;
                    eObj["text"] = elem.text;
                    eObj["fontSize"] = elem.fontSize;
                    eObj["createdAt"] = elem.createdAt;

                    nlohmann::json ptsArr = nlohmann::json::array();
                    for (const auto& pt : elem.points) {
                        ptsArr.push_back({ pt.x, pt.z });
                    }
                    eObj["points"] = ptsArr;
                    elemArr.push_back(eObj);
                }
                layObj["elements"] = elemArr;
                layersArr.push_back(layObj);
            }
            root["layers"] = layersArr;

            std::ofstream ofs(filePath);
            if (ofs.is_open()) {
                ofs << root.dump(2);
            }
        } catch (...) {}
    }

    void LoadDrawings() {
        s_layers.clear();
        if (s_currentWorldId.empty()) {
            EnsureDefaultLayer();
            return;
        }

        std::string filePath = GetDrawingsFilePath(s_currentDim);
        try {
            if (std::filesystem::exists(filePath)) {
                std::ifstream ifs(filePath);
                if (ifs.is_open()) {
                    nlohmann::json root = nlohmann::json::parse(ifs);
                    g_activeLayerId = root.value("activeLayerId", "layer_default");
                    if (root.contains("layers") && root["layers"].is_array()) {
                        for (const auto& layObj : root["layers"]) {
                            DrawingLayer lay;
                            lay.id = layObj.value("id", "layer_default");
                            lay.name = layObj.value("name", "Layer");
                            lay.visible = layObj.value("visible", true);
                            lay.locked = layObj.value("locked", false);
                            lay.opacity = layObj.value("opacity", 1.0f);
                            if (layObj.contains("themeColor") && layObj["themeColor"].is_array() && layObj["themeColor"].size() == 4) {
                                for (int i = 0; i < 4; ++i) lay.themeColor[i] = layObj["themeColor"][i].get<float>();
                            }

                            if (layObj.contains("elements") && layObj["elements"].is_array()) {
                                for (const auto& eObj : layObj["elements"]) {
                                    DrawingElement elem;
                                    elem.id = eObj.value("id", GenerateUUID());
                                    elem.type = static_cast<DrawingToolType>(eObj.value("type", 0));
                                    elem.thickness = eObj.value("thickness", 3.0f);
                                    elem.filled = eObj.value("filled", false);
                                    elem.radius = eObj.value("radius", 0.0f);
                                    elem.text = eObj.value("text", "");
                                    elem.fontSize = eObj.value("fontSize", 16.0f);
                                    elem.createdAt = eObj.value("createdAt", 0ULL);
                                    if (eObj.contains("color") && eObj["color"].is_array() && eObj["color"].size() == 4) {
                                        for (int i = 0; i < 4; ++i) elem.color[i] = eObj["color"][i].get<float>();
                                    }
                                    if (eObj.contains("fillColor") && eObj["fillColor"].is_array() && eObj["fillColor"].size() == 4) {
                                        for (int i = 0; i < 4; ++i) elem.fillColor[i] = eObj["fillColor"][i].get<float>();
                                    }

                                    if (eObj.contains("points") && eObj["points"].is_array()) {
                                        for (const auto& ptArr : eObj["points"]) {
                                            if (ptArr.is_array() && ptArr.size() == 2) {
                                                elem.points.push_back({ ptArr[0].get<float>(), ptArr[1].get<float>() });
                                            }
                                        }
                                    }
                                    elem.RecomputeBounds();
                                    lay.elements.push_back(elem);
                                }
                            }
                            s_layers.push_back(lay);
                        }
                    }
                }
            }
        } catch (...) {}
        EnsureDefaultLayer();
    }

    void DrawingElement::RecomputeBounds() {
        if (type == DrawingToolType::Circle) {
            if (!points.empty()) {
                minX = points[0].x - radius;
                maxX = points[0].x + radius;
                minZ = points[0].z - radius;
                maxZ = points[0].z + radius;
            }
            return;
        }

        if (points.empty()) {
            minX = maxX = minZ = maxZ = 0.0f;
            return;
        }

        minX = maxX = points[0].x;
        minZ = maxZ = points[0].z;

        for (size_t i = 1; i < points.size(); ++i) {
            minX = std::min(minX, points[i].x);
            maxX = std::max(maxX, points[i].x);
            minZ = std::min(minZ, points[i].z);
            maxZ = std::max(maxZ, points[i].z);
        }

        if (type == DrawingToolType::Text) {
            // 文字标牌给予一个适度的世界尺寸容限
            float pad = 10.0f;
            minX -= pad; maxX += pad;
            minZ -= pad; maxZ += pad;
        }
    }

    bool DrawingElement::HitTest(float worldX, float worldZ, float toleranceWorld) const {
        // AABB 初筛
        if (worldX < minX - toleranceWorld || worldX > maxX + toleranceWorld ||
            worldZ < minZ - toleranceWorld || worldZ > maxZ + toleranceWorld) {
            return false;
        }

        float tolSq = toleranceWorld * toleranceWorld;

        if (type == DrawingToolType::Circle) {
            if (points.empty()) return false;
            float dx = worldX - points[0].x;
            float dz = worldZ - points[0].z;
            float d = std::sqrt(dx * dx + dz * dz);
            if (filled) return d <= (radius + toleranceWorld);
            return std::abs(d - radius) <= toleranceWorld;
        }

        if (type == DrawingToolType::Line || type == DrawingToolType::Arrow) {
            if (points.size() < 2) return false;
            return DistToSegmentSq(worldX, worldZ, points[0].x, points[0].z, points[1].x, points[1].z) <= tolSq;
        }

        if (type == DrawingToolType::Rect) {
            if (points.size() < 2) return false;
            float x0 = std::min(points[0].x, points[1].x);
            float x1 = std::max(points[0].x, points[1].x);
            float z0 = std::min(points[0].z, points[1].z);
            float z1 = std::max(points[0].z, points[1].z);
            if (filled) {
                return (worldX >= x0 - toleranceWorld && worldX <= x1 + toleranceWorld &&
                        worldZ >= z0 - toleranceWorld && worldZ <= z1 + toleranceWorld);
            }
            // 边缘四条线段测试
            if (DistToSegmentSq(worldX, worldZ, x0, z0, x1, z0) <= tolSq) return true;
            if (DistToSegmentSq(worldX, worldZ, x1, z0, x1, z1) <= tolSq) return true;
            if (DistToSegmentSq(worldX, worldZ, x1, z1, x0, z1) <= tolSq) return true;
            if (DistToSegmentSq(worldX, worldZ, x0, z1, x0, z0) <= tolSq) return true;
            return false;
        }

        if (type == DrawingToolType::Pen) {
            if (points.size() < 2) {
                if (points.size() == 1) {
                    float dx = worldX - points[0].x;
                    float dz = worldZ - points[0].z;
                    return (dx * dx + dz * dz) <= tolSq;
                }
                return false;
            }
            for (size_t i = 0; i + 1 < points.size(); ++i) {
                if (DistToSegmentSq(worldX, worldZ, points[i].x, points[i].z, points[i + 1].x, points[i + 1].z) <= tolSq) {
                    return true;
                }
            }
            return false;
        }

        if (type == DrawingToolType::Text) {
            if (points.empty()) return false;
            float dx = std::abs(worldX - points[0].x);
            float dz = std::abs(worldZ - points[0].z);
            return (dx <= toleranceWorld * 2.0f && dz <= toleranceWorld * 2.0f);
        }

        return false;
    }

    // ==========================================
    // 渲染：全屏大地图标注绘制
    // ==========================================
    void RenderBigMapDrawings(ImDrawList* drawList, const ImVec2& center, float zoom, float playerX, float playerZ, float offsetX, float offsetZ, const ImVec2& displaySize, int viewDim) {
        std::lock_guard<std::mutex> lock(g_drawingMutex);

        auto WorldToScreen = [&](float wx, float wz) -> ImVec2 {
            return ImVec2(
                center.x + (wx - playerX) * zoom + offsetX,
                center.y + (wz - playerZ) * zoom + offsetZ
            );
        };

        // 视口边界 (世界坐标) 用于 O(1) 裁剪
        float wX0 = playerX - (center.x + offsetX) / zoom;
        float wX1 = playerX + (displaySize.x - center.x - offsetX) / zoom;
        float wZ0 = playerZ - (center.y + offsetZ) / zoom;
        float wZ1 = playerZ + (displaySize.y - center.y - offsetZ) / zoom;

        // 1. 绘制已保存的各可见图层元素
        for (const auto& lay : s_layers) {
            if (!lay.visible || lay.opacity <= 0.01f) continue;
            float layAlpha = std::clamp(lay.opacity, 0.05f, 1.0f);

            for (const auto& elem : lay.elements) {
                // 视口裁剪
                if (elem.maxX < wX0 || elem.minX > wX1 || elem.maxZ < wZ0 || elem.minZ > wZ1) {
                    continue;
                }

                ImU32 strokeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    elem.color[0], elem.color[1], elem.color[2], elem.color[3] * layAlpha
                ));
                ImU32 fillCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    elem.fillColor[0], elem.fillColor[1], elem.fillColor[2], elem.fillColor[3] * layAlpha
                ));
                float thick = std::max(1.0f, elem.thickness);

                if (elem.type == DrawingToolType::Pen) {
                    if (elem.points.size() >= 2) {
                        std::vector<ImVec2> sPoints;
                        sPoints.reserve(elem.points.size());
                        for (const auto& pt : elem.points) {
                            sPoints.push_back(WorldToScreen(pt.x, pt.z));
                        }
                        drawList->AddPolyline(sPoints.data(), (int)sPoints.size(), strokeCol, 0, thick);
                    } else if (elem.points.size() == 1) {
                        drawList->AddCircleFilled(WorldToScreen(elem.points[0].x, elem.points[0].z), thick * 0.7f, strokeCol);
                    }
                } else if (elem.type == DrawingToolType::Line) {
                    if (elem.points.size() >= 2) {
                        drawList->AddLine(WorldToScreen(elem.points[0].x, elem.points[0].z),
                                          WorldToScreen(elem.points[1].x, elem.points[1].z), strokeCol, thick);
                    }
                } else if (elem.type == DrawingToolType::Arrow) {
                    if (elem.points.size() >= 2) {
                        ImVec2 p0 = WorldToScreen(elem.points[0].x, elem.points[0].z);
                        ImVec2 p1 = WorldToScreen(elem.points[1].x, elem.points[1].z);
                        drawList->AddLine(p0, p1, strokeCol, thick);

                        // 绘制箭头尖端
                        float adx = p1.x - p0.x;
                        float ady = p1.y - p0.y;
                        float alen = std::sqrt(adx * adx + ady * ady);
                        if (alen > 4.0f) {
                            float ux = adx / alen;
                            float uy = ady / alen;
                            float arrowSz = std::clamp(thick * 3.5f, 10.0f, 28.0f);
                            float arrowW = arrowSz * 0.55f;
                            ImVec2 leftWing(p1.x - ux * arrowSz - uy * arrowW, p1.y - uy * arrowSz + ux * arrowW);
                            ImVec2 rightWing(p1.x - ux * arrowSz + uy * arrowW, p1.y - uy * arrowSz - ux * arrowW);
                            drawList->AddTriangleFilled(p1, leftWing, rightWing, strokeCol);
                        }
                    }
                } else if (elem.type == DrawingToolType::Rect) {
                    if (elem.points.size() >= 2) {
                        ImVec2 p0 = WorldToScreen(elem.points[0].x, elem.points[0].z);
                        ImVec2 p1 = WorldToScreen(elem.points[1].x, elem.points[1].z);
                        ImVec2 rMin(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
                        ImVec2 rMax(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
                        if (elem.filled) {
                            drawList->AddRectFilled(rMin, rMax, fillCol, 2.0f);
                        }
                        drawList->AddRect(rMin, rMax, strokeCol, 2.0f, 0, thick);
                    }
                } else if (elem.type == DrawingToolType::Circle) {
                    if (!elem.points.empty()) {
                        ImVec2 c = WorldToScreen(elem.points[0].x, elem.points[0].z);
                        float sRadius = elem.radius * zoom;
                        if (elem.filled) {
                            drawList->AddCircleFilled(c, sRadius, fillCol, 48);
                        }
                        drawList->AddCircle(c, sRadius, strokeCol, 48, thick);
                    }
                } else if (elem.type == DrawingToolType::Text) {
                    if (!elem.points.empty() && !elem.text.empty()) {
                        ImVec2 pos = WorldToScreen(elem.points[0].x, elem.points[0].z);
                        ImFont* font = ImGui::GetFont();
                        float fSize = std::clamp(elem.fontSize, 12.0f, 32.0f);
                        ImVec2 tSize = font->CalcTextSizeA(fSize, FLT_MAX, 0.0f, elem.text.c_str());
                        float padX = 6.0f;
                        float padY = 3.0f;
                        ImVec2 bMin(pos.x - padX, pos.y - padY);
                        ImVec2 bMax(pos.x + tSize.x + padX, pos.y + tSize.y + padY);

                        drawList->AddRectFilled(bMin, bMax, IM_COL32(15, 20, 26, (int)(220 * layAlpha)), 4.0f);
                        drawList->AddRect(bMin, bMax, strokeCol, 4.0f, 0, 1.5f);
                        drawList->AddText(font, fSize, pos, strokeCol, elem.text.c_str());
                    }
                }
            }
        }

        // 2. 实时正在绘制的预览交互图元
        if (g_drawingModeActive && g_isDrawingNow) {
            ImU32 previewCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                g_brushColor[0], g_brushColor[1], g_brushColor[2], g_brushColor[3]
            ));
            ImU32 previewFill = ImGui::ColorConvertFloat4ToU32(ImVec4(
                g_fillColor[0], g_fillColor[1], g_fillColor[2], g_shapeFilled ? g_fillColor[3] : 0.0f
            ));
            float thick = std::max(1.0f, g_brushThickness);

            if (g_currentTool == DrawingToolType::Pen) {
                if (g_currentPenStroke.size() >= 2) {
                    std::vector<ImVec2> sPoints;
                    sPoints.reserve(g_currentPenStroke.size() + 1);
                    for (const auto& pt : g_currentPenStroke) {
                        sPoints.push_back(WorldToScreen(pt.x, pt.z));
                    }
                    sPoints.push_back(WorldToScreen(g_dragCurrentWorld.x, g_dragCurrentWorld.z));
                    drawList->AddPolyline(sPoints.data(), (int)sPoints.size(), previewCol, 0, thick);
                } else if (!g_currentPenStroke.empty()) {
                    ImVec2 p0 = WorldToScreen(g_currentPenStroke[0].x, g_currentPenStroke[0].z);
                    ImVec2 p1 = WorldToScreen(g_dragCurrentWorld.x, g_dragCurrentWorld.z);
                    drawList->AddLine(p0, p1, previewCol, thick);
                }
            } else if (g_currentTool == DrawingToolType::Line) {
                drawList->AddLine(WorldToScreen(g_dragStartWorld.x, g_dragStartWorld.z),
                                  WorldToScreen(g_dragCurrentWorld.x, g_dragCurrentWorld.z), previewCol, thick);
            } else if (g_currentTool == DrawingToolType::Arrow) {
                ImVec2 p0 = WorldToScreen(g_dragStartWorld.x, g_dragStartWorld.z);
                ImVec2 p1 = WorldToScreen(g_dragCurrentWorld.x, g_dragCurrentWorld.z);
                drawList->AddLine(p0, p1, previewCol, thick);

                float adx = p1.x - p0.x;
                float ady = p1.y - p0.y;
                float alen = std::sqrt(adx * adx + ady * ady);
                if (alen > 4.0f) {
                    float ux = adx / alen;
                    float uy = ady / alen;
                    float arrowSz = std::clamp(thick * 3.5f, 10.0f, 28.0f);
                    float arrowW = arrowSz * 0.55f;
                    ImVec2 leftWing(p1.x - ux * arrowSz - uy * arrowW, p1.y - uy * arrowSz + ux * arrowW);
                    ImVec2 rightWing(p1.x - ux * arrowSz + uy * arrowW, p1.y - uy * arrowSz - ux * arrowW);
                    drawList->AddTriangleFilled(p1, leftWing, rightWing, previewCol);
                }
            } else if (g_currentTool == DrawingToolType::Rect) {
                ImVec2 p0 = WorldToScreen(g_dragStartWorld.x, g_dragStartWorld.z);
                ImVec2 p1 = WorldToScreen(g_dragCurrentWorld.x, g_dragCurrentWorld.z);
                ImVec2 rMin(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
                ImVec2 rMax(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
                if (g_shapeFilled) {
                    drawList->AddRectFilled(rMin, rMax, previewFill, 2.0f);
                }
                drawList->AddRect(rMin, rMax, previewCol, 2.0f, 0, thick);
            } else if (g_currentTool == DrawingToolType::Circle) {
                ImVec2 c = WorldToScreen(g_dragStartWorld.x, g_dragStartWorld.z);
                float dx = g_dragCurrentWorld.x - g_dragStartWorld.x;
                float dz = g_dragCurrentWorld.z - g_dragStartWorld.z;
                float wRadius = std::sqrt(dx * dx + dz * dz);
                float sRadius = wRadius * zoom;
                if (g_shapeFilled) {
                    drawList->AddCircleFilled(c, sRadius, previewFill, 48);
                }
                drawList->AddCircle(c, sRadius, previewCol, 48, thick);
            }
        }

        // 3. 橡皮擦光标指示环
        if (g_drawingModeActive && g_currentTool == DrawingToolType::Eraser) {
            ImVec2 mPos = ImGui::GetIO().MousePos;
            drawList->AddCircle(mPos, g_eraserRadius, IM_COL32(255, 60, 60, 220), 24, 1.8f);
            drawList->AddCircleFilled(mPos, 2.0f, IM_COL32(255, 60, 60, 255));
        }
    }

    // ==========================================
    // 渲染：小地图同步标注绘制
    // ==========================================
    void RenderMinimapDrawings(ImDrawList* drawList, float cx, float cy, float scale, float cosRot, float sinRot, float playerX, float playerZ, float zoomRadius, bool isSquare, const ImVec2& mapMin, const ImVec2& mapMax, int viewDim) {
        if (!MapRenderState::showDrawingsOnMinimap) return;
        std::lock_guard<std::mutex> lock(g_drawingMutex);

        auto WorldToMinimap = [&](float wx, float wz) -> ImVec2 {
            float dx = wx - playerX;
            float dz = wz - playerZ;
            float rx = dx * cosRot - dz * sinRot;
            float rz = dx * sinRot + dz * cosRot;
            return ImVec2(cx + rx * scale, cy + rz * scale);
        };

        if (isSquare) {
            drawList->PushClipRect(mapMin, mapMax, true);
        }

        for (const auto& lay : s_layers) {
            if (!lay.visible || lay.opacity <= 0.01f) continue;
            float layAlpha = lay.opacity * 0.85f;

            for (const auto& elem : lay.elements) {
                ImU32 strokeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    elem.color[0], elem.color[1], elem.color[2], elem.color[3] * layAlpha
                ));
                float thick = std::max(1.0f, elem.thickness * 0.75f);

                if (elem.type == DrawingToolType::Pen || elem.type == DrawingToolType::Line || elem.type == DrawingToolType::Arrow) {
                    if (elem.points.size() >= 2) {
                        for (size_t i = 0; i + 1 < elem.points.size(); ++i) {
                            ImVec2 p0 = WorldToMinimap(elem.points[i].x, elem.points[i].z);
                            ImVec2 p1 = WorldToMinimap(elem.points[i + 1].x, elem.points[i + 1].z);
                            drawList->AddLine(p0, p1, strokeCol, thick);
                        }
                    }
                } else if (elem.type == DrawingToolType::Rect) {
                    if (elem.points.size() >= 2) {
                        ImVec2 p0 = WorldToMinimap(elem.points[0].x, elem.points[0].z);
                        ImVec2 p1 = WorldToMinimap(elem.points[1].x, elem.points[0].z);
                        ImVec2 p2 = WorldToMinimap(elem.points[1].x, elem.points[1].z);
                        ImVec2 p3 = WorldToMinimap(elem.points[0].x, elem.points[1].z);
                        drawList->AddQuad(p0, p1, p2, p3, strokeCol, thick);
                    }
                } else if (elem.type == DrawingToolType::Circle) {
                    if (!elem.points.empty()) {
                        ImVec2 c = WorldToMinimap(elem.points[0].x, elem.points[0].z);
                        drawList->AddCircle(c, elem.radius * scale, strokeCol, 32, thick);
                    }
                } else if (elem.type == DrawingToolType::Text) {
                    if (!elem.points.empty()) {
                        ImVec2 c = WorldToMinimap(elem.points[0].x, elem.points[0].z);
                        drawList->AddCircleFilled(c, 3.5f, strokeCol);
                    }
                }
            }
        }

        if (isSquare) {
            drawList->PopClipRect();
        }
    }

    // ==========================================
    // 交互：画布鼠标绘制输入处理
    // ==========================================
    bool HandleBigMapCanvasInput(bool isHoveringCanvas, float hoverWx, float hoverWz, float zoom, bool isShiftHeld) {
        if (!g_drawingModeActive) return false;
        ImGuiIO& io = ImGui::GetIO();

        // 橡皮擦半径转为世界单位
        float eraserWorldRadius = std::max(1.0f, g_eraserRadius / zoom);

        // 悬停光标状态提示
        if (isHoveringCanvas && !isShiftHeld) {
            if (g_currentTool == DrawingToolType::Text) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
            } else if (g_currentTool == DrawingToolType::Eraser) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            } else if (g_currentTool == DrawingToolType::Line || g_currentTool == DrawingToolType::Arrow ||
                       g_currentTool == DrawingToolType::Rect || g_currentTool == DrawingToolType::Circle) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        }

        // 1. 鼠标左键按下
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isHoveringCanvas && !isShiftHeld) {
            DrawingLayer* lay = GetActiveLayer();
            if (lay && !lay->locked) {
                if (g_currentTool == DrawingToolType::Eraser) {
                    EraseAtWorldPos(hoverWx, hoverWz, eraserWorldRadius);
                } else if (g_currentTool == DrawingToolType::Text) {
                    g_pendingTextPos = { hoverWx, hoverWz };
                    g_showTextInputModal = true;
                    g_textInputBuffer[0] = '\0';
                } else {
                    g_isDrawingNow = true;
                    g_dragStartWorld = { hoverWx, hoverWz };
                    g_dragCurrentWorld = g_dragStartWorld;
                    if (g_currentTool == DrawingToolType::Pen) {
                        g_currentPenStroke.clear();
                        g_currentPenStroke.push_back(g_dragStartWorld);
                    }
                }
                return true;
            }
        }

        // 2. 鼠标左键拖拽中
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && g_isDrawingNow) {
            g_dragCurrentWorld = { hoverWx, hoverWz };
            if (g_currentTool == DrawingToolType::Pen) {
                if (!g_currentPenStroke.empty()) {
                    const auto& last = g_currentPenStroke.back();
                    float dx = hoverWx - last.x;
                    float dz = hoverWz - last.z;
                    // 动态自适应抽稀，防止每帧生成过密点
                    float minStep = std::max(0.1f, 0.5f / zoom);
                    if (dx * dx + dz * dz >= minStep * minStep) {
                        g_currentPenStroke.push_back({ hoverWx, hoverWz });
                    }
                }
            }
            return true;
        }

        // 3. 橡皮擦按住持续擦除
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && g_currentTool == DrawingToolType::Eraser && isHoveringCanvas) {
            EraseAtWorldPos(hoverWx, hoverWz, eraserWorldRadius);
            return true;
        }

        // 4. 鼠标左键释放 -> 提交图形
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && g_isDrawingNow) {
            g_isDrawingNow = false;
            DrawingLayer* lay = GetActiveLayer();
            if (lay && !lay->locked) {
                DrawingElement elem;
                elem.id = GenerateUUID();
                elem.type = g_currentTool;
                elem.thickness = g_brushThickness;
                elem.filled = g_shapeFilled;
                elem.fontSize = g_textFontSize;
                elem.createdAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                for (int i = 0; i < 4; ++i) {
                    elem.color[i] = g_brushColor[i];
                    elem.fillColor[i] = g_fillColor[i];
                }

                if (g_currentTool == DrawingToolType::Pen) {
                    if (g_currentPenStroke.size() >= 1) {
                        if (g_currentPenStroke.size() == 1) {
                            // 单击落点：微小偏移形成独立实心点
                            g_currentPenStroke.push_back({ g_currentPenStroke[0].x + 0.05f, g_currentPenStroke[0].z + 0.05f });
                        }
                        elem.points = g_currentPenStroke;
                        elem.RecomputeBounds();
                        AddElementToActiveLayer(elem);
                    }
                    g_currentPenStroke.clear();
                } else if (g_currentTool == DrawingToolType::Line || g_currentTool == DrawingToolType::Arrow) {
                    float dx = g_dragCurrentWorld.x - g_dragStartWorld.x;
                    float dz = g_dragCurrentWorld.z - g_dragStartWorld.z;
                    if (dx * dx + dz * dz >= 0.01f) {
                        elem.points = { g_dragStartWorld, g_dragCurrentWorld };
                        elem.RecomputeBounds();
                        AddElementToActiveLayer(elem);
                    }
                } else if (g_currentTool == DrawingToolType::Rect) {
                    float dx = g_dragCurrentWorld.x - g_dragStartWorld.x;
                    float dz = g_dragCurrentWorld.z - g_dragStartWorld.z;
                    if (dx * dx + dz * dz >= 0.01f) {
                        elem.points = { g_dragStartWorld, g_dragCurrentWorld };
                        elem.RecomputeBounds();
                        AddElementToActiveLayer(elem);
                    }
                } else if (g_currentTool == DrawingToolType::Circle) {
                    float dx = g_dragCurrentWorld.x - g_dragStartWorld.x;
                    float dz = g_dragCurrentWorld.z - g_dragStartWorld.z;
                    float r = std::sqrt(dx * dx + dz * dz);
                    if (r >= 0.05f) {
                        elem.points = { g_dragStartWorld };
                        elem.radius = r;
                        elem.RecomputeBounds();
                        AddElementToActiveLayer(elem);
                    }
                }
            }
            return true;
        }

        // 5. 鼠标右键按下 -> 取消当前绘制中的图形
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && g_isDrawingNow) {
            g_isDrawingNow = false;
            g_currentPenStroke.clear();
            return true;
        }

        // 6. 快捷键快捷撤销/重做 (兼容 Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z)
        bool isCtrl = io.KeyCtrl || IsPhysicalKeyDown(VK_CONTROL);
        bool isShift = io.KeyShift || IsPhysicalKeyDown(VK_SHIFT);
        bool zPressed = ImGui::IsKeyPressed(ImGuiKey_Z);
        bool yPressed = ImGui::IsKeyPressed(ImGuiKey_Y);

        if (isCtrl) {
            if ((yPressed || (zPressed && isShift)) && CanRedo()) {
                Redo();
                return true;
            } else if (zPressed && !isShift && CanUndo()) {
                Undo();
                return true;
            }
        }

        return false;
    }

    // ==========================================
    // UI 面板：涂鸦悬浮工具栏与图层管理
    // ==========================================
    void RenderDrawingToolbar(const ImVec2& displaySize) {
        if (!g_drawingModeActive) return;

        // 全局快捷键响应 (不依赖光标是否悬停在画布内部)
        ImGuiIO& io = ImGui::GetIO();
        bool isTextInput = (ImGui::GetCurrentContext() && io.WantTextInput);
        if (!isTextInput) {
            bool isCtrl = io.KeyCtrl || IsPhysicalKeyDown(VK_CONTROL);
            bool isShift = io.KeyShift || IsPhysicalKeyDown(VK_SHIFT);
            bool zPressed = ImGui::IsKeyPressed(ImGuiKey_Z);
            bool yPressed = ImGui::IsKeyPressed(ImGuiKey_Y);

            if (isCtrl) {
                if ((yPressed || (zPressed && isShift)) && CanRedo()) {
                    Redo();
                } else if (zPressed && !isShift && CanUndo()) {
                    Undo();
                }
            }
        }

        static bool s_showLayersDrawer = false;
        static bool s_confirmClearLayer = false;
        static char s_newLayerNameBuf[64] = "";

        float curScale = std::clamp(MapRenderState::globalUIScale, 0.25f, 4.0f);

        // 悬浮工具栏窗口布局：基于屏幕水平中点 (pivot 0.5) 居中对齐，AutoResize 自动测量全部按钮与控件总宽度，彻底解决右侧被裁切问题
        float tbY = 20.0f * curScale;
        ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, tbY), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f * curScale, 0.0f), ImVec2(displaySize.x - 20.0f * curScale, displaySize.y));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f * curScale);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * curScale, 8.0f * curScale));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.12f, 0.18f, 0.94f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.55f, 0.85f, 0.85f));

        ImVec2 tbWinPos(0.0f, 0.0f);
        ImVec2 tbWinSize(0.0f, 0.0f);

        if (ImGui::Begin("##DrawingModeToolbar", nullptr, flags)) {
            tbWinPos = ImGui::GetWindowPos();
            tbWinSize = ImGui::GetWindowSize();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float btnSz = 28.0f * curScale;

            auto ToolBtn = [&](DrawingToolType t, const char* label, const char* tooltip) {
                bool isCur = (g_currentTool == t);
                if (isCur) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.90f, 0.95f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.65f, 1.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.80f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.20f, 0.28f, 0.85f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.35f, 0.48f, 0.95f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.45f, 0.60f, 1.0f));
                }

                if (ImGui::Button(label, ImVec2(btnSz, btnSz))) {
                    g_currentTool = t;
                    g_isDrawingNow = false;
                }
                ImGui::PopStyleColor(3);

                if (isCur) {
                    drawList->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(100, 200, 255, 240), 4.0f, 0, 1.5f);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tooltip);
                }
                ImGui::SameLine();
            };

            // 7 大绘制工具按钮
            ToolBtn(DrawingToolType::Pen, "\xe2\x9c\x8e", LanguageManager::GetText("DRAWING_TOOL_PEN"));      // ✏
            ToolBtn(DrawingToolType::Line, "\xe2\x80\x95", LanguageManager::GetText("DRAWING_TOOL_LINE"));    // ―
            ToolBtn(DrawingToolType::Arrow, "\xe2\x9e\x9c", LanguageManager::GetText("DRAWING_TOOL_ARROW"));  // ➜
            ToolBtn(DrawingToolType::Rect, "\xe2\x96\xad", LanguageManager::GetText("DRAWING_TOOL_RECT"));    // ▭
            ToolBtn(DrawingToolType::Circle, "\xe2\x97\x8b", LanguageManager::GetText("DRAWING_TOOL_CIRCLE"));// ○
            ToolBtn(DrawingToolType::Text, "T", LanguageManager::GetText("DRAWING_TOOL_TEXT"));                // T
            ToolBtn(DrawingToolType::Eraser, "\xe2\x8c\xab", LanguageManager::GetText("DRAWING_TOOL_ERASER"));// ⌫

            ImGui::Spacing();
            ImGui::SameLine();

            // 快捷调色盘
            static const float kQuickPalette[8][4] = {
                { 1.00f, 0.25f, 0.25f, 0.95f }, // 红
                { 1.00f, 0.60f, 0.15f, 0.95f }, // 橙
                { 1.00f, 0.90f, 0.20f, 0.95f }, // 黄
                { 0.25f, 0.85f, 0.35f, 0.95f }, // 绿
                { 0.15f, 0.80f, 0.95f, 0.95f }, // 青
                { 0.30f, 0.50f, 1.00f, 0.95f }, // 蓝
                { 0.85f, 0.35f, 0.95f, 0.95f }, // 紫
                { 1.00f, 1.00f, 1.00f, 0.95f }  // 白
            };

            float swatchSz = 18.0f * curScale;
            for (int i = 0; i < 8; ++i) {
                ImVec4 c(kQuickPalette[i][0], kQuickPalette[i][1], kQuickPalette[i][2], kQuickPalette[i][3]);
                ImGui::PushStyleColor(ImGuiCol_Button, c);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
                char swId[32];
                snprintf(swId, sizeof(swId), "##sw_%d", i);
                if (ImGui::Button(swId, ImVec2(swatchSz, swatchSz))) {
                    for (int k = 0; k < 4; ++k) {
                        g_brushColor[k] = kQuickPalette[i][k];
                        g_fillColor[k] = kQuickPalette[i][k];
                    }
                    g_fillColor[3] = 0.25f;
                }
                ImGui::PopStyleColor(3);

                // 选中颜色光圈高亮
                bool isColSelected = (std::abs(g_brushColor[0] - kQuickPalette[i][0]) < 0.05f &&
                                      std::abs(g_brushColor[1] - kQuickPalette[i][1]) < 0.05f &&
                                      std::abs(g_brushColor[2] - kQuickPalette[i][2]) < 0.05f);
                if (isColSelected) {
                    drawList->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 255, 255), 2.0f, 0, 2.0f);
                }
                ImGui::SameLine();
            }

            // 自定义颜色选择拾色按钮
            if (ImGui::ColorButton("##CustomColorPickerBtn", ImVec4(g_brushColor[0], g_brushColor[1], g_brushColor[2], g_brushColor[3]), ImGuiColorEditFlags_NoAlpha, ImVec2(swatchSz, swatchSz))) {
                ImGui::OpenPopup("CustomColorPickerPopup");
            }
            if (ImGui::BeginPopup("CustomColorPickerPopup")) {
                ImGui::ColorPicker4("##FullPicker", g_brushColor, ImGuiColorEditFlags_AlphaBar);
                for (int k = 0; k < 3; ++k) g_fillColor[k] = g_brushColor[k];
                ImGui::EndPopup();
            }
            ImGui::SameLine();

            // 填充多边形开关 (支持矩形与圆形，其它工具禁用呈现，避免切换工具导致工具栏宽度突变跳动)
            bool canFill = (g_currentTool == DrawingToolType::Rect || g_currentTool == DrawingToolType::Circle);
            if (!canFill) ImGui::BeginDisabled();
            ImGui::Checkbox(LanguageManager::GetText("DRAWING_FILL"), &g_shapeFilled);
            if (!canFill) ImGui::EndDisabled();
            ImGui::SameLine();

            // 线宽调节滑动条
            ImGui::PushItemWidth(85.0f * curScale);
            ImGui::SliderFloat("##BrushThickSlider", &g_brushThickness, 1.0f, 16.0f, "%.0f px");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LanguageManager::GetText("DRAWING_STROKE_WIDTH"));
            ImGui::SameLine();

            // 撤销 / 重做
            if (!CanUndo()) ImGui::BeginDisabled();
            if (ImGui::Button("\xe2\x86\xb6##UndoBtn", ImVec2(btnSz, btnSz))) Undo(); // ↶
            if (!CanUndo()) ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (Ctrl+Z)", LanguageManager::GetText("DRAWING_UNDO"));
            ImGui::SameLine();

            if (!CanRedo()) ImGui::BeginDisabled();
            if (ImGui::Button("\xe2\x86\xb7##RedoBtn", ImVec2(btnSz, btnSz))) Redo(); // ↷
            if (!CanRedo()) ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (Ctrl+Y)", LanguageManager::GetText("DRAWING_REDO"));
            ImGui::SameLine();

            // 图层管理抽屉按钮
            bool isLayerOpen = s_showLayersDrawer;
            if (isLayerOpen) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.70f, 0.95f));
            }
            if (ImGui::Button("\xe2\x96\x92##LayersBtn", ImVec2(btnSz, btnSz))) { // ▒
                s_showLayersDrawer = !s_showLayersDrawer;
            }
            if (isLayerOpen) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LanguageManager::GetText("DRAWING_LAYERS"));
            ImGui::SameLine();

            // 退出涂鸦模式
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.20f, 0.20f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Button("\xe2\x9c\x95##CloseDrawingBtn", ImVec2(btnSz, btnSz))) { // ✕
                g_drawingModeActive = false;
                g_isDrawingNow = false;
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LanguageManager::GetText("CLOSE"));

            ImGui::End();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        // ==========================================
        // 浮动图层管理器面板 (Layer Manager Drawer)
        // ==========================================
        if (s_showLayersDrawer) {
            float layW = 320.0f * curScale;
            float layH = 340.0f * curScale;
            float layX = std::clamp(tbWinPos.x + tbWinSize.x - layW, 10.0f * curScale, displaySize.x - layW - 10.0f * curScale);
            float layY = tbWinPos.y + tbWinSize.y + 8.0f * curScale;

            ImGui::SetNextWindowPos(ImVec2(layX, layY), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(layW, layH));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f * curScale);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.13f, 0.19f, 0.96f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.60f, 0.90f, 0.85f));

            if (ImGui::Begin(LanguageManager::GetText("DRAWING_LAYERS"), &s_showLayersDrawer, ImGuiWindowFlags_NoCollapse)) {
                ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "%s", LanguageManager::GetText("DRAWING_LAYERS"));
                ImGui::Separator();

                // 新建图层输入栏
                ImGui::PushItemWidth(layW - 90.0f * curScale);
                bool enterNewLayer = InputIMEHelper::InputTextWithIME("##NewLayerName", s_newLayerNameBuf, sizeof(s_newLayerNameBuf), LanguageManager::GetText("WP_FOLDER_NAME"), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("DRAWING_LAYER_NEW")) || enterNewLayer) {
                    AddLayer(s_newLayerNameBuf);
                    s_newLayerNameBuf[0] = '\0';
                }

                ImGui::Spacing();
                ImGui::BeginChild("LayersListChild", ImVec2(0.0f, layH - 145.0f * curScale), true);

                for (size_t i = 0; i < s_layers.size(); ++i) {
                    auto& lay = s_layers[i];
                    ImGui::PushID(lay.id.c_str());

                    // 可见性眼球开关
                    const char* eyeIcon = lay.visible ? "\xe2\x97\x89" : "\xe2\x97\x8c"; // ◉ vs ◌
                    if (ImGui::Button(eyeIcon, ImVec2(22.0f * curScale, 22.0f * curScale))) {
                        lay.visible = !lay.visible;
                        SaveDrawings();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", lay.visible ? LanguageManager::GetText("DRAWING_LAYER_HIDDEN") : LanguageManager::GetText("DRAWING_LAYER_VISIBLE"));
                    }
                    ImGui::SameLine();

                    // 锁定状态按钮
                    const char* lockIcon = lay.locked ? "\xe2\x94\xb4" : "\xe2\x94\xbc"; // 锁定 vs 解锁
                    if (ImGui::Button(lockIcon, ImVec2(22.0f * curScale, 22.0f * curScale))) {
                        lay.locked = !lay.locked;
                        SaveDrawings();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", lay.locked ? LanguageManager::GetText("DRAWING_LAYER_LOCKED") : LanguageManager::GetText("DRAWING_LAYER_UNLOCKED"));
                    }
                    ImGui::SameLine();

                    // 图层激活单选选择
                    bool isCurActive = (lay.id == g_activeLayerId);
                    char layLabel[128];
                    snprintf(layLabel, sizeof(layLabel), "%s (%zu)", lay.name.c_str(), lay.elements.size());
                    if (ImGui::Selectable(layLabel, isCurActive, 0, ImVec2(layW - 160.0f * curScale, 22.0f * curScale))) {
                        g_activeLayerId = lay.id;
                    }

                    // 删除按钮 (仅当大于 1 个图层时可用)
                    if (s_layers.size() > 1) {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                        if (ImGui::Button("\xe2\x9c\x95##DelLay", ImVec2(22.0f * curScale, 22.0f * curScale))) {
                            RemoveLayer(lay.id);
                            ImGui::PopStyleColor();
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopStyleColor();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LanguageManager::GetText("DRAWING_LAYER_DELETE"));
                    }

                    // 不透明度微调滑块
                    ImGui::SetNextItemWidth(layW - 70.0f * curScale);
                    if (ImGui::SliderFloat("##OpacitySlider", &lay.opacity, 0.1f, 1.0f, "%.2f")) {
                        SaveDrawings();
                    }

                    ImGui::PopID();
                    ImGui::Separator();
                }
                ImGui::EndChild();

                // 底部清空当前图层操作
                DrawingLayer* actLay = GetActiveLayer();
                if (actLay && !actLay->locked && !actLay->elements.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.22f, 0.22f, 0.85f));
                    if (ImGui::Button(LanguageManager::GetText("DRAWING_CLEAR_LAYER"), ImVec2(-1.0f, 24.0f * curScale))) {
                        s_confirmClearLayer = true;
                    }
                    ImGui::PopStyleColor();
                }

                ImGui::End();
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }

        // 清空确认弹窗
        if (s_confirmClearLayer) {
            ImGui::OpenPopup("ConfirmClearLayerModal");
            s_confirmClearLayer = false;
        }
        if (ImGui::BeginPopupModal("ConfirmClearLayerModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", LanguageManager::GetText("DRAWING_CLEAR_LAYER_CONFIRM"));
            ImGui::Spacing();
            if (ImGui::Button(LanguageManager::GetText("CONFIRM"), ImVec2(100.0f * curScale, 0))) {
                ClearActiveLayer();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(LanguageManager::GetText("CANCEL"), ImVec2(100.0f * curScale, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // ==========================================
        // 文字标注输入模态框 (Add Text Annotation)
        // ==========================================
        if (g_showTextInputModal) {
            ImGui::OpenPopup("AddTextAnnotationModal");
            g_showTextInputModal = false;
        }

        if (ImGui::BeginPopupModal("AddTextAnnotationModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "%s", LanguageManager::GetText("DRAWING_TEXT_INPUT_TITLE"));
            ImGui::Separator();
            ImGui::Text("%s", LanguageManager::GetText("DRAWING_TEXT_PROMPT"));

            ImGui::PushItemWidth(260.0f * curScale);
            bool enterPressed = InputIMEHelper::InputTextWithIME("##TextAnnotationInput", g_textInputBuffer, sizeof(g_textInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();

            ImGui::SliderFloat(LanguageManager::GetText("DRAWING_TEXT_FONT_SIZE"), &g_textFontSize, 12.0f, 32.0f, "%.0f pt");

            ImGui::Spacing();
            if (ImGui::Button(LanguageManager::GetText("CONFIRM"), ImVec2(120.0f * curScale, 0)) || enterPressed) {
                if (g_textInputBuffer[0] != '\0') {
                    DrawingElement elem;
                    elem.id = GenerateUUID();
                    elem.type = DrawingToolType::Text;
                    elem.points = { g_pendingTextPos };
                    elem.text = g_textInputBuffer;
                    elem.fontSize = g_textFontSize;
                    elem.thickness = 2.0f;
                    elem.createdAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    for (int i = 0; i < 4; ++i) elem.color[i] = g_brushColor[i];
                    elem.RecomputeBounds();
                    AddElementToActiveLayer(elem);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(LanguageManager::GetText("CANCEL"), ImVec2(120.0f * curScale, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

} // namespace DrawingManager
