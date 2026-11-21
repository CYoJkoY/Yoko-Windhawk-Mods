#pragma once

// Compile-time canvas interface strings and language resolution.
//
// The string table order is fixed: it mirrors LanguageId, the Windhawk setting
// options, and the native localization regressions.

#include "platform.h"

#include <array>
#include <cstdint>
#include <string_view>

enum class LanguageId : uint8_t {
    English,
    French,
    SimplifiedChinese,
    Japanese,
    Korean,
    TraditionalChinese,
    Russian,
    Polish,
    Spanish,
    Portuguese,
    German,
    Turkish,
    Italian,
    Count,
};

enum class CanvasTextId : uint8_t {
    MinimizedBadge,
    OtherDesktopBadge,
    PreviewUnavailableBadge,
    EmptyCanvas,
    PinTooltip,
    PinActiveTooltip,
    CloseTooltip,
    GroupTooltip,
    UngroupTooltip,
    Count,
};

struct CanvasLocaleEntry {
    std::wstring_view texts[static_cast<size_t>(LanguageId::Count)];
};

inline constexpr std::array<CanvasLocaleEntry,
                            static_cast<size_t>(CanvasTextId::Count)>
    kCanvasStrings = {{
        {{L"MINIMIZED", L"RÉDUIT", L"已最小化", L"最小化", L"최소화",
          L"已最小化", L"СВЁРНУТО", L"ZMINIMALIZOWANE", L"MINIMIZADO",
          L"MINIMIZADO", L"MINIMIERT", L"KÜÇÜLTÜLDÜ", L"RIDOTTO"}},
        {{L"OTHER DESKTOP", L"AUTRE BUREAU", L"其他桌面", L"別のデスクトップ",
          L"다른 데스크톱", L"其他桌面", L"ДРУГОЙ РАБОЧИЙ СТОЛ",
          L"INNY PULPIT", L"OTRO ESCRITORIO", L"OUTRA ÁREA DE TRABALHO",
          L"ANDERER DESKTOP", L"BAŞKA MASAÜSTÜ", L"ALTRO DESKTOP"}},
        {{L"PREVIEW UNAVAILABLE", L"APERÇU INDISPONIBLE", L"预览不可用",
          L"プレビュー利用不可", L"미리 보기 사용 불가", L"預覽無法使用",
          L"ПРЕДПРОСМОТР НЕДОСТУПЕН", L"PODGLĄD NIEDOSTĘPNY",
          L"VISTA PREVIA NO DISPONIBLE", L"PRÉ-VISUALIZAÇÃO INDISPONÍVEL",
          L"VORSCHAU NICHT VERFÜGBAR", L"ÖNİZLEME KULLANILAMIYOR",
          L"ANTEPRIMA NON DISPONIBILE"}},
        {{L"No eligible windows", L"Aucune fenêtre éligible", L"没有可用窗口",
          L"対象のウィンドウがありません", L"대상 창이 없습니다",
          L"沒有可用視窗", L"Нет подходящих окон", L"Brak kwalifikujących okien",
          L"No hay ventanas aptas", L"Não há janelas elegíveis",
          L"Keine geeigneten Fenster", L"Uygun pencere yok",
          L"Nessuna finestra idonea"}},
        {{L"Pin to top", L"Épingler", L"置顶", L"最前面に固定",
          L"맨 위에 고정", L"置頂", L"Закрепить", L"Przypnij", L"Fijar arriba",
          L"Fixar no topo", L"Anheften", L"Üste sabitle", L"Fissa in alto"}},
        {{L"Unpin", L"Détacher", L"取消置顶", L"固定を解除", L"고정 해제",
          L"取消置頂", L"Открепить", L"Odepnij", L"Desfijar", L"Desafixar",
          L"Lösen", L"Sabitlemeyi kaldır", L"Rimuovi fissaggio"}},
        {{L"Close window", L"Fermer la fenêtre", L"关闭窗口",
          L"ウィンドウを閉じる", L"창 닫기", L"關閉視窗", L"Закрыть окно",
          L"Zamknij okno", L"Cerrar ventana", L"Fechar janela",
          L"Fenster schließen", L"Pencereyi kapat", L"Chiudi finestra"}},
        {{L"Group windows", L"Regrouper", L"分组", L"グループ化", L"그룹화",
          L"分組", L"Сгруппировать", L"Grupuj", L"Agrupar", L"Agrupar",
          L"Gruppieren", L"Grupla", L"Raggruppa"}},
        {{L"Ungroup", L"Dissocier", L"取消分组", L"グループ解除", L"그룹 해제",
          L"取消分組", L"Разгруппировать", L"Rozgrupuj", L"Desagrupar",
          L"Desagrupar", L"Gruppierung aufheben", L"Gruplamayı kaldır",
          L"Separa"}},
    }};

// Setting key -> language. Keys are stable identifiers; display names live in
// the Windhawk metadata so translators only touch one place.
struct LanguageOption {
    const wchar_t* code;
    LanguageId language;
};

inline constexpr LanguageOption kLanguageOptions[] = {
    {L"en", LanguageId::English},
    {L"fr", LanguageId::French},
    {L"zh", LanguageId::SimplifiedChinese},
    {L"zh_TW", LanguageId::TraditionalChinese},
    {L"ja", LanguageId::Japanese},
    {L"ko", LanguageId::Korean},
    {L"ru", LanguageId::Russian},
    {L"pl", LanguageId::Polish},
    {L"es", LanguageId::Spanish},
    {L"pt", LanguageId::Portuguese},
    {L"de", LanguageId::German},
    {L"tr", LanguageId::Turkish},
    {L"it", LanguageId::Italian},
};

inline constexpr wchar_t kAutoLanguageCode[] = L"auto";

// System UI language -> canvas language, in addition to the Chinese sublanguage
// rule that needs the sublanguage identifier.
struct SystemLanguageRule {
    WORD primary;
    LanguageId language;
};

inline constexpr SystemLanguageRule kSystemLanguageRules[] = {
    {LANG_FRENCH, LanguageId::French},
    {LANG_JAPANESE, LanguageId::Japanese},
    {LANG_KOREAN, LanguageId::Korean},
    {LANG_RUSSIAN, LanguageId::Russian},
    {LANG_POLISH, LanguageId::Polish},
    {LANG_SPANISH, LanguageId::Spanish},
    {LANG_PORTUGUESE, LanguageId::Portuguese},
    {LANG_GERMAN, LanguageId::German},
    {LANG_TURKISH, LanguageId::Turkish},
    {LANG_ITALIAN, LanguageId::Italian},
};

[[nodiscard]] inline const wchar_t* CanvasText(CanvasTextId textId,
                                               LanguageId language) {
    const auto entryIndex = static_cast<size_t>(textId);
    const auto languageIndex = static_cast<size_t>(language);
    if (entryIndex >= static_cast<size_t>(CanvasTextId::Count) ||
        languageIndex >= static_cast<size_t>(LanguageId::Count)) {
        return L"";
    }
    return kCanvasStrings[entryIndex].texts[languageIndex].data();
}

[[nodiscard]] inline size_t CanvasTextLength(CanvasTextId textId,
                                             LanguageId language) {
    const auto entryIndex = static_cast<size_t>(textId);
    const auto languageIndex = static_cast<size_t>(language);
    if (entryIndex >= static_cast<size_t>(CanvasTextId::Count) ||
        languageIndex >= static_cast<size_t>(LanguageId::Count)) {
        return 0;
    }
    return kCanvasStrings[entryIndex].texts[languageIndex].size();
}

// Every Chinese sublanguage listed here uses Traditional characters: Taiwan,
// Hong Kong and Macau. Simplified Chinese is the fallback for the rest.
[[nodiscard]] inline bool IsTraditionalChineseSubLanguage(WORD subLanguage) {
    return subLanguage == SUBLANG_CHINESE_TRADITIONAL ||
           subLanguage == SUBLANG_CHINESE_HONGKONG ||
           subLanguage == SUBLANG_CHINESE_MACAU;
}

[[nodiscard]] inline LanguageId DetectSystemLanguage() {
    const LANGID identifier = GetUserDefaultUILanguage();
    const WORD primary = PRIMARYLANGID(identifier);
    if (primary == LANG_CHINESE) {
        return IsTraditionalChineseSubLanguage(SUBLANGID(identifier))
                   ? LanguageId::TraditionalChinese
                   : LanguageId::SimplifiedChinese;
    }
    for (const SystemLanguageRule& rule : kSystemLanguageRules) {
        if (rule.primary == primary) {
            return rule.language;
        }
    }
    return LanguageId::English;
}

// Matches an explicit setting key. "auto" is not a language and is handled by
// ResolveLanguageSetting instead.
[[nodiscard]] inline bool TryParseLanguageCode(const wchar_t* code,
                                               LanguageId& language) {
    if (code == nullptr) {
        return false;
    }
    for (const LanguageOption& option : kLanguageOptions) {
        if (_wcsicmp(option.code, code) == 0) {
            language = option.language;
            return true;
        }
    }
    return false;
}

// Resolves the stored setting value. "auto" and a missing value follow the
// system UI language; an unrecognized key falls back to English so a corrupted
// setting never leaves the canvas without text.
[[nodiscard]] inline LanguageId ResolveLanguageSetting(const wchar_t* code) {
    if (code == nullptr || _wcsicmp(code, kAutoLanguageCode) == 0) {
        return DetectSystemLanguage();
    }
    LanguageId language = LanguageId::English;
    if (!TryParseLanguageCode(code, language)) {
        return LanguageId::English;
    }
    return language;
}
