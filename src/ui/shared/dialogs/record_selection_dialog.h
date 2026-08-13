#pragma once

#include "dialog_shell.h"

#include <QList>
#include <QPair>
#include <QString>

class NoWheelComboBox;
class QWidget;

class RecordSelectionDialog final : public DialogShell
{
    Q_OBJECT

public:
    RecordSelectionDialog(
        const QString& title,
        const QString& prompt,
        const QList<QPair<QString, int>>& records,
        QWidget* parent = nullptr
        );

    [[nodiscard]] int selectedRecordId() const;

private:
    NoWheelComboBox* m_records = nullptr;
};
