#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/classes_navigation_snapshot.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassesNavigationReadPort final
    : public Application::ClassesNavigationSnapshotReadPort
{
public:
    explicit ApplicationServicesClassesNavigationReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassesNavigationReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassesNavigationReadPort(
        const ApplicationServicesClassesNavigationReadPort&
        ) = delete;
    ApplicationServicesClassesNavigationReadPort& operator=(
        const ApplicationServicesClassesNavigationReadPort&
        ) = delete;
    ApplicationServicesClassesNavigationReadPort(
        ApplicationServicesClassesNavigationReadPort&&
        ) = delete;
    ApplicationServicesClassesNavigationReadPort& operator=(
        ApplicationServicesClassesNavigationReadPort&&
        ) = delete;

    [[nodiscard]] Application::ClassesNavigationSnapshotResult readClasses(
        const Application::ClassesNavigationSnapshotQuery& query
        ) const override
    {
        std::vector<int> legacyIds;
        legacyIds.reserve(query.classes.size());
        for (const Application::ClassesNavigationClass& source : query.classes)
        {
            const std::optional<int> classId = legacyClassId(
                source.classId.value()
                );
            if (!classId)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be canonical positive integers."
                    );
            }
            legacyIds.push_back(*classId);
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        ClassInfoRepository* const repository = session->classInfoRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        try
        {
            QList<int> classIds;
            classIds.reserve(static_cast<qsizetype>(legacyIds.size()));
            for (const int classId : legacyIds)
            {
                classIds.append(classId);
            }

            const Result<QList<ClassNavigationReadRecord>> loaded =
                repository->loadClassesNavigationRecords(classIds);
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            std::unordered_map<int, const ClassNavigationReadRecord*>
                recordsById;
            recordsById.reserve(
                static_cast<std::size_t>(loaded->size())
                );
            for (const ClassNavigationReadRecord& record : *loaded)
            {
                if (!recordsById.emplace(record.classId, &record).second)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The classes navigation source returned duplicate class IDs."
                        );
                }
            }

            Application::ClassesNavigationSnapshot snapshot;
            snapshot.classes.reserve(query.classes.size());
            for (std::size_t index = 0; index < query.classes.size(); ++index)
            {
                const Application::ClassesNavigationClass& requested =
                    query.classes[index];
                Application::ClassesNavigationEntry entry{
                    .classId = requested.classId,
                    .className = requested.className
                };

                const auto record = recordsById.find(legacyIds[index]);
                if (record == recordsById.end())
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The classes navigation source omitted a requested class."
                        );
                }

                const ClassNavigationReadRecord& metadata = *record->second;
                if (metadata.hasClassInfo)
                {
                    entry.grade = metadata.grade.toStdU16String();
                    entry.level = metadata.level.toStdU16String();
                    entry.teacherEnglishName =
                        metadata.teacherEnglishName.toStdU16String();
                    entry.teacherKoreanName =
                        metadata.teacherKoreanName.toStdU16String();
                    entry.regularSchedule = scheduleRows(
                        metadata.regularTimes
                        );
                    entry.intensiveSchedule = scheduleRows(
                        metadata.intensiveTimes
                        );
                }

                snapshot.classes.push_back(std::move(entry));
            }

            return Application::ClassesNavigationSnapshotResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Classes navigation metadata could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Classes navigation metadata could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassId(
        const std::string& value
        )
    {
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

    [[nodiscard]] static std::vector<Application::ClassesNavigationScheduleRow>
    scheduleRows(const QList<ClassTime>& times)
    {
        std::vector<Application::ClassesNavigationScheduleRow> result;
        result.reserve(static_cast<std::size_t>(times.size()));
        for (const ClassTime& time : times)
        {
            result.push_back({
                .day = time.day.toStdU16String(),
                .startTime = time.startTime.toStdU16String(),
                .endTime = time.endTime.toStdU16String()
            });
        }
        return result;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::ClassesNavigationSnapshotResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Classes navigation metadata could not be loaded.";
        }

        return Application::ClassesNavigationSnapshotResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::InvalidInput
        });
    }

    [[nodiscard]] static Application::ClassesNavigationSnapshotResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for classes navigation is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
