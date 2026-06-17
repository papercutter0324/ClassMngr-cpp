#include "campus_dashboard_page.h"

#include "campus_dashboard_page_detail.h"
#include "features/campus/ui/campus_map_preview.h"
#include "core/settingsmanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTimer>

namespace Detail = CampusDashboardPageDetail;

void CampusDashboardPage::loadCampuses()
{
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

    const bool photocopierUnavailable =
        campus.photocopierCode
            .trimmed()
            .compare(
                QString::fromLatin1(Detail::NotApplicableText),
                Qt::CaseInsensitive
                ) == 0;

    if (m_photocopierCodeUnavailableCheck)
    {
        m_photocopierCodeUnavailableCheck->setChecked(
            photocopierUnavailable
            );
    }

    m_photocopierCodeEdit->setText(
        photocopierUnavailable
            ? QString::fromLatin1(Detail::NotApplicableText)
            : campus.photocopierCode
        );

    updatePhotocopierCodeState();

    populateHousingSections(campus.housingLocations);

    m_loading = false;

    updateMapPreview();
    updateDirectionsCompleteAddresses();
    updateHousingCompleteAddresses();
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

void CampusDashboardPage::updatePhotocopierCodeState()
{
    if (!m_photocopierCodeEdit || !m_photocopierCodeUnavailableCheck)
    {
        return;
    }

    const bool unavailable =
        m_photocopierCodeUnavailableCheck->isChecked();

    if (unavailable)
    {
        const QSignalBlocker blocker(m_photocopierCodeEdit);

        m_photocopierCodeEdit->setText(
            QString::fromLatin1(Detail::NotApplicableText)
            );
    }
    else if (
        m_photocopierCodeEdit
            ->text()
            .trimmed()
            .compare(
                QString::fromLatin1(Detail::NotApplicableText),
                Qt::CaseInsensitive
                ) == 0
        )
    {
        const QSignalBlocker blocker(m_photocopierCodeEdit);

        m_photocopierCodeEdit->clear();
    }

    m_photocopierCodeEdit->setEnabled(!unavailable);
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
        tr("New Campus");
    campus.campusCode =
        QStringLiteral("NEW");
    campus.printerDriverUrlUnavailable =
        true;
    campus.housingLocations.append(
        Detail::emptyHousingLocation()
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

Status CampusDashboardPage::readFieldsIntoCampus(
    CampusInfo* campus
    ) const
{
    if (!campus)
    {
        return std::unexpected(
            tr("Campus data could not be prepared.")
            );
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
        Detail::transitStepsFromText(
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
        m_photocopierCodeUnavailableCheck
            && m_photocopierCodeUnavailableCheck->isChecked()
                ? QString::fromLatin1(Detail::NotApplicableText)
                : m_photocopierCodeEdit->text();
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

    return {};
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

    const Status fieldsRead =
        readFieldsIntoCampus(&updated);

    if (!fieldsRead)
    {
        setStatus(fieldsRead.error());
        return false;
    }

    const Status saved =
        m_repository.saveCampus(updated);

    if (!saved)
    {
        setStatus(saved.error());
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
