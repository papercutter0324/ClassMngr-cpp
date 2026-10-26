#include "next/ui/qt_file_dialog_presenter.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QStandardPaths>
#include <QThread>
#include <QWidget>

#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

using Application::FileDialogPath;

QString qString(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(
        bytes.constData(),
        static_cast<std::size_t>(bytes.size())
        );
}

QString qPath(const FileDialogPath& path)
{
    return qString(path.value());
}

QStringList qNameFilters(const std::vector<std::string>& nameFilters)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(nameFilters.size()));
    for (const std::string& filter : nameFilters)
    {
        result.append(qString(filter));
    }
    return result;
}

FileDialogPath fileDialogPath(const QString& path)
{
    return FileDialogPath(utf8(path));
}

QString defaultDirectory(const Application::FileDialogPurpose purpose)
{
    const QStandardPaths::StandardLocation location =
        purpose == Application::FileDialogPurpose::SignatureImage
            ? QStandardPaths::PicturesLocation
            : QStandardPaths::DocumentsLocation;
    const QString directory = QStandardPaths::writableLocation(location);
    return directory.isEmpty() || !QDir(directory).exists()
        ? QDir::homePath()
        : directory;
}

QString initialDirectory(
    Application::FileDialogDirectoryPreferencesPort& preferences,
    const Application::FileDialogPurpose purpose,
    const FileDialogPath& requestedDirectory
    )
{
    const QString requested = qPath(requestedDirectory);
    if (!requested.trimmed().isEmpty())
    {
        return QDir::cleanPath(requested);
    }

    const Application::DirectoryPreferencePath rememberedPath =
        preferences.readDirectory(purpose);
    const QString remembered = qString(rememberedPath.value());
    return remembered.isEmpty() ? defaultDirectory(purpose) : remembered;
}

QString normalizedExistingPath(const QString& path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty()
        ? QDir::cleanPath(info.absoluteFilePath())
        : canonical;
}

QString normalizedOutputPath(const QString& path)
{
    const QFileInfo info(path);
    const QFileInfo directoryInfo(info.absolutePath());
    const QString canonicalDirectory = directoryInfo.canonicalFilePath();
    const QString directory = canonicalDirectory.isEmpty()
        ? QDir::cleanPath(directoryInfo.absoluteFilePath())
        : canonicalDirectory;
    return QDir(directory).filePath(info.fileName());
}

bool canPresentFileDialog(const QWidget* parent)
{
    const auto* application = qobject_cast<QApplication*>(
        QCoreApplication::instance()
        );
    return application
        && application->thread() == QThread::currentThread()
        && (!parent || parent->thread() == QThread::currentThread());
}

template <typename Selection>
Domain::Result<Application::FileDialogResponse<Selection>> presenterUnavailable()
{
    return Domain::Result<Application::FileDialogResponse<Selection>>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Qt file dialogs require the application's GUI thread.",
        .recoverable = false
    });
}

void applyDialogPolicy(QFileDialog& dialog, const bool useNativeDialog)
{
    dialog.setObjectName(QStringLiteral("classmngrNextFileDialog"));
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeDialog);
}

void applyNameFilters(
    QFileDialog& dialog,
    const std::vector<std::string>& nameFilters
    )
{
    const QStringList filters = qNameFilters(nameFilters);
    if (!filters.isEmpty())
    {
        dialog.setNameFilters(filters);
    }
}

std::optional<QString> acceptedPath(QFileDialog& dialog)
{
    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty())
    {
        return std::nullopt;
    }
    return selectedFiles.first();
}

std::optional<Domain::OperationError> rememberDirectory(
    Application::FileDialogDirectoryPreferencesPort& preferences,
    const Application::FileDialogPurpose purpose,
    const QString& directory
    )
{
    const Domain::Result<void> result = preferences.writeDirectory(
        purpose,
        Application::DirectoryPreferencePath(utf8(directory))
        );
    if (result)
    {
        return std::nullopt;
    }
    return result.error();
}

} // namespace

QtFileDialogPresenter::QtFileDialogPresenter(
    Application::FileDialogDirectoryPreferencesPort& directoryPreferences,
    QWidget* parent,
    const FileDialogBackend backend
    ) noexcept
    : m_directoryPreferences(directoryPreferences),
      m_parent(parent),
      m_backend(backend)
{
}

bool QtFileDialogPresenter::platformUsesNativeDialogs() noexcept
{
#if defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

bool QtFileDialogPresenter::usesNativeDialogs() const noexcept
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

Domain::Result<
    Application::FileDialogResponse<Application::FileDialogPath>>
QtFileDialogPresenter::openOneFile(
    const Application::OpenFileDialogRequest& request
    )
{
    using Response = Application::FileDialogResponse<FileDialogPath>;
    if (!canPresentFileDialog(m_parent))
    {
        return presenterUnavailable<FileDialogPath>();
    }

    QFileDialog dialog(
        m_parent,
        qString(request.title),
        initialDirectory(
            m_directoryPreferences,
            request.purpose,
            request.initialDirectory
            )
        );
    applyDialogPolicy(dialog, usesNativeDialogs());
    applyNameFilters(dialog, request.nameFilters);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);

    const std::optional<QString> selectedPath = acceptedPath(dialog);
    if (!selectedPath)
    {
        return Domain::Result<Response>::success(Response{});
    }

    const QString normalized = normalizedExistingPath(*selectedPath);
    Response response;
    response.selection = fileDialogPath(normalized);
    response.preferenceWarning = rememberDirectory(
        m_directoryPreferences,
        request.purpose,
        QFileInfo(normalized).absolutePath()
        );
    return Domain::Result<Response>::success(std::move(response));
}

Domain::Result<
    Application::FileDialogResponse<std::vector<FileDialogPath>>>
QtFileDialogPresenter::openManyFiles(
    const Application::OpenFileDialogRequest& request
    )
{
    using Paths = std::vector<FileDialogPath>;
    using Response = Application::FileDialogResponse<Paths>;
    if (!canPresentFileDialog(m_parent))
    {
        return presenterUnavailable<Paths>();
    }

    QFileDialog dialog(
        m_parent,
        qString(request.title),
        initialDirectory(
            m_directoryPreferences,
            request.purpose,
            request.initialDirectory
            )
        );
    applyDialogPolicy(dialog, usesNativeDialogs());
    applyNameFilters(dialog, request.nameFilters);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFiles);

    if (dialog.exec() != QDialog::Accepted)
    {
        return Domain::Result<Response>::success(Response{});
    }

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty())
    {
        return Domain::Result<Response>::success(Response{});
    }

    Paths paths;
    paths.reserve(static_cast<std::size_t>(selectedFiles.size()));
    for (const QString& selectedFile : selectedFiles)
    {
        paths.push_back(fileDialogPath(normalizedExistingPath(selectedFile)));
    }

    Response response;
    response.selection = paths;
    response.preferenceWarning = rememberDirectory(
        m_directoryPreferences,
        request.purpose,
        QFileInfo(qPath(paths.front())).absolutePath()
        );
    return Domain::Result<Response>::success(std::move(response));
}

Domain::Result<
    Application::FileDialogResponse<Application::SavedFileDialogSelection>>
QtFileDialogPresenter::saveFile(
    const Application::SaveFileDialogRequest& request
    )
{
    using Selection = Application::SavedFileDialogSelection;
    using Response = Application::FileDialogResponse<Selection>;
    if (!canPresentFileDialog(m_parent))
    {
        return presenterUnavailable<Selection>();
    }

    QString startingPath = initialDirectory(
        m_directoryPreferences,
        request.purpose,
        request.initialDirectory
        );
    if (!request.suggestedFileName.empty())
    {
        startingPath = QDir(startingPath).filePath(
            qString(request.suggestedFileName)
            );
    }

    const bool accessoryRequested = !request.openAfterSavingText.empty();
    const bool useNativeDialog = usesNativeDialogs() && !accessoryRequested;
    QFileDialog dialog(m_parent, qString(request.title), startingPath);
    applyDialogPolicy(dialog, useNativeDialog);
    applyNameFilters(dialog, request.nameFilters);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix(qString(request.defaultSuffix));
    dialog.setOption(
        QFileDialog::DontConfirmOverwrite,
        !request.confirmOverwrite
        );

    QCheckBox* openAfterSavingCheck = nullptr;
    if (accessoryRequested)
    {
        openAfterSavingCheck = new QCheckBox(
            qString(request.openAfterSavingText),
            &dialog
            );
        openAfterSavingCheck->setObjectName(
            QStringLiteral("classmngrNextOpenAfterSaving")
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

    const std::optional<QString> selectedPath = acceptedPath(dialog);
    if (!selectedPath)
    {
        return Domain::Result<Response>::success(Response{});
    }

    const QString normalized = normalizedOutputPath(*selectedPath);
    Response response;
    response.selection = Selection{
        .path = fileDialogPath(normalized),
        .openAfterSaving = openAfterSavingCheck
            && openAfterSavingCheck->isChecked()
    };
    response.preferenceWarning = rememberDirectory(
        m_directoryPreferences,
        request.purpose,
        QFileInfo(normalized).absolutePath()
        );
    return Domain::Result<Response>::success(std::move(response));
}

Domain::Result<
    Application::FileDialogResponse<Application::FileDialogPath>>
QtFileDialogPresenter::selectDirectory(
    const Application::SelectDirectoryDialogRequest& request
    )
{
    using Response = Application::FileDialogResponse<FileDialogPath>;
    if (!canPresentFileDialog(m_parent))
    {
        return presenterUnavailable<FileDialogPath>();
    }

    QFileDialog dialog(
        m_parent,
        qString(request.title),
        initialDirectory(
            m_directoryPreferences,
            request.purpose,
            request.initialDirectory
            )
        );
    applyDialogPolicy(dialog, usesNativeDialogs());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);

    const std::optional<QString> selectedPath = acceptedPath(dialog);
    if (!selectedPath)
    {
        return Domain::Result<Response>::success(Response{});
    }

    const QString normalized = normalizedExistingPath(*selectedPath);
    Response response;
    response.selection = fileDialogPath(normalized);
    response.preferenceWarning = rememberDirectory(
        m_directoryPreferences,
        request.purpose,
        normalized
        );
    return Domain::Result<Response>::success(std::move(response));
}

} // namespace ClassMngr::Next::Ui
