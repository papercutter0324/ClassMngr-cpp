#include "uniform_width_tab_bar.h"

#include <algorithm>

#include <QEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>

UniformWidthTabBar::UniformWidthTabBar(
    QWidget* parent
    )
    : QTabBar(parent)
{
    setDrawBase(false);
    setExpanding(false);
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

void UniformWidthTabBar::wheelEvent(
    QWheelEvent* event
    )
{
    event->ignore();
}

UniformWidthTabWidget::UniformWidthTabWidget(
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

    setTabBar(
        uniformTabBar
        );
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
    QTabBar* bar =
        tabBar();

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
        bar->sizeHint().width();

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
