#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace
{
bool scheduleSettingBool(
    classmngr::engine::ApplicationSettingsService& settings,
    std::string_view key,
    bool fallback
    )
{
    const auto loaded = settings.load(key);
    return loaded ? winrt::ClassMngrWinUI::implementation::MainWindowDetail::settingBool(
        *loaded,
        fallback
        ) : fallback;
}

std::string scheduleHoverBorderColor(
    classmngr::engine::ApplicationSettingsService& settings
    )
{
    constexpr std::string_view defaultColor = "#D39B25";
    const auto loaded = settings.load("schedule/hoverBorderColor");
    if (!loaded)
    {
        return std::string(defaultColor);
    }
    if (const auto* value = std::get_if<std::string>(&*loaded);
        value && !value->empty())
    {
        return *value;
    }
    return std::string(defaultColor);
}
} // namespace

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshScheduleBoard()
{
    using namespace Microsoft::UI::Xaml;
    using namespace ClassMngrWinUIScheduleBoard;

    if (!m_scheduleBoardRoot)
    {
        return;
    }

    if (!m_openDatabase)
    {
        m_scheduleBoardRoot.Visibility(Visibility::Collapsed);
        updateScheduleDisplayButtons();
        return;
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    const bool showWeekends = scheduleSettingBool(
        settings,
        "schedule_show_weekends",
        false
        );
    const bool showAllIntensiveHours = scheduleSettingBool(
        settings,
        "schedule_show_all_hours_v2",
        false
        );
    const bool use24h = scheduleSettingBool(
        settings,
        "schedule_use_24h",
        false
        );
    const bool showEnglishNames = scheduleSettingBool(
        settings,
        "schedule_show_korean_teacher_english_names",
        false
        );
    const bool testingAffectsM1 = scheduleSettingBool(
        settings,
        "schedule_testing_affects_m1",
        false
        );
    const std::string hoverBorderColor = scheduleHoverBorderColor(settings);
    const auto visibleDays =
        classmngr::engine::ScheduleReportService::visibleDays(showWeekends);
    const bool useIntensive =
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Intensive;
    const auto build = classmngr::engine::ScheduleBuilderService::build(
        m_scheduleInfos,
        useIntensive,
        visibleDays
        );

    classmngr::engine::ScheduleReportRequest request;
    request.days = visibleDays;
    request.displayMode = m_scheduleDisplayMode;
    request.use24h = use24h;
    request.testingAffectsM1 = testingAffectsM1;
    request.rowFilter = useIntensive && !showAllIntensiveHours
        ? classmngr::engine::ScheduleReportRowFilter::TrimEmptyOuterRows
        : classmngr::engine::ScheduleReportRowFilter::None;

    classmngr::engine::IntensiveSlotStateService slotService(*m_openDatabase);
    if (const auto states = slotService.list())
    {
        for (const auto& state : *states)
        {
            request.slotStateOverrides.emplace(
                classmngr::engine::ScheduleReportService::slotKey(
                    state.day,
                    state.startTime
                    ),
                state.state
                );
        }
    }

    if (
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Testing
        )
    {
        classmngr::engine::TestingBlockService blockService(*m_openDatabase);
        classmngr::engine::TestingClassService testingClassService(
            *m_openDatabase
            );
        const auto assignments = blockService.listAssignments();
        const auto testingClasses = testingClassService.list();
        if (assignments && testingClasses)
        {
            for (const auto& assignment : *assignments)
            {
                classmngr::engine::ScheduleReportTestingAssignmentView view;
                view.assignment.day = assignment.day;
                view.assignment.startTime = assignment.startTime;
                view.assignment.room = assignment.room;
                view.assignment.classId = assignment.classId;
                view.assignment.kind = assignment.kind
                    == classmngr::engine::TestingAssignmentKind::SpecialClass
                    ? classmngr::engine::ScheduleReportTestingAssignmentKind::SpecialClass
                    : classmngr::engine::ScheduleReportTestingAssignmentKind::PlainTesting;

                if (
                    assignment.kind
                        == classmngr::engine::TestingAssignmentKind::SpecialClass
                    )
                {
                    const auto testingClass = std::find_if(
                        testingClasses->cbegin(),
                        testingClasses->cend(),
                        [&assignment](const auto& candidate) {
                            return candidate.classId == assignment.classId;
                        }
                        );
                    if (testingClass != testingClasses->cend())
                    {
                        view.testingClassEntry.classId = testingClass->classId;
                        view.testingClassEntry.kind =
                            classmngr::engine::ScheduleReportEntryKind::TestingClass;
                        view.testingClassEntry.className = testingClass->name;
                        view.testingClassEntry.roomNumber = testingClass->room;
                        view.testingClassEntry.classGrade = testingClass->grade;
                        view.testingClassEntry.classLevel = testingClass->level;
                        view.testingClassEntry.classColor = testingClass->classColor;
                        view.testingClassEntry.fontColor = testingClass->fontColor;
                    }
                }

                request.testingAssignments.emplace(
                    classmngr::engine::ScheduleReportService::slotKey(
                        assignment.day,
                        assignment.startTime
                        ),
                    std::move(view)
                    );
            }
        }
    }

    const auto model = classmngr::engine::ScheduleReportService::build(
        build,
        request
        );
    m_scheduleBoardRoot.Visibility(Visibility::Visible);
    render(
        m_scheduleBoardRoot,
        model,
        RenderOptions{
            true,
            showEnglishNames,
            false,
            m_scheduleDisplayMode,
            hoverBorderColor
        }
        );
    updateScheduleDisplayButtons();
}

void MainWindow::setScheduleDisplayMode(int mode)
{
    if (
        mode < static_cast<int>(
            classmngr::engine::ScheduleReportDisplayMode::Regular
            )
        || mode > static_cast<int>(
            classmngr::engine::ScheduleReportDisplayMode::Testing
            )
        )
    {
        return;
    }

    m_scheduleDisplayMode =
        static_cast<classmngr::engine::ScheduleReportDisplayMode>(mode);
    if (m_openDatabase)
    {
        classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
        const auto settingValue = mode == static_cast<int>(
            classmngr::engine::ScheduleReportDisplayMode::Intensive
            )
            ? std::string("intensive")
            : mode == static_cast<int>(
                classmngr::engine::ScheduleReportDisplayMode::Testing
                )
                ? std::string("testing")
                : std::string("regular");
        static_cast<void>(settings.save(
            "schedule_display_mode",
            classmngr::engine::SettingValue{settingValue}
            ));
    }
    updateScheduleDisplayButtons();
    refreshScheduleBoard();
}

void MainWindow::updateScheduleDisplayButtons()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    const auto setButtonState = [](auto const& button, bool selected) {
        if (!button)
        {
            return;
        }

        button.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{
                    255,
                    static_cast<std::uint8_t>(selected ? 59 : 48),
                    static_cast<std::uint8_t>(selected ? 169 : 53),
                    static_cast<std::uint8_t>(selected ? 225 : 60)
                }
                )
            );
        button.Foreground(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{
                    255,
                    255,
                    255,
                    255
                }
                )
            );
        button.BorderBrush(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 92, 99, 108}
                )
            );
        button.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        button.CornerRadius(CornerRadius{5.0, 5.0, 5.0, 5.0});
    };

    setButtonState(
        m_scheduleRegularModeButton,
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Regular
        );
    setButtonState(
        m_scheduleIntensiveModeButton,
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Intensive
        );
    setButtonState(
        m_scheduleTestingModeButton,
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Testing
    );
    setButtonState(m_scheduleImportModeButton, false);

    const bool testing =
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Testing;
    if (m_scheduleTestingClassesButton)
    {
        m_scheduleTestingClassesButton.Visibility(
            testing ? Visibility::Visible : Visibility::Collapsed
            );
        m_scheduleTestingClassesButton.IsEnabled(static_cast<bool>(m_openDatabase));
    }

    if (m_scheduleTestingBanner)
    {
        m_scheduleTestingBanner.Visibility(
            testing ? Visibility::Visible : Visibility::Collapsed
            );
        if (testing)
        {
            bool testingAffectsM1 = false;
            if (m_openDatabase)
            {
                classmngr::engine::ApplicationSettingsService settings(
                    *m_openDatabase
                    );
                testingAffectsM1 = scheduleSettingBool(
                    settings,
                    "schedule_testing_affects_m1",
                    false
                    );
            }
            if (const auto text = m_scheduleTestingBanner.Child().try_as<TextBlock>())
            {
                text.Text(
                    testingAffectsM1
                        ? L"Testing View \u2014 M1, M2, and M3 classes are hidden"
                        : L"Testing View \u2014 M2 and M3 classes are hidden; M1 classes remain"
                    );
            }
        }
    }
}

void MainWindow::handleScheduleSlotClick(
    std::wstring day,
    std::wstring timeLabel,
    std::wstring currentState,
    std::wstring defaultState,
    bool slotTogglingEnabled,
    bool testingBlockCreationEnabled
    )
{
    if (!m_openDatabase)
    {
        return;
    }

    if (
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Intensive
        && slotTogglingEnabled
        )
    {
        classmngr::engine::IntensiveSlotStateService service(*m_openDatabase);
        const auto saved = service.save(
            asUtf8(day),
            asUtf8(timeLabel),
            classmngr::engine::ScheduleReportService::nextSlotState(
                asUtf8(currentState)
                ),
            asUtf8(defaultState)
            );
        if (!saved)
        {
            if (m_scheduleWorkspaceStatusText)
            {
                m_scheduleWorkspaceStatusText.Text(winrt::hstring(
                    L"Schedule slot could not be updated: "
                        + asWide(saved.error().message)
                    ));
            }
            return;
        }

        m_dirtyState.markDirty();
        updateFileCommandState();
        refreshScheduleWorkspace();
        return;
    }

    if (
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Testing
        && (currentState == L"testing" || testingBlockCreationEnabled)
        )
    {
        if (m_scheduleTabs)
        {
            m_scheduleTabs.SelectedIndex(2);
        }
        if (m_testingDayCombo)
        {
            for (int index = 0;
                 index < static_cast<int>(m_testingDayCombo.Items().Size());
                 ++index)
            {
                const auto item = m_testingDayCombo.Items().GetAt(index)
                    .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
                if (item && boxedString(item.Tag()) == day)
                {
                    m_testingDayCombo.SelectedIndex(index);
                    break;
                }
            }
        }
        if (m_testingStartTextBox)
        {
            m_testingStartTextBox.Text(timeLabel);
        }
        if (m_testingReplaceExistingCheck)
        {
            m_testingReplaceExistingCheck.IsChecked(
                currentState == L"testing"
                );
        }
        if (m_testingClassSelector)
        {
            for (const auto& assignment : m_testingAssignments)
            {
                if (asWide(assignment.day) != day
                    || asWide(assignment.startTime) != timeLabel
                    || assignment.classId <= 0)
                {
                    continue;
                }
                for (int index = 0;
                     index < static_cast<int>(
                         m_testingClassSelector.Items().Size()
                         );
                     ++index)
                {
                    const auto item = m_testingClassSelector.Items().GetAt(index)
                        .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
                    if (item && boxedInt(item.Tag()) == assignment.classId)
                    {
                        m_testingClassSelector.SelectedIndex(index);
                        break;
                    }
                }
                break;
            }
        }
        if (m_testingStatusText)
        {
            m_testingStatusText.Text(
                currentState == L"testing"
                    ? L"This slot already has a testing assignment. Enable replacement only after review."
                    : L"Choose a testing class or plain testing block for the selected slot."
                );
        }
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
