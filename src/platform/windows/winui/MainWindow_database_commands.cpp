#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

winrt::fire_and_forget MainWindow::openDatabasePicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportDatabaseOpenError(
                {},
                "The database file picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.FileTypeFilter().Append(L".tps");
            picker.FileTypeFilter().Append(L".db");
            const auto file = co_await picker.PickSingleFileAsync();
            if (file)
            {
                static_cast<void>(openDatabasePath(asWString(file.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportDatabaseOpenError({}, winrt::to_string(error.message()));
    }
    catch (...)
    {
        reportDatabaseOpenError(
            {},
            "The database file picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openNewDatabasePicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileSavePicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportDatabaseOpenError(
                {},
                "The database save picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.CommitButtonText(L"Create");
            picker.SuggestedFileName(L"Teacher Profile.tps");
            picker.DefaultFileExtension(L".tps");
            picker.FileTypeChoices().Insert(
                L"Teacher Profile database",
                winrt::single_threaded_vector<winrt::hstring>({L".tps"})
                );
            const auto file = co_await picker.PickSaveFileAsync();
            if (file)
            {
                static_cast<void>(createDatabasePath(asWString(file.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportDatabaseOpenError({}, winrt::to_string(error.message()));
    }
    catch (...)
    {
        reportDatabaseOpenError(
            {},
            "The database save picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openSaveDatabasePicker(bool exportOnly)
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileSavePicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportOutputError(
                exportOnly ? L"Export database" : L"Save database",
                {},
                "The database save picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.CommitButtonText(
                exportOnly
                    ? winrt::hstring(L"Export")
                    : winrt::hstring(L"Save")
                );

            std::wstring suggestedName = L"Teacher Profile.tps";
            if (!m_currentDatabasePath.empty())
            {
                const std::filesystem::path currentPath(m_currentDatabasePath);
                if (!currentPath.filename().empty())
                {
                    suggestedName = currentPath.filename().wstring();
                }
            }
            picker.SuggestedFileName(winrt::hstring(suggestedName));
            picker.DefaultFileExtension(L".tps");
            picker.FileTypeChoices().Insert(
                L"Teacher Profile database",
                winrt::single_threaded_vector<winrt::hstring>({L".tps"})
                );
            const auto file = co_await picker.PickSaveFileAsync();
            if (file)
            {
                const std::wstring selectedPath = asWString(file.Path());
                if (exportOnly)
                {
                    static_cast<void>(exportDatabasePath(selectedPath));
                }
                else
                {
                    static_cast<void>(saveDatabasePath(selectedPath));
                }
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportOutputError(
            exportOnly ? L"Export database" : L"Save database",
            {},
            winrt::to_string(error.message())
            );
    }
    catch (...)
    {
        reportOutputError(
            exportOnly ? L"Export database" : L"Save database",
            {},
            "The database save picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openCurrentPageSavePicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileSavePicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportOutputError(
                L"Save current page",
                {},
                "The page save picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.CommitButtonText(L"Save");
            picker.SuggestedFileName(L"campus-information.json");
            picker.DefaultFileExtension(L".json");
            picker.FileTypeChoices().Insert(
                L"JSON document",
                winrt::single_threaded_vector<winrt::hstring>({L".json"})
                );
            const auto file = co_await picker.PickSaveFileAsync();
            if (file)
            {
                static_cast<void>(saveCurrentPagePath(asWString(file.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportOutputError(
            L"Save current page",
            {},
            winrt::to_string(error.message())
            );
    }
    catch (...)
    {
        reportOutputError(
            L"Save current page",
            {},
            "The page save picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openCampusResourcesFolderPicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FolderPicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportOutputError(
                L"Export campus resources",
                {},
                "The folder picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.SuggestedStartLocation(
                winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary
                );
            picker.FileTypeFilter().Append(L"*");
            const auto folder = co_await picker.PickSingleFolderAsync();
            if (folder)
            {
                static_cast<void>(exportCampusResourcesPath(asWString(folder.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportOutputError(
            L"Export campus resources",
            {},
            winrt::to_string(error.message())
            );
    }
    catch (...)
    {
        reportOutputError(
            L"Export campus resources",
            {},
            "The folder picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

bool MainWindow::openDatabasePath(std::wstring_view path)
{
    if (path.empty())
    {
        reportDatabaseOpenError(path, "A database path was not provided.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!isSupportedDatabasePath(candidate))
    {
        reportDatabaseOpenError(
            candidate,
            "Only .tps and .db database files are supported."
            );
        return false;
    }
    if (!pathExists(candidate))
    {
        reportDatabaseOpenError(candidate, "The database file does not exist.");
        return false;
    }

    try
    {
        classmngr::engine::OpenDatabaseOptions options;
        options.createParentDirectories = false;
        auto opened = classmngr::engine::OpenDatabase::execute(
            asUtf8(candidate),
            options
            );
        if (!opened)
        {
            reportDatabaseOpenError(candidate, opened.error().message);
            return false;
        }

        m_openDatabase = std::move(*opened);
        m_currentDatabasePath = candidate;
        m_dirtyState.markClean();
        addRecentDatabasePath(candidate);
        const std::wstring status = L"Database: " + candidate;
        if (m_shellDatabaseStatusText)
        {
            m_shellDatabaseStatusText.Text(winrt::hstring(status));
        }
        if (m_statusText)
        {
            m_statusText.Text(L"Database opened.");
        }
        refreshCampusInformationPage();
        refreshPersonalDetailsPage();
        refreshSubPrepPage();
        refreshClassesPage();
        refreshCalendarPage();
        updateFileCommandState();
        saveShellState();
        return true;
    }
    catch (std::exception const& error)
    {
        reportDatabaseOpenError(candidate, error.what());
    }
    catch (...)
    {
        reportDatabaseOpenError(candidate, "The database could not be opened.");
    }
    return false;
}

bool MainWindow::createDatabasePath(std::wstring_view path)
{
    if (path.empty())
    {
        reportDatabaseOpenError(path, "A new database path was not provided.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!classmngr::engine::DatabaseFileFormat::isNativePath(
            asUtf8(candidate)
            ))
    {
        reportDatabaseOpenError(
            candidate,
            "New databases must use the .tps file type."
            );
        return false;
    }

    // The Qt controller closes the active database before replacing or
    // creating the selected path. Release the SQLite owner before the native
    // picker-selected replacement is removed, even when the destination is a
    // different file.
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshCampusInformationPage();
    refreshPersonalDetailsPage();
    refreshSubPrepPage();
    refreshClassesPage();
    refreshCalendarPage();

    if (pathExists(candidate))
    {
        std::error_code removeError;
        const bool removed = std::filesystem::remove(
            std::filesystem::path(candidate),
            removeError
            );
        if (removeError || !removed)
        {
            reportDatabaseOpenError(
                candidate,
                removeError
                    ? removeError.message()
                    : "The selected database file could not be replaced."
                );
            return false;
        }
    }

    try
    {
        classmngr::engine::OpenDatabaseOptions options;
        options.createParentDirectories = true;
        auto opened = classmngr::engine::OpenDatabase::execute(
            asUtf8(candidate),
            options
            );
        if (!opened)
        {
            reportDatabaseOpenError(candidate, opened.error().message);
            return false;
        }

        m_openDatabase = std::move(*opened);
        m_currentDatabasePath = candidate;
        m_dirtyState.markClean();
        addRecentDatabasePath(candidate);
        const std::wstring status = L"Database: " + candidate;
        if (m_shellDatabaseStatusText)
        {
            m_shellDatabaseStatusText.Text(winrt::hstring(status));
        }
        if (m_statusText)
        {
            m_statusText.Text(L"New database created.");
        }
        refreshCampusInformationPage();
        refreshPersonalDetailsPage();
        refreshSubPrepPage();
        refreshClassesPage();
        refreshCalendarPage();
        updateFileCommandState();
        saveShellState();
        return true;
    }
    catch (std::exception const& error)
    {
        reportDatabaseOpenError(candidate, error.what());
    }
    catch (...)
    {
        reportDatabaseOpenError(candidate, "The new database could not be created.");
    }
    return false;
}

bool MainWindow::saveDatabasePath(std::wstring_view path)
{
    if (!m_openDatabase || m_currentDatabasePath.empty())
    {
        reportOutputError(L"Save database", path, "No file-backed database is open.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!classmngr::engine::DatabaseFileFormat::isNativePath(asUtf8(candidate)))
    {
        reportOutputError(
            L"Save database",
            candidate,
            "Saved databases must use the .tps file type."
            );
        return false;
    }

    if (samePath(candidate, m_currentDatabasePath))
    {
        m_dirtyState.markClean();
        if (m_statusText)
        {
            m_statusText.Text(L"Changes saved.");
        }
        updateFileCommandState();
        return true;
    }

    classmngr::windows::winui::WindowsFileSystem fileSystem;
    const auto copied = fileSystem.copyFile(
        asUtf8(m_currentDatabasePath),
        asUtf8(candidate),
        true
        );
    if (!copied)
    {
        reportOutputError(L"Save database", candidate, copied.error().message);
        return false;
    }

    if (!openDatabasePath(candidate))
    {
        reportOutputError(
            L"Save database",
            candidate,
            "The saved database could not be reopened."
            );
        return false;
    }

    m_dirtyState.markClean();
    if (m_statusText)
    {
        m_statusText.Text(L"Database saved as a new file.");
    }
    updateFileCommandState();
    return true;
}

bool MainWindow::exportDatabasePath(std::wstring_view path)
{
    if (!m_openDatabase || m_currentDatabasePath.empty())
    {
        reportOutputError(L"Export database", path, "No file-backed database is open.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!classmngr::engine::DatabaseFileFormat::isNativePath(asUtf8(candidate)))
    {
        reportOutputError(
            L"Export database",
            candidate,
            "Exported databases must use the .tps file type."
            );
        return false;
    }

    if (!samePath(candidate, m_currentDatabasePath))
    {
        classmngr::windows::winui::WindowsFileSystem fileSystem;
        const auto copied = fileSystem.copyFile(
            asUtf8(m_currentDatabasePath),
            asUtf8(candidate),
            true
            );
        if (!copied)
        {
            reportOutputError(L"Export database", candidate, copied.error().message);
            return false;
        }
    }

    if (m_statusText)
    {
        m_statusText.Text(L"Database exported.");
    }
    return true;
}

bool MainWindow::saveCurrentPagePath(std::wstring_view path)
{
    if (!isCampusPageId(m_currentPageId))
    {
        reportOutputError(
            L"Save current page",
            path,
            "Only Campus Directory pages are available for export."
            );
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    const std::wstring extension = std::filesystem::path(candidate).extension().wstring();
    if (extension.size() != 5
        || extension[0] != L'.'
        || std::towlower(extension[1]) != L'j'
        || std::towlower(extension[2]) != L's'
        || std::towlower(extension[3]) != L'o'
        || std::towlower(extension[4]) != L'n')
    {
        reportOutputError(
            L"Save current page",
            candidate,
            "The current page must be saved as a .json file."
            );
        return false;
    }

    classmngr::windows::winui::WindowsFileSystem fileSystem;
    const std::string json = currentPageExportJson();
    const auto written = fileSystem.writeBytes(asUtf8(candidate), json, true);
    if (!written)
    {
        reportOutputError(L"Save current page", candidate, written.error().message);
        return false;
    }

    if (m_statusText)
    {
        m_statusText.Text(L"Campus information page saved.");
    }
    return true;
}

std::string MainWindow::currentPageExportJson() const
{
    std::string output;
    output.reserve(4096);
    output += "{\n  \"format\": ";
    appendJsonEscaped(output, "classmngr.phase5.campus-information.v1");
    output += ",\n  \"page\": ";
    appendJsonEscaped(output, asUtf8(m_currentPageId));
    output += ",\n  \"state\": ";
    appendJsonEscaped(output, asUtf8(m_campusInformationState));
    output += ",\n  \"campuses\": [";

    const auto appendField = [](std::string& value,
                                char const* key,
                                std::string_view fieldValue,
                                bool last) {
        value += "\n      \"";
        value += key;
        value += "\": ";
        appendJsonEscaped(value, fieldValue);
        value += last ? "\n" : ",";
    };

    for (std::size_t index = 0; index < m_campusRecords.size(); ++index)
    {
        const classmngr::engine::CampusRecord& campus = m_campusRecords[index];
        output += index == 0 ? "\n    {" : ",\n    {";
        output += "\n      \"id\": ";
        output += std::to_string(campus.id);
        appendField(output, "name", campus.name, false);
        appendField(output, "buildingName", campus.buildingName, false);
        appendField(output, "address", campus.address, false);
        appendField(output, "phoneNumber", campus.phoneNumber, false);
        appendField(output, "officeNumber", campus.officeNumber, false);
        appendField(output, "transitSteps", campus.transitSteps, false);
        appendField(output, "arrivalInfo", campus.arrivalInfo, false);
        appendField(output, "imagePath", campus.imagePath, false);
        appendField(output, "officeWifi", campus.officeWifi, false);
        appendField(output, "officeWifiPassword", campus.officeWifiPassword, false);
        appendField(output, "printerName", campus.printerName, false);
        appendField(output, "printerSteps", campus.printerSteps, false);
        appendField(output, "photocopierCode", campus.photocopierCode, false);
        appendField(output, "housingLocations", campus.housingLocations, true);
        output += "    }";
    }
    if (!m_campusRecords.empty())
    {
        output += "\n  ";
    }
    output += "]\n}\n";
    return output;
}

bool MainWindow::exportCampusResourcesPath(std::wstring_view path)
{
    if (!isCampusPageId(m_currentPageId))
    {
        reportOutputError(
            L"Export campus resources",
            path,
            "Only Campus Directory pages are available for resource export."
            );
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    std::error_code directoryError;
    if (!std::filesystem::is_directory(
            std::filesystem::path(candidate),
            directoryError
            ) || directoryError)
    {
        reportOutputError(
            L"Export campus resources",
            candidate,
            "The selected export location is not a folder."
            );
        return false;
    }

    classmngr::windows::winui::WindowsFileSystem fileSystem;
    const std::filesystem::path resourceDirectory =
        std::filesystem::path(candidate) / L"campus-resources";
    const auto created = fileSystem.createDirectories(
        asUtf8(resourceDirectory.wstring())
        );
    if (!created)
    {
        reportOutputError(
            L"Export campus resources",
            resourceDirectory.wstring(),
            created.error().message
            );
        return false;
    }

    const std::filesystem::path jsonPath =
        std::filesystem::path(candidate) / L"campus-information.json";
    const auto jsonWritten = fileSystem.writeBytes(
        asUtf8(jsonPath.wstring()),
        currentPageExportJson(),
        true
        );
    if (!jsonWritten)
    {
        reportOutputError(
            L"Export campus resources",
            jsonPath.wstring(),
            jsonWritten.error().message
            );
        return false;
    }

    classmngr::windows::winui::WindowsResourceProvider resourceProvider;
    std::set<std::string> usedNames;
    for (classmngr::engine::CampusRecord const& campus : m_campusRecords)
    {
        if (campus.imagePath.empty())
        {
            continue;
        }

        const std::string fileName = campusResourceFileName(campus.imagePath);
        if (fileName.empty())
        {
            reportOutputError(
                L"Export campus resources",
                candidate,
                "A campus image reference has an unsafe file name."
                );
            return false;
        }

        const auto bytes = resourceProvider.readBytes(campus.imagePath);
        if (!bytes)
        {
            reportOutputError(
                L"Export campus resources",
                candidate,
                bytes.error().message
                );
            return false;
        }

        const std::string uniqueName = uniqueCampusResourceFileName(
            fileName,
            usedNames
            );
        const std::filesystem::path outputPath =
            resourceDirectory / std::filesystem::path(
                winrt::to_hstring(uniqueName).c_str()
                );
        const std::string bytesAsString(
            reinterpret_cast<char const*>(bytes->data()),
            bytes->size()
            );
        const auto written = fileSystem.writeBytes(
            asUtf8(outputPath.wstring()),
            bytesAsString,
            true
            );
        if (!written)
        {
            reportOutputError(
                L"Export campus resources",
                outputPath.wstring(),
                written.error().message
                );
            return false;
        }
    }

    if (m_statusText)
    {
        m_statusText.Text(L"Campus information and resources exported.");
    }
    return true;
}

void MainWindow::openMostRecentDatabase()
{
    m_recentDatabasePaths = pruneRecentDatabasePaths(m_recentDatabasePaths);
    refreshRecentDatabaseMenu();
    saveShellState();
    if (!m_recentDatabasePaths.empty())
    {
        static_cast<void>(openDatabasePath(m_recentDatabasePaths.front()));
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
