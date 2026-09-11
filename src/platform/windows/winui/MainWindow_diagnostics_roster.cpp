#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

bool MainWindow::runPhase6RosterChecks()
{
    m_phase6RosterFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6RosterFailureMask = failureMask;
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_classLoading = false;
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    m_classNew = false;
    m_classRosterLoading = false;
    m_classRosterDirty = false;

    navigateTo(classesPageId);
    refreshClassesPage();
    const bool noDatabaseReady =
        m_currentPageId == classesPageId
        && m_classRosterStatusText
        && m_classRosterStatusText.Text() == L"No database open."
        && m_classRosterImportScoresButton
        && !m_classRosterImportScoresButton.IsEnabled()
        && m_classRosterAddColumnButton
        && !m_classRosterAddColumnButton.IsEnabled()
        && m_classRosterRemoveColumnButton
        && !m_classRosterRemoveColumnButton.IsEnabled();
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto sourceId = repository.create("Roster Source");
    const auto targetId = repository.create("Roster Target");
    if (!sourceId || !targetId)
    {
        return fail(4);
    }

    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(8);
    }
    const auto levels = classmngr::engine::ClassInfoConfig::levelsForGrade(
        grades.front()
        );
    const auto readingBooks = classmngr::engine::ClassInfoConfig::readingBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    const auto essayBooks = classmngr::engine::ClassInfoConfig::essayBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    if (levels.empty() || readingBooks.empty() || essayBooks.empty())
    {
        return fail(16);
    }

    const auto saveClassInfo = [this, &grades, &levels, &readingBooks,
                                &essayBooks](int classId) {
        classmngr::engine::ClassInfo info;
        info.classId = classId;
        info.classGrade = grades.front();
        info.classLevel = levels.front();
        info.readingBook = readingBooks.front();
        info.essayBook = essayBooks.front();
        info.classColor = "#FFFFFF";
        info.fontColor = "#000000";
        classmngr::engine::ClassInfoService service(*m_openDatabase);
        return service.save(info);
    };
    if (!saveClassInfo(*sourceId) || !saveClassInfo(*targetId))
    {
        return fail(32);
    }

    classmngr::engine::Roster sourceRoster;
    for (const std::string_view column : classmngr::engine::RosterBaseColumns)
    {
        sourceRoster.columns.emplace_back(column);
    }
    sourceRoster.columnWidths = {140, 140, 100, 140, 100, 100};
    sourceRoster.rows.push_back({
        "Alice",
        winrt::to_string(winrt::hstring(L"\uC568\uB9AC\uC2A4")),
        "",
        "",
        "",
        ""
        });
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    if (!rosterService.save(*sourceId, sourceRoster))
    {
        return fail(64);
    }

    refreshClassesPage();
    const bool populatedReady =
        m_classSelectedId == *sourceId
        && m_classRosterHeaderGrid
        && m_classRosterHeaderGrid.Children().Size() == 6
        && m_classRosterList
        && m_classRosterList.Items().Size()
            == classmngr::engine::RosterValidator::MaximumRows
        && m_classRosterTransferTargets.size() == 1
        && m_classRosterTransferTargets.front().classId == *targetId
        && m_classRosterStatusText.Text().empty()
        && m_classRosterImportScoresButton.IsEnabled()
        && m_classRosterAddColumnButton.IsEnabled()
        && !m_classRosterRemoveColumnButton.IsEnabled();
    if (!populatedReady)
    {
        return fail(128);
    }

    m_classRosterList.SelectedIndex(0);
    if (m_classRosterCellBoxes.empty()
        || m_classRosterCellBoxes.front().size() < 2)
    {
        return fail(256);
    }
    m_classRosterCellBoxes.front().front().Text(L"Alice Updated");
    const auto savedRoster = rosterService.load(*sourceId);
    const bool savedReady = savedRoster
        && !m_classRosterDirty
        && classmngr::engine::rosterStudentCount(*savedRoster) == 1
        && savedRoster->rows.front().front() == "Alice Updated";
    if (!savedReady)
    {
        return fail(512);
    }

    m_classRosterCellBoxes.front().front().Text(L"Alice \u2603");
    const bool invalidReady =
        m_classRosterDirty
        && m_classRosterValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && m_classRosterStatusText.Text() == L"Roster could not be saved.";
    if (!invalidReady)
    {
        return fail(1024);
    }
    m_classRosterCellBoxes.front().front().Text(L"Alice Updated");
    if (m_classRosterDirty)
    {
        return fail(2048);
    }

    const bool fixedRowsReady =
        m_classRosterList.Items().Size()
            == classmngr::engine::RosterValidator::MaximumRows
        && m_classRosterCellBoxes.size()
            == classmngr::engine::RosterValidator::MaximumRows
        && m_classRosterCellBoxes.back().front().Text().empty()
        && !m_classRosterDirty;
    if (!fixedRowsReady)
    {
        return fail(4096);
    }

    m_classRosterList.SelectedIndex(0);
    const auto contextMenu = createClassRosterContextMenu(0);
    const auto transferMenu = contextMenu.Items().Size() > 1
        ? contextMenu.Items().GetAt(1).try_as<
            Microsoft::UI::Xaml::Controls::MenuFlyoutSubItem>()
        : nullptr;
    const bool contextMenuReady =
        contextMenu.Items().Size() == 2
        && contextMenu.Items().GetAt(0).try_as<
            Microsoft::UI::Xaml::Controls::MenuFlyoutItem>()
        && transferMenu
        && transferMenu.Items().Size() == 1;
    if (!contextMenuReady)
    {
        return fail(8192);
    }

    transferClassRosterRow(0, *targetId);
    const auto transferredSource = rosterService.load(*sourceId);
    const auto transferredTarget = rosterService.load(*targetId);
    const bool transferReady = transferredSource
        && transferredTarget
        && classmngr::engine::rosterStudentCount(*transferredSource) == 0
        && classmngr::engine::rosterStudentCount(*transferredTarget) == 1
        && transferredTarget->rows.front().front() == "Alice Updated"
        && !m_classRosterDirty;
    if (!transferReady)
    {
        return fail(32768);
    }

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshClassesPage();
    const bool clearReady =
        m_classRosterStatusText.Text() == L"No database open."
        && !m_classRosterImportScoresButton.IsEnabled()
        && !m_classRosterAddColumnButton.IsEnabled()
        && m_classRosterTransferTargets.empty();
    if (!clearReady)
    {
        return fail(65536);
    }
    return true;
}

uint32_t MainWindow::phase6RosterFailureMask() const noexcept
{
    return m_phase6RosterFailureMask;
}

} // namespace winrt::ClassMngrWinUI::implementation
