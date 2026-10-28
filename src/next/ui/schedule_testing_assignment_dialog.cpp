#include "next/ui/schedule_testing_assignment_dialog.h"

#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <string>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int PlainMode = 0;
constexpr int TestingClassMode = 1;
constexpr int RemoveMode = 2;

} // namespace

ScheduleTestingAssignmentDialog::ScheduleTestingAssignmentDialog(
    const Application::ScheduleTestingClassChoicesSnapshot& choices,
    const bool hasExistingAssignment,
    const QString& existingRoom,
    const std::optional<Domain::ClassId>& existingClassId,
    QWidget* parent
    )
    : QDialog(parent)
    , m_hasExistingAssignment(hasExistingAssignment)
{
    setWindowTitle(m_hasExistingAssignment
        ? tr("Edit Testing Assignment") : tr("Add Testing Assignment"));
    setModal(true);
    setMinimumWidth(420);
    m_layout = new QVBoxLayout(this);
    buildUi(choices, existingClassId);
    m_mode->setCurrentIndex(existingClassId ? TestingClassMode : PlainMode);
    m_room->setText(existingRoom);
    m_room->selectAll();
    updateModeUi();
    adjustSize();
    setFixedSize(size());
}

ScheduleTestingAssignmentDialog::Action
ScheduleTestingAssignmentDialog::selectedAction() const noexcept
{
    return m_action;
}

QString ScheduleTestingAssignmentDialog::room() const
{
    return m_room ? m_room->text().trimmed() : QString{};
}

std::optional<Domain::ClassId>
ScheduleTestingAssignmentDialog::selectedClassId() const
{
    if (!m_class)
    {
        return std::nullopt;
    }
    const std::string value = m_class->currentData().toString().toStdString();
    return Domain::ClassId::fromString(value);
}

void ScheduleTestingAssignmentDialog::buildUi(
    const Application::ScheduleTestingClassChoicesSnapshot& choices,
    const std::optional<Domain::ClassId>& existingClassId
    )
{
    auto* const layout = m_layout;
    layout->setAlignment(Qt::AlignTop);
    layout->addWidget(new QLabel(tr("Assignment Type"), this));

    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("nextTestingAssignmentMode"));
    m_mode->addItem(tr("Oral Testing Block"), PlainMode);
    m_mode->addItem(tr("Testing Class"), TestingClassMode);
    m_mode->addItem(tr("Essay Block"), RemoveMode);
    layout->addWidget(m_mode);

    m_roomLabel = new QLabel(tr("Room"), this);
    layout->addWidget(m_roomLabel);
    m_room = new QLineEdit(this);
    m_room->setObjectName(QStringLiteral("nextTestingAssignmentRoom"));
    m_room->setPlaceholderText(tr("Optional for an Oral Testing block"));
    layout->addWidget(m_room);

    m_classSection = new QWidget(this);
    auto* const classLayout = new QVBoxLayout(m_classSection);
    classLayout->setContentsMargins(0, 8, 0, 8);
    classLayout->setSpacing(12);
    m_classLabel = new QLabel(tr("Testing Class"), m_classSection);
    classLayout->addWidget(m_classLabel);
    m_class = new QComboBox(m_classSection);
    m_class->setObjectName(QStringLiteral("nextTestingAssignmentClass"));
    for (const Application::ScheduleTestingClassChoice& choice : choices.choices)
    {
        const QString id = QString::fromStdString(choice.classId.value());
        const QString name = QString::fromStdU16String(choice.name);
        const QString grade = QString::fromStdU16String(choice.grade);
        const QString level = QString::fromStdU16String(choice.level);
        const QString room = QString::fromStdU16String(choice.room);
        m_class->addItem(tr("%1 — %2 %3 — Room %4")
            .arg(name, grade, level, room), id);
    }
    if (existingClassId)
    {
        const int index = m_class->findData(
            QString::fromStdString(existingClassId->value()));
        if (index >= 0)
        {
            m_class->setCurrentIndex(index);
        }
    }
    classLayout->addWidget(m_class);
    layout->addWidget(m_classSection);

    auto* const footer = new TextFitDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(footer);
    footer->button(QDialogButtonBox::Save)->setText(tr("Save"));
    footer->button(QDialogButtonBox::Save)->setObjectName(
        QStringLiteral("nextTestingAssignmentSave"));
    footer->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    footer->button(QDialogButtonBox::Cancel)->setObjectName(
        QStringLiteral("nextTestingAssignmentCancel"));
    m_manageClasses = footer->addButton(
        tr("Manage Testing Classes"), QDialogButtonBox::ActionRole);
    m_manageClasses->setObjectName(
        QStringLiteral("nextTestingAssignmentManageClasses"));

    connect(m_mode, &QComboBox::currentIndexChanged,
        this, &ScheduleTestingAssignmentDialog::updateModeUi);
    connect(m_manageClasses, &QPushButton::clicked, this, [this] {
        m_action = Action::ManageTestingClasses;
        QDialog::accept();
    });
    connect(footer->button(QDialogButtonBox::Save), &QPushButton::clicked,
        this, &ScheduleTestingAssignmentDialog::accept);
}

void ScheduleTestingAssignmentDialog::updateModeUi()
{
    const bool plain = m_mode->currentData().toInt() == PlainMode;
    const bool testingClass = m_mode->currentData().toInt() == TestingClassMode;
    m_roomLabel->setVisible(plain);
    m_room->setVisible(plain);
    m_classSection->setVisible(testingClass);
    m_manageClasses->setVisible(testingClass);
}

void ScheduleTestingAssignmentDialog::accept()
{
    const int mode = m_mode->currentData().toInt();
    if (mode == TestingClassMode)
    {
        if (!selectedClassId())
        {
            QMessageBox::warning(this, tr("Testing Assignment"),
                tr("Choose a testing class or manage your classes."));
            return;
        }
        m_action = Action::AssignTestingClass;
    }
    else if (mode == RemoveMode)
    {
        m_action = Action::RemoveAssignment;
    }
    else
    {
        m_action = Action::SavePlainTesting;
    }
    QDialog::accept();
}

} // namespace ClassMngr::Next::Ui
