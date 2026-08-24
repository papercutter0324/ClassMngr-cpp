#include "speaking_eval_delegate.h"

#include "core/fontmanager.h"
#include "core/utils/student_name_utils.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_notes_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"

#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

SpeakingEvalDelegate::SpeakingEvalDelegate(
    QObject* parent,
    bool readOnly
    )
    : QStyledItemDelegate(parent)
    , m_readOnly(readOnly)
{
}

QWidget* SpeakingEvalDelegate::createEditor(
    QWidget* parent,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
    ) const
{
    Q_UNUSED(option);

    if (m_readOnly)
    {
        return nullptr;
    }

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    if (
        column == SpeakingEvalColumn::Comments
        || column == SpeakingEvalColumn::Notes
        )
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

    if (m_readOnly)
    {
        return false;
    }

    const auto column =
        SpeakingEval::columnFromInt(
            index.column()
            );

    if (
        column != SpeakingEvalColumn::Comments
        && column != SpeakingEvalColumn::Notes
        )
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

    return showNotesDialog(
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

    const auto koreanNameIssues =
        column == SpeakingEvalColumn::KoreanName
            ? StudentNameUtils::validateKoreanName(
                index.data(Qt::DisplayRole).toString()
                )
            : QList<StudentNameUtils::ValidationIssue>();
    const bool hasKoreanNameCaution = koreanNameIssues.contains(
        StudentNameUtils::ValidationIssue::KoreanUnusualLength
        )
        && !koreanNameIssues.contains(
            StudentNameUtils::ValidationIssue::KoreanContainsInvalidCharacters
            );

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

    if (hasKoreanNameCaution)
    {
        painter->fillRect(
            option.rect,
            QColor(245, 158, 11, 90)
            );
    }
    else if (hasErrors)
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

    if (hasKoreanNameCaution)
    {
        textColor = SpeakingEval::contrastTextColor(
            QColor(245, 158, 11)
            );
    }
    else if (hasErrors)
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

bool SpeakingEvalDelegate::showNotesDialog(
    QAbstractItemModel* model,
    const QModelIndex& index
    ) const
{
    if (!model || !index.isValid())
    {
        return false;
    }

    const QModelIndex notesIndex =
        model->index(
            index.row(),
            SpeakingEval::toInt(SpeakingEvalColumn::Notes)
            );
    const QModelIndex commentsIndex =
        model->index(
            index.row(),
            SpeakingEval::toInt(SpeakingEvalColumn::Comments)
            );
    const QModelIndex englishNameIndex =
        model->index(
            index.row(),
            SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
            );
    const QModelIndex koreanNameIndex =
        model->index(
            index.row(),
            SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
            );
    if (
        !notesIndex.isValid()
        || !commentsIndex.isValid()
        || !englishNameIndex.isValid()
        || !koreanNameIndex.isValid()
        )
    {
        return false;
    }

    SpeakingEvalNotesDialog dialog(
        notesIndex.data(Qt::EditRole).toString(),
        commentsIndex.data(Qt::EditRole).toString(),
        index.column()
                == SpeakingEval::toInt(SpeakingEvalColumn::Comments)
            ? SpeakingEvalNotesDialog::InitialSection::Comment
            : SpeakingEvalNotesDialog::InitialSection::Notes,
        qobject_cast<QWidget*>(parent()),
        englishNameIndex.data(Qt::EditRole).toString(),
        koreanNameIndex.data(Qt::EditRole).toString()
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return true;
    }

    QList<SpeakingEvalCellEdit> changes;

    const auto addChange =
        [&changes](
            const QModelIndex& changedIndex,
            const QString& newValue
            )
        {
            const QString oldValue =
                changedIndex.data(Qt::EditRole).toString();
            if (oldValue == newValue)
            {
                return;
            }

            changes.append(
                {
                    changedIndex.row(),
                    changedIndex.column(),
                    oldValue,
                    newValue
                }
                );
        };

    if (dialog.hasNotesChanges())
    {
        addChange(notesIndex, dialog.notes());
    }
    if (dialog.hasCommentChanges())
    {
        addChange(commentsIndex, dialog.comment());
    }

    if (changes.isEmpty())
    {
        return true;
    }

    if (
        auto* table =
            qobject_cast<SpeakingEvalTableView*>(parent())
        )
    {
        table->applyChanges(
            changes,
            tr("Edit Notes and Comment")
            );
    }
    else
    {
        for (const SpeakingEvalCellEdit& change : changes)
        {
            model->setData(
                model->index(change.row, change.column),
                change.newValue,
                Qt::EditRole
                );
        }
    }

    return true;
}
