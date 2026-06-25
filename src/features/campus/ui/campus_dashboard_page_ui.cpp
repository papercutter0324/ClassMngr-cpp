#include "campus_dashboard_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "campus_dashboard_page_detail.h"
#include "core/fontmanager.h"
#include "features/campus/ui/campus_map_preview.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/utils/widget_sizing.h"

#include <QCheckBox>
#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
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

#include <algorithm>
#include <utility>

namespace Detail = CampusDashboardPageDetail;

QWidget* CampusDashboardPage::createAddressTab()
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

    m_nameEdit =
        new QLineEdit(container);

    m_lineEdits.append(m_nameEdit);
    m_nameEdit->hide();

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

    insertFormRow(
        m_directionsEnglishAddress.form,
        0,
        QT_TR_NOOP("Building Name:"),
        m_buildingEdit
        );

    insertFormRow(
        m_directionsEnglishAddress.summaryForm,
        1,
        QT_TR_NOOP("Phone Number:"),
        m_phoneEdit
        );

    m_directionsEnglishAddress.line2Suffix =
        m_buildingEdit;
    m_directionsEnglishAddress.componentFields.append(
        m_buildingEdit
        );

    m_directionsKoreanAddress =
        createAddressSection(
            container,
            true
            );

    m_buildingKrEdit =
        new QLineEdit(m_directionsKoreanAddress.container);

    m_phoneKrEdit =
        new QLineEdit(m_directionsKoreanAddress.container);

    const QFont koreanFont =
        FontManager::getKoreanFont();

    m_buildingKrEdit->setFont(koreanFont);
    m_phoneKrEdit->setFont(koreanFont);

    m_lineEdits.append(m_buildingKrEdit);
    m_lineEdits.append(m_phoneKrEdit);

    insertFormRow(
        m_directionsKoreanAddress.form,
        0,
        QT_TR_NOOP("Building Name:"),
        m_buildingKrEdit
        );

    insertFormRow(
        m_directionsKoreanAddress.summaryForm,
        1,
        QT_TR_NOOP("Phone Number:"),
        m_phoneKrEdit
        );

    m_directionsKoreanAddress.line2Suffix =
        m_buildingKrEdit;
    m_directionsKoreanAddress.componentFields.append(
        m_buildingKrEdit
        );
    alignAddressDetailsWithCompleteField(
        &m_directionsEnglishAddress
        );
    alignAddressDetailsWithCompleteField(
        &m_directionsKoreanAddress
        );
    hideAddressComponents(
        &m_directionsEnglishAddress,
        &m_directionsKoreanAddress
        );

    layout->addWidget(m_directionsEnglishAddress.container);
    layout->addWidget(m_directionsKoreanAddress.container);
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

    showDirectionsLanguage(true);

    return tab;
}

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
        new TextFitPushButton(
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

QLabel* CampusDashboardPage::createTranslatableLabel(
    const char* sourceText,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(
            tr(sourceText),
            parent
            );

    TranslatableLabel entry;
    entry.label = label;
    entry.sourceText = sourceText;
    m_translatableLabels.append(entry);

    return label;
}

void CampusDashboardPage::addFormRow(
    QFormLayout* form,
    const char* labelText,
    QWidget* field
    )
{
    if (!form)
    {
        return;
    }

    form->addRow(
        createTranslatableLabel(
            labelText,
            form->parentWidget()
            ),
        field
        );
}

void CampusDashboardPage::insertFormRow(
    QFormLayout* form,
    int row,
    const char* labelText,
    QWidget* field
    )
{
    if (!form)
    {
        return;
    }

    form->insertRow(
        row,
        createTranslatableLabel(
            labelText,
            form->parentWidget()
            ),
        field
        );
}

void CampusDashboardPage::retranslateRegisteredLabels()
{
    for (const TranslatableLabel& entry : std::as_const(m_translatableLabels))
    {
        if (
            entry.label
            && entry.sourceText
            )
        {
            entry.label->setText(
                tr(entry.sourceText)
            );
        }
    }

    alignAllAddressDetailsWithCompleteFields();
}

void CampusDashboardPage::alignAddressDetailsWithCompleteField(
    AddressSectionWidgets* section
    ) const
{
    if (
        !section
        || !section->form
        || !section->summaryForm
        )
    {
        return;
    }

    int summaryLabelColumnWidth = 0;

    for (int row = 0; row < section->summaryForm->rowCount(); ++row)
    {
        QLayoutItem* item =
            section->summaryForm->itemAt(
                row,
                QFormLayout::LabelRole
                );

        if (auto* label =
                item
                    ? qobject_cast<QLabel*>(item->widget())
                    : nullptr)
        {
            summaryLabelColumnWidth =
                std::max(
                    summaryLabelColumnWidth,
                    label->sizeHint().width()
                    );
        }
    }

    if (summaryLabelColumnWidth <= 0)
    {
        return;
    }

    int horizontalSpacing =
        section->summaryForm->horizontalSpacing();

    if (horizontalSpacing < 0)
    {
        horizontalSpacing =
            section->summaryForm->spacing();
    }

    const int leftMargin =
        summaryLabelColumnWidth
        + std::max(
            0,
            horizontalSpacing
            );

    section->form->setContentsMargins(
        leftMargin,
        0,
        0,
        0
        );
}

void CampusDashboardPage::alignAllAddressDetailsWithCompleteFields()
{
    alignAddressDetailsWithCompleteField(&m_directionsEnglishAddress);
    alignAddressDetailsWithCompleteField(&m_directionsKoreanAddress);

    for (HousingSectionWidgets& section : m_housingSections)
    {
        alignAddressDetailsWithCompleteField(&section.english);
        alignAddressDetailsWithCompleteField(&section.korean);
    }
}

QLineEdit* CampusDashboardPage::addLineField(
    QFormLayout* form,
    const char* labelText
    )
{
    auto* edit =
        new QLineEdit(this);

    edit->setMinimumWidth(280);

    addFormRow(
        form,
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
    const char* labelText,
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

    addFormRow(
        form,
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
        (metrics.lineSpacing() * visibleLines * 6) / 5
            + framePadding
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
        0,
        0,
        0,
        0
        );

    section.form->setSpacing(8);
    section.form->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    auto* completeAddressContainer =
        new QWidget(section.container);

    completeAddressContainer->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    auto* completeForm =
        new QFormLayout(completeAddressContainer);
    section.summaryForm =
        completeForm;

    completeForm->setContentsMargins(0, 0, 0, 0);
    completeForm->setSpacing(8);
    completeForm->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    section.complete =
        new QPlainTextEdit(completeAddressContainer);

    if (koreanAddress)
    {
        section.complete->setFont(
            FontManager::getKoreanFont()
            );
    }

    section.complete->setReadOnly(true);
    section.complete->setMinimumWidth(280);
    section.complete->setLineWrapMode(QPlainTextEdit::NoWrap);
    section.complete->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    section.complete->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    configureExpandingTextField(
        section.complete,
        1,
        8
        );
    section.complete->setFixedHeight(
        CompleteAddressMinimumHeight
        );

    section.toggleLanguageButton =
        new TextFitPushButton(
            koreanAddress
                ? tr("Show English")
                : tr("Show Korean"),
            completeAddressContainer
            );

    section.toggleAddressSystemButton =
        new TextFitPushButton(
            tr("Show Classic"),
            completeAddressContainer
            );

    section.toggleAddressComponentsButton =
        new TextFitPushButton(
            tr("Show Details"),
            completeAddressContainer
            );

    Detail::setStaticToggleButtonWidths(
        section.toggleLanguageButton,
        section.toggleAddressSystemButton,
        section.toggleAddressComponentsButton,
        {
            tr("Show English"),
            tr("Show Korean"),
            tr("Show Modern"),
            tr("Show Classic"),
            tr("Show Details"),
            tr("Hide Details")
        }
        );

    auto* completeRow =
        new QWidget(completeAddressContainer);

    completeRow->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    auto* completeLayout =
        new QHBoxLayout(completeRow);

    completeLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    completeLayout->setSpacing(8);
    section.completeControls =
        new QWidget(completeRow);

    section.completeControls->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Expanding
        );

    auto* controlsLayout =
        new QVBoxLayout(section.completeControls);

    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);
    controlsLayout->setAlignment(Qt::AlignTop);
    controlsLayout->addWidget(
        section.toggleLanguageButton,
        0,
        Qt::AlignTop | Qt::AlignRight
        );
    controlsLayout->addWidget(
        section.toggleAddressSystemButton,
        0,
        Qt::AlignTop | Qt::AlignRight
        );
    controlsLayout->addWidget(
        section.toggleAddressComponentsButton,
        0,
        Qt::AlignTop | Qt::AlignRight
        );

    completeLayout->addWidget(section.complete, 1);
    completeLayout->addWidget(
        section.completeControls,
        0,
        Qt::AlignTop
        );

    addFormRow(
        completeForm,
        QT_TR_NOOP("Complete Address:"),
        completeRow
        );

    sectionLayout->addWidget(completeAddressContainer);

    m_alwaysReadOnlyTextEdits.append(section.complete);

    connect(
        section.toggleLanguageButton,
        &QPushButton::clicked,
        this,
        [this, button = section.toggleLanguageButton]()
        {
            handleAddressLanguageToggle(button);
        }
        );

    connect(
        section.toggleAddressSystemButton,
        &QPushButton::clicked,
        this,
        [this, button = section.toggleAddressSystemButton]()
        {
            handleAddressSystemToggle(button);
        }
        );

    connect(
        section.toggleAddressComponentsButton,
        &QPushButton::clicked,
        this,
        [this, button = section.toggleAddressComponentsButton]()
        {
            handleAddressComponentsToggle(button);
        }
        );

    if (includeBuildingName)
    {
        section.buildingName =
            addLineField(
                section.form,
                QT_TR_NOOP("Building Name:")
                );

        section.line2Suffix =
            section.buildingName;
    }

    section.province =
        addLineField(
            section.form,
            QT_TR_NOOP("Province:")
            );

    section.city =
        addLineField(
            section.form,
            QT_TR_NOOP("City:")
            );

    section.district =
        addLineField(
            section.form,
            QT_TR_NOOP("District:")
            );

    section.line1 =
        addLineField(
            section.form,
            QT_TR_NOOP("Address Line 1:")
            );

    section.line2 =
        addLineField(
            section.form,
            QT_TR_NOOP("Address Line 2:")
            );

    section.postalCode =
        addLineField(
            section.form,
            QT_TR_NOOP("Postal Code:")
            );

    section.componentFields = {
        section.buildingName,
        section.province,
        section.city,
        section.district,
        section.line1,
        section.line2,
        section.postalCode
    };

    section.componentFields.removeAll(nullptr);

    alignAddressDetailsWithCompleteField(
        &section
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

    if (koreanAddress)
    {
        const QFont koreanFont =
            FontManager::getKoreanFont();

        for (QLineEdit* edit : {
                 section.buildingName,
                 section.province,
                 section.city,
                 section.district,
                 section.line1,
                 section.line2,
                 section.postalCode
             })
        {
            if (edit)
            {
                edit->setFont(koreanFont);
            }
        }
    }

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
