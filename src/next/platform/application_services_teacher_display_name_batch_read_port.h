#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/teacher_display_name_batch_read_port.h"

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

class ApplicationServicesTeacherDisplayNameBatchReadPort final
    : public Application::TeacherDisplayNameBatchReadPort
{
public:
    explicit ApplicationServicesTeacherDisplayNameBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::TeacherDisplayNameBatchReadResult
    readTeacherDisplayNames(
        const std::vector<Domain::TeacherId>& teacherIds
        ) const override
    {
        if (teacherIds.empty())
        {
            return Application::TeacherDisplayNameBatchReadResult::success({});
        }

        QList<int> legacyTeacherIds;
        legacyTeacherIds.reserve(
            static_cast<qsizetype>(teacherIds.size())
            );
        for (const Domain::TeacherId& teacherId : teacherIds)
        {
            const std::optional<int> legacyId = legacyTeacherId(teacherId);
            if (!legacyId)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Teacher IDs must be canonical positive integers.",
                    false
                    );
            }
            legacyTeacherIds.append(*legacyId);
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "No active Teacher Display Name database session is available.",
                true
                );
        }

        TeacherRepository* const repository = session->teacherRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The Teacher Display Name repository is unavailable.",
                true
                );
        }

        Result<QList<TeacherDisplayNameBatchReadRecord>> loaded = [&]()
            -> Result<QList<TeacherDisplayNameBatchReadRecord>>
        {
            try
            {
                return repository->loadTeacherDisplayNameRecords(
                    legacyTeacherIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(QStringLiteral(
                    "Teacher display names could not be loaded."
                    ));
            }
            catch (...)
            {
                return std::unexpected(QStringLiteral(
                    "Teacher display names could not be loaded."
                    ));
            }
        }();

        if (!loaded)
        {
            return failure(
                Domain::ErrorCode::Technical,
                utf8(loaded.error()).empty()
                    ? "Teacher display names could not be loaded."
                    : utf8(loaded.error()),
                false
                );
        }

        std::vector<Application::TeacherDisplayNameBatchReadSnapshot>
            snapshots;
        snapshots.reserve(static_cast<std::size_t>(loaded->size()));
        for (const TeacherDisplayNameBatchReadRecord& record : *loaded)
        {
            if (record.teacherId <= 0)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The teacher display name repository returned an invalid teacher identifier.",
                    false
                    );
            }

            snapshots.push_back({
                .teacherId = *Domain::TeacherId::fromString(
                    std::to_string(record.teacherId)),
                .fields = {
                    .teacherKr = record.teacherKr.toStdU16String(),
                    .teacherEn = record.teacherEn.toStdU16String(),
                    .preferredRomanization =
                        record.preferredRomanization.toStdU16String(),
                    .preferredName = record.preferredName.toStdU16String()
                }
            });
        }

        return Application::TeacherDisplayNameBatchReadResult::success(
            std::move(snapshots)
            );
    }

private:
    [[nodiscard]] static std::optional<int> legacyTeacherId(
        const Domain::TeacherId& id
        )
    {
        const std::string& value = id.value();
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

    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Application::TeacherDisplayNameBatchReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable
        )
    {
        return Application::TeacherDisplayNameBatchReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
