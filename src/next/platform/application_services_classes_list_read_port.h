#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "next/application/classes_list_read_port.h"

#include <QByteArray>

#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassesListReadPort final
    : public Application::ClassesListReadPort
{
public:
    explicit ApplicationServicesClassesListReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassesListReadResult
    readClassesList() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        ClassRepository* const repository = session->classRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active class repository is unavailable."
                );
        }

        try
        {
            const Result<QList<Classroom>> loaded = repository->getClasses();
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::ClassesListSnapshot snapshot;
            snapshot.classes.reserve(static_cast<std::size_t>(loaded->size()));
            for (const Classroom& classroom : *loaded)
            {
                if (classroom.id <= 0)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The classes list contains an invalid class identifier."
                        );
                }

                const auto classId = Domain::ClassId::fromString(
                    std::to_string(classroom.id)
                    );
                if (!classId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The classes list contains an invalid class identifier."
                        );
                }

                snapshot.classes.push_back({
                    .classId = *classId,
                    .className = classroom.name.toStdU16String()
                });
            }

            return Application::ClassesListReadResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Classes could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Classes could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Application::ClassesListReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Classes could not be loaded.";
        }
        return Application::ClassesListReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        });
    }

    [[nodiscard]] static Application::ClassesListReadResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for classes is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
