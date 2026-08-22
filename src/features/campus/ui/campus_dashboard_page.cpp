#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"
#include "features/campus/ui/campus_map_preview.h"

#include "core/resource_paths.h"
#include "core/settingsmanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>

namespace Detail = CampusDashboardPageDetail;

CampusDashboardPage::CampusDashboardPage(
    bool adminMode,
    QWidget *parent
    )
    : BasePage(parent)
    , m_adminMode(adminMode)
    , m_repository(
        ResourcePaths::Campuses::directory()
        )
{
    buildUi();
    applyAdminMode();

    m_saveTimer =
        new QTimer(this);

    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(Detail::AutosaveDebounceMs);

    connect(
        m_saveTimer,
        &QTimer::timeout,
        this,
        &CampusDashboardPage::handleSaveTimeout
        );

    loadCampuses();
}

void CampusDashboardPage::showAddress()
{
    if (m_tabs && m_addressTab)
    {
        m_tabs->setCurrentWidget(m_addressTab);
    }
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

    if (currentTab == m_addressTab)
    {
        return tr("Address");
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
        return tr("Maps");
    }

    return m_tabs->tabText(
        m_tabs->currentIndex()
        );
}

QString CampusDashboardPage::currentSectionKey() const
{
    if (!m_tabs || m_tabs->currentIndex() < 0)
    {
        return QStringLiteral("campus_information");
    }

    QWidget* currentTab =
        m_tabs->currentWidget();

    if (currentTab == m_informationTab)
    {
        return QStringLiteral("campus_information");
    }

    if (currentTab == m_addressTab)
    {
        return QStringLiteral("campus_address");
    }

    if (currentTab == m_directionsTab)
    {
        return QStringLiteral("campus_directions");
    }

    if (currentTab == m_housingTab)
    {
        return QStringLiteral("campus_housing");
    }

    if (currentTab == m_mapTab)
    {
        return QStringLiteral("campus_map");
    }

    return QString();
}

void CampusDashboardPage::refresh()
{
    if (!isVisible())
    {
        return;
    }

    loadCampuses();
}

Status CampusDashboardPage::prepareForActivation()
{
    updateMapPreview();
    return {};
}

void CampusDashboardPage::releaseFeatureResources()
{
    // Map cards own QPixmaps decoded from the campuses pack. Delete them
    // before PageManager releases the final campuses lease.
    clearMapSections();
}

void CampusDashboardPage::retranslateUi()
{
    if (m_tabs)
    {
        if (m_informationTab)
        {
            const int index =
                m_tabs->indexOf(m_informationTab);

            if (index >= 0)
            {
                m_tabs->setTabText(
                    index,
                    tr("Information")
                    );
            }
        }

        if (m_addressTab)
        {
            const int index =
                m_tabs->indexOf(m_addressTab);

            if (index >= 0)
            {
                m_tabs->setTabText(
                    index,
                    tr("Address")
                    );
            }
        }

        if (m_directionsTab)
        {
            const int index =
                m_tabs->indexOf(m_directionsTab);

            if (index >= 0)
            {
                m_tabs->setTabText(
                    index,
                    tr("Directions")
                    );
            }
        }

        if (m_housingTab)
        {
            const int index =
                m_tabs->indexOf(m_housingTab);

            if (index >= 0)
            {
                m_tabs->setTabText(
                    index,
                    tr("Housing")
                    );
            }
        }

        if (m_mapTab)
        {
            const int index =
                m_tabs->indexOf(m_mapTab);

            if (index >= 0)
            {
                m_tabs->setTabText(
                    index,
                    tr("Maps")
                    );
            }
        }
    }

    retranslateRegisteredLabels();

    if (m_newCampusButton)
    {
        m_newCampusButton->setText(
            tr("New Campus")
            );
    }

    if (m_saveCampusButton)
    {
        m_saveCampusButton->setText(
            m_dirty
                ? tr("Save Campus *")
                : tr("Save Campus")
            );
    }

    if (m_addHousingButton)
    {
        m_addHousingButton->setText(
            tr("Add Housing Location")
            );
    }

    if (m_housingEmptyLabel)
    {
        m_housingEmptyLabel->setText(
            tr("No housing information available")
            );
    }

    if (m_printerDriverUrlUnavailableCheck)
    {
        m_printerDriverUrlUnavailableCheck->setText(
            tr("N/A")
            );
    }

    if (m_photocopierCodeUnavailableCheck)
    {
        m_photocopierCodeUnavailableCheck->setText(
            tr("N/A")
            );
    }

    showDirectionsLanguage(
        m_directionsShowingEnglish
        );

    updateAddressSystemButton(
        &m_directionsEnglishAddress
        );
    updateAddressSystemButton(
        &m_directionsKoreanAddress
        );
    updateAddressComponentsButton(
        &m_directionsEnglishAddress
        );
    updateAddressComponentsButton(
        &m_directionsKoreanAddress
        );

    const auto updateControlWidths =
        [this](AddressSectionWidgets* section)
    {
        if (!section)
        {
            return;
        }

        Detail::setStaticToggleButtonWidths(
            section->toggleLanguageButton,
            section->toggleAddressSystemButton,
            section->toggleAddressComponentsButton,
            {
                tr("Show English"),
                tr("Show Korean"),
                tr("Show Modern"),
                tr("Show Classic"),
                tr("Show Details"),
                tr("Hide Details")
            }
            );
    };

    updateControlWidths(&m_directionsEnglishAddress);
    updateControlWidths(&m_directionsKoreanAddress);

    for (HousingSectionWidgets& section : m_housingSections)
    {
        if (section.english.toggleLanguageButton)
        {
            section.english.toggleLanguageButton->setText(
                tr("Show Korean")
                );
        }

        if (section.korean.toggleLanguageButton)
        {
            section.korean.toggleLanguageButton->setText(
                tr("Show English")
                );
        }

        if (section.removeButton)
        {
            section.removeButton->setText(
                tr("Remove")
                );
        }

        updateAddressSystemButton(
            &section.english
            );
        updateAddressSystemButton(
            &section.korean
            );
        updateAddressComponentsButton(
            &section.english
            );
        updateAddressComponentsButton(
            &section.korean
            );
        updateControlWidths(&section.english);
        updateControlWidths(&section.korean);
    }

    updateDirectionsCompleteAddresses();
    updateHousingCompleteAddresses();
    updateMapPreview();
    updateCampusSaveButton();
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

void CampusDashboardPage::setSaveMode(
    SaveMode mode
    )
{
    if (m_saveMode == mode)
    {
        return;
    }

    m_saveMode =
        mode;

    updateCampusSaveButton();

    if (!m_saveTimer)
    {
        return;
    }

    if (
        m_saveMode == SaveMode::Automatic
        && m_dirty
        )
    {
        setStatus(tr("Saving..."));
        m_saveTimer->start();
    }
    else
    {
        m_saveTimer->stop();

        if (
            m_saveMode == SaveMode::Manual
            && m_dirty
            )
        {
            setStatus(tr("Unsaved changes"));
        }
    }
}

void CampusDashboardPage::discardChanges()
{
    if (m_saveTimer)
    {
        m_saveTimer->stop();
    }

    m_dirty = false;

    populateFields(m_currentCampus);
    updateCampusSaveButton();
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

    updateCampusSaveButton();
    setStatus(QString());
}

void CampusDashboardPage::handleFieldEdited()
{
    if (m_loading)
    {
        return;
    }

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

void CampusDashboardPage::emitCurrentSectionChanged()
{
    const QString sectionKey =
        currentSectionKey();

    if (!sectionKey.isEmpty())
    {
        emit sectionChanged(sectionKey);
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
