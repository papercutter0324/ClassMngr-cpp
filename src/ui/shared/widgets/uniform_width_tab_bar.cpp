#include "uniform_width_tab_bar.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyleOption>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

namespace
{

constexpr int NavigationTabHorizontalPadding = 14;
constexpr int NavigationTabVerticalPadding = 6;
constexpr int NavigationTabGap = 6;
constexpr qreal NavigationTabRadius = 7.0;
constexpr qreal NavigationTabBorderWidth = 1.0;
constexpr int NavigationTabBorderExtent = 2;
constexpr int NavigationTabIconSpacing = 4;
const QColor NavigationTabSelectedTextColor(16, 20, 24);

class EdgeAlignedTabBarStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QRect subElementRect(
        SubElement element,
        const QStyleOption* option,
        const QWidget* widget
        ) const override
    {
        QRect rect =
            QProxyStyle::subElementRect(
                element,
                option,
                widget
                );

        const auto* tabBar =
            qobject_cast<const QTabBar*>(
                widget
                );

        if (
            !tabBar
            || tabBar->shape() == QTabBar::RoundedWest
            || tabBar->shape() == QTabBar::RoundedEast
            || tabBar->shape() == QTabBar::TriangularWest
            || tabBar->shape() == QTabBar::TriangularEast
            )
        {
            return rect;
        }

        if (element == QStyle::SE_TabBarScrollLeftButton)
        {
            rect.moveLeft(0);
        }
        else if (element == QStyle::SE_TabBarScrollRightButton)
        {
            rect.moveRight(
                tabBar->width() - 1
                );
        }

        return rect;
    }
};

}

UniformWidthTabBar::UniformWidthTabBar(
    QWidget* parent
    )
    : QTabBar(parent)
{
    setDrawBase(false);
    setExpanding(false);

    auto* edgeAlignedStyle =
        new EdgeAlignedTabBarStyle(
            QApplication::style()->name()
            );
    edgeAlignedStyle->setParent(this);
    setStyle(edgeAlignedStyle);

    connect(
        this,
        &QTabBar::currentChanged,
        this,
        [this]
        {
            scheduleScrollControlRefresh();
        }
        );
}

QSize UniformWidthTabBar::tabSizeHint(
    int index
    ) const
{
    QSize hint =
        naturalTabSizeHint(index);

    int widestTabWidth =
        hint.width();

    for (int tabIndex = 0; tabIndex < count(); ++tabIndex)
    {
        widestTabWidth =
            std::max(
                widestTabWidth,
                naturalTabSizeHint(tabIndex).width()
                );
    }

    hint.setWidth(
        widestTabWidth
        );

    return hint;
}

UniformWidthTabAppearance UniformWidthTabBar::tabAppearance() const
{
    return m_tabAppearance;
}

void UniformWidthTabBar::setTabAppearance(
    UniformWidthTabAppearance appearance
    )
{
    if (m_tabAppearance == appearance)
    {
        return;
    }

    m_tabAppearance =
        appearance;

    setProperty(
        "uniformTabAppearance",
        appearance == UniformWidthTabAppearance::NavigationPill
            ? QStringLiteral("navigationPill")
            : QStringLiteral("platform")
        );

    setAttribute(
        Qt::WA_TranslucentBackground,
        appearance == UniformWidthTabAppearance::NavigationPill
        );
    setAutoFillBackground(false);
    updateGeometry();
    update();
}

QColor UniformWidthTabBar::navigationTabFillColor() const
{
    return m_navigationTabFillColor;
}

void UniformWidthTabBar::setNavigationTabFillColor(
    const QColor& color
    )
{
    m_navigationTabFillColor =
        color;
    update();
}

QColor UniformWidthTabBar::navigationTabBorderColor() const
{
    return m_navigationTabBorderColor;
}

void UniformWidthTabBar::setNavigationTabBorderColor(
    const QColor& color
    )
{
    m_navigationTabBorderColor =
        color;
    update();
}

QColor UniformWidthTabBar::navigationTabHoverColor() const
{
    return m_navigationTabHoverColor;
}

void UniformWidthTabBar::setNavigationTabHoverColor(
    const QColor& color
    )
{
    m_navigationTabHoverColor =
        color;
    update();
}

QColor UniformWidthTabBar::navigationTabSelectedColor() const
{
    return m_navigationTabSelectedColor;
}

void UniformWidthTabBar::setNavigationTabSelectedColor(
    const QColor& color
    )
{
    m_navigationTabSelectedColor =
        color;
    update();
}

QColor UniformWidthTabBar::navigationTabTextColor() const
{
    return m_navigationTabTextColor;
}

void UniformWidthTabBar::setNavigationTabTextColor(
    const QColor& color
    )
{
    m_navigationTabTextColor =
        color;
    update();
}

int UniformWidthTabBar::naturalWidth() const
{
    int width = 0;

    for (int index = 0; index < count(); ++index)
    {
        width +=
            tabSizeHint(index).width();
    }

    return width;
}

void UniformWidthTabBar::paintEvent(
    QPaintEvent* event
    )
{
    if (m_tabAppearance != UniformWidthTabAppearance::NavigationPill)
    {
        QTabBar::paintEvent(event);
        return;
    }

    Q_UNUSED(event);
    paintNavigationPills();
}

void UniformWidthTabBar::tabLayoutChange()
{
    QTabBar::tabLayoutChange();
    scheduleScrollControlRefresh();
}

void UniformWidthTabBar::resizeEvent(
    QResizeEvent* event
    )
{
    QTabBar::resizeEvent(event);
    refreshScrollControls();
}

void UniformWidthTabBar::showEvent(
    QShowEvent* event
    )
{
    QTabBar::showEvent(event);
    refreshScrollControls();
}

void UniformWidthTabBar::wheelEvent(
    QWheelEvent* event
    )
{
    event->ignore();
}

QSize UniformWidthTabBar::naturalTabSizeHint(
    int index
    ) const
{
    if (
        m_tabAppearance != UniformWidthTabAppearance::NavigationPill
        || index < 0
        || index >= count()
        )
    {
        return QTabBar::tabSizeHint(index);
    }

    const QFontMetrics metrics(font());
    const QIcon icon =
        tabIcon(index);
    const int iconExtent =
        style()->pixelMetric(
            QStyle::PM_SmallIconSize,
            nullptr,
            this
            );
    const QSize iconSize =
        icon.isNull()
            ? QSize()
            : icon.actualSize(
                QSize(iconExtent, iconExtent)
                );

    int contentWidth =
        metrics.horizontalAdvance(
            tabText(index)
            );

    if (!iconSize.isEmpty())
    {
        contentWidth +=
            iconSize.width()
            + NavigationTabIconSpacing;
    }

    const int contentHeight =
        std::max(
            metrics.height(),
            iconSize.height()
            );

    return QSize(
        contentWidth
            + (2 * NavigationTabHorizontalPadding)
            + NavigationTabGap
            + NavigationTabBorderExtent,
        contentHeight
            + (2 * NavigationTabVerticalPadding)
            + NavigationTabBorderExtent
        );
}

void UniformWidthTabBar::paintNavigationPills()
{
    QPainter painter(this);
    painter.setRenderHint(
        QPainter::Antialiasing,
        true
        );
    painter.setFont(font());

    for (int index = 0; index < count(); ++index)
    {
        QStyleOptionTab option;
        initStyleOption(
            &option,
            index
            );

        const bool enabled =
            option.state.testFlag(
                QStyle::State_Enabled
                );
        const bool selected =
            option.state.testFlag(
                QStyle::State_Selected
                );
        const bool hovered =
            option.state.testFlag(
                QStyle::State_MouseOver
                );
        const QPalette::ColorGroup colorGroup =
            enabled
                ? (
                    isActiveWindow()
                        ? QPalette::Active
                        : QPalette::Inactive
                    )
                : QPalette::Disabled;

        QColor fillColor =
            m_navigationTabFillColor.isValid()
                ? m_navigationTabFillColor
                : option.palette.color(
                    colorGroup,
                    QPalette::Button
                    );
        QColor borderColor =
            m_navigationTabBorderColor.isValid()
                ? m_navigationTabBorderColor
                : option.palette.color(
                    colorGroup,
                    QPalette::Mid
                    );
        QColor textColor =
            m_navigationTabTextColor.isValid()
                ? m_navigationTabTextColor
                : option.palette.color(
                    colorGroup,
                    QPalette::ButtonText
                    );

        if (selected)
        {
            fillColor =
                m_navigationTabSelectedColor.isValid()
                    ? m_navigationTabSelectedColor
                    : option.palette.color(
                        colorGroup,
                        QPalette::Highlight
                        );
            borderColor =
                fillColor;
            textColor =
                NavigationTabSelectedTextColor;
        }
        else if (hovered)
        {
            fillColor =
                m_navigationTabHoverColor.isValid()
                    ? m_navigationTabHoverColor
                    : option.palette.color(
                        colorGroup,
                        QPalette::Light
                        );
            borderColor =
                m_navigationTabSelectedColor.isValid()
                    ? m_navigationTabSelectedColor
                    : option.palette.color(
                        colorGroup,
                        QPalette::Highlight
                        );
        }

        const QRect tabBounds =
            tabRect(index);
        const QRectF pillRect =
            QRectF(
                tabBounds.adjusted(
                    0,
                    0,
                    -NavigationTabGap,
                    0
                    )
                ).adjusted(
                    NavigationTabBorderWidth / 2.0,
                    NavigationTabBorderWidth / 2.0,
                    -NavigationTabBorderWidth / 2.0,
                    -NavigationTabBorderWidth / 2.0
                    );

        painter.setBrush(
            fillColor
            );
        painter.setPen(
            QPen(
                borderColor,
                NavigationTabBorderWidth
                )
            );
        painter.drawRoundedRect(
            pillRect,
            NavigationTabRadius,
            NavigationTabRadius
            );

        QRect contentRect =
            pillRect.toAlignedRect().adjusted(
                NavigationTabHorizontalPadding,
                0,
                -NavigationTabHorizontalPadding,
                0
                );
        const QIcon icon =
            tabIcon(index);

        painter.setPen(
            textColor
            );

        if (icon.isNull())
        {
            const QString text =
                painter.fontMetrics().elidedText(
                    tabText(index),
                    elideMode(),
                    contentRect.width()
                    );

            painter.drawText(
                contentRect,
                Qt::AlignCenter,
                text
                );
            continue;
        }

        const int iconExtent =
            style()->pixelMetric(
                QStyle::PM_SmallIconSize,
                &option,
                this
                );
        const QSize iconSize =
            icon.actualSize(
                QSize(iconExtent, iconExtent)
                );
        const int textWidth =
            painter.fontMetrics().horizontalAdvance(
                tabText(index)
                );
        const int combinedWidth =
            iconSize.width()
            + NavigationTabIconSpacing
            + textWidth;
        const int iconLeft =
            contentRect.center().x()
            - (combinedWidth / 2);
        const QRect iconRect(
            iconLeft,
            contentRect.center().y()
                - (iconSize.height() / 2),
            iconSize.width(),
            iconSize.height()
            );

        icon.paint(
            &painter,
            iconRect,
            Qt::AlignCenter,
            enabled
                ? QIcon::Normal
                : QIcon::Disabled,
            selected
                ? QIcon::On
                : QIcon::Off
            );

        const QRect textRect(
            iconRect.right()
                + 1
                + NavigationTabIconSpacing,
            contentRect.top(),
            contentRect.right()
                - iconRect.right()
                - NavigationTabIconSpacing,
            contentRect.height()
            );
        const QString text =
            painter.fontMetrics().elidedText(
                tabText(index),
                elideMode(),
                textRect.width()
                );

        painter.drawText(
            textRect,
            Qt::AlignVCenter | Qt::AlignLeft,
            text
            );
    }
}

void UniformWidthTabBar::scheduleScrollControlRefresh()
{
    QTimer::singleShot(
        0,
        this,
        [this]
        {
            refreshScrollControls();
        }
        );
}

QToolButton* UniformWidthTabBar::scrollButton(
    const char* objectName
    ) const
{
    return findChild<QToolButton*>(
        QString::fromLatin1(objectName),
        Qt::FindDirectChildrenOnly
        );
}

void UniformWidthTabBar::refreshScrollControls()
{
    QToolButton* leftButton =
        scrollButton("ScrollLeftButton");
    QToolButton* rightButton =
        scrollButton("ScrollRightButton");

    if (!leftButton || !rightButton)
    {
        return;
    }

    const bool scrollingRequired =
        naturalWidth() > width();

    removeTrailingGap(
        leftButton,
        rightButton,
        scrollingRequired
        );

    leftButton->setVisible(scrollingRequired);
    rightButton->setVisible(scrollingRequired);

    if (scrollingRequired)
    {
        leftButton->raise();
        rightButton->raise();
    }
}

void UniformWidthTabBar::removeTrailingGap(
    QToolButton* leftButton,
    QToolButton* rightButton,
    bool scrollingRequired
    )
{
    if (count() <= 0)
    {
        return;
    }

    const int rightEdge =
        scrollingRequired
            ? rightButton->x()
            : width();

    for (int step = 0; step < count(); ++step)
    {
        const QRect lastTab =
            tabRect(count() - 1);

        if (
            lastTab.right() >= rightEdge - 1
            || !leftButton->isEnabled()
            )
        {
            break;
        }

        leftButton->click();
    }
}

UniformWidthTabWidget::UniformWidthTabWidget(
    const QString& tabBarObjectName,
    QWidget* parent
    )
    : UniformWidthTabWidget(
        UniformWidthTabKind::Generic,
        tabBarObjectName,
        parent
        )
{
}

UniformWidthTabWidget::UniformWidthTabWidget(
    UniformWidthTabKind kind,
    const QString& tabBarObjectName,
    QWidget* parent
    )
    : QTabWidget(parent)
{
    auto* uniformTabBar =
        new UniformWidthTabBar(this);

    uniformTabBar->setObjectName(
        tabBarObjectName
        );
    uniformTabBar->installEventFilter(
        this
        );

    const QFont navigationFont =
        QApplication::font();

    setFont(
        navigationFont
        );
    uniformTabBar->setFont(
        navigationFont
        );

    setTabBar(
        uniformTabBar
        );

    setTabKind(
        kind
        );
}

UniformWidthTabKind UniformWidthTabWidget::tabKind() const
{
    return m_tabKind;
}

void UniformWidthTabWidget::setTabKind(
    UniformWidthTabKind kind
    )
{
    m_tabKind =
        kind;

    const QString value =
        kindPropertyValue(kind);

    setProperty(
        "uniformTabKind",
        value
        );

    if (tabBar())
    {
        tabBar()->setProperty(
            "uniformTabKind",
            value
            );
    }

    if (kind == UniformWidthTabKind::Generic)
    {
        return;
    }

    if (
        kind == UniformWidthTabKind::Grade
        || kind == UniformWidthTabKind::Class
        )
    {
        setTabAppearance(
            UniformWidthTabAppearance::NavigationPill
            );
    }

    setTabShape(
        QTabWidget::Rounded
        );
    setDocumentMode(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
}

UniformWidthTabAppearance UniformWidthTabWidget::tabAppearance() const
{
    return m_tabAppearance;
}

void UniformWidthTabWidget::setTabAppearance(
    UniformWidthTabAppearance appearance
    )
{
    if (m_tabAppearance == appearance)
    {
        return;
    }

    m_tabAppearance =
        appearance;

    setProperty(
        "uniformTabAppearance",
        appearance == UniformWidthTabAppearance::NavigationPill
            ? QStringLiteral("navigationPill")
            : QStringLiteral("platform")
        );

    if (auto* uniformTabBar =
            qobject_cast<UniformWidthTabBar*>(
                tabBar()
                ))
    {
        uniformTabBar->setTabAppearance(
            appearance
            );
    }

    updateGeometry();
    update();
    centerTabBar();
}

bool UniformWidthTabWidget::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        watched == tabBar()
        && event
        && (
            event->type() == QEvent::Move
            || event->type() == QEvent::Resize
            || event->type() == QEvent::Show
            || event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::Paint
            )
        )
    {
        centerTabBar();
    }

    return QTabWidget::eventFilter(
        watched,
        event
        );
}

void UniformWidthTabWidget::resizeEvent(
    QResizeEvent* event
    )
{
    QTabWidget::resizeEvent(event);
    centerTabBar();
}

void UniformWidthTabWidget::showEvent(
    QShowEvent* event
    )
{
    QTabWidget::showEvent(event);
    centerTabBar();
}

void UniformWidthTabWidget::tabInserted(
    int index
    )
{
    QTabWidget::tabInserted(index);
    centerTabBar();
}

void UniformWidthTabWidget::tabRemoved(
    int index
    )
{
    QTabWidget::tabRemoved(index);
    centerTabBar();
}

void UniformWidthTabWidget::centerTabBar()
{
    auto* bar =
        qobject_cast<UniformWidthTabBar*>(
            tabBar()
            );

    if (
        !bar
        || bar->count() <= 0
        || tabPosition() == QTabWidget::West
        || tabPosition() == QTabWidget::East
        )
    {
        return;
    }

    const int availableWidth =
        width();
    const int naturalWidth =
        bar->naturalWidth();

    if (
        availableWidth <= 0
        || naturalWidth <= 0
        )
    {
        return;
    }

    QRect geometry =
        bar->geometry();

    const int centeredWidth =
        std::min(
            naturalWidth,
            availableWidth
            );
    const int centeredX =
        (availableWidth - centeredWidth) / 2;

    if (
        geometry.x() == centeredX
        && geometry.width() == centeredWidth
        )
    {
        return;
    }

    geometry.setX(
        centeredX
        );
    geometry.setWidth(
        centeredWidth
        );

    bar->setGeometry(
        geometry
        );
}

QString UniformWidthTabWidget::kindPropertyValue(
    UniformWidthTabKind kind
    )
{
    switch (kind)
    {
    case UniformWidthTabKind::Grade:
        return QStringLiteral("grade");

    case UniformWidthTabKind::Class:
        return QStringLiteral("class");

    case UniformWidthTabKind::Section:
        return QStringLiteral("section");

    case UniformWidthTabKind::Generic:
        break;
    }

    return QStringLiteral("generic");
}
