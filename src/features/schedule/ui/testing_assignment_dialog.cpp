#include "testing_assignment_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "data/data_service.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
constexpr int PlainTestingMode = 0;
constexpr int TestingClassMode = 1;
constexpr int EssayMode = 2;
}

TestingAssignmentDialog::TestingAssignmentDialog(
    DataService* dataService,
    const TestingAssignment* existingAssignment,
    QWidget* parent
    )
    : QDialog(parent)
    , m_dataService(dataService)
    , m_hasExistingAssignment(existingAssignment != nullptr)
{
    if (existingAssignment)
    {
        m_existingAssignment = *existingAssignment;
    }

    setWindowTitle(
        m_hasExistingAssignment
            ? tr("Edit Testing Assignment")
            : tr("Add Testing Assignment")
        );
    setModal(true);
    setMinimumWidth(420);
    buildUi();
    loadTestingClasses();

    if (
        m_hasExistingAssignment
        && m_existingAssignment.kind
            == TestingAssignmentKind::SpecialClass
        )
    {
        m_modeCombo->setCurrentIndex(TestingClassMode);
        const int index =
            m_classCombo->findData(
                m_existingAssignment.classId
                );
        if (index >= 0)
        {
            m_classCombo->setCurrentIndex(index);
        }
    }
    else
    {
        m_modeCombo->setCurrentIndex(PlainTestingMode);
        m_roomEdit->setText(m_existingAssignment.room);
        m_roomEdit->selectAll();
    }

    updateModeUi();
    adjustSize();
    setFixedSize(size());
}

TestingAssignmentDialog::Action
TestingAssignmentDialog::selectedAction() const
{
    return m_action;
}

QString TestingAssignmentDialog::room() const
{
    return m_roomEdit
        ? m_roomEdit->text().trimmed()
        : QString();
}

int TestingAssignmentDialog::selectedClassId() const
{
    return m_classCombo
        ? m_classCombo->currentData().toInt()
        : -1;
}

void TestingAssignmentDialog::buildUi()
{
    auto* layout =
        new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignTop);

    auto* modeLabel =
        new QLabel(tr("Assignment Type"), this);
    layout->addWidget(modeLabel);

    m_modeCombo =
        new QComboBox(this);
    m_modeCombo->setObjectName(
        QStringLiteral("testingAssignmentModeCombo")
        );
    m_modeCombo->addItem(
        tr("Oral Testing Block"),
        PlainTestingMode
        );
    m_modeCombo->addItem(
        tr("Testing Class"),
        TestingClassMode
        );
    m_modeCombo->addItem(
        tr("Essay Block"),
        EssayMode
        );
    layout->addWidget(m_modeCombo);

    m_roomLabel =
        new QLabel(tr("Room"), this);
    layout->addWidget(m_roomLabel);

    m_roomEdit =
        new QLineEdit(this);
    m_roomEdit->setObjectName(
        QStringLiteral("testingAssignmentRoomEdit")
        );
    m_roomEdit->setPlaceholderText(
        tr("Optional for an Oral Testing block")
        );
    layout->addWidget(m_roomEdit);

    m_classSection =
        new QWidget(this);
    auto* classSectionLayout =
        new QVBoxLayout(m_classSection);
    classSectionLayout->setContentsMargins(0, 8, 0, 8);
    classSectionLayout->setSpacing(12);

    m_classLabel =
        new QLabel(tr("Testing Class"), m_classSection);
    classSectionLayout->addWidget(m_classLabel);

    m_classCombo =
        new QComboBox(m_classSection);
    m_classCombo->setObjectName(
        QStringLiteral("testingAssignmentClassCombo")
        );
    classSectionLayout->addWidget(m_classCombo);
    layout->addWidget(m_classSection);

    auto* footerRow =
        new QWidget(this);
    auto* footerLayout =
        new QHBoxLayout(footerRow);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(8);

    m_manageClassesButton =
        new TextFitPushButton(
            tr("Manage Classes"),
            footerRow
            );
    m_manageClassesButton->setObjectName(
        QStringLiteral("testingAssignmentManageClassesButton")
        );
    footerLayout->addWidget(m_manageClassesButton);
    footerLayout->addStretch(1);

    auto* footer =
        new TextFitDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel,
            footerRow
            );
    footer->button(QDialogButtonBox::Save)->setText(tr("Save"));
    footer->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

    footerLayout->addWidget(footer);
    layout->addWidget(footerRow);

    connect(
        m_modeCombo,
        &QComboBox::currentIndexChanged,
        this,
        &TestingAssignmentDialog::updateModeUi
        );
    connect(
        m_manageClassesButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            m_action = Action::ManageTestingClasses;
            accept();
        }
        );
    connect(
        footer,
        &QDialogButtonBox::accepted,
        this,
        &TestingAssignmentDialog::acceptSave
        );
    connect(
        footer,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );
}

void TestingAssignmentDialog::loadTestingClasses()
{
    m_classCombo->clear();

    if (!m_dataService || !m_dataService->isOpen())
    {
        return;
    }

    const Result<QList<TestingClass>> testingClasses =
        m_dataService->loadTestingClasses();
    if (!testingClasses)
    {
        DialogServices::showWarning(
            this,
            tr("Testing Classes"),
            testingClasses.error()
            );
        return;
    }

    for (const TestingClass& testingClass : *testingClasses)
    {
        m_classCombo->addItem(
            tr("%1 — %2 %3 — Room %4")
                .arg(
                    testingClass.name,
                    testingClass.grade,
                    testingClass.level,
                    testingClass.room
                    ),
            testingClass.classId
            );
    }
}

void TestingAssignmentDialog::updateModeUi()
{
    const bool testingClassMode =
        m_modeCombo->currentIndex() == TestingClassMode;
    const bool plainTestingMode =
        m_modeCombo->currentIndex() == PlainTestingMode;
    m_roomLabel->setVisible(plainTestingMode);
    m_roomEdit->setVisible(plainTestingMode);
    m_classSection->setVisible(testingClassMode);
    m_manageClassesButton->setVisible(testingClassMode);
}

void TestingAssignmentDialog::acceptSave()
{
    const int mode =
        m_modeCombo->currentIndex();

    if (mode == TestingClassMode)
    {
        if (selectedClassId() <= 0)
        {
            DialogServices::showWarning(
                this,
                tr("Testing Assignment"),
                tr("Choose a testing class or manage your classes.")
                );
            return;
        }
        m_action = Action::AssignTestingClass;
    }
    else if (mode == EssayMode)
    {
        m_action = Action::RemoveAssignment;
    }
    else
    {
        m_action = Action::SavePlainTesting;
    }

    accept();
}
