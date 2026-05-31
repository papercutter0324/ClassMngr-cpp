#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QString>
#include <QStringList>



// =========================================================
// Font Style Metadata
// =========================================================

struct FontStyleInfo
{
    int weight;
    bool italic;
};



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
        QApplication &app
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
        const QFont &font
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

    static void buildStyleRegistry();

    static void resolveCoreFamilies();

    static QString findBestStyle(
        const QString &family,
        int weight,
        bool italic
        );



    // =====================================================
    // State
    // =====================================================

    static bool s_loaded;

    static QString s_interFamily;

    static QString s_pretendardFamily;

    static QStringList s_fontPaths;

    static QHash<
        QString,
        QHash<QString, FontStyleInfo>
        > s_styles;



    // =====================================================
    // Constants
    // =====================================================

    static constexpr int DEFAULT_SIZE = 12;
};



#endif // FONTMANAGER_H