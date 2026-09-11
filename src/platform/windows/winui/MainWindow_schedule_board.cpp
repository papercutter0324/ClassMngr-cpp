#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

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
        return;
    }

    const auto visibleDays =
        classmngr::engine::ScheduleReportService::visibleDays(false);
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
    request.rowFilter = useIntensive
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
        RenderOptions{true, false, false}
        );
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
    updateScheduleDisplayButtons();
    refreshScheduleBoard();
}

void MainWindow::updateScheduleDisplayButtons()
{
    using namespace Microsoft::UI::Xaml;

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
        if (m_testingStatusText)
        {
            m_testingStatusText.Text(
                L"Choose a testing class or plain testing block for the selected slot."
                );
        }
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
