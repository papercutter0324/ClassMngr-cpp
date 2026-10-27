#pragma once

#include "next/application/teacher_create.h"
#include "next/application/teacher_delete.h"
#include "next/application/teacher_directory_read_port.h"
#include "next/application/teacher_profile_read_port.h"

#include <QPointer>
#include <QString>

#include <functional>
#include <optional>

class QWidget;

namespace ClassMngr::Next::Ui
{

struct KoreanTeacherDirectoryActionPorts final
{
    std::function<Application::TeacherDirectoryReadResult()> readTeachers;
    std::function<Application::TeacherCreateResult(
        const Application::TeacherCreateRequest&)> createTeacher;
    std::function<Application::TeacherProfileReadResult(
        const Domain::TeacherId&)> readTeacherProfile;
    std::function<Application::TeacherDeleteResult(
        const Application::TeacherDeleteRequest&)> deleteTeacher;
    std::function<bool()> refreshTeacherRows;
    std::function<bool(const Domain::TeacherId&)> openCreatedTeacherProfile;
    std::function<std::optional<Domain::TeacherId>(
        const Application::TeacherDirectoryReadSnapshot&)> chooseTeacherToDelete;
    std::function<bool(
        const Application::TeacherProfileReadSnapshot&)> confirmDelete;
    std::function<void(const QString& title, const QString& message)> reportError;
};

// Owns the modal New/Delete flows attached to the Korean Teachers sidebar
// group's context menu. Teacher selection itself stays in the sidebar tree.
class KoreanTeacherDirectoryActions final
{
public:
    explicit KoreanTeacherDirectoryActions(
        QWidget* parent,
        KoreanTeacherDirectoryActionPorts ports
        );

    void createTeacher();
    void deleteTeacher(std::optional<Domain::TeacherId> requestedTeacherId);

private:
    [[nodiscard]] std::optional<Domain::TeacherId> chooseTeacherToDelete(
        const Application::TeacherDirectoryReadSnapshot& snapshot
        ) const;
    [[nodiscard]] bool confirmDelete(
        const Application::TeacherProfileReadSnapshot& profile
        ) const;
    void reportError(const QString& title, const QString& message) const;

    QPointer<QWidget> m_parent;
    KoreanTeacherDirectoryActionPorts m_ports;
};

} // namespace ClassMngr::Next::Ui
