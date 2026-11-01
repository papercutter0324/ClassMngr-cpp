#pragma once

#include "domain/models/class_transfer.h"
#include "next/application/class_transfer_apply_request.h"
#include "next/application/class_transfer_preview_query.h"
#include "next/application/teacher_display_name_batch_read_port.h"
#include "ui/shared/dialogs/dialog_shell.h"

#include <QList>
#include <QString>

class ApplicationServices;
class QComboBox;
class QLabel;
class QPushButton;

class ClassImportDialog : public DialogShell
{
    Q_OBJECT

public:
    explicit ClassImportDialog(
        ApplicationServices* applicationServices,
        const ClassTransferPackage& package,
        const ClassImportPreview& preview,
        QWidget* parent = nullptr
        );

    ClassImportDialog(
        ApplicationServices* applicationServices,
        const ClassTransferPackage& package,
        const ClassImportPreview& preview,
        const ClassMngr::Next::Application::
            TeacherDisplayNameBatchReadPort& teacherDisplayNameBatchReadPort,
        QWidget* parent = nullptr
        );

    explicit ClassImportDialog(
        ApplicationServices* applicationServices,
        const ClassTransferPackage& package,
        const ClassMngr::Next::Application::ClassTransferPreviewResult& preview,
        QWidget* parent = nullptr
        );

    ClassImportDialog(
        ApplicationServices* applicationServices,
        const ClassTransferPackage& package,
        const ClassMngr::Next::Application::ClassTransferPreviewResult& preview,
        const ClassMngr::Next::Application::
            TeacherDisplayNameBatchReadPort& teacherDisplayNameBatchReadPort,
        QWidget* parent = nullptr
        );

    [[nodiscard]] ClassImportPlan importPlan() const;
    [[nodiscard]] ClassMngr::Next::Application::ClassTransferApplyRequest applyRequest() const;

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

    ClassTransferPackage m_package;
    ClassImportPreview m_preview;
    QList<ClassRow> m_classRows;
    QList<TeacherRow> m_teacherRows;
    QLabel* m_validationLabel = nullptr;
    QPushButton* m_importButton = nullptr;
};
