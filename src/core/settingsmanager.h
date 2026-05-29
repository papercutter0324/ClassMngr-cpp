#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <optional>

class SettingsManager
{
public:
    // =====================================================
    // Singleton Access
    // =====================================================

    static SettingsManager& instance();

    // =====================================================
    // Disable Copy / Move
    // =====================================================

    SettingsManager(
        const SettingsManager&
        ) = delete;

    SettingsManager& operator=(
        const SettingsManager&
        ) = delete;

    SettingsManager(
        SettingsManager&&
        ) = delete;

    SettingsManager& operator=(
        SettingsManager&&
        ) = delete;

    // =====================================================
    // Organization / App
    // =====================================================

    static constexpr auto ORG =
        "PaperCloud";
    static constexpr auto APP =
        "ClassMngr";

    // =====================================================
    // Keys
    // =====================================================

    struct Keys
    {
        static constexpr auto THEME =
            "ui/theme";

        static constexpr auto SAVE_MODE =
            "app/saveMode";

        static constexpr auto WINDOW_GEOMETRY =
            "ui/windowGeometry";

        static constexpr auto LAST_CAMPUS_ID =
            "campus/lastSelectedId";

        static constexpr auto RECENT_FILES =
            "files/recent";

        static constexpr auto LAST_FILE =
            "files/last";
    };

    // =====================================================
    // Generic Helpers
    // =====================================================

    QVariant get(
        const QString& key,
        const QVariant& defaultValue = QVariant()
        ) const;

    void set(
        const QString& key,
        const QVariant& value
        );

    void remove(
        const QString& key
        );

    void clear();

    void sync();

    // =====================================================
    // Theme
    // =====================================================

    QString getTheme() const;
    void setTheme(const QString& theme);

    // =====================================================
    // Save Mode
    // =====================================================

    QString getSaveMode() const;
    void setSaveMode(const QString& mode);

    // =====================================================
    // Window Geometry
    // =====================================================

    QByteArray getWindowGeometry() const;
    void setWindowGeometry(
        const QByteArray& geometry
        );

    // =====================================================
    // Campus
    // =====================================================

    std::optional<int> getLastCampusId() const;
    void setLastCampusId(
        int campusId
        );

    // =====================================================
    // Recent Files
    // =====================================================

    QStringList getRecentFiles() const;

    void setRecentFiles(
        const QStringList& files
        );

    void clearRecentFiles();

    QString getLastFile() const;

    void setLastFile(
        const QString& path
        );

private:
    // =====================================================
    // Private Constructor
    // =====================================================

    SettingsManager();

    // =====================================================
    // State
    // =====================================================

    QSettings m_settings;
};

#endif // SETTINGSMANAGER_H