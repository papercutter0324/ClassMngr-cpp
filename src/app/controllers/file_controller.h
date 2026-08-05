#ifndef FILE_CONTROLLER_H
#define FILE_CONTROLLER_H

#include <QObject>
#include <QString>

#include "ui/shared/actions/action_registry.h"

class ApplicationServices;
class MainWindow;

class FileController : public QObject
{
    Q_OBJECT

public:
    explicit FileController(
        ApplicationServices* services,
        MainWindow* window,
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    void populateRecentMenu();

    void loadMostRecentDatabase();

    void loadDatabaseOnStartup(
        const QString& filePath
        );

    void setSaveMode(
        SaveMode mode
        );

    bool confirmUnsavedChanges(
        bool exiting = false
        );

public slots:
    void autosave();

private:
    void newFile();
    void openFile();

    void openSpecificFile(
        const QString& filePath
        );

    bool loadDatabase(
        const QString& filePath,
        bool showErrorMessage = true
        );

    void saveFile();
    void saveAsFile();

    void exportAsFile();

    void closeFile();

    void updateRecentFiles(
        const QString& filePath
        );

    void clearRecentFiles();

    void saveDatabase();

    bool saveDatabaseAs(
        const QString& filePath
        );

    bool exportDatabaseAs(
        const QString& filePath
        );

    void closeActiveDatabase();

    void enterNoDatabaseState();

    QString mostRecentDatabasePath() const;

    void pruneRecentFile(
        const QString& filePath
        );

    QString databaseDialogDirectory() const;

    QString defaultDatabaseDirectory() const;

    void rememberDatabaseDirectory(
        const QString& filePath
        );

    QString normalizeInputFilePath(
        const QString& filePath
        ) const;

    QString normalizeNativeOutputFilePath(
        const QString& filePath
        ) const;

    QString absoluteFilePath(
        const QString& filePath,
        bool createDirectories
        ) const;

    void setLoadedFileState();
    void setNoFileState();

private:
    ApplicationServices* m_services{};

    MainWindow* m_window{};

    ActionRegistry* m_actions{};

    QString m_currentFile;
};

#endif // FILE_CONTROLLER_H
