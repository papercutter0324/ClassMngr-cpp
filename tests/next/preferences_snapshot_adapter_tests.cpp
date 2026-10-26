#include "next/application/preferences_snapshot_adapter.h"

#include <iostream>
#include <stdexcept>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{
void expect(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
struct ThemePort final : ThemePreferencesPort
{
    mutable Theme value = Theme::SystemDefault;
    Theme read() const override { return value; }
    Domain::Result<void> write(Theme candidate) const override
    { value = candidate; return Domain::Result<void>::success(); }
};
struct LanguagePort final : LanguagePreferencesPort
{
    mutable LanguagePreference value = LanguagePreference::SystemDefault;
    LanguagePreference read() const override { return value; }
    Domain::Result<void> write(LanguagePreference candidate) const override
    { value = candidate; return Domain::Result<void>::success(); }
    void clear() const override { value = LanguagePreference::SystemDefault; }
};
struct FontPort final : FontSizePreferencesPort
{
    mutable FontSize value = FontSize::Normal;
    FontSize read() const override { return value; }
    Domain::Result<void> write(FontSize candidate) const override
    { value = candidate; return Domain::Result<void>::success(); }
};
struct SidebarPort final : SidebarDisplayPreferencesPort
{
    SidebarDisplayPreferences value;
    bool failRead = false;
    SidebarDisplayPreferencesResult load() const override
    {
        if (failRead) return SidebarDisplayPreferencesResult::failure(
            {Domain::ErrorCode::Technical, "Sidebar read failed.", true});
        return SidebarDisplayPreferencesResult::success(value);
    }
    SidebarDisplayPreferencesSaveResult save(const SidebarDisplayPreferencesSaveRequest& candidate) override
    { value = candidate; return SidebarDisplayPreferencesSaveResult::success(); }
};
struct TeacherPort final : ShowAllTeachersPreferencesPort
{
    mutable bool value = true;
    bool read() const override { return value; }
    void write(bool candidate) const override { value = candidate; }
};
struct PowerPointPort final : PowerPointDataAccessNoticePreferencesPort
{
    mutable PowerPointDataAccessNoticePreferences value;
    PowerPointDataAccessNoticePreferences read() const override { return value; }
    void write(const PowerPointDataAccessNoticePreferences& candidate) const override { value = candidate; }
};
struct UpdatesPort final : AutomaticUpdatePreferencesPort
{
    mutable AutomaticUpdatePreferences value;
    AutomaticUpdatePreferences read() const override { return value; }
    Domain::Result<void> write(const AutomaticUpdatePreferences& candidate) const override
    { value = candidate; return Domain::Result<void>::success(); }
};
struct TimeoutPort final : ExcelImportTimeoutPreferencesPort
{
    mutable ExcelImportTimeoutPreferences value;
    mutable bool failWrite = false;
    ExcelImportTimeoutPreferences read() const override { return value; }
    Domain::Result<void> write(const ExcelImportTimeoutPreferences& candidate) const override
    {
        if (failWrite) return Domain::Result<void>::failure(
            {Domain::ErrorCode::Technical, "Timeout write failed.", true});
        value = candidate;
        return Domain::Result<void>::success();
    }
};
struct CampusPort final : LastSelectedCampusPort
{
    mutable std::optional<Domain::CampusId> value;
    mutable int writes = 0;
    std::optional<Domain::CampusId> read() const override { return value; }
    Domain::Result<void> write(const std::optional<Domain::CampusId>& candidate) const override
    { ++writes; value = candidate; return Domain::Result<void>::success(); }
};

void verify()
{
    ThemePort theme;
    LanguagePort language;
    FontPort font;
    SidebarPort sidebar;
    TeacherPort teachers;
    PowerPointPort powerpoint;
    UpdatesPort updates;
    TimeoutPort timeout;
    CampusPort campus;
    PreferencesSnapshotAdapter adapter({theme, language, font, sidebar, teachers, powerpoint, updates, timeout, campus});
    auto defaults = adapter.read();
    expect(defaults && defaults.value() == UserPreferencesState{}, "Default full read.");
    UserPreferencesState candidate;
    expect(bool(candidate.setThemePreference(ThemePreference::Dark)), "Theme valid.");
    expect(bool(candidate.setLanguagePreference(LanguagePreference::Korean)), "Language valid.");
    expect(bool(candidate.setFontSize(FontSize::ExtraLarge)), "Font valid.");
    expect(bool(candidate.setSidebarTooltipsEnabled(false)), "Sidebar tooltips valid.");
    expect(bool(candidate.setSidebarMarqueeEnabled(false)), "Sidebar marquee valid.");
    expect(bool(candidate.setShowAllTeachers(false)), "Teachers valid.");
    expect(bool(candidate.setShowPowerPointDataAccessNotice(false)), "Notice valid.");
    expect(bool(candidate.setAutomaticUpdateChecksEnabled(false)), "Updates valid.");
    expect(bool(candidate.setExcelImportTimeoutSeconds(300)), "Timeout valid.");
    expect(bool(candidate.setLastSelectedCampusId(Domain::CampusId::fromString("campus-a"))), "Campus valid.");
    timeout.failWrite = true;
    auto failed = adapter.persist(candidate.snapshot());
    expect(!failed && failed.error().message == "Timeout write failed." && failed.error().recoverable,
           "First error is preserved.");
    expect(campus.writes == 0, "Stop at first failure before campus.");
    auto partial = adapter.read();
    expect(partial && partial.value().snapshot().fontSize() == FontSize::ExtraLarge
           && !partial.value().snapshot().automaticUpdateChecksEnabled()
           && partial.value().snapshot().excelImportTimeoutSeconds() == 120
           && !partial.value().snapshot().lastSelectedCampusId(), "Earlier writes remain visible without rollback.");
    timeout.failWrite = false;
    expect(bool(adapter.persist(candidate.snapshot())), "Full idempotent retry succeeds.");
    const auto roundTrip = adapter.read();
    expect(roundTrip && roundTrip.value() == candidate, "Every modeled field round trips.");
    expect(bool(adapter.persist(candidate.snapshot())), "Repeated full write succeeds.");
    expect(bool(candidate.clearLastSelectedCampusId()), "Campus clearing valid.");
    expect(bool(adapter.persist(candidate.snapshot())), "Campus absence saves.");
    expect(!adapter.read().value().snapshot().lastSelectedCampusId(), "Campus absence round trips.");
    sidebar.failRead = true;
    expect(!adapter.read() && adapter.read().error().message == "Sidebar read failed.", "Reader failure surfaces.");
    sidebar.failRead = false;
    font.value = static_cast<FontSize>(99);
    expect(!adapter.read() && adapter.read().error().code == Domain::ErrorCode::InvalidInput,
           "Invalid typed inputs cannot bypass state validation.");
}
}
int main()
{
    try { verify(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "PreferencesSnapshotAdapter tests passed.\n";
}
