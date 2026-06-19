#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QStringList>

class FontManagerTests;


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

};

#endif // FONTMANAGER_H
