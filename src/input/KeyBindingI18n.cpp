#include "mc/locale/I18nImpl.h"
#include "mc/locale/Localization.h"

#include "ll/api/i18n/I18n.h"
#include "ll/api/memory/Hook.h"

// LeviLamina 的 ll::i18n 与世界(游戏)I18n 相互独立：原版「设置 → 控制 → 键盘和鼠标」
// 界面只读取游戏的 mc::I18nImpl::get，因此这里挂钩该函数，把 key.ChiyanMap.* 重定向到
// ll::i18n（数据来自 lang/*.json），让原版界面也能显示本地化名称。其余按键照常走原版逻辑。
namespace {

constexpr char const kChiyanKeyPrefix[] = "key.ChiyanMap.";

using I18nImplGet2 = std::string (::I18nImpl::*)(
    std::string const&,
    std::shared_ptr<::Localization const> const);
using I18nImplGet3 = std::string (::I18nImpl::*)(
    std::string const&,
    std::vector<std::string> const&,
    std::shared_ptr<::Localization const> const);

[[nodiscard]] std::string_view LookupChiyanKey(std::string const& key) {
    if (key.starts_with(kChiyanKeyPrefix)) {
        return ll::i18n::getInstance().get(key, {});
    }
    return {};
}

} // namespace

// get(langString, locale)
LL_AUTO_TYPE_INSTANCE_HOOK(
    ChiyanI18nGetHook,
    ::ll::memory::HookPriority::Normal,
    ::I18nImpl,
    static_cast<I18nImplGet2>(&::I18nImpl::$get),
    std::string,
    std::string const&                          langString,
    std::shared_ptr<::Localization const> const locale
) {
    auto const tr = LookupChiyanKey(langString);
    if (!tr.empty()) return std::string(tr);
    return origin(langString, locale);
}

// get(id, params, locale)
LL_AUTO_TYPE_INSTANCE_HOOK(
    ChiyanI18nGetWithParamsHook,
    ::ll::memory::HookPriority::Normal,
    ::I18nImpl,
    static_cast<I18nImplGet3>(&::I18nImpl::$get),
    std::string,
    std::string const&                          id,
    std::vector<std::string> const&             params,
    std::shared_ptr<::Localization const> const locale
) {
    auto const tr = LookupChiyanKey(id);
    if (!tr.empty()) return std::string(tr);
    return origin(id, params, locale);
}
