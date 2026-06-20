#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QStringList>

class FontManagerTests;
class QLabel;


// =========================================================
// Font Manager
// =========================================================

class FontManager
{
public:
    static constexpr int stdEnglishFont = 12;
    static constexpr int stdKoreanFont = 13;

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
        int size = stdKoreanFont,
        int weight = QFont::Normal,
        bool italic = false
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
