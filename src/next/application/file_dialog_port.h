#pragma once

#include "next/application/file_dialog_directory_preferences.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// A filesystem path carried through the application boundary as UTF-8. The
// value is preserved as supplied; path normalization belongs to the presenter.
class FileDialogPath final
{
public:
    FileDialogPath() = default;

    explicit FileDialogPath(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_value.empty();
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(const FileDialogPath&, const FileDialogPath&) = default;

private:
    std::string m_value;
};

struct OpenFileDialogRequest final
{
    // Text values and filters use UTF-8, like FileDialogPath.
    FileDialogPurpose purpose = FileDialogPurpose::General;
    std::string title;
    FileDialogPath initialDirectory;
    std::vector<std::string> nameFilters;
};

struct SaveFileDialogRequest final
{
    FileDialogPurpose purpose = FileDialogPurpose::General;
    std::string title;
    FileDialogPath initialDirectory;
    std::string suggestedFileName;
    std::vector<std::string> nameFilters;
    std::string defaultSuffix;
    bool confirmOverwrite = true;
    // A non-empty label requests an optional, initially unchecked accessory.
    std::string openAfterSavingText;
};

struct SelectDirectoryDialogRequest final
{
    FileDialogPurpose purpose = FileDialogPurpose::General;
    std::string title;
    FileDialogPath initialDirectory;
};

struct SavedFileDialogSelection final
{
    FileDialogPath path;
    bool openAfterSaving = false;

    friend bool operator==(
        const SavedFileDialogSelection&,
        const SavedFileDialogSelection&
        ) = default;
};

// Cancellation is a successful response with no selection. A preference
// warning is reported beside an accepted selection so callers never lose the
// path because remembering its directory failed.
template <typename Selection>
struct FileDialogResponse final
{
    std::optional<Selection> selection;
    std::optional<Domain::OperationError> preferenceWarning;
};

class FileDialogPort
{
public:
    virtual ~FileDialogPort() = default;

    [[nodiscard]] virtual Domain::Result<FileDialogResponse<FileDialogPath>>
    openOneFile(const OpenFileDialogRequest& request) = 0;

    [[nodiscard]] virtual Domain::Result<
        FileDialogResponse<std::vector<FileDialogPath>>>
    openManyFiles(const OpenFileDialogRequest& request) = 0;

    [[nodiscard]] virtual Domain::Result<
        FileDialogResponse<SavedFileDialogSelection>>
    saveFile(const SaveFileDialogRequest& request) = 0;

    [[nodiscard]] virtual Domain::Result<FileDialogResponse<FileDialogPath>>
    selectDirectory(const SelectDirectoryDialogRequest& request) = 0;
};

} // namespace ClassMngr::Next::Application
