#include "next/application/user_preferences_state.h"

#include <QtTest/QtTest>

#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

class NextApplicationUserPreferencesTests final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsMatchVerifiedLegacyDefaults();
    void enumPreferencesAcceptEveryDeclaredValue();
    void fontSizesValidateAndParticipateInEquality();
    void flagsAcceptBothValues();
    void timeoutChoiceConversionsAreClosedAndExplicit();
    void validTimeoutsUpdateTheSnapshot();
    void invalidTimeoutPreservesSnapshotAndReturnsDeterministicError();
    void invalidEnumsPreserveSnapshotAndReturnDeterministicErrors();
    void campusSelectionIsTypedValidatedAndClearable();
    void invalidCampusPreservesSnapshotAndReturnsDeterministicError();
    void snapshotsAndStatesAreCopyableValueTypes();
};

void NextApplicationUserPreferencesTests::defaultsMatchVerifiedLegacyDefaults()
{
    UserPreferencesState state;
    const UserPreferencesSnapshot snapshot = state.snapshot();

    QCOMPARE(snapshot.fontSize(), FontSize::Normal);
    QCOMPARE(snapshot.themePreference(), ThemePreference::SystemDefault);
    QCOMPARE(snapshot.languagePreference(), LanguagePreference::SystemDefault);
    QVERIFY(snapshot.sidebarTooltipsEnabled());
    QVERIFY(snapshot.sidebarMarqueeEnabled());
    QVERIFY(snapshot.showAllTeachers());
    QVERIFY(snapshot.showAllKoreanTeachers());
    QVERIFY(snapshot.showPowerPointDataAccessNotice());
    QVERIFY(snapshot.showPowerPointNotice());
    QVERIFY(snapshot.automaticUpdateChecksEnabled());
    QCOMPARE(
        snapshot.excelImportTimeoutSeconds(),
        kDefaultExcelImportTimeoutSeconds
        );
    QCOMPARE(
        snapshot.excelImportTimeoutChoice(),
        kDefaultExcelImportTimeoutChoice
        );
    QVERIFY(!snapshot.lastSelectedCampusId().has_value());
}

void NextApplicationUserPreferencesTests::enumPreferencesAcceptEveryDeclaredValue()
{
    UserPreferencesState state;

    QVERIFY(state.setThemePreference(ThemePreference::Light));
    QCOMPARE(state.snapshot().themePreference(), ThemePreference::Light);
    QVERIFY(state.setThemePreference(ThemePreference::Dark));
    QCOMPARE(state.snapshot().themePreference(), ThemePreference::Dark);
    QVERIFY(state.setThemePreference(ThemePreference::SystemDefault));
    QCOMPARE(
        state.snapshot().themePreference(),
        ThemePreference::SystemDefault
        );

    QVERIFY(state.setLanguagePreference(LanguagePreference::English));
    QCOMPARE(state.snapshot().languagePreference(), LanguagePreference::English);
    QVERIFY(state.setLanguagePreference(LanguagePreference::Korean));
    QCOMPARE(state.snapshot().languagePreference(), LanguagePreference::Korean);
    QVERIFY(state.setLanguagePreference(LanguagePreference::SystemDefault));
    QCOMPARE(
        state.snapshot().languagePreference(),
        LanguagePreference::SystemDefault
        );
}

void NextApplicationUserPreferencesTests::fontSizesValidateAndParticipateInEquality()
{
    UserPreferencesState state;
    const auto original = state;
    for (const auto size : {FontSize::Small, FontSize::Normal, FontSize::Large, FontSize::ExtraLarge})
    {
        QVERIFY(state.setFontSize(size));
        QCOMPARE(state.snapshot().fontSize(), size);
    }
    QVERIFY(state != original);
    const auto before = state.snapshot();
    const auto invalid = state.setFontSize(static_cast<FontSize>(99));
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, ErrorCode::InvalidInput);
    QCOMPARE(state.snapshot(), before);
}

void NextApplicationUserPreferencesTests::flagsAcceptBothValues()
{
    UserPreferencesState state;

    QVERIFY(state.setSidebarTooltipsEnabled(false));
    QVERIFY(!state.snapshot().sidebarTooltipsEnabled());
    QVERIFY(state.setSidebarTooltipsEnabled(true));
    QVERIFY(state.snapshot().sidebarTooltipsEnabled());

    QVERIFY(state.setSidebarMarqueeEnabled(false));
    QVERIFY(!state.snapshot().sidebarMarqueeEnabled());
    QVERIFY(state.setSidebarMarqueeEnabled(true));
    QVERIFY(state.snapshot().sidebarMarqueeEnabled());

    QVERIFY(state.setShowAllTeachers(false));
    QVERIFY(!state.snapshot().showAllTeachers());
    QVERIFY(state.setShowAllKoreanTeachers(true));
    QVERIFY(state.snapshot().showAllTeachers());

    QVERIFY(state.setShowPowerPointDataAccessNotice(false));
    QVERIFY(!state.snapshot().showPowerPointDataAccessNotice());
    QVERIFY(state.setShowPowerPointNotice(true));
    QVERIFY(state.snapshot().showPowerPointDataAccessNotice());

    QVERIFY(state.setAutomaticUpdateChecksEnabled(false));
    QVERIFY(!state.snapshot().automaticUpdateChecksEnabled());
    QVERIFY(state.setAutomaticUpdateChecksEnabled(true));
    QVERIFY(state.snapshot().automaticUpdateChecksEnabled());
}

void NextApplicationUserPreferencesTests::
timeoutChoiceConversionsAreClosedAndExplicit()
{
    const std::array<std::pair<int, ExcelImportTimeoutChoice>, 4> choices = {{
        {30, ExcelImportTimeoutChoice::Seconds30},
        {60, ExcelImportTimeoutChoice::Seconds60},
        {120, ExcelImportTimeoutChoice::Seconds120},
        {300, ExcelImportTimeoutChoice::Seconds300}
    }};

    for (const auto& [seconds, choice] : choices)
    {
        QVERIFY(isValidExcelImportTimeoutChoice(choice));
        QCOMPARE(excelImportTimeoutChoiceToSeconds(choice), seconds);
        QCOMPARE(excelImportTimeoutSecondsToChoice(seconds), choice);
        const auto strictChoice = tryExcelImportTimeoutSecondsToChoice(seconds);
        QVERIFY(strictChoice.has_value());
        QCOMPARE(*strictChoice, choice);
    }

    const auto invalidChoice = static_cast<ExcelImportTimeoutChoice>(99);
    QVERIFY(!isValidExcelImportTimeoutChoice(invalidChoice));
    QCOMPARE(
        excelImportTimeoutChoiceToSeconds(invalidChoice),
        kDefaultExcelImportTimeoutSeconds
        );
    QCOMPARE(
        excelImportTimeoutSecondsToChoice(90),
        kDefaultExcelImportTimeoutChoice
        );
    QVERIFY(!tryExcelImportTimeoutSecondsToChoice(90).has_value());

    for (const int unsupportedSeconds : {29, 31, 59, 61, 90, 119, 121, 299, 301})
    {
        QVERIFY(!tryExcelImportTimeoutSecondsToChoice(
            unsupportedSeconds
            ).has_value());
    }
}

void NextApplicationUserPreferencesTests::validTimeoutsUpdateTheSnapshot()
{
    UserPreferencesState state;

    QCOMPARE(kMinimumExcelImportTimeoutSeconds, 30);
    QCOMPARE(kMaximumExcelImportTimeoutSeconds, 300);
    QCOMPARE(kMinExcelImportTimeoutSeconds, kMinimumExcelImportTimeoutSeconds);
    QCOMPARE(kMaxExcelImportTimeoutSeconds, kMaximumExcelImportTimeoutSeconds);

    const std::array<std::pair<int, ExcelImportTimeoutChoice>, 4> choices = {{
        {30, ExcelImportTimeoutChoice::Seconds30},
        {60, ExcelImportTimeoutChoice::Seconds60},
        {120, ExcelImportTimeoutChoice::Seconds120},
        {300, ExcelImportTimeoutChoice::Seconds300}
    }};

    for (const auto& [seconds, choice] : choices)
    {
        QVERIFY(state.setExcelImportTimeoutSeconds(seconds));
        QCOMPARE(state.snapshot().excelImportTimeoutChoice(), choice);
        QCOMPARE(state.snapshot().excelImportTimeoutSeconds(), seconds);
        QCOMPARE(state.snapshot().importTimeoutSeconds(), seconds);
    }

    QVERIFY(state.setImportTimeoutSeconds(30));
    QCOMPARE(
        state.snapshot().excelImportTimeoutChoice(),
        ExcelImportTimeoutChoice::Seconds30
        );
    QCOMPARE(state.snapshot().importTimeoutSeconds(), 30);
}

void NextApplicationUserPreferencesTests::invalidTimeoutPreservesSnapshotAndReturnsDeterministicError()
{
    UserPreferencesState state;
    QVERIFY(state.setExcelImportTimeoutSeconds(60));
    const UserPreferencesSnapshot before = state.snapshot();

    const auto first = state.setExcelImportTimeoutSeconds(29);
    QVERIFY(!first);
    QCOMPARE(first.error().code, ErrorCode::InvalidInput);
    QVERIFY(!first.error().recoverable);
    QVERIFY(
        first.error().message
        == "Excel import timeout must be 30, 60, 120, or 300 seconds."
        );
    QVERIFY(state.snapshot() == before);

    const auto second = state.setImportTimeoutSeconds(301);
    QVERIFY(!second);
    QCOMPARE(second.error().code, ErrorCode::InvalidInput);
    QVERIFY(!second.error().recoverable);
    QVERIFY(second.error().message == first.error().message);
    QVERIFY(state.snapshot() == before);

    const auto unsupported = state.setExcelImportTimeoutSeconds(90);
    QVERIFY(!unsupported);
    QCOMPARE(unsupported.error().code, ErrorCode::InvalidInput);
    QVERIFY(!unsupported.error().recoverable);
    QVERIFY(unsupported.error().message == first.error().message);
    QVERIFY(state.snapshot() == before);
}

void NextApplicationUserPreferencesTests::invalidEnumsPreserveSnapshotAndReturnDeterministicErrors()
{
    UserPreferencesState state;
    QVERIFY(state.setThemePreference(ThemePreference::Dark));
    QVERIFY(state.setLanguagePreference(LanguagePreference::Korean));
    const UserPreferencesSnapshot before = state.snapshot();

    const auto invalidTheme = state.setThemePreference(
        static_cast<ThemePreference>(99)
        );
    QVERIFY(!invalidTheme);
    QCOMPARE(invalidTheme.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        invalidTheme.error().message
        == "Theme preference must be SystemDefault, Light, or Dark."
        );
    QVERIFY(state.snapshot() == before);

    const auto invalidLanguage = state.setLanguagePreference(
        static_cast<LanguagePreference>(99)
        );
    QVERIFY(!invalidLanguage);
    QCOMPARE(invalidLanguage.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        invalidLanguage.error().message
        == "Language preference must be SystemDefault, English, or Korean."
        );
    QVERIFY(state.snapshot() == before);
}

void NextApplicationUserPreferencesTests::campusSelectionIsTypedValidatedAndClearable()
{
    UserPreferencesState state;
    const auto campusId = Domain::CampusId::fromString("campus-seoul");
    QVERIFY(campusId.has_value());

    QVERIFY(state.setLastSelectedCampusId(*campusId));
    const UserPreferencesSnapshot selected = state.snapshot();
    const auto selectedCampus = selected.lastSelectedCampusId();
    QVERIFY(selectedCampus.has_value());
    QVERIFY(*selectedCampus == *campusId);
    QVERIFY(selected.lastSelectedCampus().has_value());

    QVERIFY(state.clearLastSelectedCampusId());
    QVERIFY(!state.snapshot().lastSelectedCampusId().has_value());

    QVERIFY(state.setLastSelectedCampus(*campusId));
    QVERIFY(state.snapshot().lastSelectedCampusId().has_value());
    QVERIFY(state.clearLastSelectedCampus());
    QVERIFY(!state.snapshot().lastSelectedCampusId().has_value());

    QVERIFY(state.setLastSelectedCampusId(std::nullopt));
    QVERIFY(!state.snapshot().lastSelectedCampusId().has_value());
}

void NextApplicationUserPreferencesTests::invalidCampusPreservesSnapshotAndReturnsDeterministicError()
{
    UserPreferencesState state;
    const auto campusId = Domain::CampusId::fromString("campus-seoul");
    QVERIFY(campusId.has_value());
    QVERIFY(state.setLastSelectedCampusId(*campusId));

    const std::string maximumLengthValue(
        kMaximumLastSelectedCampusIdLength,
        'x'
        );
    const auto maximumLengthId = Domain::CampusId::fromString(
        maximumLengthValue
        );
    QVERIFY(maximumLengthId.has_value());
    QVERIFY(state.setLastSelectedCampusId(*maximumLengthId));
    const auto maximumLengthSelection =
        state.snapshot().lastSelectedCampusId();
    QVERIFY(maximumLengthSelection.has_value());
    QVERIFY(maximumLengthSelection->value() == maximumLengthValue);

    QVERIFY(state.setLastSelectedCampusId(*campusId));
    const UserPreferencesSnapshot before = state.snapshot();

    const auto blankId = Domain::CampusId::fromString(" \t\r\n");
    QVERIFY(blankId.has_value());
    const auto blankResult = state.setLastSelectedCampusId(*blankId);
    QVERIFY(!blankResult);
    QCOMPARE(blankResult.error().code, ErrorCode::InvalidInput);
    QVERIFY(
        blankResult.error().message
        == "Last selected campus identifier must be non-blank and bounded."
        );
    QVERIFY(state.snapshot() == before);

    const std::string nonBreakingSpaceValue("\xC2\xA0", 2);
    const auto nonBreakingSpaceId = Domain::CampusId::fromString(
        nonBreakingSpaceValue
        );
    QVERIFY(nonBreakingSpaceId.has_value());
    const auto nonBreakingSpaceResult =
        state.setLastSelectedCampusId(*nonBreakingSpaceId);
    QVERIFY(!nonBreakingSpaceResult);
    QCOMPARE(
        nonBreakingSpaceResult.error().code,
        ErrorCode::InvalidInput
        );
    QVERIFY(state.snapshot() == before);

    const std::string ideographicSpaceValue("\xE3\x80\x80", 3);
    const auto ideographicSpaceId = Domain::CampusId::fromString(
        ideographicSpaceValue
        );
    QVERIFY(ideographicSpaceId.has_value());
    const auto ideographicSpaceResult =
        state.setLastSelectedCampusId(*ideographicSpaceId);
    QVERIFY(!ideographicSpaceResult);
    QCOMPARE(
        ideographicSpaceResult.error().code,
        ErrorCode::InvalidInput
        );
    QVERIFY(state.snapshot() == before);

    const auto oversizedId = Domain::CampusId::fromString(
        std::string(kMaximumLastSelectedCampusIdLength + 1, 'x')
        );
    QVERIFY(oversizedId.has_value());
    const auto oversizedResult = state.setLastSelectedCampusId(*oversizedId);
    QVERIFY(!oversizedResult);
    QCOMPARE(oversizedResult.error().code, ErrorCode::InvalidInput);
    QVERIFY(oversizedResult.error().message == blankResult.error().message);
    QVERIFY(state.snapshot() == before);
}

void NextApplicationUserPreferencesTests::snapshotsAndStatesAreCopyableValueTypes()
{
    static_assert(std::is_copy_constructible_v<UserPreferencesSnapshot>);
    static_assert(std::is_copy_assignable_v<UserPreferencesSnapshot>);
    static_assert(std::is_copy_constructible_v<UserPreferencesState>);
    static_assert(std::is_copy_assignable_v<UserPreferencesState>);
    static_assert(!std::is_polymorphic_v<UserPreferencesSnapshot>);
    static_assert(!std::is_polymorphic_v<UserPreferencesState>);

    UserPreferencesState state;
    const auto campusId = Domain::CampusId::fromString("campus-seoul");
    QVERIFY(campusId.has_value());
    QVERIFY(state.setLastSelectedCampusId(*campusId));
    QVERIFY(state.setSidebarMarqueeEnabled(false));

    const UserPreferencesSnapshot originalSnapshot = state.snapshot();
    const UserPreferencesSnapshot copiedSnapshot = originalSnapshot;
    QVERIFY(copiedSnapshot == originalSnapshot);

    const auto copiedCampus = copiedSnapshot.lastSelectedCampusId();
    QVERIFY(copiedCampus.has_value());
    QVERIFY(*copiedCampus == *campusId);

    UserPreferencesState copiedState = state;
    QVERIFY(copiedState == state);
    QVERIFY(copiedState.setSidebarMarqueeEnabled(true));
    QVERIFY(copiedState != state);
    QVERIFY(!state.snapshot().sidebarMarqueeEnabled());
    QVERIFY(!state.snapshot().lastSelectedCampusId().value().value().empty());

    QVERIFY(state.clearLastSelectedCampus());
    QVERIFY(copiedCampus.has_value());
    QVERIFY(copiedSnapshot.lastSelectedCampusId().has_value());
}

QTEST_APPLESS_MAIN(NextApplicationUserPreferencesTests)

#include "next_application_user_preferences_tests.moc"
