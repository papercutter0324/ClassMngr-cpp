#include "campus_dashboard_page.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "campus_dashboard_page_detail.h"
#include "features/campus/ui/campus_map_preview.h"
#include "core/settingsmanager.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace Detail = CampusDashboardPageDetail;

namespace
{
QStringList mapImagePaths(
    const QJsonObject& map
    )
{
    QStringList imagePaths;

    const QJsonArray images =
        map
            .value(QStringLiteral("images"))
            .toArray();

    for (const QJsonValue& image : images)
    {
        const QString imagePath =
            image.toString().trimmed();

        if (!imagePath.isEmpty())
        {
            imagePaths.append(imagePath);
        }
    }

    return imagePaths;
}

QString mapLink(
    const QJsonObject& map,
    const QString& provider
    )
{
    return map
        .value(QStringLiteral("links"))
        .toObject()
        .value(provider)
        .toString();
}
}

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
    updateCampusSaveButton();
    setStatus(tr("New campus"));
}

void CampusDashboardPage::handleManualCampusSave()
{
    if (!m_adminMode)
    {
        return;
    }

    m_dirty = true;
    updateCampusSaveButton();
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
    if (!m_adminMode)
    {
        return;
    }

    updateCampusSaveButton();

    if (
        m_saveMode != SaveMode::Automatic
        || !m_saveTimer
        )
    {
        setStatus(tr("Unsaved changes"));
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

    updateCampusSaveButton();

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

void CampusDashboardPage::updateCampusSaveButton()
{
    if (!m_saveCampusButton)
    {
        return;
    }

    const bool showSaveButton =
        m_saveMode != SaveMode::Automatic;

    m_saveCampusButton->setVisible(
        showSaveButton
        );

    m_saveCampusButton->setEnabled(
        showSaveButton
        && m_adminMode
        && m_dirty
        );
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
    if (!m_mapSectionsLayout)
    {
        return;
    }

    clearMapSections();

    addMapSection(
        tr("Campus"),
        m_currentCampus.mapImagePaths,
        m_currentCampus.naverMapUrl,
        m_currentCampus.kakaoMapUrl
        );

    int housingNumber = 0;

    for (const HousingSectionWidgets& housing : m_housingSections)
    {
        if (!housingSectionHasContent(housing))
        {
            continue;
        }

        ++housingNumber;

        const QString housingName =
            housing.name
                ? housing.name->text().trimmed()
                : QString();

        addMapSection(
            housingName.isEmpty()
                ? tr("Housing %1").arg(housingNumber)
                : housingName,
            mapImagePaths(housing.map),
            mapLink(housing.map, QStringLiteral("naver")),
            mapLink(housing.map, QStringLiteral("kakao"))
            );
    }
}

void CampusDashboardPage::clearMapSections()
{
    for (const MapSectionWidgets& section : std::as_const(m_mapSections))
    {
        if (section.card)
        {
            m_mapSectionsLayout->removeWidget(section.card);
            delete section.card;
        }
    }

    m_mapSections.clear();
}

void CampusDashboardPage::addMapSection(
    const QString& title,
    const QStringList& imagePaths,
    const QString& naverUrl,
    const QString& kakaoUrl
    )
{
    if (!m_mapSectionsLayout)
    {
        return;
    }

    MapSectionWidgets section;

    section.card =
        new SectionCard(
            title,
            m_mapTab
            );

    section.preview =
        new CampusMapPreview(section.card);

    auto* controls =
        new QWidget(section.preview);
    auto* linkLayout =
        new QHBoxLayout(controls);

    linkLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );
    linkLayout->setSpacing(8);
    linkLayout->addStretch();

    if (CampusMapPreview::isValidMapUrl(naverUrl))
    {
        section.naverButton =
            new TextFitPushButton(
                tr("Naver Maps"),
                controls
                );

        linkLayout->addWidget(section.naverButton);

        connect(
            section.naverButton,
            &QPushButton::clicked,
            this,
            [this, naverUrl]()
            {
                openMapUrl(naverUrl);
            }
            );
    }

    if (CampusMapPreview::isValidMapUrl(kakaoUrl))
    {
        section.kakaoButton =
            new TextFitPushButton(
                tr("Kakao Maps"),
                controls
                );

        linkLayout->addWidget(section.kakaoButton);

        connect(
            section.kakaoButton,
            &QPushButton::clicked,
            this,
            [this, kakaoUrl]()
            {
                openMapUrl(kakaoUrl);
            }
            );
    }

    linkLayout->addStretch();

    if (
        section.naverButton
        || section.kakaoButton
        )
    {
        section.preview->setMapControls(controls);
    }
    else
    {
        delete controls;
    }

    section.preview->setImagePaths(imagePaths);

    section.card
        ->contentLayout()
        ->addWidget(section.preview);

    m_mapSectionsLayout->addWidget(section.card);
    m_mapSections.append(section);
}

bool CampusDashboardPage::housingSectionHasContent(
    const HousingSectionWidgets& section
    ) const
{
    return (
        section.name
        && !section.name->text().trimmed().isEmpty()
        )
        || !section.map.isEmpty();
}

void CampusDashboardPage::openMapUrl(
    const QString& url
    )
{
    if (!CampusMapPreview::isValidMapUrl(url))
    {
        return;
    }

    if (
        !QDesktopServices::openUrl(
            QUrl::fromUserInput(url.trimmed())
            )
        )
    {
        QMessageBox::warning(
            this,
            tr("Unable to Open Map"),
            tr("The map link could not be opened.")
            );
    }
}
