#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_info.h"
#include "next/application/my_classes_class_information_read_port.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesMyClassesClassInformationReadPort final
    : public Application::MyClassesClassInformationReadPort
{
public:
    explicit ApplicationServicesMyClassesClassInformationReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    explicit ApplicationServicesMyClassesClassInformationReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    ApplicationServicesMyClassesClassInformationReadPort(
        const ApplicationServicesMyClassesClassInformationReadPort&
        ) = delete;
    ApplicationServicesMyClassesClassInformationReadPort& operator=(
        const ApplicationServicesMyClassesClassInformationReadPort&
        ) = delete;
    ApplicationServicesMyClassesClassInformationReadPort(
        ApplicationServicesMyClassesClassInformationReadPort&&
        ) = delete;
    ApplicationServicesMyClassesClassInformationReadPort& operator=(
        ApplicationServicesMyClassesClassInformationReadPort&&
        ) = delete;

    [[nodiscard]] Application::MyClassesClassInformationReadResult
    readMyClassesClassInformation(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer.",
                false
                );
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "My Classes class information is unavailable.",
                true
                );
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "My Classes class information repository is unavailable.",
                true
                );
        }

        try
        {
            const Result<ClassInfo> loaded =
                repository->loadClassInfo(*legacyClassId);
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error()),
                    true
                    );
            }

            const auto loadedClassId = Domain::ClassId::fromString(
                std::to_string(loaded->classId)
                );
            if (!loadedClassId || loaded->classId <= 0)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "My Classes class information has an invalid class identifier.",
                    false
                    );
            }

            Application::MyClassesClassInformationFields fields;
            fields.classGrade = loaded->classGrade.toStdU16String();
            fields.classLevel = loaded->classLevel.toStdU16String();
            fields.regularSchedule = schedule(loaded->classTimes);
            fields.intensiveSchedule = schedule(loaded->intensiveTimes);
            fields.notes = loaded->notes.toStdU16String();
            fields.timeFillerActivities =
                loaded->timeFillerActivities.toStdU16String();
            if (loaded->teacherId > 0)
            {
                fields.teacherId = Domain::TeacherId::fromString(
                    std::to_string(loaded->teacherId)
                    );
            }

            return Application::MyClassesClassInformationReadResult::success({
                .classId = *loadedClassId,
                .fields = std::move(fields)
            });
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "My Classes class information could not be loaded.",
                true
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "My Classes class information could not be loaded.",
                true
                );
        }
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

    [[nodiscard]] static std::vector<Application::MyClassesScheduleEntry>
    schedule(const QList<ClassTime>& times)
    {
        std::vector<Application::MyClassesScheduleEntry> entries;
        entries.reserve(static_cast<std::size_t>(times.size()));
        for (const ClassTime& time : times)
        {
            entries.push_back({
                .day = time.day.toStdU16String(),
                .startTime = time.startTime.toStdU16String(),
                .endTime = time.endTime.toStdU16String()
            });
        }
        return entries;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::MyClassesClassInformationReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable
        )
    {
        return Application::MyClassesClassInformationReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
