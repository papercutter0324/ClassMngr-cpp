#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/class_co_teacher_page_read_port.h"

#include <QByteArray>

#include <charconv>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassCoTeacherPageReadPort final
    : public Application::ClassCoTeacherPageReadPort
{
public:
    explicit ApplicationServicesClassCoTeacherPageReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassCoTeacherPageReadResult
    readClassCoTeacherPage(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return Application::ClassCoTeacherPageReadResult::failure(error(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."
                ));
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return snapshotWithFailures(classId, unavailableError());
        }

        ClassInfoRepository* const classInfoRepository =
            session->classInfoRepository();
        if (!classInfoRepository)
        {
            return snapshotWithFailures(classId, error(
                Domain::ErrorCode::Technical,
                "Co-teacher class details could not be loaded."
                ));
        }

        Result<ClassPageDetailsReadRecord> loadedInfo =
            [&]() -> Result<ClassPageDetailsReadRecord>
        {
            try
            {
                return classInfoRepository->loadClassPageDetails(
                    *legacyClassId
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    QStringLiteral("Co-teacher class details could not be loaded.")
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    QStringLiteral("Co-teacher class details could not be loaded.")
                    );
            }
        }();

        if (!loadedInfo)
        {
            Application::ClassCoTeacherPageReadSnapshot snapshot{
                classId,
                failure<Application::ClassCoTeacherPageFields>(
                    fromLegacyError(loadedInfo.error())
                    ),
                failure<std::u16string>(error(
                    Domain::ErrorCode::NotFound,
                    "The co-teacher display name is unavailable."
                    ))
            };
            return Application::ClassCoTeacherPageReadResult::success(
                std::move(snapshot)
                );
        }

        const std::optional<Domain::ClassId> matchedClassId =
            Domain::ClassId::fromString(std::to_string(loadedInfo->classId));
        if (!matchedClassId)
        {
            return Application::ClassCoTeacherPageReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "Co-teacher class details returned an invalid class identifier."
                ));
        }

        std::optional<Domain::TeacherId> selectedTeacherId;
        Domain::Result<std::u16string> teacherDisplayName =
            Domain::Result<std::u16string>::success({});
        if (loadedInfo->teacherId > 0)
        {
            const std::string typedTeacherId =
                std::to_string(loadedInfo->teacherId);
            selectedTeacherId =
                Domain::TeacherId::fromString(typedTeacherId);
            if (!selectedTeacherId)
            {
                return Application::ClassCoTeacherPageReadResult::failure(error(
                    Domain::ErrorCode::Validation,
                    "Co-teacher class details returned an invalid teacher identifier."
                    ));
            }

            teacherDisplayName = loadTeacherDisplayName(
                session->teacherRepository(),
                loadedInfo->teacherId
                );
        }

        Application::ClassCoTeacherPageReadSnapshot snapshot{
            *matchedClassId,
            Domain::Result<Application::ClassCoTeacherPageFields>::success(
                projectClassFields(*loadedInfo, selectedTeacherId)
                ),
            std::move(teacherDisplayName)
        };
        return Application::ClassCoTeacherPageReadResult::success(
            std::move(snapshot)
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
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
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
            "The active database session for co-teacher details is unavailable."
            );
    }

    [[nodiscard]] static Domain::OperationError fromLegacyError(
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

        const QByteArray bytes = message.toUtf8();
        return error(
            code,
            bytes.isEmpty()
                ? "Co-teacher class details could not be loaded."
                : bytes.toStdString()
            );
    }

    template <typename Value>
    [[nodiscard]] static Domain::Result<Value> failure(
        Domain::OperationError operationError
        )
    {
        return Domain::Result<Value>::failure(std::move(operationError));
    }

    [[nodiscard]] static Application::ClassCoTeacherPageFields projectClassFields(
        const ClassPageDetailsReadRecord& info,
        std::optional<Domain::TeacherId> selectedTeacherId
        )
    {
        Application::ClassCoTeacherPageFields fields;
        fields.selectedTeacherId = std::move(selectedTeacherId);
        fields.classGrade = info.classGrade.toStdU16String();
        fields.classLevel = info.classLevel.toStdU16String();
        fields.regularSchedule.reserve(
            static_cast<std::size_t>(info.regularTimes.size())
            );
        for (const ClassPageDetailsRegularTime& time : info.regularTimes)
        {
            fields.regularSchedule.push_back({
                .day = time.day.toStdU16String(),
                .startTime = time.startTime.toStdU16String()
            });
        }
        return fields;
    }

    [[nodiscard]] static Application::ClassCoTeacherPageReadResult
    snapshotWithFailures(
        const Domain::ClassId& classId,
        const Domain::OperationError& sourceError
        )
    {
        Application::ClassCoTeacherPageReadSnapshot snapshot{
            classId,
            failure<Application::ClassCoTeacherPageFields>(sourceError),
            failure<std::u16string>(sourceError)
        };
        return Application::ClassCoTeacherPageReadResult::success(
            std::move(snapshot)
            );
    }

    [[nodiscard]] static Domain::Result<std::u16string>
    loadTeacherDisplayName(
        TeacherRepository* repository,
        const int teacherId
        )
    {
        if (!repository)
        {
            return failure<std::u16string>(error(
                Domain::ErrorCode::NotFound,
                "The co-teacher repository is unavailable."
                ));
        }

        try
        {
            const Result<TeacherDisplayNameReadRecord> loaded =
                repository->loadTeacherDisplayNameFields(teacherId);
            if (!loaded)
            {
                return failure<std::u16string>(
                    fromLegacyError(loaded.error())
                    );
            }
            Teacher teacher;
            teacher.teacherKr = loaded->teacherKr;
            teacher.teacherEn = loaded->teacherEn;
            teacher.preferredRomanization = loaded->preferredRomanization;
            teacher.preferredName = loaded->preferredName;
            return Domain::Result<std::u16string>::success(
                teacher.preferredDisplayName().toStdU16String()
                );
        }
        catch (const std::exception&)
        {
            return failure<std::u16string>(error(
                Domain::ErrorCode::Technical,
                "The co-teacher display name could not be loaded."
                ));
        }
        catch (...)
        {
            return failure<std::u16string>(error(
                Domain::ErrorCode::Technical,
                "The co-teacher display name could not be loaded."
                ));
        }
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
