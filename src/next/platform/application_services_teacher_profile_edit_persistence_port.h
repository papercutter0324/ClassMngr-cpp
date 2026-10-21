#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/application/teacher_profile_edit_use_case.h"

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

class ApplicationServicesTeacherProfileEditPersistencePort final
    : public Application::TeacherProfileEditPersistencePort
{
public:
    explicit ApplicationServicesTeacherProfileEditPersistencePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesTeacherProfileEditPersistencePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesTeacherProfileEditPersistencePort(
        const ApplicationServicesTeacherProfileEditPersistencePort&
        ) = delete;
    ApplicationServicesTeacherProfileEditPersistencePort& operator=(
        const ApplicationServicesTeacherProfileEditPersistencePort&
        ) = delete;
    ApplicationServicesTeacherProfileEditPersistencePort(
        ApplicationServicesTeacherProfileEditPersistencePort&&
        ) = delete;
    ApplicationServicesTeacherProfileEditPersistencePort& operator=(
        ApplicationServicesTeacherProfileEditPersistencePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> update(
        const Domain::TeacherProfile& profile
        ) const override
    {
        TeacherRepository* const repository = activeRepository();
        if (!repository)
        {
            return Domain::Result<void>::failure(unavailableError());
        }

        const std::optional<qint64> parsedId = wideTeacherId(profile.id);
        if (!parsedId)
        {
            return Domain::Result<void>::failure(invalidTeacherIdError());
        }

        try
        {
            const TeacherUpdateOutcome result =
                repository->updateTeacherWithOutcome(
                    teacherFromProfile(profile), *parsedId);
            if (result.code == TeacherUpdateOutcomeCode::Missing)
            {
                return Domain::Result<void>::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = utf8(result.diagnostic),
                    .recoverable = true
                });
            }
            if (result.code != TeacherUpdateOutcomeCode::Updated)
            {
                return Domain::Result<void>::failure(
                    technicalError(result.diagnostic,
                        QStringLiteral("Updating teacher failed.")));
            }
            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(technicalError(
                QStringLiteral("Updating teacher failed.")));
        }
        catch (...)
        {
            return Domain::Result<void>::failure(technicalError(
                QStringLiteral("Updating teacher failed.")));
        }
    }

    [[nodiscard]] Domain::Result<Domain::TeacherProfile> reload(
        const Domain::TeacherId id
        ) const override
    {
        const std::optional<qint64> parsedId = wideTeacherId(id);
        if (!parsedId)
        {
            return Domain::Result<Domain::TeacherProfile>::failure(
                invalidTeacherIdError());
        }

        TeacherRepository* const repository = activeRepository();
        if (!repository)
        {
            return Domain::Result<Domain::TeacherProfile>::failure(
                unavailableError());
        }

        try
        {
            const TeacherProfileLookupResult result =
                repository->lookupTeacherProfileById(
                    *parsedId, QObject::tr("Loading teacher"));
            switch (result.code)
            {
            case TeacherProfileLookupCode::Found:
                if (result.profile.teacherId != *parsedId)
                {
                    return Domain::Result<Domain::TeacherProfile>::failure(
                        technicalError(QStringLiteral(
                            "Reloaded teacher ID does not match the requested ID.")));
                }

                return Domain::Result<Domain::TeacherProfile>::success({
                    .id = id,
                    .fields = profileFieldsFromRecord(result.profile)
                });
            case TeacherProfileLookupCode::Missing:
                return Domain::Result<Domain::TeacherProfile>::failure({
                    .code = Domain::ErrorCode::NotFound,
                    .message = utf8(result.diagnostic),
                    .recoverable = true
                });
            case TeacherProfileLookupCode::Failure:
                return Domain::Result<Domain::TeacherProfile>::failure(
                    technicalError(result.diagnostic,
                        QStringLiteral("Reloading teacher failed.")));
            }

            return Domain::Result<Domain::TeacherProfile>::failure(
                technicalError(QStringLiteral("Reloading teacher failed.")));
        }
        catch (const std::exception&)
        {
            return Domain::Result<Domain::TeacherProfile>::failure(
                technicalError(QStringLiteral("Reloading teacher failed.")));
        }
        catch (...)
        {
            return Domain::Result<Domain::TeacherProfile>::failure(
                technicalError(QStringLiteral("Reloading teacher failed.")));
        }
    }

private:
    [[nodiscard]] static std::optional<qint64> wideTeacherId(
        const Domain::TeacherId& id
        )
    {
        const std::string& value = id.value();
        qint64 parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (
            error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value
            )
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] TeacherRepository* activeRepository() const noexcept
    {
        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return nullptr;
        }
        return store->teacherRepository();
    }

    [[nodiscard]] static QString legacyText(
        const std::u16string& value
        )
    {
        return QString::fromUtf16(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static std::string utf8(
        const QString& value
        )
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Teacher teacherFromProfile(
        const Domain::TeacherProfile& profile
        )
    {
        const Domain::TeacherProfileFields& fields = profile.fields;
        return {
            .teacherKr = legacyText(fields.teacherKr),
            .teacherEn = legacyText(fields.teacherEn),
            .preferredRomanization = legacyText(fields.preferredRomanization),
            .preferredName = legacyText(fields.preferredName),
            .roomNumber = legacyText(fields.roomNumber),
            .birthday = legacyText(fields.birthday),
            .phoneNumber = legacyText(fields.phoneNumber),
            .wifiName = legacyText(fields.wifiName),
            .wifiPassword = legacyText(fields.wifiPassword),
            .internetType = legacyText(fields.internetType),
            .zoomId = legacyText(fields.zoomId),
            .zoomPassword = legacyText(fields.zoomPassword),
            .projectionType = legacyText(fields.projectionType),
            .notes = legacyText(fields.notes)
        };
    }

    [[nodiscard]] static Domain::TeacherProfileFields profileFieldsFromRecord(
        const TeacherProfileReadRecord& teacher
        )
    {
        return {
            .teacherKr = teacher.teacherKr.toStdU16String(),
            .teacherEn = teacher.teacherEn.toStdU16String(),
            .preferredRomanization =
                teacher.preferredRomanization.toStdU16String(),
            .preferredName = teacher.preferredName.toStdU16String(),
            .roomNumber = teacher.roomNumber.toStdU16String(),
            .birthday = teacher.birthday.toStdU16String(),
            .phoneNumber = teacher.phoneNumber.toStdU16String(),
            .wifiName = teacher.wifiName.toStdU16String(),
            .wifiPassword = teacher.wifiPassword.toStdU16String(),
            .internetType = teacher.internetType.toStdU16String(),
            .zoomId = teacher.zoomId.toStdU16String(),
            .zoomPassword = teacher.zoomPassword.toStdU16String(),
            .projectionType = teacher.projectionType.toStdU16String(),
            .notes = teacher.notes.toStdU16String()
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "No active Teacher Profile database session is available.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError invalidTeacherIdError()
    {
        return {
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Teacher ID must be a canonical positive integer.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(
        const QString& message,
        const QString& fallback = QStringLiteral("Teacher Profile persistence failed.")
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = utf8(message.isEmpty() ? fallback : message),
            .recoverable = true
        };
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
