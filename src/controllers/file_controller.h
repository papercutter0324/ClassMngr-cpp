#ifndef FILE_CONTROLLER_H
#define FILE_CONTROLLER_H

#include <QObject>
#include "ui/actions/action_registry.h"

class FileController : public QObject
{
    Q_OBJECT

public:
    explicit FileController(QObject* parent = nullptr);

    void connectActions(ActionRegistry& actions);

    void populate_recent_menu();

    void setSaveMode(SaveMode mode);

private:
    ActionRegistry* m_actions = nullptr;

private:
    void newFile();
    void openFile();
    void saveFile();
    void saveAsFile();
    void exportAsFile();
    void closeFile();
};
#endif