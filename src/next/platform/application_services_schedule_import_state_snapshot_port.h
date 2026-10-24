#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/schedule_import_state_snapshot.h"

#include <QByteArray>
#include <QHash>
#include <QSet>
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

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return Failure{
                FailureKind::ActiveSessionUnavailable,
                FailureSource::Session,
                {}
            };
        }

        ClassRepository* const classRepository = store->classRepository();
        TeacherRepository* const teacherRepository =
            store->teacherRepository();
        ClassInfoRepository* const classInfoRepository =
            store->classInfoRepository();
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
            const RepositoryResult<QList<Classroom>> loadedClasses =
                classRepository->getClasses();
            const Result<QList<ScheduleImportTeacherReadRecord>>
                loadedTeachers =
                    teacherRepository->loadScheduleImportTeacherRecords();
            if (!loadedClasses)
            {
                return readFailure(
                    FailureSource::Classes,
                    toLegacyDiagnostic(loadedClasses.error())
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

            {
                QSet<int> classIds;
                classIds.reserve(loadedClasses->size());
                for (const Classroom& classroom : *loadedClasses)
                {
                    if (classroom.id <= 0
                        || classIds.contains(classroom.id)
                        || !classInfoById.contains(classroom.id))
                    {
                        return invalidSnapshot(
                            "The class list and schedule source returned inconsistent class IDs."
                            );
                    }
                    classIds.insert(classroom.id);
                }
                if (classInfoById.size() != classIds.size())
                {
                    return invalidSnapshot(
                        "The class list and schedule source returned inconsistent class IDs."
                        );
                }
            }

            Application::ScheduleImportStateSnapshot snapshot;
            snapshot.teachers.reserve(
                static_cast<std::size_t>(loadedTeachers->size())
                );
            {
                QSet<qint64> teacherIds;
                teacherIds.reserve(loadedTeachers->size());
                for (const ScheduleImportTeacherReadRecord& teacher :
                     *loadedTeachers)
                {
                    const std::optional<Domain::TeacherId> id =
                        canonicalTeacherId(teacher.teacherId);
                    if (!id || teacherIds.contains(teacher.teacherId))
                    {
                        return invalidSnapshot(
                            "The teacher source returned an invalid or duplicate teacher ID."
                            );
                    }
                    teacherIds.insert(teacher.teacherId);
                    snapshot.teachers.push_back({
                        *id,
                        teacher.teacherKr.toStdU16String(),
                        teacher.roomNumber.toStdU16String()
                    });
                }
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

    [[nodiscard]] static std::optional<Domain::TeacherId> canonicalTeacherId(
        const qint64 value
        )
    {
        if (value <= 0)
        {
            return std::nullopt;
        }
        return Domain::TeacherId::fromString(std::to_string(value));
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
