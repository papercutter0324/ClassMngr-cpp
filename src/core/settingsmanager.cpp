#include "settingsmanager.h"

#include <QDir>
#include <QtGlobal>

namespace
{
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

bool SettingsManager::sidebarMarqueeEnabled() const
{
    return get(
        Keys::SIDEBAR_MARQUEE_ENABLED,
        false
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
