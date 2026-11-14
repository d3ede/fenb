#pragma once
#include <cstddef>

enum class Language { English, Russian, Spanish, Portuguese, Count };

inline constexpr std::size_t LanguageCount = static_cast<std::size_t>(Language::Count);
// Names remain in their own language so the selector is recognizable in every locale.
inline constexpr const char *LanguageNames[] = {"English", "Русский", "Español", "Português (Brasil)"};
inline constexpr const char *LanguageCodes[] = {"en", "ru", "es", "pt"};
static_assert(sizeof(LanguageNames) / sizeof(LanguageNames[0]) == LanguageCount);
static_assert(sizeof(LanguageCodes) / sizeof(LanguageCodes[0]) == LanguageCount);

inline constexpr std::size_t LanguageIndex(Language language) {
    auto index = static_cast<std::size_t>(language);
    return index < LanguageCount ? index : 0;
}

enum class UiText {
    None,
#define UI_TEXT(id, en, ru, es, pt) id,
#define UI_DIAGNOSTIC_TEXT(id, en, ru, es, pt) UI_TEXT(id, en, ru, es, pt)
#include "localization.inc"
#undef UI_DIAGNOSTIC_TEXT
#undef UI_TEXT
    Count
};

namespace Localization {
inline constexpr const char *Strings[][LanguageCount] = {
    {"", "", "", ""},
#define UI_TEXT(id, en, ru, es, pt) {en, ru, es, pt},
#ifdef FENB_DIAGNOSTICS
#define UI_DIAGNOSTIC_TEXT(id, en, ru, es, pt) UI_TEXT(id, en, ru, es, pt)
#else
// Keep IDs identical in both builds without shipping diagnostic labels.
#define UI_DIAGNOSTIC_TEXT(id, en, ru, es, pt) UI_TEXT(id, "", "", "", "")
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
    return Localization::Strings[index][LanguageIndex(language)];
}
