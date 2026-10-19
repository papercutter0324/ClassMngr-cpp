#pragma once

#include "next/application/directory_preference_path.h"
#include "next/domain/operation_result.h"

namespace ClassMngr::Next::Application
{

enum class FileDialogPurpose
{
    General,
    TeacherProfile,
    ImportWorkbook,
    ExportReport,
    SignatureImage,
    GeneratedPdf,
    ClassTransfer,
    SubPrepPackage
};

// Directory values cross this Qt-free boundary as UTF-8. An empty read means
// that no directory has been remembered for the requested purpose.
class FileDialogDirectoryPreferencesPort
{
public:
    virtual ~FileDialogDirectoryPreferencesPort() = default;

    [[nodiscard]] virtual DirectoryPreferencePath readDirectory(
        FileDialogPurpose purpose
        ) const = 0;

    [[nodiscard]] virtual Domain::Result<void> writeDirectory(
        FileDialogPurpose purpose,
        const DirectoryPreferencePath& directory
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
