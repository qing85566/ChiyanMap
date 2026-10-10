#include "render/BlockColorMap.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <fstream>
#include "mod/ChiyanMap.h"

namespace BlockColorMap {

static void LogColorMissInternal(std::string const& tag, std::string const& name) {
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

bool IsInvisibleOrTechnicalOverlay(std::string const& rawName) noexcept {
    if (rawName.empty()) return true;
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    if (name == "minecraft:air" || name == "air") return true;
    if (name.find("barrier") != std::string::npos) return true;
    if (name.find("light_block") != std::string::npos) return true;
    if (name.find("structure_void") != std::string::npos) return true;
    if (name.find("placeholder") != std::string::npos) return true;
    if (name.find("unknown") != std::string::npos) return true;
    if (name.find("info_update") != std::string::npos) return true;
    if (name.find("invisiblebedrock") != std::string::npos || name.find("invisible_bedrock") != std::string::npos) return true;
    if (name.find("movingblock") != std::string::npos || name.find("moving_block") != std::string::npos) return true;
    if (name.find("pistonarmcollision") != std::string::npos || name.find("piston_arm_collision") != std::string::npos) return true;
    if (name.find("border_block") != std::string::npos) return true;
    if (name == "minecraft:allow" || name == "allow" || name == "minecraft:deny" || name == "deny") return true;
    if (name == "minecraft:camera" || name == "camera") return true;
    if (name.find("tripwire") != std::string::npos || name.find("trip_wire") != std::string::npos) return true;
    if (name.find("frame") != std::string::npos && name.find("end_portal_frame") == std::string::npos) return true;
    if (name.find("reserved") != std::string::npos) return true;
    return false;
}

// [原木与菌柄判断] 检测方块是否为有年轮/树皮朝向区分的原木或菌柄
bool IsPillarLogOrStem(std::string const& rawName) noexcept {
    if (rawName.empty()) return false;
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    if (name.find("wood") != std::string::npos || name.find("hyphae") != std::string::npos) return false;
    if (name.find("log") != std::string::npos) return true;
    if (name.find("crimson_stem") != std::string::npos || name.find("warped_stem") != std::string::npos) return true;
    return false;
}

// [水平放置原木转树皮名称] 水平放置时映射为对应树皮材质 (_log -> _wood, _stem -> _hyphae)
std::string GetHorizontalWoodName(std::string const& name) {
    std::string result = name;
    size_t pos = result.find("stem");
    if (pos != std::string::npos) {
        result.replace(pos, 4, "hyphae");
        return result;
    }
    pos = result.find("log");
    if (pos != std::string::npos) {
        result.replace(pos, 3, "wood");
        return result;
    }
    return result;
}

mce::Color GetBlockColor(std::string const& rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol) {
    // 统一转为全小写，杜绝基岩版 camelCase 命名（如 invisibleBedrock, seaLantern, tripWire, concretePowder）匹配问题
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    if (IsInvisibleOrTechnicalOverlay(name)) {
        return mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // =========================================================================
    // [1176 全量方块像素级精准色彩查找表 - 融合地图材质采样与基岩版特调]
    // =========================================================================
    static const std::unordered_map<std::string, mce::Color> s_exactBlockColors = {
        {"acacia_button", mce::Color(0.706f, 0.384f, 0.212f, 1.00f)},
        {"acacia_door", mce::Color(0.627f, 0.353f, 0.220f, 1.00f)},
        {"acacia_fence", mce::Color(0.706f, 0.384f, 0.212f, 1.00f)},
        {"acacia_fence_gate", mce::Color(0.706f, 0.384f, 0.212f, 1.00f)},
        {"acacia_hanging_sign", mce::Color(0.604f, 0.345f, 0.255f, 1.00f)},
        {"acacia_log", mce::Color(0.588f, 0.345f, 0.212f, 1.00f)},
        {"acacia_planks", mce::Color(0.706f, 0.384f, 0.212f, 1.00f)},
        {"acacia_pressure_plate", mce::Color(0.706f, 0.384f, 0.212f, 1.00f)},
        {"acacia_sapling", mce::Color(0.475f, 0.463f, 0.094f, 1.00f)},
        {"acacia_shelf", mce::Color(0.592f, 0.325f, 0.200f, 1.00f)},
        {"acacia_sign", mce::Color(0.647f, 0.373f, 0.224f, 1.00f)},
        {"acacia_slab", mce::Color(0.706f, 0.384f, 0.212f, 1.00f)},
        {"acacia_stairs", mce::Color(0.706f, 0.384f, 0.212f, 1.00f)},
        {"acacia_trapdoor", mce::Color(0.612f, 0.341f, 0.204f, 1.00f)},
        {"acacia_wall_hanging_sign", mce::Color(0.604f, 0.345f, 0.255f, 1.00f)},
        {"acacia_wall_sign", mce::Color(0.647f, 0.373f, 0.224f, 1.00f)},
        {"acacia_wood", mce::Color(0.424f, 0.396f, 0.357f, 1.00f)},
        {"activator_rail", mce::Color(0.396f, 0.306f, 0.271f, 1.00f)},
        {"allium", mce::Color(0.714f, 0.502f, 0.890f, 1.00f)},
        {"amethyst_block", mce::Color(0.537f, 0.396f, 0.753f, 1.00f)},
        {"amethyst_cluster", mce::Color(0.651f, 0.502f, 0.827f, 1.00f)},
        {"ancient_debris", mce::Color(0.373f, 0.259f, 0.227f, 1.00f)},
        {"andesite", mce::Color(0.529f, 0.533f, 0.533f, 1.00f)},
        {"andesite_slab", mce::Color(0.529f, 0.533f, 0.533f, 1.00f)},
        {"andesite_stairs", mce::Color(0.529f, 0.533f, 0.533f, 1.00f)},
        {"andesite_wall", mce::Color(0.529f, 0.533f, 0.533f, 1.00f)},
        {"anvil", mce::Color(0.286f, 0.286f, 0.286f, 1.00f)},
        {"azalea", mce::Color(0.404f, 0.498f, 0.184f, 1.00f)},
        {"azalea_leaves", mce::Color(0.345f, 0.443f, 0.169f, 1.00f)},
        {"azalea_leaves_flowered", mce::Color(0.353f, 0.431f, 0.196f, 1.00f)},
        {"azure_bluet", mce::Color(0.910f, 0.940f, 0.940f, 1.00f)},
        {"bamboo", mce::Color(0.337f, 0.533f, 0.051f, 1.00f)},
        {"bamboo_block", mce::Color(0.514f, 0.576f, 0.227f, 1.00f)},
        {"bamboo_button", mce::Color(0.776f, 0.698f, 0.306f, 1.00f)},
        {"bamboo_door", mce::Color(0.761f, 0.678f, 0.322f, 1.00f)},
        {"bamboo_fence", mce::Color(0.800f, 0.722f, 0.337f, 1.00f)},
        {"bamboo_fence_gate", mce::Color(0.839f, 0.753f, 0.361f, 1.00f)},
        {"bamboo_hanging_sign", mce::Color(0.722f, 0.663f, 0.361f, 1.00f)},
        {"bamboo_mosaic", mce::Color(0.765f, 0.690f, 0.298f, 1.00f)},
        {"bamboo_mosaic_slab", mce::Color(0.765f, 0.690f, 0.298f, 1.00f)},
        {"bamboo_mosaic_stairs", mce::Color(0.765f, 0.690f, 0.298f, 1.00f)},
        {"bamboo_planks", mce::Color(0.776f, 0.698f, 0.306f, 1.00f)},
        {"bamboo_pressure_plate", mce::Color(0.776f, 0.698f, 0.306f, 1.00f)},
        {"bamboo_sapling", mce::Color(0.337f, 0.325f, 0.122f, 1.00f)},
        {"bamboo_shelf", mce::Color(0.706f, 0.643f, 0.282f, 1.00f)},
        {"bamboo_sign", mce::Color(0.831f, 0.745f, 0.357f, 1.00f)},
        {"bamboo_slab", mce::Color(0.776f, 0.698f, 0.306f, 1.00f)},
        {"bamboo_stairs", mce::Color(0.776f, 0.698f, 0.306f, 1.00f)},
        {"bamboo_trapdoor", mce::Color(0.780f, 0.706f, 0.333f, 1.00f)},
        {"bamboo_wall_hanging_sign", mce::Color(0.722f, 0.663f, 0.361f, 1.00f)},
        {"bamboo_wall_sign", mce::Color(0.831f, 0.745f, 0.357f, 1.00f)},
        {"barrel", mce::Color(0.529f, 0.392f, 0.227f, 1.00f)},
        {"basalt", mce::Color(0.310f, 0.314f, 0.333f, 1.00f)},
        {"beacon", mce::Color(0.482f, 0.871f, 0.847f, 1.00f)},
        {"bedrock", mce::Color(0.357f, 0.357f, 0.357f, 1.00f)},
        {"bee_nest", mce::Color(0.788f, 0.620f, 0.294f, 1.00f)},
        {"beehive", mce::Color(0.706f, 0.573f, 0.353f, 1.00f)},
        {"beetroots", mce::Color(0.298f, 0.588f, 0.161f, 1.00f)},
        {"bell", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"big_dripleaf", mce::Color(0.494f, 0.627f, 0.224f, 1.00f)},
        {"big_dripleaf_stem", mce::Color(0.373f, 0.463f, 0.180f, 1.00f)},
        {"birch_button", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"birch_door", mce::Color(0.843f, 0.804f, 0.671f, 1.00f)},
        {"birch_fence", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"birch_fence_gate", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"birch_hanging_sign", mce::Color(0.682f, 0.624f, 0.451f, 1.00f)},
        {"birch_leaves", mce::Color(0.263f, 0.341f, 0.173f, 1.00f)},
        {"birch_log", mce::Color(0.729f, 0.682f, 0.541f, 1.00f)},
        {"birch_planks", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"birch_pressure_plate", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"birch_sapling", mce::Color(0.537f, 0.655f, 0.349f, 1.00f)},
        {"birch_shelf", mce::Color(0.667f, 0.588f, 0.400f, 1.00f)},
        {"birch_sign", mce::Color(0.788f, 0.733f, 0.533f, 1.00f)},
        {"birch_slab", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"birch_stairs", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"birch_trapdoor", mce::Color(0.804f, 0.753f, 0.608f, 1.00f)},
        {"birch_wall_hanging_sign", mce::Color(0.682f, 0.624f, 0.451f, 1.00f)},
        {"birch_wall_sign", mce::Color(0.788f, 0.733f, 0.533f, 1.00f)},
        {"birch_wood", mce::Color(0.839f, 0.835f, 0.816f, 1.00f)},
        {"black_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"black_bed", mce::Color(0.129f, 0.129f, 0.149f, 1.00f)},
        {"black_candle", mce::Color(0.145f, 0.137f, 0.220f, 1.00f)},
        {"black_candle_cake", mce::Color(0.145f, 0.137f, 0.220f, 1.00f)},
        {"black_carpet", mce::Color(0.082f, 0.082f, 0.102f, 1.00f)},
        {"black_concrete", mce::Color(0.031f, 0.039f, 0.059f, 1.00f)},
        {"black_concrete_powder", mce::Color(0.094f, 0.102f, 0.122f, 1.00f)},
        {"black_glazed_terracotta", mce::Color(0.341f, 0.141f, 0.153f, 1.00f)},
        {"black_shulker_box", mce::Color(0.098f, 0.102f, 0.118f, 1.00f)},
        {"black_stained_glass", mce::Color(0.098f, 0.098f, 0.098f, 1.00f)},
        {"black_stained_glass_pane", mce::Color(0.090f, 0.090f, 0.090f, 1.00f)},
        {"black_terracotta", mce::Color(0.145f, 0.090f, 0.063f, 1.00f)},
        {"black_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"black_wool", mce::Color(0.082f, 0.082f, 0.102f, 1.00f)},
        {"blackstone", mce::Color(0.161f, 0.141f, 0.157f, 1.00f)},
        {"blackstone_slab", mce::Color(0.161f, 0.141f, 0.157f, 1.00f)},
        {"blackstone_stairs", mce::Color(0.161f, 0.141f, 0.157f, 1.00f)},
        {"blackstone_wall", mce::Color(0.165f, 0.137f, 0.161f, 1.00f)},
        {"blast_furnace", mce::Color(0.318f, 0.314f, 0.318f, 1.00f)},
        {"blue_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"blue_bed", mce::Color(0.255f, 0.349f, 0.722f, 1.00f)},
        {"blue_candle", mce::Color(0.227f, 0.302f, 0.647f, 1.00f)},
        {"blue_candle_cake", mce::Color(0.227f, 0.302f, 0.647f, 1.00f)},
        {"blue_carpet", mce::Color(0.208f, 0.224f, 0.616f, 1.00f)},
        {"blue_concrete", mce::Color(0.173f, 0.180f, 0.561f, 1.00f)},
        {"blue_concrete_powder", mce::Color(0.275f, 0.286f, 0.651f, 1.00f)},
        {"blue_glazed_terracotta", mce::Color(0.169f, 0.220f, 0.502f, 1.00f)},
        {"blue_ice", mce::Color(0.455f, 0.655f, 0.992f, 1.00f)},
        {"blue_orchid", mce::Color(0.180f, 0.643f, 0.757f, 1.00f)},
        {"blue_shulker_box", mce::Color(0.173f, 0.180f, 0.557f, 1.00f)},
        {"blue_stained_glass", mce::Color(0.200f, 0.298f, 0.698f, 1.00f)},
        {"blue_stained_glass_pane", mce::Color(0.184f, 0.282f, 0.663f, 1.00f)},
        {"blue_terracotta", mce::Color(0.290f, 0.231f, 0.357f, 1.00f)},
        {"blue_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"blue_wool", mce::Color(0.208f, 0.224f, 0.616f, 1.00f)},
        {"bone_block", mce::Color(0.820f, 0.808f, 0.702f, 1.00f)},
        {"bookshelf", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"brain_coral", mce::Color(0.776f, 0.322f, 0.588f, 1.00f)},
        {"brain_coral_block", mce::Color(0.808f, 0.353f, 0.624f, 1.00f)},
        {"brain_coral_fan", mce::Color(0.800f, 0.337f, 0.612f, 1.00f)},
        {"brain_coral_wall_fan", mce::Color(0.800f, 0.337f, 0.612f, 1.00f)},
        {"brewing_stand", mce::Color(0.451f, 0.408f, 0.408f, 1.00f)},
        {"brick_block", mce::Color(0.580f, 0.318f, 0.247f, 1.00f)},
        {"brick_slab", mce::Color(0.580f, 0.318f, 0.247f, 1.00f)},
        {"brick_stairs", mce::Color(0.580f, 0.318f, 0.247f, 1.00f)},
        {"brick_wall", mce::Color(0.580f, 0.318f, 0.247f, 1.00f)},
        {"bricks", mce::Color(0.580f, 0.318f, 0.247f, 1.00f)},
        {"brown_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"brown_bed", mce::Color(0.541f, 0.341f, 0.196f, 1.00f)},
        {"brown_candle", mce::Color(0.447f, 0.278f, 0.153f, 1.00f)},
        {"brown_candle_cake", mce::Color(0.447f, 0.278f, 0.153f, 1.00f)},
        {"brown_carpet", mce::Color(0.447f, 0.282f, 0.157f, 1.00f)},
        {"brown_concrete", mce::Color(0.376f, 0.231f, 0.122f, 1.00f)},
        {"brown_concrete_powder", mce::Color(0.486f, 0.329f, 0.208f, 1.00f)},
        {"brown_glazed_terracotta", mce::Color(0.475f, 0.416f, 0.333f, 1.00f)},
        {"brown_mushroom", mce::Color(0.639f, 0.486f, 0.384f, 1.00f)},
        {"brown_mushroom_block", mce::Color(0.584f, 0.435f, 0.318f, 1.00f)},
        {"brown_shulker_box", mce::Color(0.420f, 0.259f, 0.141f, 1.00f)},
        {"brown_stained_glass", mce::Color(0.400f, 0.298f, 0.200f, 1.00f)},
        {"brown_stained_glass_pane", mce::Color(0.376f, 0.282f, 0.184f, 1.00f)},
        {"brown_terracotta", mce::Color(0.302f, 0.200f, 0.137f, 1.00f)},
        {"brown_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"brown_wool", mce::Color(0.447f, 0.282f, 0.157f, 1.00f)},
        {"bubble_coral", mce::Color(0.643f, 0.090f, 0.635f, 1.00f)},
        {"bubble_coral_block", mce::Color(0.647f, 0.098f, 0.635f, 1.00f)},
        {"bubble_coral_fan", mce::Color(0.651f, 0.145f, 0.643f, 1.00f)},
        {"bubble_coral_wall_fan", mce::Color(0.651f, 0.145f, 0.643f, 1.00f)},
        {"budding_amethyst", mce::Color(0.522f, 0.373f, 0.737f, 1.00f)},
        {"cactus_flower", mce::Color(0.822f, 0.473f, 0.531f, 1.00f)},
        {"cake", mce::Color(0.973f, 0.875f, 0.839f, 1.00f)},
        {"calcite", mce::Color(0.878f, 0.886f, 0.871f, 1.00f)},
        {"calibrated_sculk_sensor", mce::Color(0.212f, 0.345f, 0.443f, 1.00f)},
        {"campfire", mce::Color(0.860f, 0.620f, 0.228f, 1.00f)},
        {"candle", mce::Color(0.949f, 0.820f, 0.616f, 1.00f)},
        {"candle_cake", mce::Color(0.949f, 0.820f, 0.616f, 1.00f)},
        {"carrots", mce::Color(0.149f, 0.388f, 0.145f, 1.00f)},
        {"cartography_table", mce::Color(0.420f, 0.341f, 0.267f, 1.00f)},
        {"carved_pumpkin", mce::Color(0.773f, 0.463f, 0.098f, 1.00f)},
        {"cauldron", mce::Color(0.290f, 0.286f, 0.290f, 1.00f)},
        {"cave_vines", mce::Color(0.345f, 0.420f, 0.161f, 1.00f)},
        {"cave_vines_plant", mce::Color(0.341f, 0.408f, 0.153f, 1.00f)},
        {"chain_command_block", mce::Color(0.498f, 0.588f, 0.529f, 1.00f)},
        {"cherry_button", mce::Color(0.902f, 0.749f, 0.725f, 1.00f)},
        {"cherry_door", mce::Color(0.882f, 0.690f, 0.667f, 1.00f)},
        {"cherry_fence", mce::Color(0.902f, 0.749f, 0.725f, 1.00f)},
        {"cherry_fence_gate", mce::Color(0.902f, 0.749f, 0.725f, 1.00f)},
        {"cherry_hanging_sign", mce::Color(0.725f, 0.506f, 0.533f, 1.00f)},
        {"cherry_leaves", mce::Color(0.900f, 0.650f, 0.750f, 1.00f)},
        {"cherry_log", mce::Color(0.890f, 0.690f, 0.700f, 1.00f)},
        {"cherry_planks", mce::Color(0.902f, 0.749f, 0.725f, 1.00f)},
        {"cherry_pressure_plate", mce::Color(0.902f, 0.749f, 0.725f, 1.00f)},
        {"cherry_sapling", mce::Color(0.643f, 0.463f, 0.561f, 1.00f)},
        {"cherry_shelf", mce::Color(0.773f, 0.486f, 0.486f, 1.00f)},
        {"cherry_sign", mce::Color(0.812f, 0.651f, 0.639f, 1.00f)},
        {"cherry_slab", mce::Color(0.902f, 0.749f, 0.725f, 1.00f)},
        {"cherry_stairs", mce::Color(0.902f, 0.749f, 0.725f, 1.00f)},
        {"cherry_trapdoor", mce::Color(0.894f, 0.733f, 0.710f, 1.00f)},
        {"cherry_wall_hanging_sign", mce::Color(0.725f, 0.506f, 0.533f, 1.00f)},
        {"cherry_wall_sign", mce::Color(0.812f, 0.651f, 0.639f, 1.00f)},
        {"cherry_wood", mce::Color(0.212f, 0.125f, 0.173f, 1.00f)},
        {"chest", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"chipped_anvil", mce::Color(0.278f, 0.278f, 0.278f, 1.00f)},
        {"chiseled_bookshelf", mce::Color(0.702f, 0.573f, 0.353f, 1.00f)},
        {"chiseled_cinnabar", mce::Color(0.584f, 0.224f, 0.220f, 1.00f)},
        {"chiseled_copper", mce::Color(0.765f, 0.424f, 0.322f, 1.00f)},
        {"chiseled_deepslate", mce::Color(0.212f, 0.216f, 0.216f, 1.00f)},
        {"chiseled_nether_bricks", mce::Color(0.196f, 0.094f, 0.114f, 1.00f)},
        {"chiseled_polished_blackstone", mce::Color(0.208f, 0.188f, 0.216f, 1.00f)},
        {"chiseled_quartz_block", mce::Color(0.906f, 0.886f, 0.847f, 1.00f)},
        {"chiseled_red_sandstone", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"chiseled_resin_bricks", mce::Color(0.808f, 0.357f, 0.106f, 1.00f)},
        {"chiseled_sandstone", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"chiseled_stone_bricks", mce::Color(0.463f, 0.459f, 0.463f, 1.00f)},
        {"chiseled_sulfur", mce::Color(0.757f, 0.702f, 0.369f, 1.00f)},
        {"chiseled_tuff", mce::Color(0.365f, 0.384f, 0.357f, 1.00f)},
        {"chiseled_tuff_bricks", mce::Color(0.439f, 0.451f, 0.420f, 1.00f)},
        {"chorus_flower", mce::Color(0.369f, 0.224f, 0.365f, 1.00f)},
        {"chorus_plant", mce::Color(0.369f, 0.224f, 0.365f, 1.00f)},
        {"cinnabar", mce::Color(0.588f, 0.314f, 0.290f, 1.00f)},
        {"cinnabar_brick", mce::Color(0.580f, 0.220f, 0.212f, 1.00f)},
        {"cinnabar_brick_slab", mce::Color(0.565f, 0.208f, 0.196f, 1.00f)},
        {"cinnabar_brick_stairs", mce::Color(0.514f, 0.192f, 0.184f, 1.00f)},
        {"cinnabar_brick_wall", mce::Color(0.482f, 0.275f, 0.204f, 1.00f)},
        {"cinnabar_bricks", mce::Color(0.624f, 0.247f, 0.235f, 1.00f)},
        {"cinnabar_slab", mce::Color(0.565f, 0.314f, 0.298f, 1.00f)},
        {"cinnabar_stairs", mce::Color(0.576f, 0.318f, 0.306f, 1.00f)},
        {"clay", mce::Color(0.627f, 0.651f, 0.706f, 1.00f)},
        {"closed_eyeblossom", mce::Color(0.424f, 0.386f, 0.397f, 1.00f)},
        {"coal_block", mce::Color(0.059f, 0.059f, 0.059f, 1.00f)},
        {"coal_ore", mce::Color(0.400f, 0.400f, 0.400f, 1.00f)},
        {"coarse_dirt", mce::Color(0.467f, 0.333f, 0.231f, 1.00f)},
        {"cobbled_deepslate", mce::Color(0.310f, 0.310f, 0.322f, 1.00f)},
        {"cobbled_deepslate_slab", mce::Color(0.310f, 0.310f, 0.322f, 1.00f)},
        {"cobbled_deepslate_stairs", mce::Color(0.310f, 0.310f, 0.322f, 1.00f)},
        {"cobbled_deepslate_wall", mce::Color(0.310f, 0.310f, 0.322f, 1.00f)},
        {"cobblestone", mce::Color(0.498f, 0.498f, 0.498f, 1.00f)},
        {"cobblestone_slab", mce::Color(0.498f, 0.498f, 0.498f, 1.00f)},
        {"cobblestone_stairs", mce::Color(0.498f, 0.498f, 0.498f, 1.00f)},
        {"cobblestone_wall", mce::Color(0.498f, 0.498f, 0.498f, 1.00f)},
        {"cobweb", mce::Color(0.910f, 0.925f, 0.929f, 1.00f)},
        {"cocoa", mce::Color(0.522f, 0.529f, 0.243f, 1.00f)},
        {"colored_torch_blue", mce::Color(0.200f, 0.500f, 0.950f, 1.00f)},
        {"colored_torch_bp", mce::Color(0.600f, 0.300f, 0.900f, 1.00f)},
        {"colored_torch_green", mce::Color(0.200f, 0.850f, 0.200f, 1.00f)},
        {"colored_torch_purple", mce::Color(0.750f, 0.250f, 0.850f, 1.00f)},
        {"colored_torch_red", mce::Color(0.950f, 0.150f, 0.150f, 1.00f)},
        {"colored_torch_rg", mce::Color(0.850f, 0.850f, 0.200f, 1.00f)},
        {"command_block", mce::Color(0.682f, 0.475f, 0.361f, 1.00f)},
        {"comparator", mce::Color(0.306f, 0.200f, 0.125f, 1.00f)},
        {"composter", mce::Color(0.596f, 0.384f, 0.200f, 1.00f)},
        {"conduit", mce::Color(0.635f, 0.549f, 0.451f, 1.00f)},
        {"copper_bars", mce::Color(0.635f, 0.337f, 0.235f, 1.00f)},
        {"copper_block", mce::Color(0.792f, 0.447f, 0.337f, 1.00f)},
        {"copper_bulb", mce::Color(0.651f, 0.369f, 0.251f, 1.00f)},
        {"copper_chain", mce::Color(0.596f, 0.310f, 0.216f, 1.00f)},
        {"copper_chest", mce::Color(0.792f, 0.447f, 0.337f, 1.00f)},
        {"copper_door", mce::Color(0.765f, 0.431f, 0.329f, 1.00f)},
        {"copper_golem_statue", mce::Color(0.792f, 0.447f, 0.337f, 1.00f)},
        {"copper_grate", mce::Color(0.796f, 0.451f, 0.341f, 1.00f)},
        {"copper_lantern", mce::Color(0.643f, 0.431f, 0.302f, 1.00f)},
        {"copper_ore", mce::Color(0.498f, 0.486f, 0.459f, 1.00f)},
        {"copper_torch", mce::Color(0.200f, 0.880f, 0.500f, 1.00f)},
        {"copper_trapdoor", mce::Color(0.784f, 0.439f, 0.341f, 1.00f)},
        {"copper_wall_torch", mce::Color(0.200f, 0.880f, 0.500f, 1.00f)},
        {"cornflower", mce::Color(0.325f, 0.427f, 0.875f, 1.00f)},
        {"cracked_deepslate_bricks", mce::Color(0.278f, 0.278f, 0.282f, 1.00f)},
        {"cracked_deepslate_tiles", mce::Color(0.212f, 0.212f, 0.212f, 1.00f)},
        {"cracked_nether_bricks", mce::Color(0.188f, 0.090f, 0.110f, 1.00f)},
        {"cracked_polished_blackstone_bricks", mce::Color(0.184f, 0.157f, 0.184f, 1.00f)},
        {"cracked_stone_bricks", mce::Color(0.486f, 0.482f, 0.486f, 1.00f)},
        {"crafter", mce::Color(0.435f, 0.380f, 0.388f, 1.00f)},
        {"crafting_table", mce::Color(0.471f, 0.286f, 0.169f, 1.00f)},
        {"creaking_heart", mce::Color(0.267f, 0.216f, 0.200f, 1.00f)},
        {"creeper_head", mce::Color(0.455f, 0.742f, 0.423f, 1.00f)},
        {"creeper_wall_head", mce::Color(0.455f, 0.742f, 0.423f, 1.00f)},
        {"crimson_button", mce::Color(0.455f, 0.216f, 0.314f, 1.00f)},
        {"crimson_door", mce::Color(0.451f, 0.216f, 0.310f, 1.00f)},
        {"crimson_fence", mce::Color(0.455f, 0.216f, 0.314f, 1.00f)},
        {"crimson_fence_gate", mce::Color(0.455f, 0.216f, 0.314f, 1.00f)},
        {"crimson_fungus", mce::Color(0.549f, 0.145f, 0.098f, 1.00f)},
        {"crimson_hanging_sign", mce::Color(0.486f, 0.235f, 0.357f, 1.00f)},
        {"crimson_hyphae", mce::Color(0.365f, 0.098f, 0.114f, 1.00f)},
        {"crimson_nylium", mce::Color(0.514f, 0.122f, 0.122f, 1.00f)},
        {"crimson_planks", mce::Color(0.455f, 0.216f, 0.314f, 1.00f)},
        {"crimson_pressure_plate", mce::Color(0.455f, 0.216f, 0.314f, 1.00f)},
        {"crimson_roots", mce::Color(0.529f, 0.031f, 0.173f, 1.00f)},
        {"crimson_shelf", mce::Color(0.494f, 0.196f, 0.314f, 1.00f)},
        {"crimson_sign", mce::Color(0.431f, 0.200f, 0.286f, 1.00f)},
        {"crimson_slab", mce::Color(0.455f, 0.216f, 0.314f, 1.00f)},
        {"crimson_stairs", mce::Color(0.455f, 0.216f, 0.314f, 1.00f)},
        {"crimson_stem", mce::Color(0.439f, 0.192f, 0.275f, 1.00f)},
        {"crimson_trapdoor", mce::Color(0.400f, 0.196f, 0.278f, 1.00f)},
        {"crimson_wall_hanging_sign", mce::Color(0.486f, 0.235f, 0.357f, 1.00f)},
        {"crimson_wall_sign", mce::Color(0.431f, 0.200f, 0.286f, 1.00f)},
        {"crying_obsidian", mce::Color(0.122f, 0.039f, 0.220f, 1.00f)},
        {"cut_copper", mce::Color(0.804f, 0.455f, 0.353f, 1.00f)},
        {"cut_copper_slab", mce::Color(0.804f, 0.455f, 0.353f, 1.00f)},
        {"cut_copper_stairs", mce::Color(0.804f, 0.455f, 0.353f, 1.00f)},
        {"cut_red_sandstone", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"cut_red_sandstone_slab", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"cut_sandstone", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"cut_sandstone_slab", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"cyan_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"cyan_bed", mce::Color(0.071f, 0.604f, 0.600f, 1.00f)},
        {"cyan_candle", mce::Color(0.063f, 0.502f, 0.498f, 1.00f)},
        {"cyan_candle_cake", mce::Color(0.063f, 0.502f, 0.498f, 1.00f)},
        {"cyan_carpet", mce::Color(0.082f, 0.541f, 0.569f, 1.00f)},
        {"cyan_concrete", mce::Color(0.082f, 0.467f, 0.533f, 1.00f)},
        {"cyan_concrete_powder", mce::Color(0.141f, 0.569f, 0.608f, 1.00f)},
        {"cyan_glazed_terracotta", mce::Color(0.345f, 0.557f, 0.569f, 1.00f)},
        {"cyan_shulker_box", mce::Color(0.078f, 0.482f, 0.537f, 1.00f)},
        {"cyan_stained_glass", mce::Color(0.298f, 0.498f, 0.600f, 1.00f)},
        {"cyan_stained_glass_pane", mce::Color(0.282f, 0.475f, 0.569f, 1.00f)},
        {"cyan_terracotta", mce::Color(0.337f, 0.357f, 0.357f, 1.00f)},
        {"cyan_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"cyan_wool", mce::Color(0.082f, 0.541f, 0.569f, 1.00f)},
        {"damaged_anvil", mce::Color(0.271f, 0.271f, 0.271f, 1.00f)},
        {"dandelion", mce::Color(0.957f, 0.804f, 0.251f, 1.00f)},
        {"dark_oak_button", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"dark_oak_door", mce::Color(0.298f, 0.204f, 0.110f, 1.00f)},
        {"dark_oak_fence", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"dark_oak_fence_gate", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"dark_oak_hanging_sign", mce::Color(0.278f, 0.231f, 0.180f, 1.00f)},
        {"dark_oak_log", mce::Color(0.263f, 0.176f, 0.086f, 1.00f)},
        {"dark_oak_planks", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"dark_oak_pressure_plate", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"dark_oak_sapling", mce::Color(0.231f, 0.349f, 0.114f, 1.00f)},
        {"dark_oak_shelf", mce::Color(0.243f, 0.184f, 0.118f, 1.00f)},
        {"dark_oak_sign", mce::Color(0.271f, 0.180f, 0.086f, 1.00f)},
        {"dark_oak_slab", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"dark_oak_stairs", mce::Color(0.294f, 0.192f, 0.090f, 1.00f)},
        {"dark_oak_trapdoor", mce::Color(0.325f, 0.216f, 0.102f, 1.00f)},
        {"dark_oak_wall_hanging_sign", mce::Color(0.278f, 0.231f, 0.180f, 1.00f)},
        {"dark_oak_wall_sign", mce::Color(0.271f, 0.180f, 0.086f, 1.00f)},
        {"darkoak_standing_sign", mce::Color(0.271f, 0.180f, 0.086f, 1.00f)},
        {"darkoak_wall_sign", mce::Color(0.271f, 0.180f, 0.086f, 1.00f)},
        {"dark_oak_wood", mce::Color(0.251f, 0.192f, 0.110f, 1.00f)},
        {"dark_prismarine", mce::Color(0.227f, 0.420f, 0.345f, 1.00f)},
        {"dark_prismarine_slab", mce::Color(0.227f, 0.420f, 0.345f, 1.00f)},
        {"dark_prismarine_stairs", mce::Color(0.227f, 0.420f, 0.345f, 1.00f)},
        {"daylight_detector", mce::Color(0.510f, 0.455f, 0.369f, 1.00f)},
        {"dead_brain_coral", mce::Color(0.518f, 0.482f, 0.467f, 1.00f)},
        {"dead_brain_coral_block", mce::Color(0.482f, 0.459f, 0.443f, 1.00f)},
        {"dead_brain_coral_fan", mce::Color(0.522f, 0.490f, 0.475f, 1.00f)},
        {"dead_brain_coral_wall_fan", mce::Color(0.522f, 0.490f, 0.475f, 1.00f)},
        {"dead_bubble_coral", mce::Color(0.525f, 0.494f, 0.478f, 1.00f)},
        {"dead_bubble_coral_block", mce::Color(0.514f, 0.482f, 0.467f, 1.00f)},
        {"dead_bubble_coral_fan", mce::Color(0.569f, 0.545f, 0.525f, 1.00f)},
        {"dead_bubble_coral_wall_fan", mce::Color(0.569f, 0.545f, 0.525f, 1.00f)},
        {"dead_bush", mce::Color(0.421f, 0.309f, 0.159f, 1.00f)},
        {"dead_fire_coral", mce::Color(0.541f, 0.510f, 0.494f, 1.00f)},
        {"dead_fire_coral_block", mce::Color(0.522f, 0.490f, 0.475f, 1.00f)},
        {"dead_fire_coral_fan", mce::Color(0.482f, 0.459f, 0.447f, 1.00f)},
        {"dead_fire_coral_wall_fan", mce::Color(0.482f, 0.459f, 0.447f, 1.00f)},
        {"dead_horn_coral", mce::Color(0.557f, 0.529f, 0.506f, 1.00f)},
        {"dead_horn_coral_block", mce::Color(0.522f, 0.490f, 0.475f, 1.00f)},
        {"dead_horn_coral_fan", mce::Color(0.533f, 0.506f, 0.486f, 1.00f)},
        {"dead_horn_coral_wall_fan", mce::Color(0.533f, 0.506f, 0.486f, 1.00f)},
        {"dead_tube_coral", mce::Color(0.463f, 0.435f, 0.424f, 1.00f)},
        {"dead_tube_coral_block", mce::Color(0.510f, 0.482f, 0.467f, 1.00f)},
        {"dead_tube_coral_fan", mce::Color(0.506f, 0.482f, 0.467f, 1.00f)},
        {"dead_tube_coral_wall_fan", mce::Color(0.506f, 0.482f, 0.467f, 1.00f)},
        {"deadbush", mce::Color(0.421f, 0.309f, 0.159f, 1.00f)},
        {"decorated_pot", mce::Color(0.596f, 0.369f, 0.263f, 1.00f)},
        {"deepslate", mce::Color(0.345f, 0.345f, 0.353f, 1.00f)},
        {"deepslate_brick_slab", mce::Color(0.306f, 0.306f, 0.310f, 1.00f)},
        {"deepslate_brick_stairs", mce::Color(0.306f, 0.306f, 0.310f, 1.00f)},
        {"deepslate_brick_wall", mce::Color(0.306f, 0.306f, 0.310f, 1.00f)},
        {"deepslate_bricks", mce::Color(0.306f, 0.306f, 0.310f, 1.00f)},
        {"deepslate_coal_ore", mce::Color(0.290f, 0.290f, 0.294f, 1.00f)},
        {"deepslate_copper_ore", mce::Color(0.373f, 0.361f, 0.341f, 1.00f)},
        {"deepslate_diamond_ore", mce::Color(0.325f, 0.447f, 0.447f, 1.00f)},
        {"deepslate_emerald_ore", mce::Color(0.314f, 0.420f, 0.349f, 1.00f)},
        {"deepslate_gold_ore", mce::Color(0.459f, 0.404f, 0.298f, 1.00f)},
        {"deepslate_iron_ore", mce::Color(0.420f, 0.392f, 0.369f, 1.00f)},
        {"deepslate_lapis_ore", mce::Color(0.298f, 0.349f, 0.455f, 1.00f)},
        {"deepslate_redstone_ore", mce::Color(0.424f, 0.282f, 0.290f, 1.00f)},
        {"deepslate_tile_slab", mce::Color(0.224f, 0.224f, 0.224f, 1.00f)},
        {"deepslate_tile_stairs", mce::Color(0.224f, 0.224f, 0.224f, 1.00f)},
        {"deepslate_tile_wall", mce::Color(0.224f, 0.224f, 0.224f, 1.00f)},
        {"deepslate_tiles", mce::Color(0.224f, 0.224f, 0.224f, 1.00f)},
        {"detector_rail", mce::Color(0.447f, 0.384f, 0.341f, 1.00f)},
        {"diamond_block", mce::Color(0.384f, 0.945f, 0.906f, 1.00f)},
        {"diamond_ore", mce::Color(0.459f, 0.561f, 0.557f, 1.00f)},
        {"diorite", mce::Color(0.745f, 0.741f, 0.745f, 1.00f)},
        {"diorite_slab", mce::Color(0.745f, 0.741f, 0.745f, 1.00f)},
        {"diorite_stairs", mce::Color(0.745f, 0.741f, 0.745f, 1.00f)},
        {"diorite_wall", mce::Color(0.745f, 0.741f, 0.745f, 1.00f)},
        {"dirt", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"dirt_path", mce::Color(0.576f, 0.471f, 0.255f, 1.00f)},
        {"dispenser", mce::Color(0.380f, 0.380f, 0.380f, 1.00f)},
        {"dragon_egg", mce::Color(0.047f, 0.031f, 0.063f, 1.00f)},
        {"dragon_head", mce::Color(0.147f, 0.145f, 0.147f, 1.00f)},
        {"dragon_wall_head", mce::Color(0.147f, 0.145f, 0.147f, 1.00f)},
        {"dried_ghast", mce::Color(0.471f, 0.459f, 0.459f, 1.00f)},
        {"dried_kelp_block", mce::Color(0.196f, 0.224f, 0.149f, 1.00f)},
        {"dripstone_block", mce::Color(0.533f, 0.427f, 0.365f, 1.00f)},
        {"dropper", mce::Color(0.380f, 0.376f, 0.376f, 1.00f)},
        {"emerald_block", mce::Color(0.125f, 0.812f, 0.329f, 1.00f)},
        {"emerald_ore", mce::Color(0.420f, 0.529f, 0.451f, 1.00f)},
        {"enchanting_table", mce::Color(0.502f, 0.294f, 0.333f, 1.00f)},
        {"end_brick_stairs", mce::Color(0.840f, 0.850f, 0.600f, 1.00f)},
        {"end_bricks", mce::Color(0.840f, 0.850f, 0.600f, 1.00f)},
        {"end_gateway", mce::Color(0.059f, 0.043f, 0.098f, 1.00f)},
        {"end_portal", mce::Color(0.059f, 0.043f, 0.098f, 1.00f)},
        {"end_portal_frame", mce::Color(0.357f, 0.475f, 0.380f, 1.00f)},
        {"end_rod", mce::Color(0.820f, 0.780f, 0.737f, 1.00f)},
        {"end_stone", mce::Color(0.835f, 0.851f, 0.604f, 1.00f)},
        {"end_stone_brick_slab", mce::Color(0.840f, 0.850f, 0.600f, 1.00f)},
        {"end_stone_brick_stairs", mce::Color(0.840f, 0.850f, 0.600f, 1.00f)},
        {"end_stone_brick_wall", mce::Color(0.840f, 0.850f, 0.600f, 1.00f)},
        {"end_stone_bricks", mce::Color(0.840f, 0.850f, 0.600f, 1.00f)},
        {"ender_chest", mce::Color(0.059f, 0.043f, 0.098f, 1.00f)},
        {"exposed_chiseled_copper", mce::Color(0.651f, 0.486f, 0.420f, 1.00f)},
        {"exposed_copper", mce::Color(0.671f, 0.510f, 0.431f, 1.00f)},
        {"exposed_copper_bars", mce::Color(0.553f, 0.431f, 0.361f, 1.00f)},
        {"exposed_copper_bulb", mce::Color(0.573f, 0.439f, 0.376f, 1.00f)},
        {"exposed_copper_chain", mce::Color(0.498f, 0.400f, 0.325f, 1.00f)},
        {"exposed_copper_chest", mce::Color(0.671f, 0.510f, 0.431f, 1.00f)},
        {"exposed_copper_door", mce::Color(0.655f, 0.486f, 0.424f, 1.00f)},
        {"exposed_copper_golem_statue", mce::Color(0.671f, 0.510f, 0.431f, 1.00f)},
        {"exposed_copper_grate", mce::Color(0.694f, 0.518f, 0.443f, 1.00f)},
        {"exposed_copper_lantern", mce::Color(0.608f, 0.525f, 0.424f, 1.00f)},
        {"exposed_copper_trapdoor", mce::Color(0.675f, 0.506f, 0.435f, 1.00f)},
        {"exposed_cut_copper", mce::Color(0.663f, 0.506f, 0.431f, 1.00f)},
        {"exposed_cut_copper_slab", mce::Color(0.663f, 0.506f, 0.431f, 1.00f)},
        {"exposed_cut_copper_stairs", mce::Color(0.663f, 0.506f, 0.431f, 1.00f)},
        {"exposed_lightning_rod", mce::Color(0.651f, 0.494f, 0.427f, 1.00f)},
        {"farmland", mce::Color(0.561f, 0.404f, 0.278f, 1.00f)},
        {"fence", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"fence_gate", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"fire", mce::Color(0.835f, 0.557f, 0.196f, 1.00f)},
        {"fire_coral", mce::Color(0.667f, 0.149f, 0.184f, 1.00f)},
        {"fire_coral_block", mce::Color(0.655f, 0.141f, 0.184f, 1.00f)},
        {"fire_coral_fan", mce::Color(0.616f, 0.133f, 0.176f, 1.00f)},
        {"fire_coral_wall_fan", mce::Color(0.616f, 0.133f, 0.176f, 1.00f)},
        {"firefly_bush", mce::Color(0.580f, 0.500f, 0.220f, 1.00f)},
        {"fletching_table", mce::Color(0.827f, 0.757f, 0.561f, 1.00f)},
        {"flower_pot", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"flowering_azalea", mce::Color(0.447f, 0.478f, 0.263f, 1.00f)},
        {"flowering_azalea_leaves", mce::Color(0.353f, 0.431f, 0.196f, 1.00f)},
        {"frogspawn", mce::Color(0.424f, 0.365f, 0.329f, 1.00f)},
        {"frosted_ice", mce::Color(0.545f, 0.706f, 0.988f, 1.00f)},
        {"furnace", mce::Color(0.435f, 0.431f, 0.431f, 1.00f)},
        {"gilded_blackstone", mce::Color(0.224f, 0.176f, 0.153f, 1.00f)},
        {"glass", mce::Color(0.784f, 0.898f, 0.902f, 1.00f)},
        {"glass_pane", mce::Color(0.616f, 0.792f, 0.831f, 1.00f)},
        {"glow_item_frame", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"glow_lichen", mce::Color(0.443f, 0.510f, 0.475f, 1.00f)},
        {"glowstone", mce::Color(0.667f, 0.510f, 0.322f, 1.00f)},
        {"gold_block", mce::Color(0.984f, 0.831f, 0.220f, 1.00f)},
        {"gold_ore", mce::Color(0.573f, 0.522f, 0.404f, 1.00f)},
        {"golden_dandelion", mce::Color(0.655f, 0.416f, 0.169f, 1.00f)},
        {"golden_rail", mce::Color(0.498f, 0.404f, 0.271f, 1.00f)},
        {"granite", mce::Color(0.576f, 0.400f, 0.333f, 1.00f)},
        {"granite_slab", mce::Color(0.576f, 0.400f, 0.333f, 1.00f)},
        {"granite_stairs", mce::Color(0.576f, 0.400f, 0.333f, 1.00f)},
        {"granite_wall", mce::Color(0.576f, 0.400f, 0.333f, 1.00f)},
        {"gravel", mce::Color(0.510f, 0.490f, 0.486f, 1.00f)},
        {"gray_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"gray_bed", mce::Color(0.369f, 0.431f, 0.443f, 1.00f)},
        {"gray_candle", mce::Color(0.322f, 0.380f, 0.384f, 1.00f)},
        {"gray_candle_cake", mce::Color(0.322f, 0.380f, 0.384f, 1.00f)},
        {"gray_carpet", mce::Color(0.247f, 0.267f, 0.278f, 1.00f)},
        {"gray_concrete", mce::Color(0.212f, 0.224f, 0.239f, 1.00f)},
        {"gray_concrete_powder", mce::Color(0.298f, 0.314f, 0.325f, 1.00f)},
        {"gray_glazed_terracotta", mce::Color(0.318f, 0.341f, 0.357f, 1.00f)},
        {"gray_shulker_box", mce::Color(0.220f, 0.231f, 0.247f, 1.00f)},
        {"gray_stained_glass", mce::Color(0.298f, 0.298f, 0.298f, 1.00f)},
        {"gray_stained_glass_pane", mce::Color(0.282f, 0.282f, 0.282f, 1.00f)},
        {"gray_terracotta", mce::Color(0.224f, 0.165f, 0.137f, 1.00f)},
        {"gray_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"gray_wool", mce::Color(0.247f, 0.267f, 0.278f, 1.00f)},
        {"green_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"green_bed", mce::Color(0.357f, 0.482f, 0.082f, 1.00f)},
        {"green_candle", mce::Color(0.290f, 0.384f, 0.075f, 1.00f)},
        {"green_candle_cake", mce::Color(0.290f, 0.384f, 0.075f, 1.00f)},
        {"green_carpet", mce::Color(0.333f, 0.431f, 0.106f, 1.00f)},
        {"green_concrete", mce::Color(0.286f, 0.357f, 0.141f, 1.00f)},
        {"green_concrete_powder", mce::Color(0.376f, 0.463f, 0.173f, 1.00f)},
        {"green_glazed_terracotta", mce::Color(0.369f, 0.486f, 0.137f, 1.00f)},
        {"green_shulker_box", mce::Color(0.310f, 0.400f, 0.122f, 1.00f)},
        {"green_stained_glass", mce::Color(0.400f, 0.498f, 0.200f, 1.00f)},
        {"green_stained_glass_pane", mce::Color(0.376f, 0.475f, 0.184f, 1.00f)},
        {"green_terracotta", mce::Color(0.298f, 0.325f, 0.165f, 1.00f)},
        {"green_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"green_wool", mce::Color(0.333f, 0.431f, 0.106f, 1.00f)},
        {"grindstone", mce::Color(0.557f, 0.557f, 0.557f, 1.00f)},
        {"hanging_roots", mce::Color(0.608f, 0.435f, 0.337f, 1.00f)},
        {"hardened_clay", mce::Color(0.596f, 0.369f, 0.263f, 1.00f)},
        {"hay_block", mce::Color(0.671f, 0.561f, 0.043f, 1.00f)},
        {"heavy_core", mce::Color(0.341f, 0.357f, 0.392f, 1.00f)},
        {"heavy_weighted_pressure_plate", mce::Color(0.867f, 0.863f, 0.863f, 1.00f)},
        {"honey_block", mce::Color(0.984f, 0.722f, 0.204f, 0.75f)},
        {"honeycomb_block", mce::Color(0.902f, 0.612f, 0.165f, 1.00f)},
        {"hopper", mce::Color(0.294f, 0.290f, 0.294f, 1.00f)},
        {"horn_coral", mce::Color(0.816f, 0.729f, 0.243f, 1.00f)},
        {"horn_coral_block", mce::Color(0.843f, 0.780f, 0.255f, 1.00f)},
        {"horn_coral_fan", mce::Color(0.820f, 0.733f, 0.243f, 1.00f)},
        {"horn_coral_wall_fan", mce::Color(0.820f, 0.733f, 0.243f, 1.00f)},
        {"ice", mce::Color(0.565f, 0.718f, 0.992f, 1.00f)},
        {"infested_chiseled_stone_bricks", mce::Color(0.463f, 0.459f, 0.463f, 1.00f)},
        {"infested_cobblestone", mce::Color(0.498f, 0.498f, 0.498f, 1.00f)},
        {"infested_cracked_stone_bricks", mce::Color(0.486f, 0.482f, 0.486f, 1.00f)},
        {"infested_deepslate", mce::Color(0.345f, 0.345f, 0.353f, 1.00f)},
        {"infested_mossy_stone_bricks", mce::Color(0.471f, 0.498f, 0.420f, 1.00f)},
        {"infested_stone", mce::Color(0.486f, 0.486f, 0.486f, 1.00f)},
        {"infested_stone_bricks", mce::Color(0.502f, 0.498f, 0.502f, 1.00f)},
        {"iron_bars", mce::Color(0.576f, 0.584f, 0.573f, 1.00f)},
        {"iron_block", mce::Color(0.867f, 0.863f, 0.863f, 1.00f)},
        {"iron_chain", mce::Color(0.204f, 0.227f, 0.294f, 1.00f)},
        {"iron_door", mce::Color(0.769f, 0.765f, 0.765f, 1.00f)},
        {"iron_ore", mce::Color(0.525f, 0.498f, 0.471f, 1.00f)},
        {"iron_trapdoor", mce::Color(0.796f, 0.792f, 0.792f, 1.00f)},
        {"item_frame", mce::Color(0.804f, 0.737f, 0.506f, 1.00f)},
        {"jack_o_lantern", mce::Color(0.773f, 0.463f, 0.098f, 1.00f)},
        {"jigsaw", mce::Color(0.255f, 0.220f, 0.260f, 1.00f)},
        {"jukebox", mce::Color(0.369f, 0.255f, 0.184f, 1.00f)},
        {"jungle_button", mce::Color(0.694f, 0.506f, 0.365f, 1.00f)},
        {"jungle_door", mce::Color(0.667f, 0.490f, 0.349f, 1.00f)},
        {"jungle_fence", mce::Color(0.694f, 0.506f, 0.365f, 1.00f)},
        {"jungle_fence_gate", mce::Color(0.694f, 0.506f, 0.365f, 1.00f)},
        {"jungle_hanging_sign", mce::Color(0.580f, 0.471f, 0.325f, 1.00f)},
        {"jungle_log", mce::Color(0.580f, 0.424f, 0.275f, 1.00f)},
        {"jungle_planks", mce::Color(0.694f, 0.506f, 0.365f, 1.00f)},
        {"jungle_pressure_plate", mce::Color(0.694f, 0.506f, 0.365f, 1.00f)},
        {"jungle_sapling", mce::Color(0.184f, 0.306f, 0.063f, 1.00f)},
        {"jungle_shelf", mce::Color(0.596f, 0.459f, 0.278f, 1.00f)},
        {"jungle_sign", mce::Color(0.620f, 0.451f, 0.310f, 1.00f)},
        {"jungle_slab", mce::Color(0.694f, 0.506f, 0.365f, 1.00f)},
        {"jungle_stairs", mce::Color(0.694f, 0.506f, 0.365f, 1.00f)},
        {"jungle_trapdoor", mce::Color(0.580f, 0.420f, 0.290f, 1.00f)},
        {"jungle_wall_hanging_sign", mce::Color(0.580f, 0.471f, 0.325f, 1.00f)},
        {"jungle_wall_sign", mce::Color(0.620f, 0.451f, 0.310f, 1.00f)},
        {"jungle_wood", mce::Color(0.349f, 0.275f, 0.102f, 1.00f)},
        {"kelp", mce::Color(0.345f, 0.557f, 0.180f, 1.00f)},
        {"kelp_plant", mce::Color(0.337f, 0.518f, 0.169f, 1.00f)},
        {"ladder", mce::Color(0.455f, 0.349f, 0.196f, 1.00f)},
        {"lantern", mce::Color(0.396f, 0.369f, 0.353f, 1.00f)},
        {"lapis_block", mce::Color(0.125f, 0.278f, 0.565f, 1.00f)},
        {"lapis_ore", mce::Color(0.400f, 0.447f, 0.553f, 1.00f)},
        {"large_amethyst_bud", mce::Color(0.694f, 0.557f, 0.820f, 1.00f)},
        {"lava_cauldron", mce::Color(0.847f, 0.408f, 0.102f, 1.00f)},
        {"leaf_litter", mce::Color(0.500f, 0.350f, 0.240f, 1.00f)},
        {"lectern", mce::Color(0.678f, 0.541f, 0.329f, 1.00f)},
        {"lever", mce::Color(0.353f, 0.298f, 0.224f, 1.00f)},
        {"light_blue_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"light_blue_bed", mce::Color(0.161f, 0.616f, 0.835f, 1.00f)},
        {"light_blue_candle", mce::Color(0.129f, 0.553f, 0.796f, 1.00f)},
        {"light_blue_candle_cake", mce::Color(0.129f, 0.553f, 0.796f, 1.00f)},
        {"light_blue_carpet", mce::Color(0.227f, 0.690f, 0.851f, 1.00f)},
        {"light_blue_concrete", mce::Color(0.137f, 0.537f, 0.776f, 1.00f)},
        {"light_blue_concrete_powder", mce::Color(0.290f, 0.710f, 0.835f, 1.00f)},
        {"light_blue_glazed_terracotta", mce::Color(0.365f, 0.620f, 0.788f, 1.00f)},
        {"light_blue_shulker_box", mce::Color(0.196f, 0.651f, 0.835f, 1.00f)},
        {"light_blue_stained_glass", mce::Color(0.400f, 0.600f, 0.847f, 1.00f)},
        {"light_blue_stained_glass_pane", mce::Color(0.376f, 0.569f, 0.804f, 1.00f)},
        {"light_blue_terracotta", mce::Color(0.443f, 0.424f, 0.537f, 1.00f)},
        {"light_blue_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"light_blue_wool", mce::Color(0.227f, 0.690f, 0.851f, 1.00f)},
        {"light_gray_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"light_gray_bed", mce::Color(0.561f, 0.569f, 0.533f, 1.00f)},
        {"light_gray_candle", mce::Color(0.475f, 0.486f, 0.447f, 1.00f)},
        {"light_gray_candle_cake", mce::Color(0.475f, 0.486f, 0.447f, 1.00f)},
        {"light_gray_carpet", mce::Color(0.557f, 0.557f, 0.529f, 1.00f)},
        {"light_gray_concrete", mce::Color(0.490f, 0.490f, 0.451f, 1.00f)},
        {"light_gray_concrete_powder", mce::Color(0.604f, 0.604f, 0.576f, 1.00f)},
        {"light_gray_glazed_terracotta", mce::Color(0.549f, 0.639f, 0.647f, 1.00f)},
        {"light_gray_shulker_box", mce::Color(0.494f, 0.494f, 0.459f, 1.00f)},
        {"light_gray_stained_glass", mce::Color(0.600f, 0.600f, 0.600f, 1.00f)},
        {"light_gray_stained_glass_pane", mce::Color(0.569f, 0.569f, 0.569f, 1.00f)},
        {"light_gray_terracotta", mce::Color(0.529f, 0.416f, 0.380f, 1.00f)},
        {"light_gray_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"light_gray_wool", mce::Color(0.557f, 0.557f, 0.529f, 1.00f)},
        {"light_weighted_pressure_plate", mce::Color(0.984f, 0.831f, 0.220f, 1.00f)},
        {"lightning_rod", mce::Color(0.800f, 0.455f, 0.345f, 1.00f)},
        {"lilac", mce::Color(0.745f, 0.455f, 0.753f, 1.00f)},
        {"lily_of_the_valley", mce::Color(0.929f, 0.929f, 0.929f, 1.00f)},
        {"lime_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"lime_bed", mce::Color(0.471f, 0.761f, 0.102f, 1.00f)},
        {"lime_candle", mce::Color(0.388f, 0.698f, 0.078f, 1.00f)},
        {"lime_candle_cake", mce::Color(0.388f, 0.698f, 0.078f, 1.00f)},
        {"lime_carpet", mce::Color(0.439f, 0.725f, 0.098f, 1.00f)},
        {"lime_concrete", mce::Color(0.369f, 0.659f, 0.094f, 1.00f)},
        {"lime_concrete_powder", mce::Color(0.486f, 0.737f, 0.161f, 1.00f)},
        {"lime_glazed_terracotta", mce::Color(0.549f, 0.733f, 0.173f, 1.00f)},
        {"lime_shulker_box", mce::Color(0.396f, 0.686f, 0.090f, 1.00f)},
        {"lime_stained_glass", mce::Color(0.498f, 0.800f, 0.098f, 1.00f)},
        {"lime_stained_glass_pane", mce::Color(0.475f, 0.757f, 0.090f, 1.00f)},
        {"lime_terracotta", mce::Color(0.404f, 0.459f, 0.204f, 1.00f)},
        {"lime_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"lime_wool", mce::Color(0.439f, 0.725f, 0.098f, 1.00f)},
        {"lodestone", mce::Color(0.573f, 0.580f, 0.592f, 1.00f)},
        {"loom", mce::Color(0.682f, 0.592f, 0.506f, 1.00f)},
        {"magenta_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"magenta_bed", mce::Color(0.722f, 0.227f, 0.682f, 1.00f)},
        {"magenta_candle", mce::Color(0.647f, 0.176f, 0.616f, 1.00f)},
        {"magenta_candle_cake", mce::Color(0.647f, 0.176f, 0.616f, 1.00f)},
        {"magenta_carpet", mce::Color(0.745f, 0.271f, 0.706f, 1.00f)},
        {"magenta_concrete", mce::Color(0.663f, 0.188f, 0.624f, 1.00f)},
        {"magenta_concrete_powder", mce::Color(0.749f, 0.325f, 0.718f, 1.00f)},
        {"magenta_glazed_terracotta", mce::Color(0.792f, 0.376f, 0.733f, 1.00f)},
        {"magenta_shulker_box", mce::Color(0.686f, 0.216f, 0.647f, 1.00f)},
        {"magenta_stained_glass", mce::Color(0.698f, 0.298f, 0.847f, 1.00f)},
        {"magenta_stained_glass_pane", mce::Color(0.663f, 0.282f, 0.804f, 1.00f)},
        {"magenta_terracotta", mce::Color(0.584f, 0.345f, 0.424f, 1.00f)},
        {"magenta_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"magenta_wool", mce::Color(0.745f, 0.271f, 0.706f, 1.00f)},
        {"magma", mce::Color(0.565f, 0.251f, 0.125f, 1.00f)},
        {"magma_block", mce::Color(0.565f, 0.251f, 0.125f, 1.00f)},
        {"mangrove_button", mce::Color(0.494f, 0.251f, 0.212f, 1.00f)},
        {"mangrove_door", mce::Color(0.447f, 0.196f, 0.184f, 1.00f)},
        {"mangrove_fence", mce::Color(0.494f, 0.251f, 0.212f, 1.00f)},
        {"mangrove_fence_gate", mce::Color(0.494f, 0.251f, 0.212f, 1.00f)},
        {"mangrove_hanging_sign", mce::Color(0.435f, 0.231f, 0.224f, 1.00f)},
        {"mangrove_log", mce::Color(0.404f, 0.192f, 0.165f, 1.00f)},
        {"mangrove_planks", mce::Color(0.494f, 0.251f, 0.212f, 1.00f)},
        {"mangrove_pressure_plate", mce::Color(0.494f, 0.251f, 0.212f, 1.00f)},
        {"mangrove_propagule", mce::Color(0.373f, 0.686f, 0.345f, 1.00f)},
        {"mangrove_roots", mce::Color(0.430f, 0.270f, 0.160f, 1.00f)},
        {"mangrove_shelf", mce::Color(0.416f, 0.165f, 0.157f, 1.00f)},
        {"mangrove_sign", mce::Color(0.463f, 0.239f, 0.196f, 1.00f)},
        {"mangrove_slab", mce::Color(0.494f, 0.251f, 0.212f, 1.00f)},
        {"mangrove_stairs", mce::Color(0.494f, 0.251f, 0.212f, 1.00f)},
        {"mangrove_trapdoor", mce::Color(0.431f, 0.180f, 0.165f, 1.00f)},
        {"mangrove_wall_hanging_sign", mce::Color(0.435f, 0.231f, 0.224f, 1.00f)},
        {"mangrove_wall_sign", mce::Color(0.463f, 0.239f, 0.196f, 1.00f)},
        {"mangrove_wood", mce::Color(0.333f, 0.267f, 0.161f, 1.00f)},
        {"medium_amethyst_bud", mce::Color(0.659f, 0.506f, 0.808f, 1.00f)},
        {"melon", mce::Color(0.431f, 0.565f, 0.118f, 1.00f)},
        {"melon_block", mce::Color(0.431f, 0.565f, 0.118f, 1.00f)},
        {"monster_egg", mce::Color(0.475f, 0.471f, 0.475f, 1.00f)},
        {"moss_block", mce::Color(0.341f, 0.424f, 0.173f, 1.00f)},
        {"moss_carpet", mce::Color(0.341f, 0.424f, 0.173f, 1.00f)},
        {"mossy_cobblestone", mce::Color(0.427f, 0.459f, 0.369f, 1.00f)},
        {"mossy_cobblestone_slab", mce::Color(0.427f, 0.459f, 0.369f, 1.00f)},
        {"mossy_cobblestone_stairs", mce::Color(0.427f, 0.459f, 0.369f, 1.00f)},
        {"mossy_cobblestone_wall", mce::Color(0.427f, 0.459f, 0.369f, 1.00f)},
        {"mossy_stone_brick_slab", mce::Color(0.471f, 0.498f, 0.420f, 1.00f)},
        {"mossy_stone_brick_stairs", mce::Color(0.471f, 0.498f, 0.420f, 1.00f)},
        {"mossy_stone_brick_wall", mce::Color(0.471f, 0.498f, 0.420f, 1.00f)},
        {"mossy_stone_bricks", mce::Color(0.471f, 0.498f, 0.420f, 1.00f)},
        {"moving_piston", mce::Color(0.451f, 0.431f, 0.396f, 1.00f)},
        {"mud", mce::Color(0.231f, 0.224f, 0.235f, 1.00f)},
        {"mud_brick_slab", mce::Color(0.541f, 0.408f, 0.310f, 1.00f)},
        {"mud_brick_stairs", mce::Color(0.541f, 0.408f, 0.310f, 1.00f)},
        {"mud_brick_wall", mce::Color(0.541f, 0.408f, 0.310f, 1.00f)},
        {"mud_bricks", mce::Color(0.541f, 0.408f, 0.310f, 1.00f)},
        {"muddy_mangrove_roots", mce::Color(0.350f, 0.250f, 0.150f, 1.00f)},
        {"mushroom_stem", mce::Color(0.796f, 0.769f, 0.725f, 1.00f)},
        {"mycelium", mce::Color(0.431f, 0.384f, 0.392f, 1.00f)},
        {"nether_brick_fence", mce::Color(0.216f, 0.102f, 0.122f, 1.00f)},
        {"nether_brick_slab", mce::Color(0.216f, 0.102f, 0.122f, 1.00f)},
        {"nether_brick_stairs", mce::Color(0.216f, 0.102f, 0.122f, 1.00f)},
        {"nether_brick_wall", mce::Color(0.216f, 0.102f, 0.122f, 1.00f)},
        {"nether_bricks", mce::Color(0.216f, 0.102f, 0.122f, 1.00f)},
        {"nether_gold_ore", mce::Color(0.435f, 0.204f, 0.157f, 1.00f)},
        {"nether_portal", mce::Color(0.353f, 0.051f, 0.757f, 0.76f)},
        {"nether_quartz_ore", mce::Color(0.459f, 0.251f, 0.239f, 1.00f)},
        {"nether_sprouts", mce::Color(0.071f, 0.647f, 0.522f, 1.00f)},
        {"nether_wart", mce::Color(0.467f, 0.078f, 0.090f, 1.00f)},
        {"nether_wart_block", mce::Color(0.447f, 0.008f, 0.004f, 1.00f)},
        {"netherite_block", mce::Color(0.263f, 0.243f, 0.251f, 1.00f)},
        {"netherrack", mce::Color(0.380f, 0.149f, 0.149f, 1.00f)},
        {"note_block", mce::Color(0.420f, 0.282f, 0.200f, 1.00f)},
        {"noteblock", mce::Color(0.420f, 0.282f, 0.200f, 1.00f)},
        {"oak_button", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"oak_door", mce::Color(0.565f, 0.447f, 0.275f, 1.00f)},
        {"oak_fence", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"oak_fence_gate", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"oak_hanging_sign", mce::Color(0.620f, 0.518f, 0.345f, 1.00f)},
        {"oak_log", mce::Color(0.592f, 0.475f, 0.286f, 1.00f)},
        {"oak_planks", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"oak_pressure_plate", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"oak_sapling", mce::Color(0.306f, 0.439f, 0.169f, 1.00f)},
        {"oak_shelf", mce::Color(0.545f, 0.431f, 0.251f, 1.00f)},
        {"oak_sign", mce::Color(0.655f, 0.529f, 0.318f, 1.00f)},
        {"oak_slab", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"oak_stairs", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"oak_trapdoor", mce::Color(0.486f, 0.384f, 0.220f, 1.00f)},
        {"oak_wall_hanging_sign", mce::Color(0.620f, 0.518f, 0.345f, 1.00f)},
        {"oak_wall_sign", mce::Color(0.655f, 0.529f, 0.318f, 1.00f)},
        {"oak_wood", mce::Color(0.463f, 0.361f, 0.216f, 1.00f)},
        {"observer", mce::Color(0.384f, 0.384f, 0.384f, 1.00f)},
        {"obsidian", mce::Color(0.059f, 0.043f, 0.098f, 1.00f)},
        {"ochre_froglight", mce::Color(0.980f, 0.961f, 0.808f, 1.00f)},
        {"orange_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"orange_bed", mce::Color(0.976f, 0.490f, 0.082f, 1.00f)},
        {"orange_candle", mce::Color(0.894f, 0.396f, 0.016f, 1.00f)},
        {"orange_candle_cake", mce::Color(0.894f, 0.396f, 0.016f, 1.00f)},
        {"orange_carpet", mce::Color(0.945f, 0.463f, 0.078f, 1.00f)},
        {"orange_concrete", mce::Color(0.878f, 0.380f, 0.000f, 1.00f)},
        {"orange_concrete_powder", mce::Color(0.886f, 0.514f, 0.125f, 1.00f)},
        {"orange_glazed_terracotta", mce::Color(0.880f, 0.460f, 0.110f, 1.00f)},
        {"orange_shulker_box", mce::Color(0.925f, 0.424f, 0.035f, 1.00f)},
        {"orange_stained_glass", mce::Color(0.847f, 0.498f, 0.200f, 1.00f)},
        {"orange_stained_glass_pane", mce::Color(0.804f, 0.475f, 0.184f, 1.00f)},
        {"orange_terracotta", mce::Color(0.631f, 0.325f, 0.145f, 1.00f)},
        {"orange_tulip", mce::Color(0.851f, 0.518f, 0.149f, 1.00f)},
        {"orange_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"orange_wool", mce::Color(0.945f, 0.463f, 0.078f, 1.00f)},
        {"open_eyeblossom", mce::Color(0.710f, 0.350f, 0.120f, 1.00f)},
        {"oxeye_daisy", mce::Color(0.910f, 0.940f, 0.940f, 1.00f)},
        {"oxidized_chiseled_copper", mce::Color(0.353f, 0.675f, 0.549f, 1.00f)},
        {"oxidized_copper", mce::Color(0.345f, 0.682f, 0.557f, 1.00f)},
        {"oxidized_copper_bars", mce::Color(0.259f, 0.502f, 0.412f, 1.00f)},
        {"oxidized_copper_bulb", mce::Color(0.302f, 0.569f, 0.467f, 1.00f)},
        {"oxidized_copper_chain", mce::Color(0.243f, 0.467f, 0.392f, 1.00f)},
        {"oxidized_copper_chest", mce::Color(0.345f, 0.682f, 0.557f, 1.00f)},
        {"oxidized_copper_door", mce::Color(0.329f, 0.635f, 0.522f, 1.00f)},
        {"oxidized_copper_golem_statue", mce::Color(0.345f, 0.682f, 0.557f, 1.00f)},
        {"oxidized_copper_grate", mce::Color(0.353f, 0.686f, 0.561f, 1.00f)},
        {"oxidized_copper_lantern", mce::Color(0.310f, 0.561f, 0.447f, 1.00f)},
        {"oxidized_copper_trapdoor", mce::Color(0.357f, 0.678f, 0.557f, 1.00f)},
        {"oxidized_cut_copper", mce::Color(0.345f, 0.667f, 0.545f, 1.00f)},
        {"oxidized_cut_copper_slab", mce::Color(0.345f, 0.667f, 0.545f, 1.00f)},
        {"oxidized_cut_copper_stairs", mce::Color(0.345f, 0.667f, 0.545f, 1.00f)},
        {"oxidized_lightning_rod", mce::Color(0.333f, 0.647f, 0.549f, 1.00f)},
        {"packed_ice", mce::Color(0.553f, 0.702f, 0.980f, 1.00f)},
        {"packed_mud", mce::Color(0.557f, 0.420f, 0.310f, 1.00f)},
        {"pale_hanging_moss", mce::Color(0.376f, 0.392f, 0.369f, 1.00f)},
        {"pale_leaves", mce::Color(0.431f, 0.451f, 0.420f, 1.00f)},
        {"pale_moss_block", mce::Color(0.408f, 0.431f, 0.404f, 1.00f)},
        {"pale_moss_carpet", mce::Color(0.408f, 0.431f, 0.404f, 1.00f)},
        {"pale_oak_button", mce::Color(0.945f, 0.914f, 0.906f, 1.00f)},
        {"pale_oak_door", mce::Color(0.848f, 0.816f, 0.811f, 1.00f)},
        {"pale_oak_leaves", mce::Color(0.431f, 0.451f, 0.420f, 1.00f)},
        {"pale_oak_fence", mce::Color(0.859f, 0.820f, 0.816f, 1.00f)},
        {"pale_oak_fence_gate", mce::Color(0.859f, 0.820f, 0.816f, 1.00f)},
        {"pale_oak_hanging_sign", mce::Color(0.859f, 0.820f, 0.816f, 1.00f)},
        {"pale_oak_log", mce::Color(0.765f, 0.729f, 0.725f, 1.00f)},
        {"pale_oak_planks", mce::Color(0.859f, 0.820f, 0.816f, 1.00f)},
        {"pale_oak_pressure_plate", mce::Color(0.765f, 0.753f, 0.710f, 1.00f)},
        {"pale_oak_sapling", mce::Color(0.318f, 0.400f, 0.212f, 1.00f)},
        {"pale_oak_shelf", mce::Color(0.961f, 0.929f, 0.925f, 1.00f)},
        {"pale_oak_sign", mce::Color(0.945f, 0.914f, 0.906f, 1.00f)},
        {"pale_oak_standing_sign", mce::Color(0.945f, 0.914f, 0.906f, 1.00f)},
        {"pale_oak_slab", mce::Color(0.898f, 0.859f, 0.855f, 1.00f)},
        {"pale_oak_stairs", mce::Color(0.847f, 0.808f, 0.804f, 1.00f)},
        {"pale_oak_trapdoor", mce::Color(0.863f, 0.820f, 0.816f, 1.00f)},
        {"pale_oak_wall_hanging_sign", mce::Color(0.961f, 0.929f, 0.925f, 1.00f)},
        {"pale_oak_wall_sign", mce::Color(0.945f, 0.914f, 0.906f, 1.00f)},
        {"pale_oak_wood", mce::Color(0.333f, 0.294f, 0.286f, 1.00f)},
        {"pearlescent_froglight", mce::Color(0.961f, 0.941f, 0.937f, 1.00f)},
        {"peony", mce::Color(0.902f, 0.698f, 0.969f, 1.00f)},
        {"petrified_oak_slab", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"piglin_head", mce::Color(0.937f, 0.700f, 0.506f, 1.00f)},
        {"piglin_wall_head", mce::Color(0.937f, 0.700f, 0.506f, 1.00f)},
        {"pink_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"pink_bed", mce::Color(0.945f, 0.510f, 0.651f, 1.00f)},
        {"pink_candle", mce::Color(0.847f, 0.404f, 0.573f, 1.00f)},
        {"pink_candle_cake", mce::Color(0.847f, 0.404f, 0.573f, 1.00f)},
        {"pink_carpet", mce::Color(0.933f, 0.557f, 0.678f, 1.00f)},
        {"pink_concrete", mce::Color(0.835f, 0.392f, 0.557f, 1.00f)},
        {"pink_concrete_powder", mce::Color(0.894f, 0.596f, 0.706f, 1.00f)},
        {"pink_glazed_terracotta", mce::Color(0.882f, 0.588f, 0.686f, 1.00f)},
        {"pink_petals", mce::Color(0.965f, 0.710f, 0.851f, 1.00f)},
        {"pink_shulker_box", mce::Color(0.910f, 0.486f, 0.624f, 1.00f)},
        {"pink_stained_glass", mce::Color(0.949f, 0.498f, 0.647f, 1.00f)},
        {"pink_stained_glass_pane", mce::Color(0.902f, 0.475f, 0.616f, 1.00f)},
        {"pink_terracotta", mce::Color(0.631f, 0.306f, 0.306f, 1.00f)},
        {"pink_tulip", mce::Color(0.922f, 0.769f, 0.980f, 1.00f)},
        {"pink_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"pink_wool", mce::Color(0.933f, 0.557f, 0.678f, 1.00f)},
        {"piston", mce::Color(0.451f, 0.431f, 0.396f, 1.00f)},
        {"piston_head", mce::Color(0.451f, 0.431f, 0.396f, 1.00f)},
        {"pitcher_crop", mce::Color(0.757f, 0.647f, 0.404f, 1.00f)},
        {"pitcher_plant", mce::Color(0.485f, 0.575f, 0.755f, 1.00f)},
        {"player_head", mce::Color(0.450f, 0.320f, 0.220f, 1.00f)},
        {"player_wall_head", mce::Color(0.450f, 0.320f, 0.220f, 1.00f)},
        {"podzol", mce::Color(0.365f, 0.247f, 0.094f, 1.00f)},
        {"pointed_dripstone", mce::Color(0.510f, 0.404f, 0.353f, 1.00f)},
        {"polished_andesite", mce::Color(0.561f, 0.569f, 0.561f, 1.00f)},
        {"polished_andesite_slab", mce::Color(0.561f, 0.569f, 0.561f, 1.00f)},
        {"polished_andesite_stairs", mce::Color(0.561f, 0.569f, 0.561f, 1.00f)},
        {"polished_basalt", mce::Color(0.380f, 0.376f, 0.388f, 1.00f)},
        {"polished_blackstone", mce::Color(0.216f, 0.200f, 0.231f, 1.00f)},
        {"polished_blackstone_brick_slab", mce::Color(0.204f, 0.184f, 0.216f, 1.00f)},
        {"polished_blackstone_brick_stairs", mce::Color(0.204f, 0.184f, 0.216f, 1.00f)},
        {"polished_blackstone_brick_wall", mce::Color(0.204f, 0.184f, 0.216f, 1.00f)},
        {"polished_blackstone_bricks", mce::Color(0.204f, 0.184f, 0.216f, 1.00f)},
        {"polished_blackstone_button", mce::Color(0.216f, 0.200f, 0.231f, 1.00f)},
        {"polished_blackstone_pressure_plate", mce::Color(0.216f, 0.200f, 0.231f, 1.00f)},
        {"polished_blackstone_slab", mce::Color(0.216f, 0.200f, 0.231f, 1.00f)},
        {"polished_blackstone_stairs", mce::Color(0.216f, 0.200f, 0.231f, 1.00f)},
        {"polished_blackstone_wall", mce::Color(0.216f, 0.200f, 0.231f, 1.00f)},
        {"polished_cinnabar", mce::Color(0.631f, 0.247f, 0.235f, 1.00f)},
        {"polished_cinnabar_slab", mce::Color(0.576f, 0.216f, 0.208f, 1.00f)},
        {"polished_cinnabar_stairs", mce::Color(0.565f, 0.212f, 0.200f, 1.00f)},
        {"polished_deepslate", mce::Color(0.318f, 0.318f, 0.322f, 1.00f)},
        {"polished_deepslate_slab", mce::Color(0.318f, 0.318f, 0.322f, 1.00f)},
        {"polished_deepslate_stairs", mce::Color(0.318f, 0.318f, 0.322f, 1.00f)},
        {"polished_deepslate_wall", mce::Color(0.318f, 0.318f, 0.322f, 1.00f)},
        {"polished_diorite", mce::Color(0.812f, 0.808f, 0.816f, 1.00f)},
        {"polished_diorite_slab", mce::Color(0.812f, 0.808f, 0.816f, 1.00f)},
        {"polished_diorite_stairs", mce::Color(0.812f, 0.808f, 0.816f, 1.00f)},
        {"polished_granite", mce::Color(0.643f, 0.455f, 0.384f, 1.00f)},
        {"polished_granite_slab", mce::Color(0.643f, 0.455f, 0.384f, 1.00f)},
        {"polished_granite_stairs", mce::Color(0.643f, 0.455f, 0.384f, 1.00f)},
        {"polished_sulfur", mce::Color(0.761f, 0.702f, 0.369f, 1.00f)},
        {"polished_sulfur_slab", mce::Color(0.694f, 0.620f, 0.337f, 1.00f)},
        {"polished_sulfur_stairs", mce::Color(0.627f, 0.565f, 0.306f, 1.00f)},
        {"polished_tuff", mce::Color(0.392f, 0.412f, 0.392f, 1.00f)},
        {"polished_tuff_slab", mce::Color(0.392f, 0.412f, 0.392f, 1.00f)},
        {"polished_tuff_stairs", mce::Color(0.392f, 0.412f, 0.392f, 1.00f)},
        {"polished_tuff_wall", mce::Color(0.392f, 0.412f, 0.392f, 1.00f)},
        {"poppy", mce::Color(0.788f, 0.161f, 0.145f, 1.00f)},
        {"potatoes", mce::Color(0.310f, 0.616f, 0.165f, 1.00f)},
        {"potent_sulfur", mce::Color(0.804f, 0.796f, 0.373f, 1.00f)},
        {"potted_acacia_sapling", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_allium", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_azalea_bush", mce::Color(0.388f, 0.482f, 0.184f, 1.00f)},
        {"potted_azure_bluet", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_bamboo", mce::Color(0.337f, 0.533f, 0.051f, 1.00f)},
        {"potted_birch_sapling", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_blue_orchid", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_brown_mushroom", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_cactus", mce::Color(0.337f, 0.498f, 0.169f, 1.00f)},
        {"potted_cherry_sapling", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_closed_eyeblossom", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_cornflower", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_crimson_fungus", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_crimson_roots", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_dandelion", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_dark_oak_sapling", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_dead_bush", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_fern", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_flowering_azalea_bush", mce::Color(0.467f, 0.482f, 0.306f, 1.00f)},
        {"potted_golden_dandelion", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_jungle_sapling", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_lily_of_the_valley", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_mangrove_propagule", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_oak_sapling", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_open_eyeblossom", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_orange_tulip", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_oxeye_daisy", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_pale_oak_sapling", mce::Color(0.443f, 0.424f, 0.400f, 1.00f)},
        {"potted_pink_tulip", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_poppy", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_red_mushroom", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_red_tulip", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_spruce_sapling", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_torchflower", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_warped_fungus", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_warped_roots", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_white_tulip", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"potted_wither_rose", mce::Color(0.537f, 0.384f, 0.267f, 1.00f)},
        {"powder_snow", mce::Color(0.973f, 0.992f, 0.992f, 1.00f)},
        {"powder_snow_cauldron", mce::Color(0.973f, 0.992f, 0.992f, 1.00f)},
        {"powered_rail", mce::Color(0.498f, 0.404f, 0.271f, 1.00f)},
        {"prismarine", mce::Color(0.376f, 0.627f, 0.565f, 1.00f)},
        {"prismarine_brick_slab", mce::Color(0.424f, 0.702f, 0.659f, 1.00f)},
        {"prismarine_brick_stairs", mce::Color(0.424f, 0.702f, 0.659f, 1.00f)},
        {"prismarine_bricks", mce::Color(0.424f, 0.702f, 0.659f, 1.00f)},
        {"prismarine_bricks_stairs", mce::Color(0.424f, 0.702f, 0.659f, 1.00f)},
        {"prismarine_slab", mce::Color(0.376f, 0.627f, 0.565f, 1.00f)},
        {"prismarine_stairs", mce::Color(0.376f, 0.627f, 0.565f, 1.00f)},
        {"prismarine_wall", mce::Color(0.376f, 0.627f, 0.565f, 1.00f)},
        {"pumpkin", mce::Color(0.773f, 0.463f, 0.098f, 1.00f)},
        {"purple_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"purple_bed", mce::Color(0.498f, 0.165f, 0.694f, 1.00f)},
        {"purple_candle", mce::Color(0.420f, 0.133f, 0.643f, 1.00f)},
        {"purple_candle_cake", mce::Color(0.420f, 0.133f, 0.643f, 1.00f)},
        {"purple_carpet", mce::Color(0.478f, 0.165f, 0.675f, 1.00f)},
        {"purple_concrete", mce::Color(0.392f, 0.122f, 0.612f, 1.00f)},
        {"purple_concrete_powder", mce::Color(0.506f, 0.212f, 0.686f, 1.00f)},
        {"purple_glazed_terracotta", mce::Color(0.435f, 0.204f, 0.596f, 1.00f)},
        {"purple_shulker_box", mce::Color(0.408f, 0.129f, 0.620f, 1.00f)},
        {"purple_stained_glass", mce::Color(0.498f, 0.247f, 0.698f, 1.00f)},
        {"purple_stained_glass_pane", mce::Color(0.475f, 0.235f, 0.663f, 1.00f)},
        {"purple_terracotta", mce::Color(0.463f, 0.275f, 0.337f, 1.00f)},
        {"purple_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"purple_wool", mce::Color(0.478f, 0.165f, 0.675f, 1.00f)},
        {"purpur_block", mce::Color(0.702f, 0.537f, 0.702f, 1.00f)},
        {"purpur_pillar", mce::Color(0.710f, 0.545f, 0.706f, 1.00f)},
        {"purpur_slab", mce::Color(0.702f, 0.537f, 0.702f, 1.00f)},
        {"purpur_stairs", mce::Color(0.702f, 0.537f, 0.702f, 1.00f)},
        {"quartz_block", mce::Color(0.933f, 0.910f, 0.886f, 1.00f)},
        {"quartz_bricks", mce::Color(0.933f, 0.914f, 0.890f, 1.00f)},
        {"quartz_ore", mce::Color(0.459f, 0.251f, 0.239f, 1.00f)},
        {"quartz_pillar", mce::Color(0.922f, 0.898f, 0.871f, 1.00f)},
        {"quartz_slab", mce::Color(0.933f, 0.910f, 0.886f, 1.00f)},
        {"quartz_stairs", mce::Color(0.933f, 0.910f, 0.886f, 1.00f)},
        {"rail", mce::Color(0.435f, 0.392f, 0.318f, 1.00f)},
        {"raw_copper_block", mce::Color(0.608f, 0.416f, 0.310f, 1.00f)},
        {"raw_gold_block", mce::Color(0.875f, 0.671f, 0.176f, 1.00f)},
        {"raw_iron_block", mce::Color(0.647f, 0.529f, 0.420f, 1.00f)},
        {"red_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"red_bed", mce::Color(0.694f, 0.184f, 0.153f, 1.00f)},
        {"red_candle", mce::Color(0.620f, 0.149f, 0.133f, 1.00f)},
        {"red_candle_cake", mce::Color(0.620f, 0.149f, 0.133f, 1.00f)},
        {"red_carpet", mce::Color(0.631f, 0.153f, 0.133f, 1.00f)},
        {"red_concrete", mce::Color(0.557f, 0.125f, 0.125f, 1.00f)},
        {"red_concrete_powder", mce::Color(0.663f, 0.216f, 0.200f, 1.00f)},
        {"red_flower", mce::Color(0.788f, 0.161f, 0.145f, 1.00f)},
        {"red_glazed_terracotta", mce::Color(0.678f, 0.231f, 0.212f, 1.00f)},
        {"red_mushroom", mce::Color(0.824f, 0.263f, 0.235f, 1.00f)},
        {"red_mushroom_block", mce::Color(0.784f, 0.180f, 0.173f, 1.00f)},
        {"red_nether_brick_slab", mce::Color(0.333f, 0.047f, 0.059f, 1.00f)},
        {"red_nether_brick_stairs", mce::Color(0.333f, 0.047f, 0.059f, 1.00f)},
        {"red_nether_brick_wall", mce::Color(0.333f, 0.047f, 0.059f, 1.00f)},
        {"red_nether_bricks", mce::Color(0.333f, 0.047f, 0.059f, 1.00f)},
        {"red_sand", mce::Color(0.748f, 0.404f, 0.130f, 1.00f)},
        {"red_sandstone", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"red_sandstone_slab", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"red_sandstone_stairs", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"red_sandstone_wall", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"red_shulker_box", mce::Color(0.557f, 0.122f, 0.118f, 1.00f)},
        {"red_stained_glass", mce::Color(0.600f, 0.200f, 0.200f, 1.00f)},
        {"red_stained_glass_pane", mce::Color(0.569f, 0.184f, 0.184f, 1.00f)},
        {"red_terracotta", mce::Color(0.561f, 0.239f, 0.180f, 1.00f)},
        {"red_tulip", mce::Color(0.824f, 0.173f, 0.161f, 1.00f)},
        {"red_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"red_wool", mce::Color(0.631f, 0.153f, 0.133f, 1.00f)},
        {"redstone_block", mce::Color(0.690f, 0.098f, 0.020f, 1.00f)},
        {"redstone_lamp", mce::Color(0.369f, 0.212f, 0.114f, 1.00f)},
        {"redstone_ore", mce::Color(0.557f, 0.420f, 0.420f, 1.00f)},
        {"redstone_torch", mce::Color(0.970f, 0.080f, 0.080f, 1.00f)},
        {"redstone_wall_torch", mce::Color(0.970f, 0.080f, 0.080f, 1.00f)},
        {"unlit_redstone_torch", mce::Color(0.352f, 0.140f, 0.088f, 1.00f)},
        {"redstone_wire", mce::Color(0.969f, 0.969f, 0.969f, 1.00f)},
        {"reinforced_deepslate", mce::Color(0.314f, 0.325f, 0.310f, 1.00f)},
        {"repeater", mce::Color(0.306f, 0.200f, 0.125f, 1.00f)},
        {"repeating_command_block", mce::Color(0.478f, 0.380f, 0.624f, 1.00f)},
        {"resin_block", mce::Color(0.839f, 0.369f, 0.086f, 1.00f)},
        {"resin_brick_slab", mce::Color(0.886f, 0.435f, 0.122f, 1.00f)},
        {"resin_brick_stairs", mce::Color(0.886f, 0.435f, 0.122f, 1.00f)},
        {"resin_brick_wall", mce::Color(0.886f, 0.435f, 0.122f, 1.00f)},
        {"resin_bricks", mce::Color(0.886f, 0.435f, 0.122f, 1.00f)},
        {"resin_clump", mce::Color(0.890f, 0.455f, 0.094f, 1.00f)},
        {"respawn_anchor", mce::Color(0.133f, 0.082f, 0.208f, 1.00f)},
        {"rooted_dirt", mce::Color(0.569f, 0.408f, 0.302f, 1.00f)},
        {"rose_bush", mce::Color(0.808f, 0.169f, 0.161f, 1.00f)},
        {"rubber_trapdoor", mce::Color(0.443f, 0.373f, 0.208f, 1.00f)},
        {"sand", mce::Color(0.855f, 0.804f, 0.631f, 1.00f)},
        {"sandstone", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"sandstone_slab", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"sandstone_stairs", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"sandstone_wall", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"sapling", mce::Color(0.404f, 0.384f, 0.216f, 1.00f)},
        {"scaffolding", mce::Color(0.667f, 0.510f, 0.278f, 1.00f)},
        {"sculk", mce::Color(0.051f, 0.118f, 0.141f, 1.00f)},
        {"sculk_catalyst", mce::Color(0.059f, 0.125f, 0.149f, 1.00f)},
        {"sculk_sensor", mce::Color(0.027f, 0.255f, 0.310f, 1.00f)},
        {"sculk_shrieker", mce::Color(0.267f, 0.396f, 0.365f, 1.00f)},
        {"sculk_vein", mce::Color(0.031f, 0.204f, 0.243f, 1.00f)},
        {"sea_lantern", mce::Color(0.675f, 0.784f, 0.749f, 1.00f)},
        {"sea_pickle", mce::Color(0.349f, 0.376f, 0.153f, 1.00f)},
        {"seagrass", mce::Color(0.188f, 0.478f, 0.020f, 1.00f)},
        {"short_dry_grass", mce::Color(0.733f, 0.622f, 0.424f, 1.00f)},
        {"shroomlight", mce::Color(0.949f, 0.588f, 0.294f, 1.00f)},
        {"shulker_box", mce::Color(0.553f, 0.384f, 0.553f, 1.00f)},
        {"skeleton_skull", mce::Color(0.705f, 0.704f, 0.705f, 1.00f)},
        {"skeleton_wall_skull", mce::Color(0.705f, 0.704f, 0.705f, 1.00f)},
        {"skull", mce::Color(0.705f, 0.704f, 0.705f, 1.00f)},
        {"silver_glazed_terracotta", mce::Color(0.565f, 0.651f, 0.655f, 1.00f)},
        {"silver_terracotta", mce::Color(0.529f, 0.416f, 0.380f, 1.00f)},
        {"slime", mce::Color(0.439f, 0.757f, 0.361f, 0.71f)},
        {"slime_block", mce::Color(0.439f, 0.757f, 0.361f, 0.71f)},
        {"small_amethyst_bud", mce::Color(0.537f, 0.408f, 0.773f, 1.00f)},
        {"small_dripleaf", mce::Color(0.373f, 0.463f, 0.180f, 1.00f)},
        {"smithing_table", mce::Color(0.220f, 0.227f, 0.275f, 1.00f)},
        {"smoker", mce::Color(0.333f, 0.325f, 0.318f, 1.00f)},
        {"smooth_basalt", mce::Color(0.282f, 0.282f, 0.306f, 1.00f)},
        {"smooth_quartz", mce::Color(0.925f, 0.902f, 0.878f, 1.00f)},
        {"smooth_quartz_slab", mce::Color(0.925f, 0.902f, 0.878f, 1.00f)},
        {"smooth_quartz_stairs", mce::Color(0.925f, 0.902f, 0.878f, 1.00f)},
        {"smooth_red_sandstone", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"smooth_red_sandstone_slab", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"smooth_red_sandstone_stairs", mce::Color(0.711f, 0.384f, 0.123f, 1.00f)},
        {"smooth_sandstone", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"smooth_sandstone_slab", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"smooth_sandstone_stairs", mce::Color(0.875f, 0.835f, 0.667f, 1.00f)},
        {"smooth_stone", mce::Color(0.620f, 0.620f, 0.620f, 1.00f)},
        {"smooth_stone_slab", mce::Color(0.620f, 0.620f, 0.620f, 1.00f)},
        {"sniffer_egg", mce::Color(0.545f, 0.404f, 0.263f, 1.00f)},
        {"snow", mce::Color(0.976f, 0.996f, 0.996f, 1.00f)},
        {"snow_block", mce::Color(0.976f, 0.996f, 0.996f, 1.00f)},
        {"soul_campfire", mce::Color(0.320f, 0.800f, 0.810f, 1.00f)},
        {"soul_fire", mce::Color(0.255f, 0.812f, 0.831f, 1.00f)},
        {"soul_lantern", mce::Color(0.250f, 0.850f, 0.880f, 1.00f)},
        {"soul_sand", mce::Color(0.330f, 0.250f, 0.200f, 1.00f)},
        {"soul_soil", mce::Color(0.265f, 0.200f, 0.160f, 1.00f)},
        {"soul_torch", mce::Color(0.250f, 0.850f, 0.880f, 1.00f)},
        {"soul_wall_torch", mce::Color(0.250f, 0.850f, 0.880f, 1.00f)},
        {"spawner", mce::Color(0.137f, 0.157f, 0.227f, 1.00f)},
        {"sponge", mce::Color(0.765f, 0.753f, 0.290f, 1.00f)},
        {"spore_blossom", mce::Color(0.816f, 0.376f, 0.627f, 1.00f)},
        {"spruce_button", mce::Color(0.494f, 0.365f, 0.208f, 1.00f)},
        {"spruce_door", mce::Color(0.439f, 0.333f, 0.204f, 1.00f)},
        {"spruce_fence", mce::Color(0.494f, 0.365f, 0.208f, 1.00f)},
        {"spruce_fence_gate", mce::Color(0.494f, 0.365f, 0.208f, 1.00f)},
        {"spruce_hanging_sign", mce::Color(0.435f, 0.345f, 0.239f, 1.00f)},
        {"spruce_log", mce::Color(0.427f, 0.314f, 0.180f, 1.00f)},
        {"spruce_planks", mce::Color(0.494f, 0.365f, 0.208f, 1.00f)},
        {"spruce_pressure_plate", mce::Color(0.494f, 0.365f, 0.208f, 1.00f)},
        {"spruce_sapling", mce::Color(0.180f, 0.247f, 0.149f, 1.00f)},
        {"spruce_shelf", mce::Color(0.408f, 0.318f, 0.176f, 1.00f)},
        {"spruce_sign", mce::Color(0.435f, 0.318f, 0.180f, 1.00f)},
        {"spruce_slab", mce::Color(0.494f, 0.365f, 0.208f, 1.00f)},
        {"spruce_stairs", mce::Color(0.494f, 0.365f, 0.208f, 1.00f)},
        {"spruce_trapdoor", mce::Color(0.404f, 0.306f, 0.188f, 1.00f)},
        {"spruce_wall_hanging_sign", mce::Color(0.435f, 0.345f, 0.239f, 1.00f)},
        {"spruce_wall_sign", mce::Color(0.435f, 0.318f, 0.180f, 1.00f)},
        {"spruce_wood", mce::Color(0.243f, 0.161f, 0.075f, 1.00f)},
        {"standing_sign", mce::Color(0.655f, 0.529f, 0.318f, 1.00f)},
        {"stained_glass", mce::Color(0.784f, 0.898f, 0.902f, 1.00f)},
        {"stained_glass_pane", mce::Color(0.616f, 0.792f, 0.831f, 1.00f)},
        {"stained_hardened_clay", mce::Color(0.596f, 0.369f, 0.263f, 1.00f)},
        {"sticky_piston", mce::Color(0.451f, 0.431f, 0.396f, 1.00f)},
        {"stone", mce::Color(0.486f, 0.486f, 0.486f, 1.00f)},
        {"stone_brick_slab", mce::Color(0.502f, 0.498f, 0.502f, 1.00f)},
        {"stone_brick_stairs", mce::Color(0.502f, 0.498f, 0.502f, 1.00f)},
        {"stone_brick_wall", mce::Color(0.502f, 0.498f, 0.502f, 1.00f)},
        {"stone_bricks", mce::Color(0.502f, 0.498f, 0.502f, 1.00f)},
        {"stone_button", mce::Color(0.486f, 0.486f, 0.486f, 1.00f)},
        {"stone_pressure_plate", mce::Color(0.486f, 0.486f, 0.486f, 1.00f)},
        {"stone_slab", mce::Color(0.486f, 0.486f, 0.486f, 1.00f)},
        {"stone_stairs", mce::Color(0.486f, 0.486f, 0.486f, 1.00f)},
        {"stonebrick", mce::Color(0.502f, 0.498f, 0.502f, 1.00f)},
        {"stonecutter", mce::Color(0.475f, 0.455f, 0.427f, 1.00f)},
        {"stripped_acacia_log", mce::Color(0.647f, 0.353f, 0.200f, 1.00f)},
        {"stripped_acacia_wood", mce::Color(0.682f, 0.365f, 0.231f, 1.00f)},
        {"stripped_bamboo_block", mce::Color(0.784f, 0.706f, 0.314f, 1.00f)},
        {"stripped_birch_log", mce::Color(0.745f, 0.671f, 0.451f, 1.00f)},
        {"stripped_birch_wood", mce::Color(0.773f, 0.690f, 0.463f, 1.00f)},
        {"stripped_cherry_log", mce::Color(0.839f, 0.569f, 0.580f, 1.00f)},
        {"stripped_cherry_wood", mce::Color(0.839f, 0.569f, 0.580f, 1.00f)},
        {"stripped_crimson_hyphae", mce::Color(0.533f, 0.220f, 0.349f, 1.00f)},
        {"stripped_crimson_stem", mce::Color(0.475f, 0.220f, 0.322f, 1.00f)},
        {"stripped_dark_oak_log", mce::Color(0.255f, 0.173f, 0.086f, 1.00f)},
        {"stripped_dark_oak_wood", mce::Color(0.286f, 0.224f, 0.141f, 1.00f)},
        {"stripped_jungle_log", mce::Color(0.647f, 0.478f, 0.318f, 1.00f)},
        {"stripped_jungle_wood", mce::Color(0.671f, 0.522f, 0.333f, 1.00f)},
        {"stripped_mangrove_log", mce::Color(0.427f, 0.169f, 0.169f, 1.00f)},
        {"stripped_mangrove_wood", mce::Color(0.467f, 0.212f, 0.184f, 1.00f)},
        {"stripped_oak_log", mce::Color(0.627f, 0.506f, 0.302f, 1.00f)},
        {"stripped_oak_wood", mce::Color(0.694f, 0.561f, 0.333f, 1.00f)},
        {"stripped_pale_oak_log", mce::Color(0.875f, 0.843f, 0.839f, 1.00f)},
        {"stripped_pale_oak_wood", mce::Color(0.918f, 0.890f, 0.886f, 1.00f)},
        {"stripped_spruce_log", mce::Color(0.443f, 0.337f, 0.196f, 1.00f)},
        {"stripped_spruce_wood", mce::Color(0.451f, 0.349f, 0.204f, 1.00f)},
        {"stripped_warped_hyphae", mce::Color(0.224f, 0.588f, 0.576f, 1.00f)},
        {"stripped_warped_stem", mce::Color(0.204f, 0.502f, 0.486f, 1.00f)},
        {"structure_block", mce::Color(0.267f, 0.224f, 0.271f, 1.00f)},
        {"sulfur", mce::Color(0.714f, 0.659f, 0.392f, 1.00f)},
        {"sulfur_brick", mce::Color(0.714f, 0.659f, 0.392f, 1.00f)},
        {"sulfur_brick_slab", mce::Color(0.698f, 0.631f, 0.353f, 1.00f)},
        {"sulfur_brick_stairs", mce::Color(0.694f, 0.627f, 0.349f, 1.00f)},
        {"sulfur_brick_wall", mce::Color(0.596f, 0.569f, 0.322f, 1.00f)},
        {"sulfur_bricks", mce::Color(0.765f, 0.710f, 0.373f, 1.00f)},
        {"sulfur_slab", mce::Color(0.714f, 0.659f, 0.392f, 1.00f)},
        {"sulfur_spike", mce::Color(0.706f, 0.651f, 0.369f, 1.00f)},
        {"sulfur_stairs", mce::Color(0.690f, 0.643f, 0.380f, 1.00f)},
        {"sunflower", mce::Color(0.973f, 0.784f, 0.212f, 1.00f)},
        {"suspicious_gravel", mce::Color(0.502f, 0.482f, 0.478f, 1.00f)},
        {"suspicious_sand", mce::Color(0.847f, 0.792f, 0.612f, 1.00f)},
        {"sweet_berry_bush", mce::Color(0.192f, 0.373f, 0.224f, 1.00f)},
        {"tall_dry_grass", mce::Color(0.771f, 0.674f, 0.482f, 1.00f)},
        {"tall_seagrass", mce::Color(0.180f, 0.451f, 0.024f, 1.00f)},
        {"target", mce::Color(0.867f, 0.663f, 0.620f, 1.00f)},
        {"test_block", mce::Color(0.522f, 0.761f, 0.471f, 1.00f)},
        {"test_instance_block", mce::Color(0.502f, 0.475f, 0.471f, 1.00f)},
        {"terracotta", mce::Color(0.596f, 0.369f, 0.263f, 1.00f)},
        {"tinted_glass", mce::Color(0.169f, 0.153f, 0.176f, 1.00f)},
        {"tnt", mce::Color(0.604f, 0.275f, 0.212f, 1.00f)},
        {"torch", mce::Color(0.980f, 0.820f, 0.300f, 1.00f)},
        {"torchflower", mce::Color(0.923f, 0.642f, 0.215f, 1.00f)},
        {"torchflower_crop", mce::Color(0.125f, 0.424f, 0.290f, 1.00f)},
        {"trapped_chest", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"trial_spawner", mce::Color(0.247f, 0.314f, 0.361f, 1.00f)},
        {"tripwire", mce::Color(0.522f, 0.522f, 0.522f, 0.65f)},
        {"tripwire_hook", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"trapdoor", mce::Color(0.486f, 0.384f, 0.220f, 1.00f)},
        {"tube_coral", mce::Color(0.184f, 0.325f, 0.761f, 1.00f)},
        {"tube_coral_block", mce::Color(0.188f, 0.341f, 0.804f, 1.00f)},
        {"tube_coral_fan", mce::Color(0.204f, 0.369f, 0.824f, 1.00f)},
        {"tube_coral_wall_fan", mce::Color(0.204f, 0.369f, 0.824f, 1.00f)},
        {"tuff", mce::Color(0.420f, 0.424f, 0.404f, 1.00f)},
        {"tuff_brick_slab", mce::Color(0.408f, 0.424f, 0.400f, 1.00f)},
        {"tuff_brick_stairs", mce::Color(0.408f, 0.424f, 0.400f, 1.00f)},
        {"tuff_brick_wall", mce::Color(0.408f, 0.424f, 0.400f, 1.00f)},
        {"tuff_bricks", mce::Color(0.408f, 0.424f, 0.400f, 1.00f)},
        {"tuff_slab", mce::Color(0.420f, 0.424f, 0.404f, 1.00f)},
        {"tuff_stairs", mce::Color(0.420f, 0.424f, 0.404f, 1.00f)},
        {"tuff_wall", mce::Color(0.420f, 0.424f, 0.404f, 1.00f)},
        {"turtle_egg", mce::Color(0.898f, 0.898f, 0.765f, 1.00f)},
        {"underwater_torch", mce::Color(0.780f, 0.250f, 0.820f, 1.00f)},
        {"twisting_vines", mce::Color(0.075f, 0.561f, 0.482f, 1.00f)},
        {"twisting_vines_plant", mce::Color(0.078f, 0.522f, 0.471f, 1.00f)},
        {"vault", mce::Color(0.208f, 0.267f, 0.306f, 1.00f)},
        {"verdant_froglight", mce::Color(0.898f, 0.957f, 0.894f, 1.00f)},
        {"wall_torch", mce::Color(0.980f, 0.820f, 0.300f, 1.00f)},
        {"wall_sign", mce::Color(0.655f, 0.529f, 0.318f, 1.00f)},
        {"warped_button", mce::Color(0.200f, 0.478f, 0.455f, 1.00f)},
        {"warped_door", mce::Color(0.173f, 0.514f, 0.482f, 1.00f)},
        {"warped_fence", mce::Color(0.200f, 0.478f, 0.455f, 1.00f)},
        {"warped_fence_gate", mce::Color(0.200f, 0.478f, 0.455f, 1.00f)},
        {"warped_fungus", mce::Color(0.275f, 0.408f, 0.329f, 1.00f)},
        {"warped_hanging_sign", mce::Color(0.227f, 0.533f, 0.533f, 1.00f)},
        {"warped_hyphae", mce::Color(0.227f, 0.231f, 0.306f, 1.00f)},
        {"warped_nylium", mce::Color(0.161f, 0.447f, 0.400f, 1.00f)},
        {"warped_planks", mce::Color(0.200f, 0.478f, 0.455f, 1.00f)},
        {"warped_pressure_plate", mce::Color(0.200f, 0.478f, 0.455f, 1.00f)},
        {"warped_roots", mce::Color(0.078f, 0.541f, 0.486f, 1.00f)},
        {"warped_shelf", mce::Color(0.188f, 0.518f, 0.482f, 1.00f)},
        {"warped_sign", mce::Color(0.192f, 0.435f, 0.424f, 1.00f)},
        {"warped_slab", mce::Color(0.200f, 0.478f, 0.455f, 1.00f)},
        {"warped_stairs", mce::Color(0.200f, 0.478f, 0.455f, 1.00f)},
        {"warped_stem", mce::Color(0.208f, 0.427f, 0.427f, 1.00f)},
        {"warped_trapdoor", mce::Color(0.184f, 0.463f, 0.435f, 1.00f)},
        {"warped_wall_hanging_sign", mce::Color(0.227f, 0.533f, 0.533f, 1.00f)},
        {"warped_wall_sign", mce::Color(0.192f, 0.435f, 0.424f, 1.00f)},
        {"warped_wart_block", mce::Color(0.086f, 0.467f, 0.478f, 1.00f)},
        {"water_cauldron", mce::Color(0.702f, 0.702f, 0.702f, 0.71f)},
        {"waxed_chiseled_copper", mce::Color(0.765f, 0.424f, 0.322f, 1.00f)},
        {"waxed_copper", mce::Color(0.725f, 0.408f, 0.298f, 1.00f)},
        {"waxed_copper_bars", mce::Color(0.635f, 0.337f, 0.235f, 1.00f)},
        {"waxed_copper_block", mce::Color(0.792f, 0.447f, 0.337f, 1.00f)},
        {"waxed_copper_bulb", mce::Color(0.651f, 0.369f, 0.251f, 1.00f)},
        {"waxed_copper_chain", mce::Color(0.596f, 0.310f, 0.216f, 1.00f)},
        {"waxed_copper_chest", mce::Color(0.792f, 0.447f, 0.337f, 1.00f)},
        {"waxed_copper_door", mce::Color(0.765f, 0.431f, 0.329f, 1.00f)},
        {"waxed_copper_golem_statue", mce::Color(0.792f, 0.447f, 0.337f, 1.00f)},
        {"waxed_copper_grate", mce::Color(0.796f, 0.451f, 0.341f, 1.00f)},
        {"waxed_copper_lantern", mce::Color(0.643f, 0.431f, 0.302f, 1.00f)},
        {"waxed_copper_trapdoor", mce::Color(0.784f, 0.439f, 0.341f, 1.00f)},
        {"waxed_cut_copper", mce::Color(0.804f, 0.455f, 0.353f, 1.00f)},
        {"waxed_cut_copper_slab", mce::Color(0.804f, 0.455f, 0.353f, 1.00f)},
        {"waxed_cut_copper_stairs", mce::Color(0.804f, 0.455f, 0.353f, 1.00f)},
        {"waxed_exposed_chiseled_copper", mce::Color(0.651f, 0.486f, 0.420f, 1.00f)},
        {"waxed_exposed_copper", mce::Color(0.671f, 0.510f, 0.431f, 1.00f)},
        {"waxed_exposed_copper_bars", mce::Color(0.553f, 0.431f, 0.361f, 1.00f)},
        {"waxed_exposed_copper_bulb", mce::Color(0.573f, 0.439f, 0.376f, 1.00f)},
        {"waxed_exposed_copper_chain", mce::Color(0.498f, 0.400f, 0.325f, 1.00f)},
        {"waxed_exposed_copper_chest", mce::Color(0.671f, 0.510f, 0.431f, 1.00f)},
        {"waxed_exposed_copper_door", mce::Color(0.655f, 0.486f, 0.424f, 1.00f)},
        {"waxed_exposed_copper_golem_statue", mce::Color(0.671f, 0.510f, 0.431f, 1.00f)},
        {"waxed_exposed_copper_grate", mce::Color(0.694f, 0.518f, 0.443f, 1.00f)},
        {"waxed_exposed_copper_lantern", mce::Color(0.608f, 0.525f, 0.424f, 1.00f)},
        {"waxed_exposed_copper_trapdoor", mce::Color(0.675f, 0.506f, 0.435f, 1.00f)},
        {"waxed_exposed_cut_copper", mce::Color(0.663f, 0.506f, 0.431f, 1.00f)},
        {"waxed_exposed_cut_copper_slab", mce::Color(0.663f, 0.506f, 0.431f, 1.00f)},
        {"waxed_exposed_cut_copper_stairs", mce::Color(0.663f, 0.506f, 0.431f, 1.00f)},
        {"waxed_exposed_lightning_rod", mce::Color(0.651f, 0.494f, 0.427f, 1.00f)},
        {"waxed_lightning_rod", mce::Color(0.800f, 0.455f, 0.345f, 1.00f)},
        {"waxed_oxidized_chiseled_copper", mce::Color(0.353f, 0.675f, 0.549f, 1.00f)},
        {"waxed_oxidized_copper", mce::Color(0.345f, 0.682f, 0.557f, 1.00f)},
        {"waxed_oxidized_copper_bars", mce::Color(0.259f, 0.502f, 0.412f, 1.00f)},
        {"waxed_oxidized_copper_bulb", mce::Color(0.302f, 0.569f, 0.467f, 1.00f)},
        {"waxed_oxidized_copper_chain", mce::Color(0.243f, 0.467f, 0.392f, 1.00f)},
        {"waxed_oxidized_copper_chest", mce::Color(0.345f, 0.682f, 0.557f, 1.00f)},
        {"waxed_oxidized_copper_door", mce::Color(0.329f, 0.635f, 0.522f, 1.00f)},
        {"waxed_oxidized_copper_golem_statue", mce::Color(0.345f, 0.682f, 0.557f, 1.00f)},
        {"waxed_oxidized_copper_grate", mce::Color(0.353f, 0.686f, 0.561f, 1.00f)},
        {"waxed_oxidized_copper_lantern", mce::Color(0.310f, 0.561f, 0.447f, 1.00f)},
        {"waxed_oxidized_copper_trapdoor", mce::Color(0.357f, 0.678f, 0.557f, 1.00f)},
        {"waxed_oxidized_cut_copper", mce::Color(0.345f, 0.667f, 0.545f, 1.00f)},
        {"waxed_oxidized_cut_copper_slab", mce::Color(0.345f, 0.667f, 0.545f, 1.00f)},
        {"waxed_oxidized_cut_copper_stairs", mce::Color(0.345f, 0.667f, 0.545f, 1.00f)},
        {"waxed_oxidized_lightning_rod", mce::Color(0.333f, 0.647f, 0.549f, 1.00f)},
        {"waxed_weathered_chiseled_copper", mce::Color(0.427f, 0.627f, 0.478f, 1.00f)},
        {"waxed_weathered_copper", mce::Color(0.443f, 0.631f, 0.467f, 1.00f)},
        {"waxed_weathered_copper_bars", mce::Color(0.361f, 0.506f, 0.369f, 1.00f)},
        {"waxed_weathered_copper_bulb", mce::Color(0.373f, 0.533f, 0.431f, 1.00f)},
        {"waxed_weathered_copper_chain", mce::Color(0.286f, 0.431f, 0.357f, 1.00f)},
        {"waxed_weathered_copper_chest", mce::Color(0.443f, 0.631f, 0.467f, 1.00f)},
        {"waxed_weathered_copper_door", mce::Color(0.439f, 0.596f, 0.439f, 1.00f)},
        {"waxed_weathered_copper_golem_statue", mce::Color(0.443f, 0.631f, 0.467f, 1.00f)},
        {"waxed_weathered_copper_grate", mce::Color(0.435f, 0.643f, 0.482f, 1.00f)},
        {"waxed_weathered_copper_lantern", mce::Color(0.388f, 0.529f, 0.392f, 1.00f)},
        {"waxed_weathered_copper_trapdoor", mce::Color(0.451f, 0.639f, 0.471f, 1.00f)},
        {"waxed_weathered_cut_copper", mce::Color(0.451f, 0.627f, 0.467f, 1.00f)},
        {"waxed_weathered_cut_copper_slab", mce::Color(0.451f, 0.627f, 0.467f, 1.00f)},
        {"waxed_weathered_cut_copper_stairs", mce::Color(0.451f, 0.627f, 0.467f, 1.00f)},
        {"waxed_weathered_lightning_rod", mce::Color(0.416f, 0.584f, 0.431f, 1.00f)},
        {"weathered_chiseled_copper", mce::Color(0.427f, 0.627f, 0.478f, 1.00f)},
        {"weathered_copper", mce::Color(0.443f, 0.631f, 0.467f, 1.00f)},
        {"weathered_copper_bars", mce::Color(0.361f, 0.506f, 0.369f, 1.00f)},
        {"weathered_copper_bulb", mce::Color(0.373f, 0.533f, 0.431f, 1.00f)},
        {"weathered_copper_chain", mce::Color(0.286f, 0.431f, 0.357f, 1.00f)},
        {"weathered_copper_chest", mce::Color(0.443f, 0.631f, 0.467f, 1.00f)},
        {"weathered_copper_door", mce::Color(0.439f, 0.596f, 0.439f, 1.00f)},
        {"weathered_copper_golem_statue", mce::Color(0.443f, 0.631f, 0.467f, 1.00f)},
        {"weathered_copper_grate", mce::Color(0.435f, 0.643f, 0.482f, 1.00f)},
        {"weathered_copper_lantern", mce::Color(0.388f, 0.529f, 0.392f, 1.00f)},
        {"weathered_copper_trapdoor", mce::Color(0.451f, 0.639f, 0.471f, 1.00f)},
        {"weathered_cut_copper", mce::Color(0.451f, 0.627f, 0.467f, 1.00f)},
        {"weathered_cut_copper_slab", mce::Color(0.451f, 0.627f, 0.467f, 1.00f)},
        {"weathered_cut_copper_stairs", mce::Color(0.451f, 0.627f, 0.467f, 1.00f)},
        {"weathered_lightning_rod", mce::Color(0.416f, 0.584f, 0.431f, 1.00f)},
        {"web", mce::Color(0.910f, 0.925f, 0.929f, 1.00f)},
        {"weeping_vines", mce::Color(0.408f, 0.004f, 0.000f, 1.00f)},
        {"weeping_vines_plant", mce::Color(0.522f, 0.067f, 0.055f, 1.00f)},
        {"wet_sponge", mce::Color(0.667f, 0.698f, 0.271f, 1.00f)},
        {"wheat", mce::Color(0.031f, 0.533f, 0.024f, 1.00f)},
        {"white_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"white_bed", mce::Color(0.953f, 0.957f, 0.957f, 1.00f)},
        {"white_candle", mce::Color(0.855f, 0.882f, 0.882f, 1.00f)},
        {"white_candle_cake", mce::Color(0.855f, 0.882f, 0.882f, 1.00f)},
        {"white_carpet", mce::Color(0.918f, 0.929f, 0.929f, 1.00f)},
        {"white_concrete", mce::Color(0.812f, 0.835f, 0.839f, 1.00f)},
        {"white_concrete_powder", mce::Color(0.882f, 0.890f, 0.890f, 1.00f)},
        {"white_glazed_terracotta", mce::Color(0.850f, 0.870f, 0.850f, 1.00f)},
        {"white_shulker_box", mce::Color(0.851f, 0.871f, 0.875f, 1.00f)},
        {"white_stained_glass", mce::Color(1.000f, 1.000f, 1.000f, 1.00f)},
        {"white_stained_glass_pane", mce::Color(0.949f, 0.949f, 0.949f, 1.00f)},
        {"white_terracotta", mce::Color(0.820f, 0.698f, 0.631f, 1.00f)},
        {"white_tulip", mce::Color(0.804f, 0.871f, 0.871f, 1.00f)},
        {"white_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"white_wool", mce::Color(0.918f, 0.929f, 0.929f, 1.00f)},
        {"wildflowers", mce::Color(0.973f, 0.847f, 0.392f, 1.00f)},
        {"wither_rose", mce::Color(0.176f, 0.184f, 0.102f, 1.00f)},
        {"wither_skeleton_skull", mce::Color(0.169f, 0.169f, 0.169f, 1.00f)},
        {"wither_skeleton_wall_skull", mce::Color(0.169f, 0.169f, 0.169f, 1.00f)},
        {"wooden_door", mce::Color(0.565f, 0.447f, 0.275f, 1.00f)},
        {"wooden_pressure_plate", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"yellow_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"yellow_bed", mce::Color(0.996f, 0.788f, 0.141f, 1.00f)},
        {"yellow_candle", mce::Color(0.851f, 0.667f, 0.196f, 1.00f)},
        {"yellow_candle_cake", mce::Color(0.851f, 0.667f, 0.196f, 1.00f)},
        {"yellow_carpet", mce::Color(0.973f, 0.776f, 0.157f, 1.00f)},
        {"yellow_concrete", mce::Color(0.945f, 0.686f, 0.082f, 1.00f)},
        {"yellow_concrete_powder", mce::Color(0.910f, 0.776f, 0.216f, 1.00f)},
        {"yellow_flower", mce::Color(0.957f, 0.804f, 0.251f, 1.00f)},
        {"yellow_glazed_terracotta", mce::Color(0.902f, 0.741f, 0.337f, 1.00f)},
        {"yellow_shulker_box", mce::Color(0.976f, 0.745f, 0.118f, 1.00f)},
        {"yellow_stained_glass", mce::Color(0.898f, 0.898f, 0.200f, 1.00f)},
        {"yellow_stained_glass_pane", mce::Color(0.851f, 0.851f, 0.184f, 1.00f)},
        {"yellow_terracotta", mce::Color(0.729f, 0.522f, 0.137f, 1.00f)},
        {"yellow_wall_banner", mce::Color(0.702f, 0.573f, 0.349f, 1.00f)},
        {"yellow_wool", mce::Color(0.973f, 0.776f, 0.157f, 1.00f)},
        {"zombie_head", mce::Color(0.257f, 0.419f, 0.190f, 1.00f)},
        {"zombie_wall_head", mce::Color(0.257f, 0.419f, 0.190f, 1.00f)},
    };

    // 去除命名空间前缀 "minecraft:" 与强化玻璃前缀 "hard_" 便于快速精确查找
    std::string cleanName = name;
    if (cleanName.rfind("minecraft:", 0) == 0) cleanName = cleanName.substr(10);
    if (cleanName.rfind("hard_", 0) == 0) cleanName = cleanName.substr(5);

    auto exactIt = s_exactBlockColors.find(cleanName);
    if (exactIt != s_exactBlockColors.end()) {
        return exactIt->second;
    }

    // [蛙明灯 (Froglights)] 实机高亮发光采样
    if (name.find("froglight") != std::string::npos) {
        if (name.find("ochre") != std::string::npos) return mce::Color(0.95f, 0.94f, 0.82f, 1.0f);       // 赭黄 #F2EFD0
        if (name.find("pearlescent") != std::string::npos) return mce::Color(0.93f, 0.91f, 0.91f, 1.0f); // 珠光 #EEE8E8
        if (name.find("verdant") != std::string::npos) return mce::Color(0.89f, 0.93f, 0.89f, 1.0f);     // 青翠 #E4EEE4
        return mce::Color(0.93f, 0.93f, 0.88f, 1.0f);
    }

    // [苍白橡木全套 (Pale Oak)] 原版冷调银灰白系列 - 优先于通用木材规则
    if (name.find("pale_oak") != std::string::npos) {
        if (name.find("leaves") != std::string::npos) return mce::Color(0.431f, 0.451f, 0.420f, 1.0f);
        if (name.find("sapling") != std::string::npos) return mce::Color(0.45f, 0.52f, 0.38f, 1.0f);
        if (name.find("stripped") != std::string::npos) return mce::Color(0.87f, 0.84f, 0.84f, 1.0f); // 去皮苍白橡木 #DFD7D6
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            // 原木横截面/树皮
            return mce::Color(0.33f, 0.29f, 0.29f, 1.0f); // 深暗灰褐色树皮 #554B49
        }
        // 木板/台阶/楼梯/栅栏/门/活板门/压力板/告示牌
        return mce::Color(0.86f, 0.82f, 0.82f, 1.0f); // 冷白浅灰 #DBD1D0
    }

    // [铜质全套建筑体系 (Copper System)] 含普通/斑驳/锈蚀/氧化、切制、雕纹、格栅、铜门、活板门、铜灯
    if (name.find("copper") != std::string::npos && name.find("ore") == std::string::npos && name.find("raw") == std::string::npos) {
        if (name.find("oxidized") != std::string::npos) {
            // 氧化阶段 (蓝绿)
            if (name.find("bulb") != std::string::npos) return mce::Color(0.35f, 0.60f, 0.55f, 1.0f);
            return mce::Color(0.30f, 0.59f, 0.52f, 1.0f); // #4D9785
        }
        if (name.find("weathered") != std::string::npos) {
            // 锈蚀阶段 (灰蓝绿)
            if (name.find("bulb") != std::string::npos) return mce::Color(0.42f, 0.55f, 0.48f, 1.0f);
            return mce::Color(0.39f, 0.53f, 0.45f, 1.0f); // #638874
        }
        if (name.find("exposed") != std::string::npos) {
            // 斑驳阶段 (暗棕绿)
            if (name.find("bulb") != std::string::npos) return mce::Color(0.62f, 0.50f, 0.42f, 1.0f);
            return mce::Color(0.59f, 0.47f, 0.39f, 1.0f); // #967863
        }
        // 初始无氧化阶段 (红铜)
        if (name.find("bulb") != std::string::npos) return mce::Color(0.75f, 0.45f, 0.35f, 1.0f);
        return mce::Color(0.72f, 0.40f, 0.30f, 1.0f); // #B8674D
    }

    // [深层矿石体系 (Deepslate Ores)] 深板岩基底 + 矿石共生色
    if (name.find("deepslate") != std::string::npos && name.find("ore") != std::string::npos) {
        if (name.find("coal") != std::string::npos) return mce::Color(0.31f, 0.31f, 0.31f, 1.0f);     // #4E4E50
        if (name.find("iron") != std::string::npos) return mce::Color(0.46f, 0.42f, 0.40f, 1.0f);     // #756B66
        if (name.find("copper") != std::string::npos) return mce::Color(0.42f, 0.35f, 0.32f, 1.0f);   // #6A5A52
        if (name.find("gold") != std::string::npos) return mce::Color(0.38f, 0.34f, 0.29f, 1.0f);     // #625749
        if (name.find("redstone") != std::string::npos) return mce::Color(0.51f, 0.23f, 0.23f, 1.0f); // #813A3B
        if (name.find("lapis") != std::string::npos) return mce::Color(0.23f, 0.30f, 0.41f, 1.0f);    // #3B4D68
        if (name.find("emerald") != std::string::npos) return mce::Color(0.25f, 0.36f, 0.29f, 1.0f);  // #405C4A
        if (name.find("diamond") != std::string::npos) return mce::Color(0.31f, 0.47f, 0.47f, 1.0f);  // #4F7878
        return mce::Color(0.35f, 0.35f, 0.36f, 1.0f);
    }

    // [金属块与粗矿块] 实机采样高精度色
    if (name.find("raw_iron_block") != std::string::npos || name.find("raw_iron") != std::string::npos) return mce::Color(0.63f, 0.51f, 0.40f, 1.0f); // #A08367
    if (name.find("raw_gold_block") != std::string::npos || name.find("raw_gold") != std::string::npos) return mce::Color(0.84f, 0.64f, 0.17f, 1.0f); // #D6A32C
    if (name.find("raw_copper_block") != std::string::npos || name.find("raw_copper") != std::string::npos) return mce::Color(0.56f, 0.40f, 0.29f, 1.0f); // #8F654B
    if (name.find("iron_block") != std::string::npos || name.find("block_of_iron") != std::string::npos) return mce::Color(0.84f, 0.84f, 0.84f, 1.0f); // 纯银白 #D5D5D5
    if (name.find("diamond_block") != std::string::npos || name.find("block_of_diamond") != std::string::npos) return mce::Color(0.40f, 0.94f, 0.89f, 1.0f); // 天青碧蓝 #67F0E3
    if (name.find("heavy_weighted_pressure_plate") != std::string::npos) return mce::Color(0.85f, 0.85f, 0.85f, 1.0f); // 铁压力板 #DADAD9
    if (name.find("light_weighted_pressure_plate") != std::string::npos) return mce::Color(0.97f, 0.82f, 0.22f, 1.0f); // 金压力板 #F8D239

    // [失活珊瑚体系 (Dead Coral)] 原版干枯钙化灰白质感
    if (name.find("dead_") != std::string::npos && name.find("coral") != std::string::npos) {
        if (name.find("block") != std::string::npos) return mce::Color(0.49f, 0.46f, 0.45f, 1.0f); // 珊瑚块 #7E7672
        if (name.find("fan") != std::string::npos) return mce::Color(0.46f, 0.47f, 0.41f, 1.0f);   // 珊瑚扇 #767869
        return mce::Color(0.48f, 0.47f, 0.43f, 1.0f);
    }

    // [树脂体系 (Resin)] 鲜艳树脂金橙琥珀色
    if (name.find("resin") != std::string::npos) {
        if (name.find("clump") != std::string::npos) return mce::Color(0.40f, 0.42f, 0.20f, 1.0f);
        if (name.find("brick") != std::string::npos) return mce::Color(0.79f, 0.35f, 0.09f, 1.0f); // 树脂砖 #CA5818
        return mce::Color(0.82f, 0.38f, 0.10f, 1.0f); // 树脂块 #D26019
    }

    // [末地石与末地石砖] 恢复原版纯正浅黄白岩石色 (彻底纠正原暗灰绿)
    if (name.find("end_stone") != std::string::npos || name.find("end_brick") != std::string::npos) return mce::Color(0.84f, 0.85f, 0.60f, 1.0f); // #D5D99A
    // [蛋糕] 奶油白糖霜顶面配果干红
    if (name.find("cake") != std::string::npos) return mce::Color(0.94f, 0.73f, 0.71f, 1.0f); // #EFBBB5
    // [附魔台] 羊皮纸翻开书页色
    if (name.find("enchanting_table") != std::string::npos || name.find("enchantment_table") != std::string::npos) return mce::Color(0.81f, 0.75f, 0.58f, 1.0f); // #CEBE95
    // [海绵与湿海绵]
    if (name.find("wet_sponge") != std::string::npos) return mce::Color(0.65f, 0.69f, 0.27f, 1.0f); // #A5AF44
    if (name.find("sponge") != std::string::npos) return mce::Color(0.75f, 0.73f, 0.28f, 1.0f);     // #BEBA48
    // [红石灯] 未通电深红木褐格栅
    if (name.find("redstone_lamp") != std::string::npos) return mce::Color(0.36f, 0.21f, 0.11f, 1.0f); // #5C351D
    // [凝灰岩全套 (Tuff)] 中性暗褐灰
    if (name.find("tuff") != std::string::npos) return mce::Color(0.41f, 0.42f, 0.39f, 1.0f); // #696A64
    // [书架与雕纹书架] 顶面橡木纹理色
    if (name.find("bookshelf") != std::string::npos) return mce::Color(0.63f, 0.51f, 0.31f, 1.0f); // #A1814E
    // [嘎吱之心]
    if (name.find("creaking_heart") != std::string::npos) return mce::Color(0.38f, 0.35f, 0.32f, 1.0f);
    // [眼眸花 (Eyeblossom)] 闭眼为灰褐色(eyeblossom_dormant.png RGB 108,98,101)，开眼为特征明亮橙光(RGB 181,89,31)
    if (name.find("closed_eyeblossom") != std::string::npos || name.find("dormant") != std::string::npos) {
        return mce::Color(0.424f, 0.386f, 0.397f, 1.0f); // 闭合灰白色 #6C6265
    }
    if (name.find("open_eyeblossom") != std::string::npos || name.find("eyeblossom") != std::string::npos) {
        return mce::Color(0.71f, 0.35f, 0.12f, 1.0f); // 绽放橙光
    }
    // [潮涌核心]
    if (name.find("conduit") != std::string::npos) return mce::Color(0.62f, 0.55f, 0.45f, 1.0f);
    // [试炼刷怪笼与宝库]
    if (name.find("trial_spawner") != std::string::npos) return mce::Color(0.45f, 0.38f, 0.32f, 1.0f);
    if (name.find("vault") != std::string::npos) return mce::Color(0.42f, 0.39f, 0.35f, 1.0f);
    // [紫水晶芽]
    if (name.find("amethyst_bud") != std::string::npos) return mce::Color(0.70f, 0.58f, 0.78f, 1.0f);
    // [玻璃与染色玻璃通用回退]
    if (name.find("glass") != std::string::npos) {
        if (name.find("light_blue") != std::string::npos) return mce::Color(0.400f, 0.600f, 0.847f, 1.00f);
        if (name.find("blue") != std::string::npos) return mce::Color(0.200f, 0.298f, 0.698f, 1.00f);
        if (name.find("cyan") != std::string::npos) return mce::Color(0.298f, 0.498f, 0.600f, 1.00f);
        if (name.find("lime") != std::string::npos) return mce::Color(0.498f, 0.800f, 0.098f, 1.00f);
        if (name.find("green") != std::string::npos) return mce::Color(0.400f, 0.498f, 0.200f, 1.00f);
        if (name.find("pink") != std::string::npos) return mce::Color(0.949f, 0.498f, 0.647f, 1.00f);
        if (name.find("magenta") != std::string::npos) return mce::Color(0.698f, 0.298f, 0.847f, 1.00f);
        if (name.find("purple") != std::string::npos) return mce::Color(0.498f, 0.247f, 0.698f, 1.00f);
        if (name.find("yellow") != std::string::npos) return mce::Color(0.898f, 0.898f, 0.200f, 1.00f);
        if (name.find("orange") != std::string::npos) return mce::Color(0.847f, 0.498f, 0.200f, 1.00f);
        if (name.find("red") != std::string::npos) return mce::Color(0.600f, 0.200f, 0.200f, 1.00f);
        if (name.find("white") != std::string::npos) return mce::Color(1.000f, 1.000f, 1.000f, 1.00f);
        if (name.find("light_gray") != std::string::npos) return mce::Color(0.600f, 0.600f, 0.600f, 1.00f);
        if (name.find("gray") != std::string::npos) return mce::Color(0.298f, 0.298f, 0.298f, 1.00f);
        if (name.find("black") != std::string::npos) return mce::Color(0.098f, 0.098f, 0.098f, 1.00f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.400f, 0.298f, 0.200f, 1.00f);
        if (name.find("tinted") != std::string::npos) return mce::Color(0.169f, 0.153f, 0.176f, 1.00f);
        return mce::Color(0.784f, 0.898f, 0.902f, 1.00f);
    }
    if (name.find("path") != std::string::npos || name.find("farmland") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    // [竹板/竹马赛克/竹制品 vs 生竹子]
    if (name.find("bamboo") != std::string::npos) {
        if (name.find("planks") != std::string::npos || name.find("mosaic") != std::string::npos ||
            name.find("stairs") != std::string::npos || name.find("slab") != std::string::npos ||
            name.find("fence") != std::string::npos || name.find("door") != std::string::npos ||
            name.find("trapdoor") != std::string::npos || name.find("sign") != std::string::npos ||
            name.find("button") != std::string::npos || name.find("pressure_plate") != std::string::npos) {
            return mce::Color(0.85f, 0.78f, 0.55f, 1.0f);
        }
        return mce::Color(0.40f, 0.70f, 0.20f, 1.0f);
    }
    // [干海带块] 必须在通用 kelp 规则之前
    if (name.find("dried_kelp_block") != std::string::npos) return mce::Color(0.25f, 0.35f, 0.15f, 1.0f);
    // [枯萎的灌木] 原版基岩版名称为 minecraft:deadbush（无下划线，Java版为 dead_bush）
    // 原版贴图采样真实均值 RGB(108, 79, 41) #6b4e28，必须在通用 bush 规则之前，彻底消除被按生物群系草色误染为绿色的严重失真缺陷
    if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos) return mce::Color(0.421f, 0.309f, 0.159f, 1.0f);
    // [矮枯草丛 / 高枯草丛 (Dry Grass)] 原版不受生物群系草色影响，为固定金黄麦草/枯草色
    // 材质采样：short_dry_grass RGB(187, 159, 108) #bb9e6c，tall_dry_grass RGB(197, 172, 123) #c4ab7a
    // 必须在通用 grass 规则之前，彻底杜绝恶地与沙漠枯草被误染为绿草
    if (name.find("tall_dry_grass") != std::string::npos) return mce::Color(0.771f, 0.674f, 0.482f, 1.0f);
    if (name.find("dry_grass") != std::string::npos) return mce::Color(0.733f, 0.622f, 0.424f, 1.0f);
    // [仙人掌花] 恶地/沙漠新植物，亮粉红色花瓣 RGB(210, 121, 135) #d17887，杜绝被通用 flower 泛化为黄色
    if (name.find("cactus_flower") != std::string::npos) return mce::Color(0.822f, 0.473f, 0.531f, 1.0f);
    // [萤火虫灌木丛] 原版基岩版名称为 minecraft:firefly_bush
    // 由深暗橄榄褐枯枝与温暖金黄发光萤火虫组成，实机采样暖金琥珀色 RGB(148, 128, 56) #948038
    // 必须在通用 bush 规则之前，彻底消除被按生物群系草色误染为绿草导致完全隐形的问题
    if (name.find("firefly_bush") != std::string::npos || name.find("firefly") != std::string::npos) return mce::Color(0.58f, 0.50f, 0.22f, 1.0f);
    // [枯叶堆 / 掉落枯叶 (Leaf Litter)] 原版森林地表落叶覆盖层，基岩版标识为 minecraft:leaf_litter
    // 实机纹理与物品采样暖秋枯叶红褐色 RGB(127, 88, 60) #7f583c
    // 必须在通用 leaf/leaves 树叶规则之前，彻底杜绝被误按树叶染为绿色而在草地上隐形
    if (name.find("leaf_litter") != std::string::npos || name.find("litter") != std::string::npos || name.find("fallen_leaf") != std::string::npos || name.find("fallen_leaves") != std::string::npos) return mce::Color(0.50f, 0.35f, 0.24f, 1.0f);
    // [红灌木 (实验版)]
    if (name.find("red_shrub") != std::string::npos) return mce::Color(0.48f, 0.14f, 0.09f, 1.0f);

    // [荷叶] 必须在通用 water 规则之前（Bedrock 原版名称为 minecraft:waterlily，含子串 "water"）
    // 原版荷叶不受群系染色影响，取实机真实采样深森林绿 RGB(19, 72, 27)
    if (name.find("lily_pad") != std::string::npos || name.find("waterlily") != std::string::npos) {
        return mce::Color(0.08f, 0.35f, 0.12f, 1.0f);
    }
    // [气泡柱]
    if (name.find("bubble_column") != std::string::npos) return waterCol;

    if (name.find("water") != std::string::npos) return waterCol;
    if (name.find("pink_petals") != std::string::npos) return mce::Color(0.95f, 0.68f, 0.78f, 1.0f);

    if (name.find("orange_tulip") != std::string::npos) return mce::Color(0.851f, 0.518f, 0.149f, 1.0f);
    if (name.find("peony") != std::string::npos || name.find("pink_tulip") != std::string::npos) return mce::Color(0.922f, 0.769f, 0.980f, 1.0f);
    if (name.find("dandelion") != std::string::npos || name.find("sunflower") != std::string::npos || name.find("yellow_flower") != std::string::npos) return mce::Color(0.973f, 0.784f, 0.212f, 1.0f);
    if (name.find("rose") != std::string::npos || name.find("poppy") != std::string::npos || name.find("red_flower") != std::string::npos || name.find("red_tulip") != std::string::npos) return mce::Color(0.808f, 0.169f, 0.161f, 1.0f);
    if (name.find("orchid") != std::string::npos || name.find("cornflower") != std::string::npos) return mce::Color(0.325f, 0.427f, 0.875f, 1.0f);
    if (name.find("allium") != std::string::npos || name.find("lilac") != std::string::npos || name.find("syringa") != std::string::npos) return mce::Color(0.745f, 0.455f, 0.753f, 1.0f);
    if (name.find("daisy") != std::string::npos || name.find("bluet") != std::string::npos || name.find("valley") != std::string::npos || name.find("white_tulip") != std::string::npos) return mce::Color(0.929f, 0.929f, 0.929f, 1.0f);
    if (name.find("pitcher") != std::string::npos) {
        if (name.find("crop") != std::string::npos) return mce::Color(0.757f, 0.647f, 0.404f, 1.0f);
        return mce::Color(0.485f, 0.575f, 0.755f, 1.0f);
    }
    if (name.find("torchflower") != std::string::npos) return mce::Color(0.923f, 0.642f, 0.215f, 1.0f);
    // [杜鹃花与盛开杜鹃花树叶] 必须在通用 flower 规则前，杜绝因含 "flowered" 或 "flowering" 被误判为金黄色小花
    if (name.find("azalea") != std::string::npos) {
        if (name.find("flowered") != std::string::npos || name.find("flowering") != std::string::npos) {
            return mce::Color(0.353f, 0.431f, 0.196f, 1.0f);
        }
        return mce::Color(0.345f, 0.443f, 0.169f, 1.0f);
    }

    if (name.find("flower") != std::string::npos || name.find("bloom") != std::string::npos || name.find("blossom") != std::string::npos) return mce::Color(0.92f, 0.85f, 0.25f, 1.0f);

    // [陶瓦与硬化粘土] 恶地/平顶山核心构成方块，原版专属温润暗沉大地色表（采样真实纹理均值）
    // 必须在通用高饱和染色规则前处理，防止恶地各色彩层被纯鲜艳羊毛色劫持
    if (name.find("terracotta") != std::string::npos || name.find("hardened_clay") != std::string::npos) {
        if (name.find("white") != std::string::npos) {
            if (name.find("glazed") != std::string::npos) return mce::Color(0.850f, 0.870f, 0.850f, 1.0f);
            return mce::Color(0.822f, 0.698f, 0.633f, 1.0f);
        }
        if (name.find("orange") != std::string::npos) {
            if (name.find("glazed") != std::string::npos) return mce::Color(0.880f, 0.460f, 0.110f, 1.0f);
            return mce::Color(0.634f, 0.329f, 0.148f, 1.0f);
        }
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

    // [红沙与红砂岩系列] 恶地/风蚀恶地地表与沙柱，原版真实贴图采样：红沙 RGB(191,103,33 #be6621)、红砂岩 RGB(181,98,32 #b5611f)
    // 必须在通用染色规则之前，杜绝恶地在地图上沦为鲜红血泊
    if (name.find("red_sandstone") != std::string::npos) return mce::Color(0.711f, 0.384f, 0.123f, 1.0f);
    if (name.find("red_sand") != std::string::npos) return mce::Color(0.748f, 0.404f, 0.130f, 1.0f);

    // [冰系列] blue_ice 必须在通用 blue_ 规则之前，避免误判为深皇家蓝
    if (name.find("blue_ice") != std::string::npos) return mce::Color(0.45f, 0.65f, 0.95f, 1.0f);
    if (name.find("packed_ice") != std::string::npos) return mce::Color(0.55f, 0.70f, 0.92f, 1.0f);
    if (name.find("ice") != std::string::npos || name.find("frosted") != std::string::npos) return mce::Color(0.44f, 0.57f, 0.80f, 1.0f);

    // [红色下界砖] 必须在通用 red_ 规则之前
    if (name.find("red_nether_brick") != std::string::npos) return mce::Color(0.38f, 0.08f, 0.10f, 1.0f);

    // [蘑菇与下界真菌] 必须在通用染色规则之前，避免 red_mushroom / brown_mushroom 被劫持
    if (name.find("mushroom") != std::string::npos || name.find("fungus") != std::string::npos || name.find("fungi") != std::string::npos) {
        if (name.find("red") != std::string::npos || name.find("crimson") != std::string::npos) return mce::Color(0.85f, 0.20f, 0.20f, 1.0f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
        if (name.find("warped") != std::string::npos) return mce::Color(0.15f, 0.55f, 0.50f, 1.0f);
        return mce::Color(0.80f, 0.70f, 0.60f, 1.0f);
    }

    // [树叶系列] 必须在通用染色规则之前，避免 red_poplar_leaves / yellow_poplar_leaves 等树叶被误染
    if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos) {
        if (name.find("cherry") != std::string::npos) return mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
        if (name.find("mangrove") != std::string::npos) return mce::Color(0.32f, 0.40f, 0.09f, 1.0f);
        if (name.find("pale") != std::string::npos) return mce::Color(0.431f, 0.451f, 0.420f, 1.0f);
        if (name.find("birch") != std::string::npos) return mce::Color(0.263f, 0.341f, 0.173f, 1.0f);
        if (name.find("spruce") != std::string::npos || name.find("pine") != std::string::npos) return mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
        return foliageCol;
    }

    // [通用 16 染色规则 (羊毛/地毯/混凝土/潜影盒/蜡烛/彩色玻璃等)]
    auto hasColor = [&](const char* col) -> bool {
        std::string p1 = std::string(col) + "_";
        if (name.rfind(p1, 0) == 0) return true;
        std::string p2 = std::string(":") + col + "_";
        if (name.find(p2) != std::string::npos) return true;
        std::string p3 = std::string("_") + col + "_";
        if (name.find(p3) != std::string::npos) return true;
        std::string p4 = std::string("_") + col;
        if (name.size() >= p4.size() && name.compare(name.size() - p4.size(), p4.size(), p4) == 0) return true;
        return false;
    };

    if (hasColor("white") || name.find("white_") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.95f, 1.0f);
    if (hasColor("orange") || name.find("orange_") != std::string::npos) return mce::Color(0.85f, 0.50f, 0.20f, 1.0f);
    if (hasColor("magenta") || name.find("magenta_") != std::string::npos) return mce::Color(0.75f, 0.35f, 0.75f, 1.0f);
    if (hasColor("light_blue") || name.find("light_blue_") != std::string::npos) return mce::Color(0.40f, 0.65f, 0.90f, 1.0f);
    if (hasColor("yellow") || name.find("yellow_") != std::string::npos) return mce::Color(0.90f, 0.85f, 0.20f, 1.0f);
    if (hasColor("lime") || name.find("lime_") != std::string::npos) return mce::Color(0.45f, 0.85f, 0.20f, 1.0f);
    if (hasColor("pink") || name.find("pink_") != std::string::npos) return mce::Color(0.90f, 0.55f, 0.70f, 1.0f);
    if (hasColor("light_gray") || hasColor("silver") || name.find("light_gray_") != std::string::npos || name.find("silver_") != std::string::npos) return mce::Color(0.65f, 0.65f, 0.65f, 1.0f);
    if (hasColor("gray") || name.find("gray_") != std::string::npos) return mce::Color(0.40f, 0.40f, 0.40f, 1.0f);
    if (hasColor("cyan") || name.find("cyan_") != std::string::npos) return mce::Color(0.20f, 0.60f, 0.60f, 1.0f);
    if (hasColor("purple") || name.find("purple_") != std::string::npos) return mce::Color(0.50f, 0.25f, 0.60f, 1.0f);
    if (hasColor("blue") || name.find("blue_") != std::string::npos) return mce::Color(0.20f, 0.30f, 0.70f, 1.0f);
    if (hasColor("brown") || name.find("brown_") != std::string::npos) return mce::Color(0.45f, 0.30f, 0.15f, 1.0f);
    if (hasColor("green") || name.find("green_") != std::string::npos) return mce::Color(0.30f, 0.50f, 0.20f, 1.0f);
    if (hasColor("red") || (name.find("red_") != std::string::npos && name.find("weathered_") == std::string::npos && name.find("powered_") == std::string::npos)) return mce::Color(0.75f, 0.20f, 0.20f, 1.0f);
    if (hasColor("black") || name.find("black_") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.15f, 1.0f);

    if (name.find("double_plant") != std::string::npos) {
        if (name.find("syringa") != std::string::npos || name.find("lilac") != std::string::npos) return mce::Color(0.745f, 0.455f, 0.753f, 1.00f);
        if (name.find("paeonia") != std::string::npos || name.find("peony") != std::string::npos) return mce::Color(0.902f, 0.698f, 0.969f, 1.00f);
        if (name.find("rose") != std::string::npos) return mce::Color(0.808f, 0.169f, 0.161f, 1.00f);
        if (name.find("sunflower") != std::string::npos) return mce::Color(0.973f, 0.784f, 0.212f, 1.00f);
        return mce::Color(0.55f, 0.75f, 0.25f, 1.0f);
    }

    if (name.find("warped_wart") != std::string::npos) return mce::Color(0.20f, 0.50f, 0.42f, 1.0f);
    if (name.find("wart") != std::string::npos) return mce::Color(0.65f, 0.10f, 0.10f, 1.0f);
    if (name.find("chorus") != std::string::npos) return mce::Color(0.60f, 0.40f, 0.60f, 1.0f);
    // [下界根/下界苗]
    if (name.find("crimson_roots") != std::string::npos) return mce::Color(0.55f, 0.10f, 0.10f, 1.0f);
    if (name.find("warped_roots") != std::string::npos) return mce::Color(0.15f, 0.40f, 0.35f, 1.0f);
    if (name.find("nether_sprouts") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);

    // [下界木] 必须在 grass 检查之前（"stem" 同时匹配作物茎和下界木，需优先处理下界木）
    if (name.find("crimson_stem") != std::string::npos || name.find("crimson_hyphae") != std::string::npos) return mce::Color(0.45f, 0.18f, 0.18f, 1.0f);
    if (name.find("warped_stem") != std::string::npos || name.find("warped_hyphae") != std::string::npos) return mce::Color(0.15f, 0.40f, 0.38f, 1.0f);
    // [垂泪藤/缠怨藤] 必须在通用 vine 规则之前
    if (name.find("weeping_vines") != std::string::npos) return mce::Color(0.45f, 0.08f, 0.08f, 1.0f);
    if (name.find("twisting_vines") != std::string::npos) return mce::Color(0.15f, 0.50f, 0.45f, 1.0f);
    // [苍白苔藓/苍白垂须] 必须在通用 moss 规则之前
    if (name.find("pale_moss") != std::string::npos || name.find("pale_hanging_moss") != std::string::npos) return mce::Color(0.42f, 0.44f, 0.41f, 1.0f);

    // [仙人掌] 必须在植物通用规则前（仙人掌为固定特征墨绿色，不随生物群系染色）
    if (name.find("cactus") != std::string::npos) return mce::Color(0.33f, 0.52f, 0.18f, 1.0f);

    // [洞穴藤蔓/发光浆果] 必须在通用 vine 规则之前
    if (name.find("cave_vines") != std::string::npos) {
        if (name.find("berries") != std::string::npos || name.find("berry") != std::string::npos) {
            return mce::Color(0.78f, 0.55f, 0.18f, 1.0f);
        }
        return mce::Color(0.35f, 0.40f, 0.15f, 1.0f);
    }

    // [苔藓块与苔藓地毯] 原版不受生物群系染色影响，固定为浓郁温润的苔藓绿 RGB(89, 110, 45)
    if (name.find("moss") != std::string::npos) return mce::Color(0.35f, 0.43f, 0.18f, 1.0f);

    // [甘蔗/芦苇] 必须在植物通用规则前（Bedrock 名称为 reeds，不受群系染色影响，取清爽嫩绿）
    if (name.find("sugar_cane") != std::string::npos || name.find("reeds") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.25f, 1.0f);

    // [藤蔓 (Vine)] 原版贴图为灰度图，随群系树叶 colormap 染色 (foliageCol)
    if (name == "vine" || name == "minecraft:vine") return foliageCol;

    if (name.find("grass") != std::string::npos || name.find("fern") != std::string::npos ||
        name.find("shrub") != std::string::npos || name.find("plant") != std::string::npos || name.find("vine") != std::string::npos ||
        name.find("sapling") != std::string::npos || name.find("propagule") != std::string::npos ||
        name.find("wheat") != std::string::npos || name.find("carrot") != std::string::npos || name.find("potato") != std::string::npos ||
        name.find("beetroot") != std::string::npos || name.find("crop") != std::string::npos || name.find("stem") != std::string::npos ||
        name.find("bush") != std::string::npos || name.find("seagrass") != std::string::npos || name.find("kelp") != std::string::npos ||
        name.find("lichen") != std::string::npos || name.find("seed") != std::string::npos) {
        return grassCol;
    }
    // [灰化土] 表层枯叶深棕偏暗
    if (name.find("podzol") != std::string::npos) return mce::Color(0.38f, 0.27f, 0.14f, 1.0f);
    // [粗制泥土/砂土] 稍偏灰暗深棕
    if (name.find("coarse_dirt") != std::string::npos) return mce::Color(0.47f, 0.34f, 0.23f, 1.0f);
    // [缠根泥土]
    if (name.find("dirt_with_roots") != std::string::npos || (name.find("rooted") != std::string::npos && name.find("dirt") != std::string::npos)) {
        return mce::Color(0.56f, 0.41f, 0.30f, 1.0f);
    }
    // [泥土] 原版贴图均值 RGB(134, 96, 67)，温暖明亮自然大地色，消除原本偏深偏暗问题
    if (name.find("dirt") != std::string::npos) return mce::Color(0.53f, 0.38f, 0.26f, 1.0f);

    if (name.find("pumpkin") != std::string::npos || name.find("melon") != std::string::npos) {
        if (name.find("melon") != std::string::npos) return mce::Color(0.50f, 0.65f, 0.15f, 1.0f);
        return mce::Color(0.90f, 0.45f, 0.05f, 1.0f);
    }

    if (name.find("hay_block") != std::string::npos || name.find("hay_bale") != std::string::npos) return mce::Color(0.78f, 0.68f, 0.18f, 1.0f);
    if (name.find("bone_block") != std::string::npos) return mce::Color(0.88f, 0.86f, 0.78f, 1.0f);
    if (name.find("beehive") != std::string::npos || name.find("bee_nest") != std::string::npos) return mce::Color(0.85f, 0.65f, 0.28f, 1.0f);
    if (name.find("cake") != std::string::npos) return mce::Color(0.92f, 0.85f, 0.78f, 1.0f);

    if (name.find("end_stone") != std::string::npos || name.find("end_brick") != std::string::npos) return mce::Color(0.539f, 0.633f, 0.422f, 1.0f);
    if (name.find("sandstone") != std::string::npos) return mce::Color(0.890f, 0.840f, 0.680f, 1.0f);
    if (name.find("redstone") != std::string::npos) return mce::Color(0.85f, 0.15f, 0.15f, 1.0f);
    if (name.find("glowstone") != std::string::npos) return mce::Color(1.0f, 0.85f, 0.30f, 1.0f);
    if (name.find("lodestone") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);
    if (name.find("end_portal") != std::string::npos) return mce::Color(0.10f, 0.25f, 0.25f, 1.0f);
    // [紫珀方块] 末地城标志性紫色方块（purpur_block/purpur_pillar/purpur_stairs/purpur_slab）
    // 必须在下方通用 stairs/slab 规则（约 L339）之前处理，否则紫珀楼梯/台阶会被误判为木色棕色
    // 实际贴图平均色约 RGB(153,108,172)，与紫水晶(0.58,0.48,0.68) 区分：紫珀更纯净、绿分量更低
    if (name.find("purpur") != std::string::npos) return mce::Color(0.60f, 0.42f, 0.68f, 1.0f);

    if (name.find("netherrack") != std::string::npos) return mce::Color(0.45f, 0.12f, 0.12f, 1.0f);
    if (name.find("nether_wart") != std::string::npos) return mce::Color(0.60f, 0.14f, 0.14f, 1.0f);
    if (name.find("magma") != std::string::npos) return mce::Color(0.60f, 0.20f, 0.08f, 1.0f);
    if (name.find("lava") != std::string::npos) return mce::Color(0.95f, 0.30f, 0.0f, 1.0f);
    // [红色下界砖] 必须在 nether_brick 之前
    if (name.find("red_nether_brick") != std::string::npos) return mce::Color(0.38f, 0.08f, 0.10f, 1.0f);
    if (name.find("nether_brick") != std::string::npos) return mce::Color(0.25f, 0.10f, 0.15f, 1.0f);
    if (name.find("gilded_blackstone") != std::string::npos) return mce::Color(0.22f, 0.18f, 0.12f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.18f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    // [哭泣黑曜石 / 重生锚] 必须在 obsidian 之前
    if (name.find("crying_obsidian") != std::string::npos) return mce::Color(0.20f, 0.08f, 0.28f, 1.0f);
    if (name.find("respawn_anchor") != std::string::npos) return mce::Color(0.25f, 0.12f, 0.32f, 1.0f);
    if (name.find("obsidian") != std::string::npos) return mce::Color(0.06f, 0.04f, 0.09f, 1.0f);
    if (name.find("crimson_nylium") != std::string::npos) return mce::Color(0.55f, 0.15f, 0.15f, 1.0f);
    if (name.find("warped_nylium") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);
    if (name.find("shroomlight") != std::string::npos) return mce::Color(1.0f, 0.60f, 0.20f, 1.0f);
    if (name.find("quartz_ore") != std::string::npos) return mce::Color(0.65f, 0.55f, 0.55f, 1.0f);
    if (name.find("quartz") != std::string::npos) return mce::Color(0.92f, 0.90f, 0.86f, 1.0f);
    if (name.find("nether_gold_ore") != std::string::npos) return mce::Color(0.65f, 0.35f, 0.15f, 1.0f);

    // [杜鹃花丛与杜鹃树叶] 原版不受生物群系着色影响，为固定特征青润绿色，盛开型带花蕾粉紫调
    if (name.find("flowering_azalea") != std::string::npos) return mce::Color(0.48f, 0.46f, 0.32f, 1.0f);
    if (name.find("azalea") != std::string::npos) return mce::Color(0.40f, 0.49f, 0.19f, 1.0f);
    // [孢子花] 繁茂洞穴天花板花卉，标志性洋红色花瓣
    if (name.find("spore_blossom") != std::string::npos || name.find("spore") != std::string::npos) return mce::Color(0.81f, 0.38f, 0.62f, 1.0f);

    if (name.find("snow") != std::string::npos) return mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
    if (name.find("soul_sand") != std::string::npos) return mce::Color(0.330f, 0.250f, 0.200f, 1.0f);
    if (name.find("soul_soil") != std::string::npos) return mce::Color(0.265f, 0.200f, 0.160f, 1.0f);
    if (name.find("sand") != std::string::npos) return mce::Color(0.880f, 0.830f, 0.660f, 1.0f);

    // [羊毛与地毯兜底（处理 Bedrock 无颜色前缀的 minecraft:wool / minecraft:carpet）]
    if (name.find("wool") != std::string::npos || name.find("carpet") != std::string::npos) return mce::Color(0.88f, 0.88f, 0.88f, 1.0f);
    // [混凝土与混凝土粉末兜底（处理 Bedrock 无颜色前缀的 minecraft:concrete / concretePowder）]
    if (name.find("concrete") != std::string::npos) return mce::Color(0.82f, 0.82f, 0.82f, 1.0f);
    // [潜影盒兜底（处理 Bedrock 无前缀 minecraft:shulker_box / undyed_shulker_box）]
    if (name.find("shulker_box") != std::string::npos) return mce::Color(0.58f, 0.40f, 0.60f, 1.0f);
    // [床（处理 Bedrock 无前缀 minecraft:bed）]
    if (name.find("bed") != std::string::npos && name.find("bedrock") == std::string::npos) return mce::Color(0.75f, 0.20f, 0.20f, 1.0f);
    // [蜡烛（暖黄蜡色）]
    if (name.find("candle") != std::string::npos) return mce::Color(0.85f, 0.75f, 0.55f, 1.0f);
    // [青蛙灯与青蛙卵]
    if (name.find("froglight") != std::string::npos) return mce::Color(0.92f, 0.90f, 0.80f, 1.0f);
    if (name.find("frog_spawn") != std::string::npos || name.find("frogspawn") != std::string::npos) return mce::Color(0.45f, 0.40f, 0.35f, 0.7f);

    // [苍白橡木] 灰白色木材，区别于普通橡木的暖棕色
    if (name.find("pale_oak") != std::string::npos) return mce::Color(0.68f, 0.66f, 0.60f, 1.0f);
    // [嘎枝之心] 苍白橡木质地，需在通用 wood 规则前处理（名称含 "wood"）
    if (name.find("creaking_heart") != std::string::npos) return mce::Color(0.55f, 0.52f, 0.48f, 1.0f);
    // [深板岩] 冷调深暗蓝灰岩石（原生贴图均值 RGB(80,80,83)），必须在 wood/stairs/slab 检查之前
    if (name.find("deepslate") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.33f, 1.0f);
    // [海晶石系列]
    if (name.find("prismarine") != std::string::npos) {
        if (name.find("dark") != std::string::npos) return mce::Color(0.20f, 0.35f, 0.30f, 1.0f);
        if (name.find("brick") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.55f, 1.0f);
        return mce::Color(0.45f, 0.65f, 0.50f, 1.0f);
    }
    // [珊瑚系列] Tube=蓝 Brain=粉 Bubble=紫 Fire=红 Horn=黄 Dead=灰
    if (name.find("coral") != std::string::npos) {
        if (name.find("tube") != std::string::npos) return mce::Color(0.20f, 0.35f, 0.75f, 1.0f);
        if (name.find("brain") != std::string::npos) return mce::Color(0.85f, 0.40f, 0.55f, 1.0f);
        if (name.find("bubble") != std::string::npos) return mce::Color(0.65f, 0.30f, 0.70f, 1.0f);
        if (name.find("fire") != std::string::npos) return mce::Color(0.75f, 0.25f, 0.25f, 1.0f);
        if (name.find("horn") != std::string::npos) return mce::Color(0.75f, 0.70f, 0.20f, 1.0f);
        if (name.find("dead") != std::string::npos) return mce::Color(0.70f, 0.70f, 0.70f, 1.0f);
        return mce::Color(0.50f, 0.80f, 0.80f, 1.0f);
    }
    // [工作台与功能容器类方块]
    if (name.find("anvil") != std::string::npos) return mce::Color(0.25f, 0.25f, 0.25f, 1.0f);
    if (name.find("barrel") != std::string::npos) return mce::Color(0.53f, 0.40f, 0.24f, 1.0f);
    if (name.find("blast_furnace") != std::string::npos) return mce::Color(0.32f, 0.32f, 0.32f, 1.0f);
    if (name.find("smoker") != std::string::npos) return mce::Color(0.28f, 0.24f, 0.20f, 1.0f);
    if (name.find("hopper") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.30f, 1.0f);
    if (name.find("lectern") != std::string::npos) return mce::Color(0.65f, 0.48f, 0.25f, 1.0f);
    if (name.find("smithing_table") != std::string::npos) return mce::Color(0.22f, 0.24f, 0.28f, 1.0f);
    if (name.find("loom") != std::string::npos) return mce::Color(0.62f, 0.50f, 0.35f, 1.0f);
    if (name.find("cartography_table") != std::string::npos) return mce::Color(0.58f, 0.46f, 0.32f, 1.0f);
    if (name.find("fletching_table") != std::string::npos) return mce::Color(0.75f, 0.68f, 0.48f, 1.0f);
    if (name.find("crafting_table") != std::string::npos) return mce::Color(0.68f, 0.52f, 0.32f, 1.0f);
    if (name.find("grindstone") != std::string::npos) return mce::Color(0.50f, 0.50f, 0.50f, 1.0f);
    if (name.find("cauldron") != std::string::npos) return mce::Color(0.25f, 0.25f, 0.25f, 1.0f);
    if (name.find("bell") != std::string::npos) return mce::Color(0.92f, 0.78f, 0.22f, 1.0f);
    if (name.find("beacon") != std::string::npos) return mce::Color(0.45f, 0.85f, 0.85f, 1.0f);
    if (name.find("conduit") != std::string::npos) return mce::Color(0.40f, 0.75f, 0.75f, 1.0f);
    if (name.find("enchanting_table") != std::string::npos) return mce::Color(0.35f, 0.15f, 0.18f, 1.0f);
    if (name.find("brewing_stand") != std::string::npos) return mce::Color(0.50f, 0.45f, 0.40f, 1.0f);
    if (name.find("dispenser") != std::string::npos || name.find("dropper") != std::string::npos) return mce::Color(0.48f, 0.48f, 0.48f, 1.0f);
    if (name.find("crafter") != std::string::npos) return mce::Color(0.42f, 0.40f, 0.40f, 1.0f);
    if (name.find("target") != std::string::npos) return mce::Color(0.85f, 0.72f, 0.55f, 1.0f);
    if (name.find("ender_chest") != std::string::npos) return mce::Color(0.12f, 0.18f, 0.18f, 1.0f);
    if (name.find("bookshelf") != std::string::npos) return mce::Color(0.58f, 0.42f, 0.24f, 1.0f);
    if (name.find("composter") != std::string::npos) return mce::Color(0.42f, 0.30f, 0.18f, 1.0f);
    // [熔炉/活塞/侦测器/TNT/音符盒/刷怪笼/传送门/铁轨等功能方块]
    if (name.find("furnace") != std::string::npos) return mce::Color(0.45f, 0.45f, 0.45f, 1.0f);
    if (name.find("piston") != std::string::npos) return mce::Color(0.55f, 0.48f, 0.38f, 1.0f);
    if (name.find("observer") != std::string::npos) return mce::Color(0.42f, 0.42f, 0.42f, 1.0f);
    if (name.find("tnt") != std::string::npos) return mce::Color(0.82f, 0.25f, 0.20f, 1.0f);
    if (name.find("jukebox") != std::string::npos || name.find("noteblock") != std::string::npos) return mce::Color(0.58f, 0.40f, 0.25f, 1.0f);
    if (name.find("spawner") != std::string::npos) return mce::Color(0.22f, 0.26f, 0.30f, 1.0f);
    if (name.find("portal") != std::string::npos) return mce::Color(0.48f, 0.18f, 0.72f, 0.85f);
    if (name.find("end_gateway") != std::string::npos) return mce::Color(0.05f, 0.05f, 0.10f, 1.0f);
    if (name.find("rail") != std::string::npos) return mce::Color(0.55f, 0.48f, 0.38f, 1.0f);
    if (name.find("command_block") != std::string::npos) return mce::Color(0.68f, 0.45f, 0.32f, 1.0f);
    if (name.find("banner") != std::string::npos) return mce::Color(0.70f, 0.60f, 0.40f, 1.0f);
    if (name.find("monster_egg") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);
    if (name.find("netherreactor") != std::string::npos) return mce::Color(0.20f, 0.60f, 0.80f, 1.0f);

    // [照明、营火与装饰]
    if (name.find("soul_campfire") != std::string::npos) return mce::Color(0.32f, 0.80f, 0.81f, 1.0f);
    if (name.find("campfire") != std::string::npos) return mce::Color(0.86f, 0.62f, 0.23f, 1.0f);
    if (name.find("soul_fire") != std::string::npos) return mce::Color(0.20f, 0.65f, 0.75f, 1.0f);
    if (name.find("fire") != std::string::npos) return mce::Color(0.95f, 0.55f, 0.10f, 1.0f);
    if (name.find("redstone_torch") != std::string::npos) {
        if (name.find("unlit") != std::string::npos || name.find("off") != std::string::npos) return mce::Color(0.352f, 0.140f, 0.088f, 1.0f);
        return mce::Color(0.970f, 0.080f, 0.080f, 1.0f);
    }
    if (name.find("underwater_torch") != std::string::npos) return mce::Color(0.78f, 0.25f, 0.82f, 1.0f);
    if (name.find("copper_torch") != std::string::npos) return mce::Color(0.20f, 0.88f, 0.50f, 1.0f);
    if (name.find("soul_lantern") != std::string::npos || name.find("soul_torch") != std::string::npos) return mce::Color(0.25f, 0.85f, 0.88f, 1.0f);
    if (name.find("lantern") != std::string::npos || name.find("torch") != std::string::npos) return mce::Color(0.98f, 0.82f, 0.30f, 1.0f);
    if (name.find("chain") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    if (name.find("scaffolding") != std::string::npos) return mce::Color(0.75f, 0.65f, 0.40f, 1.0f);
    if (name.find("web") != std::string::npos) return mce::Color(0.85f, 0.85f, 0.85f, 0.7f);
    if (name.find("ladder") != std::string::npos) return mce::Color(0.55f, 0.42f, 0.25f, 1.0f);
    if (name.find("lever") != std::string::npos || name.find("repeater") != std::string::npos || name.find("comparator") != std::string::npos) return mce::Color(0.55f, 0.50f, 0.50f, 1.0f);
    if (name.find("daylight_detector") != std::string::npos) return mce::Color(0.55f, 0.50f, 0.45f, 1.0f);
    if (name.find("lightning_rod") != std::string::npos) return mce::Color(0.72f, 0.45f, 0.28f, 1.0f);
    if (name.find("end_rod") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.90f, 1.0f);
    if (name.find("dragon_egg") != std::string::npos) return mce::Color(0.08f, 0.06f, 0.12f, 1.0f);
    if (name.find("decorated_pot") != std::string::npos) return mce::Color(0.62f, 0.38f, 0.25f, 1.0f);
    if (name.find("flower_pot") != std::string::npos) return mce::Color(0.55f, 0.30f, 0.20f, 1.0f);
    // [自然装饰与特殊生物衍生方块]
    if (name.find("sea_pickle") != std::string::npos) return mce::Color(0.42f, 0.60f, 0.22f, 1.0f);
    if (name.find("turtle_egg") != std::string::npos) return mce::Color(0.85f, 0.88f, 0.80f, 1.0f);
    if (name.find("cocoa") != std::string::npos) return mce::Color(0.68f, 0.38f, 0.18f, 1.0f);
    if (name.find("skull") != std::string::npos || (name.find("head") != std::string::npos && name.find("piston") == std::string::npos)) {
        if (name.find("creeper") != std::string::npos) return mce::Color(0.455f, 0.742f, 0.423f, 1.0f);
        if (name.find("zombie") != std::string::npos) return mce::Color(0.257f, 0.419f, 0.190f, 1.0f);
        if (name.find("wither") != std::string::npos) return mce::Color(0.169f, 0.169f, 0.169f, 1.0f);
        if (name.find("piglin") != std::string::npos) return mce::Color(0.937f, 0.700f, 0.506f, 1.0f);
        if (name.find("dragon") != std::string::npos) return mce::Color(0.147f, 0.145f, 0.147f, 1.0f);
        if (name.find("player") != std::string::npos) return mce::Color(0.450f, 0.320f, 0.220f, 1.0f);
        return mce::Color(0.705f, 0.704f, 0.705f, 1.0f); // 骷髅头骨默认骨白
    }
    if (name.find("hanging_roots") != std::string::npos) return mce::Color(0.50f, 0.35f, 0.20f, 1.0f);

    // [矿石] 统一处理所有矿石方块，每种带特征矿物色调（必须在金属纯块/粗矿/金属规则前）
    if (name.find("ore") != std::string::npos) {
        if (name.find("gold") != std::string::npos) return mce::Color(0.55f, 0.45f, 0.20f, 1.0f);
        if (name.find("iron") != std::string::npos) return mce::Color(0.50f, 0.42f, 0.32f, 1.0f);
        if (name.find("copper") != std::string::npos) return mce::Color(0.55f, 0.38f, 0.25f, 1.0f);
        if (name.find("diamond") != std::string::npos) return mce::Color(0.42f, 0.52f, 0.55f, 1.0f);
        if (name.find("emerald") != std::string::npos) return mce::Color(0.30f, 0.52f, 0.35f, 1.0f);
        if (name.find("lapis") != std::string::npos) return mce::Color(0.22f, 0.35f, 0.55f, 1.0f);
        if (name.find("coal") != std::string::npos) return mce::Color(0.25f, 0.25f, 0.25f, 1.0f);
        return mce::Color(0.45f, 0.45f, 0.45f, 1.0f);
    }

    // [粗矿块] 棕色调（必须在 iron/copper 规则前）
    if (name.find("raw_") != std::string::npos) return mce::Color(0.50f, 0.38f, 0.25f, 1.0f);

    // [金属与宝石纯块]
    if (name.find("gold_block") != std::string::npos || name.find("block_of_gold") != std::string::npos) return mce::Color(0.98f, 0.85f, 0.25f, 1.0f);
    if (name.find("diamond_block") != std::string::npos || name.find("block_of_diamond") != std::string::npos) return mce::Color(0.38f, 0.85f, 0.82f, 1.0f);
    if (name.find("emerald_block") != std::string::npos || name.find("block_of_emerald") != std::string::npos) return mce::Color(0.20f, 0.78f, 0.35f, 1.0f);
    if (name.find("lapis_block") != std::string::npos || name.find("block_of_lapis") != std::string::npos) return mce::Color(0.12f, 0.28f, 0.65f, 1.0f);
    if (name.find("netherite_block") != std::string::npos || name.find("block_of_netherite") != std::string::npos) return mce::Color(0.22f, 0.20f, 0.22f, 1.0f);
    if (name.find("coal_block") != std::string::npos || name.find("block_of_coal") != std::string::npos) return mce::Color(0.12f, 0.12f, 0.12f, 1.0f);
    if (name.find("ancient_debris") != std::string::npos) return mce::Color(0.35f, 0.28f, 0.24f, 1.0f);

    // [铜与氧化阶段] 必须涵盖所有变体（cut, chiseled, grate, bulb, door, stairs, slab 等）
    if (name.find("copper") != std::string::npos) {
        if (name.find("oxidized") != std::string::npos) return mce::Color(0.30f, 0.55f, 0.50f, 1.0f);
        if (name.find("weathered") != std::string::npos) return mce::Color(0.35f, 0.50f, 0.40f, 1.0f);
        if (name.find("exposed") != std::string::npos) return mce::Color(0.55f, 0.45f, 0.35f, 1.0f);
        return mce::Color(0.72f, 0.45f, 0.28f, 1.0f);
    }

    // [铁质方块与铁门/铁栏杆]
    if (name.find("iron") != std::string::npos) return mce::Color(0.72f, 0.72f, 0.72f, 1.0f);

    // [木种精准色彩区分] 必须在 generic planks/stairs/slab 前处理
    // 1. 云杉木：完整保留已调校精准参数（村庄屋顶原木 0.212, 0.149, 0.075）
    if (name.find("spruce") != std::string::npos) {
        if (name.find("stripped") != std::string::npos) {
            return mce::Color(0.45f, 0.34f, 0.20f, 1.0f); // 去皮云杉木
        }
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            return mce::Color(0.212f, 0.149f, 0.075f, 1.0f); // 云杉原木/云杉木
        }
        return mce::Color(0.45f, 0.33f, 0.19f, 1.0f); // 云杉木板/楼梯/台阶等
    }

    // 2. 樱花木 (木质浅粉，树皮暗褐)
    if (name.find("cherry") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.89f, 0.70f, 0.68f, 1.0f);
            }
            return mce::Color(0.35f, 0.25f, 0.25f, 1.0f);
        }
        return mce::Color(0.89f, 0.70f, 0.68f, 1.0f);
    }

    // 3. 红树木 (深红褐泥红色)
    if (name.find("mangrove") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.48f, 0.22f, 0.18f, 1.0f);
            }
            return mce::Color(0.35f, 0.20f, 0.15f, 1.0f);
        }
        return mce::Color(0.48f, 0.22f, 0.18f, 1.0f);
    }

    // 4. 金合欢木 (标志性鲜橙色)
    if (name.find("acacia") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.68f, 0.38f, 0.20f, 1.0f);
            }
            return mce::Color(0.40f, 0.38f, 0.35f, 1.0f);
        }
        return mce::Color(0.68f, 0.38f, 0.20f, 1.0f);
    }

    // 5. 白桦木 (白底黑斑树皮，淡米黄木质)
    if (name.find("birch") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.82f, 0.74f, 0.55f, 1.0f);
            }
            return mce::Color(0.82f, 0.82f, 0.80f, 1.0f);
        }
        return mce::Color(0.82f, 0.74f, 0.55f, 1.0f);
    }

    // 6. 深色橡木 (浓郁黑巧深褐色)
    if (name.find("dark_oak") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.28f, 0.18f, 0.10f, 1.0f);
            }
            return mce::Color(0.22f, 0.14f, 0.08f, 1.0f);
        }
        return mce::Color(0.28f, 0.18f, 0.10f, 1.0f);
    }

    // 7. 丛林木 (微红暖木色)
    if (name.find("jungle") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.60f, 0.42f, 0.28f, 1.0f);
            }
            return mce::Color(0.35f, 0.30f, 0.18f, 1.0f);
        }
        return mce::Color(0.60f, 0.42f, 0.28f, 1.0f);
    }

    // 8. 绯红与诡异下界木制品 (门/楼梯/台阶/栅栏等)
    if (name.find("crimson") != std::string::npos) return mce::Color(0.48f, 0.18f, 0.24f, 1.0f);
    if (name.find("warped") != std::string::npos) return mce::Color(0.18f, 0.42f, 0.40f, 1.0f);

    // [泥方块/泥砖/带根红树根] 泥土深褐色（必须在 stone/brick 规则前）
    if (name.find("mud") != std::string::npos) {
        if (name.find("brick") != std::string::npos) return mce::Color(0.35f, 0.28f, 0.20f, 1.0f);
        return mce::Color(0.34f, 0.28f, 0.24f, 1.0f); // 泥巴方块与泥化红树根：深灰泥褐色
    }

    // [滴水石与垂滴叶]（含 "stone" 但应偏棕色，需在 stone 检查前处理）
    if (name.find("dripstone") != std::string::npos) return mce::Color(0.525f, 0.424f, 0.365f, 1.0f);
    if (name.find("dripleaf") != std::string::npos) return mce::Color(0.44f, 0.56f, 0.20f, 1.0f);

    // [石材与砖石变体] 必须在 generic stairs/slab 之前，确保 stone_stairs/brick_stairs 不会被误判为木头
    if (name.find("granite") != std::string::npos) return mce::Color(0.60f, 0.45f, 0.38f, 1.0f);
    if (name.find("diorite") != std::string::npos) return mce::Color(0.80f, 0.80f, 0.80f, 1.0f);
    if (name.find("andesite") != std::string::npos) return mce::Color(0.52f, 0.52f, 0.52f, 1.0f);
    if (name.find("tuff") != std::string::npos) return mce::Color(0.42f, 0.44f, 0.40f, 1.0f);
    if (name.find("brick") != std::string::npos) return mce::Color(0.62f, 0.35f, 0.28f, 1.0f);
    if (name.find("stone") != std::string::npos || name.find("cobble") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);

    // [通用木质与结构兜底]
    if (name.find("planks") != std::string::npos || name.find("oak") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
    if (name.find("wood") != std::string::npos || name.find("log") != std::string::npos || name.find("stem") != std::string::npos ||
        name.find("stairs") != std::string::npos || name.find("slab") != std::string::npos || name.find("fence") != std::string::npos ||
        name.find("door") != std::string::npos || name.find("trapdoor") != std::string::npos || name.find("sign") != std::string::npos ||
        name.find("chest") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);

    // [基岩] 极深灰色
    if (name.find("bedrock") != std::string::npos) return mce::Color(0.18f, 0.18f, 0.18f, 1.0f);

    // [海绵] 淡黄色
    if (name.find("sponge") != std::string::npos) return mce::Color(0.76f, 0.71f, 0.31f, 1.0f);
    // [黏液块] 浅绿色半透明
    if (name.find("slime") != std::string::npos) return mce::Color(0.49f, 0.74f, 0.35f, 1.0f);
    // [蜂蜜系列] 金黄/橙黄色
    if (name.find("honeycomb") != std::string::npos) return mce::Color(0.81f, 0.53f, 0.15f, 1.0f);
    if (name.find("honey_block") != std::string::npos) return mce::Color(0.89f, 0.58f, 0.12f, 1.0f);
    // [幽匿系列] 标志性深暗青翠/荧光青蓝色，告别纯黑死色
    if (name.find("sculk_catalyst") != std::string::npos) return mce::Color(0.20f, 0.32f, 0.35f, 1.0f);
    if (name.find("sculk_shrieker") != std::string::npos) return mce::Color(0.48f, 0.45f, 0.38f, 1.0f);
    if (name.find("calibrated_sculk_sensor") != std::string::npos) return mce::Color(0.35f, 0.30f, 0.50f, 1.0f);
    if (name.find("sculk_sensor") != std::string::npos) return mce::Color(0.07f, 0.38f, 0.42f, 1.0f);
    if (name.find("sculk_vein") != std::string::npos) return mce::Color(0.08f, 0.32f, 0.36f, 1.0f);
    if (name.find("sculk") != std::string::npos) return mce::Color(0.07f, 0.28f, 0.32f, 1.0f);
    // [树脂系列] 琥珀橙黄色
    if (name.find("resin") != std::string::npos) return mce::Color(0.63f, 0.39f, 0.12f, 1.0f);
    // [沉重核心] 深灰蓝色（1.21 重锤相关）
    if (name.find("heavy_core") != std::string::npos) return mce::Color(0.24f, 0.25f, 0.29f, 1.0f);
    // [试炼大厅 1.21] 刷怪笼、自动合成器、宝库
    if (name.find("trial_spawner") != std::string::npos) return mce::Color(0.63f, 0.39f, 0.24f, 1.0f);
    if (name.find("crafter") != std::string::npos) return mce::Color(0.42f, 0.40f, 0.40f, 1.0f);
    if (name.find("vault") != std::string::npos) return mce::Color(0.48f, 0.40f, 0.35f, 1.0f);
    if (name.find("jigsaw") != std::string::npos) return mce::Color(0.255f, 0.220f, 0.260f, 1.0f);
    if (name.find("structure_block") != std::string::npos) return mce::Color(0.267f, 0.224f, 0.271f, 1.0f);
    // [嗅探兽蛋]
    if (name.find("sniffer_egg") != std::string::npos) return mce::Color(0.78f, 0.38f, 0.22f, 1.0f);
    if (name.find("redstone_torch") != std::string::npos) {
        if (name.find("unlit") != std::string::npos || name.find("off") != std::string::npos) return mce::Color(0.352f, 0.140f, 0.088f, 1.0f);
        return mce::Color(0.970f, 0.080f, 0.080f, 1.0f);
    }
    if (name.find("underwater_torch") != std::string::npos) return mce::Color(0.78f, 0.25f, 0.82f, 1.0f);
    if (name.find("copper_torch") != std::string::npos) return mce::Color(0.20f, 0.88f, 0.50f, 1.0f);
    if (name.find("soul_torch") != std::string::npos || name.find("soul_lantern") != std::string::npos) return mce::Color(0.25f, 0.85f, 0.88f, 1.0f);
    if (name.find("torch") != std::string::npos || name.find("lantern") != std::string::npos) return mce::Color(0.98f, 0.82f, 0.30f, 1.0f);
    // [教育版与实验性方块]
    if (name.find("cinnabar") != std::string::npos) return mce::Color(0.68f, 0.22f, 0.20f, 1.0f);
    if (name.find("sulfur") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.22f, 1.0f);
    if (name.find("dried_ghast") != std::string::npos) return mce::Color(0.90f, 0.90f, 0.90f, 1.0f);
    if (name.find("shelf") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
    if (name.find("chemistry_table") != std::string::npos || name.find("lab_table") != std::string::npos ||
        name.find("compound_creator") != std::string::npos || name.find("chemical_heat") != std::string::npos) {
        return mce::Color(0.70f, 0.70f, 0.72f, 1.0f);
    }
    // [红树根] 深棕色
    if (name.find("mangrove_roots") != std::string::npos) return mce::Color(0.43f, 0.27f, 0.16f, 1.0f);
    // [菌丝体] 灰紫色（蘑菇岛地表）
    if (name.find("mycelium") != std::string::npos) return mce::Color(0.48f, 0.42f, 0.42f, 1.0f);
    // [紫水晶] 淡紫色
    if (name.find("amethyst") != std::string::npos) return mce::Color(0.58f, 0.48f, 0.68f, 1.0f);
    // [方解石] 原版为平滑的灰白/米白色方块
    if (name.find("calcite") != std::string::npos) return mce::Color(0.86f, 0.86f, 0.82f, 1.0f);

    // [黏土] 标志性浅灰蓝黏土，自然柔和不刺眼
    if (name.find("clay") != std::string::npos) return mce::Color(0.58f, 0.61f, 0.66f, 1.0f);

    // [墙、沙砾兜底]
    if (name.find("wall") != std::string::npos || name.find("gravel") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);

    LogColorMissInternal("BlockMiss", name);

    // [步骤2安全兜底] 彻底终结伪随机哈希杂色，未知方块以自然中性岩石灰融入地貌
    return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);
}

} // namespace BlockColorMap
