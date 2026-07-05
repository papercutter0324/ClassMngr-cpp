#include "campus_dashboard_page.h"

#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/no_wheel_combobox.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>

void CampusDashboardPage::buildUi()
{
    contentLayout()->setContentsMargins(
        12,
        12,
        12,
        0
        );

    auto* selectorLayout =
        new QHBoxLayout;

    selectorLayout->setSpacing(8);

    auto* selectorLabel =
        createTranslatableLabel(
            QT_TR_NOOP("Campus:"),
            this
            );

    m_campusCombo =
        new NoWheelComboBox(this);
    WidgetSizing::installTextAwareFieldWidth(
        m_campusCombo,
        UiConstants::Forms::FieldMinimumWidth,
        QSizePolicy::Maximum
        );

    selectorLayout->addWidget(selectorLabel);
    selectorLayout->addWidget(
        m_campusCombo
        );

    if (m_adminMode)
    {
        auto* campusNameLabel =
            createTranslatableLabel(
                QT_TR_NOOP("Campus Name:"),
                this
                );

        m_campusNameEdit =
            new QLineEdit(this);

        m_campusNameEdit->setMinimumWidth(180);
        m_campusNameEdit->setMaximumWidth(260);
        m_lineEdits.append(m_campusNameEdit);

        selectorLayout->addWidget(campusNameLabel);
        selectorLayout->addWidget(m_campusNameEdit);

        connect(
            m_campusNameEdit,
            &QLineEdit::textEdited,
            this,
            [this](const QString& text)
            {
                if (m_nameEdit)
                {
                    const QSignalBlocker blocker(m_nameEdit);
                    m_nameEdit->setText(text);
                }

                if (
                    m_campusCombo
                    && m_campusCombo->currentIndex() >= 0
                    )
                {
                    const QSignalBlocker blocker(m_campusCombo);

                    m_campusCombo->setItemText(
                        m_campusCombo->currentIndex(),
                        text
                        );
                    updateCampusSelectorWidth();
                }

                handleFieldEdited();
            }
            );

        connect(
            m_campusNameEdit,
            &QLineEdit::editingFinished,
            this,
            &CampusDashboardPage::normalizeCampusNameField
            );

        auto* campusCodeLabel =
            createTranslatableLabel(
                QT_TR_NOOP("Campus Code:"),
                this
                );

        m_campusCodeEdit =
            new QLineEdit(this);

        m_campusCodeEdit->setMaximumWidth(120);
        m_lineEdits.append(m_campusCodeEdit);

        selectorLayout->addWidget(campusCodeLabel);
        selectorLayout->addWidget(m_campusCodeEdit);

        connect(
            m_campusCodeEdit,
            &QLineEdit::textEdited,
            this,
            [this]()
            {
                handleFieldEdited();
            }
            );
    }

    selectorLayout->addStretch();

    if (m_adminMode)
    {
        m_newCampusButton =
            new TextFitPushButton(
                tr("New Campus"),
                this
                );

        m_saveCampusButton =
            new TextFitPushButton(
                tr("Save Campus"),
                this
                );

        m_saveCampusButton->setObjectName("primaryButton");
        m_saveCampusButton->setEnabled(false);

        selectorLayout->addWidget(m_newCampusButton);
        selectorLayout->addWidget(m_saveCampusButton);

        connect(
            m_newCampusButton,
            &QPushButton::clicked,
            this,
            &CampusDashboardPage::handleNewCampus
            );

        connect(
            m_saveCampusButton,
            &QPushButton::clicked,
            this,
            &CampusDashboardPage::handleManualCampusSave
            );

        updateCampusSaveButton();
    }

    contentLayout()->addLayout(selectorLayout);

    m_tabs =
        new QTabWidget(this);

    m_informationTab =
        createInformationTab();

    m_addressTab =
        createAddressTab();

    m_directionsTab =
        createDirectionsTab();

    m_housingTab =
        createHousingTab();

    m_mapTab =
        createMapTab();

    m_tabs->addTab(
        m_informationTab,
        tr("Information")
        );

    m_tabs->addTab(
        m_directionsTab,
        tr("Directions")
        );

    m_tabs->addTab(
        m_addressTab,
        tr("Address")
        );

    m_tabs->addTab(
        m_housingTab,
        tr("Housing")
        );

    m_tabs->addTab(
        m_mapTab,
        tr("Maps")
        );

    contentLayout()->addWidget(
        m_tabs,
        1
        );

    m_statusLabel =
        new QLabel(this);

    m_statusLabel->setVisible(m_adminMode);

    bottomLayout()->addWidget(m_statusLabel);
    bottomLayout()->addStretch();

    connect(
        m_campusCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this](int)
        {
            loadSelectedCampus();
        }
        );

    connect(
        m_tabs,
        &QTabWidget::currentChanged,
        this,
        [this](int)
        {
            emitCurrentSectionChanged();
        }
        );
}

void CampusDashboardPage::applyAdminMode()
{
    const bool readOnly =
        !m_adminMode;

    for (QLineEdit* edit : m_lineEdits)
    {
        edit->setReadOnly(readOnly);
    }

    for (QPlainTextEdit* edit : m_textEdits)
    {
        edit->setReadOnly(readOnly);
    }

    for (QLineEdit* edit : m_alwaysReadOnlyLineEdits)
    {
        edit->setReadOnly(true);
    }

    for (QPlainTextEdit* edit : m_alwaysReadOnlyTextEdits)
    {
        edit->setReadOnly(true);
    }

    if (m_addHousingButton)
    {
        m_addHousingButton->setVisible(m_adminMode);
    }

    if (m_printerDriverUrlUnavailableCheck)
    {
        m_printerDriverUrlUnavailableCheck->setEnabled(m_adminMode);
    }

    updatePrinterDriverUrlState();

    if (m_photocopierCodeUnavailableCheck)
    {
        m_photocopierCodeUnavailableCheck->setEnabled(m_adminMode);
    }

    updatePhotocopierCodeState();

    updateHousingRemoveButtonVisibility();
}
