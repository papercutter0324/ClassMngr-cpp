#pragma once

#include "ui/shared/dialogs/file_dialog_service.h"

#include <QQueue>
#include <QVector>

class FakeFileDialogService final : public IFileDialogService
{
public:
    [[nodiscard]] std::optional<QString> openFile(
        const OpenFileRequest& request
        ) override
    {
        openFileRequests.append(request);
        return scriptedOpenFiles.isEmpty()
            ? std::nullopt
            : scriptedOpenFiles.dequeue();
    }

    [[nodiscard]] QStringList openFiles(
        const OpenFileRequest& request
        ) override
    {
        openFilesRequests.append(request);
        return scriptedOpenFileLists.isEmpty()
            ? QStringList()
            : scriptedOpenFileLists.dequeue();
    }

    [[nodiscard]] std::optional<QString> saveFile(
        const SaveFileRequest& request
        ) override
    {
        saveFileRequests.append(request);
        return scriptedSaveFiles.isEmpty()
            ? std::nullopt
            : scriptedSaveFiles.dequeue();
    }

    [[nodiscard]] std::optional<SaveFileSelection> saveFileWithOptions(
        const SaveFileRequest& request
        ) override
    {
        saveFileWithOptionsRequests.append(request);
        return scriptedSaveFileSelections.isEmpty()
            ? std::nullopt
            : scriptedSaveFileSelections.dequeue();
    }

    [[nodiscard]] std::optional<QString> selectDirectory(
        const DirectoryRequest& request
        ) override
    {
        directoryRequests.append(request);
        return scriptedDirectories.isEmpty()
            ? std::nullopt
            : scriptedDirectories.dequeue();
    }

    QVector<OpenFileRequest> openFileRequests;
    QVector<OpenFileRequest> openFilesRequests;
    QVector<SaveFileRequest> saveFileRequests;
    QVector<SaveFileRequest> saveFileWithOptionsRequests;
    QVector<DirectoryRequest> directoryRequests;
    QQueue<std::optional<QString>> scriptedOpenFiles;
    QQueue<QStringList> scriptedOpenFileLists;
    QQueue<std::optional<QString>> scriptedSaveFiles;
    QQueue<std::optional<SaveFileSelection>> scriptedSaveFileSelections;
    QQueue<std::optional<QString>> scriptedDirectories;
};
