#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/selected_class_subtitle_read_port.h"

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

class ApplicationServicesSelectedClassSubtitleReadPort final
    : public Application::SelectedClassSubtitleReadPort
{
public:
    explicit ApplicationServicesSelectedClassSubtitleReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::SelectedClassSubtitleReadResult
    readSelectedClassSubtitle(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return Application::SelectedClassSubtitleReadResult::failure(error(
                Domain::ErrorCode::InvalidInput,
                "Selected class ID must be a canonical positive integer."
                ));
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::SelectedClassSubtitleReadResult::failure(
                unavailableError()
                );
        }

        ClassInfoRepository* const classRepository =
            session->classInfoRepository();
        if (!classRepository)
        {
            return Application::SelectedClassSubtitleReadResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The selected class subtitle repository is unavailable."
                ));
        }

        Result<ClassSubtitleReadOutcome> loadedClass = [&]()
            -> Result<ClassSubtitleReadOutcome>
        {
            try
            {
                return classRepository->loadClassSubtitleRecordDetailed(
                    *legacyClassId
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    QStringLiteral("The selected class details could not be loaded.")
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    QStringLiteral("The selected class details could not be loaded.")
                    );
            }
        }();

        if (!loadedClass)
        {
            Application::SelectedClassSubtitleReadSnapshot snapshot{
                classId,
                Domain::Result<
                    Application::SelectedClassSubtitleFields
                    >::failure(classFieldsRepositoryError(
                        loadedClass.error()
                        )),
                Domain::Result<std::optional<
                    Application::SelectedClassSubtitleTeacherFields
                    >>::success(std::nullopt)
            };
            return Application::SelectedClassSubtitleReadResult::success(
                std::move(snapshot)
                );
        }

        if (loadedClass->record.classId != *legacyClassId)
        {
            return Application::SelectedClassSubtitleReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "The selected class subtitle repository returned a different class identifier."
                ));
        }

        if (loadedClass->parentMissing)
        {
            const Domain::OperationError missingClass = error(
                Domain::ErrorCode::NotFound,
                "The selected class no longer exists."
                );
            Application::SelectedClassSubtitleReadSnapshot snapshot{
                classId,
                Domain::Result<
                    Application::SelectedClassSubtitleFields
                    >::failure(missingClass),
                Domain::Result<std::optional<
                    Application::SelectedClassSubtitleTeacherFields
                    >>::failure(missingClass)
            };
            return Application::SelectedClassSubtitleReadResult::success(
                std::move(snapshot)
                );
        }

        Application::SelectedClassSubtitleFields classFields;
        classFields.classGrade = loadedClass->record.grade.toStdU16String();
        classFields.classLevel = loadedClass->record.level.toStdU16String();
        classFields.regularSchedule.reserve(
            static_cast<std::size_t>(loadedClass->record.regularTimes.size())
            );
        for (const ClassTime& row : loadedClass->record.regularTimes)
        {
            classFields.regularSchedule.push_back({
                row.day.toStdU16String(),
                row.startTime.toStdU16String()
            });
        }

        Domain::Result<std::optional<
            Application::SelectedClassSubtitleTeacherFields
            >> assignedTeacher =
                Domain::Result<std::optional<
                    Application::SelectedClassSubtitleTeacherFields
                    >>::success(std::nullopt);

        if (loadedClass->record.teacherId > 0)
        {
            TeacherRepository* const teacherRepository =
                session->teacherRepository();
            if (!teacherRepository)
            {
                assignedTeacher = Domain::Result<std::optional<
                    Application::SelectedClassSubtitleTeacherFields
                    >>::failure(error(
                        Domain::ErrorCode::NotFound,
                        "The assigned teacher display name repository is unavailable."
                        ));
            }
            else
            {
                TeacherDisplayNameLookupResult loadedTeacher = [&]()
                    -> TeacherDisplayNameLookupResult
                {
                    try
                    {
                        return teacherRepository->lookupTeacherDisplayName(
                            loadedClass->record.teacherId
                            );
                    }
                    catch (const std::exception&)
                    {
                        return {
                            .code = TeacherDisplayNameLookupCode::Failure,
                            .diagnostic = QStringLiteral(
                                "The assigned teacher display name could not be loaded."
                                )
                        };
                    }
                    catch (...)
                    {
                        return {
                            .code = TeacherDisplayNameLookupCode::Failure,
                            .diagnostic = QStringLiteral(
                                "The assigned teacher display name could not be loaded."
                                )
                        };
                    }
                }();

                if (loadedTeacher.code != TeacherDisplayNameLookupCode::Found)
                {
                    assignedTeacher = Domain::Result<std::optional<
                        Application::SelectedClassSubtitleTeacherFields
                        >>::failure(
                            fromTeacherDisplayNameLookup(loadedTeacher)
                            );
                }
                else
                {
                    Application::SelectedClassSubtitleTeacherFields fields{
                        loadedTeacher.record.teacherKr.toStdU16String(),
                        loadedTeacher.record.teacherEn.toStdU16String(),
                        loadedTeacher.record.preferredRomanization
                            .toStdU16String(),
                        loadedTeacher.record.preferredName.toStdU16String()
                    };
                    assignedTeacher = Domain::Result<std::optional<
                        Application::SelectedClassSubtitleTeacherFields
                        >>::success(std::move(fields));
                }
            }
        }

        Application::SelectedClassSubtitleReadSnapshot snapshot{
            classId,
            Domain::Result<
                Application::SelectedClassSubtitleFields
                >::success(std::move(classFields)),
            std::move(assignedTeacher)
        };
        return Application::SelectedClassSubtitleReadResult::success(
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
        const auto [end, conversionError] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (conversionError != std::errc{}
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
            "The active database session for the selected class subtitle is unavailable."
            );
    }

    [[nodiscard]] static Domain::OperationError classFieldsRepositoryError(
        const QString& message
        )
    {
        const bool invalidPersistedTeacherId = message.toLower().contains(
            QStringLiteral("database returned an invalid teacher identifier")
            );
        const QByteArray bytes = message.toUtf8();
        return error(
            invalidPersistedTeacherId
                ? Domain::ErrorCode::Validation
                : Domain::ErrorCode::Technical,
            bytes.isEmpty()
                ? "The selected class subtitle source failed."
                : bytes.toStdString()
            );
    }

    [[nodiscard]] static Domain::OperationError fromTeacherDisplayNameLookup(
        const TeacherDisplayNameLookupResult& lookup
        )
    {
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        switch (lookup.code)
        {
        case TeacherDisplayNameLookupCode::Missing:
            code = Domain::ErrorCode::NotFound;
            break;
        case TeacherDisplayNameLookupCode::InvalidInput:
            code = Domain::ErrorCode::InvalidInput;
            break;
        case TeacherDisplayNameLookupCode::Found:
        case TeacherDisplayNameLookupCode::Failure:
            code = Domain::ErrorCode::Technical;
            break;
        }

        const QByteArray bytes = lookup.diagnostic.toUtf8();
        return error(
            code,
            bytes.isEmpty()
                ? "The assigned teacher display name could not be loaded."
                : bytes.toStdString()
            );
    }

    ApplicationServices* m_services;
};

} // namespace ClassMngr::Next::Platform
