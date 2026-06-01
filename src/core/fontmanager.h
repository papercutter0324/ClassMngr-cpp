#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QString>
#include <QStringList>



// =========================================================
// Font Manager
// =========================================================

class FontManager
{
public:

    // =====================================================
    // Setup
    // =====================================================

    static void loadFonts();

    static void applyGlobalFont(
        QApplication& app
        );



    // =====================================================
    // Fonts
    // =====================================================

    static QFont getUiFont(
        int size = -1,
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

    // =====================================================
    // Internal Helpers
    // =====================================================

    static void resolveCoreFamilies();



    // =====================================================
    // State
    // =====================================================

    static bool s_loaded;

    static QString s_interFamily;

    static QString s_pretendardFamily;

    static QStringList s_loadedFamilies;

    static QStringList s_fontPaths;



    // =====================================================
    // Constants
    // =====================================================

    static constexpr int DEFAULT_SIZE = 12;
};

#endif // FONTMANAGER_H