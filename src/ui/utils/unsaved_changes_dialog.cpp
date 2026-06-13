#include "unsaved_changes_dialog.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QWidget>

UnsavedChangesChoice showUnsavedChangesDialog(
    QWidget* parent,
    const QString& title,
    const QString& message
    )
{
    QMessageBox dialog(parent);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(title);
    dialog.setText(message);
    dialog.setInformativeText(
        QObject::tr("Save your changes before leaving?")
        );

    QPushButton* saveButton =
        dialog.addButton(
            QObject::tr("Save"),
            QMessageBox::AcceptRole
            );

    QPushButton* discardButton =
        dialog.addButton(
            QObject::tr("Discard Changes"),
            QMessageBox::DestructiveRole
            );

    QPushButton* cancelButton =
        dialog.addButton(
            QObject::tr("Cancel"),
            QMessageBox::RejectRole
            );

    dialog.setDefaultButton(cancelButton);
    dialog.setEscapeButton(cancelButton);

    dialog.exec();

    QAbstractButton* clickedButton =
        dialog.clickedButton();

    if (clickedButton == saveButton)
    {
        return UnsavedChangesChoice::Save;
    }

    if (clickedButton == discardButton)
    {
        return UnsavedChangesChoice::Discard;
    }

    return UnsavedChangesChoice::Cancel;
}
