#pragma once

#include <QString>

class QWidget;

enum class UnsavedChangesChoice
{
    Save,
    Discard,
    Cancel
};

UnsavedChangesChoice showUnsavedChangesDialog(
    QWidget* parent,
    const QString& title,
    const QString& message
    );
