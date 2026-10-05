#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/schedule_editor_class_info_query.h"

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

class ApplicationServicesScheduleEditorClassInfoReadPort final
    : public Application::ScheduleEditorClassInfoReadPort
{
public:
    explicit ApplicationServicesScheduleEditorClassInfoReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ScheduleEditorClassInfoReadResult
    readScheduleEditorClassInfo(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId =
            legacyId(classId.value());
        if (!legacyClassId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Selected class identifier must be a canonical positive integer."
                );
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        try
        {
            const Result<ScheduleEditorClassInfoReadRecord> loaded =
                repository->loadScheduleEditorClassInfoRecord(*legacyClassId);
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            const auto snapshotClassId = Domain::ClassId::fromString(
                std::to_string(loaded->classId)
                );
            if (!snapshotClassId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Loaded class details have an invalid class identifier."
                    );
            }

            return Application::ScheduleEditorClassInfoReadResult::success({
                .classId = *snapshotClassId,
                .classGrade = loaded->classGrade.toStdU16String(),
                .classLevel = loaded->classLevel.toStdU16String(),
                .readingBook = loaded->readingBook.toStdU16String(),
                .essayBook = loaded->essayBook.toStdU16String(),
                .classColor = loaded->classColor.toStdU16String(),
                .fontColor = loaded->fontColor.toStdU16String(),
                .teacherKoreanName =
                    loaded->teacherKoreanName.toStdU16String(),
                .roomNumber = loaded->roomNumber.toStdU16String()
            });
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be loaded."
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

    [[nodiscard]] static Application::ScheduleEditorClassInfoReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Class details could not be loaded.";
        }

        return Application::ScheduleEditorClassInfoReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Validation
        });
    }

    [[nodiscard]] static Application::ScheduleEditorClassInfoReadResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for class details is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
