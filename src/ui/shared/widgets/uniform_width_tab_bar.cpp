#include "uniform_width_tab_bar.h"

#include "core/fontmanager.h"
#include "ui/shared/widgets/navigation_pill_style.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QProxyStyle>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyleOption>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

namespace
{

constexpr int ScrollButtonWidth = 28;
constexpr int DragScrollStep = 48;
constexpr int NavigationSettingsButtonWidth = 46;

class EdgeAlignedTabBarStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    int pixelMetric(
        PixelMetric metric,
        const QStyleOption* option,
        const QWidget* widget
        ) const override
    {
        const int metricValue =
            QProxyStyle::pixelMetric(
                metric,
                option,
                widget
                );

        if (
            metric == QStyle::PM_TabBarScrollButtonWidth
            && qobject_cast<const UniformWidthTabBar*>(widget)
            )
        {
            return std::max(
                metricValue,
                ScrollButtonWidth
                );
        }

        return metricValue;
    }

    void drawPrimitive(
        PrimitiveElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget
        ) const override
    {
        const bool isScrollArrow =
            element == QStyle::PE_IndicatorArrowLeft
            || element == QStyle::PE_IndicatorArrowRight;
        const QWidget* parent =
            widget
                ? widget->parentWidget()
                : nullptr;

        if (
            !isScrollArrow
            || !option
            || !painter
            || !qobject_cast<const UniformWidthTabBar*>(parent)
            )
        {
            QProxyStyle::drawPrimitive(
                element,
                option,
                painter,
                widget
                );
            return;
        }

        const QPalette::ColorGroup colorGroup =
            option->state.testFlag(QStyle::State_Enabled)
                ? QPalette::Active
                : QPalette::Disabled;
        const QColor arrowColor =
            option->palette.color(
                colorGroup,
                QPalette::ButtonText
                );
        const QPointF center =
            option->rect.center();
        constexpr qreal ArrowHalfWidth = 3.0;
        constexpr qreal ArrowHalfHeight = 5.0;
        const qreal direction =
            element == QStyle::PE_IndicatorArrowLeft
                ? -1.0
                : 1.0;
        const QPointF arrowPoints[] = {
            QPointF(
                center.x() - (direction * ArrowHalfWidth),
                center.y() - ArrowHalfHeight
                ),
            QPointF(
                center.x() + (direction * ArrowHalfWidth),
                center.y()
                ),
            QPointF(
                center.x() - (direction * ArrowHalfWidth),
                center.y() + ArrowHalfHeight
                )
        };

        painter->save();
        painter->setRenderHint(
            QPainter::Antialiasing,
            true
            );
        painter->setBrush(Qt::NoBrush);
        painter->setPen(
            QPen(
                arrowColor,
                2.25,
                Qt::SolidLine,
                Qt::RoundCap,
                Qt::RoundJoin
                )
            );
        painter->drawPolyline(
            arrowPoints,
            3
            );
        painter->restore();
    }

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

bool UniformWidthTabBar::currentSelectionVisible() const
{
    return m_currentSelectionVisible;
}

void UniformWidthTabBar::setCurrentSelectionVisible(
    bool visible
    )
{
    if (m_currentSelectionVisible == visible)
    {
        return;
    }

    m_currentSelectionVisible = visible;
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

void UniformWidthTabBar::changeEvent(
    QEvent* event
    )
{
    QTabBar::changeEvent(event);

    if (
        event
        && (
            event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            || event->type() == QEvent::StyleChange
            )
        )
    {
        scheduleScrollControlRefresh();
    }
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

void UniformWidthTabBar::mouseMoveEvent(
    QMouseEvent* event
    )
{
    if (
        !event
        || !m_dragScrollCandidate
        || !event->buttons().testFlag(Qt::LeftButton)
        )
    {
        QTabBar::mouseMoveEvent(event);
        return;
    }

    const int currentX =
        event->position().toPoint().x();
    const int totalHorizontalDistance =
        currentX - m_dragPressPosition.x();

    if (
        !m_dragScrolling
        && std::abs(totalHorizontalDistance)
            >= QApplication::startDragDistance()
        )
    {
        m_dragScrolling = true;
        setCursor(
            Qt::ClosedHandCursor
            );
    }

    const int horizontalDistance =
        currentX - m_lastDragX;
    m_lastDragX =
        currentX;

    if (m_dragScrolling)
    {
        scrollForDragDistance(
            horizontalDistance
            );
    }

    event->accept();
}

void UniformWidthTabBar::mousePressEvent(
    QMouseEvent* event
    )
{
    QToolButton* leftButton =
        scrollButton("ScrollLeftButton");
    QToolButton* rightButton =
        scrollButton("ScrollRightButton");
    const bool canDragScroll =
        event
        && event->button() == Qt::LeftButton
        && isEnabled()
        && leftButton
        && rightButton
        && leftButton->isVisible()
        && rightButton->isVisible();

    if (!canDragScroll)
    {
        QTabBar::mousePressEvent(event);
        return;
    }

    m_dragScrollCandidate = true;
    m_dragScrolling = false;
    m_dragPressPosition =
        event->position().toPoint();
    m_lastDragX =
        m_dragPressPosition.x();
    m_dragScrollRemainder = 0;
    event->accept();
}

void UniformWidthTabBar::mouseReleaseEvent(
    QMouseEvent* event
    )
{
    if (
        !event
        || event->button() != Qt::LeftButton
        || !m_dragScrollCandidate
        )
    {
        QTabBar::mouseReleaseEvent(event);
        return;
    }

    const bool wasDragScrolling =
        m_dragScrolling;

    m_dragScrollCandidate = false;
    m_dragScrolling = false;
    m_dragScrollRemainder = 0;
    unsetCursor();

    if (wasDragScrolling)
    {
        event->accept();
        return;
    }

    const QPoint globalPressPosition =
        mapToGlobal(
            m_dragPressPosition
            );
    QMouseEvent replayedPress(
        QEvent::MouseButtonPress,
        QPointF(m_dragPressPosition),
        QPointF(globalPressPosition),
        Qt::LeftButton,
        Qt::LeftButton,
        event->modifiers(),
        event->pointingDevice()
        );
    QTabBar::mousePressEvent(
        &replayedPress
        );

    QMouseEvent replayedRelease(
        QEvent::MouseButtonRelease,
        event->position(),
        event->globalPosition(),
        Qt::LeftButton,
        Qt::NoButton,
        event->modifiers(),
        event->pointingDevice()
        );
    QTabBar::mouseReleaseEvent(
        &replayedRelease
        );
    event->setAccepted(
        replayedRelease.isAccepted()
        );
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
    scheduleScrollControlRefresh();
}

void UniformWidthTabBar::showEvent(
    QShowEvent* event
    )
{
    QTabBar::showEvent(event);
    scheduleScrollControlRefresh();
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

    QSize hint = NavigationPillStyle::sizeHint(
        fontMetrics(),
        icon,
        tabText(index),
        iconSize,
        NavigationPillStyle::Gap
        );
    hint.setHeight(NavigationPillStyle::ControlHeight);

    return hint;
}

void UniformWidthTabBar::paintNavigationPills()
{
    QPainter painter(this);

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
            m_currentSelectionVisible
            && option.state.testFlag(
                QStyle::State_Selected
                );
        const bool hovered =
            option.state.testFlag(
                QStyle::State_MouseOver
                );
        NavigationPillStyle::paint(
            &painter,
            tabRect(index),
            tabText(index),
            tabIcon(index),
            font(),
            elideMode(),
            style(),
            this,
            option.palette,
            {
                enabled,
                selected,
                hovered,
                isActiveWindow()
            },
            {
                m_navigationTabFillColor,
                m_navigationTabBorderColor,
                m_navigationTabHoverColor,
                m_navigationTabSelectedColor,
                m_navigationTabTextColor
            },
            NavigationPillStyle::Gap
            );
    }
}

void UniformWidthTabBar::scheduleScrollControlRefresh()
{
    if (m_scrollControlRefreshScheduled)
    {
        return;
    }

    m_scrollControlRefreshScheduled =
        true;

    QTimer::singleShot(
        0,
        this,
        [this]
        {
            refreshScrollControls();
            m_scrollControlRefreshScheduled =
                false;
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

    if (
        !leftButton
        || !rightButton
        || !leftButton->isVisible()
        || !rightButton->isVisible()
        )
    {
        return;
    }

    removeTrailingGap(
        leftButton,
        rightButton
        );
}

void UniformWidthTabBar::scrollForDragDistance(
    int horizontalDistance
    )
{
    m_dragScrollRemainder +=
        horizontalDistance;

    QToolButton* leftButton =
        scrollButton("ScrollLeftButton");
    QToolButton* rightButton =
        scrollButton("ScrollRightButton");

    while (std::abs(m_dragScrollRemainder) >= DragScrollStep)
    {
        const bool draggingRight =
            m_dragScrollRemainder > 0;
        QToolButton* button =
            draggingRight
                ? leftButton
                : rightButton;

        if (
            !button
            || !button->isVisible()
            || !button->isEnabled()
            )
        {
            m_dragScrollRemainder = 0;
            break;
        }

        button->click();
        m_dragScrollRemainder +=
            draggingRight
                ? -DragScrollStep
                : DragScrollStep;
    }
}

void UniformWidthTabBar::removeTrailingGap(
    QToolButton* leftButton,
    QToolButton* rightButton
    )
{
    if (count() <= 0)
    {
        return;
    }

    const int rightEdge =
        rightButton->x();

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
    m_tabAlignment =
        kind == UniformWidthTabKind::Class
        || kind == UniformWidthTabKind::Section
            ? UniformWidthTabAlignment::Center
            : UniformWidthTabAlignment::Leading;

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
    scheduleAlignTabBar();
}

UniformWidthTabAlignment UniformWidthTabWidget::tabAlignment() const
{
    return m_tabAlignment;
}

void UniformWidthTabWidget::configureNavigationSettingsButton(
    QPushButton* button
    )
{
    if (!button)
    {
        return;
    }

    button->setProperty("role", QStringLiteral("icon_button"));
    button->setFixedWidth(NavigationSettingsButtonWidth);
    button->setText(QStringLiteral("\u2699"));
    button->setFont(
        FontManager::getUiFont(
            18,
            QFont::DemiBold
            )
        );
    button->setFixedHeight(NavigationPillStyle::ControlHeight);
}

void UniformWidthTabWidget::setTabAlignment(
    UniformWidthTabAlignment alignment
    )
{
    if (m_tabAlignment == alignment)
    {
        return;
    }

    m_tabAlignment = alignment;
    scheduleAlignTabBar();
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
            )
        )
    {
        const auto* uniformTabBar =
            qobject_cast<const UniformWidthTabBar*>(
                tabBar()
                );

        if (
            isVisible()
            && uniformTabBar
            && uniformTabBar->naturalWidth() <= width()
            )
        {
            alignTabBar();
        }

        scheduleAlignTabBar();
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
    scheduleAlignTabBar();
}

void UniformWidthTabWidget::showEvent(
    QShowEvent* event
    )
{
    QTabWidget::showEvent(event);
    scheduleAlignTabBar();
}

void UniformWidthTabWidget::tabInserted(
    int index
    )
{
    QTabWidget::tabInserted(index);
    scheduleAlignTabBar();
}

void UniformWidthTabWidget::tabRemoved(
    int index
    )
{
    QTabWidget::tabRemoved(index);
    scheduleAlignTabBar();
}

void UniformWidthTabWidget::scheduleAlignTabBar()
{
    if (m_alignmentScheduled)
    {
        return;
    }

    m_alignmentScheduled =
        true;

    QTimer::singleShot(
        0,
        this,
        [this]
        {
            alignTabBar();
            m_alignmentScheduled =
                false;
        }
        );
}

void UniformWidthTabWidget::alignTabBar()
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

    const bool tabsAtBottom =
        tabPosition() == QTabWidget::South;
    const Qt::Corner leadingCorner =
        tabsAtBottom
            ? Qt::BottomLeftCorner
            : Qt::TopLeftCorner;
    const Qt::Corner trailingCorner =
        tabsAtBottom
            ? Qt::BottomRightCorner
            : Qt::TopRightCorner;
    const auto cornerWidth =
        [](const QWidget* corner)
        {
            if (!corner)
            {
                return 0;
            }

            return std::max(
                corner->width(),
                corner->sizeHint().width()
                );
        };
    const int leadingCornerWidth =
        cornerWidth(cornerWidget(leadingCorner));
    const int trailingCornerWidth =
        cornerWidth(cornerWidget(trailingCorner));
    const int availableWidth =
        width() - leadingCornerWidth - trailingCornerWidth;
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
    const int alignedX =
        m_tabAlignment == UniformWidthTabAlignment::Center
            ? leadingCornerWidth
                + ((availableWidth - centeredWidth) / 2)
            : leadingCornerWidth;

    if (
        geometry.x() == alignedX
        && geometry.width() == centeredWidth
        )
    {
        return;
    }

    geometry.setX(
        alignedX
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
