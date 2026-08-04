#include "speaking_eval_private_notes_editor.h"

#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

namespace
{

struct PrivateNotes
{
    QString didWell;
    QString needsImprovement;
};

const QString& bulletPrefix()
{
    static const QString prefix =
        QStringLiteral("• ");
    return prefix;
}

QString withoutBulletPrefix(
    const QString& line
    )
{
    if (line.startsWith(bulletPrefix()))
    {
        return line.mid(bulletPrefix().size());
    }

    if (line.startsWith(QChar(0x2022)))
    {
        return line.mid(1).trimmed();
    }

    return line;
}

QString bulletListText(
    QString text
    )
{
    if (text.isEmpty())
    {
        return {};
    }

    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList lines =
        text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString& line : lines)
    {
        line =
            bulletPrefix()
            + withoutBulletPrefix(line);
    }

    return lines.join(QLatin1Char('\n'));
}

PrivateNotes splitPrivateNotes(
    const QString& notes
    )
{
    const QString didWellMarker =
        QStringLiteral("[Did Well]\n");
    const QString needsImprovementMarker =
        QStringLiteral("\n[Needs Improvement]\n");

    if (!notes.startsWith(didWellMarker))
    {
        return { notes, {} };
    }

    const qsizetype separator =
        notes.indexOf(
            needsImprovementMarker,
            didWellMarker.size()
            );
    if (separator < 0)
    {
        return { notes, {} };
    }

    return {
        notes.mid(
            didWellMarker.size(),
            separator - didWellMarker.size()
            ),
        notes.mid(separator + needsImprovementMarker.size())
    };
}

QString joinPrivateNotes(
    const QString& didWell,
    const QString& needsImprovement
    )
{
    if (didWell.isEmpty() && needsImprovement.isEmpty())
    {
        return {};
    }

    return QStringLiteral("[Did Well]\n%1\n[Needs Improvement]\n%2")
        .arg(didWell, needsImprovement);
}

} // namespace

SpeakingEvalBulletListEdit::SpeakingEvalBulletListEdit(
    QWidget* parent
    )
    : QPlainTextEdit(parent)
{
}

void SpeakingEvalBulletListEdit::keyPressEvent(
    QKeyEvent* event
    )
{
    if (
        event->key() == Qt::Key_Return
        || event->key() == Qt::Key_Enter
        )
    {
        QTextCursor cursor = textCursor();
        const QString currentLine =
            cursor.block().text();

        if (
            currentLine == bulletPrefix()
            && !cursor.hasSelection()
            )
        {
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(
                QTextCursor::Right,
                QTextCursor::KeepAnchor,
                bulletPrefix().size()
                );
            cursor.removeSelectedText();
            setTextCursor(cursor);
            return;
        }

        cursor.insertBlock();
        cursor.insertText(bulletPrefix());
        setTextCursor(cursor);
        return;
    }

    if (
        event->key() == Qt::Key_Backspace
        && !textCursor().hasSelection()
        && textCursor().block().text() == bulletPrefix()
        && textCursor().positionInBlock() == bulletPrefix().size()
        )
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(
            QTextCursor::Right,
            QTextCursor::KeepAnchor,
            bulletPrefix().size()
            );
        cursor.removeSelectedText();
        setTextCursor(cursor);
        return;
    }

    if (
        !event->text().isEmpty()
        && event->text().at(0).isPrint()
        )
    {
        ensureCurrentLineHasBullet();
    }

    QPlainTextEdit::keyPressEvent(event);
}

void SpeakingEvalBulletListEdit::inputMethodEvent(
    QInputMethodEvent* event
    )
{
    if (
        !event->commitString().isEmpty()
        || !event->preeditString().isEmpty()
        )
    {
        ensureCurrentLineHasBullet();
    }

    QPlainTextEdit::inputMethodEvent(event);
}

void SpeakingEvalBulletListEdit::insertFromMimeData(
    const QMimeData* source
    )
{
    if (!source || !source->hasText())
    {
        QPlainTextEdit::insertFromMimeData(source);
        return;
    }

    QString text = source->text();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines =
        text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    QTextCursor cursor = textCursor();
    QString insertion;
    if (cursor.block().text().isEmpty())
    {
        insertion += bulletPrefix();
    }

    for (int index = 0; index < lines.size(); ++index)
    {
        if (index > 0)
        {
            insertion +=
                QLatin1Char('\n')
                + bulletPrefix();
        }
        insertion +=
            withoutBulletPrefix(lines.at(index));
    }

    cursor.insertText(insertion);
    setTextCursor(cursor);
}

void SpeakingEvalBulletListEdit::ensureCurrentLineHasBullet()
{
    QTextCursor cursor = textCursor();
    if (!cursor.block().text().isEmpty())
    {
        return;
    }

    const QSignalBlocker blocker(this);
    cursor.insertText(bulletPrefix());
    setTextCursor(cursor);
}

SpeakingEvalPrivateNotesEditor::SpeakingEvalPrivateNotesEditor(
    QWidget* parent
    )
    : QWidget(parent)
{
    setObjectName(
        QStringLiteral("speakingEvalPrivateNotesFields")
        );

    auto* fieldsLayout =
        new QHBoxLayout(this);
    fieldsLayout->setContentsMargins(0, 0, 0, 0);
    fieldsLayout->setSpacing(12);

    auto addField =
        [this, fieldsLayout](
            const QString& labelText,
            const QString& objectName,
            const QString& placeholderText
            )
        {
            auto* field =
                new QWidget(this);
            auto* fieldLayout =
                new QVBoxLayout(field);
            fieldLayout->setContentsMargins(0, 0, 0, 0);
            fieldLayout->setSpacing(4);
            fieldLayout->addWidget(
                new QLabel(labelText, field)
                );

            auto* editor =
                new SpeakingEvalBulletListEdit(field);
            editor->setObjectName(objectName);
            editor->setPlaceholderText(placeholderText);
            editor->setTabChangesFocus(true);
            fieldLayout->addWidget(editor);
            fieldsLayout->addWidget(field, 1);
            return editor;
        };

    m_didWellEdit =
        addField(
            tr("Did Well"),
            QStringLiteral("speakingEvalDidWellNotes"),
            tr("Add positive observations…")
            );
    m_needsImprovementEdit =
        addField(
            tr("Needs Improvement"),
            QStringLiteral("speakingEvalNeedsImprovementNotes"),
            tr("Add areas for improvement…")
            );

    connect(
        m_didWellEdit,
        &QPlainTextEdit::textChanged,
        this,
        &SpeakingEvalPrivateNotesEditor::notesChanged
        );
    connect(
        m_needsImprovementEdit,
        &QPlainTextEdit::textChanged,
        this,
        &SpeakingEvalPrivateNotesEditor::notesChanged
        );
}

void SpeakingEvalPrivateNotesEditor::setNotes(
    const QString& notes
    )
{
    const PrivateNotes parts =
        splitPrivateNotes(notes);
    const QSignalBlocker didWellBlocker(m_didWellEdit);
    const QSignalBlocker needsImprovementBlocker(
        m_needsImprovementEdit
        );
    m_didWellEdit->setPlainText(
        bulletListText(parts.didWell)
        );
    m_needsImprovementEdit->setPlainText(
        bulletListText(parts.needsImprovement)
        );
}

QString SpeakingEvalPrivateNotesEditor::notes() const
{
    return joinPrivateNotes(
        m_didWellEdit
            ? m_didWellEdit->toPlainText()
            : QString(),
        m_needsImprovementEdit
            ? m_needsImprovementEdit->toPlainText()
            : QString()
        );
}

void SpeakingEvalPrivateNotesEditor::setEditorHeight(
    int height
    )
{
    if (m_didWellEdit)
    {
        m_didWellEdit->setFixedHeight(height);
    }
    if (m_needsImprovementEdit)
    {
        m_needsImprovementEdit->setFixedHeight(height);
    }
}
