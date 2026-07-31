#pragma once

#include "domain/models/schedule_import.h"
#include "features/schedule/ui/schedule_import_dialog_shared.h"

#include <QDialog>
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

    [[nodiscard]] ScheduleImportPlan importPlan() const;

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
    QVBoxLayout* m_teacherLayout = nullptr;
    QVBoxLayout* m_classLayout = nullptr;
    QLabel* m_reviewStatus = nullptr;
    QLabel* m_reviewSummary = nullptr;
    QList<TeacherControl> m_teacherControls;
    QList<ClassControl> m_classControls;
    QString m_activeScheduleConflictSignature;
    QString m_lastWarnedScheduleConflictSignature;
    QString m_pendingScheduleConflictSignature;
    QString m_pendingScheduleConflictMessage;
    bool m_scheduleConflictWarningQueued = false;

    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_importButton = nullptr;
};
