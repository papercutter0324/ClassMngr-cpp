#include "sidebar_marquee_delegate.h"

#include "core/fontmanager.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTreeWidget>
#include <QtMath>

#include <algorithm>

namespace
{
bool isHangul(QChar character)
{
    const ushort value = character.unicode();

    return (value >= 0x1100 && value <= 0x11ff)
        || (value >= 0x3130 && value <= 0x318f)
        || (value >= 0xa960 && value <= 0xa97f)
        || (value >= 0xac00 && value <= 0xd7a3)
        || (value >= 0xd7b0 && value <= 0xd7ff);
}

bool containsHangul(const QString& text)
{
    return std::ranges::any_of(
        text,
        [](QChar character)
        {
            return isHangul(character);
        }
        );
}

QList<QTextLayout::FormatRange> textFormats(
    const QString& text,
    const QColor& color
    )
{
    QList<QTextLayout::FormatRange> formats;

    QTextLayout::FormatRange baseRange;
    baseRange.start = 0;
    baseRange.length = text.size();
    baseRange.format.setForeground(color);
    formats.append(baseRange);

    int start = -1;

    const auto appendKoreanRange =
        [&formats, &color](int rangeStart, int rangeEnd)
    {
        if (rangeStart < 0 || rangeEnd <= rangeStart)
        {
            return;
        }

        QTextLayout::FormatRange range;
        range.start = rangeStart;
        range.length = rangeEnd - rangeStart;
        range.format.setFont(
            FontManager::getKoreanFont()
            );
        range.format.setForeground(color);
        formats.append(range);
    };

    for (int index = 0; index < text.size(); ++index)
    {
        if (isHangul(text.at(index)))
        {
            if (start < 0)
            {
                start = index;
            }
        }
        else if (start >= 0)
        {
            appendKoreanRange(start, index);
            start = -1;
        }
    }

    appendKoreanRange(start, text.size());

    return formats;
}

qreal formattedTextWidth(
    const QString& text,
    const QFont& baseFont
    )
{
    QTextLayout layout(text, baseFont);
    layout.setFormats(
        textFormats(text, QColor(Qt::black))
        );
    layout.beginLayout();
    QTextLine line = layout.createLine();

    if (line.isValid())
    {
        line.setLineWidth(1000000.0);
    }

    layout.endLayout();

    return line.isValid()
        ? line.naturalTextWidth()
        : 0.0;
}

void drawFormattedText(
    QPainter* painter,
    const QString& text,
    const QFont& baseFont,
    const QColor& color,
    const QRect& textRect,
    int x
    )
{
    QTextLayout layout(text, baseFont);
    layout.setFormats(
        textFormats(text, color)
        );
    layout.beginLayout();
    QTextLine line = layout.createLine();

    if (line.isValid())
    {
        line.setLineWidth(1000000.0);
    }

    layout.endLayout();

    if (!line.isValid())
    {
        return;
    }

    const qreal y =
        textRect.top()
        + (textRect.height() - line.height()) / 2.0;

    layout.draw(
        painter,
        QPointF(x, y)
        );
}
}

SidebarMarqueeDelegate::SidebarMarqueeDelegate(
    QTreeWidget* tree,
    QObject* parent
    )
    : QStyledItemDelegate(parent)
    , m_tree(tree)
{
    m_timer.setInterval(30);

    connect(
        &m_timer,
        &QTimer::timeout,
        this,
        &SidebarMarqueeDelegate::advanceMarquee
        );

    if (m_tree && m_tree->viewport())
    {
        m_tree->viewport()->setMouseTracking(true);
        m_tree->viewport()->installEventFilter(this);
    }
}

void SidebarMarqueeDelegate::setMarqueeEnabled(
    bool enabled
    )
{
    if (m_enabled == enabled)
    {
        return;
    }

    m_enabled = enabled;
    resetMarquee();
}

void SidebarMarqueeDelegate::resetMarquee()
{
    const QModelIndex oldIndex =
        m_hoveredIndex;

    m_timer.stop();
    m_offset = 0;
    m_hoveredIndex = QPersistentModelIndex();

    updateIndex(oldIndex);
}

int SidebarMarqueeDelegate::textWidth(
    const QString& text
    ) const
{
    const QFont baseFont =
        m_tree
            ? m_tree->font()
            : FontManager::getUiFont(
                  FontManager::stdEnglishFont
                  );

    return qCeil(
        formattedTextWidth(text, baseFont)
        );
}

void SidebarMarqueeDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QModelIndex hoveredIndex =
        m_hoveredIndex;

    const bool scrolling =
        m_enabled
        && hoveredIndex == index
        && isOverflowing(opt);

    if (!scrolling && !containsHangul(opt.text))
    {
        QStyledItemDelegate::paint(
            painter,
            option,
            index
            );
        return;
    }

    const QWidget* widget =
        opt.widget;

    QStyle* style =
        widget
            ? widget->style()
            : QApplication::style();

    QStyleOptionViewItem backgroundOpt(opt);
    backgroundOpt.text.clear();
    backgroundOpt.features &=
        ~QStyleOptionViewItem::HasDisplay;

    style->drawControl(
        QStyle::CE_ItemViewItem,
        &backgroundOpt,
        painter,
        widget
        );

    const QRect textRect =
        style->subElementRect(
            QStyle::SE_ItemViewItemText,
            &opt,
            widget
            );

    const QRect visibleTextRect =
        clippedTextRect(textRect);

    if (!visibleTextRect.isValid())
    {
        return;
    }

    const int textWidth = qCeil(
        formattedTextWidth(
            opt.text,
            opt.font
            )
        );

    if (scrolling && textWidth <= visibleTextRect.width())
    {
        return;
    }

    painter->save();

    painter->setClipRect(
        visibleTextRect
        );

    const QPalette::ColorGroup colorGroup =
        (opt.state & QStyle::State_Enabled)
            ? ((opt.state & QStyle::State_Active)
                   ? QPalette::Active
                   : QPalette::Inactive)
            : QPalette::Disabled;

    const QPalette::ColorRole colorRole =
        (opt.state & QStyle::State_Selected)
            ? QPalette::HighlightedText
            : QPalette::Text;

    painter->setPen(
        opt.palette.color(
            colorGroup,
            colorRole
            )
        );

    const int cycleWidth = textWidth + ScrollGap;
    const int offset =
        scrolling && cycleWidth > 0
            ? m_offset % cycleWidth
            : 0;

    int x = textRect.left() - offset;

    do
    {
        drawFormattedText(
            painter,
            opt.text,
            opt.font,
            painter->pen().color(),
            textRect,
            x
            );

        x += cycleWidth;
    }
    while (scrolling && x < visibleTextRect.right());

    painter->restore();
}

QSize SidebarMarqueeDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    QSize size =
        QStyledItemDelegate::sizeHint(
            option,
            index
            );

    const int koreanTextHeight =
        QFontMetrics(
            FontManager::getKoreanFont()
            ).height();

    size.setHeight(
        std::max(
            size.height(),
            koreanTextHeight + 6
            )
        );

    return size;
}

bool SidebarMarqueeDelegate::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        m_tree
        && watched == m_tree->viewport()
        )
    {
        if (event->type() == QEvent::MouseMove)
        {
            auto* mouseEvent =
                static_cast<QMouseEvent*>(event);

            setHoveredIndex(
                m_tree->indexAt(
                    mouseEvent->pos()
                    )
                );
        }
        else if (event->type() == QEvent::Leave)
        {
            setHoveredIndex(
                QModelIndex()
                );
        }
    }

    return QStyledItemDelegate::eventFilter(
        watched,
        event
        );
}

void SidebarMarqueeDelegate::setHoveredIndex(
    const QModelIndex& index
    )
{
    const QModelIndex oldIndex =
        m_hoveredIndex;

    if (oldIndex == index)
    {
        return;
    }

    m_hoveredIndex =
        index;

    m_offset = 0;

    updateIndex(oldIndex);
    updateIndex(index);

    updateTimer();
}

void SidebarMarqueeDelegate::advanceMarquee()
{
    if (
        !m_enabled
        || !m_hoveredIndex.isValid()
        )
    {
        m_timer.stop();
        return;
    }

    const QModelIndex index =
        m_hoveredIndex;

    if (!isIndexOverflowing(index))
    {
        m_timer.stop();
        m_offset = 0;
        updateIndex(index);
        return;
    }

    ++m_offset;

    if (m_offset > 100000)
    {
        m_offset = 0;
    }

    updateIndex(index);
}

void SidebarMarqueeDelegate::updateTimer()
{
    if (
        m_enabled
        && m_hoveredIndex.isValid()
        && isIndexOverflowing(m_hoveredIndex)
        )
    {
        if (!m_timer.isActive())
        {
            m_timer.start();
        }
    }
    else
    {
        m_timer.stop();
    }
}

void SidebarMarqueeDelegate::updateIndex(
    const QModelIndex& index
    ) const
{
    if (
        !m_tree
        || !m_tree->viewport()
        || !index.isValid()
        )
    {
        return;
    }

    m_tree->viewport()->update(
        m_tree->visualRect(index)
        );
}

bool SidebarMarqueeDelegate::isIndexOverflowing(
    const QModelIndex& index
    ) const
{
    if (
        !m_tree
        || !index.isValid()
        )
    {
        return false;
    }

    QStyleOptionViewItem option;
    option.initFrom(
        m_tree->viewport()
        );
    option.widget =
        m_tree->viewport();
    option.rect =
        m_tree->visualRect(index);

    initStyleOption(
        &option,
        index
        );

    return isOverflowing(option);
}

bool SidebarMarqueeDelegate::isOverflowing(
    const QStyleOptionViewItem& option
    ) const
{
    if (
        !m_tree
        || !m_tree->viewport()
        || option.text.isEmpty()
        )
    {
        return false;
    }

    const QWidget* widget =
        option.widget;

    QStyle* style =
        widget
            ? widget->style()
            : QApplication::style();

    const QRect textRect =
        style->subElementRect(
            QStyle::SE_ItemViewItemText,
            &option,
            widget
            );

    const QRect visibleTextRect =
        clippedTextRect(textRect);

    if (!visibleTextRect.isValid())
    {
        return false;
    }

    return formattedTextWidth(
               option.text,
               option.font
               ) > visibleTextRect.width();
}

QRect SidebarMarqueeDelegate::clippedTextRect(
    const QRect& textRect
    ) const
{
    if (
        !m_tree
        || !m_tree->viewport()
        )
    {
        return textRect;
    }

    return textRect.intersected(
        m_tree->viewport()->rect()
        );
}
