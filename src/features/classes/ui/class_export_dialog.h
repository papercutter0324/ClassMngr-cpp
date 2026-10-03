#pragma once

#include "ui/shared/dialogs/dialog_shell.h"

#include <QList>

class ApplicationServices;
class QListWidget;
class QPushButton;

class ClassExportDialog : public DialogShell
{
    Q_OBJECT

public:
    explicit ClassExportDialog(
        ApplicationServices* applicationServices,
        QWidget* parent = nullptr
        );

    [[nodiscard]] QList<int> selectedClassIds() const;

private:
    void setAllChecked(bool checked);
    void updateExportEnabled();

    QListWidget* m_classList = nullptr;
    QPushButton* m_exportButton = nullptr;
};
