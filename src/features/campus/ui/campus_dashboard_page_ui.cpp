#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTextDocument>
#include <QVBoxLayout>

namespace Detail = CampusDashboardPageDetail;

QWidget* CampusDashboardPage::createDirectionsTab()
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

    layout->setSpacing(10);

    auto* campusForm =
        new QFormLayout;

    campusForm->setContentsMargins(
        0,
        0,
        0,
        0
        );

    campusForm->setSpacing(10);
    campusForm->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    auto* campusRow =
        new QWidget(container);

    auto* campusRowLayout =
        new QHBoxLayout(campusRow);

    campusRowLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    campusRowLayout->setSpacing(8);

    m_nameEdit =
        new QLineEdit(campusRow);

    m_directionsToggleLanguageButton =
        new QPushButton(
            tr("Show Korean"),
            campusRow
            );

    Detail::setStaticToggleButtonWidth(m_directionsToggleLanguageButton);

    m_lineEdits.append(m_nameEdit);

    campusRowLayout->addWidget(m_nameEdit, 1);
    campusRowLayout->addWidget(m_directionsToggleLanguageButton);

    campusForm->addRow(
        tr("Campus Name:"),
        campusRow
        );

    layout->addLayout(campusForm);

    m_directionsEnglishAddress =
        createAddressSection(
            container,
            false
            );

    m_buildingEdit =
        new QLineEdit(m_directionsEnglishAddress.container);

    m_phoneEdit =
        new QLineEdit(m_directionsEnglishAddress.container);

    m_lineEdits.append(m_buildingEdit);
    m_lineEdits.append(m_phoneEdit);

    m_directionsEnglishAddress.form->insertRow(
        1,
        tr("Building Name:"),
        m_buildingEdit
        );

    m_directionsEnglishAddress.form->insertRow(
        2,
        tr("Phone Number:"),
        m_phoneEdit
        );

    m_directionsEnglishAddress.line2Suffix =
        m_buildingEdit;

    m_directionsKoreanAddress =
        createAddressSection(
            container,
            true
            );

    m_buildingKrEdit =
        new QLineEdit(m_directionsKoreanAddress.container);

    m_phoneKrEdit =
        new QLineEdit(m_directionsKoreanAddress.container);

    m_lineEdits.append(m_buildingKrEdit);
    m_lineEdits.append(m_phoneKrEdit);

    m_directionsKoreanAddress.form->insertRow(
        1,
        tr("Building Name:"),
        m_buildingKrEdit
        );

    m_directionsKoreanAddress.form->insertRow(
        2,
        tr("Phone Number:"),
        m_phoneKrEdit
        );

    m_directionsKoreanAddress.line2Suffix =
        m_buildingKrEdit;

    layout->addWidget(m_directionsEnglishAddress.container);
    layout->addWidget(m_directionsKoreanAddress.container);

    auto* transitForm =
        new QFormLayout;

    transitForm->setContentsMargins(
        0,
        0,
        0,
        0
        );

    transitForm->setSpacing(10);
    transitForm->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    m_transitStepsEdit =
        addTextField(
            transitForm,
            tr("Transit Steps:"),
            5,
            10
            );

    m_arrivalInfoEdit =
        addTextField(
            transitForm,
            tr("Upon Arriving:"),
            5,
            10
            );

    layout->addLayout(transitForm);
    layout->addStretch();

    scroll->setWidget(container);
    root->addWidget(scroll);

    connect(
        m_nameEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_nameEdit,
        &QLineEdit::editingFinished,
        this,
        &CampusDashboardPage::normalizeCampusNameField
        );

    connect(
        m_buildingEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_buildingKrEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        m_phoneEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            syncPhoneFields(m_phoneEdit);
            handleFieldEdited();
        }
        );

    connect(
        m_phoneKrEdit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            syncPhoneFields(m_phoneKrEdit);
            handleFieldEdited();
        }
        );

    connect(
        m_directionsToggleLanguageButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            showDirectionsLanguage(!m_directionsShowingEnglish);
        }
        );

    showDirectionsLanguage(true);

    return tab;
}

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
            tr("Office Number:")
            );

    m_officeWifiEdit =
        addLineField(
            form,
            tr("Office WiFi:")
            );

    m_officeWifiPasswordEdit =
        addLineField(
            form,
            tr("WiFi Password:")
            );

    m_printerNameEdit =
        addLineField(
            form,
            tr("Printer Name:")
            );

    m_printerStepsEdit =
        addTextField(
            form,
            tr("Printer Installation Steps:"),
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

    form->addRow(
        tr("Printer Driver URL:"),
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

    form->addRow(
        tr("Photocopier Code:"),
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

QWidget* CampusDashboardPage::createHousingTab()
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

    container->setFrameShape(QFrame::NoFrame);
    container->setFrameShadow(QFrame::Plain);

    auto* containerLayout =
        new QVBoxLayout(container);

    containerLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    containerLayout->setSpacing(10);

    m_housingSectionsLayout =
        new QVBoxLayout;

    m_housingSectionsLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    m_housingSectionsLayout->setSpacing(12);

    m_housingEmptyLabel =
        new QLabel(
            tr("No housing information available"),
            container
            );

    m_housingSectionsLayout->addWidget(m_housingEmptyLabel);
    m_housingSectionsLayout->addStretch();

    containerLayout->addLayout(m_housingSectionsLayout, 1);

    scroll->setWidget(container);
    root->addWidget(scroll);

    m_addHousingButton =
        new QPushButton(
            tr("Add Housing Location"),
            container
            );

    m_addHousingButton->setVisible(m_adminMode);

    auto* buttonLayout =
        new QHBoxLayout;

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_addHousingButton);

    containerLayout->addLayout(buttonLayout);

    connect(
        m_addHousingButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            addHousingSectionFromJson(Detail::emptyHousingLocation());
            updateHousingCompleteAddresses();
            m_dirty = true;
            scheduleSave();
        }
        );

    return tab;
}

QWidget* CampusDashboardPage::createMapTab()
{
    QFormLayout* form = nullptr;

    QWidget* tab =
        Detail::createScrollContainer(
            this,
            &form
            );

    m_imagePathEdit =
        addLineField(
            form,
            tr("Image Path:")
            );

    m_mapPreviewLabel =
        new QLabel(tab);

    m_mapPreviewLabel->setAlignment(
        Qt::AlignCenter
        );

    m_mapPreviewLabel->setMinimumHeight(260);
    m_mapPreviewLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    form->addRow(
        tr("Preview:"),
        m_mapPreviewLabel
        );

    return tab;
}

QLineEdit* CampusDashboardPage::addLineField(
    QFormLayout* form,
    const QString& labelText
    )
{
    auto* edit =
        new QLineEdit(this);

    edit->setMinimumWidth(280);

    form->addRow(
        labelText,
        edit
        );

    m_lineEdits.append(edit);

    connect(
        edit,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    return edit;
}

QPlainTextEdit* CampusDashboardPage::addTextField(
    QFormLayout* form,
    const QString& labelText,
    int minimumLines,
    int maximumLines
    )
{
    auto* edit =
        new QPlainTextEdit(this);

    edit->setTabChangesFocus(true);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    configureExpandingTextField(
        edit,
        minimumLines,
        maximumLines
        );

    form->addRow(
        labelText,
        edit
        );

    m_textEdits.append(edit);

    connect(
        edit,
        &QPlainTextEdit::textChanged,
        this,
        [this, edit]()
        {
            updateTextFieldHeight(edit);
            handleFieldEdited();
        }
        );

    return edit;
}

void CampusDashboardPage::configureExpandingTextField(
    QPlainTextEdit* edit,
    int minimumLines,
    int maximumLines
    )
{
    if (!edit)
    {
        return;
    }

    edit->setProperty(
        "minimumVisibleLines",
        minimumLines
        );

    edit->setProperty(
        "maximumVisibleLines",
        maximumLines
        );

    updateTextFieldHeight(edit);
}

void CampusDashboardPage::updateTextFieldHeight(
    QPlainTextEdit* edit
    )
{
    if (!edit)
    {
        return;
    }

    const int minimumLines =
        edit
            ->property("minimumVisibleLines")
            .toInt();

    const int maximumLines =
        edit
            ->property("maximumVisibleLines")
            .toInt();

    const int blockCount =
        edit->document()
            ? edit->document()->blockCount()
            : minimumLines;

    const int visibleLines =
        qBound(
            minimumLines,
            blockCount,
            maximumLines
            );

    const QFontMetrics metrics(edit->font());

    const int framePadding =
        edit->frameWidth() * 2 + 12;

    edit->setFixedHeight(
        (metrics.lineSpacing() * visibleLines * 6) / 5 + framePadding
        );
}

CampusDashboardPage::AddressSectionWidgets
CampusDashboardPage::createAddressSection(
    QWidget* parent,
    bool koreanAddress,
    bool includeBuildingName
    )
{
    AddressSectionWidgets section;

    section.koreanAddress =
        koreanAddress;

    section.container =
        new QWidget(parent);

    auto* sectionLayout =
        new QVBoxLayout(section.container);

    sectionLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    sectionLayout->setSpacing(8);

    auto* addressContainer =
        new QWidget(section.container);

    section.form =
        new QFormLayout(addressContainer);

    section.form->setContentsMargins(
        24,
        0,
        0,
        0
        );

    section.form->setSpacing(8);
    section.form->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    section.complete =
        new QPlainTextEdit(section.container);

    section.complete->setReadOnly(true);
    section.complete->setMinimumWidth(280);
    section.complete->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    section.complete->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    configureExpandingTextField(
        section.complete,
        1,
        8
        );

    section.toggleAddressSystemButton =
        new QPushButton(
            tr("Show Classic"),
            addressContainer
            );

    Detail::setStaticToggleButtonWidth(section.toggleAddressSystemButton);

    auto* completeRow =
        new QWidget(addressContainer);

    auto* completeLayout =
        new QHBoxLayout(completeRow);

    completeLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    completeLayout->setSpacing(8);
    completeLayout->addWidget(section.complete, 1);
    completeLayout->addWidget(
        section.toggleAddressSystemButton,
        0,
        Qt::AlignTop
        );

    section.form->addRow(
        tr("Complete Address:"),
        completeRow
        );

    m_alwaysReadOnlyTextEdits.append(section.complete);

    connect(
        section.toggleAddressSystemButton,
        &QPushButton::clicked,
        this,
        [this, button = section.toggleAddressSystemButton]()
        {
            handleAddressSystemToggle(button);
        }
        );

    if (includeBuildingName)
    {
        section.buildingName =
            addLineField(
                section.form,
                tr("Building Name:")
                );

        section.line2Suffix =
            section.buildingName;
    }

    section.province =
        addLineField(
            section.form,
            tr("Province:")
            );

    section.city =
        addLineField(
            section.form,
            tr("City:")
            );

    section.district =
        addLineField(
            section.form,
            tr("District:")
            );

    section.line1 =
        addLineField(
            section.form,
            tr("Address Line 1:")
            );

    section.line2 =
        addLineField(
            section.form,
            tr("Address Line 2:")
            );

    section.postalCode =
        addLineField(
            section.form,
            tr("Postal Code:")
            );

    section.note =
        addLineField(
            section.form,
            tr("Note:")
            );

    sectionLayout->addWidget(addressContainer);

    auto connectMirroredField =
        [this](QLineEdit* edit, const QString& key)
    {
        if (!edit)
        {
            return;
        }

        connect(
            edit,
            &QLineEdit::textEdited,
            this,
            [this, edit, key](const QString&)
            {
                handleAddressVariantFieldEdited(
                    edit,
                    key
                    );
            }
            );
    };

    connectMirroredField(
        section.buildingName,
        QStringLiteral("building_name")
        );

    connectMirroredField(
        section.province,
        QStringLiteral("province")
        );

    connectMirroredField(
        section.city,
        QStringLiteral("city")
        );

    connectMirroredField(
        section.district,
        QStringLiteral("district")
        );

    connectMirroredField(
        section.line2,
        QStringLiteral("line2")
        );

    return section;
}

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
        new QLabel(
            tr("Campus:"),
            this
            );

    m_campusCombo =
        new QComboBox(this);

    selectorLayout->addWidget(selectorLabel);
    selectorLayout->addWidget(m_campusCombo);

    if (m_adminMode)
    {
        auto* campusCodeLabel =
            new QLabel(
                tr("Campus Code:"),
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
            new QPushButton(
                tr("New Campus"),
                this
                );

        m_saveCampusButton =
            new QPushButton(
                tr("Save Campus"),
                this
                );

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
    }

    contentLayout()->addLayout(selectorLayout);

    m_tabs =
        new QTabWidget(this);

    m_directionsTab =
        createDirectionsTab();

    m_informationTab =
        createInformationTab();

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
        m_housingTab,
        tr("Housing")
        );

    m_tabs->addTab(
        m_mapTab,
        tr("Map")
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
