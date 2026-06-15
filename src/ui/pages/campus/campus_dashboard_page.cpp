#include "campus_dashboard_page.h"

#include "core/appsettings.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTextOption>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
constexpr int AutosaveDebounceMs = 800;

const QString LastCampusSettingsKey =
    QStringLiteral("campus/lastSelectedJsonId");

QString housingJsonText(
    const QJsonArray& housingLocations
    )
{
    const QJsonDocument document(housingLocations);

    return QString::fromUtf8(
        document.toJson(QJsonDocument::Indented)
        );
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
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container =
        new QWidget(scroll);

    auto* formLayout =
        new QFormLayout(container);

    formLayout->setContentsMargins(
        12,
        12,
        12,
        12
        );

    formLayout->setSpacing(10);
    formLayout->setFieldGrowthPolicy(
        QFormLayout::ExpandingFieldsGrow
        );

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
    , m_repository(QString::fromUtf8(AppSettings::DefaultCampusDirectory))
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

void CampusDashboardPage::showMap()
{
    if (m_tabs && m_mapTab)
    {
        m_tabs->setCurrentWidget(m_mapTab);
    }
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

    QSettings settings;

    settings.setValue(
        LastCampusSettingsKey,
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
    QFormLayout* form = nullptr;

    QWidget* tab =
        createScrollContainer(
            this,
            &form
            );

    m_nameEdit =
        addLineField(
            form,
            tr("Campus Name:")
            );

    m_buildingEdit =
        addLineField(
            form,
            tr("Building Name:")
            );

    m_addressEdit =
        addLineField(
            form,
            tr("Address:")
            );

    m_phoneEdit =
        addLineField(
            form,
            tr("Phone Number:")
            );

    m_transitStepsEdit =
        addTextField(
            form,
            tr("Transit Steps:"),
            130
            );

    m_arrivalInfoEdit =
        addTextField(
            form,
            tr("Upon Arriving:"),
            110
            );

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
            120
            );

    m_photocopierCodeEdit =
        addLineField(
            form,
            tr("Photocopier Code:")
            );

    m_housingLocationsEdit =
        addTextField(
            form,
            tr("Housing Locations:"),
            260
            );

    m_housingLocationsEdit->setWordWrapMode(
        QTextOption::NoWrap
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
    int minimumHeight
    )
{
    auto* edit =
        new QPlainTextEdit(this);

    edit->setMinimumHeight(minimumHeight);
    edit->setTabChangesFocus(true);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);

    form->addRow(
        labelText,
        edit
        );

    m_textEdits.append(edit);

    connect(
        edit,
        &QPlainTextEdit::textChanged,
        this,
        &CampusDashboardPage::handleFieldEdited
        );

    return edit;
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
    selectorLayout->addStretch();

    contentLayout()->addLayout(selectorLayout);

    m_tabs =
        new QTabWidget(this);

    m_directionsTab =
        createDirectionsTab();

    m_informationTab =
        createInformationTab();

    m_mapTab =
        createMapTab();

    m_tabs->addTab(
        m_directionsTab,
        tr("Directions")
        );

    m_tabs->addTab(
        m_informationTab,
        tr("Information")
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
            campus.campusName.trimmed().isEmpty()
                ? campus.id
                : campus.campusName,
            campus.id
            );
    }

    if (campuses.isEmpty())
    {
        return;
    }

    QSettings settings;

    QString campusIdToSelect =
        currentCampusId;

    if (campusIdToSelect.isEmpty())
    {
        campusIdToSelect =
            settings
                .value(LastCampusSettingsKey)
                .toString();
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
    m_buildingEdit->setText(campus.buildingName);
    m_addressEdit->setText(campus.address);
    m_phoneEdit->setText(campus.phoneNumber);
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
    m_photocopierCodeEdit->setText(campus.photocopierCode);
    m_housingLocationsEdit->setPlainText(
        housingJsonText(campus.housingLocations)
        );

    m_loading = false;

    updateMapPreview();
}

bool CampusDashboardPage::readFieldsIntoCampus(
    CampusInfo* campus,
    QString* errorMessage
    ) const
{
    if (!campus)
    {
        return false;
    }

    CampusInfo updated =
        m_currentCampus;

    updated.campusName =
        m_nameEdit->text();
    updated.buildingName =
        m_buildingEdit->text();
    updated.address =
        m_addressEdit->text();
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
    updated.photocopierCode =
        m_photocopierCodeEdit->text();

    const QString housingText =
        m_housingLocationsEdit
            ->toPlainText()
            .trimmed();

    if (housingText.isEmpty())
    {
        updated.housingLocations = {};
    }
    else
    {
        QJsonParseError parseError;

        const QJsonDocument document =
            QJsonDocument::fromJson(
                housingText.toUtf8(),
                &parseError
                );

        if (
            parseError.error != QJsonParseError::NoError
            || !document.isArray()
            )
        {
            if (errorMessage)
            {
                *errorMessage =
                    tr("Housing Locations must be a JSON array.");
            }

            return false;
        }

        updated.housingLocations =
            document.array();
    }

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

    const int currentIndex =
        m_campusCombo->currentIndex();

    if (currentIndex >= 0)
    {
        m_campusCombo->setItemText(
            currentIndex,
            m_currentCampus.campusName.trimmed().isEmpty()
                ? m_currentCampus.id
                : m_currentCampus.campusName
            );

        m_campusCombo->setItemData(
            currentIndex,
            m_currentCampus.id
            );
    }

    setStatus(tr("Saved"));

    return true;
}

void CampusDashboardPage::updateMapPreview()
{
    if (!m_mapPreviewLabel || !m_imagePathEdit)
    {
        return;
    }

    const QString imagePath =
        m_imagePathEdit->text().trimmed();

    if (imagePath.isEmpty() || !QFileInfo::exists(imagePath))
    {
        m_mapPreviewLabel->setPixmap(QPixmap());
        m_mapPreviewLabel->setText(tr("No map available"));
        return;
    }

    QPixmap pixmap(imagePath);

    if (pixmap.isNull())
    {
        m_mapPreviewLabel->setPixmap(QPixmap());
        m_mapPreviewLabel->setText(tr("No map available"));
        return;
    }

    m_mapPreviewLabel->setText(QString());
    m_mapPreviewLabel->setPixmap(
        pixmap.scaled(
            800,
            520,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );
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
