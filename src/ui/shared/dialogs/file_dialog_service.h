#pragma once

#include <QString>
#include <QStringList>

#include <optional>

class QSettings;
class QWidget;

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

enum class FileDialogBackend
{
    PlatformDefault,
    Native,
    Qt
};

struct OpenFileRequest
{
    QWidget* parent = nullptr;
    QString title;
    FileDialogPurpose purpose = FileDialogPurpose::General;
    QString initialDirectory;
    QStringList nameFilters;
};

struct SaveFileRequest
{
    QWidget* parent = nullptr;
    QString title;
    FileDialogPurpose purpose = FileDialogPurpose::General;
    QString initialDirectory;
    QString suggestedFileName;
    QStringList nameFilters;
    QString defaultSuffix;
    bool confirmOverwrite = true;
    QString openAfterSavingText;
};

struct SaveFileSelection
{
    QString path;
    bool openAfterSaving = false;
};

struct DirectoryRequest
{
    QWidget* parent = nullptr;
    QString title;
    FileDialogPurpose purpose = FileDialogPurpose::General;
    QString initialDirectory;
};

class IFileDialogService
{
public:
    virtual ~IFileDialogService() = default;

    [[nodiscard]] virtual std::optional<QString> openFile(
        const OpenFileRequest& request
        ) = 0;

    [[nodiscard]] virtual QStringList openFiles(
        const OpenFileRequest& request
        ) = 0;

    [[nodiscard]] virtual std::optional<QString> saveFile(
        const SaveFileRequest& request
        ) = 0;

    [[nodiscard]] virtual std::optional<SaveFileSelection> saveFileWithOptions(
        const SaveFileRequest& request
        ) = 0;

    [[nodiscard]] virtual std::optional<QString> selectDirectory(
        const DirectoryRequest& request
        ) = 0;
};

class QtFileDialogService final : public IFileDialogService
{
public:
    explicit QtFileDialogService(
        QSettings* settings = nullptr,
        FileDialogBackend backend = FileDialogBackend::PlatformDefault
        );

    [[nodiscard]] static bool platformUsesNativeDialogs();

    [[nodiscard]] std::optional<QString> openFile(
        const OpenFileRequest& request
        ) override;

    [[nodiscard]] QStringList openFiles(
        const OpenFileRequest& request
        ) override;

    [[nodiscard]] std::optional<QString> saveFile(
        const SaveFileRequest& request
        ) override;

    [[nodiscard]] std::optional<SaveFileSelection> saveFileWithOptions(
        const SaveFileRequest& request
        ) override;

    [[nodiscard]] std::optional<QString> selectDirectory(
        const DirectoryRequest& request
        ) override;

private:
    [[nodiscard]] bool usesNativeDialogs() const;
    [[nodiscard]] QString initialDirectory(
        FileDialogPurpose purpose,
        const QString& requestedDirectory
        ) const;
    void rememberDirectory(
        FileDialogPurpose purpose,
        const QString& selectedPath,
        bool selectedPathIsDirectory
        );

    QSettings* m_settings = nullptr;
    FileDialogBackend m_backend = FileDialogBackend::PlatformDefault;
};

namespace DialogServices
{

[[nodiscard]] IFileDialogService& fileDialogs();

void setFileDialogServiceForTesting(
    IFileDialogService* service
    );

}
