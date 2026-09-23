#pragma once

#include <string>
#include <string_view>

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

    [[nodiscard]] virtual std::string readDirectory(
        FileDialogPurpose purpose
        ) const = 0;

    virtual void writeDirectory(
        FileDialogPurpose purpose,
        std::string_view utf8Directory
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
