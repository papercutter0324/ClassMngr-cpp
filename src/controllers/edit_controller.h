#ifndef EDIT_CONTROLLER_H
#define EDIT_CONTROLLER_H

#include <QObject>

class QWidget;

class ActionRegistry;

class EditController : public QObject
{
    Q_OBJECT

public:
    explicit EditController(
        QObject* parent = nullptr
        );

    void connectActions(
        ActionRegistry& actions
        );

    void updateActions();

private:
    bool dispatch(
        const char* method
        );

    bool hasMethod(
        QWidget* widget,
        const char* method
        ) const;

private slots:
    void cut();
    void copy();
    void paste();

    void undo();
    void redo();

    void updatePasteState();

private:
    ActionRegistry* m_actions{};
};

#endif // EDIT_CONTROLLER_H