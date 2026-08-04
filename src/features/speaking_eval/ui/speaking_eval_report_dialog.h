#pragma once

#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <QDialog>

class QComboBox;
class QDate;
class QLabel;
class QPushButton;
class SpeakingEvalPrivateNotesEditor;

[[nodiscard]] QString speakingEvalReportDate(
    const QDate& date,
    SpeakingEvalReportTemplate reportTemplate
    );

[[nodiscard]] QList<SpeakingEvalBatchReportService::StudentReport>
buildSpeakingEvalStudentReports(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    const QByteArray& signatureImage = {}
    );

[[nodiscard]] int speakingEvalReportIndexForSourceRow(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    int sourceRow
    );

class SpeakingEvalReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpeakingEvalReportDialog(
        const SpeakingEvalRows& rows,
        const ClassInfo& classInfo,
        QWidget* parent = nullptr
        );

    explicit SpeakingEvalReportDialog(
        const SpeakingEvalRows& rows,
        const ClassInfo& classInfo,
        const QByteArray& signatureImage,
        QWidget* parent = nullptr
        );

    explicit SpeakingEvalReportDialog(
        const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
        int currentStudentIndex,
        QWidget* parent = nullptr,
        bool interactive = false
        );

signals:
    void reportValueEdited(
        int sourceRow,
        SpeakingEvalColumn column,
        const QString& value
        );

private:
    [[nodiscard]] const SpeakingEvalBatchReportService::StudentReport*
    currentReport() const;

    void updateReport();

    void moveToPreviousStudent();

    void moveToNextStudent();

    void updatePrivateNotes();

    [[nodiscard]] QString currentAiPrompt() const;

    [[nodiscard]] QString aiPromptUnavailableReason() const;

    void updateAiPromptActions();

    void previewAiPrompt();

    void copyAiPromptAndOpen();

    void printCurrentReport();

    void saveCurrentReportAsPdf();

private:
    QList<SpeakingEvalBatchReportService::StudentReport> m_reports;
    QComboBox* m_studentSelector = nullptr;
    QLabel* m_notesLabel = nullptr;
    SpeakingEvalPrivateNotesEditor* m_notesFields = nullptr;
    QPushButton* m_previewAiPromptButton = nullptr;
    QPushButton* m_copyOpenAiPromptButton = nullptr;
    SpeakingEvalReportWidget* m_report = nullptr;
    bool m_interactive = false;
};
