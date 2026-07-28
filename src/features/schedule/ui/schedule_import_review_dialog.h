#pragma once

#include "domain/models/schedule_import.h"
#include "features/schedule/ui/schedule_import_dialog_shared.h"

#include <QDialog>
#include <QList>
#include <QString>

class ApplicationServices;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class ScheduleWidget;

class ScheduleImportReviewDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ScheduleImportReviewDialog(
        ApplicationServices* services,
        ScheduleImportReviewRequest request,
        QWidget* parent = nullptr
        );

    [[nodiscard]] bool prepare();

private:
    struct TeacherControl
    {
        QString teacherKey;
        QComboBox* action = nullptr;
        QComboBox* room = nullptr;
    };

    struct ClassControl
    {
        int candidateIndex = -1;
        QString teacherKey;
        QComboBox* action = nullptr;
        QPushButton* colorButton = nullptr;
        QString color;
        QLabel* details = nullptr;
    };

    void buildUi();
    void rebuildResolutionControls();
    void chooseClassColor(int candidateIndex);
    void updateClassColorButton(ClassControl* control);
    void updateReviewState();
    void resizeForReviewStage();
    void applyImport();

    [[nodiscard]] ScheduleImportPlan importPlan() const;

    ApplicationServices* m_services = nullptr;
    ScheduleImportReviewRequest m_request;
    ScheduleImportPreview m_preview;
    bool m_prepared = false;

    QWidget* m_reviewPage = nullptr;
    ScheduleWidget* m_previewWidget = nullptr;
    QLabel* m_warningLabel = nullptr;
    QCheckBox* m_warningAcknowledgement = nullptr;
    QWidget* m_resolutionContent = nullptr;
    QVBoxLayout* m_resolutionLayout = nullptr;
    QScrollArea* m_resolutionScrollArea = nullptr;
    QLabel* m_reviewStatus = nullptr;
    QLabel* m_reviewSummary = nullptr;
    QList<TeacherControl> m_teacherControls;
    QList<ClassControl> m_classControls;

    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_importButton = nullptr;
};
