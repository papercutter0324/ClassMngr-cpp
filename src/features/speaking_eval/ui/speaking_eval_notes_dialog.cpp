#include "speaking_eval_notes_dialog.h"

#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_private_notes_editor.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

class LimitedCommentEdit final : public QPlainTextEdit
{
public:
    explicit LimitedCommentEdit(
        QWidget* parent = nullptr
        )
        : QPlainTextEdit(parent)
    {
        connect(
            document(),
            &QTextDocument::contentsChanged,
            this,
            [this]()
            {
                trimToLimit();
            }
            );
    }

    [[nodiscard]] int textLength() const
    {
        return toPlainText().size();
    }

    [[nodiscard]] QString cleanText() const
    {
        return toPlainText()
            .trimmed()
            .left(SpeakingEval::CommentMaxLength);
    }

protected:
    void keyPressEvent(
        QKeyEvent* event
        ) override
    {
        const bool controlKey =
            event->matches(QKeySequence::Copy)
            || event->matches(QKeySequence::Cut)
            || event->matches(QKeySequence::Paste)
            || event->matches(QKeySequence::Undo)
            || event->matches(QKeySequence::Redo)
            || event->key() == Qt::Key_Backspace
            || event->key() == Qt::Key_Delete
            || event->key() == Qt::Key_Left
            || event->key() == Qt::Key_Right
            || event->key() == Qt::Key_Up
            || event->key() == Qt::Key_Down
            || event->key() == Qt::Key_Home
            || event->key() == Qt::Key_End;

        if (
            !controlKey
            && !textCursor().hasSelection()
            && textLength() >= SpeakingEval::CommentMaxLength
            )
        {
            return;
        }

        QPlainTextEdit::keyPressEvent(event);
    }

    void insertFromMimeData(
        const QMimeData* source
        ) override
    {
        if (!source)
        {
            return;
        }

        QTextCursor cursor =
            textCursor();
        const int selectedLength =
            cursor.hasSelection()
                ? cursor.selectedText().size()
                : 0;
        const int remaining =
            SpeakingEval::CommentMaxLength
            - (textLength() - selectedLength);
        if (remaining <= 0)
        {
            return;
        }

        cursor.insertText(
            source->text().left(remaining)
            );
    }

private:
    void trimToLimit()
    {
        const QString text =
            toPlainText();
        if (text.size() <= SpeakingEval::CommentMaxLength)
        {
            return;
        }

        const int position =
            qMax(0, textCursor().position());
        blockSignals(true);
        setPlainText(
            text.left(SpeakingEval::CommentMaxLength)
            );
        blockSignals(false);

        QTextCursor cursor(document());
        cursor.setPosition(
            std::min(
                position,
                SpeakingEval::CommentMaxLength
                )
            );
        setTextCursor(cursor);
    }
};

void updateCounter(
    LimitedCommentEdit* editor,
    QLabel* counter
    )
{
    if (!editor || !counter)
    {
        return;
    }

    const int length =
        editor->textLength();
    counter->setText(
        QObject::tr("Characters: %1/%2")
            .arg(length)
            .arg(SpeakingEval::CommentMaxLength)
        );

    if (length >= SpeakingEval::CommentMaxLength)
    {
        counter->setStyleSheet(
            QStringLiteral("color: red;")
            );
    }
    else if (
        length
        >= static_cast<int>(
            SpeakingEval::CommentMaxLength * 0.9
            )
        )
    {
        counter->setStyleSheet(
            QStringLiteral("color: orange;")
            );
    }
    else
    {
        counter->setStyleSheet({});
    }
}

} // namespace

SpeakingEvalNotesDialog::SpeakingEvalNotesDialog(
    const QString& notes,
    const QString& comment,
    InitialSection initialSection,
    QWidget* parent
    )
    : QDialog(parent)
    , m_originalNotes(notes)
    , m_originalComment(comment)
    , m_initialSection(initialSection)
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
    connect(
        m_notesEditor,
        &SpeakingEvalPrivateNotesEditor::notesChanged,
        this,
        [this]()
        {
            m_notesChanged = true;
        }
        );

    layout->addWidget(
        new QLabel(
            tr("Comment"),
            this
            )
        );

    m_commentEdit =
        new LimitedCommentEdit(this);
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
    counter->setObjectName(
        QStringLiteral("speakingEvalNotesDialogCommentCounter")
        );
    layout->addWidget(counter);

    auto* clearCommentButton =
        new TextFitPushButton(
            tr("Clear Comment"),
            this
            );
    clearCommentButton->setObjectName(
        QStringLiteral("speakingEvalNotesDialogClearComment")
        );

    const auto syncCommentUi =
        [this, counter, clearCommentButton]()
        {
            auto* editor =
                static_cast<LimitedCommentEdit*>(
                    m_commentEdit
                    );
            updateCounter(editor, counter);
            clearCommentButton->setEnabled(
                editor && editor->textLength() > 0
                );
        };

    connect(
        m_commentEdit,
        &QPlainTextEdit::textChanged,
        this,
        [this, syncCommentUi]()
        {
            m_commentChanged = true;
            syncCommentUi();
        }
        );
    connect(
        clearCommentButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (m_commentEdit->toPlainText().isEmpty())
            {
                return;
            }

            QMessageBox confirmation(
                QMessageBox::Question,
                tr("Clear text?"),
                tr("Are you sure you want to clear the comment?"),
                QMessageBox::Yes | QMessageBox::No,
                this
                );
            confirmation.setDefaultButton(QMessageBox::No);
            if (
                confirmation.exec()
                == QMessageBox::Yes
                )
            {
                m_commentEdit->clear();
            }
        }
        );
    syncCommentUi();

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

    auto* buttonLayout =
        new QHBoxLayout;
    buttonLayout->addWidget(clearCommentButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(buttons);
    layout->addLayout(buttonLayout);
}

QString SpeakingEvalNotesDialog::notes() const
{
    return m_notesChanged && m_notesEditor
        ? m_notesEditor->notes()
        : m_originalNotes;
}

QString SpeakingEvalNotesDialog::comment() const
{
    if (!m_commentChanged)
    {
        return m_originalComment;
    }

    const auto* editor =
        static_cast<const LimitedCommentEdit*>(
            m_commentEdit
            );
    return editor
        ? editor->cleanText()
        : m_originalComment;
}

bool SpeakingEvalNotesDialog::hasNotesChanges() const
{
    return m_notesChanged
        && notes() != m_originalNotes;
}

bool SpeakingEvalNotesDialog::hasCommentChanges() const
{
    return m_commentChanged
        && comment() != m_originalComment;
}

void SpeakingEvalNotesDialog::showEvent(
    QShowEvent* event
    )
{
    QDialog::showEvent(event);

    if (m_initialFocusApplied)
    {
        return;
    }

    m_initialFocusApplied = true;
    if (
        m_initialSection == InitialSection::Comment
        && m_commentEdit
        )
    {
        m_commentEdit->setFocus(Qt::OtherFocusReason);
        m_commentEdit->moveCursor(QTextCursor::End);
    }
    else if (m_notesEditor)
    {
        m_notesEditor->focusDidWell();
    }
}
