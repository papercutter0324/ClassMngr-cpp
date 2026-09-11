#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::ContinueButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);

    if (!m_nameTextBox || !m_homeViewModel || !m_homeCommand)
    {
        return;
    }

    if (m_nameTextBox.Text().empty())
    {
        classmngr::engine::ValidationResult validation;
        validation.add(classmngr::engine::ValidationIssue{
            "required",
            "name",
            classmngr::engine::ValidationSeverity::Error,
            0,
            0
            });
        presentValidationSummary(validation);
        m_statusText.Text(L"Enter a name.");
        return;
    }

    if (m_homeCommand->CanExecute(nullptr))
    {
        m_homeViewModel->ClearValidation();
        m_homeCommand->Execute(nullptr);
        updateHomePresentation();
    }
}

void MainWindow::ShellInfoButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showOwnedDialog();
}

void MainWindow::OpenDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openDatabasePicker();
}

void MainWindow::NewDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openNewDatabasePicker();
}

void MainWindow::SaveDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        reportOutputError(L"Save database", {}, "No database is open.");
        return;
    }

    // Engine-backed writes commit at the service boundary.  The shell-level
    // save command therefore closes the prototype dirty-state transaction
    // and records the successful save without issuing ad hoc SQL.
    m_dirtyState.markClean();
    if (m_statusText)
    {
        m_statusText.Text(L"Changes saved.");
    }
    updateFileCommandState();
}

void MainWindow::SaveDatabaseAsMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openSaveDatabasePicker(false);
}

void MainWindow::ExportDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openSaveDatabasePicker(true);
}

void MainWindow::CloseDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    if (m_shellDatabaseStatusText)
    {
        m_shellDatabaseStatusText.Text(L"No database open");
    }
    if (m_statusText)
    {
        m_statusText.Text(L"Database closed.");
    }
    refreshCampusInformationPage();
    refreshPersonalDetailsPage();
    refreshClassesPage();
    refreshCalendarPage();
    updateFileCommandState();
    saveShellState();
}

void MainWindow::SaveCurrentPageMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_currentPageId == classesPageId && m_classSectionIndex == 3)
    {
        openSpeakingBatchReportDialog(true);
        return;
    }
    openCurrentPageSavePicker();
}

void MainWindow::PrintCurrentPageMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_currentPageId == classesPageId && m_classSectionIndex == 3)
    {
        openSpeakingBatchReportDialog(false);
    }
}

void MainWindow::ExportCampusResourcesMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openCampusResourcesFolderPicker();
}

void MainWindow::RecentDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    const auto item = sender.try_as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    if (item)
    {
        const std::wstring path = boxedString(item.Tag());
        if (!path.empty())
        {
            static_cast<void>(openDatabasePath(path));
        }
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
