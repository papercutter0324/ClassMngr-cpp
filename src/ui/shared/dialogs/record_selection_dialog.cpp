#include "record_selection_dialog.h"

#include "ui/shared/widgets/no_wheel_combobox.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <limits>

namespace
{
QList<QPair<QString, qint64>> wideRecords(
    const QList<QPair<QString, int>>& records)
{
    QList<QPair<QString, qint64>> result;
    for (const auto& record : records)
    {
        result.append({record.first, record.second});
    }
    return result;
}
}

RecordSelectionDialog::RecordSelectionDialog(
    const QString& title,
    const QString& prompt,
    const QList<QPair<QString, int>>& records,
    QWidget* parent
    )
    : RecordSelectionDialog(title, prompt, wideRecords(records), parent)
{
}

RecordSelectionDialog::RecordSelectionDialog(
    const QString& title,
    const QString& prompt,
    const QList<QPair<QString, qint64>>& records,
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
                okButton->setEnabled(selectedWideRecordId() > 0);
            }
        }
        );
}

int RecordSelectionDialog::selectedRecordId() const
{
    const qint64 id = selectedWideRecordId();
    return id >= std::numeric_limits<int>::min()
        && id <= std::numeric_limits<int>::max()
        ? static_cast<int>(id) : -1;
}

qint64 RecordSelectionDialog::selectedWideRecordId() const
{
    return m_records->currentData().toLongLong();
}
