#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/schedule_builder_source_snapshot.h"

#include <QByteArray>
#include <QString>

#include <exception>
#include <optional>
#include <string>
#include <unordered_map>
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
        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return unavailableFailure();
        }

        try
        {
            ClassInfoRepository* const repository =
                store->classInfoRepository();
            if (!repository)
            {
                return unavailableFailure();
            }

            Application::ScheduleBuilderSourceSnapshot snapshot;
            std::unordered_map<int, std::size_t> indexesByClassId;
            std::optional<std::string> validationFailure;
            const Status loaded = repository->visitScheduleClassInfoRows(
                [&](const ClassInfo& info) -> Status
                {
                    const std::optional<Domain::ClassId> classId =
                        canonicalClassId(info.classId);
                    if (!classId)
                    {
                        validationFailure = "The schedule class source returned "
                            "an invalid class ID.";
                        return std::unexpected(QString::fromStdString(
                            *validationFailure
                            ));
                    }

                    const std::size_t classIndex = snapshot.classes.size();
                    indexesByClassId.emplace(info.classId, classIndex);
                    snapshot.classes.push_back({
                        .classId = *classId,
                        .teacherKoreanName = info.teacherKr.toStdU16String(),
                        .teacherEnglishName = info.teacherEn.toStdU16String(),
                        .teacherPreferredName =
                            info.teacherPreferredName.toStdU16String(),
                        .roomNumber = info.roomNumber.toStdU16String(),
                        .grade = info.classGrade.toStdU16String(),
                        .level = info.classLevel.toStdU16String(),
                        .classColor = info.classColor.toStdU16String(),
                        .fontColor = info.fontColor.toStdU16String()
                    });
                    return {};
                },
                [&](
                    const int classId,
                    const ClassTime& time,
                    const bool intensive
                    ) -> Status
                {
                    const auto index = indexesByClassId.find(classId);
                    if (index == indexesByClassId.end())
                    {
                        return {};
                    }

                    auto& schedule = intensive
                        ? snapshot.classes[index->second].intensiveSchedule
                        : snapshot.classes[index->second].regularSchedule;
                    schedule.push_back({
                        .day = Application::ScheduleBuilderSourceScheduleText(
                            time.day.toStdU16String()
                            ),
                        .startTime =
                            Application::ScheduleBuilderSourceScheduleText(
                                time.startTime.toStdU16String()
                                ),
                        .endTime = Application::ScheduleBuilderSourceScheduleText(
                            time.endTime.toStdU16String()
                            )
                    });
                    return {};
                }
                );
            if (!loaded)
            {
                return failure(
                    validationFailure
                        ? Domain::ErrorCode::Validation
                        : Domain::ErrorCode::Technical,
                    validationFailure.value_or(toStdString(loaded.error()))
                    );
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
            "The active workspace for schedule classes is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
