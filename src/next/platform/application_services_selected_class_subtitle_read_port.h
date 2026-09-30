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

        Result<ClassSubtitleReadRecord> loadedClass = [&]()
            -> Result<ClassSubtitleReadRecord>
        {
            try
            {
                return classRepository->loadClassSubtitleRecord(*legacyClassId);
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
                    >::failure(legacyError(loadedClass.error())),
                Domain::Result<std::optional<
                    Application::SelectedClassSubtitleTeacherFields
                    >>::success(std::nullopt)
            };
            return Application::SelectedClassSubtitleReadResult::success(
                std::move(snapshot)
                );
        }

        if (loadedClass->classId != *legacyClassId)
        {
            return Application::SelectedClassSubtitleReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "The selected class subtitle repository returned a different class identifier."
                ));
        }

        Application::SelectedClassSubtitleFields classFields;
        classFields.classGrade = loadedClass->grade.toStdU16String();
        classFields.classLevel = loadedClass->level.toStdU16String();
        classFields.regularSchedule.reserve(
            static_cast<std::size_t>(loadedClass->regularTimes.size())
            );
        for (const ClassTime& row : loadedClass->regularTimes)
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

        if (loadedClass->teacherId > 0)
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
                Result<TeacherDisplayNameReadRecord> loadedTeacher = [&]()
                    -> Result<TeacherDisplayNameReadRecord>
                {
                    try
                    {
                        return teacherRepository->loadTeacherDisplayNameFields(
                            loadedClass->teacherId
                            );
                    }
                    catch (const std::exception&)
                    {
                        return std::unexpected(
                            QStringLiteral("The assigned teacher display name could not be loaded.")
                            );
                    }
                    catch (...)
                    {
                        return std::unexpected(
                            QStringLiteral("The assigned teacher display name could not be loaded.")
                            );
                    }
                }();

                if (!loadedTeacher)
                {
                    assignedTeacher = Domain::Result<std::optional<
                        Application::SelectedClassSubtitleTeacherFields
                        >>::failure(legacyError(loadedTeacher.error()));
                }
                else
                {
                    Application::SelectedClassSubtitleTeacherFields fields{
                        loadedTeacher->teacherKr.toStdU16String(),
                        loadedTeacher->teacherEn.toStdU16String(),
                        loadedTeacher->preferredRomanization.toStdU16String(),
                        loadedTeacher->preferredName.toStdU16String()
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

    [[nodiscard]] static Domain::OperationError legacyError(
        const QString& message
        )
    {
        const QString normalized = message.toLower();
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        if (normalized.contains(QStringLiteral("not found"))
            || normalized.contains(QStringLiteral("no matching record"))
            || normalized.contains(QStringLiteral("unavailable")))
        {
            code = Domain::ErrorCode::NotFound;
        }
        else if (normalized.contains(QStringLiteral("invalid")))
        {
            code = Domain::ErrorCode::InvalidInput;
        }
        else if (normalized.contains(QStringLiteral("validation")))
        {
            code = Domain::ErrorCode::Validation;
        }

        const QByteArray bytes = message.toUtf8();
        return error(
            code,
            bytes.isEmpty()
                ? "The selected class subtitle source failed."
                : bytes.toStdString()
            );
    }

    ApplicationServices* m_services;
};

} // namespace ClassMngr::Next::Platform
