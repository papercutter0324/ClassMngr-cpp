#include "edit_controller.h"

#include "ui/actions/action_registry.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>

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