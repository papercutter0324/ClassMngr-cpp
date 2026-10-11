#pragma once

#include "domain/models/schedule_import.h"
#include "features/schedule/ui/schedule_import_dialog_shared.h"
#include "features/schedule/ui/schedule_import_resolution_controls.h"
#include "next/application/schedule_import_apply_use_case.h"
#include "ui/shared/dialogs/dialog_shell.h"

#include <QList>
#include <QString>
#include <QStringList>

class ApplicationServices;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QResizeEvent;
class QScrollArea;
class QSplitter;
class QTabWidget;
class ScheduleWidget;

class ScheduleImportReviewDialog final : public DialogShell
{
    Q_OBJECT

public:
    explicit ScheduleImportReviewDialog(
        ApplicationServices* services,
        ScheduleImportReviewRequest request,
        QWidget* parent = nullptr
        );
    ~ScheduleImportReviewDialog() override;

    [[nodiscard]] bool prepare();

private:
    using TeacherControl =
        ScheduleImportResolutionControls::TeacherControl;
    using ClassControl =
        ScheduleImportResolutionControls::ClassControl;

    void buildUi();
    void rebuildResolutionControls(
        const ClassMngr::Next::Application::ScheduleImportStateSnapshot& snapshot
        );
    void rebuildRealizedControls();
    void updateRealizedWindowMetrics();
    void chooseClassColor(int candidateIndex);
    void updateReviewState();
    void updateScheduleConflictWarning(
        const QStringList& conflicts
        );
    void resizeForReviewStage();
    void updatePreviewVisibleRows();
    void applyImport();

protected:
    void resizeEvent(
        QResizeEvent* event
        ) override;

    [[nodiscard]] ClassMngr::Next::Application::ScheduleImportApplyRequest
    applyRequest() const;

    ApplicationServices* m_services = nullptr;
    ScheduleImportReviewRequest m_request;
    ScheduleImportPreview m_preview;
    bool m_prepared = false;

    QWidget* m_reviewPage = nullptr;
    QGroupBox* m_intensiveModeSection = nullptr;
    QRadioButton* m_updateIntensiveRadio = nullptr;
    QRadioButton* m_replaceIntensiveRadio = nullptr;
    QSplitter* m_reviewSplitter = nullptr;
    QWidget* m_previewPane = nullptr;
    QLabel* m_previewHeading = nullptr;
    ScheduleWidget* m_previewWidget = nullptr;
    QLabel* m_warningLabel = nullptr;
    QCheckBox* m_warningAcknowledgement = nullptr;
    QTabWidget* m_resolutionTabs = nullptr;
    QScrollArea* m_warningScrollArea = nullptr;
    QScrollArea* m_teacherScrollArea = nullptr;
    QScrollArea* m_classScrollArea = nullptr;
    QWidget* m_teacherContent = nullptr;
    QWidget* m_classContent = nullptr;
    QLabel* m_reviewStatus = nullptr;
    QLabel* m_reviewSummary = nullptr;
    ScheduleImportResolutionControls::BuildResult m_resolutionModel;
    ClassMngr::Next::Application::ScheduleImportStateSnapshot
        m_resolutionStateSnapshot;
    int m_lastTeacherFirst = -1;
    int m_lastTeacherLast = -1;
    int m_lastClassFirst = -1;
    int m_lastClassLast = -1;
    int m_lastTeacherContentWidth = -1;
    int m_lastClassContentWidth = -1;
    QString m_activeScheduleConflictSignature;
    QString m_lastWarnedScheduleConflictSignature;
    QString m_pendingScheduleConflictSignature;
    QString m_pendingScheduleConflictMessage;
    bool m_scheduleConflictWarningQueued = false;

    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_importButton = nullptr;
};
