#include "file_dialog_service.h"

#include "ui/shared/styles/file_dialog_icon_style.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QStandardPaths>
#include <QWidget>

#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace
{

IFileDialogService* testFileDialogService = nullptr;
IFileDialogService* applicationFileDialogService = nullptr;

QString defaultDirectory(
    FileDialogPurpose purpose
    )
{
    const QStandardPaths::StandardLocation location =
        purpose == FileDialogPurpose::SignatureImage
            ? QStandardPaths::PicturesLocation
            : QStandardPaths::DocumentsLocation;
    const QString directory =
        QStandardPaths::writableLocation(location);
    return directory.isEmpty()
        ? QDir::homePath()
        : directory;
}

QString normalizedExistingPath(
    const QString& path
    )
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty()
        ? QDir::cleanPath(info.absoluteFilePath())
        : canonical;
}

QString normalizedOutputPath(
    const QString& path
    )
{
    const QFileInfo info(path);
    const QFileInfo directoryInfo(info.absolutePath());
    const QString canonicalDirectory = directoryInfo.canonicalFilePath();
    const QString directory = canonicalDirectory.isEmpty()
        ? QDir::cleanPath(directoryInfo.absoluteFilePath())
        : canonicalDirectory;
    return QDir(directory).filePath(info.fileName());
}

void applyQtDialogPolicy(
    QFileDialog& dialog,
    bool useNativeDialog
    )
{
    dialog.setObjectName(
        QStringLiteral("classmngrFileDialog")
        );
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setOption(
        QFileDialog::DontUseNativeDialog,
        !useNativeDialog
        );

    if (useNativeDialog)
    {
        return;
    }

    auto* style = new FileDialogIconStyle();
    style->setParent(&dialog);
    dialog.setStyle(style);
    style->polish(&dialog);
}

void applyNameFilters(
    QFileDialog& dialog,
    const QStringList& nameFilters
    )
{
    if (!nameFilters.isEmpty())
    {
        dialog.setNameFilters(nameFilters);
    }
}

}

QtFileDialogService::QtFileDialogService(
    ClassMngr::Next::Application::FileDialogDirectoryPreferencesPort&
        directoryPreferences,
    FileDialogBackend backend
    )
    : m_directoryPreferences(directoryPreferences),
      m_backend(backend)
{
}

bool QtFileDialogService::platformUsesNativeDialogs()
{
#if defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

bool QtFileDialogService::usesNativeDialogs() const
{
    switch (m_backend)
    {
        case FileDialogBackend::Native:
            return true;
        case FileDialogBackend::Qt:
            return false;
        case FileDialogBackend::PlatformDefault:
            return platformUsesNativeDialogs();
    }

    return platformUsesNativeDialogs();
}

QString QtFileDialogService::initialDirectory(
    FileDialogPurpose purpose,
    const QString& requestedDirectory
    ) const
{
    if (!requestedDirectory.trimmed().isEmpty())
    {
        return QDir::cleanPath(requestedDirectory);
    }

    const std::string rememberedUtf8 =
        m_directoryPreferences.readDirectory(purpose);
    const QString remembered = QString::fromUtf8(
        rememberedUtf8.data(),
        static_cast<qsizetype>(rememberedUtf8.size())
        );
    return remembered.isEmpty()
        ? defaultDirectory(purpose)
        : remembered;
}

void QtFileDialogService::rememberDirectory(
    FileDialogPurpose purpose,
    const QString& selectedPath,
    bool selectedPathIsDirectory
    )
{
    const QString directory = selectedPathIsDirectory
        ? normalizedExistingPath(selectedPath)
        : QFileInfo(selectedPath).absolutePath();

    const QByteArray encodedDirectory = directory.toUtf8();
    m_directoryPreferences.writeDirectory(
        purpose,
        std::string_view(
            encodedDirectory.constData(),
            static_cast<std::size_t>(encodedDirectory.size())
            )
        );
}

std::optional<QString> QtFileDialogService::openFile(
    const OpenFileRequest& request
    )
{
    QFileDialog dialog(
        request.parent,
        request.title,
        initialDirectory(request.purpose, request.initialDirectory)
        );
    applyQtDialogPolicy(dialog, usesNativeDialogs());
    applyNameFilters(dialog, request.nameFilters);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty())
    {
        return std::nullopt;
    }

    const QString selectedPath = normalizedExistingPath(selectedFiles.first());
    rememberDirectory(request.purpose, selectedPath, false);
    return selectedPath;
}

QStringList QtFileDialogService::openFiles(
    const OpenFileRequest& request
    )
{
    QFileDialog dialog(
        request.parent,
        request.title,
        initialDirectory(request.purpose, request.initialDirectory)
        );
    applyQtDialogPolicy(dialog, usesNativeDialogs());
    applyNameFilters(dialog, request.nameFilters);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFiles);

    if (dialog.exec() != QDialog::Accepted)
    {
        return {};
    }

    QStringList selectedFiles = dialog.selectedFiles();
    for (QString& selectedFile : selectedFiles)
    {
        selectedFile = normalizedExistingPath(selectedFile);
    }

    if (!selectedFiles.isEmpty())
    {
        rememberDirectory(request.purpose, selectedFiles.first(), false);
    }
    return selectedFiles;
}

std::optional<QString> QtFileDialogService::saveFile(
    const SaveFileRequest& request
    )
{
    const std::optional<SaveFileSelection> selection =
        saveFileWithOptions(request);
    return selection
        ? std::optional<QString>(selection->path)
        : std::nullopt;
}

std::optional<SaveFileSelection> QtFileDialogService::saveFileWithOptions(
    const SaveFileRequest& request
    )
{
    QString startingPath = initialDirectory(
        request.purpose,
        request.initialDirectory
        );
    if (!request.suggestedFileName.isEmpty())
    {
        startingPath = QDir(startingPath).filePath(
            request.suggestedFileName
            );
    }

    QFileDialog dialog(
        request.parent,
        request.title,
        startingPath
        );
    const bool useNativeDialog =
        usesNativeDialogs()
        && request.openAfterSavingText.isEmpty();
    applyQtDialogPolicy(dialog, useNativeDialog);
    applyNameFilters(dialog, request.nameFilters);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix(request.defaultSuffix);
    dialog.setOption(
        QFileDialog::DontConfirmOverwrite,
        !request.confirmOverwrite
        );

    QCheckBox* openAfterSavingCheck = nullptr;
    if (
        !request.openAfterSavingText.isEmpty()
        && !useNativeDialog
        )
    {
        openAfterSavingCheck = new QCheckBox(
            request.openAfterSavingText,
            &dialog
            );
        openAfterSavingCheck->setObjectName(
            QStringLiteral("fileDialogOpenAfterSaving")
            );
        if (auto* layout = qobject_cast<QGridLayout*>(dialog.layout()))
        {
            layout->addWidget(
                openAfterSavingCheck,
                layout->rowCount(),
                0,
                1,
                layout->columnCount()
                );
        }
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty())
    {
        return std::nullopt;
    }

    const QString selectedPath = normalizedOutputPath(selectedFiles.first());
    rememberDirectory(request.purpose, selectedPath, false);
    return SaveFileSelection{
        selectedPath,
        openAfterSavingCheck && openAfterSavingCheck->isChecked()
    };
}

std::optional<QString> QtFileDialogService::selectDirectory(
    const DirectoryRequest& request
    )
{
    QFileDialog dialog(
        request.parent,
        request.title,
        initialDirectory(request.purpose, request.initialDirectory)
        );
    applyQtDialogPolicy(dialog, usesNativeDialogs());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty())
    {
        return std::nullopt;
    }

    const QString selectedPath = normalizedExistingPath(selectedFiles.first());
    rememberDirectory(request.purpose, selectedPath, true);
    return selectedPath;
}

IFileDialogService& DialogServices::fileDialogs()
{
    if (testFileDialogService)
    {
        return *testFileDialogService;
    }

    if (applicationFileDialogService)
    {
        return *applicationFileDialogService;
    }

    throw std::logic_error(
        "File dialog service must be installed before application use."
        );
}

void DialogServices::setFileDialogServiceForApplication(
    IFileDialogService* service
    )
{
    applicationFileDialogService = service;
}

void DialogServices::setFileDialogServiceForTesting(
    IFileDialogService* service
    )
{
    testFileDialogService = service;
}
