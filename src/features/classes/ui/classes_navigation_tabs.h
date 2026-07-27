#pragma once

#include "ui/shared/widgets/uniform_width_tab_bar.h"

class QString;
class QWidget;

namespace ClassesNavigationTabs
{

UniformWidthTabWidget* create(
    UniformWidthTabKind kind,
    const QString& tabBarObjectName,
    QWidget* parent
    );

}
