#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <memory>
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

        static constexpr auto SPLITTER_STATE =
            "splitterState";

        static constexpr auto LAST_CAMPUS_ID =
            "campus/lastSelectedId";

        static constexpr auto LAST_CAMPUS_JSON_ID =
            "campus/lastSelectedJsonId";

        static constexpr auto SIDEBAR_TOOLTIPS_ENABLED =
            "options/sidebarTooltipsEnabled";

        static constexpr auto SIDEBAR_MARQUEE_ENABLED =
            "options/sidebarMarqueeEnabled";

        static constexpr auto SHOW_ALL_KOREAN_TEACHERS =
            "options/showAllKoreanTeachers";

        static constexpr auto SHOW_POWERPOINT_DATA_ACCESS_NOTICE =
            "options/showPowerPointDataAccessNotice";

        static constexpr auto EXCEL_IMPORT_TIMEOUT_SECONDS =
            "imports/excelTimeoutSeconds";

        static constexpr auto AUTOMATIC_UPDATE_CHECKS_ENABLED =
            "updates/automaticChecksEnabled";

        static constexpr auto SKIPPED_UPDATE_VERSION =
            "updates/skippedVersion";

        static constexpr auto RECENT_FILES =
            "files/recent";

        static constexpr auto LAST_FILE =
            "files/last";

        static constexpr auto LAST_DATABASE_DIRECTORY =
            "files/lastDirectory";
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

    QByteArray getSplitterState() const;
    void setSplitterState(
        const QByteArray& state
        );

    // =====================================================
    // Campus
    // =====================================================

    std::optional<int> getLastCampusId() const;
    void setLastCampusId(
        int campusId
        );

    QString getLastCampusJsonId() const;
    void setLastCampusJsonId(
        const QString& campusId
        );

    // =====================================================
    // Sidebar
    // =====================================================

    bool sidebarTooltipsEnabled() const;
    void setSidebarTooltipsEnabled(
        bool enabled
        );

    bool sidebarMarqueeEnabled() const;
    void setSidebarMarqueeEnabled(
        bool enabled
        );

    bool showAllKoreanTeachers() const;
    void setShowAllKoreanTeachers(
        bool enabled
        );

    bool showPowerPointDataAccessNotice() const;
    void setShowPowerPointDataAccessNotice(
        bool enabled
        );

    // =====================================================
    // Imports
    // =====================================================

    [[nodiscard]] int excelImportTimeoutSeconds() const;
    void setExcelImportTimeoutSeconds(
        int seconds
        );

    // =====================================================
    // Updates
    // =====================================================

    bool automaticUpdateChecksEnabled() const;
    void setAutomaticUpdateChecksEnabled(
        bool enabled
        );

    QString skippedUpdateVersion() const;
    void setSkippedUpdateVersion(
        const QString& version
        );
    void clearSkippedUpdateVersion();

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

    QString getLastDatabaseDirectory() const;

    void setLastDatabaseDirectory(
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

    std::unique_ptr<QSettings> m_settings;
};
