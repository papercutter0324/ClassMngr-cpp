#include "edit_controller.h"

#include "ui/actions/action_registry.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>

EditController::EditController(QObject* parent)
    : QObject(parent)
{
}

void EditController::connectActions(ActionRegistry& actions)
{
    m_actions = &actions;

    connect(actions.cut, &QAction::triggered,
            this, [this] { cut(); });

    connect(actions.copy, &QAction::triggered,
            this, [this] { copy(); });

    connect(actions.paste, &QAction::triggered,
            this, [this] { paste(); });

    connect(actions.undo, &QAction::triggered,
            this, [this] { undo(); });

    connect(actions.redo, &QAction::triggered,
            this, [this] { redo(); });

    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &EditController::updatePasteState);
}

void EditController::setEnabled(bool enabled)
{
    m_actions->cut->setEnabled(enabled);
    m_actions->copy->setEnabled(enabled);
    m_actions->paste->setEnabled(enabled);
    m_actions->undo->setEnabled(enabled);
    m_actions->redo->setEnabled(enabled);
}

void EditController::cut()
{
    // TODO
}

void EditController::copy()
{
    // TODO
}

void EditController::paste()
{
    // TODO
}

void EditController::undo()
{
    // TODO
}

void EditController::redo()
{
    // TODO
}

void EditController::updatePasteState()
{
    if (!m_actions)
        return;

    m_actions->paste->setEnabled(
        !QApplication::clipboard()->text().isEmpty()
        );
}