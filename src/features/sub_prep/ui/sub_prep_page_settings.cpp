#include "sub_prep_page_p.h"

#include "next/platform/application_services_current_campus_preferences_port.h"
#include "next/platform/application_services_sub_prep_personal_zoom_preferences_port.h"
#include "next/platform/application_services_sub_prep_preferences_port.h"
#include "next/platform/sub_prep_campus_directory_query_adapter.h"

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
    if (!m_services)
    {
        return;
    }

    const QSignalBlocker materialsBlocker(m_classMaterialsEdit);
    const QSignalBlocker gradingBlocker(m_gradingInstructionsEdit);
    const QSignalBlocker specialBlocker(m_specialInstructionsEdit);
    const QSignalBlocker notesBlocker(m_subNotesEdit);

    ClassMngr::Next::Platform::
        ApplicationServicesSubPrepPreferencesPort
        subPrepPreferencesPort(*m_services);
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
    if (!m_services)
    {
        return;
    }

    const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
    const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);

    ClassMngr::Next::Platform::
        ApplicationServicesSubPrepPersonalZoomPreferencesPort
        personalZoomPreferencesPort(*m_services);
    const auto storedPreferences =
        personalZoomPreferencesPort.load();
    if (!storedPreferences)
    {
        return;
    }

    const auto& preferences =
        storedPreferences.value();
    const auto fromUtf8 = [](const std::string& value)
    {
        return QString::fromUtf8(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    };
    const QString loginId =
        fromUtf8(preferences.loginId);
    const QString password =
        fromUtf8(preferences.password);

    m_zoomLoginIdEdit->setText(
        preferences.unavailable
            ? NotAvailableText
            : valueOrNa(loginId)
        );
    m_zoomPasswordEdit->setText(
        preferences.unavailable
            ? NotAvailableText
            : valueOrNa(password)
        );

    updateReadOnlyFieldWidths();
}

void SubPrepPage::loadCampuses()
{
    ClassMngr::Next::Platform::
        ApplicationServicesCurrentCampusPreferencesPort
        currentCampusPreferencesPort(m_services);

    if (!currentCampusPreferencesPort.isAvailable())
    {
        return;
    }

    const bool wasLoading =
        m_loading;
    m_loading = true;

    m_campuses =
        ClassMngr::Next::Platform::
            SubPrepCampusDirectoryQueryAdapter().loadCampuses();

    const std::string storedCampus =
        currentCampusPreferencesPort.read();
    const QString savedCampus = QString::fromUtf8(
        storedCampus.data(),
        static_cast<qsizetype>(storedCampus.size())
        );

    QString campusId;

    for (const auto& campus : std::as_const(m_campuses))
    {
        const QString candidateId = campusMetadataText(campus.id);
        if (
            candidateId.compare(
                savedCampus.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            || campusDisplayName(campus).compare(
                savedCampus.trimmed(),
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campusId = candidateId;
            break;
        }
    }

    if (campusId.isEmpty() && !m_campuses.empty())
    {
        campusId = campusMetadataText(m_campuses.front().id);
    }

    loadCampusFields(campusId);
    updateReadOnlyFieldWidths();

    m_loading = wasLoading;
}

void SubPrepPage::loadCampusFields(
    const QString& campusId
    )
{
    const ClassMngr::Next::Application::SubPrepCampusMetadata* campus =
        nullptr;

    for (const auto& candidate : std::as_const(m_campuses))
    {
        if (
            campusMetadataText(candidate.id).compare(
                campusId,
                Qt::CaseInsensitive
                ) == 0
            )
        {
            campus = &candidate;
            break;
        }
    }

    const QSignalBlocker officeBlocker(m_officeNumberEdit);
    const QSignalBlocker wifiBlocker(m_officeWifiEdit);
    const QSignalBlocker wifiPasswordBlocker(m_officeWifiPasswordEdit);
    const QSignalBlocker photocopierBlocker(m_photocopierCodeEdit);
    const auto detailText = [](const std::string* value)
    {
        return value
            ? valueOrNa(campusMetadataText(*value))
            : NotAvailableText;
    };

    m_officeNumberEdit->setText(
        detailText(campus ? &campus->officeNumber : nullptr)
        );
    m_officeWifiEdit->setText(
        detailText(campus ? &campus->wifiName : nullptr)
        );
    m_officeWifiPasswordEdit->setText(
        detailText(campus ? &campus->wifiPassword : nullptr)
        );
    m_photocopierCodeEdit->setText(
        detailText(campus ? &campus->photocopierCode : nullptr)
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
    ClassMngr::Next::Platform::
        ApplicationServicesCurrentCampusPreferencesPort
        currentCampusPreferencesPort(m_services);

    if (!currentCampusPreferencesPort.isAvailable())
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
        subPrepPreferencesPort(*m_services);
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
