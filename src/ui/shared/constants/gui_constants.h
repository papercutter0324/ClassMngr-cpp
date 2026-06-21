#pragma once

namespace UiConstants
{
// =====================================================
// Main Window
// =====================================================

namespace MainWindow
{
inline constexpr int SidebarMinWidth = 150;
inline constexpr int SidebarMaxWidth = 400;
inline constexpr int SidebarFrameWidth = 2;
inline constexpr int SidebarFrameRadius = 8;
inline constexpr int PagesMinWidth   = 600;
}



// =====================================================
// Page Layout
// =====================================================

namespace Pages
{
inline constexpr int Margin               = 24;
inline constexpr int Spacing              = 10;
inline constexpr int MajorSectionSpacing  = 48;
inline constexpr int TitleFontSize        = 24;
inline constexpr int SubtitleFontSize     = 11;
inline constexpr int SectionTitleFontSize = 20;
inline constexpr int HeaderMargin         = 0;
inline constexpr int HeaderSpacing        = 2;
inline constexpr int HeaderContentSpacing = 24;
}



// =====================================================
// Cards
// =====================================================

namespace Cards
{
inline constexpr int Margin  = 20;
inline constexpr int Spacing = 16;
}



// =====================================================
// Forms
// =====================================================

namespace Forms
{
inline constexpr int HorizontalSpacing = 16;
inline constexpr int VerticalSpacing   = 4;

inline constexpr int LabelIndent = 4;

inline constexpr int FieldMinimumWidth = 190;
inline constexpr int FieldMaximumWidth = FieldMinimumWidth * 2;
}



// =====================================================
// Class Info Page
// =====================================================

namespace ClassInfo
{
inline constexpr int MinimumInterItemSpacing = 16;
inline constexpr int TextWidthPadding        = 10;

namespace Page
{
inline constexpr int ContentMargin  = 24;
inline constexpr int ContentSpacing = 20;
}

namespace SectionCard
{
inline constexpr int Margin  = 20;
inline constexpr int Spacing = 16;
}

namespace Form
{
inline constexpr int HorizontalSpacing = MinimumInterItemSpacing;
inline constexpr int VerticalSpacing   = 4;
inline constexpr int LabelIndent       = 4;

inline constexpr int GroupSpacerHeight = 12;
}

namespace Teacher
{
inline constexpr int ColumnStretch       = 0;
inline constexpr int FillerColumnStretch = 1;
inline constexpr int FieldMinWidth       = 190;
inline constexpr int FieldMaxWidth       = FieldMinWidth * 2;
}

namespace Details
{
inline constexpr int ColorPreviewWidth         = 40;
inline constexpr int ColorPreviewHeight        = 24;
inline constexpr int ColorPreviewButtonSpacing = 5;
inline constexpr int ColorButtonExtraWidth     = 10;

inline constexpr int GradeMaxWidth             = 95;
inline constexpr int StudentCountMaxWidth      = 110;
inline constexpr int LevelComboExtraWidth      = 10;
inline constexpr int EssayBookWidthReduction   = 30;

inline constexpr int ColorColumnStretch        = 1;
inline constexpr int GradeColumnStretch        = 1;
inline constexpr int LevelColumnStretch        = 1;
inline constexpr int StudentCountColumnStretch = 0;
inline constexpr int ReadingBookColumnStretch  = 2;
inline constexpr int EssayBookColumnStretch    = 2;
inline constexpr int FieldColumnStretch        = 0;
inline constexpr int FillerColumnStretch       = 1;
}

namespace Schedule
{
inline constexpr int HorizontalSpacing = MinimumInterItemSpacing;
inline constexpr int VerticalSpacing   = 4;
inline constexpr int SubtitleFontSize  = 12;

inline constexpr int DayColumnStretch       = 1;
inline constexpr int StartTimeColumnStretch = 0;
inline constexpr int EndTimeColumnStretch   = 0;
inline constexpr int RemoveColumnStretch    = 0;
inline constexpr int FillerColumnStretch    = 1;

inline constexpr int RowColumnSpan       = 5;
inline constexpr int AddButtonFixedWidth = 200;
}

namespace TimeRow
{
inline constexpr int DayComboMaxWidth = 160;

inline constexpr int StartComboWidth       = 70;
inline constexpr int MinuteComboExtraWidth = 10;
inline constexpr int EndComboWidth         = 120;
inline constexpr int RemoveButtonWidth     = 90;

inline constexpr int StartLayoutSpacing = 8;
inline constexpr int HorizontalSpacing  = MinimumInterItemSpacing;
inline constexpr int VerticalSpacing    = 0;

inline constexpr int DayColumnStretch    = 0;
inline constexpr int StartColumnStretch  = 0;
inline constexpr int EndColumnStretch    = 0;
inline constexpr int RemoveColumnStretch = 0;
inline constexpr int FillerColumnStretch = 1;
}
}



// =====================================================
// Editors
// =====================================================

namespace Editors
{
inline constexpr int NotesMinimumHeight = 180;
}
}
