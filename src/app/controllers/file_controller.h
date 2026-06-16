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

    void loadDatabase(
        const QString& filePath
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

    void saveDatabaseAs(
        const QString& filePath
        );

    void exportDatabaseAs(
        const QString& filePath
        );

    QString normalizeFilePath(
        const QString& filePath,
        const QString& extension = ".db"
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