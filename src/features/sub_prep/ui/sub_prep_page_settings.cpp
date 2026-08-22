#include "sub_prep_page_p.h"

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

    m_classMaterialsEdit->setPlainText(
        settingsService
            ->loadOrDefault(
                SettingsKeys::ClassMaterials,
                QString()
                )
            .toString()
        );

    const QVariant storedGrading =
        settingsService->loadOrDefault(
            SettingsKeys::BookReportGrading,
            QVariant()
            );
    const QVariant storedSpecial =
        settingsService->loadOrDefault(
            SettingsKeys::BookReportSpecialInstructions,
            QVariant()
            );

    if (storedGrading.isValid())
    {
        m_gradingInstructionsEdit->setPlainText(
            storedGrading.toString()
            );
        m_specialInstructionsEdit->setPlainText(
            storedSpecial.isValid()
                ? storedSpecial.toString()
                : QString()
            );
    }
    else
    {
        m_gradingInstructionsEdit->setPlainText(
            defaultGradingInstructions()
            );
        m_specialInstructionsEdit->setPlainText(
            storedSpecial.isValid()
                ? storedSpecial.toString()
                : defaultSpecialInstructions()
            );
    }

    m_subNotesEdit->setPlainText(
        settingsService
            ->loadOrDefault(
                SettingsKeys::SubNotes,
                QString()
                )
            .toString()
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

    const QString savedCampus =
        settingsService
            ->loadOrDefault(
                SettingsKeys::MyInfoCampus,
                QString()
                )
            .toString();

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

    const Status saved = settingsService->saveAll({
        {
            SettingsKeys::ClassMaterials,
            m_classMaterialsEdit->toPlainText()
        },
        {
            SettingsKeys::BookReportGrading,
            m_gradingInstructionsEdit->toPlainText()
        },
        {
            SettingsKeys::BookReportSpecialInstructions,
            m_specialInstructionsEdit->toPlainText()
        },
        {
            SettingsKeys::SubNotes,
            m_subNotesEdit->toPlainText()
        }
    });
    if (!saved)
    {
        return false;
    }

    clearDirty();
    return true;
}
