#include "settingsmanager.h"

#include <QDir>
#include <QtGlobal>

namespace
{
constexpr int DefaultExcelImportTimeoutSeconds = 120;

bool isSupportedExcelImportTimeoutSeconds(
    int seconds
    )
{
    switch (seconds)
    {
    case 30:
    case 60:
    case 120:
    case 300:
        return true;
    default:
        return false;
    }
}

std::unique_ptr<QSettings> createSettings()
{
    const QString settingsRoot =
        qEnvironmentVariable("CLASSMNGR_SETTINGS_ROOT");

    if (settingsRoot.trimmed().isEmpty())
    {
        return std::make_unique<QSettings>(
            SettingsManager::ORG,
            SettingsManager::APP
            );
    }

    QDir directory(settingsRoot);

    directory.mkpath(
        QString::fromUtf8(SettingsManager::ORG)
        );

    return std::make_unique<QSettings>(
        directory.filePath(
            QStringLiteral("%1/%2.ini")
                .arg(
                    QString::fromUtf8(SettingsManager::ORG),
                    QString::fromUtf8(SettingsManager::APP)
                    )
            ),
        QSettings::IniFormat
        );
}
}

// =========================================================
// Singleton Access
// =========================================================

SettingsManager&
SettingsManager::instance()
{
    static SettingsManager instance;
    return instance;
}

// =========================================================
// Init
// =========================================================

SettingsManager::SettingsManager()
    : m_settings(createSettings())
{
}

// =========================================================
// Generic Helpers
// =========================================================

QVariant SettingsManager::get(
    const QString& key,
    const QVariant& defaultValue
    ) const
{
    return m_settings->value(key, defaultValue);
}

void SettingsManager::set(
    const QString& key,
    const QVariant& value
    )
{
    m_settings->setValue(key, value);
}

void SettingsManager::remove(
    const QString& key
    )
{
    m_settings->remove(key);
}

void SettingsManager::clear()
{
    m_settings->clear();
}

void SettingsManager::sync()
{
    m_settings->sync();
}

// =========================================================
// Theme
// =========================================================

QString SettingsManager::getTheme() const
{
    return get(
               Keys::THEME,
               "system"
               ).toString();
}

void SettingsManager::setTheme(
    const QString& theme
    )
{
    set(Keys::THEME, theme);
}

QSettings::Status SettingsManager::writeThemePreference(
    const int theme,
    const ThemePreferenceSync& synchronize
    )
{
    const bool hadPreviousTheme = m_settings->contains(Keys::THEME_PREFERENCE);
    const QVariant previousTheme = m_settings->value(Keys::THEME_PREFERENCE);
    m_settings->setValue(Keys::THEME_PREFERENCE, theme);
    QSettings::Status status;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }
    if (status != QSettings::NoError)
    {
        // QSettings may retry synchronization during destruction or share its
        // pending changes with another instance. Remove the attempted theme
        // before replacing the failed instance.
        if (hadPreviousTheme)
        {
            m_settings->setValue(Keys::THEME_PREFERENCE, previousTheme);
        }
        else
        {
            m_settings->remove(Keys::THEME_PREFERENCE);
        }
        // Destroy the failed instance before opening a fresh one, so neither
        // its unsaved candidate nor its latched error survives the retry.
        m_settings.reset();
        m_settings = createSettings();
    }
    return status;
}

// =========================================================
// Save Mode
// =========================================================

QString SettingsManager::getSaveMode() const
{
    return get(
               Keys::SAVE_MODE,
               "Automatic (on change)"
               ).toString();
}

void SettingsManager::setSaveMode(
    const QString& mode
    )
{
    set(Keys::SAVE_MODE, mode);
}

QSettings::Status SettingsManager::writeSaveModePreference(
    const int mode,
    const SaveModePreferenceSync& synchronize
    )
{
    const bool hadPreviousMode =
        m_settings->contains(Keys::SAVE_MODE_PREFERENCE);
    const QVariant previousMode =
        m_settings->value(Keys::SAVE_MODE_PREFERENCE);
    m_settings->setValue(
        Keys::SAVE_MODE_PREFERENCE,
        mode
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore the attempted key before replacing this instance so its
        // pending candidate cannot be retried during teardown.
        if (hadPreviousMode)
        {
            m_settings->setValue(
                Keys::SAVE_MODE_PREFERENCE,
                previousMode
                );
        }
        else
        {
            m_settings->remove(Keys::SAVE_MODE_PREFERENCE);
        }

        // A failed QSettings instance retains its error status. Recreate it so
        // callers can read the restored value and retry a later write.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeFontSizePreference(
    const int fontSize,
    const FontSizePreferenceSync& synchronize
    )
{
    const bool hadPreviousFontSize =
        m_settings->contains(Keys::FONT_SIZE_PREFERENCE);
    const QVariant previousFontSize =
        m_settings->value(Keys::FONT_SIZE_PREFERENCE);
    m_settings->setValue(
        Keys::FONT_SIZE_PREFERENCE,
        fontSize
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore this key before replacing the failed instance so its
        // pending candidate cannot be retried during teardown.
        if (hadPreviousFontSize)
        {
            m_settings->setValue(
                Keys::FONT_SIZE_PREFERENCE,
                previousFontSize
                );
        }
        else
        {
            m_settings->remove(Keys::FONT_SIZE_PREFERENCE);
        }

        // Reopen the store so reads see the restored value and a later write
        // can retry with a clean QSettings status.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeAiCommentVoicePreference(
    const int voice,
    const AiCommentVoicePreferenceSync& synchronize
    )
{
    const bool hadPreviousVoice =
        m_settings->contains(Keys::AI_COMMENT_VOICE_PREFERENCE);
    const QVariant previousVoice =
        m_settings->value(Keys::AI_COMMENT_VOICE_PREFERENCE);

    m_settings->setValue(
        Keys::AI_COMMENT_VOICE_PREFERENCE,
        voice
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Remove the pending candidate before destroying this instance so it
        // cannot be retried during teardown.
        if (hadPreviousVoice)
        {
            m_settings->setValue(
                Keys::AI_COMMENT_VOICE_PREFERENCE,
                previousVoice
                );
        }
        else
        {
            m_settings->remove(Keys::AI_COMMENT_VOICE_PREFERENCE);
        }

        // A failed QSettings instance keeps its error status. Reopen the
        // store so reads see the restored preference and retries can succeed.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeAiCommentProviderPreference(
    const int provider,
    const AiCommentProviderPreferenceSync& synchronize
    )
{
    const bool hadPreviousProvider =
        m_settings->contains(Keys::AI_COMMENT_PROVIDER_PREFERENCE);
    const QVariant previousProvider =
        m_settings->value(Keys::AI_COMMENT_PROVIDER_PREFERENCE);

    m_settings->setValue(
        Keys::AI_COMMENT_PROVIDER_PREFERENCE,
        provider
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Remove the pending candidate before destroying this instance so it
        // cannot be retried during teardown.
        if (hadPreviousProvider)
        {
            m_settings->setValue(
                Keys::AI_COMMENT_PROVIDER_PREFERENCE,
                previousProvider
                );
        }
        else
        {
            m_settings->remove(Keys::AI_COMMENT_PROVIDER_PREFERENCE);
        }

        // A failed QSettings instance keeps its error status. Reopen the
        // store so reads see the restored value and retries can succeed.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeAiCommentCustomWebsitePreferences(
    const QString& websiteUrl,
    const AiCommentCustomWebsitePreferenceSync& synchronize
    )
{
    const bool hadPreviousProvider =
        m_settings->contains(Keys::AI_COMMENT_PROVIDER_PREFERENCE);
    const QVariant previousProvider =
        m_settings->value(Keys::AI_COMMENT_PROVIDER_PREFERENCE);
    const bool hadPreviousWebsiteUrl =
        m_settings->contains(
            Keys::AI_COMMENT_CUSTOM_WEBSITE_URL_PREFERENCE
            );
    const QVariant previousWebsiteUrl =
        m_settings->value(
            Keys::AI_COMMENT_CUSTOM_WEBSITE_URL_PREFERENCE
            );

    m_settings->setValue(
        Keys::AI_COMMENT_PROVIDER_PREFERENCE,
        AI_COMMENT_CUSTOM_WEBSITE_PROVIDER_VALUE
        );
    m_settings->setValue(
        Keys::AI_COMMENT_CUSTOM_WEBSITE_URL_PREFERENCE,
        websiteUrl
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore both captured states before destroying this instance so
        // its failed candidates cannot be retried during teardown.
        if (hadPreviousProvider)
        {
            m_settings->setValue(
                Keys::AI_COMMENT_PROVIDER_PREFERENCE,
                previousProvider
                );
        }
        else
        {
            m_settings->remove(Keys::AI_COMMENT_PROVIDER_PREFERENCE);
        }

        if (hadPreviousWebsiteUrl)
        {
            m_settings->setValue(
                Keys::AI_COMMENT_CUSTOM_WEBSITE_URL_PREFERENCE,
                previousWebsiteUrl
                );
        }
        else
        {
            m_settings->remove(
                Keys::AI_COMMENT_CUSTOM_WEBSITE_URL_PREFERENCE
                );
        }

        // Reopen after a reported error because QSettings retains its status.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeLanguagePreference(
    const int language,
    const LanguagePreferenceSync& synchronize
    )
{
    const bool hadPreviousLanguage =
        m_settings->contains(Keys::LANGUAGE_PREFERENCE);
    const QVariant previousLanguage =
        m_settings->value(Keys::LANGUAGE_PREFERENCE);

    m_settings->setValue(
        Keys::LANGUAGE_PREFERENCE,
        language
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Remove the pending candidate before destroying this instance so it
        // cannot be retried during teardown.
        if (hadPreviousLanguage)
        {
            m_settings->setValue(
                Keys::LANGUAGE_PREFERENCE,
                previousLanguage
                );
        }
        else
        {
            m_settings->remove(Keys::LANGUAGE_PREFERENCE);
        }

        // A failed QSettings instance keeps its error status. Reopen the
        // store so reads see the restored value and retries can succeed.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeDocumentViewerBackgroundPreference(
    const int background,
    const DocumentViewerBackgroundPreferenceSync& synchronize
    )
{
    const bool hadPreviousBackground =
        m_settings->contains(Keys::DOCUMENT_VIEWER_BACKGROUND_PREFERENCE);
    const QVariant previousBackground =
        m_settings->value(Keys::DOCUMENT_VIEWER_BACKGROUND_PREFERENCE);

    m_settings->setValue(
        Keys::DOCUMENT_VIEWER_BACKGROUND_PREFERENCE,
        background
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore the attempted value before destroying this instance so it
        // cannot be retried during teardown.
        if (hadPreviousBackground)
        {
            m_settings->setValue(
                Keys::DOCUMENT_VIEWER_BACKGROUND_PREFERENCE,
                previousBackground
                );
        }
        else
        {
            m_settings->remove(
                Keys::DOCUMENT_VIEWER_BACKGROUND_PREFERENCE
                );
        }

        // A failed QSettings instance keeps its error status. Reopen the
        // store so reads see the restored preference and retries can work.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeDocumentPageSpacingPreference(
    const int spacing,
    const DocumentPageSpacingPreferenceSync& synchronize
    )
{
    const bool hadPreviousSpacing =
        m_settings->contains(Keys::DOCUMENT_PAGE_SPACING_PREFERENCE);
    const QVariant previousSpacing =
        m_settings->value(Keys::DOCUMENT_PAGE_SPACING_PREFERENCE);

    m_settings->setValue(
        Keys::DOCUMENT_PAGE_SPACING_PREFERENCE,
        spacing
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore the attempted value before destroying this instance so it
        // cannot be retried during teardown.
        if (hadPreviousSpacing)
        {
            m_settings->setValue(
                Keys::DOCUMENT_PAGE_SPACING_PREFERENCE,
                previousSpacing
                );
        }
        else
        {
            m_settings->remove(
                Keys::DOCUMENT_PAGE_SPACING_PREFERENCE
                );
        }

        // A failed QSettings instance keeps its error status. Reopen the
        // store so reads see the restored preference and retries can work.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

// =========================================================
// Window Geometry
// =========================================================

QByteArray SettingsManager::getWindowGeometry() const
{
    return get(
               Keys::WINDOW_GEOMETRY
               ).toByteArray();
}

void SettingsManager::setWindowGeometry(
    const QByteArray& geometry
    )
{
    set(
        Keys::WINDOW_GEOMETRY,
        geometry
        );
}

QByteArray SettingsManager::getSplitterState() const
{
    return get(
        Keys::SPLITTER_STATE
        ).toByteArray();
}

void SettingsManager::setSplitterState(
    const QByteArray& state
    )
{
    set(
        Keys::SPLITTER_STATE,
        state
        );
}

// =========================================================
// Campus
// =========================================================

std::optional<int>
SettingsManager::getLastCampusId() const
{
    QVariant value =
        get(Keys::LAST_CAMPUS_ID);

    if (!value.isValid())
    {
        return std::nullopt;
    }

    return value.toInt();
}

void SettingsManager::setLastCampusId(
    int campusId
    )
{
    set(
        Keys::LAST_CAMPUS_ID,
        campusId
        );
}

QString SettingsManager::getLastCampusJsonId() const
{
    return get(
        Keys::LAST_CAMPUS_JSON_ID,
        QString()
        ).toString();
}

void SettingsManager::setLastCampusJsonId(
    const QString& campusId
    )
{
    set(
        Keys::LAST_CAMPUS_JSON_ID,
        campusId
        );
}

QSettings::Status SettingsManager::writeLastCampusJsonIdPreference(
    const QString& campusId,
    const LastCampusJsonIdPreferenceSync& synchronize
    )
{
    const bool hadPreviousCampusId =
        m_settings->contains(Keys::LAST_CAMPUS_JSON_ID);
    const QVariant previousCampusId =
        m_settings->value(Keys::LAST_CAMPUS_JSON_ID);

    m_settings->setValue(
        Keys::LAST_CAMPUS_JSON_ID,
        campusId
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore only the attempted preference before destroying this
        // instance, so unrelated settings and the legacy integer key remain
        // untouched and the failed candidate cannot be retried on teardown.
        if (hadPreviousCampusId)
        {
            m_settings->setValue(
                Keys::LAST_CAMPUS_JSON_ID,
                previousCampusId
                );
        }
        else
        {
            m_settings->remove(Keys::LAST_CAMPUS_JSON_ID);
        }

        // A failed QSettings instance retains its status. Reopen the store so
        // the restored value is visible and a later write can retry.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

// =========================================================
// Sidebar
// =========================================================

bool SettingsManager::sidebarTooltipsEnabled() const
{
    return get(
        Keys::SIDEBAR_TOOLTIPS_ENABLED,
        true
        ).toBool();
}

void SettingsManager::setSidebarTooltipsEnabled(
    bool enabled
    )
{
    set(
        Keys::SIDEBAR_TOOLTIPS_ENABLED,
        enabled
        );
}

QSettings::Status SettingsManager::writeSidebarDisplayPreferences(
    const bool sidebarTooltipsEnabled,
    const bool sidebarMarqueeEnabled,
    const SidebarDisplayPreferencesSync& synchronize
    )
{
    const bool hadPreviousTooltips =
        m_settings->contains(Keys::SIDEBAR_TOOLTIPS_ENABLED);
    const QVariant previousTooltips =
        m_settings->value(Keys::SIDEBAR_TOOLTIPS_ENABLED);
    const bool hadPreviousMarquee =
        m_settings->contains(Keys::SIDEBAR_MARQUEE_ENABLED);
    const QVariant previousMarquee =
        m_settings->value(Keys::SIDEBAR_MARQUEE_ENABLED);

    m_settings->setValue(
        Keys::SIDEBAR_TOOLTIPS_ENABLED,
        sidebarTooltipsEnabled
        );
    m_settings->setValue(
        Keys::SIDEBAR_MARQUEE_ENABLED,
        sidebarMarqueeEnabled
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore the attempted values before replacing this instance so its
        // pending candidates cannot be retried during teardown.
        if (hadPreviousTooltips)
        {
            m_settings->setValue(
                Keys::SIDEBAR_TOOLTIPS_ENABLED,
                previousTooltips
                );
        }
        else
        {
            m_settings->remove(Keys::SIDEBAR_TOOLTIPS_ENABLED);
        }

        if (hadPreviousMarquee)
        {
            m_settings->setValue(
                Keys::SIDEBAR_MARQUEE_ENABLED,
                previousMarquee
                );
        }
        else
        {
            m_settings->remove(Keys::SIDEBAR_MARQUEE_ENABLED);
        }

        // A failed QSettings instance keeps its error status. Reopen the
        // store so reads see both restored preferences and retries can work.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

bool SettingsManager::sidebarMarqueeEnabled() const
{
    return get(
        Keys::SIDEBAR_MARQUEE_ENABLED,
        true
        ).toBool();
}

void SettingsManager::setSidebarMarqueeEnabled(
    bool enabled
    )
{
    set(
        Keys::SIDEBAR_MARQUEE_ENABLED,
        enabled
        );
}

bool SettingsManager::showAllKoreanTeachers() const
{
    return get(
        Keys::SHOW_ALL_KOREAN_TEACHERS,
        true
        ).toBool();
}

void SettingsManager::setShowAllKoreanTeachers(
    bool enabled
    )
{
    set(
        Keys::SHOW_ALL_KOREAN_TEACHERS,
        enabled
        );
}

bool SettingsManager::showPowerPointDataAccessNotice() const
{
    return get(
        Keys::SHOW_POWERPOINT_DATA_ACCESS_NOTICE,
        true
        ).toBool();
}

void SettingsManager::setShowPowerPointDataAccessNotice(
    bool enabled
    )
{
    set(
        Keys::SHOW_POWERPOINT_DATA_ACCESS_NOTICE,
        enabled
        );
}

// =========================================================
// Imports
// =========================================================

int SettingsManager::excelImportTimeoutSeconds() const
{
    const int seconds =
        get(
            Keys::EXCEL_IMPORT_TIMEOUT_SECONDS,
            DefaultExcelImportTimeoutSeconds
            ).toInt();
    return isSupportedExcelImportTimeoutSeconds(seconds)
        ? seconds
        : DefaultExcelImportTimeoutSeconds;
}

void SettingsManager::setExcelImportTimeoutSeconds(
    int seconds
    )
{
    set(
        Keys::EXCEL_IMPORT_TIMEOUT_SECONDS,
        isSupportedExcelImportTimeoutSeconds(seconds)
            ? seconds
            : DefaultExcelImportTimeoutSeconds
        );
}

QSettings::Status SettingsManager::writeExcelImportTimeoutPreference(
    const int seconds,
    const ExcelImportTimeoutPreferenceSync& synchronize
    )
{
    const bool hadPreviousTimeout =
        m_settings->contains(Keys::EXCEL_IMPORT_TIMEOUT_SECONDS);
    const QVariant previousTimeout =
        m_settings->value(Keys::EXCEL_IMPORT_TIMEOUT_SECONDS);
    m_settings->setValue(
        Keys::EXCEL_IMPORT_TIMEOUT_SECONDS,
        isSupportedExcelImportTimeoutSeconds(seconds)
            ? seconds
            : DefaultExcelImportTimeoutSeconds
        );

    QSettings::Status status;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore this key in the failed instance before destroying it. That
        // prevents its cached candidate from being retried during teardown.
        if (hadPreviousTimeout)
        {
            m_settings->setValue(
                Keys::EXCEL_IMPORT_TIMEOUT_SECONDS,
                previousTimeout
                );
        }
        else
        {
            m_settings->remove(Keys::EXCEL_IMPORT_TIMEOUT_SECONDS);
        }

        // Replace the failed instance so its candidate and latched error do
        // not affect reads or a later retry.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

// =========================================================
// Updates
// =========================================================

bool SettingsManager::automaticUpdateChecksEnabled() const
{
    return get(
        Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED,
        true
        ).toBool();
}

void SettingsManager::setAutomaticUpdateChecksEnabled(
    bool enabled
    )
{
    set(
        Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED,
        enabled
        );
}

QSettings::Status SettingsManager::writeAutomaticUpdatePreference(
    const bool enabled,
    const AutomaticUpdatePreferenceSync& synchronize
    )
{
    const bool hadPreviousPreference =
        m_settings->contains(Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED);
    const QVariant previousPreference =
        m_settings->value(Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED);

    m_settings->setValue(
        Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED,
        enabled
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Remove the pending candidate before destroying this instance so
        // QSettings cannot retry it during teardown.
        if (hadPreviousPreference)
        {
            m_settings->setValue(
                Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED,
                previousPreference
                );
        }
        else
        {
            m_settings->remove(
                Keys::AUTOMATIC_UPDATE_CHECKS_ENABLED
                );
        }

        // A failed instance keeps its error status. Reopen the store so reads
        // see the previous value and a later write can retry cleanly.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::writeSkippedUpdateVersionPreference(
    const QString& version,
    const SkippedUpdateVersionPreferenceSync& synchronize
    )
{
    const bool hadPreviousVersion =
        m_settings->contains(Keys::SKIPPED_UPDATE_VERSION);
    const QVariant previousVersion =
        m_settings->value(Keys::SKIPPED_UPDATE_VERSION);

    m_settings->setValue(
        Keys::SKIPPED_UPDATE_VERSION,
        version
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore the attempted value before destroying this instance so it
        // cannot be retried during teardown.
        if (hadPreviousVersion)
        {
            m_settings->setValue(
                Keys::SKIPPED_UPDATE_VERSION,
                previousVersion
                );
        }
        else
        {
            m_settings->remove(Keys::SKIPPED_UPDATE_VERSION);
        }

        // A failed QSettings instance keeps its error status. Reopen the
        // store so reads see the restored preference and retries can work.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status SettingsManager::clearSkippedUpdateVersionPreference(
    const SkippedUpdateVersionPreferenceSync& synchronize
    )
{
    const bool hadPreviousVersion =
        m_settings->contains(Keys::SKIPPED_UPDATE_VERSION);
    const QVariant previousVersion =
        m_settings->value(Keys::SKIPPED_UPDATE_VERSION);

    m_settings->remove(
        Keys::SKIPPED_UPDATE_VERSION
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore the previous key before destroying this instance so its
        // pending removal cannot be retried during teardown.
        if (hadPreviousVersion)
        {
            m_settings->setValue(
                Keys::SKIPPED_UPDATE_VERSION,
                previousVersion
                );
        }
        else
        {
            m_settings->remove(
                Keys::SKIPPED_UPDATE_VERSION
                );
        }

        // Reopen the store so reads see the restored preference and a later
        // clear can retry with a clean QSettings status.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QString SettingsManager::skippedUpdateVersion() const
{
    return get(
        Keys::SKIPPED_UPDATE_VERSION,
        QString()
        ).toString().trimmed();
}

void SettingsManager::setSkippedUpdateVersion(
    const QString& version
    )
{
    set(
        Keys::SKIPPED_UPDATE_VERSION,
        version.trimmed()
        );
}

void SettingsManager::clearSkippedUpdateVersion()
{
    remove(
        Keys::SKIPPED_UPDATE_VERSION
        );
}

// =========================================================
// Recent Files
// =========================================================

QStringList
SettingsManager::getRecentFiles() const
{
    return get(
               Keys::RECENT_FILES,
               QStringList()
               ).toStringList();
}

void SettingsManager::setRecentFiles(
    const QStringList& files
    )
{
    set(
        Keys::RECENT_FILES,
        files
        );

    sync();
}

void SettingsManager::clearRecentFiles()
{
    setRecentFiles({});
}

QString SettingsManager::getLastFile() const
{
    return get(
               Keys::LAST_FILE,
               ""
               ).toString();
}

void SettingsManager::setLastFile(
    const QString& path
    )
{
    set(
        Keys::LAST_FILE,
        path
        );

    sync();
}

QSettings::Status SettingsManager::writeRecentWorkspaceHistoryPreferences(
    const QStringList& recentFiles,
    const QString& lastFile,
    const RecentWorkspaceHistoryPreferencesSync& synchronize
    )
{
    const bool hadPreviousRecentFiles =
        m_settings->contains(Keys::RECENT_FILES);
    const QVariant previousRecentFiles =
        m_settings->value(Keys::RECENT_FILES);
    const bool hadPreviousLastFile =
        m_settings->contains(Keys::LAST_FILE);
    const QVariant previousLastFile =
        m_settings->value(Keys::LAST_FILE);

    m_settings->setValue(
        Keys::RECENT_FILES,
        recentFiles
        );
    m_settings->setValue(
        Keys::LAST_FILE,
        lastFile
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore both history keys before destroying this instance so its
        // failed candidates cannot be retried during teardown.
        if (hadPreviousRecentFiles)
        {
            m_settings->setValue(
                Keys::RECENT_FILES,
                previousRecentFiles
                );
        }
        else
        {
            m_settings->remove(Keys::RECENT_FILES);
        }

        if (hadPreviousLastFile)
        {
            m_settings->setValue(
                Keys::LAST_FILE,
                previousLastFile
                );
        }
        else
        {
            m_settings->remove(Keys::LAST_FILE);
        }

        // Reopen after a failure because QSettings keeps the status latched;
        // unrelated preferences remain untouched and a later write can retry.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QString SettingsManager::getLastDatabaseDirectory() const
{
    return get(
               Keys::LAST_DATABASE_DIRECTORY,
               ""
               ).toString();
}

void SettingsManager::setLastDatabaseDirectory(
    const QString& path
    )
{
    set(
        Keys::LAST_DATABASE_DIRECTORY,
        path
        );

    sync();
}

QSettings::Status SettingsManager::writeLastDatabaseDirectoryPreference(
    const QString& path,
    const LastDatabaseDirectoryPreferenceSync& synchronize
    )
{
    const bool hadPreviousDirectory =
        m_settings->contains(Keys::LAST_DATABASE_DIRECTORY);
    const QVariant previousDirectory =
        m_settings->value(Keys::LAST_DATABASE_DIRECTORY);

    m_settings->setValue(
        Keys::LAST_DATABASE_DIRECTORY,
        path
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore only this preference before destroying the failed
        // instance, so its pending candidate cannot be retried on teardown.
        if (hadPreviousDirectory)
        {
            m_settings->setValue(
                Keys::LAST_DATABASE_DIRECTORY,
                previousDirectory
                );
        }
        else
        {
            m_settings->remove(Keys::LAST_DATABASE_DIRECTORY);
        }

        // QSettings keeps its error latched. Reopen it so reads observe the
        // restored value and a later checked write can retry successfully.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}

QSettings::Status
SettingsManager::writeUpcomingBirthdayDismissalDatePreference(
    const QDate& date,
    const UpcomingBirthdayDismissalPreferenceSync& synchronize
    )
{
    if (!date.isValid())
    {
        return QSettings::NoError;
    }

    const bool hadPreviousDate =
        m_settings->contains(Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE);
    const QVariant previousDate =
        m_settings->value(Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE);

    m_settings->setValue(
        Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE,
        date
        );

    QSettings::Status status = QSettings::NoError;
    if (synchronize)
    {
        status = synchronize(*m_settings);
    }
    else
    {
        m_settings->sync();
        status = m_settings->status();
    }

    if (status != QSettings::NoError)
    {
        // Restore only the dismissal date before destroying this instance,
        // so the failed candidate cannot be retried during teardown.
        if (hadPreviousDate)
        {
            m_settings->setValue(
                Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE,
                previousDate
                );
        }
        else
        {
            m_settings->remove(
                Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE
                );
        }

        // A failed QSettings instance retains its error status. Reopen it so
        // reads see the restored state and later writes can retry.
        m_settings.reset();
        m_settings = createSettings();
    }

    return status;
}
