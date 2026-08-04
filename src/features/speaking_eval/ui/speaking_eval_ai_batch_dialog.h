#pragma once

#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"

#include <QDialog>
#include <QList>
#include <QString>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

struct SpeakingEvalAiBatchAcceptedComment
{
    int sourceRow = -1;
    QString oldComment;
    QString newComment;
};

class SpeakingEvalAiBatchDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SpeakingEvalAiBatchDialog(
        const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
        QWidget* parent = nullptr
        );

    [[nodiscard]] QList<SpeakingEvalAiBatchAcceptedComment>
    acceptedComments() const;

private:
    void resetGeneratedContent();

    void updateCreatePromptButton();

    void createPrompt();

    void copyPrompt(
        bool openProvider
        );

    void parseResponse();

    void updateReviewRow(
        int row
        );

    void updateApplyButton();

    void applyComments();

private:
    QList<SpeakingEvalBatchReportService::StudentReport> m_reports;
    QList<int> m_selectedReportIndexes;
    QList<SpeakingEvalAiBatchAcceptedComment> m_acceptedComments;
    QTableWidget* m_selectionTable = nullptr;
    QPlainTextEdit* m_promptEdit = nullptr;
    QPushButton* m_createPromptButton = nullptr;
    QPushButton* m_copyPromptButton = nullptr;
    QPushButton* m_copyOpenButton = nullptr;
    QPlainTextEdit* m_responseEdit = nullptr;
    QPushButton* m_parseButton = nullptr;
    QLabel* m_parseSummary = nullptr;
    QTableWidget* m_reviewTable = nullptr;
    QPushButton* m_applyButton = nullptr;
};
