#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/teacher_display_name_batch_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
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

        QList<qint64> legacyTeacherIds;
        legacyTeacherIds.reserve(
            static_cast<qsizetype>(teacherIds.size())
            );
        for (const Domain::TeacherId& teacherId : teacherIds)
        {
            const auto legacyId =
                Application::parseCanonicalPositiveDecimalId64(
                    teacherId.value()
                    );
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

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "No active Teacher Display Name database session is available.",
                true
                );
        }

        TeacherRepository* const repository = store->teacherRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The Teacher Display Name repository is unavailable.",
                true
                );
        }

        RepositoryResult<QList<MyClassesTeacherDisplayNameBatchReadRecord>> loaded = [&]()
            -> RepositoryResult<QList<MyClassesTeacherDisplayNameBatchReadRecord>>
        {
            try
            {
                return repository->loadMyClassesTeacherDisplayNameRecords(
                    legacyTeacherIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(RepositoryError{
                    RepositoryErrorCode::Technical,
                    QStringLiteral("Teacher display names could not be loaded.")
                });
            }
            catch (...)
            {
                return std::unexpected(RepositoryError{
                    RepositoryErrorCode::Technical,
                    QStringLiteral("Teacher display names could not be loaded.")
                });
            }
        }();

        if (!loaded)
        {
            return failure(
                domainErrorCode(loaded.error().code),
                utf8(toLegacyDiagnostic(loaded.error())).empty()
                    ? "Teacher display names could not be loaded."
                    : utf8(toLegacyDiagnostic(loaded.error())),
                false
                );
        }

        std::vector<Application::TeacherDisplayNameBatchReadSnapshot>
            snapshots;
        snapshots.reserve(static_cast<std::size_t>(loaded->size()));
        for (const MyClassesTeacherDisplayNameBatchReadRecord& record : *loaded)
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
    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
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
