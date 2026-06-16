#include "fontmanager.h"

#include "core/resource_paths.h"

#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>



// =========================================================
// Static Members
// =========================================================

bool FontManager::s_loaded = false;

QString FontManager::s_interFamily;

QString FontManager::s_pretendardFamily;

QStringList FontManager::s_loadedFamilies;

QStringList FontManager::s_fontPaths;



// =========================================================
// Load Fonts
// =========================================================

void FontManager::loadFonts()
{
    if (s_loaded)
    {
        return;
    }

    if (!QGuiApplication::instance())
    {
        qFatal(
            "QApplication must exist before loading fonts"
            );
    }

    qDebug() << "Checking font existence:";

    s_fontPaths =
        {
            ResourcePaths::Fonts::inter(),
            ResourcePaths::Fonts::pretendard()
        };

    const QStringList& fontPaths =
        s_fontPaths;

    for (const QString& path : fontPaths)
    {
        qDebug()
        << path
        << QFile::exists(path);
    }



    // =====================================================
    // Load Fonts
    // =====================================================

    s_loadedFamilies.clear();

    for (const QString& path : fontPaths)
    {
        int fontId = -1;

        if (QFile::exists(path))
        {
            fontId =
                QFontDatabase::addApplicationFont(
                    path
                    );
        }

        if (fontId == -1)
        {
            qWarning()
            << "[FontManager] Failed to load:"
            << path;

            continue;
        }

        const QStringList families =
            QFontDatabase::applicationFontFamilies(
                fontId
                );

        s_loadedFamilies.append(
            families
            );

        qDebug()
            << "[FontManager] Loaded:"
            << path
            << "->"
            << families;
    }



    // =====================================================
    // Resolve Families
    // =====================================================

    resolveCoreFamilies();



    // =====================================================
    // Validation
    // =====================================================

    if (s_interFamily.isEmpty())
    {
        qWarning()
        << "[FontManager] Inter not found";
    }

    if (s_pretendardFamily.isEmpty())
    {
        qWarning()
        << "[FontManager] Pretendard not found";
    }

    s_loaded = true;
}



// =========================================================
// Resolve Preferred Families
// =========================================================

void FontManager::resolveCoreFamilies()
{
    s_interFamily.clear();
    s_pretendardFamily.clear();

    for (const QString& family :
         std::as_const(s_loadedFamilies))
    {
        const QString lower =
            family.toLower();

        if (lower == "inter")
        {
            s_interFamily = family;
        }

        if (lower.contains("pretendard"))
        {
            s_pretendardFamily = family;
        }
    }
}



// =========================================================
// Platform Font Size
// =========================================================

int FontManager::getPlatformFontSize()
{
#ifdef Q_OS_MACOS
    return DEFAULT_SIZE + 1;
#else
    return DEFAULT_SIZE;
#endif
}



// =========================================================
// Main UI Font
// =========================================================

QFont FontManager::getUiFont(
    int size,
    int weight,
    bool italic
    )
{
    if (!s_loaded)
    {
        loadFonts();
    }

    if (s_interFamily.isEmpty())
    {
        return QFont();
    }

    if (size < 0)
    {
        size =
            getPlatformFontSize();
    }

    QFont font(
        s_interFamily,
        size
        );

    font.setWeight(
        static_cast<QFont::Weight>(
            weight
            )
        );

    font.setItalic(
        italic
        );



    // =====================================================
    // Korean Fallback
    // =====================================================

    if (!s_pretendardFamily.isEmpty())
    {
        font.setFamilies(
            {
                s_interFamily,
                s_pretendardFamily
            });
    }



    // =====================================================
    // Rendering Quality
    // =====================================================

    font.setHintingPreference(
        QFont::PreferFullHinting
        );

    font.setStyleStrategy(
        QFont::PreferAntialias
        );

    return font;
}



// =========================================================
// Metrics
// =========================================================

QFontMetrics FontManager::getFontMetrics(
    const QFont& font
    )
{
    return QFontMetrics(
        font
        );
}



// =========================================================
// Apply Globally
// =========================================================

void FontManager::applyGlobalFont(
    QApplication& app
    )
{
    const QFont font =
        getUiFont();

    app.setFont(
        font
        );

    qDebug()
        << "[FontManager] Global font applied:"
        << font.family();
}



// =========================================================
// Cache Control
// =========================================================

void FontManager::clearCache()
{
    // Placeholder
}



// =========================================================
// Debug
// =========================================================

void FontManager::debugDump()
{
    if (!s_loaded)
    {
        loadFonts();
    }

    qDebug()
        << "\n[FontManager Debug]";

    qDebug()
        << "Inter:"
        << s_interFamily;

    qDebug()
        << "Pretendard:"
        << s_pretendardFamily;

    qDebug()
        << "\nLoaded Families:";

    for (const QString& family :
         std::as_const(s_loadedFamilies))
    {
        qDebug()
        << " "
        << family;
    }
}
