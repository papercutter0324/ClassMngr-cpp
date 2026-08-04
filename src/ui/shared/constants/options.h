#pragma once

enum class SaveMode
{
    Automatic,
    Manual
};

enum class Theme
{
    Dark,
    Light
};

enum class FontSize
{
    Small = -2,
    Normal = 0,
    Large = 2,
    ExtraLarge = 4
};

constexpr int fontSizeOffset(
    FontSize fontSize
    )
{
    return static_cast<int>(fontSize);
}

constexpr FontSize fontSizeFromStoredValue(
    int value
    )
{
    switch (value)
    {
    case fontSizeOffset(FontSize::Small):
        return FontSize::Small;

    case fontSizeOffset(FontSize::Large):
        return FontSize::Large;

    case fontSizeOffset(FontSize::ExtraLarge):
        return FontSize::ExtraLarge;

    case fontSizeOffset(FontSize::Normal):
    default:
        return FontSize::Normal;
    }
}

enum class Language
{
    SystemDefault = 0,
    English = 1,
    // Keep Korean's stored value stable. Values 2-4 were the old
    // per-region English options and are migrated to English on startup.
    Korean = 5
};

enum class DocumentPageSpacing
{
    None = 0,
    Small = 1,
    Medium = 2,
    Large = 3
};

enum class DocumentViewerBackground
{
    Default = 0,
    White = 1,
    Black = 2
};

enum class AiCommentProvider
{
    ChatGPT = 0,
    Gemini = 1,
    Claude = 2,
    MicrosoftCopilot = 3,
    CustomWebsite = 4
};

enum class AiCommentVoice
{
    DirectToStudent = 0,
    ThirdPerson = 1
};

inline constexpr int DocumentPageSpacingSmallPixels = 8;

constexpr int documentPageSpacingPixels(
    DocumentPageSpacing spacing
    )
{
    switch (spacing)
    {
    case DocumentPageSpacing::None:
        return 0;

    case DocumentPageSpacing::Medium:
        return DocumentPageSpacingSmallPixels * 2;

    case DocumentPageSpacing::Large:
        return DocumentPageSpacingSmallPixels * 4;

    case DocumentPageSpacing::Small:
    default:
        return DocumentPageSpacingSmallPixels;
    }
}
