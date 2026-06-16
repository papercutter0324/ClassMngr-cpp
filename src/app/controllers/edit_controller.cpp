#include "edit_controller.h"

#include "ui/shared/actions/action_registry.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

namespace
{

bool isClipboardMutationMethod(
    const char* method
    )
{
    const QByteArray methodName(method);

    return methodName == "cut"
        || methodName == "paste";
}

bool isReadOnlyTextWidget(
    QWidget* widget
    )
{
    if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
    {
        return lineEdit->isReadOnly();
    }

    if (auto* textEdit = qobject_cast<QTextEdit*>(widget))
    {
        return textEdit->isReadOnly();
    }

    if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(widget))
    {
        return plainTextEdit->isReadOnly();
    }

    return false;
}

bool methodAllowed(
    QWidget* widget,
    const char* method
    )
{
    return !isClipboardMutationMethod(method)
        || !isReadOnlyTextWidget(widget);
}

} // namespace

EditController::EditController(
    QObject* parent
    )
    : QObject(parent)
{
}

void EditController::connectActions(
    ActionRegistry& actions
    )
{
    m_actions = &actions;

    connect(
        actions.cut,
        &QAction::triggered,
        this,
        &EditController::cut
        );

    connect(
        actions.copy,
        &QAction::triggered,
        this,
        &EditController::copy
        );

    connect(
        actions.paste,
        &QAction::triggered,
        this,
        &EditController::paste
        );

    connect(
        actions.undo,
        &QAction::triggered,
        this,
        &EditController::undo
        );

    connect(
        actions.redo,
        &QAction::triggered,
        this,
        &EditController::redo
        );

    connect(
        QApplication::clipboard(),
        &QClipboard::dataChanged,
        this,
        &EditController::updatePasteState
        );

    connect(
        qApp,
        &QApplication::focusChanged,
        this,
        [this](QWidget*, QWidget*)
        {
            updateActions();
        }
        );

    updateActions();
}

void EditController::updateActions()
{
    if (!m_actions)
        return;

    QWidget* widget =
        QApplication::focusWidget();

    m_actions->copy->setEnabled(
        hasMethod(widget, "copy")
        );

    m_actions->cut->setEnabled(
        hasMethod(widget, "cut")
        );

    m_actions->undo->setEnabled(
        hasMethod(widget, "undo")
        );

    m_actions->redo->setEnabled(
        hasMethod(widget, "redo")
        );

    m_actions->paste->setEnabled(
        hasMethod(widget, "paste")
        && !QApplication::clipboard()
                ->text()
                .isEmpty()
        );
}

bool EditController::dispatch(
    const char* method
    )
{
    QWidget* widget =
        QApplication::focusWidget();

    while (widget)
    {
        const QByteArray signature =
            QByteArray(method) + "()";

        if (
            widget->metaObject()->indexOfMethod(
                signature.constData()
                ) >= 0
            )
        {
            if (!methodAllowed(widget, method))
            {
                return false;
            }

            if (
                QMetaObject::invokeMethod(
                    widget,
                    method,
                    Qt::DirectConnection
                    )
                )
            {
                return true;
            }
        }

        widget =
            qobject_cast<QWidget*>(
                widget->parent()
                );
    }

    return false;
}

bool EditController::hasMethod(
    QWidget* widget,
    const char* method
    ) const
{
    while (widget)
    {
        const QByteArray signature =
            QByteArray(method) + "()";

        if (
            widget->metaObject()->indexOfMethod(
                signature.constData()
                ) >= 0
            )
        {
            return methodAllowed(
                widget,
                method
                );
        }

        widget =
            qobject_cast<QWidget*>(
                widget->parent()
                );
    }

    return false;
}

void EditController::cut()
{
    dispatch("cut");
}

void EditController::copy()
{
    dispatch("copy");
}

void EditController::paste()
{
    dispatch("paste");
}

void EditController::undo()
{
    dispatch("undo");
}

void EditController::redo()
{
    dispatch("redo");
}

void EditController::updatePasteState()
{
    updateActions();
}
