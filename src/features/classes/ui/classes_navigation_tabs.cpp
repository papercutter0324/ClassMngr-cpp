#include "classes_navigation_tabs.h"

#include <QApplication>
#include <QFont>
#include <QTabBar>

namespace ClassesNavigationTabs
{

UniformWidthTabWidget* create(
    UniformWidthTabKind kind,
    const QString& tabBarObjectName,
    QWidget* parent
    )
{
    auto* tabs =
        new UniformWidthTabWidget(
            kind,
            tabBarObjectName,
            parent
            );
    const QFont navigationFont =
        QApplication::font();

    tabs->setFont(
        navigationFont
        );
    tabs->tabBar()->setFont(
        navigationFont
        );
    tabs->setTabAppearance(
        UniformWidthTabAppearance::NavigationPill
        );

    return tabs;
}

}
