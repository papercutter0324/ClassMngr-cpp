#include "speaking_eval_delegate.h"

#include "core/fontmanager.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"

#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

class LimitedCommentEdit : public QPlainTextEdit
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

    int textLength() const
    {
        return toPlainText().size();
    }

    QString cleanText() const
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

        const int currentLength =
            textLength() - selectedLength;

        const int remaining =
            SpeakingEval::CommentMaxLength - currentLength;

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

        QTextCursor cursor =
            textCursor();

        const int position =
            cursor.position();

        blockSignals(true);
        setPlainText(
            text.left(SpeakingEval::CommentMaxLength)
            );
        blockSignals(false);

        cursor =
            textCursor();

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
    QLabel* label
    )
{
    if (!editor || !label)
    {
        return;
    }

    const int length =
        editor->textLength();

    label->setText(
        QObject::tr("Characters: %1/%2")
            .arg(length)
            .arg(SpeakingEval::CommentMaxLength)
        );

    if (length >= SpeakingEval::CommentMaxLength)
    {
        label->setStyleSheet(QStringLiteral("color: red;"));
    }
    else if (length >= static_cast<int>(SpeakingEval::CommentMaxLength * 0.9))
    {
        label->setStyleSheet(QStringLiteral("color: orange;"));
    }
    else
    {
        label->setStyleSheet(QString());
    }
}

} // namespace

SpeakingEvalDelegate::SpeakingEvalDelegate(
    QObject* parent
    )
    : QStyledItemDelegate(parent)
{
}

QWidget* SpeakingEvalDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    Q_UNUSED(option);

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    if (column == SpeakingEvalColumn::Comments)
    {
        return nullptr;
    }

    if (SpeakingEval::isScoringColumn(column))
    {
        auto* editor =
            new NoWheelComboBox(parent);

        editor->setEditable(true);
        editor->addItems(
            SpeakingEval::scoreValues()
            );
        editor->setInsertPolicy(QComboBox::NoInsert);

        if (auto* lineEdit = editor->lineEdit())
        {
            lineEdit->setAlignment(Qt::AlignCenter);
        }

        return editor;
    }

    QWidget* editor =
        QStyledItemDelegate::createEditor(
            parent,
            option,
            index
            );

    if (auto* lineEdit = qobject_cast<QLineEdit*>(editor))
    {
        lineEdit->setAlignment(Qt::AlignCenter);

        if (column == SpeakingEvalColumn::KoreanName)
        {
            lineEdit->setFont(
                FontManager::getKoreanFont()
                );
        }
    }

    return editor;
}

void SpeakingEvalDelegate::setEditorData(
    QWidget* editor,
    const QModelIndex& index
    ) const
{
    if (auto* comboBox = qobject_cast<QComboBox*>(editor))
    {
        comboBox->setCurrentText(
            index.data(Qt::EditRole).toString()
            );

        return;
    }

    QStyledItemDelegate::setEditorData(
        editor,
        index
        );
}

void SpeakingEvalDelegate::setModelData(
    QWidget* editor,
    QAbstractItemModel* model,
    const QModelIndex& index
    ) const
{
    QString newValue;

    if (auto* comboBox = qobject_cast<QComboBox*>(editor))
    {
        newValue =
            comboBox->currentText();
    }
    else if (auto* lineEdit = qobject_cast<QLineEdit*>(editor))
    {
        newValue =
            lineEdit->text();
    }
    else
    {
        QStyledItemDelegate::setModelData(
            editor,
            model,
            index
            );

        return;
    }

    const QString oldValue =
        index.data(Qt::EditRole).toString();

    if (oldValue == newValue)
    {
        return;
    }

    if (
        auto* table =
            qobject_cast<SpeakingEvalTableView*>(parent())
        )
    {
        table->applyChanges(
            {
                {
                    index.row(),
                    index.column(),
                    oldValue,
                    newValue
                }
            },
            tr("Edit")
            );

        return;
    }

    model->setData(
        index,
        newValue,
        Qt::EditRole
        );
}

bool SpeakingEvalDelegate::editorEvent(
    QEvent* event,
    QAbstractItemModel* model,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    )
{
    Q_UNUSED(option);

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    if (column != SpeakingEvalColumn::Comments)
    {
        return QStyledItemDelegate::editorEvent(
            event,
            model,
            option,
            index
            );
    }

    if (event->type() != QEvent::MouseButtonRelease)
    {
        return false;
    }

    auto* mouseEvent =
        static_cast<QMouseEvent*>(event);

    if (mouseEvent->button() != Qt::LeftButton)
    {
        return false;
    }

    return showCommentDialog(
        model,
        index
        );
}

void SpeakingEvalDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    if (!index.isValid())
    {
        return;
    }

    painter->save();

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    const QColor background =
        SpeakingEval::columnColor(column);

    painter->fillRect(
        option.rect,
        background
        );

    const auto* speakingModel =
        qobject_cast<const SpeakingEvalModel*>(
            index.model()
            );

    const QString key =
        QStringLiteral("%1:%2")
            .arg(index.row())
            .arg(index.column());

    const bool hasErrors =
        speakingModel
        && !speakingModel->errorsForCell(
            index.row(),
            index.column()
            ).isEmpty();

    const bool isDirty =
        speakingModel
        && speakingModel->dirtyCellKeys().contains(key);

    if (option.state & QStyle::State_Selected)
    {
        painter->fillRect(
            option.rect,
            QColor(0, 120, 215, 60)
            );
    }

    if (hasErrors)
    {
        painter->fillRect(
            option.rect,
            QColor(255, 0, 0, 80)
            );
    }
    else if (isDirty)
    {
        painter->fillRect(
            option.rect,
            QColor(255, 215, 0, 50)
            );
    }

    painter->setFont(
        column == SpeakingEvalColumn::KoreanName
            ? FontManager::getKoreanFont()
            : FontManager::getUiFont(12)
        );

    QColor textColor =
        SpeakingEval::contrastTextColor(background);

    if (hasErrors)
    {
        textColor =
            SpeakingEval::contrastTextColor(
                QColor(255, 0, 0)
                );
    }

    painter->setPen(textColor);

    const QString text =
        index.data(Qt::DisplayRole).toString();

    const QFontMetrics metrics(
        painter->font()
        );

    const QString elidedText =
        metrics.elidedText(
            text,
            Qt::ElideRight,
            option.rect.width() - 8
            );

    painter->drawText(
        option.rect.adjusted(4, 0, -4, 0),
        Qt::AlignCenter,
        elidedText
        );

    if (SpeakingEval::hasThickBorderAfter(column))
    {
        QPen pen(Qt::black);
        pen.setWidth(2);
        pen.setCosmetic(true);

        painter->setPen(pen);
        painter->drawLine(
            option.rect.right() - 1,
            option.rect.top(),
            option.rect.right() - 1,
            option.rect.bottom()
            );
    }

    QPen rowPen(Qt::black);
    rowPen.setWidth(1);
    rowPen.setStyle(Qt::DotLine);
    rowPen.setCosmetic(true);

    painter->setPen(rowPen);
    painter->drawLine(
        option.rect.left(),
        option.rect.bottom() - 1,
        option.rect.right(),
        option.rect.bottom() - 1
        );

    painter->restore();
}

QSize SpeakingEvalDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    QSize size =
        QStyledItemDelegate::sizeHint(
            option,
            index
            );

    size.setHeight(
        SpeakingEval::RowHeight
        );

    return size;
}

bool SpeakingEvalDelegate::showCommentDialog(
    QAbstractItemModel* model,
    const QModelIndex& index
    ) const
{
    if (!model || !index.isValid())
    {
        return false;
    }

    QDialog dialog(
        qobject_cast<QWidget*>(parent())
        );

    dialog.setWindowTitle(
        tr("Enter Comment")
        );

    dialog.setMinimumSize(
        QSize(400, 300)
        );

    auto* layout =
        new QVBoxLayout(&dialog);

    auto* editor =
        new LimitedCommentEdit(&dialog);

    editor->setPlainText(
        index.data(Qt::EditRole).toString()
        );

    auto* counter =
        new QLabel(&dialog);

    auto* buttonLayout =
        new QHBoxLayout;

    auto* clearButton =
        new QPushButton(
            tr("Clear"),
            &dialog
            );

    auto* okButton =
        new QPushButton(
            tr("OK"),
            &dialog
            );

    auto* cancelButton =
        new QPushButton(
            tr("Cancel"),
            &dialog
            );

    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    layout->addWidget(editor);
    layout->addWidget(counter);
    layout->addLayout(buttonLayout);

    const auto syncCounter =
        [editor, counter, clearButton]()
        {
            updateCounter(
                editor,
                counter
                );

            clearButton->setEnabled(
                editor->textLength() > 0
                );
        };

    connect(
        editor->document(),
        &QTextDocument::contentsChanged,
        &dialog,
        syncCounter
        );

    connect(
        clearButton,
        &QPushButton::clicked,
        &dialog,
        [editor, &dialog]()
        {
            if (!editor->toPlainText().isEmpty())
            {
                const auto response =
                    QMessageBox::question(
                        &dialog,
                        QObject::tr("Clear text?"),
                        QObject::tr("Are you sure you want to clear the comment?")
                        );

                if (response != QMessageBox::Yes)
                {
                    return;
                }
            }

            editor->clear();
        }
        );

    connect(
        okButton,
        &QPushButton::clicked,
        &dialog,
        &QDialog::accept
        );

    connect(
        cancelButton,
        &QPushButton::clicked,
        &dialog,
        &QDialog::reject
        );

    syncCounter();

    if (dialog.exec() != QDialog::Accepted)
    {
        return true;
    }

    const QString oldValue =
        index.data(Qt::EditRole).toString();

    const QString newValue =
        editor->cleanText();

    if (oldValue == newValue)
    {
        return true;
    }

    if (
        auto* table =
            qobject_cast<SpeakingEvalTableView*>(parent())
        )
    {
        table->applyChanges(
            {
                {
                    index.row(),
                    index.column(),
                    oldValue,
                    newValue
                }
            },
            tr("Edit Comment")
            );
    }
    else
    {
        model->setData(
            index,
            newValue,
            Qt::EditRole
            );
    }

    return true;
}
