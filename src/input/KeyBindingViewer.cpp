#include "input/KeyBindingViewer.h"

#include <string>
#include <vector>

#include "input/ChiyanKeyRegistryInfo.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/form/SimpleForm.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/client/player/LocalPlayer.h"

namespace ChiyanMap::Input {

namespace {

// 占位键码（未绑定真实按键的自定义动作使用），界面上显示为「未绑定」。
bool IsPlaceholderKey(int vk) {
    static constexpr int ranges[][2] = {
        {0x88, 0x8F}, {0x92, 0x9F}, {0xB8, 0xB9},
        {0xC1, 0xC2}, {0xE7, 0xE8}, {0xF6, 0xF9},
    };
    for (auto const& r : ranges) {
        if (vk >= r[0] && vk <= r[1]) return true;
    }
    return false;
}

std::string KeyCodeToName(int vk) {
    switch (vk) {
        case 0x08: return "Backspace";
        case 0x09: return "Tab";
        case 0x0D: return "Enter";
        case 0x10: return "Shift";
        case 0x11: return "Ctrl";
        case 0x12: return "Alt";
        case 0x13: return "Pause";
        case 0x1B: return "Esc";
        case 0x20: return "Space";
        case 0x21: return "PageUp";
        case 0x22: return "PageDown";
        case 0x23: return "End";
        case 0x24: return "Home";
        case 0x25: return "←";
        case 0x26: return "↑";
        case 0x27: return "→";
        case 0x28: return "↓";
        case 0x2D: return "Insert";
        case 0x2E: return "Delete";
    }
    if (vk >= 0x70 && vk <= 0x87) return "F" + std::to_string(vk - 0x6F); // F1..F24
    if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
    if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
    if (vk >= 0x60 && vk <= 0x69) return "Num" + std::to_string(vk - 0x60);
    return "0x" + std::to_string(vk);
}

std::string FormatKeyCodes(std::vector<int> const& codes) {
    if (codes.empty()) return "未绑定";
    std::string out;
    for (size_t i = 0; i < codes.size(); ++i) {
        int vk = codes[i];
        if (IsPlaceholderKey(vk)) return "未绑定";
        if (i) out += " + ";
        out += KeyCodeToName(vk);
    }
    return out;
}

} // namespace

void ShowKeybindingForm(Player& player) {
    ll::form::SimpleForm form("赤岩地图 · 快捷键", "");
    form.appendHeader("以下均为「赤岩地图」模组注册的自定义按键");
    form.appendDivider();
    if (gChiyanRegisteredKeys.empty()) {
        form.appendLabel("§7（暂无已注册的快捷键）");
    }
    for (auto const& k : gChiyanRegisteredKeys) {
        std::string name = std::string(ll::i18n::getInstance().get(k.actionId, {}));
        if (name.empty()) name = k.registryName;
        std::string const key = FormatKeyCodes(k.handle->getKeyCodes());
        form.appendLabel(fmt::format("§l{}§r\n§7当前按键：{}", name, key));
    }
    form.sendTo(player);
}

void RegisterKeybindingViewerCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(true);
    auto& cmd = registrar.getOrCreateCommand(
        "chiyanmap",
        "赤岩地图：查看模组自定义快捷键列表",
        CommandPermissionLevel::Any
    );
    cmd.overload().execute([](CommandOrigin const& /*origin*/, CommandOutput& output) {
        auto ci = ll::service::getClientInstance();
        if (!ci) {
            output.error("无法获取客户端实例");
            return;
        }
        LocalPlayer* lp = ci->getLocalPlayer();
        if (!lp) {
            output.error("请先进入游戏世界后再使用此命令");
            return;
        }
        ShowKeybindingForm(*lp);
    });
}

} // namespace ChiyanMap::Input
