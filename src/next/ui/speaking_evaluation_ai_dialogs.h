#pragma once

#include "next/application/ai_comment_voice_preferences.h"
#include "next/application/speaking_evaluation_ai_batch_accepted_comment_plan.h"
#include "next/application/speaking_evaluation_ai_batch_eligibility.h"
#include "next/domain/course.h"

#include <QAbstractTableModel>
#include <QDialog>
#include <QString>
#include <QUrl>

#include <functional>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableView;

namespace ClassMngr::Next::Ui
{

struct SpeakingEvaluationAiBatchStudent final
{
    int sourceRow = -1;
    QString id;
    QString englishName;
    QString koreanName;
    QString oldComment;
    QString didWell;
    QString needsImprovement;
    Domain::CourseGradeBand grade = Domain::CourseGradeBand::Other;
    Application::SpeakingEvaluationAiBatchEligibilityReason eligibilityReason =
        Application::SpeakingEvaluationAiBatchEligibilityReason::MissingName;
};

struct SpeakingEvaluationAiBatchAcceptedComment final
{
    int sourceRow = -1;
    QString oldComment;
    QString newComment;
};

class SpeakingEvaluationAiBatchSelectionModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        IncludeColumn,
        StudentColumn,
        StatusColumn,
        ColumnCount
    };

    enum Role
    {
        SourceRowRole = Qt::UserRole + 1,
        StudentIdRole
    };

    explicit SpeakingEvaluationAiBatchSelectionModel(
        std::vector<SpeakingEvaluationAiBatchStudent> students,
        QObject* parent = nullptr
        );

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole
        ) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
        ) const override;

    [[nodiscard]] bool isEligible(int row) const noexcept;
    [[nodiscard]] bool isSelected(int row) const noexcept;
    [[nodiscard]] const SpeakingEvaluationAiBatchStudent* studentAt(
        int row
        ) const noexcept;
    [[nodiscard]] std::vector<int> selectedRows() const;

private:
    std::vector<SpeakingEvaluationAiBatchStudent> m_students;
    std::vector<bool> m_selected;
};

class SpeakingEvaluationAiBatchReviewModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        ApplyColumn,
        StudentColumn,
        StatusColumn,
        CharactersColumn,
        CommentColumn,
        ColumnCount
    };

    enum Role
    {
        ValidRole = Qt::UserRole + 1,
        SourceRowRole
    };

    struct Entry final
    {
        int sourceRow = -1;
        int reportIndex = -1;
        QString studentName;
        QString oldComment;
        QString comment;
        QString responseError;
        QString responseStatus;
        bool hadNamePlaceholder = false;
        bool checked = false;
        bool valid = false;
    };

    explicit SpeakingEvaluationAiBatchReviewModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole
        ) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
        ) const override;

    void setEntries(std::vector<Entry> entries);
    void clear();
    [[nodiscard]] bool hasCheckedValidComment() const noexcept;
    [[nodiscard]] std::vector<
        Application::SpeakingEvaluationAiBatchAcceptedCommentCandidate>
    acceptedCandidates() const;
    [[nodiscard]] const Entry* entryAt(int row) const noexcept;

private:
    void updateCommentQuality(int row);

    std::vector<Entry> m_entries;
};

class SpeakingEvaluationAiPromptDialog final : public QDialog
{
public:
    explicit SpeakingEvaluationAiPromptDialog(
        QString prompt,
        QString providerName,
        QUrl providerUrl,
        std::function<bool(const QUrl&)> openExternalUrl = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] QPlainTextEdit* promptEditor() const noexcept;
    [[nodiscard]] QPushButton* copyButton() const noexcept;
    [[nodiscard]] QPushButton* copyAndOpenButton() const noexcept;

private:
    void copyPrompt(bool openProvider);

    QString m_prompt;
    QUrl m_providerUrl;
    std::function<bool(const QUrl&)> m_openExternalUrl;
    QPlainTextEdit* m_promptEditor = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_copyAndOpenButton = nullptr;
};

class SpeakingEvaluationAiBatchDialog final : public QDialog
{
public:
    explicit SpeakingEvaluationAiBatchDialog(
        std::vector<SpeakingEvaluationAiBatchStudent> students,
        Application::AiCommentVoice voice,
        QString providerName,
        QUrl providerUrl,
        std::function<bool(const QUrl&)> openExternalUrl = {},
        QWidget* parent = nullptr
        );

    [[nodiscard]] SpeakingEvaluationAiBatchSelectionModel* selectionModel()
        const noexcept;
    [[nodiscard]] SpeakingEvaluationAiBatchReviewModel* reviewModel()
        const noexcept;
    [[nodiscard]] QPlainTextEdit* promptEditor() const noexcept;
    [[nodiscard]] QPlainTextEdit* responseEditor() const noexcept;
    [[nodiscard]] QLabel* parseSummaryLabel() const noexcept;
    [[nodiscard]] QPushButton* createPromptButton() const noexcept;
    [[nodiscard]] QPushButton* parseButton() const noexcept;
    [[nodiscard]] QPushButton* applyButton() const noexcept;
    [[nodiscard]] const std::vector<SpeakingEvaluationAiBatchAcceptedComment>&
    acceptedComments() const noexcept;

private:
    void resetGeneratedContent();
    void updateActions();
    void createPrompt();
    void copyPrompt(bool openProvider);
    void parseResponse();
    void applyComments();

    std::vector<SpeakingEvaluationAiBatchStudent> m_students;
    std::vector<int> m_selectedRows;
    std::vector<SpeakingEvaluationAiBatchAcceptedComment> m_acceptedComments;
    Application::AiCommentVoice m_voice = Application::AiCommentVoice::DirectToStudent;
    QUrl m_providerUrl;
    std::function<bool(const QUrl&)> m_openExternalUrl;
    bool m_inputWithinBatchBound = true;
    SpeakingEvaluationAiBatchSelectionModel* m_selectionModel = nullptr;
    SpeakingEvaluationAiBatchReviewModel* m_reviewModel = nullptr;
    QTableView* m_selectionTable = nullptr;
    QPlainTextEdit* m_promptEditor = nullptr;
    QPlainTextEdit* m_responseEditor = nullptr;
    QLabel* m_batchStatus = nullptr;
    QLabel* m_parseSummary = nullptr;
    QPushButton* m_createPromptButton = nullptr;
    QPushButton* m_copyPromptButton = nullptr;
    QPushButton* m_copyAndOpenButton = nullptr;
    QPushButton* m_parseButton = nullptr;
    QPushButton* m_applyButton = nullptr;
};

} // namespace ClassMngr::Next::Ui
