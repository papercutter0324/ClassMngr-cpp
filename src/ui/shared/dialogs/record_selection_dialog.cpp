#include "record_selection_dialog.h"

#include "ui/shared/widgets/no_wheel_combobox.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

RecordSelectionDialog::RecordSelectionDialog(
    const QString& title,
    const QString& prompt,
    const QList<QPair<QString, int>>& records,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("sidebarRecordSelection"), parent)
    , m_records(new NoWheelComboBox(this))
{
    setWindowTitle(title);
    setMinimumWidth(360);

    auto* label = new QLabel(prompt, this);
    label->setWordWrap(true);
    label->setBuddy(m_records);

    m_records->setObjectName(QStringLiteral("sidebarRecordSelectionCombo"));
    m_records->setAccessibleName(prompt);
    m_records->addItem(QString(), -1);

    for (const auto& record : records)
    {
        m_records->addItem(record.first, record.second);
    }

    contentLayout()->addWidget(label);
    contentLayout()->addWidget(m_records);

    auto* buttons = addButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
        );
    auto* okButton = buttons->button(QDialogButtonBox::Ok);

    if (okButton)
    {
        okButton->setEnabled(false);
    }

    connect(
        m_records,
        &QComboBox::currentIndexChanged,
        this,
        [this, okButton]
        {
            if (okButton)
            {
                okButton->setEnabled(selectedRecordId() > 0);
            }
        }
        );
}

int RecordSelectionDialog::selectedRecordId() const
{
    return m_records->currentData().toInt();
}
