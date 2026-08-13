#include "file_dialog_service.h"

#include "ui/shared/styles/file_dialog_icon_style.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QWidget>

namespace
{

QString purposeKey(
    FileDialogPurpose purpose
    )
{
    switch (purpose)
    {
        case FileDialogPurpose::General:
            return QStringLiteral("general");
        case FileDialogPurpose::TeacherProfile:
            return QStringLiteral("teacher-profile");
        case FileDialogPurpose::ImportWorkbook:
            return QStringLiteral("import-workbook");
        case FileDialogPurpose::ExportReport:
            return QStringLiteral("export-report");
        case FileDialogPurpose::SignatureImage:
            return QStringLiteral("signature-image");
        case FileDialogPurpose::GeneratedPdf:
            return QStringLiteral("generated-pdf");
    }

    return QStringLiteral("general");
}

QString settingsKey(
    FileDialogPurpose purpose
    )
{
    return QStringLiteral("file-dialog/directories/%1")
        .arg(purposeKey(purpose));
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
    QSettings* settings,
    FileDialogBackend backend
    )
    : m_settings(settings),
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

    if (m_settings)
    {
        return m_settings->value(settingsKey(purpose)).toString();
    }

    QSettings settings;
    return settings.value(settingsKey(purpose)).toString();
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

    if (m_settings)
    {
        m_settings->setValue(settingsKey(purpose), directory);
        return;
    }

    QSettings settings;
    settings.setValue(settingsKey(purpose), directory);
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
    applyQtDialogPolicy(dialog, usesNativeDialogs());
    applyNameFilters(dialog, request.nameFilters);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix(request.defaultSuffix);
    dialog.setOption(
        QFileDialog::DontConfirmOverwrite,
        !request.confirmOverwrite
        );

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
    return selectedPath;
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
