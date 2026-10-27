#include "next/ui/korean_teacher_directory_actions.h"

#include "next/domain/teacher_display_name.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

[[nodiscard]] QString pageText(const char* text)
{
    return QCoreApplication::translate("KoreanTeacherListPage", text);
}

[[nodiscard]] QString messageText(const std::string& message)
{
    return QString::fromUtf8(
        message.data(),
        static_cast<qsizetype>(message.size())
        );
}

[[nodiscard]] QString teacherLabel(
    const std::u16string& preferredName,
    const std::u16string& englishName,
    const std::u16string& preferredRomanization,
    const std::u16string& koreanName
    )
{
    const Domain::TeacherDisplayName selected = Domain::TeacherDisplayName::select(
        preferredName,
        englishName,
        preferredRomanization,
        koreanName
        );
    const QString label = QString::fromStdU16String(selected.value());
    return label.isEmpty() ? pageText("New Teacher") : label;
}

[[nodiscard]] QString errorMessage(const char* fallback)
{
    return QString::fromUtf8(fallback);
}

} // namespace

KoreanTeacherDirectoryActions::KoreanTeacherDirectoryActions(
    QWidget* const parent,
    KoreanTeacherDirectoryActionPorts ports
    )
    : m_parent(parent)
    , m_ports(std::move(ports))
{
}

void KoreanTeacherDirectoryActions::createTeacher()
{
    QDialog dialog(m_parent);
    dialog.setObjectName(QStringLiteral("teacherCreateDialog"));
    dialog.setWindowTitle(pageText("Add Korean Teacher"));
    dialog.setModal(true);

    auto* const layout = new QVBoxLayout(&dialog);
    auto* const help = new QLabel(
        pageText("Enter a Korean name, English name, or preferred spelling."),
        &dialog
        );
    help->setWordWrap(true);
    layout->addWidget(help);

    auto* const form = new QFormLayout;
    auto* const koreanName = new QLineEdit(&dialog);
    koreanName->setObjectName(QStringLiteral("teacherCreateKoreanNameEdit"));
    auto* const englishName = new QLineEdit(&dialog);
    englishName->setObjectName(QStringLiteral("teacherCreateEnglishNameEdit"));
    auto* const romanization = new QLineEdit(&dialog);
    romanization->setObjectName(QStringLiteral("teacherCreatePreferredSpellingEdit"));
    form->addRow(pageText("Korean Name"), koreanName);
    form->addRow(pageText("English Name"), englishName);
    form->addRow(pageText("Preferred Spelling"), romanization);
    layout->addLayout(form);

    auto* const status = new QLabel(&dialog);
    status->setObjectName(QStringLiteral("teacherCreateStatus"));
    status->setWordWrap(true);
    status->hide();
    layout->addWidget(status);

    auto* const buttons = new QDialogButtonBox(&dialog);
    QPushButton* const createButton = buttons->addButton(
        pageText("Create"),
        QDialogButtonBox::AcceptRole
        );
    createButton->setObjectName(QStringLiteral("teacherCreateSubmitButton"));
    buttons->addButton(pageText("Cancel"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    std::optional<Domain::TeacherId> createdTeacherId;
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dialog, &QDialog::reject);
    QObject::connect(createButton, &QPushButton::clicked, &dialog, [&, this] {
        Application::TeacherCreateRequest request{
            .fields = {
                .teacherKr = koreanName->text().toStdU16String(),
                .teacherEn = englishName->text().toStdU16String(),
                .preferredRomanization =
                    romanization->text().toStdU16String()
            }
        };
        const Application::TeacherCreateResult created = m_ports.createTeacher
            ? m_ports.createTeacher(request)
            : Application::TeacherCreateResult::failure({
                  .code = Domain::ErrorCode::NotFound,
                  .message = "The active workspace store for teachers is unavailable.",
                  .recoverable = true
              });
        if (!created)
        {
            status->setText(messageText(created.error().message));
            status->show();
            return;
        }
        createdTeacherId = created.value();
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted || !createdTeacherId)
    {
        return;
    }

    if (m_ports.refreshTeacherRows && !m_ports.refreshTeacherRows())
    {
        reportError(
            pageText("Add Korean Teacher"),
            errorMessage("The teacher was created, but the Korean Teachers list could not be refreshed.")
            );
    }
    if (m_ports.openCreatedTeacherProfile
        && !m_ports.openCreatedTeacherProfile(*createdTeacherId))
    {
        return;
    }
}

void KoreanTeacherDirectoryActions::deleteTeacher(
    std::optional<Domain::TeacherId> requestedTeacherId
    )
{
    if (!requestedTeacherId)
    {
        if (!m_ports.readTeachers)
        {
            reportError(
                pageText("Delete Teacher"),
                errorMessage("The Korean teacher list is unavailable.")
                );
            return;
        }
        const Application::TeacherDirectoryReadResult loaded =
            m_ports.readTeachers();
        if (!loaded)
        {
            reportError(pageText("Delete Teacher"),
                messageText(loaded.error().message));
            return;
        }
        requestedTeacherId = chooseTeacherToDelete(loaded.value());
        if (!requestedTeacherId)
        {
            return;
        }
    }

    if (!m_ports.readTeacherProfile)
    {
        reportError(
            pageText("Delete Teacher"),
            errorMessage("The selected teacher could not be loaded.")
            );
        return;
    }

    const Application::TeacherProfileReadResult profile =
        m_ports.readTeacherProfile(*requestedTeacherId);
    if (!profile)
    {
        reportError(pageText("Delete Teacher"), messageText(profile.error().message));
        return;
    }
    if (profile.value().teacherId != *requestedTeacherId)
    {
        reportError(
            pageText("Delete Teacher"),
            errorMessage("The selected teacher could not be verified.")
            );
        return;
    }
    if (!confirmDelete(profile.value()))
    {
        return;
    }

    const Application::TeacherDeleteResult removed = m_ports.deleteTeacher
        ? m_ports.deleteTeacher({.teacherId = *requestedTeacherId})
        : Application::TeacherDeleteResult::failure({
              .code = Domain::ErrorCode::NotFound,
              .message = "The active workspace store for teachers is unavailable.",
              .recoverable = true
          });
    if (!removed)
    {
        reportError(pageText("Delete Teacher"), messageText(removed.error().message));
        return;
    }

    if (m_ports.refreshTeacherRows && !m_ports.refreshTeacherRows())
    {
        reportError(
            pageText("Delete Teacher"),
            errorMessage("The teacher was deleted, but the Korean Teachers list could not be refreshed.")
            );
    }
}

std::optional<Domain::TeacherId>
KoreanTeacherDirectoryActions::chooseTeacherToDelete(
    const Application::TeacherDirectoryReadSnapshot& snapshot
    ) const
{
    if (snapshot.teachers.empty())
    {
        return std::nullopt;
    }
    if (m_ports.chooseTeacherToDelete)
    {
        return m_ports.chooseTeacherToDelete(snapshot);
    }

    QDialog dialog(m_parent);
    dialog.setObjectName(QStringLiteral("teacherDeleteChooserDialog"));
    dialog.setWindowTitle(pageText("Delete Teacher"));
    auto* const layout = new QVBoxLayout(&dialog);
    auto* const prompt = new QLabel(
        pageText("Which teacher would you like to delete?"),
        &dialog
        );
    prompt->setWordWrap(true);
    layout->addWidget(prompt);

    auto* const teachers = new QComboBox(&dialog);
    teachers->setObjectName(QStringLiteral("teacherDeleteChooserCombo"));
    for (const Application::TeacherDirectoryEntry& teacher : snapshot.teachers)
    {
        teachers->addItem(
            teacherLabel(
                teacher.preferredName,
                teacher.teacherEn,
                teacher.preferredRomanization,
                teacher.teacherKr
                ),
            QString::fromStdString(teacher.teacherId.value())
            );
    }
    layout->addWidget(teachers);

    auto* const buttons = new QDialogButtonBox(&dialog);
    buttons->addButton(pageText("Delete"), QDialogButtonBox::AcceptRole);
    buttons->addButton(pageText("Cancel"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const auto id = Domain::TeacherId::fromString(
        teachers->currentData().toString().toStdString()
        );
    return id;
}

bool KoreanTeacherDirectoryActions::confirmDelete(
    const Application::TeacherProfileReadSnapshot& profile
    ) const
{
    if (m_ports.confirmDelete)
    {
        return m_ports.confirmDelete(profile);
    }

    const QString displayName = teacherLabel(
        profile.fields.preferredName,
        profile.fields.teacherEn,
        profile.fields.preferredRomanization,
        profile.fields.teacherKr
        );
    return QMessageBox::question(
        m_parent,
        pageText("Delete Teacher"),
        pageText("Delete %1?").arg(displayName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        ) == QMessageBox::Yes;
}

void KoreanTeacherDirectoryActions::reportError(
    const QString& title,
    const QString& message
    ) const
{
    if (m_ports.reportError)
    {
        m_ports.reportError(title, message);
        return;
    }
    QMessageBox::warning(m_parent, title, message);
}

} // namespace ClassMngr::Next::Ui
