#pragma once

#include <QDialog>

class QPlainTextEdit;
class QShowEvent;
class SpeakingEvalPrivateNotesEditor;

class SpeakingEvalNotesDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class InitialSection
    {
        Notes,
        Comment
    };

    explicit SpeakingEvalNotesDialog(
        const QString& notes,
        const QString& comment,
        InitialSection initialSection = InitialSection::Notes,
        QWidget* parent = nullptr
        );

    [[nodiscard]] QString notes() const;

    [[nodiscard]] QString comment() const;

    [[nodiscard]] bool hasNotesChanges() const;

    [[nodiscard]] bool hasCommentChanges() const;

protected:
    void showEvent(
        QShowEvent* event
        ) override;

private:
    SpeakingEvalPrivateNotesEditor* m_notesEditor = nullptr;
    QPlainTextEdit* m_commentEdit = nullptr;
    QString m_originalNotes;
    QString m_originalComment;
    InitialSection m_initialSection = InitialSection::Notes;
    bool m_notesChanged = false;
    bool m_commentChanged = false;
    bool m_initialFocusApplied = false;
};
