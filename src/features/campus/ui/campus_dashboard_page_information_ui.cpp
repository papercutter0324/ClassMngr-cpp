#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSizePolicy>
#include <QWidget>

namespace Detail = CampusDashboardPageDetail;

QWidget* CampusDashboardPage::createInformationTab()
{
    QFormLayout* form = nullptr;

    QWidget* tab =
        Detail::createScrollContainer(
            this,
            &form
            );

    m_officeNumberEdit =
        addLineField(
            form,
            QT_TR_NOOP("Office Number:")
            );

    m_officeWifiEdit =
        addLineField(
            form,
            QT_TR_NOOP("Office WiFi:")
            );

    m_officeWifiPasswordEdit =
        addLineField(
            form,
            QT_TR_NOOP("WiFi Password:")
            );

    m_printerNameEdit =
        addLineField(
            form,
            QT_TR_NOOP("Printer Name:")
            );

    m_printerStepsEdit =
        addTextField(
            form,
            QT_TR_NOOP("Printer Installation Steps:"),
            5,
            10
            );

    auto* driverRow =
        new QWidget(tab);

    driverRow->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    auto* driverLayout =
        new QHBoxLayout(driverRow);

    driverLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    driverLayout->setAlignment(Qt::AlignTop);

    m_printerDriverUrlEdit =
        new QLineEdit(driverRow);

    m_printerDriverUrlEdit->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    m_printerDriverUrlUnavailableCheck =
        new QCheckBox(
            tr("N/A"),
            driverRow
            );

    m_printerDriverUrlUnavailableCheck->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );

    m_printerDriverUrlUnavailableCheck->setChecked(true);

    m_lineEdits.append(m_printerDriverUrlEdit);

    driverLayout->addWidget(m_printerDriverUrlEdit, 1);
    driverLayout->addWidget(m_printerDriverUrlUnavailableCheck);

    addFormRow(
        form,
        QT_TR_NOOP("Printer Driver URL:"),
        driverRow
        );

    connect(
        m_printerDriverUrlEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_printerDriverUrlUnavailableCheck,
        &QCheckBox::toggled,
        this,
        [this](bool)
        {
            updatePrinterDriverUrlState();
            handleFieldEdited();
        }
        );

    auto* photocopierRow =
        new QWidget(tab);

    photocopierRow->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    auto* photocopierLayout =
        new QHBoxLayout(photocopierRow);

    photocopierLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    photocopierLayout->setAlignment(Qt::AlignTop);

    m_photocopierCodeEdit =
        new QLineEdit(photocopierRow);

    m_photocopierCodeEdit->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    m_photocopierCodeUnavailableCheck =
        new QCheckBox(
            tr("N/A"),
            photocopierRow
            );

    m_photocopierCodeUnavailableCheck->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );

    m_lineEdits.append(m_photocopierCodeEdit);

    photocopierLayout->addWidget(m_photocopierCodeEdit, 1);
    photocopierLayout->addWidget(m_photocopierCodeUnavailableCheck);

    addFormRow(
        form,
        QT_TR_NOOP("Photocopier Code:"),
        photocopierRow
        );

    connect(
        m_photocopierCodeEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_photocopierCodeUnavailableCheck,
        &QCheckBox::toggled,
        this,
        [this](bool)
        {
            updatePhotocopierCodeState();
            handleFieldEdited();
        }
        );

    return tab;
}
