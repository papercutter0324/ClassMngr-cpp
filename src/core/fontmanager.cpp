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
    return stdEnglishFont + 1;
#else
    return stdEnglishFont;
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

    if (size < 0)
    {
        size =
            getPlatformFontSize();
    }

    return buildFont(
        s_interFamily,
        s_pretendardFamily,
        size,
        weight,
        italic
        );
}



// =========================================================
// Korean UI Font
// =========================================================

QFont FontManager::getKoreanFont(
    int size,
    int weight,
    bool italic
    )
{
    if (!s_loaded)
    {
        loadFonts();
    }

    if (size < 0)
    {
        size = stdKoreanFont;
    }

    return buildFont(
        s_pretendardFamily,
        s_interFamily,
        size,
        weight,
        italic
        );
}



QFont FontManager::buildFont(
    const QString& primaryFamily,
    const QString& fallbackFamily,
    int size,
    int weight,
    bool italic
    )
{
    const QString resolvedPrimary =
        !primaryFamily.isEmpty()
            ? primaryFamily
            : fallbackFamily;

    QFont font;

    if (!resolvedPrimary.isEmpty())
    {
        font = QFont(
            resolvedPrimary,
            size
            );
    }
    else
    {
        font.setPointSize(size);
    }

    font.setWeight(
        static_cast<QFont::Weight>(
            weight
            )
        );

    font.setItalic(
        italic
        );



    // =====================================================
    // Fallback
    // =====================================================

    if (
        !fallbackFamily.isEmpty()
        && fallbackFamily != resolvedPrimary
        )
    {
        font.setFamilies(
            {
                resolvedPrimary,
                fallbackFamily
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
    QApplication& app,
    const QString& localeName
    )
{
    const bool koreanLocale =
        localeName.startsWith(
            QStringLiteral("ko"),
            Qt::CaseInsensitive
            );

    const QFont font =
        koreanLocale
            ? getKoreanFont()
            : getUiFont();

    app.setFont(
        font
        );

    qDebug()
        << "[FontManager] Global font applied:"
        << font.family()
        << font.pointSize();
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
