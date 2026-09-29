#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/schedule_testing_class_choices_query.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesScheduleTestingClassChoicesReadPort final
    : public Application::ScheduleTestingClassChoicesReadPort
{
public:
    explicit ApplicationServicesScheduleTestingClassChoicesReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesScheduleTestingClassChoicesReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ScheduleTestingClassChoicesReadResult
    readTestingClassChoices(
        const Application::ScheduleTestingClassChoicesReadQuery&
        ) const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        TestingClassRepository* const repository =
            session->testingClassRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        try
        {
            const Result<QList<TestingClass>> loaded =
                repository->loadTestingClasses();
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::ScheduleTestingClassChoicesSnapshot snapshot;
            snapshot.choices.reserve(
                static_cast<std::size_t>(loaded->size())
                );
            for (const TestingClass& testingClass : *loaded)
            {
                const std::string legacyClassId =
                    std::to_string(testingClass.classId);
                const auto classId =
                    Domain::ClassId::fromString(legacyClassId);
                if (!classId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A testing class has an invalid identifier."
                        );
                }

                snapshot.choices.push_back({
                    .classId = *classId,
                    .name = testingClass.name.toStdU16String(),
                    .grade = testingClass.grade.toStdU16String(),
                    .level = testingClass.level.toStdU16String(),
                    .room = testingClass.room.toStdU16String()
                });
            }

            return Application::ScheduleTestingClassChoicesReadResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing classes could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing classes could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::ScheduleTestingClassChoicesReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Testing classes could not be loaded.";
        }

        return Application::ScheduleTestingClassChoicesReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::NotFound
        });
    }

    [[nodiscard]] static Application::ScheduleTestingClassChoicesReadResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for testing classes is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
