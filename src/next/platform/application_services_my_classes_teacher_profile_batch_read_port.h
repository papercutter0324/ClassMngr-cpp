#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/my_classes_teacher_profile_batch_read_port.h"
#include "next/application/positive_decimal_id_validation.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesMyClassesTeacherProfileBatchReadPort final
    : public Application::MyClassesTeacherProfileBatchReadPort
{
public:
    explicit ApplicationServicesMyClassesTeacherProfileBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    explicit ApplicationServicesMyClassesTeacherProfileBatchReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    ApplicationServicesMyClassesTeacherProfileBatchReadPort(
        const ApplicationServicesMyClassesTeacherProfileBatchReadPort&
        ) = delete;
    ApplicationServicesMyClassesTeacherProfileBatchReadPort& operator=(
        const ApplicationServicesMyClassesTeacherProfileBatchReadPort&
        ) = delete;
    ApplicationServicesMyClassesTeacherProfileBatchReadPort(
        ApplicationServicesMyClassesTeacherProfileBatchReadPort&&
        ) = delete;
    ApplicationServicesMyClassesTeacherProfileBatchReadPort& operator=(
        ApplicationServicesMyClassesTeacherProfileBatchReadPort&&
        ) = delete;

    [[nodiscard]] Application::MyClassesTeacherProfileBatchReadResult
    readMyClassesTeacherProfiles(
        const std::vector<Domain::TeacherId>& teacherIds
        ) const override
    {
        if (teacherIds.empty())
        {
            return Application::MyClassesTeacherProfileBatchReadResult::success(
                {}
                );
        }

        QList<qint64> legacyTeacherIds;
        legacyTeacherIds.reserve(
            static_cast<qsizetype>(teacherIds.size())
            );
        std::unordered_set<qint64> seenTeacherIds;
        for (const Domain::TeacherId& teacherId : teacherIds)
        {
            const auto legacyId =
                Application::parseCanonicalPositiveDecimalId64(
                    teacherId.value()
                    );
            if (!legacyId || !seenTeacherIds.insert(*legacyId).second)
            {
                return Application::MyClassesTeacherProfileBatchReadResult::failure(
                    error(
                        Domain::ErrorCode::InvalidInput,
                        "My Classes teacher IDs must be canonical positive integers.",
                        false
                        )
                    );
            }
            legacyTeacherIds.append(*legacyId);
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failedEntries(
                teacherIds,
                error(
                    Domain::ErrorCode::Technical,
                    "No active Teacher Profile database session is available.",
                    false
                    )
                );
        }

        TeacherRepository* const repository = session->teacherRepository();
        if (!repository)
        {
            return failedEntries(
                teacherIds,
                error(
                    Domain::ErrorCode::Technical,
                    "The Teacher Profile repository is unavailable.",
                    false
                    )
                );
        }

        Result<QList<MyClassesTeacherProfileBatchReadRecord>> loaded = [&]()
            -> Result<QList<MyClassesTeacherProfileBatchReadRecord>>
        {
            try
            {
                return repository->loadMyClassesTeacherProfileRecords(
                    legacyTeacherIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(QStringLiteral(
                    "Assigned teacher profiles could not be loaded."
                    ));
            }
            catch (...)
            {
                return std::unexpected(QStringLiteral(
                    "Assigned teacher profiles could not be loaded."
                    ));
            }
        }();

        if (!loaded)
        {
            return failedEntries(teacherIds, repositoryError(loaded.error()));
        }

        if (loaded->size() != teacherIds.size())
        {
            return failedEntries(
                teacherIds,
                error(
                    Domain::ErrorCode::Validation,
                    "The Teacher Profile repository returned an incomplete teacher list.",
                    false
                    )
                );
        }

        Application::MyClassesTeacherProfileBatchReadSnapshot entries;
        entries.reserve(teacherIds.size());
        for (std::size_t index = 0; index < teacherIds.size(); ++index)
        {
            const MyClassesTeacherProfileBatchReadRecord& source = loaded->at(
                static_cast<qsizetype>(index)
                );
            if (source.requestedTeacherId != legacyTeacherIds.at(
                    static_cast<qsizetype>(index)))
            {
                entries.push_back({
                    .teacherId = teacherIds[index],
                    .profile = Domain::Result<
                        Application::MyClassesTeacherProfileFields
                        >::failure(error(
                            Domain::ErrorCode::Validation,
                            "The My Classes Teacher Profile repository returned records in a different identifier order.",
                            false
                            ))
                });
                continue;
            }

            switch (source.code)
            {
            case MyClassesTeacherProfileReadCode::Missing:
                entries.push_back({
                    .teacherId = teacherIds[index],
                    .profile = Domain::Result<
                        Application::MyClassesTeacherProfileFields
                        >::failure(error(
                            Domain::ErrorCode::NotFound,
                            repositoryDiagnostic(
                                source.diagnostic,
                                "The assigned teacher record was not found."
                                ),
                            true
                            ))
                });
                continue;
            case MyClassesTeacherProfileReadCode::Invalid:
                entries.push_back({
                    .teacherId = teacherIds[index],
                    .profile = Domain::Result<
                        Application::MyClassesTeacherProfileFields
                        >::failure(error(
                            Domain::ErrorCode::Validation,
                            repositoryDiagnostic(
                                source.diagnostic,
                                "The My Classes Teacher Profile repository returned an invalid record."
                                ),
                            false
                            ))
                });
                continue;
            case MyClassesTeacherProfileReadCode::Failure:
                entries.push_back({
                    .teacherId = teacherIds[index],
                    .profile = Domain::Result<
                        Application::MyClassesTeacherProfileFields
                        >::failure(repositoryError(source.diagnostic))
                });
                continue;
            case MyClassesTeacherProfileReadCode::Found:
                break;
            }

            if (source.profile.teacherId != legacyTeacherIds.at(
                    static_cast<qsizetype>(index)))
            {
                entries.push_back({
                    .teacherId = teacherIds[index],
                    .profile = Domain::Result<
                        Application::MyClassesTeacherProfileFields
                        >::failure(error(
                            Domain::ErrorCode::Validation,
                            "The My Classes Teacher Profile repository returned a different teacher identifier.",
                            false
                            ))
                });
                continue;
            }

            entries.push_back({
                .teacherId = teacherIds[index],
                .profile = Domain::Result<
                    Application::MyClassesTeacherProfileFields
                    >::success(profileFieldsFromReadFields(source.profile))
            });
        }

        return Application::MyClassesTeacherProfileBatchReadResult::success(
            std::move(entries)
            );
    }

private:
    [[nodiscard]] static Application::MyClassesTeacherProfileFields
    profileFieldsFromReadFields(
        const MyClassesTeacherProfileReadFields& fields
        )
    {
        return {
            .teacherKr = fields.teacherKr.toStdU16String(),
            .teacherEn = fields.teacherEn.toStdU16String(),
            .preferredRomanization =
                fields.preferredRomanization.toStdU16String(),
            .preferredName = fields.preferredName.toStdU16String(),
            .roomNumber = fields.roomNumber.toStdU16String(),
            .wifiName = fields.wifiName.toStdU16String(),
            .wifiPassword = fields.wifiPassword.toStdU16String(),
            .internetType = fields.internetType.toStdU16String(),
            .zoomId = fields.zoomId.toStdU16String(),
            .zoomPassword = fields.zoomPassword.toStdU16String(),
            .projectionType = fields.projectionType.toStdU16String(),
            .notes = fields.notes.toStdU16String()
        };
    }

    [[nodiscard]] static Domain::OperationError error(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable
        )
    {
        return {
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const QString& message
        )
    {
        const QByteArray bytes = message.toUtf8();
        return error(
            Domain::ErrorCode::Technical,
            bytes.isEmpty()
                ? "Teacher profile could not be loaded."
                : bytes.toStdString(),
            false
            );
    }

    [[nodiscard]] static std::string repositoryDiagnostic(
        const QString& message,
        const char* fallback
        )
    {
        const QByteArray bytes = message.toUtf8();
        return bytes.isEmpty() ? fallback : bytes.toStdString();
    }

    [[nodiscard]] static Application::MyClassesTeacherProfileBatchReadResult
    failedEntries(
        const std::vector<Domain::TeacherId>& teacherIds,
        const Domain::OperationError& failure
        )
    {
        Application::MyClassesTeacherProfileBatchReadSnapshot entries;
        entries.reserve(teacherIds.size());
        for (const Domain::TeacherId& teacherId : teacherIds)
        {
            entries.push_back({
                .teacherId = teacherId,
                .profile = Domain::Result<
                    Application::MyClassesTeacherProfileFields
                    >::failure(failure)
            });
        }
        return Application::MyClassesTeacherProfileBatchReadResult::success(
            std::move(entries)
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
