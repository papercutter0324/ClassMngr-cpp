#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QByteArray>
#include <QHash>
#include <QString>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesScheduleImportStateSnapshotPort final
    : public Application::ScheduleImportStateSnapshotReadPort
{
public:
    explicit ApplicationServicesScheduleImportStateSnapshotPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    explicit ApplicationServicesScheduleImportStateSnapshotPort(
        ApplicationServices& services
        ) noexcept
        : ApplicationServicesScheduleImportStateSnapshotPort(&services)
    {
    }

    [[nodiscard]] Application::ScheduleImportStateSnapshotOutcome
    readCurrentScheduleImportState(
        const Application::ScheduleImportStateSnapshotQuery&
        ) const override
    {
        using Failure = Application::ScheduleImportStateSnapshotFailure;
        using FailureKind =
            Application::ScheduleImportStateSnapshotFailureKind;
        using FailureSource =
            Application::ScheduleImportStateSnapshotFailureSource;

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Failure{
                FailureKind::ActiveSessionUnavailable,
                FailureSource::Session,
                {}
            };
        }

        ClassRepository* const classRepository = session->classRepository();
        TeacherRepository* const teacherRepository =
            session->teacherRepository();
        ClassInfoRepository* const classInfoRepository =
            session->classInfoRepository();
        if (!classRepository)
        {
            return repositoryUnavailable(FailureSource::Classes);
        }
        if (!teacherRepository)
        {
            return repositoryUnavailable(FailureSource::Teachers);
        }
        if (!classInfoRepository)
        {
            return repositoryUnavailable(FailureSource::ClassSchedules);
        }

        try
        {
            // Retain the legacy source ordering exactly: class names first,
            // all teachers second, then each class's schedule details. The
            // detail projection is re-indexed into the class repository order.
            const Result<QList<Classroom>> loadedClasses =
                classRepository->getClasses();
            const Result<QList<ScheduleImportTeacherReadRecord>>
                loadedTeachers =
                    teacherRepository->loadScheduleImportTeacherRecords();
            if (!loadedClasses)
            {
                return readFailure(
                    FailureSource::Classes,
                    loadedClasses.error()
                    );
            }
            if (!loadedTeachers)
            {
                return readFailure(
                    FailureSource::Teachers,
                    loadedTeachers.error()
                    );
            }

            const Result<QList<
                ScheduleImportStateSnapshotClassInfoReadRecord>>
                loadedClassInfos =
                classInfoRepository->loadScheduleImportStateSnapshotClassInfos();
            if (!loadedClassInfos)
            {
                return readFailure(
                    FailureSource::ClassSchedules,
                    loadedClassInfos.error()
                    );
            }

            QHash<
                int,
                const ScheduleImportStateSnapshotClassInfoReadRecord*>
                classInfoById;
            classInfoById.reserve(loadedClassInfos->size());
            for (const ScheduleImportStateSnapshotClassInfoReadRecord& info :
                 *loadedClassInfos)
            {
                if (info.classId <= 0
                    || classInfoById.contains(info.classId))
                {
                    return invalidSnapshot(
                        "The class schedule source returned an invalid or duplicate class ID."
                        );
                }
                classInfoById.insert(info.classId, &info);
            }

            QHash<int, QString> classNameById;
            classNameById.reserve(loadedClasses->size());
            for (const Classroom& classroom : *loadedClasses)
            {
                if (classroom.id <= 0
                    || classNameById.contains(classroom.id)
                    || !classInfoById.contains(classroom.id))
                {
                    return invalidSnapshot(
                        "The class list and schedule source returned inconsistent class IDs."
                        );
                }
                classNameById.insert(classroom.id, classroom.name);
            }
            if (classInfoById.size() != classNameById.size())
            {
                return invalidSnapshot(
                    "The class list and schedule source returned inconsistent class IDs."
                    );
            }

            Application::ScheduleImportStateSnapshot snapshot;
            snapshot.teachers.reserve(
                static_cast<std::size_t>(loadedTeachers->size())
                );
            QHash<int, const ScheduleImportTeacherReadRecord*> teacherById;
            teacherById.reserve(loadedTeachers->size());
            for (const ScheduleImportTeacherReadRecord& teacher : *loadedTeachers)
            {
                const std::optional<Domain::TeacherId> id =
                    canonicalId<Domain::TeacherId>(teacher.teacherId);
                if (!id || teacherById.contains(teacher.teacherId))
                {
                    return invalidSnapshot(
                        "The teacher source returned an invalid or duplicate teacher ID."
                        );
                }
                teacherById.insert(teacher.teacherId, &teacher);
                snapshot.teachers.push_back({
                    *id,
                    teacher.teacherKr.toStdU16String(),
                    teacher.roomNumber.toStdU16String()
                });
            }

            snapshot.classes.reserve(
                static_cast<std::size_t>(loadedClasses->size())
                );
            for (const Classroom& classroom : *loadedClasses)
            {
                const ScheduleImportStateSnapshotClassInfoReadRecord& info =
                    *classInfoById.value(classroom.id);
                const std::optional<Domain::ClassId> classId =
                    canonicalId<Domain::ClassId>(classroom.id);
                const std::optional<Domain::TeacherId> teacherId =
                    Domain::TeacherId::fromString(
                        std::to_string(info.teacherId)
                        );
                if (!classId || !teacherId)
                {
                    return invalidSnapshot(
                        "The class schedule source returned an invalid class or teacher ID."
                        );
                }

                snapshot.classes.push_back({
                    *classId,
                    *teacherId,
                    classroom.name.toStdU16String(),
                    info.classGrade.toStdU16String(),
                    info.classLevel.toStdU16String(),
                    info.classColor.toStdU16String(),
                    scheduleTimes(info.regularTimes),
                    scheduleTimes(info.intensiveTimes),
                    info.roomNumber.toStdU16String()
                });
            }

            return snapshot;
        }
        catch (const std::exception&)
        {
            return readFailure(
                FailureSource::ClassSchedules,
                std::string(
                    "Current schedule import state could not be loaded."
                    )
                );
        }
        catch (...)
        {
            return readFailure(
                FailureSource::ClassSchedules,
                std::string(
                    "Current schedule import state could not be loaded."
                    )
                );
        }
    }

private:
    template <typename Id>
    [[nodiscard]] static std::optional<Id> canonicalId(const int value)
    {
        if (value <= 0)
        {
            return std::nullopt;
        }
        return Id::fromString(std::to_string(value));
    }

    [[nodiscard]] static std::vector<
        Application::ScheduleImportStateReadTime
        > scheduleTimes(const QList<ClassTime>& source)
    {
        std::vector<Application::ScheduleImportStateReadTime> result;
        result.reserve(static_cast<std::size_t>(source.size()));
        for (const ClassTime& time : source)
        {
            result.push_back({
                time.day.toStdU16String(),
                time.startTime.toStdU16String(),
                time.endTime.toStdU16String()
            });
        }
        return result;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::ScheduleImportStateSnapshotOutcome
    repositoryUnavailable(
        const Application::ScheduleImportStateSnapshotFailureSource source
        )
    {
        return Application::ScheduleImportStateSnapshotFailure{
            Application::ScheduleImportStateSnapshotFailureKind::
                RepositoryUnavailable,
            source,
            {}
        };
    }

    [[nodiscard]] static Application::ScheduleImportStateSnapshotOutcome
    readFailure(
        const Application::ScheduleImportStateSnapshotFailureSource source,
        const QString& message
        )
    {
        return readFailure(source, toStdString(message));
    }

    [[nodiscard]] static Application::ScheduleImportStateSnapshotOutcome
    readFailure(
        const Application::ScheduleImportStateSnapshotFailureSource source,
        std::string message
        )
    {
        return Application::ScheduleImportStateSnapshotFailure{
            Application::ScheduleImportStateSnapshotFailureKind::
                RepositoryReadFailed,
            source,
            std::move(message)
        };
    }

    [[nodiscard]] static Application::ScheduleImportStateSnapshotOutcome
    invalidSnapshot(std::string message)
    {
        return Application::ScheduleImportStateSnapshotFailure{
            Application::ScheduleImportStateSnapshotFailureKind::
                InvalidSnapshot,
            Application::ScheduleImportStateSnapshotFailureSource::Snapshot,
            std::move(message)
        };
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
