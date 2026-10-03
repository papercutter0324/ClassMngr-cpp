#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/selected_class_subtitle_batch_read_port.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesSelectedClassSubtitleBatchReadPort final
    : public Application::SelectedClassSubtitleBatchReadPort
{
public:
    explicit ApplicationServicesSelectedClassSubtitleBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::SelectedClassSubtitleBatchReadResult
    readSelectedClassSubtitles(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        if (classIds.empty())
        {
            return Application::SelectedClassSubtitleBatchReadResult::success({});
        }

        QList<int> legacyClassIds;
        legacyClassIds.reserve(static_cast<qsizetype>(classIds.size()));
        for (const Domain::ClassId& classId : classIds)
        {
            const std::optional<int> legacyClassId = legacyId(classId.value());
            if (!legacyClassId)
            {
                return Application::SelectedClassSubtitleBatchReadResult::failure(
                    error(
                        Domain::ErrorCode::InvalidInput,
                        "Selected class IDs must be canonical positive integers."
                        )
                    );
            }
            legacyClassIds.append(*legacyClassId);
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::SelectedClassSubtitleBatchReadResult::failure(
                unavailableError()
                );
        }

        ClassInfoRepository* const classRepository =
            session->classInfoRepository();
        if (!classRepository)
        {
            return Application::SelectedClassSubtitleBatchReadResult::failure(
                error(
                    Domain::ErrorCode::NotFound,
                    "The selected class subtitle repository is unavailable."
                    )
                );
        }

        Result<QList<ClassSubtitleBatchReadRecord>> loadedClasses = [&]()
            -> Result<QList<ClassSubtitleBatchReadRecord>>
        {
            try
            {
                return classRepository->loadClassSubtitleRecords(
                    legacyClassIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    QStringLiteral("The selected class details could not be loaded.")
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    QStringLiteral("The selected class details could not be loaded.")
                    );
            }
        }();

        if (!loadedClasses)
        {
            std::vector<Application::SelectedClassSubtitleReadSnapshot>
                snapshots;
            snapshots.reserve(classIds.size());
            const Domain::OperationError classError =
                legacyError(loadedClasses.error());
            for (const Domain::ClassId& classId : classIds)
            {
                snapshots.push_back({
                    classId,
                    Domain::Result<
                        Application::SelectedClassSubtitleFields
                        >::failure(classError),
                    Domain::Result<std::optional<
                        Application::SelectedClassSubtitleTeacherFields
                        >>::success(std::nullopt)
                });
            }

            return Application::SelectedClassSubtitleBatchReadResult::success(
                std::move(snapshots)
                );
        }

        if (loadedClasses->size() != classIds.size())
        {
            return Application::SelectedClassSubtitleBatchReadResult::failure(
                error(
                    Domain::ErrorCode::Validation,
                    "The selected class subtitle repository returned an incomplete class list."
                    )
                );
        }

        std::vector<Application::SelectedClassSubtitleReadSnapshot> snapshots;
        snapshots.reserve(classIds.size());
        std::vector<int> assignedTeacherIds;
        std::unordered_set<int> seenTeacherIds;
        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const ClassSubtitleBatchReadRecord& loadedClass =
                loadedClasses->at(static_cast<qsizetype>(index));
            if (loadedClass.classId != legacyClassIds.at(
                    static_cast<qsizetype>(index)))
            {
                return Application::SelectedClassSubtitleBatchReadResult::failure(
                    error(
                        Domain::ErrorCode::Validation,
                        "The selected class subtitle repository returned a different class identifier order."
                        )
                    );
            }

            Application::SelectedClassSubtitleFields fields;
            fields.classGrade = loadedClass.grade.toStdU16String();
            fields.classLevel = loadedClass.level.toStdU16String();
            fields.regularSchedule.reserve(
                static_cast<std::size_t>(loadedClass.regularTimes.size())
                );
            for (const ClassTime& row : loadedClass.regularTimes)
            {
                fields.regularSchedule.push_back({
                    row.day.toStdU16String(),
                    row.startTime.toStdU16String()
                });
            }

            snapshots.push_back({
                classIds[index],
                Domain::Result<
                    Application::SelectedClassSubtitleFields
                    >::success(std::move(fields)),
                Domain::Result<std::optional<
                    Application::SelectedClassSubtitleTeacherFields
                    >>::success(std::nullopt)
            });

            if (loadedClass.teacherId > 0
                && seenTeacherIds.insert(loadedClass.teacherId).second)
            {
                assignedTeacherIds.push_back(loadedClass.teacherId);
            }
        }

        if (assignedTeacherIds.empty())
        {
            return Application::SelectedClassSubtitleBatchReadResult::success(
                std::move(snapshots)
                );
        }

        TeacherRepository* const teacherRepository =
            session->teacherRepository();
        if (!teacherRepository)
        {
            const Domain::OperationError teacherError = error(
                Domain::ErrorCode::NotFound,
                "The assigned teacher display name repository is unavailable."
                );
            markAssignedTeacherFailures(
                *loadedClasses,
                snapshots,
                teacherError
                );
            return Application::SelectedClassSubtitleBatchReadResult::success(
                std::move(snapshots)
                );
        }

        QList<int> legacyTeacherIds;
        legacyTeacherIds.reserve(
            static_cast<qsizetype>(assignedTeacherIds.size())
            );
        for (const int teacherId : assignedTeacherIds)
        {
            legacyTeacherIds.append(teacherId);
        }

        Result<QList<TeacherDisplayNameBatchReadRecord>> loadedTeachers = [&]()
            -> Result<QList<TeacherDisplayNameBatchReadRecord>>
        {
            try
            {
                return teacherRepository->loadTeacherDisplayNameRecords(
                    legacyTeacherIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    QStringLiteral("The assigned teacher display names could not be loaded.")
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    QStringLiteral("The assigned teacher display names could not be loaded.")
                    );
            }
        }();

        if (!loadedTeachers)
        {
            markAssignedTeacherFailures(
                *loadedClasses,
                snapshots,
                legacyError(loadedTeachers.error())
                );
            return Application::SelectedClassSubtitleBatchReadResult::success(
                std::move(snapshots)
                );
        }

        std::unordered_map<int, Application::SelectedClassSubtitleTeacherFields>
            teacherFieldsById;
        teacherFieldsById.reserve(
            static_cast<std::size_t>(loadedTeachers->size())
            );
        for (const TeacherDisplayNameBatchReadRecord& loadedTeacher :
             *loadedTeachers)
        {
            if (loadedTeacher.teacherId <= 0
                || !seenTeacherIds.contains(loadedTeacher.teacherId)
                || teacherFieldsById.contains(loadedTeacher.teacherId))
            {
                markAssignedTeacherFailures(
                    *loadedClasses,
                    snapshots,
                    error(
                        Domain::ErrorCode::Validation,
                        "The assigned teacher display name repository returned an unexpected teacher identifier."
                        )
                    );
                return Application::SelectedClassSubtitleBatchReadResult::success(
                    std::move(snapshots)
                    );
            }

            teacherFieldsById.emplace(
                loadedTeacher.teacherId,
                Application::SelectedClassSubtitleTeacherFields{
                    loadedTeacher.teacherKr.toStdU16String(),
                    loadedTeacher.teacherEn.toStdU16String(),
                    loadedTeacher.preferredRomanization.toStdU16String(),
                    loadedTeacher.preferredName.toStdU16String()
                }
                );
        }

        for (std::size_t index = 0; index < loadedClasses->size(); ++index)
        {
            const int teacherId = loadedClasses->at(
                static_cast<qsizetype>(index)).teacherId;
            if (teacherId <= 0)
            {
                continue;
            }

            const auto fields = teacherFieldsById.find(teacherId);
            if (fields == teacherFieldsById.end())
            {
                snapshots[index].assignedTeacher =
                    Domain::Result<std::optional<
                        Application::SelectedClassSubtitleTeacherFields
                        >>::failure(error(
                            Domain::ErrorCode::NotFound,
                            "The assigned teacher display name failed: no matching record exists."
                            ));
                continue;
            }

            snapshots[index].assignedTeacher =
                Domain::Result<std::optional<
                    Application::SelectedClassSubtitleTeacherFields
                    >>::success(fields->second);
        }

        return Application::SelectedClassSubtitleBatchReadResult::success(
            std::move(snapshots)
            );
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return std::nullopt;
        }

        int parsed = 0;
        const auto [end, conversionError] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (conversionError != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value)
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static Domain::OperationError error(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return {
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return error(
            Domain::ErrorCode::NotFound,
            "The active database session for the selected class subtitles is unavailable."
            );
    }

    [[nodiscard]] static Domain::OperationError legacyError(
        const QString& message
        )
    {
        const QString normalized = message.toLower();
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        if (normalized.contains(QStringLiteral("not found"))
            || normalized.contains(QStringLiteral("no matching record"))
            || normalized.contains(QStringLiteral("unavailable")))
        {
            code = Domain::ErrorCode::NotFound;
        }
        else if (normalized.contains(QStringLiteral("invalid")))
        {
            code = Domain::ErrorCode::InvalidInput;
        }
        else if (normalized.contains(QStringLiteral("validation")))
        {
            code = Domain::ErrorCode::Validation;
        }

        const QByteArray bytes = message.toUtf8();
        return error(
            code,
            bytes.isEmpty()
                ? "The selected class subtitle source failed."
                : bytes.toStdString()
            );
    }

    static void markAssignedTeacherFailures(
        const QList<ClassSubtitleBatchReadRecord>& loadedClasses,
        std::vector<Application::SelectedClassSubtitleReadSnapshot>& snapshots,
        const Domain::OperationError& failure
        )
    {
        for (std::size_t index = 0; index < loadedClasses.size(); ++index)
        {
            if (loadedClasses.at(static_cast<qsizetype>(index)).teacherId > 0)
            {
                snapshots[index].assignedTeacher =
                    Domain::Result<std::optional<
                        Application::SelectedClassSubtitleTeacherFields
                        >>::failure(failure);
            }
        }
    }

    ApplicationServices* m_services;
};

} // namespace ClassMngr::Next::Platform
