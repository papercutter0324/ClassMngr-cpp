#pragma once

#include <QDialog>

class QPlainTextEdit;
class SpeakingEvalPrivateNotesEditor;

class SpeakingEvalNotesDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SpeakingEvalNotesDialog(
        const QString& notes,
        const QString& comment,
        QWidget* parent = nullptr
        );

    [[nodiscard]] QString notes() const;

    [[nodiscard]] QString comment() const;

private:
    SpeakingEvalPrivateNotesEditor* m_notesEditor = nullptr;
    QPlainTextEdit* m_commentEdit = nullptr;
};
