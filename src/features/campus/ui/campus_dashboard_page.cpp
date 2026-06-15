#include "campus_dashboard_page.h"

#include "core/appsettings.h"
#include "core/resource_paths.h"
#include "core/settingsmanager.h"
#include "features/campus/ui/campus_map_preview.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
constexpr int AutosaveDebounceMs = 800;

void setStaticToggleButtonWidth(
    QPushButton* button
    )
{
    if (!button)
    {
        return;
    }

    const QFontMetrics metrics(button->font());

    const int width =
        qMax(
            qMax(
                metrics.horizontalAdvance(QStringLiteral("Show Modern")),
                metrics.horizontalAdvance(QStringLiteral("Show Classic"))
                ),
            qMax(
                metrics.horizontalAdvance(QStringLiteral("Show Korean")),
                metrics.horizontalAdvance(QStringLiteral("Show English"))
                )
            ) + 32;

    button->setFixedWidth(width);
    button->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
}

QString jsonString(
    const QJsonObject& object,
    const QString& key
    )
{
    return object.value(key).toString();
}

QStringList transitStepsFromText(
    const QString& text
    )
{
    QStringList steps;

    const QStringList lines =
        text.split(
            u'\n',
            Qt::SkipEmptyParts
            );

    for (const QString& line : lines)
    {
        const QString step =
            line.trimmed();

        if (!step.isEmpty())
        {
            steps.append(step);
        }
    }

    return steps;
}

QJsonObject jsonObject(
    const QJsonObject& object,
    const QString& key
    )
{
    return object.value(key).toObject();
}

QString combinedCityDistrict(
    const QString& city,
    const QString& district
    )
{
    return QStringList{
        city.trimmed(),
        district.trimmed()
        }
        .join(QStringLiteral(" "))
        .simplified();
}

void splitLegacyCityDistrict(
    const QString& cityDistrict,
    QString* city,
    QString* district
    )
{
    if (!city || !district)
    {
        return;
    }

    const QString trimmedCityDistrict =
        cityDistrict.trimmed();

    if (trimmedCityDistrict.isEmpty())
    {
        return;
    }

    const int splitIndex =
        trimmedCityDistrict.lastIndexOf(u' ');

    if (splitIndex > 0)
    {
        *city =
            trimmedCityDistrict.left(splitIndex).trimmed();
        *district =
            trimmedCityDistrict.mid(splitIndex + 1).trimmed();
    }
    else
    {
        *district =
            trimmedCityDistrict;
    }
}

QJsonObject normalizedAddressForUi(
    const QJsonObject& address
    )
{
    QString city =
        jsonString(
            address,
            QStringLiteral("city")
            );

    QString district =
        jsonString(
            address,
            QStringLiteral("district")
            );

    if (city.trimmed().isEmpty() && district.trimmed().isEmpty())
    {
        splitLegacyCityDistrict(
            jsonString(
                address,
                QStringLiteral("city_district")
                ),
            &city,
            &district
            );
    }

    QJsonObject normalized;

    normalized.insert(
        QStringLiteral("province"),
        jsonString(
            address,
            QStringLiteral("province")
            )
        );

    normalized.insert(
        QStringLiteral("city"),
        city
        );

    normalized.insert(
        QStringLiteral("district"),
        district
        );

    normalized.insert(
        QStringLiteral("city_district"),
        combinedCityDistrict(city, district)
        );

    normalized.insert(
        QStringLiteral("line1"),
        jsonString(
            address,
            QStringLiteral("line1")
            )
        );

    normalized.insert(
        QStringLiteral("line2"),
        jsonString(
            address,
            QStringLiteral("line2")
            )
        );

    normalized.insert(
        QStringLiteral("postal_code"),
        jsonString(
            address,
            QStringLiteral("postal_code")
            )
        );

    normalized.insert(
        QStringLiteral("addr_note"),
        jsonString(
            address,
            QStringLiteral("addr_note")
            )
        );

    return normalized;
}

QJsonObject emptyHousingLocation()
{
    QJsonObject housing;

    housing.insert(
        QStringLiteral("name"),
        QString()
        );

    housing.insert(
        QStringLiteral("en"),
        QJsonObject()
        );

    housing.insert(
        QStringLiteral("kr"),
        QJsonObject()
        );

    return housing;
}

QWidget* createScrollContainer(
    QWidget* parent,
    QFormLayout** form
    )
{
    auto* tab =
        new QWidget(parent);

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

    auto* containerLayout =
        new QVBoxLayout(container);

    containerLayout->setContentsMargins(
        12,
        12,
        12,
        12
        );

    auto* formLayout =
        new QFormLayout;

    formLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    formLayout->setSpacing(10);
    formLayout->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    containerLayout->addLayout(formLayout);
    containerLayout->addStretch();
    containerLayout->setStretch(0, 0);
    containerLayout->setStretch(1, 1);
    containerLayout->setAlignment(formLayout, Qt::AlignTop);

    scroll->setWidget(container);
    root->addWidget(scroll);

    *form = formLayout;

    return tab;
}
} // namespace

CampusDashboardPage::CampusDashboardPage(
    bool adminMode,
    QWidget *parent
    )
    : BasePage(parent)
    , m_adminMode(adminMode)
    , m_repository(
        QString::fromUtf8(AppSettings::DefaultCampusDirectory),
        ResourcePaths::Campuses::directory()
        )
{
    buildUi();
    applyAdminMode();

    m_saveTimer =
        new QTimer(this);

    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(AutosaveDebounceMs);

    connect(
        m_saveTimer,
        &QTimer::timeout,
        this,
        &CampusDashboardPage::handleSaveTimeout
        );

    loadCampuses();
}

void CampusDashboardPage::showDirections()
{
    if (m_tabs && m_directionsTab)
    {
        m_tabs->setCurrentWidget(m_directionsTab);
    }
}

void CampusDashboardPage::showInformation()
{
    if (m_tabs && m_informationTab)
    {
        m_tabs->setCurrentWidget(m_informationTab);
    }
}

void CampusDashboardPage::showHousing()
{
    if (m_tabs && m_housingTab)
    {
        m_tabs->setCurrentWidget(m_housingTab);
    }
}

void CampusDashboardPage::showMap()
{
    if (m_tabs && m_mapTab)
    {
        m_tabs->setCurrentWidget(m_mapTab);
    }
}

QString CampusDashboardPage::currentSectionName() const
{
    if (!m_tabs || m_tabs->currentIndex() < 0)
    {
        return tr("Information");
    }

    QWidget* currentTab =
        m_tabs->currentWidget();

    if (currentTab == m_informationTab)
    {
        return tr("Information");
    }

    if (currentTab == m_directionsTab)
    {
        return tr("Directions");
    }

    if (currentTab == m_housingTab)
    {
        return tr("Housing");
    }

    if (currentTab == m_mapTab)
    {
        return tr("Map");
    }

    return m_tabs->tabText(
        m_tabs->currentIndex()
        );
}

void CampusDashboardPage::refresh()
{
    if (!isVisible())
    {
        return;
    }

    loadCampuses();
}

void CampusDashboardPage::saveData()
{
    if (!m_adminMode)
    {
        return;
    }

    saveCurrentCampus();
}

bool CampusDashboardPage::saveChanges()
{
    if (!m_adminMode)
    {
        return true;
    }

    return saveCurrentCampus();
}

bool CampusDashboardPage::hasUnsavedChanges() const
{
    return m_adminMode && m_dirty;
}

void CampusDashboardPage::discardChanges()
{
    if (m_saveTimer)
    {
        m_saveTimer->stop();
    }

    m_dirty = false;

    populateFields(m_currentCampus);
    setStatus(QString());
}

void CampusDashboardPage::loadSelectedCampus()
{
    if (m_loading || !m_campusCombo)
    {
        return;
    }

    if (m_adminMode && m_dirty)
    {
        saveCurrentCampus();
    }

    const QString campusId =
        m_campusCombo
            ->currentData()
            .toString();

    if (campusId.isEmpty())
    {
        return;
    }

    const std::optional<CampusInfo> campus =
        m_repository.loadCampus(campusId);

    if (!campus.has_value())
    {
        return;
    }

    m_currentCampus =
        campus.value();
    m_currentCampusComboIndex =
        m_campusCombo->currentIndex();

    SettingsManager::instance().setLastCampusJsonId(
        m_currentCampus.id
        );

    populateFields(m_currentCampus);

    m_dirty = false;

    if (m_saveTimer)
    {
        m_saveTimer->stop();
    }

    setStatus(QString());
}

void CampusDashboardPage::handleFieldEdited()
{
    if (m_loading)
    {
        return;
    }

    updateMapPreview();
    updateDirectionsCompleteAddresses();
    updateHousingCompleteAddresses();

    if (!m_adminMode)
    {
        return;
    }

    m_dirty = true;
    scheduleSave();
}

void CampusDashboardPage::handleSaveTimeout()
{
    saveCurrentCampus();
}

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

    setStaticToggleButtonWidth(m_directionsToggleLanguageButton);

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
        createScrollContainer(
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

    m_photocopierCodeEdit =
        addLineField(
            form,
            tr("Photocopier Code:")
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
            addHousingSectionFromJson(emptyHousingLocation());
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
        createScrollContainer(
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
    bool koreanAddress
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

    setStaticToggleButtonWidth(section.toggleAddressSystemButton);

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

    sectionLayout->addWidget(addressContainer);

    if (!koreanAddress)
    {
        auto* noteForm =
            new QFormLayout;

        noteForm->setContentsMargins(
            0,
            0,
            0,
            0
            );

        noteForm->setSpacing(8);
        noteForm->setFieldGrowthPolicy(
            QFormLayout::ExpandingFieldsGrow
            );

        section.note =
            addLineField(
                noteForm,
                tr("Note:")
                );

        sectionLayout->addLayout(noteForm);
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

    updateHousingRemoveButtonVisibility();
}

void CampusDashboardPage::loadCampuses()
{
    m_repository.ensurePlaceholderCampus();

    const QList<CampusInfo> campuses =
        m_repository.loadCampuses();

    const QSignalBlocker blocker(m_campusCombo);

    const QString currentCampusId =
        m_currentCampus.id;

    m_campusCombo->clear();

    for (const CampusInfo& campus : campuses)
    {
        m_campusCombo->addItem(
            campusDisplayName(campus),
            campus.id
            );
    }

    if (campuses.isEmpty())
    {
        return;
    }

    QString campusIdToSelect =
        currentCampusId;

    if (campusIdToSelect.isEmpty())
    {
        campusIdToSelect =
            SettingsManager::instance().getLastCampusJsonId();
    }

    int index =
        campusIdToSelect.isEmpty()
            ? -1
            : m_campusCombo->findData(campusIdToSelect);

    if (index < 0)
    {
        index = 0;
    }

    m_campusCombo->setCurrentIndex(index);

    loadSelectedCampus();
}

void CampusDashboardPage::populateFields(
    const CampusInfo& campus
    )
{
    m_loading = true;

    m_nameEdit->setText(campus.campusName);

    if (m_campusCodeEdit)
    {
        m_campusCodeEdit->setText(campus.campusCode);
    }

    m_buildingEdit->setText(campus.buildingName);
    m_buildingKrEdit->setText(campus.buildingNameKr);
    m_phoneEdit->setText(campus.phoneNumber);
    m_phoneKrEdit->setText(campus.phoneNumber);

    populateAddressSection(
        &m_directionsEnglishAddress,
        campus.directionsAddressEn
        );

    populateAddressSection(
        &m_directionsKoreanAddress,
        campus.directionsAddressKr
        );

    m_officeNumberEdit->setText(campus.officeNumber);
    m_transitStepsEdit->setPlainText(
        campus.transitSteps.join(u'\n')
        );
    m_arrivalInfoEdit->setPlainText(campus.arrivalInfo);
    m_imagePathEdit->setText(campus.imageMain);
    m_officeWifiEdit->setText(campus.officeWifi);
    m_officeWifiPasswordEdit->setText(campus.officeWifiPassword);
    m_printerNameEdit->setText(campus.printerName);
    m_printerStepsEdit->setPlainText(campus.printerSteps);

    if (m_printerDriverUrlEdit)
    {
        m_printerDriverUrlEdit->setText(campus.printerDriverUrl);
    }

    if (m_printerDriverUrlUnavailableCheck)
    {
        m_printerDriverUrlUnavailableCheck->setChecked(
            campus.printerDriverUrlUnavailable
            );
    }

    updatePrinterDriverUrlState();

    m_photocopierCodeEdit->setText(campus.photocopierCode);

    populateHousingSections(campus.housingLocations);

    m_loading = false;

    updateMapPreview();
    updateDirectionsCompleteAddresses();
    updateHousingCompleteAddresses();
}

void CampusDashboardPage::populateHousingSections(
    const QJsonArray& housingLocations
    )
{
    clearHousingSections();

    if (housingLocations.isEmpty())
    {
        if (m_housingEmptyLabel)
        {
            m_housingEmptyLabel->setVisible(!m_adminMode);
        }

        if (m_adminMode)
        {
            addHousingSectionFromJson(emptyHousingLocation());
        }

        return;
    }

    if (m_housingEmptyLabel)
    {
        m_housingEmptyLabel->setVisible(false);
    }

    for (const QJsonValue& value : housingLocations)
    {
        if (value.isObject())
        {
            addHousingSectionFromJson(value.toObject());
        }
    }

    if (m_housingSections.isEmpty())
    {
        if (m_adminMode)
        {
            addHousingSectionFromJson(emptyHousingLocation());
        }
        else if (m_housingEmptyLabel)
        {
            m_housingEmptyLabel->setVisible(true);
        }
    }
}

void CampusDashboardPage::clearHousingSections()
{
    auto unregisterLineEdit =
        [this](QLineEdit* edit)
    {
        m_lineEdits.removeAll(edit);
        m_alwaysReadOnlyLineEdits.removeAll(edit);
    };

    auto unregisterTextEdit =
        [this](QPlainTextEdit* edit)
    {
        m_textEdits.removeAll(edit);
        m_alwaysReadOnlyTextEdits.removeAll(edit);
    };

    for (const HousingSectionWidgets& section : m_housingSections)
    {
        unregisterLineEdit(section.name);
        unregisterTextEdit(section.english.complete);
        unregisterLineEdit(section.english.province);
        unregisterLineEdit(section.english.city);
        unregisterLineEdit(section.english.district);
        unregisterLineEdit(section.english.line1);
        unregisterLineEdit(section.english.line2);
        unregisterLineEdit(section.english.postalCode);
        unregisterLineEdit(section.english.note);
        unregisterTextEdit(section.korean.complete);
        unregisterLineEdit(section.korean.province);
        unregisterLineEdit(section.korean.city);
        unregisterLineEdit(section.korean.district);
        unregisterLineEdit(section.korean.line1);
        unregisterLineEdit(section.korean.line2);
        unregisterLineEdit(section.korean.postalCode);
        unregisterLineEdit(section.korean.note);

        if (section.container)
        {
            section.container->deleteLater();
        }
    }

    m_housingSections.clear();
}

void CampusDashboardPage::addHousingSectionFromJson(
    const QJsonObject& housing
    )
{
    if (!m_housingSectionsLayout)
    {
        return;
    }

    if (m_housingEmptyLabel)
    {
        m_housingEmptyLabel->setVisible(false);
    }

    HousingSectionWidgets section;

    auto* container =
        new QFrame(m_housingTab);

    container->setFrameShape(QFrame::StyledPanel);

    auto* sectionLayout =
        new QVBoxLayout(container);

    sectionLayout->setContentsMargins(
        12,
        12,
        12,
        12
        );

    sectionLayout->setSpacing(10);

    section.name =
        new QLineEdit(container);

    m_lineEdits.append(section.name);

    auto* nameRow =
        new QWidget(container);

    auto* nameLayout =
        new QHBoxLayout(nameRow);

    nameLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    nameLayout->setSpacing(8);

    section.toggleLanguageButton =
        new QPushButton(
            tr("Show Korean"),
            nameRow
            );

    setStaticToggleButtonWidth(section.toggleLanguageButton);

    section.removeButton =
        new QPushButton(
            tr("Remove"),
            nameRow
            );

    nameLayout->addWidget(section.name, 1);
    nameLayout->addWidget(section.toggleLanguageButton);
    nameLayout->addWidget(section.removeButton);

    auto* nameForm =
        new QFormLayout;

    nameForm->setSpacing(8);
    nameForm->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

    nameForm->addRow(
        tr("Housing Name:"),
        nameRow
        );

    sectionLayout->addLayout(nameForm);

    section.english =
        createAddressSection(
            container,
            false
            );

    section.korean =
        createAddressSection(
            container,
            true
            );

    sectionLayout->addWidget(section.english.container);
    sectionLayout->addWidget(section.korean.container);

    section.korean.container->setVisible(false);

    section.container =
        container;

    const int insertIndex =
        qMax(
            0,
            m_housingSectionsLayout->count() - 1
            );

    m_housingSectionsLayout->insertWidget(
        insertIndex,
        container
        );

    section.name->setText(
        jsonString(
            housing,
            QStringLiteral("name")
            )
        );

    populateAddressSection(
        &section.english,
        jsonObject(
            housing,
            QStringLiteral("en")
            )
        );

    populateAddressSection(
        &section.korean,
        jsonObject(
            housing,
            QStringLiteral("kr")
            )
        );

    connect(
        section.name,
        &QLineEdit::textEdited,
        this,
        [this]()
        {
            handleFieldEdited();
        }
        );

    connect(
        section.removeButton,
        &QPushButton::clicked,
        this,
        [this, container]()
        {
            for (int i = 0; i < m_housingSections.size(); ++i)
            {
                if (m_housingSections.at(i).container != container)
                {
                    continue;
                }

                const HousingSectionWidgets section =
                    m_housingSections.takeAt(i);

                auto unregisterLineEdit =
                    [this](QLineEdit* edit)
                {
                    m_lineEdits.removeAll(edit);
                    m_alwaysReadOnlyLineEdits.removeAll(edit);
                };

                auto unregisterTextEdit =
                    [this](QPlainTextEdit* edit)
                {
                    m_textEdits.removeAll(edit);
                    m_alwaysReadOnlyTextEdits.removeAll(edit);
                };

                unregisterLineEdit(section.name);
                unregisterTextEdit(section.english.complete);
                unregisterLineEdit(section.english.province);
                unregisterLineEdit(section.english.city);
                unregisterLineEdit(section.english.district);
                unregisterLineEdit(section.english.line1);
                unregisterLineEdit(section.english.line2);
                unregisterLineEdit(section.english.postalCode);
                unregisterLineEdit(section.english.note);
                unregisterTextEdit(section.korean.complete);
                unregisterLineEdit(section.korean.province);
                unregisterLineEdit(section.korean.city);
                unregisterLineEdit(section.korean.district);
                unregisterLineEdit(section.korean.line1);
                unregisterLineEdit(section.korean.line2);
                unregisterLineEdit(section.korean.postalCode);
                unregisterLineEdit(section.korean.note);

                if (section.container)
                {
                    section.container->deleteLater();
                }

                break;
            }

            if (m_housingSections.isEmpty() && m_adminMode)
            {
                addHousingSectionFromJson(emptyHousingLocation());
            }

            updateHousingCompleteAddresses();
            updateHousingRemoveButtonVisibility();

            if (m_adminMode && !m_loading)
            {
                m_dirty = true;
                scheduleSave();
            }
        }
        );

    m_housingSections.append(section);

    connect(
        m_housingSections.last().toggleLanguageButton,
        &QPushButton::clicked,
        this,
        [this, container]()
        {
            for (HousingSectionWidgets& section : m_housingSections)
            {
                if (section.container != container)
                {
                    continue;
                }

                section.showingEnglish =
                    !section.showingEnglish;

                section.english.container->setVisible(
                    section.showingEnglish
                    );

                section.korean.container->setVisible(
                    !section.showingEnglish
                    );

                section.toggleLanguageButton->setText(
                    section.showingEnglish
                        ? tr("Show Korean")
                        : tr("Show English")
                    );

                break;
            }
        }
        );

    applyAdminMode();
    updateHousingRemoveButtonVisibility();
    updateCompleteAddress(&m_housingSections.last().english);
    updateCompleteAddress(&m_housingSections.last().korean);
}

QJsonArray CampusDashboardPage::housingSectionsToJson() const
{
    QJsonArray housingLocations;

    for (const HousingSectionWidgets& section : m_housingSections)
    {
        QJsonObject housing;

        housing.insert(
            QStringLiteral("name"),
            section.name
                ? section.name->text()
                : QString()
            );

        housing.insert(
            QStringLiteral("en"),
            addressSectionToJson(section.english)
            );

        housing.insert(
            QStringLiteral("kr"),
            addressSectionToJson(section.korean)
            );

        housingLocations.append(housing);
    }

    return housingLocations;
}

void CampusDashboardPage::populateAddressSection(
    AddressSectionWidgets* section,
    const QJsonObject& address
    )
{
    if (!section)
    {
        return;
    }

    const QJsonObject modernAddress =
        address
            .value(QStringLiteral("modern"))
            .toObject();

    const QJsonObject classicAddress =
        address
            .value(QStringLiteral("classic"))
            .toObject();

    section->modernAddress =
        normalizedAddressForUi(
            modernAddress.isEmpty()
                ? address
                : modernAddress
            );

    section->classicAddress =
        normalizedAddressForUi(
            classicAddress.isEmpty()
                ? section->modernAddress
                : classicAddress
            );

    section->showingModernAddress =
        jsonString(
            address,
            QStringLiteral("address_system")
            ).compare(QStringLiteral("classic"), Qt::CaseInsensitive) != 0;

    loadAddressFields(
        section,
        section->showingModernAddress
            ? section->modernAddress
            : section->classicAddress
        );

    updateAddressSystemButton(section);
    updateCompleteAddress(section);
}

void CampusDashboardPage::loadAddressFields(
    AddressSectionWidgets* section,
    const QJsonObject& address
    ) const
{
    if (!section)
    {
        return;
    }

    section->province->setText(
        jsonString(
            address,
            QStringLiteral("province")
            )
        );

    section->city->setText(
        jsonString(
            address,
            QStringLiteral("city")
            )
        );

    section->district->setText(
        jsonString(
            address,
            QStringLiteral("district")
            )
        );

    section->line1->setText(
        jsonString(
            address,
            QStringLiteral("line1")
            )
        );

    section->line2->setText(
        jsonString(
            address,
            QStringLiteral("line2")
            )
        );

    section->postalCode->setText(
        jsonString(
            address,
            QStringLiteral("postal_code")
            )
        );

    if (section->note)
    {
        section->note->setText(
            jsonString(
                address,
                QStringLiteral("addr_note")
                )
            );
    }
}

QJsonObject CampusDashboardPage::addressSectionToJson(
    const AddressSectionWidgets& section
    ) const
{
    QJsonObject modernAddress =
        section.modernAddress;

    QJsonObject classicAddress =
        section.classicAddress;

    if (section.showingModernAddress)
    {
        modernAddress =
            addressFieldsToJson(section);
    }
    else
    {
        classicAddress =
            addressFieldsToJson(section);
    }

    QJsonObject address =
        modernAddress;

    address.insert(
        QStringLiteral("modern"),
        modernAddress
        );

    address.insert(
        QStringLiteral("classic"),
        classicAddress
        );

    address.insert(
        QStringLiteral("address_system"),
        section.showingModernAddress
            ? QStringLiteral("modern")
            : QStringLiteral("classic")
        );

    return address;
}

QJsonObject CampusDashboardPage::addressFieldsToJson(
    const AddressSectionWidgets& section
    ) const
{
    QJsonObject address;

    address.insert(
        QStringLiteral("province"),
        section.province
            ? section.province->text()
            : QString()
        );

    address.insert(
        QStringLiteral("city"),
        section.city
            ? section.city->text()
            : QString()
        );

    address.insert(
        QStringLiteral("district"),
        section.district
            ? section.district->text()
            : QString()
        );

    address.insert(
        QStringLiteral("city_district"),
        combinedCityDistrict(
            section.city
                ? section.city->text()
                : QString(),
            section.district
                ? section.district->text()
                : QString()
            )
        );

    address.insert(
        QStringLiteral("line1"),
        section.line1
            ? section.line1->text()
            : QString()
        );

    address.insert(
        QStringLiteral("line2"),
        section.line2
            ? section.line2->text()
            : QString()
        );

    address.insert(
        QStringLiteral("postal_code"),
        section.postalCode
            ? section.postalCode->text()
            : QString()
        );

    address.insert(
        QStringLiteral("addr_note"),
        section.note
            ? section.note->text()
            : QString()
        );

    return address;
}

void CampusDashboardPage::handleAddressSystemToggle(
    QPushButton* button
    )
{
    if (!button)
    {
        return;
    }

    if (m_directionsEnglishAddress.toggleAddressSystemButton == button)
    {
        toggleAddressSystem(&m_directionsEnglishAddress);
        return;
    }

    if (m_directionsKoreanAddress.toggleAddressSystemButton == button)
    {
        toggleAddressSystem(&m_directionsKoreanAddress);
        return;
    }

    for (HousingSectionWidgets& section : m_housingSections)
    {
        if (section.english.toggleAddressSystemButton == button)
        {
            toggleAddressSystem(&section.english);
            return;
        }

        if (section.korean.toggleAddressSystemButton == button)
        {
            toggleAddressSystem(&section.korean);
            return;
        }
    }
}

void CampusDashboardPage::toggleAddressSystem(
    AddressSectionWidgets* section
    )
{
    if (!section)
    {
        return;
    }

    storeCurrentAddressVariant(section);

    section->showingModernAddress =
        !section->showingModernAddress;

    loadAddressFields(
        section,
        section->showingModernAddress
            ? section->modernAddress
            : section->classicAddress
        );

    updateAddressSystemButton(section);
    updateCompleteAddress(section);
}

void CampusDashboardPage::storeCurrentAddressVariant(
    AddressSectionWidgets* section
    ) const
{
    if (!section)
    {
        return;
    }

    if (section->showingModernAddress)
    {
        section->modernAddress =
            addressFieldsToJson(*section);
    }
    else
    {
        section->classicAddress =
            addressFieldsToJson(*section);
    }
}

void CampusDashboardPage::updateAddressSystemButton(
    AddressSectionWidgets* section
    ) const
{
    if (!section || !section->toggleAddressSystemButton)
    {
        return;
    }

    section->toggleAddressSystemButton->setText(
        section->showingModernAddress
            ? tr("Show Classic")
            : tr("Show Modern")
        );
}

QString CampusDashboardPage::completeAddressFor(
    const AddressSectionWidgets& section
    ) const
{
    const QString province =
        section.province
            ? section.province->text().trimmed()
            : QString();

    const QString city =
        section.city
            ? section.city->text().trimmed()
            : QString();

    const QString district =
        section.district
            ? section.district->text().trimmed()
            : QString();

    const QString line1 =
        section.line1
            ? section.line1->text().trimmed()
            : QString();

    const QString line2 =
        section.line2
            ? section.line2->text().trimmed()
            : QString();

    const QString line2Suffix =
        section.line2Suffix
            ? section.line2Suffix->text().trimmed()
            : QString();

    QString renderedLine2 =
        line2;

    if (!line2Suffix.isEmpty())
    {
        renderedLine2 =
            renderedLine2.isEmpty()
                ? tr("(%1)").arg(line2Suffix)
                : tr("%1 (%2)").arg(renderedLine2, line2Suffix);
    }

    const QString postalCode =
        section.postalCode
            ? section.postalCode->text().trimmed()
            : QString();

    QStringList parts;

    if (section.koreanAddress)
    {
        const QString firstLine =
            QStringList{province, city, district, line1}
                .join(QStringLiteral(" "))
                .simplified();

        if (!firstLine.isEmpty())
        {
            parts.append(firstLine);
        }

        if (!renderedLine2.isEmpty())
        {
            parts.append(renderedLine2);
        }

        parts.append(QStringLiteral("[Recipient's Name]"));

        if (!postalCode.isEmpty())
        {
            parts.append(postalCode);
        }

        return parts.join(u'\n');
    }

    parts.append(QStringLiteral("[Recipient's Name]"));

    if (!renderedLine2.isEmpty())
    {
        parts.append(renderedLine2);
    }

    if (!line1.isEmpty())
    {
        parts.append(line1);
    }

    QStringList regionParts;

    if (!district.isEmpty())
    {
        regionParts.append(district);
    }

    if (!city.isEmpty())
    {
        regionParts.append(city);
    }

    if (!province.isEmpty())
    {
        regionParts.append(province);
    }

    if (!postalCode.isEmpty())
    {
        regionParts.append(postalCode);
    }

    const QString finalLine =
        regionParts.join(QStringLiteral(" "));

    if (!finalLine.isEmpty())
    {
        parts.append(finalLine);
    }

    return parts.join(u'\n');
}

void CampusDashboardPage::updateCompleteAddress(
    AddressSectionWidgets* section
    )
{
    if (!section || !section->complete)
    {
        return;
    }

    section->complete->setPlainText(
        completeAddressFor(*section)
        );

    const int lineCount =
        qMax(
            1,
            section
                ->complete
                ->toPlainText()
                .count(u'\n') + 1
            );

    configureExpandingTextField(
        section->complete,
        lineCount,
        lineCount
        );
}

void CampusDashboardPage::updateHousingRemoveButtonVisibility()
{
    for (int i = 0; i < m_housingSections.size(); ++i)
    {
        QPushButton* removeButton =
            m_housingSections.at(i).removeButton;

        if (!removeButton)
        {
            continue;
        }

        removeButton->setVisible(
            m_adminMode && i > 0
            );
    }
}

void CampusDashboardPage::updatePrinterDriverUrlState()
{
    if (!m_printerDriverUrlEdit || !m_printerDriverUrlUnavailableCheck)
    {
        return;
    }

    m_printerDriverUrlEdit->setEnabled(
        !m_printerDriverUrlUnavailableCheck->isChecked()
        );
}

void CampusDashboardPage::showDirectionsLanguage(
    bool showEnglish
    )
{
    m_directionsShowingEnglish =
        showEnglish;

    if (m_directionsEnglishAddress.container)
    {
        m_directionsEnglishAddress.container->setVisible(showEnglish);
    }

    if (m_directionsKoreanAddress.container)
    {
        m_directionsKoreanAddress.container->setVisible(!showEnglish);
    }

    if (m_directionsToggleLanguageButton)
    {
        m_directionsToggleLanguageButton->setText(
            showEnglish
                ? tr("Show Korean")
                : tr("Show English")
            );
    }
}

void CampusDashboardPage::syncPhoneFields(
    QLineEdit* source
    )
{
    if (!source || !m_phoneEdit || !m_phoneKrEdit)
    {
        return;
    }

    QLineEdit* target =
        source == m_phoneEdit
            ? m_phoneKrEdit
            : m_phoneEdit;

    const QSignalBlocker blocker(target);

    target->setText(source->text());
}

void CampusDashboardPage::normalizeCampusNameField()
{
    if (!m_nameEdit || !m_nameEdit->text().trimmed().isEmpty())
    {
        return;
    }

    m_nameEdit->setText(tr("Default"));
    handleFieldEdited();
}

void CampusDashboardPage::handleNewCampus()
{
    if (!m_adminMode)
    {
        return;
    }

    if (m_dirty)
    {
        saveCurrentCampus();
    }

    CampusInfo campus;

    campus.campusName =
        tr("Placeholder");
    campus.campusCode =
        QStringLiteral("PLH");
    campus.printerDriverUrlUnavailable =
        true;
    campus.housingLocations.append(
        emptyHousingLocation()
        );

    {
        const QSignalBlocker blocker(m_campusCombo);

        m_campusCombo->addItem(
            campusDisplayName(campus),
            QString()
            );

        m_campusCombo->setCurrentIndex(
            m_campusCombo->count() - 1
            );
    }

    m_currentCampus =
        campus;
    m_currentCampusComboIndex =
        m_campusCombo->currentIndex();

    populateFields(m_currentCampus);
    m_dirty = true;
    setStatus(tr("New campus"));
}

void CampusDashboardPage::handleManualCampusSave()
{
    if (!m_adminMode)
    {
        return;
    }

    m_dirty = true;
    saveCurrentCampus();
}

bool CampusDashboardPage::readFieldsIntoCampus(
    CampusInfo* campus,
    QString* errorMessage
    ) const
{
    Q_UNUSED(errorMessage);

    if (!campus)
    {
        return false;
    }

    CampusInfo updated =
        m_currentCampus;

    updated.campusName =
        m_nameEdit->text().trimmed().isEmpty()
            ? tr("Default")
            : m_nameEdit->text();
    updated.campusCode =
        m_campusCodeEdit
            ? m_campusCodeEdit->text().trimmed()
            : updated.campusCode;
    updated.buildingName =
        m_buildingEdit->text();
    updated.buildingNameKr =
        m_buildingKrEdit
            ? m_buildingKrEdit->text()
            : updated.buildingNameKr;
    updated.directionsAddressEn =
        addressSectionToJson(m_directionsEnglishAddress);
    updated.directionsAddressKr =
        addressSectionToJson(m_directionsKoreanAddress);
    updated.address =
        completeAddressFor(m_directionsEnglishAddress);
    updated.phoneNumber =
        m_phoneEdit->text();
    updated.officeNumber =
        m_officeNumberEdit->text();
    updated.transitSteps =
        transitStepsFromText(
            m_transitStepsEdit->toPlainText()
            );
    updated.arrivalInfo =
        m_arrivalInfoEdit->toPlainText();
    updated.imageMain =
        m_imagePathEdit->text();
    updated.officeWifi =
        m_officeWifiEdit->text();
    updated.officeWifiPassword =
        m_officeWifiPasswordEdit->text();
    updated.printerName =
        m_printerNameEdit->text();
    updated.printerSteps =
        m_printerStepsEdit->toPlainText();
    updated.printerDriverUrl =
        m_printerDriverUrlEdit
            ? m_printerDriverUrlEdit->text()
            : updated.printerDriverUrl;
    updated.printerDriverUrlUnavailable =
        m_printerDriverUrlUnavailableCheck
            ? m_printerDriverUrlUnavailableCheck->isChecked()
            : updated.printerDriverUrlUnavailable;
    updated.photocopierCode =
        m_photocopierCodeEdit->text();
    updated.housingLocations =
        housingSectionsToJson();

    if (updated.id.trimmed().isEmpty())
    {
        updated.id =
            CampusJsonRepository::idFromName(
                updated.campusName
                );
    }

    *campus =
        updated;

    return true;
}

void CampusDashboardPage::scheduleSave()
{
    if (!m_adminMode || !m_saveTimer)
    {
        return;
    }

    setStatus(tr("Saving..."));
    m_saveTimer->start();
}

bool CampusDashboardPage::saveCurrentCampus()
{
    if (!m_adminMode)
    {
        return true;
    }

    if (!m_dirty)
    {
        return true;
    }

    CampusInfo updated;
    QString errorMessage;

    if (!readFieldsIntoCampus(&updated, &errorMessage))
    {
        setStatus(errorMessage);
        return false;
    }

    if (!m_repository.saveCampus(updated, &errorMessage))
    {
        setStatus(errorMessage);
        return false;
    }

    m_currentCampus =
        updated;

    m_dirty = false;

    if (m_saveTimer)
    {
        m_saveTimer->stop();
    }

    int indexToUpdate =
        m_currentCampusComboIndex;

    if (
        indexToUpdate < 0
        || indexToUpdate >= m_campusCombo->count()
        )
    {
        indexToUpdate =
            m_campusCombo->findData(m_currentCampus.id);
    }

    updateCampusSelectorItem(indexToUpdate);

    m_currentCampusComboIndex =
        indexToUpdate;

    setStatus(tr("Saved"));

    return true;
}

void CampusDashboardPage::updateCampusSelectorItem(
    int index
    )
{
    if (!m_campusCombo || index < 0)
    {
        return;
    }

    m_campusCombo->setItemText(
        index,
        campusDisplayName(m_currentCampus)
        );

    m_campusCombo->setItemData(
        index,
        m_currentCampus.id
        );
}

QString CampusDashboardPage::campusDisplayName(
    const CampusInfo& campus
    ) const
{
    const QString campusName =
        campus.campusName.trimmed().isEmpty()
            ? campus.id
            : campus.campusName.trimmed();

    const QString campusCode =
        campus.campusCode.trimmed();

    if (!m_adminMode && !campusCode.isEmpty())
    {
        return tr("%1 (%2)")
            .arg(campusName, campusCode);
    }

    return campusName;
}

void CampusDashboardPage::updateMapPreview()
{
    if (!m_mapPreviewLabel || !m_imagePathEdit)
    {
        return;
    }

    CampusMapPreview::update(
        m_mapPreviewLabel,
        m_imagePathEdit->text()
        );
}

void CampusDashboardPage::updateDirectionsCompleteAddresses()
{
    updateCompleteAddress(&m_directionsEnglishAddress);
    updateCompleteAddress(&m_directionsKoreanAddress);
}

void CampusDashboardPage::updateHousingCompleteAddresses()
{
    for (HousingSectionWidgets& section : m_housingSections)
    {
        updateCompleteAddress(&section.english);
        updateCompleteAddress(&section.korean);
    }
}

void CampusDashboardPage::emitCurrentSectionChanged()
{
    const QString sectionName =
        currentSectionName();

    if (!sectionName.isEmpty())
    {
        emit sectionChanged(sectionName);
    }
}

void CampusDashboardPage::setStatus(
    const QString& message
    )
{
    if (!m_statusLabel)
    {
        return;
    }

    m_statusLabel->setText(message);
}
