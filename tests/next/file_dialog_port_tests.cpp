#include "next/application/file_dialog_port.h"

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

class FakeFileDialogPort final : public FileDialogPort
{
public:
    Domain::Result<FileDialogResponse<FileDialogPath>> openOneFile(
        const OpenFileDialogRequest& request
        ) override
    {
        ++openOneCalls;
        lastOpenRequest = request;
        return openOneResult;
    }

    Domain::Result<FileDialogResponse<std::vector<FileDialogPath>>>
    openManyFiles(const OpenFileDialogRequest& request) override
    {
        ++openManyCalls;
        lastOpenRequest = request;
        return openManyResult;
    }

    Domain::Result<FileDialogResponse<SavedFileDialogSelection>> saveFile(
        const SaveFileDialogRequest& request
        ) override
    {
        ++saveCalls;
        lastSaveRequest = request;
        return saveResult;
    }

    Domain::Result<FileDialogResponse<FileDialogPath>> selectDirectory(
        const SelectDirectoryDialogRequest& request
        ) override
    {
        ++directoryCalls;
        lastDirectoryRequest = request;
        return directoryResult;
    }

    int openOneCalls = 0;
    int openManyCalls = 0;
    int saveCalls = 0;
    int directoryCalls = 0;
    OpenFileDialogRequest lastOpenRequest;
    SaveFileDialogRequest lastSaveRequest;
    SelectDirectoryDialogRequest lastDirectoryRequest;
    Domain::Result<FileDialogResponse<FileDialogPath>> openOneResult =
        Domain::Result<FileDialogResponse<FileDialogPath>>::success({});
    Domain::Result<FileDialogResponse<std::vector<FileDialogPath>>>
        openManyResult =
            Domain::Result<
                FileDialogResponse<std::vector<FileDialogPath>>>::success({});
    Domain::Result<FileDialogResponse<SavedFileDialogSelection>> saveResult =
        Domain::Result<FileDialogResponse<SavedFileDialogSelection>>::success({});
    Domain::Result<FileDialogResponse<FileDialogPath>> directoryResult =
        Domain::Result<FileDialogResponse<FileDialogPath>>::success({});
};

bool pathValuesPreserveUtf8AndRequestsHaveLegacyDefaults()
{
    const std::string utf8Path = "/Users/teacher/授業/資料📘.pdf";
    const FileDialogPath path(utf8Path);
    const OpenFileDialogRequest openRequest{
        .purpose = FileDialogPurpose::ImportWorkbook,
        .title = "選択",
        .initialDirectory = FileDialogPath("/Users/teacher/授業") ,
        .nameFilters = {"Workbook (*.xlsx *.xlsm)", "All files (*)"}
    };
    const SaveFileDialogRequest defaultSaveRequest;

    return !path.empty()
        && path.value() == utf8Path
        && path == FileDialogPath(utf8Path)
        && openRequest.purpose == FileDialogPurpose::ImportWorkbook
        && openRequest.initialDirectory.value() == "/Users/teacher/授業"
        && openRequest.nameFilters.size() == 2
        && defaultSaveRequest.confirmOverwrite
        && defaultSaveRequest.openAfterSavingText.empty()
        && defaultSaveRequest.suggestedFileName.empty()
        && defaultSaveRequest.defaultSuffix.empty();
}

bool typedOperationsCarryRequestsAndReturnNormalCancellation()
{
    FakeFileDialogPort port;
    const OpenFileDialogRequest openRequest{
        .purpose = FileDialogPurpose::SignatureImage,
        .title = "Choose image",
        .initialDirectory = FileDialogPath("/tmp/画像"),
        .nameFilters = {"Images (*.png *.jpg)"}
    };
    const SaveFileDialogRequest saveRequest{
        .purpose = FileDialogPurpose::GeneratedPdf,
        .title = "Save report",
        .initialDirectory = FileDialogPath("/tmp/授業"),
        .suggestedFileName = "成績.pdf",
        .nameFilters = {"PDF (*.pdf)"},
        .defaultSuffix = "pdf",
        .confirmOverwrite = false,
        .openAfterSavingText = "Open after saving"
    };
    const SelectDirectoryDialogRequest directoryRequest{
        .purpose = FileDialogPurpose::SubPrepPackage,
        .title = "Choose folder"
    };

    const auto openOne = port.openOneFile(openRequest);
    const auto openMany = port.openManyFiles(openRequest);
    const auto save = port.saveFile(saveRequest);
    const auto directory = port.selectDirectory(directoryRequest);

    return openOne.hasValue()
        && !openOne.value().selection
        && !openMany.value().selection
        && !save.value().selection
        && !directory.value().selection
        && !openOne.value().preferenceWarning
        && port.openOneCalls == 1
        && port.openManyCalls == 1
        && port.saveCalls == 1
        && port.directoryCalls == 1
        && port.lastOpenRequest.purpose == FileDialogPurpose::SignatureImage
        && port.lastOpenRequest.nameFilters == openRequest.nameFilters
        && port.lastSaveRequest.suggestedFileName == "成績.pdf"
        && !port.lastSaveRequest.confirmOverwrite
        && port.lastSaveRequest.openAfterSavingText == "Open after saving"
        && port.lastDirectoryRequest.purpose == FileDialogPurpose::SubPrepPackage;
}

bool selectedPathAndPreferenceWarningAreIndependent()
{
    const FileDialogPath selectedPath("/tmp/資料.pdf");
    const Domain::OperationError warning{
        .code = Domain::ErrorCode::Technical,
        .message = "Unable to remember the selected directory.",
        .recoverable = true
    };
    const FileDialogResponse<FileDialogPath> response{
        .selection = selectedPath,
        .preferenceWarning = warning
    };

    return response.selection == selectedPath
        && response.preferenceWarning == warning;
}

bool presenterErrorsRemainDistinctFromCancellation()
{
    FakeFileDialogPort port;
    const Domain::OperationError failure{
        .code = Domain::ErrorCode::Technical,
        .message = "Qt file dialog is unavailable.",
        .recoverable = false
    };
    port.openOneResult =
        Domain::Result<FileDialogResponse<FileDialogPath>>::failure(failure);

    const auto failed = port.openOneFile(OpenFileDialogRequest{});
    const auto cancelled = FakeFileDialogPort{}.openOneFile(
        OpenFileDialogRequest{}
        );
    return !failed.hasValue()
        && failed.error() == failure
        && cancelled.hasValue()
        && !cancelled.value().selection;
}

} // namespace

int main()
{
    if (!pathValuesPreserveUtf8AndRequestsHaveLegacyDefaults()
        || !typedOperationsCarryRequestsAndReturnNormalCancellation()
        || !selectedPathAndPreferenceWarningAreIndependent()
        || !presenterErrorsRemainDistinctFromCancellation())
    {
        std::cerr << "Next file-dialog port tests failed.\n";
        return 1;
    }
    std::cout << "Next file-dialog port tests passed.\n";
    return 0;
}
