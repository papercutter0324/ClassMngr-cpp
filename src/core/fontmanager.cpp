#include "fontmanager.h"

#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>

#include "../utils/platform.h"

#include <QDebug>



// =========================================================
// Static Members
// =========================================================

bool FontManager::s_loaded = false;

QString FontManager::s_interFamily;

QString FontManager::s_pretendardFamily;

QStringList FontManager::s_fontPaths =
    {
        ":/fonts/Inter.ttc",
        ":/fonts/PretendardVariable.ttf"
};

QHash<
    QString,
    QHash<QString, FontStyleInfo>
    > FontManager::s_styles;



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

    for (const QString &path : s_fontPaths)
    {
        qDebug()
        << path
        << QFile::exists(path);
    }



    // =====================================================
    // Load Fonts
    // =====================================================

    for (const QString &path : s_fontPaths)
    {
        int fontId = -1;

        if (QFile::exists(path))
        {
            fontId =
                QFontDatabase::addApplicationFont(path);
        }

        if (fontId == -1)
        {
            qWarning()
            << "[FontManager] Failed to load:"
            << path;

            continue;
        }

        QStringList families =
            QFontDatabase::applicationFontFamilies(
                fontId
                );

        qDebug()
            << "[FontManager] Loaded:"
            << path
            << "->"
            << families;
    }



    // =====================================================
    // Setup
    // =====================================================

    buildStyleRegistry();

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
// Build Style Registry
// =========================================================

void FontManager::buildStyleRegistry()
{
    QFontDatabase db;

    QStringList families =
        db.families();

    for (const QString &family : families)
    {
        QStringList styles =
            db.styles(family);

        for (const QString &style : styles)
        {
            QFont font =
                db.font(
                    family,
                    style,
                    DEFAULT_SIZE
                    );

            FontStyleInfo info;

            info.weight = font.weight();

            info.italic =
                style.toLower().contains("italic");

            s_styles[family][style.toLower()] = info;
        }
    }
}



// =========================================================
// Resolve Preferred Families
// =========================================================

void FontManager::resolveCoreFamilies()
{
    for (const QString &family : s_styles.keys())
    {
        QString lower =
            family.toLower();

        if (lower == "inter")
        {
            s_interFamily = family;
        }

        if (lower.contains("pretendard variable"))
        {
            s_pretendardFamily = family;
        }
    }



    // =====================================================
    // Fallback Pretendard Search
    // =====================================================

    if (s_pretendardFamily.isEmpty())
    {
        for (const QString &family : s_styles.keys())
        {
            if (
                family.toLower()
                    .contains("pretendard")
                )
            {
                s_pretendardFamily = family;

                break;
            }
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
// Style Matching
// =========================================================

QString FontManager::findBestStyle(
    const QString &family,
    int weight,
    bool italic
    )
{
    if (!s_styles.contains(family))
    {
        return QString();
    }

    const auto &styles =
        s_styles[family];

    QString bestStyle;

    int bestScore = INT_MAX;

    for (
        auto it = styles.begin();
        it != styles.end();
        ++it
        )
    {
        const QString &styleName =
            it.key();

        const FontStyleInfo &info =
            it.value();

        int score =
            qAbs(info.weight - weight);

        if (info.italic != italic)
        {
            score += 100;
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestStyle = styleName;
        }
    }

    return bestStyle;
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
        size = getPlatformFontSize();
    }

    QFontDatabase db;

    QString style =
        findBestStyle(
            s_interFamily,
            weight,
            italic
            );

    if (style.isEmpty())
    {
        return QFont();
    }

    QFont font =
        db.font(
            s_interFamily,
            style,
            size
            );



    // =====================================================
    // Enforce Exact Style
    // =====================================================

    font.setStyleName(style);

    font.setItalic(italic);

    font.setWeight(
        static_cast<QFont::Weight>(weight)
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
            }
            );
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
    const QFont &font
    )
{
    return QFontMetrics(font);
}



// =========================================================
// Apply Globally
// =========================================================

void FontManager::applyGlobalFont(
    QApplication &app
    )
{
    QFont font =
        getUiFont();

    app.setFont(font);

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

    qDebug() << "\n[FontManager Debug]";

    qDebug() << "Inter:"
             << s_interFamily;

    qDebug() << "Pretendard:"
             << s_pretendardFamily;

    for (
        auto familyIt = s_styles.begin();
        familyIt != s_styles.end();
        ++familyIt
        )
    {
        qDebug()
        << "\n"
        << familyIt.key()
        << ":";

        const auto &styles =
            familyIt.value();

        for (
            auto styleIt = styles.begin();
            styleIt != styles.end();
            ++styleIt
            )
        {
            const FontStyleInfo &info =
                styleIt.value();

            qDebug()
                << " "
                << styleIt.key()
                << "-> weight="
                << info.weight
                << "italic="
                << info.italic;
        }
    }
}