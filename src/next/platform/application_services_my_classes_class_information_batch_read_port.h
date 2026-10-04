#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/my_classes_class_information_batch_read_port.h"

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

class ApplicationServicesMyClassesClassInformationBatchReadPort final
    : public Application::MyClassesClassInformationBatchReadPort
{
public:
    explicit ApplicationServicesMyClassesClassInformationBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    explicit ApplicationServicesMyClassesClassInformationBatchReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    ApplicationServicesMyClassesClassInformationBatchReadPort(
        const ApplicationServicesMyClassesClassInformationBatchReadPort&
        ) = delete;
    ApplicationServicesMyClassesClassInformationBatchReadPort& operator=(
        const ApplicationServicesMyClassesClassInformationBatchReadPort&
        ) = delete;
    ApplicationServicesMyClassesClassInformationBatchReadPort(
        ApplicationServicesMyClassesClassInformationBatchReadPort&&
        ) = delete;
    ApplicationServicesMyClassesClassInformationBatchReadPort& operator=(
        ApplicationServicesMyClassesClassInformationBatchReadPort&&
        ) = delete;

    [[nodiscard]] Application::MyClassesClassInformationBatchReadResult
    readMyClassesClassInformationBatch(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        if (classIds.empty())
        {
            return Application::MyClassesClassInformationBatchReadResult::success(
                {}
                );
        }

        QList<int> legacyClassIds;
        legacyClassIds.reserve(static_cast<qsizetype>(classIds.size()));
        for (const Domain::ClassId& classId : classIds)
        {
            const std::optional<int> legacyClassId =
                legacyId(classId.value());
            if (!legacyClassId)
            {
                return Application::MyClassesClassInformationBatchReadResult::failure(
                    error(
                        Domain::ErrorCode::InvalidInput,
                        "My Classes class IDs must be canonical positive integers.",
                        false
                        )
                    );
            }
            legacyClassIds.append(*legacyClassId);
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::MyClassesClassInformationBatchReadResult::failure(
                error(
                    Domain::ErrorCode::NotFound,
                    "My Classes class information is unavailable.",
                    true
                    )
                );
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return Application::MyClassesClassInformationBatchReadResult::failure(
                error(
                    Domain::ErrorCode::NotFound,
                    "My Classes class information repository is unavailable.",
                    true
                    )
                );
        }

        Result<std::vector<MyClassesClassInformationBatchReadEntry>> loaded =
            [&]() -> Result<std::vector<MyClassesClassInformationBatchReadEntry>>
        {
            try
            {
                return repository->loadMyClassesClassInformationRecords(
                    legacyClassIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(QStringLiteral(
                    "My Classes class information could not be loaded."
                    ));
            }
            catch (...)
            {
                return std::unexpected(QStringLiteral(
                    "My Classes class information could not be loaded."
                    ));
            }
        }();

        if (!loaded)
        {
            return Application::MyClassesClassInformationBatchReadResult::failure(
                error(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error()),
                    true
                    )
                );
        }

        if (loaded->size() != classIds.size())
        {
            return Application::MyClassesClassInformationBatchReadResult::failure(
                error(
                    Domain::ErrorCode::Validation,
                    "My Classes class information returned an incomplete class list.",
                    false
                    )
                );
        }

        Application::MyClassesClassInformationBatchReadSnapshot entries;
        entries.reserve(classIds.size());
        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const MyClassesClassInformationBatchReadEntry& loadedEntry =
                loaded->at(index);
            if (loadedEntry.classId != legacyClassIds.at(
                    static_cast<qsizetype>(index)))
            {
                return Application::MyClassesClassInformationBatchReadResult::failure(
                    error(
                        Domain::ErrorCode::Validation,
                        "My Classes class information returned entries in a different identifier order.",
                        false
                        )
                    );
            }

            if (!loadedEntry.information)
            {
                entries.push_back({
                    .classId = classIds[index],
                    .information = Domain::Result<
                        Application::MyClassesClassInformationFields
                        >::failure(error(
                            Domain::ErrorCode::Technical,
                            toStdString(loadedEntry.information.error()),
                            true
                            ))
                });
                continue;
            }

            const MyClassesClassInformationReadRecord& source =
                loadedEntry.information.value();
            if (source.classId != legacyClassIds.at(
                    static_cast<qsizetype>(index)))
            {
                entries.push_back({
                    .classId = classIds[index],
                    .information = Domain::Result<
                        Application::MyClassesClassInformationFields
                        >::failure(error(
                            Domain::ErrorCode::Validation,
                            "My Classes class information returned a different class identifier.",
                            false
                            ))
                });
                continue;
            }

            Application::MyClassesClassInformationFields fields;
            fields.classGrade = source.classGrade.toStdU16String();
            fields.classLevel = source.classLevel.toStdU16String();
            fields.regularSchedule = schedule(source.regularTimes);
            fields.intensiveSchedule = schedule(source.intensiveTimes);
            fields.notes = source.notes.toStdU16String();
            fields.timeFillerActivities =
                source.timeFillerActivities.toStdU16String();
            if (source.teacherId > 0)
            {
                fields.teacherId = Domain::TeacherId::fromString(
                    std::to_string(source.teacherId)
                    );
            }

            entries.push_back({
                .classId = classIds[index],
                .information = Domain::Result<
                    Application::MyClassesClassInformationFields
                    >::success(std::move(fields))
            });
        }

        return Application::MyClassesClassInformationBatchReadResult::success(
            std::move(entries)
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

    [[nodiscard]] static std::vector<Application::MyClassesScheduleEntry>
    schedule(const QList<ClassTime>& times)
    {
        std::vector<Application::MyClassesScheduleEntry> entries;
        entries.reserve(static_cast<std::size_t>(times.size()));
        for (const ClassTime& time : times)
        {
            entries.push_back({
                .day = time.day.toStdU16String(),
                .startTime = time.startTime.toStdU16String(),
                .endTime = time.endTime.toStdU16String()
            });
        }
        return entries;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
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

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
