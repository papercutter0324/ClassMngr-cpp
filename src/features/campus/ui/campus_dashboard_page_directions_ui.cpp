#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"

#include <QFormLayout>

namespace Detail = CampusDashboardPageDetail;

QWidget* CampusDashboardPage::createDirectionsTab()
{
    QFormLayout* form = nullptr;

    QWidget* tab =
        Detail::createScrollContainer(
            this,
            &form
            );

    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_transitStepsEdit =
        addTextField(
            form,
            QT_TR_NOOP("Transit Steps:"),
            5,
            10
            );

    m_arrivalInfoEdit =
        addTextField(
            form,
            QT_TR_NOOP("Upon Arriving:"),
            5,
            10
            );

    m_directionsNoteEdit =
        addLineField(
            form,
            QT_TR_NOOP("Note:")
            );

    return tab;
}
