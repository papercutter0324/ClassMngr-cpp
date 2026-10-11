#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/my_classes_class_summary_batch_read_port.h"

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

class ApplicationServicesMyClassesClassSummaryBatchReadPort final
    : public Application::MyClassesClassSummaryBatchReadPort
{
public:
    explicit ApplicationServicesMyClassesClassSummaryBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::MyClassesClassSummaryBatchReadResult
    readMyClassesClassSummaries(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        if (classIds.empty())
        {
            return Application::MyClassesClassSummaryBatchReadResult::success(
                {}
                );
        }

        QList<int> legacyClassIds;
        legacyClassIds.reserve(static_cast<qsizetype>(classIds.size()));
        for (const Domain::ClassId& classId : classIds)
        {
            const std::optional<int> legacyClassId = legacyId(classId.value());
            if (!legacyClassId)
            {
                return Application::MyClassesClassSummaryBatchReadResult::failure(
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
            return Application::MyClassesClassSummaryBatchReadResult::failure(
                error(
                    Domain::ErrorCode::NotFound,
                    "My Classes class summaries are unavailable.",
                    true
                    )
                );
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return Application::MyClassesClassSummaryBatchReadResult::failure(
                error(
                    Domain::ErrorCode::NotFound,
                    "My Classes class-summary repository is unavailable.",
                    true
                    )
                );
        }

        Result<std::vector<MyClassesClassSummaryBatchReadEntry>> loaded =
            [&]() -> Result<std::vector<MyClassesClassSummaryBatchReadEntry>>
        {
            try
            {
                return repository->loadMyClassesClassSummaryRecords(
                    legacyClassIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(QStringLiteral(
                    "My Classes class summaries could not be loaded."
                    ));
            }
            catch (...)
            {
                return std::unexpected(QStringLiteral(
                    "My Classes class summaries could not be loaded."
                    ));
            }
        }();

        if (!loaded)
        {
            return Application::MyClassesClassSummaryBatchReadResult::failure(
                error(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error()),
                    true
                    )
                );
        }

        if (loaded->size() != classIds.size())
        {
            return Application::MyClassesClassSummaryBatchReadResult::failure(
                error(
                    Domain::ErrorCode::Validation,
                    "My Classes class summaries returned an incomplete class list.",
                    false
                    )
                );
        }

        std::vector<Application::MyClassesClassSummaryBatchReadEntry> entries;
        entries.reserve(classIds.size());
        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const MyClassesClassSummaryBatchReadEntry& source = loaded->at(index);
            if (source.classId != legacyClassIds.at(
                    static_cast<qsizetype>(index)))
            {
                entries.push_back({
                    .classId = classIds[index],
                    .summary = Domain::Result<
                        Application::MyClassesClassSummaryFields
                        >::failure(error(
                            Domain::ErrorCode::Validation,
                            "My Classes class summaries returned records in a different identifier order.",
                            false
                            ))
                });
                continue;
            }

            if (!source.summary)
            {
                entries.push_back({
                    .classId = classIds[index],
                    .summary = Domain::Result<
                        Application::MyClassesClassSummaryFields
                        >::failure(error(
                            Domain::ErrorCode::Technical,
                            toStdString(source.summary.error()),
                            true
                            ))
                });
                continue;
            }

            const MyClassesClassSummaryReadRecord& record =
                source.summary.value();
            if (record.classId != legacyClassIds.at(
                    static_cast<qsizetype>(index)))
            {
                entries.push_back({
                    .classId = classIds[index],
                    .summary = Domain::Result<
                        Application::MyClassesClassSummaryFields
                        >::failure(error(
                            Domain::ErrorCode::Validation,
                            "My Classes class summaries returned a different class identifier.",
                            false
                            ))
                });
                continue;
            }

            Application::MyClassesClassSummaryFields fields;
            fields.classGrade = record.classGrade.toStdU16String();
            fields.classLevel = record.classLevel.toStdU16String();
            fields.regularSchedule = schedule(record.regularTimes);
            fields.intensiveSchedule = schedule(record.intensiveTimes);
            if (record.teacherId > 0)
            {
                fields.teacherId = Domain::TeacherId::fromString(
                    std::to_string(record.teacherId)
                    );
            }

            entries.push_back({
                .classId = classIds[index],
                .summary = Domain::Result<
                    Application::MyClassesClassSummaryFields
                    >::success(std::move(fields))
            });
        }

        return Application::MyClassesClassSummaryBatchReadResult::success(
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
