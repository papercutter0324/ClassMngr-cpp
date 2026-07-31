#include "testing_block_dialog.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

TestingBlockDialog::TestingBlockDialog(
    const QString& room,
    bool existingBlock,
    QWidget* parent
    )
    : QDialog(parent)
{
    setWindowTitle(
        existingBlock
            ? tr("Edit Testing Block")
            : tr("Add Testing Block")
        );
    setModal(true);
    setMinimumWidth(380);

    buildUi(existingBlock);
    m_roomEdit->setText(room);
    m_roomEdit->selectAll();
}

QString TestingBlockDialog::room() const
{
    return m_roomEdit
        ? m_roomEdit->text().trimmed()
        : QString();
}

bool TestingBlockDialog::removeRequested() const
{
    return m_removeRequested;
}

void TestingBlockDialog::buildUi(
    bool existingBlock
    )
{
    auto* layout =
        new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* description =
        new QLabel(
            tr("Enter an optional room number or name for this testing assignment."),
            this
            );
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* roomLabel =
        new QLabel(
            tr("Room"),
            this
            );
    layout->addWidget(roomLabel);

    m_roomEdit =
        new QLineEdit(this);
    m_roomEdit->setObjectName(
        QStringLiteral("testingBlockRoomEdit")
        );
    m_roomEdit->setPlaceholderText(
        tr("For example: 402 or Library")
        );
    layout->addWidget(m_roomEdit);

    auto* footer =
        new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel,
            this
            );
    footer->button(QDialogButtonBox::Save)->setText(tr("Save"));
    footer->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

    if (existingBlock)
    {
        auto* removeButton =
            new TextFitPushButton(
                tr("Change to Essay"),
                this
                );
        removeButton->setObjectName(
            QStringLiteral("testingBlockRemoveButton")
            );
        footer->addButton(
            removeButton,
            QDialogButtonBox::DestructiveRole
            );

        connect(
            removeButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                m_removeRequested = true;
                accept();
            }
            );
    }

    layout->addWidget(footer);

    connect(
        footer,
        &QDialogButtonBox::accepted,
        this,
        &QDialog::accept
        );
    connect(
        footer,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );
}
