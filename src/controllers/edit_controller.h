#ifndef EDIT_CONTROLLER_H
#define EDIT_CONTROLLER_H

#include <QObject>
#include "ui/actions/action_registry.h"

class EditController : public QObject
{
    Q_OBJECT

public:
    explicit EditController(QObject* parent = nullptr);

    void connectActions(ActionRegistry& actions);

private:
    ActionRegistry* m_actions = nullptr;

private:
    void cut();
    void copy();
    void paste();
    void undo();
    void redo();
};

#endif // EDIT_CONTROLLER_H