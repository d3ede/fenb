#pragma once
#include <cstddef>

enum class Language { English, Russian };

enum class UiText {
    None,
#define UI_TEXT(id, en, ru) id,
#define UI_DIAGNOSTIC_TEXT(id, en, ru) UI_TEXT(id, en, ru)
#include "localization.inc"
#undef UI_DIAGNOSTIC_TEXT
#undef UI_TEXT
    Count
};

namespace Localization {
inline constexpr const char *Strings[][2] = {
    {"", ""},
#define UI_TEXT(id, en, ru) {en, ru},
#ifdef FENB_DIAGNOSTICS
#define UI_DIAGNOSTIC_TEXT(id, en, ru) UI_TEXT(id, en, ru)
#else
// Keep IDs identical in both builds without shipping diagnostic labels.
#define UI_DIAGNOSTIC_TEXT(id, en, ru) UI_TEXT(id, "", "")
#endif
#include "localization.inc"
#undef UI_DIAGNOSTIC_TEXT
#undef UI_TEXT
};
static_assert(sizeof(Strings) / sizeof(Strings[0]) == static_cast<std::size_t>(UiText::Count));
} // namespace Localization

inline constexpr const char *Translate(Language language, UiText text) {
    auto index = static_cast<std::size_t>(text);
    if (index >= static_cast<std::size_t>(UiText::Count))
        return "";
    return Localization::Strings[index][language == Language::Russian ? 1 : 0];
}
