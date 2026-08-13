#pragma once

#include "domain/models/class_transfer.h"
#include "ui/shared/dialogs/dialog_shell.h"

#include <QList>
#include <QString>

class DataService;
class QComboBox;
class QLabel;
class QPushButton;

class ClassImportDialog : public DialogShell
{
    Q_OBJECT

public:
    explicit ClassImportDialog(
        DataService* dataService,
        const ClassTransferPackage& package,
        const ClassImportPreview& preview,
        QWidget* parent = nullptr
        );

    [[nodiscard]] ClassImportPlan importPlan() const;

private:
    struct ClassRow
    {
        int packageClassIndex = -1;
        QComboBox* choice = nullptr;
    };

    struct TeacherRow
    {
        QString teacherKey;
        QComboBox* choice = nullptr;
    };

    void updateImportEnabled();

    DataService* m_dataService = nullptr;
    ClassTransferPackage m_package;
    QList<ClassRow> m_classRows;
    QList<TeacherRow> m_teacherRows;
    QLabel* m_validationLabel = nullptr;
    QPushButton* m_importButton = nullptr;
};
