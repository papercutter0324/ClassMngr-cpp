#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "next/application/class_details_page_read_port.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Reads the selected class, its teacher label, and its roster count through
// the active ApplicationServices session. Source outcomes are kept separate
// so one failed lookup does not discard successful reads from the others.
class ApplicationServicesClassDetailsPageReadPort final
    : public Application::ClassDetailsPageReadPort
{
public:
    explicit ApplicationServicesClassDetailsPageReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassDetailsPageReadPort(
        const ApplicationServicesClassDetailsPageReadPort&
        ) = delete;
    ApplicationServicesClassDetailsPageReadPort& operator=(
        const ApplicationServicesClassDetailsPageReadPort&
        ) = delete;
    ApplicationServicesClassDetailsPageReadPort(
        ApplicationServicesClassDetailsPageReadPort&&
        ) = delete;
    ApplicationServicesClassDetailsPageReadPort& operator=(
        ApplicationServicesClassDetailsPageReadPort&&
        ) = delete;

    [[nodiscard]] Application::ClassDetailsPageReadResult
    readClassDetailsPage(
        const Domain::ClassId& classId
        ) override
    {
        const Domain::OperationError unavailable{
            .code = Domain::ErrorCode::NotFound,
            .message = "The class details page source is unavailable.",
            .recoverable = true
        };
        Application::ClassDetailsPageReadSnapshot snapshot{
            classId,
            Domain::Result<
                Application::ClassDetailsPageFields
                >::failure(unavailable),
            Domain::Result<std::string>::failure(unavailable),
            Domain::Result<int>::failure(unavailable)
        };

        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return Application::ClassDetailsPageReadResult::success(
                failedSnapshot(
                    classId,
                    Domain::ErrorCode::InvalidInput,
                    "Selected class identifier must be a canonical positive integer."
                    )
                );
        }

        DatabaseSession* session = m_services.databaseSession();
        if (!session || !session->isOpen())
        {
            return Application::ClassDetailsPageReadResult::success(
                std::move(snapshot)
                );
        }

        ClassService* classService = m_services.classService();
        RosterService* rosterService = m_services.rosterService();
        TeacherService* teacherService = m_services.teacherService();

        std::optional<ClassInfo> loadedClassInfo;
        if (!classService || !classService->isAvailable())
        {
            snapshot.classFields = classFieldsFailure(
                Domain::ErrorCode::NotFound,
                "The class details service is unavailable."
                );
            snapshot.teacherDisplayName = teacherFailure(
                Domain::ErrorCode::NotFound,
                "The class teacher could not be identified because class details are unavailable."
                );
        }
        else
        {
            try
            {
                const ::Result<ClassInfo> source =
                    classService->classInfo(*legacyClassId);
                if (!source)
                {
                    const Domain::OperationError error = fromLegacyError(
                        source.error(),
                        "The selected class details could not be loaded."
                        );
                    snapshot.classFields = classFieldsFailure(error);
                    snapshot.teacherDisplayName = teacherFailure(
                        Domain::ErrorCode::NotFound,
                        "The class teacher could not be identified because class details were not loaded."
                        );
                }
                else
                {
                    loadedClassInfo = *source;
                    snapshot.classFields =
                        Domain::Result<
                            Application::ClassDetailsPageFields
                            >::success(fields(*source));
                }
            }
            catch (const std::exception&)
            {
                snapshot.classFields = classFieldsFailure(
                    Domain::ErrorCode::Technical,
                    "The selected class details could not be loaded."
                    );
                snapshot.teacherDisplayName = teacherFailure(
                    Domain::ErrorCode::NotFound,
                    "The class teacher could not be identified because class details were not loaded."
                    );
            }
            catch (...)
            {
                snapshot.classFields = classFieldsFailure(
                    Domain::ErrorCode::Technical,
                    "The selected class details could not be loaded."
                    );
                snapshot.teacherDisplayName = teacherFailure(
                    Domain::ErrorCode::NotFound,
                    "The class teacher could not be identified because class details were not loaded."
                    );
            }
        }

        if (loadedClassInfo.has_value())
        {
            if (loadedClassInfo->teacherId <= 0)
            {
                snapshot.teacherDisplayName =
                    Domain::Result<std::string>::success({});
            }
            else if (!teacherService || !teacherService->isAvailable())
            {
                snapshot.teacherDisplayName = teacherFailure(
                    Domain::ErrorCode::NotFound,
                    "The class teacher service is unavailable."
                    );
            }
            else
            {
                try
                {
                    const ::Result<Teacher> source =
                        teacherService->teacher(loadedClassInfo->teacherId);
                    if (!source)
                    {
                        snapshot.teacherDisplayName = teacherFailure(
                            fromLegacyError(
                                source.error(),
                                "The class teacher display name could not be loaded."
                                )
                            );
                    }
                    else
                    {
                        snapshot.teacherDisplayName =
                            Domain::Result<std::string>::success(
                                utf8(source->preferredDisplayName())
                                );
                    }
                }
                catch (const std::exception&)
                {
                    snapshot.teacherDisplayName = teacherFailure(
                        Domain::ErrorCode::Technical,
                        "The class teacher display name could not be loaded."
                        );
                }
                catch (...)
                {
                    snapshot.teacherDisplayName = teacherFailure(
                        Domain::ErrorCode::Technical,
                        "The class teacher display name could not be loaded."
                        );
                }
            }
        }

        if (!rosterService || !rosterService->isAvailable())
        {
            snapshot.studentCount = studentCountFailure(
                Domain::ErrorCode::NotFound,
                "The class roster service is unavailable."
                );
        }
        else
        {
            try
            {
                const ::Result<int> source =
                    rosterService->studentCount(*legacyClassId);
                if (!source)
                {
                    snapshot.studentCount = studentCountFailure(
                        fromLegacyError(
                            source.error(),
                            "The class student count could not be loaded."
                            )
                        );
                }
                else
                {
                    snapshot.studentCount =
                        Domain::Result<int>::success(*source);
                }
            }
            catch (const std::exception&)
            {
                snapshot.studentCount = studentCountFailure(
                    Domain::ErrorCode::Technical,
                    "The class student count could not be loaded."
                    );
            }
            catch (...)
            {
                snapshot.studentCount = studentCountFailure(
                    Domain::ErrorCode::Technical,
                    "The class student count could not be loaded."
                    );
            }
        }

        return Application::ClassDetailsPageReadResult::success(
            std::move(snapshot)
            );
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(
        const std::string& value
        )
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{} || end != value.data() + value.size()
            || parsed <= 0 || std::to_string(parsed) != value)
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::ClassDetailsPageFields fields(
        const ClassInfo& source
        )
    {
        Application::ClassDetailsPageFields value;
        value.classGrade = utf8(source.classGrade);
        value.classLevel = utf8(source.classLevel);
        value.readingBook = utf8(source.readingBook);
        value.essayBook = utf8(source.essayBook);
        value.classColor = utf8(source.classColor);
        value.fontColor = utf8(source.fontColor);
        value.regularSchedule.reserve(
            static_cast<std::size_t>(source.classTimes.size())
            );
        value.intensiveSchedule.reserve(
            static_cast<std::size_t>(source.intensiveTimes.size())
            );
        for (const ClassTime& row : source.classTimes)
        {
            value.regularSchedule.push_back({
                utf8(row.day),
                utf8(row.startTime),
                utf8(row.endTime)
            });
        }
        for (const ClassTime& row : source.intensiveTimes)
        {
            value.intensiveSchedule.push_back({
                utf8(row.day),
                utf8(row.startTime),
                utf8(row.endTime)
            });
        }
        return value;
    }

    [[nodiscard]] static Domain::OperationError fromLegacyError(
        const QString& message,
        const char* fallback
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
        return {
            .code = code,
            .message = bytes.isEmpty() ? fallback : bytes.toStdString(),
            .recoverable = false
        };
    }

    template <typename Value>
    [[nodiscard]] static Domain::Result<Value> failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Domain::Result<Value>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Domain::Result<Application::ClassDetailsPageFields>
    classFieldsFailure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return failure<Application::ClassDetailsPageFields>(
            code,
            std::move(message)
            );
    }

    [[nodiscard]] static Domain::Result<Application::ClassDetailsPageFields>
    classFieldsFailure(
        Domain::OperationError error
        )
    {
        return Domain::Result<Application::ClassDetailsPageFields>::failure(
            std::move(error)
            );
    }

    [[nodiscard]] static Domain::Result<std::string> teacherFailure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return failure<std::string>(code, std::move(message));
    }

    [[nodiscard]] static Domain::Result<std::string> teacherFailure(
        Domain::OperationError error
        )
    {
        return Domain::Result<std::string>::failure(std::move(error));
    }

    [[nodiscard]] static Domain::Result<int> studentCountFailure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return failure<int>(code, std::move(message));
    }

    [[nodiscard]] static Domain::Result<int> studentCountFailure(
        Domain::OperationError error
        )
    {
        return Domain::Result<int>::failure(std::move(error));
    }

    [[nodiscard]] static Application::ClassDetailsPageReadSnapshot
    failedSnapshot(
        const Domain::ClassId& classId,
        const Domain::ErrorCode code,
        const char* message
        )
    {
        return {
            classId,
            classFieldsFailure(code, message),
            teacherFailure(code, message),
            studentCountFailure(code, message)
        };
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
