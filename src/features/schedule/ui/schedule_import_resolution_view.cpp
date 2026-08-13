#include "schedule_import_resolution_view.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace ScheduleImportResolutionView
{
void addResolutionItem(
    QComboBox* combo,
    const QString& text,
    int action,
    int target
    )
{
    combo->addItem(text);
    const int index = combo->count() - 1;
    combo->setItemData(index, action, ActionRole);
    combo->setItemData(index, target, TargetRole);
}

void configureCompactActionCombo(QComboBox* combo)
{
    if (!combo)
    {
        return;
    }
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon
        );
    combo->setMinimumContentsLength(14);
}

QFrame* createReconciliationCard(
    QWidget* parent,
    const QString& objectName
    )
{
    auto* card = new QFrame(parent);
    card->setObjectName(objectName);
    card->setFrameShape(QFrame::StyledPanel);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    return card;
}

void addLabeledControlRow(
    QVBoxLayout* layout,
    const QString& labelText,
    QWidget* control,
    QWidget* parent
    )
{
    auto* row = new QHBoxLayout();
    auto* label = new QLabel(labelText, parent);
    label->setMinimumWidth(label->sizeHint().width());
    row->addWidget(label);
    row->addWidget(control, 1);
    layout->addLayout(row);
}

void clearLayout(QLayout* layout)
{
    if (!layout)
    {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            delete widget;
        }
        else if (QLayout* childLayout = item->layout())
        {
            clearLayout(childLayout);
            delete childLayout;
        }
        delete item;
    }
}

int findActionIndex(QComboBox* combo, int action, int target)
{
    for (int index = 0; index < combo->count(); ++index)
    {
        if (combo->itemData(index, ActionRole).toInt() != action)
        {
            continue;
        }
        if (
            target == -2
            || combo->itemData(index, TargetRole).toInt() == target
            )
        {
            return index;
        }
    }
    return -1;
}
}
