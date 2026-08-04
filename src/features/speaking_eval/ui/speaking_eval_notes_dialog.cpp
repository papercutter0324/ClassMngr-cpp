#include "speaking_eval_notes_dialog.h"

#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_private_notes_editor.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QVBoxLayout>

SpeakingEvalNotesDialog::SpeakingEvalNotesDialog(
    const QString& notes,
    const QString& comment,
    QWidget* parent
    )
    : QDialog(parent)
{
    setObjectName(
        QStringLiteral("speakingEvalNotesDialog")
        );
    setWindowTitle(
        tr("Student Notes and Comment")
        );
    resize(780, 600);

    auto* layout =
        new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    layout->addWidget(
        new QLabel(
            tr("Private Notes (not included in the report)"),
            this
            )
        );

    m_notesEditor =
        new SpeakingEvalPrivateNotesEditor(this);
    m_notesEditor->setEditorHeight(144);
    m_notesEditor->setNotes(notes);
    layout->addWidget(m_notesEditor);

    layout->addWidget(
        new QLabel(
            tr("Comment"),
            this
            )
        );

    m_commentEdit =
        new QPlainTextEdit(this);
    m_commentEdit->setObjectName(
        QStringLiteral("speakingEvalNotesDialogComment")
        );
    m_commentEdit->setPlaceholderText(
        tr("Enter the student's report comment…")
        );
    m_commentEdit->setPlainText(comment);
    m_commentEdit->setMinimumHeight(170);
    m_commentEdit->setTabChangesFocus(true);
    layout->addWidget(m_commentEdit, 1);

    auto* counter =
        new QLabel(this);
    layout->addWidget(counter);

    const auto updateCounter =
        [this, counter]()
        {
            QString text =
                m_commentEdit->toPlainText();
            if (text.size() > SpeakingEval::CommentMaxLength)
            {
                const int cursorPosition =
                    m_commentEdit->textCursor().position();
                text.truncate(SpeakingEval::CommentMaxLength);
                m_commentEdit->setPlainText(text);
                QTextCursor cursor =
                    m_commentEdit->textCursor();
                cursor.setPosition(
                    qMin(cursorPosition, text.size())
                    );
                m_commentEdit->setTextCursor(cursor);
            }

            counter->setText(
                tr("Characters: %1/%2")
                    .arg(text.size())
                    .arg(SpeakingEval::CommentMaxLength)
                );
        };

    connect(
        m_commentEdit->document(),
        &QTextDocument::contentsChanged,
        this,
        updateCounter
        );
    updateCounter();

    auto* buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            this
            );
    connect(
        buttons,
        &QDialogButtonBox::accepted,
        this,
        &QDialog::accept
        );
    connect(
        buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );
    layout->addWidget(buttons);
}

QString SpeakingEvalNotesDialog::notes() const
{
    return m_notesEditor
        ? m_notesEditor->notes()
        : QString();
}

QString SpeakingEvalNotesDialog::comment() const
{
    return m_commentEdit
        ? m_commentEdit
              ->toPlainText()
              .trimmed()
              .left(SpeakingEval::CommentMaxLength)
        : QString();
}
