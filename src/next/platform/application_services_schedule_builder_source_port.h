#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/schedule_builder_source_snapshot.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesScheduleBuilderSourcePort final
    : public Application::ScheduleBuilderSourceReadPort
{
public:
    explicit ApplicationServicesScheduleBuilderSourcePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesScheduleBuilderSourcePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ScheduleBuilderSourceResult
    readScheduleClasses(
        const Application::ScheduleBuilderSourceQuery&
        ) const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        try
        {
            ClassInfoRepository* const repository =
                session->classInfoRepository();
            if (!repository)
            {
                return unavailableFailure();
            }

            const Result<QList<ClassInfo>> loaded =
                repository->loadScheduleClassInfos();
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            Application::ScheduleBuilderSourceSnapshot snapshot;
            snapshot.classes.reserve(static_cast<std::size_t>(loaded->size()));
            for (const ClassInfo& info : *loaded)
            {
                const std::optional<Domain::ClassId> classId =
                    canonicalClassId(info.classId);
                if (!classId)
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The schedule class source returned an invalid class ID."
                        );
                }

                Application::ScheduleBuilderSourceClass entry{
                    .classId = *classId,
                    .teacherKoreanName = info.teacherKr.toStdU16String(),
                    .teacherEnglishName = info.teacherEn.toStdU16String(),
                    .teacherPreferredName =
                        info.teacherPreferredName.toStdU16String(),
                    .roomNumber = info.roomNumber.toStdU16String(),
                    .grade = info.classGrade.toStdU16String(),
                    .level = info.classLevel.toStdU16String(),
                    .classColor = info.classColor.toStdU16String(),
                    .fontColor = info.fontColor.toStdU16String(),
                    .regularSchedule = scheduleRows(info.classTimes),
                    .intensiveSchedule = scheduleRows(info.intensiveTimes)
                };
                snapshot.classes.push_back(std::move(entry));
            }

            return Application::ScheduleBuilderSourceResult::success(
                std::move(snapshot)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Schedule classes could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Schedule classes could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<Domain::ClassId> canonicalClassId(
        const int value
        )
    {
        if (value <= 0)
        {
            return std::nullopt;
        }

        const std::string text = std::to_string(value);
        return Domain::ClassId::fromString(text);
    }

    [[nodiscard]] static std::vector<
        Application::ScheduleBuilderSourceScheduleRow
        > scheduleRows(const QList<ClassTime>& times)
    {
        std::vector<Application::ScheduleBuilderSourceScheduleRow> result;
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

    [[nodiscard]] static Application::ScheduleBuilderSourceResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Schedule classes could not be loaded.";
        }

        return Application::ScheduleBuilderSourceResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Validation
        });
    }

    [[nodiscard]] static Application::ScheduleBuilderSourceResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for schedule classes is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
