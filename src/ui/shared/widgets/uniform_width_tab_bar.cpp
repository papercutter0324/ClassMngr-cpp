#include "uniform_width_tab_bar.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyleOption>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

namespace
{

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
        QTabBar::tabSizeHint(index);

    int widestTabWidth =
        hint.width();

    for (int tabIndex = 0; tabIndex < count(); ++tabIndex)
    {
        widestTabWidth =
            std::max(
                widestTabWidth,
                QTabBar::tabSizeHint(tabIndex).width()
                );
    }

    hint.setWidth(
        widestTabWidth
        );

    return hint;
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

    setTabShape(
        QTabWidget::Rounded
        );
    setDocumentMode(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
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
