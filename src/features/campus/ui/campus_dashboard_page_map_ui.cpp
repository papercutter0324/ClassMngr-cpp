#include "campus_dashboard_page.h"

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

QWidget* CampusDashboardPage::createMapTab()
{
    auto* tab =
        new QWidget(this);

    auto* root =
        new QVBoxLayout(tab);

    auto* scroll =
        new QScrollArea(tab);

    scroll->setWidgetResizable(true);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container =
        new QFrame(scroll);

    container->setFrameShape(QFrame::StyledPanel);
    container->setFrameShadow(QFrame::Plain);

    auto* layout =
        new QVBoxLayout(container);

    layout->setContentsMargins(
        12,
        12,
        12,
        12
        );
    layout->setSpacing(16);
    layout->setAlignment(Qt::AlignTop);

    m_mapSectionsLayout = layout;

    scroll->setWidget(container);
    root->addWidget(scroll);

    return tab;
}
