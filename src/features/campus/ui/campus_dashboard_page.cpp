#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"

#include "core/resource_paths.h"
#include "core/settingsmanager.h"

#include <QComboBox>
#include <QLabel>
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
