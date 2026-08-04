#include "speaking_eval_comment_edit.h"

#include "domain/models/speaking_evaluation.h"

#include <QKeyEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>

SpeakingEvalCommentEdit::SpeakingEvalCommentEdit(
    QWidget* parent
    )
    : QPlainTextEdit(parent)
{
    connect(
        document(),
        &QTextDocument::contentsChanged,
        this,
        &SpeakingEvalCommentEdit::trimToLimit
        );
}

void SpeakingEvalCommentEdit::setStudentNames(
    const QString& englishName,
    const QString& koreanName
    )
{
    m_studentName = englishName.trimmed();
    if (m_studentName.isEmpty())
    {
        m_studentName = koreanName.trimmed();
    }
}

int SpeakingEvalCommentEdit::textLength() const
{
    return toPlainText().size();
}

QString SpeakingEvalCommentEdit::cleanText() const
{
    return toPlainText()
        .trimmed()
        .left(SpeakingEval::CommentMaxLength);
}

void SpeakingEvalCommentEdit::keyPressEvent(
    QKeyEvent* event
    )
{
    const bool editingKey =
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
        !editingKey
        && !textCursor().hasSelection()
        && textLength() >= SpeakingEval::CommentMaxLength
        )
    {
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void SpeakingEvalCommentEdit::insertFromMimeData(
    const QMimeData* source
    )
{
    if (!source)
    {
        return;
    }
    if (!source->hasText())
    {
        QPlainTextEdit::insertFromMimeData(source);
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

    QString text = source->text();
    if (!m_studentName.isEmpty())
    {
        text.replace(
            QStringLiteral("STD_NAME"),
            m_studentName,
            Qt::CaseSensitive
            );
    }

    cursor.insertText(
        text.left(remaining)
        );
}

void SpeakingEvalCommentEdit::trimToLimit()
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
