#include "sub_prep_page_p.h"

#include "next/platform/application_services_current_campus_preferences_port.h"
#include "next/platform/application_services_sub_prep_preferences_port.h"

#include <string>

void SubPrepPage::loadPageData()
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadStoredSettings();
    loadPersonalZoomInformation();
    loadCampuses();
    refreshGeneratedContent();

    m_loading = false;
    clearDirty();
}

void SubPrepPage::loadStoredSettings()
{
    auto* settingsService =
        openSettingsService(m_services);

    if (!settingsService)
    {
        return;
    }

    const QSignalBlocker materialsBlocker(m_classMaterialsEdit);
    const QSignalBlocker gradingBlocker(m_gradingInstructionsEdit);
    const QSignalBlocker specialBlocker(m_specialInstructionsEdit);
    const QSignalBlocker notesBlocker(m_subNotesEdit);

    ClassMngr::Next::Platform::
        ApplicationServicesSubPrepPreferencesPort
        subPrepPreferencesPort(settingsService);
    const auto storedPreferences =
        subPrepPreferencesPort.load();
    if (!storedPreferences)
    {
        return;
    }

    const auto& preferences =
        storedPreferences.value();

    m_classMaterialsEdit->setPlainText(
        QString::fromUtf8(
            preferences.classMaterials.data(),
            static_cast<qsizetype>(preferences.classMaterials.size())
            )
        );

    if (preferences.bookReportGrading)
    {
        m_gradingInstructionsEdit->setPlainText(
            QString::fromUtf8(
                preferences.bookReportGrading->data(),
                static_cast<qsizetype>(
                    preferences.bookReportGrading->size()
                    )
                )
            );
        m_specialInstructionsEdit->setPlainText(
            preferences.bookReportSpecialInstructions
                ? QString::fromUtf8(
                    preferences.bookReportSpecialInstructions->data(),
                    static_cast<qsizetype>(
                        preferences.bookReportSpecialInstructions->size()
                        )
                    )
                : QString()
            );
    }
    else
    {
        m_gradingInstructionsEdit->setPlainText(
            defaultGradingInstructions()
        );
        m_specialInstructionsEdit->setPlainText(
            preferences.bookReportSpecialInstructions
                ? QString::fromUtf8(
                    preferences.bookReportSpecialInstructions->data(),
                    static_cast<qsizetype>(
                        preferences.bookReportSpecialInstructions->size()
                        )
                    )
                : defaultSpecialInstructions()
            );
    }

    m_subNotesEdit->setPlainText(
        QString::fromUtf8(
            preferences.subComments.data(),
            static_cast<qsizetype>(preferences.subComments.size())
            )
        );
}

void SubPrepPage::loadPersonalZoomInformation()
{
    auto* settingsService =
        openSettingsService(m_services);

    if (!settingsService)
    {
        return;
    }

    const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);

    const QString loginId =
        loadSettingWithLegacyFallback(
            settingsService,
            SettingsKeys::MyInfoZoomLoginId,
            SettingsKeys::LegacyZoomLoginId,
            NotAvailableText
            )
            .toString();
    const QString password =
        loadSettingWithLegacyFallback(
            settingsService,
            SettingsKeys::MyInfoZoomPassword,
            SettingsKeys::LegacyZoomPassword,
            NotAvailableText
            )
            .toString();
    const bool unavailable =
        loadSettingWithLegacyFallback(
            settingsService,
            SettingsKeys::MyInfoZoomNotAvailable,
            SettingsKeys::LegacyZoomNotAvailable,
            true
            )
            .toBool();

    m_zoomLoginIdEdit->setText(
        unavailable
            ? NotAvailableText
            : valueOrNa(loginId)
        );
    m_zoomPasswordEdit->setText(
        unavailable
            ? NotAvailableText
            : valueOrNa(password)
        );

    updateReadOnlyFieldWidths();
}

void SubPrepPage::loadCampuses()
{
    auto* settingsService =
        openSettingsService(m_services);

    if (!settingsService)
    {
        return;
    }

    const bool wasLoading =
        m_loading;
    m_loading = true;

    m_campuses =
        campusRepository().loadCampuses();

    ClassMngr::Next::Platform::
        ApplicationServicesCurrentCampusPreferencesPort
        currentCampusPreferencesPort(settingsService);
    const std::string storedCampus =
        currentCampusPreferencesPort.read();
    const QString savedCampus = QString::fromUtf8(
        storedCampus.data(),
        static_cast<qsizetype>(storedCampus.size())
        );

    QString campusId;

    for (const CampusInfo& campus : std::as_const(m_campuses))
    {
        if (
            campus.id.compare(
                savedCampus.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            || campusDisplayName(campus).compare(
                savedCampus.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campusId = campus.id;
            break;
        }
    }

    if (campusId.isEmpty() && !m_campuses.isEmpty())
    {
        campusId = m_campuses.first().id;
    }

    loadCampusFields(campusId);
    updateReadOnlyFieldWidths();

    m_loading = wasLoading;
}

void SubPrepPage::loadCampusFields(
    const QString& campusId
    )
{
    CampusInfo campus;
    bool found = false;

    for (const CampusInfo& candidate : std::as_const(m_campuses))
    {
        if (
            candidate.id.compare(
                campusId,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campus = candidate;
            found = true;
            break;
        }
    }

    const QSignalBlocker officeBlocker(m_officeNumberEdit);
    const QSignalBlocker wifiBlocker(m_officeWifiEdit);
    const QSignalBlocker wifiPasswordBlocker(m_officeWifiPasswordEdit);
    const QSignalBlocker photocopierBlocker(m_photocopierCodeEdit);

    m_officeNumberEdit->setText(
        found
            ? valueOrNa(campus.officeNumber)
            : NotAvailableText
        );
    m_officeWifiEdit->setText(
        found
            ? valueOrNa(campus.officeWifi)
            : NotAvailableText
        );
    m_officeWifiPasswordEdit->setText(
        found
            ? valueOrNa(campus.officeWifiPassword)
            : NotAvailableText
        );
    m_photocopierCodeEdit->setText(
        found
            ? valueOrNa(campus.photocopierCode)
            : NotAvailableText
        );

    updateReadOnlyFieldWidths();
}

void SubPrepPage::updateReadOnlyFieldWidths()
{
    WidgetSizing::updateTextAwareFieldWidth(
        m_officeNumberEdit,
        OfficeNumberFieldWidth
        );

    for (QLineEdit* edit : {
             m_officeWifiEdit,
             m_officeWifiPasswordEdit,
             m_photocopierCodeEdit,
             m_zoomLoginIdEdit,
             m_zoomPasswordEdit
             })
    {
        WidgetSizing::updateTextAwareFieldWidth(
            edit,
            CompactFieldWidth
            );
    }

    if (m_campusCard)
    {
        m_campusCard->updateGeometry();
    }

    if (m_zoomCard)
    {
        m_zoomCard->updateGeometry();
    }
}

bool SubPrepPage::saveSubPrepInternal()
{
    auto* settingsService =
        openSettingsService(m_services);

    if (!settingsService)
    {
        return false;
    }

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    restoreGradingDefaultIfNeeded();

    const auto toUtf8 = [](const QString& value)
    {
        const QByteArray encoded = value.toUtf8();
        return std::string(
            encoded.constData(),
            static_cast<std::size_t>(encoded.size())
            );
    };

    ClassMngr::Next::Platform::
        ApplicationServicesSubPrepPreferencesPort
        subPrepPreferencesPort(settingsService);
    const auto saved = subPrepPreferencesPort.save({
        .classMaterials = toUtf8(m_classMaterialsEdit->toPlainText()),
        .bookReportGrading = toUtf8(
            m_gradingInstructionsEdit->toPlainText()
            ),
        .bookReportSpecialInstructions = toUtf8(
            m_specialInstructionsEdit->toPlainText()
            ),
        .subComments = toUtf8(m_subNotesEdit->toPlainText())
    });
    if (!saved)
    {
        return false;
    }

    clearDirty();
    return true;
}
