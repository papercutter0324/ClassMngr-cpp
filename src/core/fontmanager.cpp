#include "fontmanager.h"

#include "core/resource_paths.h"

#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>



// =========================================================
// Static Members
// =========================================================

bool FontManager::s_loaded = false;

QString FontManager::s_interFamily;

QString FontManager::s_pretendardFamily;

QStringList FontManager::s_loadedFamilies;

QStringList FontManager::s_fontPaths;

int FontManager::s_sizeOffset = 0;

namespace
{
constexpr auto ManagedRichTextProperty =
    "_classmngr_managed_font_size_rich_text";

constexpr auto MaterializedInheritedFontProperty =
    "_classmngr_materialized_inherited_font";

constexpr int ManagedMenuPointSizeDelta = -2;

QFont s_currentManagedMenuFont;

bool s_hasCurrentManagedMenuFont = false;

QPointer<QObject> s_menuFontEventFilter;

struct WidgetFontSnapshot
{
    QPointer<QWidget> widget;
    QFont font;
    bool wasInherited = false;
};

struct TableItemFontSnapshot
{
    QTableWidgetItem* item = nullptr;
    QFont font;
};

void refreshMaterializedInheritedFonts(
    QWidget* widget,
    const QFont& inheritedFont
    )
{
    if (!widget)
    {
        return;
    }

    if (widget->property(
            MaterializedInheritedFontProperty
            ).toBool())
    {
        widget->setFont(inheritedFont);
    }

    const QFont childInheritedFont =
        widget->font();

    const auto children =
        widget->findChildren<QWidget*>(
            QString(),
            Qt::FindDirectChildrenOnly
            );

    for (QWidget* child : children)
    {
        refreshMaterializedInheritedFonts(
            child,
            childInheritedFont
            );
    }
}

QFont menuFontFromManagedFont(
    QFont font
    )
{
    if (font.pointSize() > 0)
    {
        font.setPointSize(
            qMax(
                1,
                font.pointSize() + ManagedMenuPointSizeDelta
                )
            );
    }

    return font;
}

void applyMenuFontToWidget(
    QWidget* widget
    )
{
    if (!widget || !s_hasCurrentManagedMenuFont)
    {
        return;
    }

    if (
        !qobject_cast<QMenuBar*>(widget)
        && !qobject_cast<QMenu*>(widget)
        )
    {
        return;
    }

    widget->setFont(
        s_currentManagedMenuFont
        );
    widget->updateGeometry();
    widget->update();
}

class MenuFontEventFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(
        QObject* object,
        QEvent* event
        ) override
    {
        if (
            event
            && (
                event->type() == QEvent::Polish
                || event->type() == QEvent::Show
                )
            )
        {
            applyMenuFontToWidget(
                qobject_cast<QWidget*>(object)
                );
        }

        return QObject::eventFilter(
            object,
            event
            );
    }
};

void ensureMenuFontEventFilter(
    QApplication& app
    )
{
    if (s_menuFontEventFilter)
    {
        return;
    }

    auto* filter =
        new MenuFontEventFilter(&app);

    app.installEventFilter(
        filter
        );

    s_menuFontEventFilter =
        filter;
}

void applyManagedMenuFont(
    QApplication& app,
    const QFont& managedFont
    )
{
    ensureMenuFontEventFilter(
        app
        );

    s_currentManagedMenuFont =
        menuFontFromManagedFont(
            managedFont
            );
    s_hasCurrentManagedMenuFont =
        true;

    app.setFont(
        s_currentManagedMenuFont,
        "QMenuBar"
        );
    app.setFont(
        s_currentManagedMenuFont,
        "QMenu"
        );

    const QWidgetList widgets =
        app.allWidgets();

    for (QWidget* widget : widgets)
    {
        if (
            qobject_cast<QMenuBar*>(widget)
            || qobject_cast<QMenu*>(widget)
            )
        {
            applyMenuFontToWidget(
                widget
                );
        }
    }
}
}



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

int FontManager::sizeOffset()
{
    return s_sizeOffset;
}

void FontManager::setSizeOffset(
    int offset
    )
{
    s_sizeOffset = offset;
}

int FontManager::adjustedPointSize(
    int baseSize
    )
{
    return qMax(
        1,
        baseSize + s_sizeOffset
        );
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

    size = adjustedPointSize(size);

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

    size = adjustedPointSize(size);

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

    applyManagedMenuFont(
        app,
        font
        );

    for (QWidget* topLevelWidget : app.topLevelWidgets())
    {
        refreshMaterializedInheritedFonts(
            topLevelWidget,
            font
            );
    }

    qDebug()
        << "[FontManager] Global font applied:"
        << font.family()
        << font.pointSize();
}

void FontManager::applyFontSize(
    QApplication& app,
    const QString& localeName,
    int offset
    )
{
    const int delta =
        offset - s_sizeOffset;

    if (delta == 0)
    {
        return;
    }

    QList<WidgetFontSnapshot> widgetFonts;
    QList<TableItemFontSnapshot> tableItemFonts;
    QList<QPair<QPointer<QLabel>, QString>> richTextLabels;

    const QWidgetList widgets =
        app.allWidgets();

    for (QWidget* widget : widgets)
    {
        if (!widget)
        {
            continue;
        }

        if (
            qobject_cast<QMenuBar*>(widget)
            || qobject_cast<QMenu*>(widget)
            )
        {
            continue;
        }

        const bool materializedInheritedFont =
            widget->property(
                MaterializedInheritedFontProperty
                ).toBool();

        widgetFonts.append(
            {
                widget,
                widget->font(),
                materializedInheritedFont
                    || !widget->testAttribute(Qt::WA_SetFont)
            }
            );

        if (auto* label = qobject_cast<QLabel*>(widget);
            label
            && label->property(ManagedRichTextProperty).toBool())
        {
            richTextLabels.append(
                {
                    label,
                    label->text()
                }
                );
        }

        auto* table =
            qobject_cast<QTableWidget*>(widget);

        if (!table)
        {
            continue;
        }

        for (int row = 0; row < table->rowCount(); ++row)
        {
            for (int column = 0;
                 column < table->columnCount();
                 ++column)
            {
                QTableWidgetItem* item =
                    table->item(row, column);

                if (
                    item
                    && item->font().pointSize() > 0
                    )
                {
                    tableItemFonts.append(
                        {
                            item,
                            item->font()
                        }
                        );
                }
            }
        }
    }

    s_sizeOffset = offset;

    applyGlobalFont(
        app,
        localeName
        );

    const auto resizeFont =
        [delta](QFont font)
        {
            if (font.pointSize() > 0)
            {
                font.setPointSize(
                    qMax(
                        1,
                        font.pointSize() + delta
                        )
                    );
            }

            return font;
        };

    for (const WidgetFontSnapshot& snapshot : widgetFonts)
    {
        if (snapshot.widget)
        {
            snapshot.widget->setFont(
                resizeFont(snapshot.font)
                );

            if (snapshot.wasInherited)
            {
                snapshot.widget->setProperty(
                    MaterializedInheritedFontProperty,
                    true
                    );
            }
        }
    }

    for (const TableItemFontSnapshot& snapshot : tableItemFonts)
    {
        if (snapshot.item)
        {
            snapshot.item->setFont(
                resizeFont(snapshot.font)
                );
        }
    }

    for (const auto& [label, html] : richTextLabels)
    {
        if (label)
        {
            label->setText(
                adjustRichTextPointSizes(
                    html,
                    delta
                    )
                );
        }
    }

    for (QWidget* widget : widgets)
    {
        if (!widget)
        {
            continue;
        }

        widget->updateGeometry();
        widget->update();

        if (QLayout* layout = widget->layout())
        {
            layout->invalidate();
        }
    }

    const QFont menuFont =
        localeName.startsWith(
            QStringLiteral("ko"),
            Qt::CaseInsensitive
            )
            ? getKoreanFont()
            : getUiFont();

    applyManagedMenuFont(
        app,
        menuFont
        );
}

void FontManager::setManagedRichText(
    QLabel* label,
    const QString& html
    )
{
    if (!label)
    {
        return;
    }

    label->setProperty(
        ManagedRichTextProperty,
        true
        );
    label->setText(html);
}

QString FontManager::adjustRichTextPointSizes(
    const QString& html,
    int delta
    )
{
    if (delta == 0)
    {
        return html;
    }

    static const QRegularExpression pointSizeExpression(
        QStringLiteral("font-size\\s*:\\s*(\\d+)pt"),
        QRegularExpression::CaseInsensitiveOption
        );

    QString adjusted = html;
    QList<QRegularExpressionMatch> matches;

    auto iterator =
        pointSizeExpression.globalMatch(html);

    while (iterator.hasNext())
    {
        matches.prepend(iterator.next());
    }

    for (const QRegularExpressionMatch& match : matches)
    {
        const int pointSize =
            match.captured(1).toInt();

        adjusted.replace(
            match.capturedStart(1),
            match.capturedLength(1),
            QString::number(
                qMax(1, pointSize + delta)
                )
            );
    }

    return adjusted;
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
