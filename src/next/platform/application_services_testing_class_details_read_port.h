#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/testing_class_details_read_query.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTestingClassDetailsReadPort final
    : public Application::TestingClassDetailsReadPort
{
public:
    explicit ApplicationServicesTestingClassDetailsReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTestingClassDetailsReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::TestingClassDetailsReadResult
    readTestingClassDetails(
        const Application::TestingClassDetailsReadQuery& query
        ) const override
    {
        const std::optional<int> legacyClassId =
            legacyId(query.classId.value());
        if (!legacyClassId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Selected testing class identifier must be a canonical positive integer."
                );
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return unavailableFailure();
        }

        TestingClassRepository* const repository =
            store->testingClassRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class details could not be loaded."
                );
        }

        try
        {
            const RepositoryResult<TestingClassDetailsReadRecord> loaded =
                repository->loadTestingClassDetailsReadRecordDetailed(
                    *legacyClassId
                    );
            if (!loaded)
            {
                return failure(
                    mapRepositoryErrorCode(loaded.error().code),
                    toStdString(loaded.error().diagnostic)
                    );
            }

            const auto loadedClassId = Domain::ClassId::fromString(
                std::to_string(loaded->classId)
                );
            if (!loadedClassId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Loaded testing class details have an invalid class identifier."
                    );
            }
            if (*loadedClassId != query.classId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Testing class details returned a different class identifier."
                    );
            }

            std::optional<Domain::TeacherId> teacherId;
            // The legacy editor treats every nonpositive ID as its "None" row.
            if (loaded->teacherId > 0)
            {
                teacherId = Domain::TeacherId::fromString(
                    std::to_string(loaded->teacherId)
                    );
                if (!teacherId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "Loaded testing class details have an invalid teacher identifier."
                        );
                }
            }

            return Application::TestingClassDetailsReadResult::success({
                .classId = *loadedClassId,
                .name = loaded->name.toStdU16String(),
                .grade = loaded->grade.toStdU16String(),
                .level = loaded->level.toStdU16String(),
                .room = loaded->room.toStdU16String(),
                .teacherId = std::move(teacherId),
                .classColor = loaded->classColor.toStdU16String(),
                .fontColor = loaded->fontColor.toStdU16String(),
                .notes = loaded->notes.toStdU16String()
            });
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class details could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Testing class details could not be loaded."
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

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Domain::ErrorCode mapRepositoryErrorCode(
        const RepositoryErrorCode code
        )
    {
        switch (code)
        {
        case RepositoryErrorCode::InvalidInput:
            return Domain::ErrorCode::InvalidInput;
        case RepositoryErrorCode::NotFound:
            return Domain::ErrorCode::NotFound;
        case RepositoryErrorCode::Conflict:
            return Domain::ErrorCode::Conflict;
        case RepositoryErrorCode::Validation:
            return Domain::ErrorCode::Validation;
        case RepositoryErrorCode::Technical:
            return Domain::ErrorCode::Technical;
        }
        return Domain::ErrorCode::Technical;
    }

    [[nodiscard]] static Application::TestingClassDetailsReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Testing class details could not be loaded.";
        }

        return Application::TestingClassDetailsReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
                || code == Domain::ErrorCode::InvalidInput
        });
    }

    [[nodiscard]] static Application::TestingClassDetailsReadResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for testing class details is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
