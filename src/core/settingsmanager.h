#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <memory>
#include <optional>
#include <functional>

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

        static constexpr auto THEME_PREFERENCE =
            "options/theme";

        static constexpr auto SAVE_MODE =
            "app/saveMode";

        static constexpr auto SAVE_MODE_PREFERENCE =
            "options/saveMode";

        static constexpr auto FONT_SIZE_PREFERENCE =
            "options/fontSize";

        static constexpr auto DOCUMENT_VIEWER_BACKGROUND_PREFERENCE =
            "options/documentViewerBackground";

        static constexpr auto DOCUMENT_PAGE_SPACING_PREFERENCE =
            "options/documentPageSpacing";

        static constexpr auto LANGUAGE_PREFERENCE =
            "options/language";

        static constexpr auto AI_COMMENT_VOICE_PREFERENCE =
            "options/aiCommentVoice";

        static constexpr auto AI_COMMENT_PROVIDER_PREFERENCE =
            "options/aiCommentProvider";

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

        static constexpr auto UPCOMING_BIRTHDAYS_DISMISSED_DATE =
            "notifications/upcomingBirthdaysDismissedDate";

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

    // An empty callback uses QSettings::sync/status. Injection lets boundary
    // tests force a write failure without relying on filesystem permissions.
    using ThemePreferenceSync = std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeThemePreference(
        int theme,
        const ThemePreferenceSync& synchronize = {}
        );

    // =====================================================
    // Save Mode
    // =====================================================

    QString getSaveMode() const;
    void setSaveMode(const QString& mode);

    // An empty callback uses QSettings::sync/status. Injection lets boundary
    // tests force a write failure without relying on filesystem permissions.
    using SaveModePreferenceSync = std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeSaveModePreference(
        int mode,
        const SaveModePreferenceSync& synchronize = {}
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores the previous key before returning.
    using FontSizePreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeFontSizePreference(
        int fontSize,
        const FontSizePreferenceSync& synchronize = {}
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores the previous key before returning.
    using AiCommentVoicePreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeAiCommentVoicePreference(
        int voice,
        const AiCommentVoicePreferenceSync& synchronize = {}
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores the previous key before returning.
    using AiCommentProviderPreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeAiCommentProviderPreference(
        int provider,
        const AiCommentProviderPreferenceSync& synchronize = {}
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores the previous key before returning.
    using LanguagePreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeLanguagePreference(
        int language,
        const LanguagePreferenceSync& synchronize = {}
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores the previous key before returning.
    using DocumentViewerBackgroundPreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeDocumentViewerBackgroundPreference(
        int background,
        const DocumentViewerBackgroundPreferenceSync& synchronize = {}
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores the previous key before returning.
    using DocumentPageSpacingPreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeDocumentPageSpacingPreference(
        int spacing,
        const DocumentPageSpacingPreferenceSync& synchronize = {}
        );

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

    // An empty callback uses QSettings::sync/status. This checked writer is
    // limited to the JSON campus preference and restores its prior state on
    // sync failure so callers can retry without disturbing other settings.
    using LastCampusJsonIdPreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeLastCampusJsonIdPreference(
        const QString& campusId,
        const LastCampusJsonIdPreferenceSync& synchronize = {}
        );

    // =====================================================
    // Sidebar
    // =====================================================

    bool sidebarTooltipsEnabled() const;
    void setSidebarTooltipsEnabled(
        bool enabled
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores both prior keys before returning.
    using SidebarDisplayPreferencesSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeSidebarDisplayPreferences(
        bool sidebarTooltipsEnabled,
        bool sidebarMarqueeEnabled,
        const SidebarDisplayPreferencesSync& synchronize = {}
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

    // An empty callback uses QSettings::sync/status. Injection lets boundary
    // tests force a failed write without relying on filesystem permissions.
    using ExcelImportTimeoutPreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeExcelImportTimeoutPreference(
        int seconds,
        const ExcelImportTimeoutPreferenceSync& synchronize = {}
        );

    // =====================================================
    // Updates
    // =====================================================

    bool automaticUpdateChecksEnabled() const;
    void setAutomaticUpdateChecksEnabled(
        bool enabled
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // writer reports failure and restores the previous key before returning.
    using AutomaticUpdatePreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeAutomaticUpdatePreference(
        bool enabled,
        const AutomaticUpdatePreferenceSync& synchronize = {}
        );

    // An empty callback uses QSettings::sync/status. The preference-specific
    // write and clear operations report failure and restore the previous key
    // state before returning.
    using SkippedUpdateVersionPreferenceSync =
        std::function<QSettings::Status(QSettings&)>;

    [[nodiscard]] QSettings::Status writeSkippedUpdateVersionPreference(
        const QString& version,
        const SkippedUpdateVersionPreferenceSync& synchronize = {}
        );

    [[nodiscard]] QSettings::Status clearSkippedUpdateVersionPreference(
        const SkippedUpdateVersionPreferenceSync& synchronize = {}
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
