#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QStringList>

class FontManagerTests;
class QLabel;
class QWidget;


// =========================================================
// Font Manager
// =========================================================

class FontManager
{
public:
    static constexpr int stdEnglishFont = 14;
    // Pretendard's point size is intentionally one point larger than the
    // corresponding Inter size.  Keep this relationship centralized so
    // per-script rendering stays consistent at every user font preference.
    static constexpr int KoreanPointSizeDelta = 1;

    static constexpr int stdKoreanFont =
        stdEnglishFont + KoreanPointSizeDelta;

    static constexpr int koreanPointSizeForEnglish(
        int englishPointSize
        )
    {
        return englishPointSize + KoreanPointSizeDelta;
    }

    // =====================================================
    // Setup
    // =====================================================

    static void loadFonts();

    static void applyGlobalFont(
        QApplication& app,
        const QString& localeName = QString()
        );

    static void applyFontSize(
        QApplication& app,
        const QString& localeName,
        int offset
        );



    // =====================================================
    // Fonts
    // =====================================================

    static QFont getUiFont(
        int size = -1,
        int weight = QFont::Normal,
        bool italic = false
        );

    static QFont getKoreanFont(
        int size = -1,
        int weight = QFont::Normal,
        bool italic = false
        );

    // Builds a Pretendard font from an already-resolved Inter font.  The
    // input size is final, so the saved size offset must not be applied again.
    static QFont getKoreanFontForUiFont(
        const QFont& englishFont
        );

    static QColor getThemedTextColor(
        const QWidget* widget
        );

    static int sizeOffset();

    static void setSizeOffset(
        int offset
        );

    static int adjustedPointSize(
        int baseSize
        );

    static void setManagedRichText(
        QLabel* label,
        const QString& html
        );

    static QString adjustRichTextPointSizes(
        const QString& html,
        int delta
        );



    // =====================================================
    // Metrics
    // =====================================================

    static QFontMetrics getFontMetrics(
        const QFont& font
        );



    // =====================================================
    // Utilities
    // =====================================================

    static int getPlatformFontSize();

    static void clearCache();

    static void debugDump();



private:
    friend class FontManagerTests;

    // =====================================================
    // Internal Helpers
    // =====================================================

    static void resolveCoreFamilies();

    static QFont buildFont(
        const QString& primaryFamily,
        const QString& fallbackFamily,
        int size,
        int weight,
        bool italic
        );



    // =====================================================
    // State
    // =====================================================

    static bool s_loaded;

    static QString s_interFamily;

    static QString s_pretendardFamily;

    static QStringList s_loadedFamilies;

    static QStringList s_fontPaths;

    static int s_sizeOffset;

};

#endif // FONTMANAGER_H
