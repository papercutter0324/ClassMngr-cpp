#pragma once

#include "next/application/file_dialog_port.h"

class QWidget;

namespace ClassMngr::Next::Ui
{

enum class FileDialogBackend
{
    PlatformDefault,
    Native,
    Qt
};

// Synchronous Qt implementation of the Qt-free file dialog port. The parent
// is borrowed by the presenter and is intentionally absent from requests.
class QtFileDialogPresenter final : public Application::FileDialogPort
{
public:
    explicit QtFileDialogPresenter(
        Application::FileDialogDirectoryPreferencesPort& directoryPreferences,
        QWidget* parent = nullptr,
        FileDialogBackend backend = FileDialogBackend::PlatformDefault
        ) noexcept;

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    openOneFile(
        const Application::OpenFileDialogRequest& request
        ) override;

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<
            std::vector<Application::FileDialogPath>>>
    openManyFiles(
        const Application::OpenFileDialogRequest& request
        ) override;

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<
            Application::SavedFileDialogSelection>>
    saveFile(
        const Application::SaveFileDialogRequest& request
        ) override;

    [[nodiscard]] Domain::Result<
        Application::FileDialogResponse<Application::FileDialogPath>>
    selectDirectory(
        const Application::SelectDirectoryDialogRequest& request
        ) override;

    [[nodiscard]] static bool platformUsesNativeDialogs() noexcept;
    [[nodiscard]] bool usesNativeDialogs() const noexcept;

private:
    Application::FileDialogDirectoryPreferencesPort& m_directoryPreferences;
    QWidget* m_parent = nullptr;
    FileDialogBackend m_backend = FileDialogBackend::PlatformDefault;
};

} // namespace ClassMngr::Next::Ui
