#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/my_workspace_schedule_read.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QTime>

#include <algorithm>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

inline constexpr std::size_t kMyWorkspaceScheduleMaximumClasses = 4'096;
inline constexpr std::size_t kMyWorkspaceScheduleMaximumRowsPerClass = 64;

// Reads the currently open WorkspaceStore and copies regular meeting rows
// into the application-owned ScheduleViewProjection. The result is capped
// before it is materialized for the page.
class WorkspaceStoreMyWorkspaceScheduleReadPort final
    : public Application::MyWorkspaceScheduleReadPort
{
public:
    explicit WorkspaceStoreMyWorkspaceScheduleReadPort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::MyWorkspaceScheduleReadResult readSchedule()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return unavailable();
        }
        ClassInfoRepository* const repository = m_store->classInfoRepository();
        if (!repository)
        {
            return unavailable();
        }

        try
        {
            std::unordered_map<int, ClassMetadata> classes;
            classes.reserve(256);
            std::unordered_map<int, std::size_t> rowsPerClass;
            std::vector<PendingRow> rows;
            rows.reserve(128);
            const RepositoryStatus visited = repository->visitScheduleClassInfoRows(
                [&](const ClassInfo& info) -> RepositoryStatus
                {
                    if (classes.size() >= kMyWorkspaceScheduleMaximumClasses)
                    {
                        return repositoryFailure(
                            QStringLiteral(
                                "The schedule class list exceeds its bounded capacity."
                                )
                            );
                    }
                    if (info.classId <= 0 || classes.contains(info.classId))
                    {
                        return repositoryFailure(
                            QStringLiteral(
                                "The schedule class source returned an invalid or duplicate class ID."
                                )
                            );
                    }

                    const std::optional<std::string> grade = boundedUtf8(
                        info.classGrade,
                        Application::kScheduleViewMaxMeetingTextLength,
                        false
                        );
                    const std::optional<std::string> level = boundedUtf8(
                        info.classLevel,
                        Application::kScheduleViewMaxMeetingTextLength,
                        false
                        );
                    const std::optional<std::string> room = boundedUtf8(
                        info.roomNumber,
                        Application::kScheduleViewMaxRoomTextLength,
                        false
                        );
                    const std::optional<std::string> preferred = boundedUtf8(
                        info.teacherPreferredName,
                        Application::kScheduleViewMaxMeetingTextLength,
                        false
                        );
                    const std::optional<std::string> english = boundedUtf8(
                        info.teacherEn,
                        Application::kScheduleViewMaxMeetingTextLength,
                        false
                        );
                    const std::optional<std::string> korean = boundedUtf8(
                        info.teacherKr,
                        Application::kScheduleViewMaxMeetingTextLength,
                        false
                        );
                    if (!grade || !level || !room || !preferred || !english || !korean)
                    {
                        return repositoryFailure(
                            QStringLiteral(
                                "Schedule class metadata contains oversized text."
                                )
                            );
                    }

                    classes.emplace(
                        info.classId,
                        ClassMetadata{
                            .teacherId = info.teacherId,
                            .grade = *grade,
                            .level = *level,
                            .room = *room,
                            .preferredTeacherName = *preferred,
                            .englishTeacherName = *english,
                            .koreanTeacherName = *korean
                        }
                        );
                    return {};
                },
                [&](const int classId, const ClassTime& time, const bool intensive)
                    -> RepositoryStatus
                {
                    if (intensive)
                    {
                        return {};
                    }
                    const auto classEntry = classes.find(classId);
                    if (classEntry == classes.end())
                    {
                        return repositoryFailure(
                            QStringLiteral(
                                "A schedule row references an unknown class ID."
                                )
                            );
                    }
                    std::size_t& classRowCount = rowsPerClass[classId];
                    if (classRowCount >= kMyWorkspaceScheduleMaximumRowsPerClass
                        || rows.size() >= Application::kScheduleViewMaxRows)
                    {
                        return repositoryFailure(
                            QStringLiteral(
                                "The workspace schedule exceeds its bounded row capacity."
                                )
                            );
                    }

                    const std::optional<std::string> day = boundedUtf8(
                        time.day,
                        Application::kScheduleViewMaxDayLabelLength,
                        true
                        );
                    const std::optional<std::string> start = boundedUtf8(
                        time.startTime,
                        Application::kScheduleViewMaxStartTextLength,
                        true
                        );
                    const std::optional<std::string> end = boundedUtf8(
                        time.endTime,
                        Application::kScheduleViewMaxEndTextLength,
                        true
                        );
                    if (!day || !start || !end)
                    {
                        return repositoryFailure(
                            QStringLiteral(
                                "A schedule row contains invalid or oversized day or time text."
                                )
                            );
                    }

                    const std::optional<Domain::ClassId> typedClassId =
                        Domain::ClassId::fromString(std::to_string(classId));
                    std::optional<Domain::TeacherId> typedTeacherId;
                    if (classEntry->second.teacherId > 0)
                    {
                        typedTeacherId = Domain::TeacherId::fromString(
                            std::to_string(classEntry->second.teacherId)
                            );
                    }
                    if (!typedClassId
                        || (classEntry->second.teacherId > 0 && !typedTeacherId))
                    {
                        return repositoryFailure(
                            QStringLiteral(
                                "A schedule row contains an invalid class or teacher ID."
                                )
                            );
                    }

                    const QString classLabel = classDisplayLabel(
                        classId,
                        classEntry->second
                        );
                    const auto meeting = boundedUtf8(
                        classLabel,
                        Application::kScheduleViewMaxMeetingTextLength,
                        true
                        );
                    if (!meeting)
                    {
                        return repositoryFailure(
                            QStringLiteral("Schedule class labels exceed their bounded limit.")
                            );
                    }

                    ++classRowCount;
                    rows.push_back({
                        .day = *day,
                        .start = *start,
                        .end = *end,
                        .dayOrder = dayOrder(time.day),
                        .classId = *typedClassId,
                        .teacherId = std::move(typedTeacherId),
                        .meeting = *meeting,
                        .room = classEntry->second.room
                    });
                    return {};
                }
                );

            if (!visited)
            {
                return repositoryFailureResult(visited.error());
            }
            if (!m_store->isOpen())
            {
                return unavailable();
            }

            std::stable_sort(
                rows.begin(),
                rows.end(),
                [](const PendingRow& left, const PendingRow& right)
                {
                    if (left.dayOrder != right.dayOrder)
                    {
                        return left.dayOrder < right.dayOrder;
                    }
                    const int leftStart = startMinutes(left.start);
                    const int rightStart = startMinutes(right.start);
                    if (leftStart != rightStart)
                    {
                        return leftStart < rightStart;
                    }
                    if (left.start != right.start)
                    {
                        return left.start < right.start;
                    }
                    if (left.end != right.end)
                    {
                        return left.end < right.end;
                    }
                    return left.classId.value() < right.classId.value();
                }
                );

            Application::ScheduleViewProjectionInput input;
            input.rows.reserve(rows.size());
            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                PendingRow& pending = rows[index];
                const auto rowNumber = static_cast<std::int32_t>(index);
                Application::ScheduleViewCell cell{
                    .order = Application::ScheduleViewCellOrder(0),
                    .slot = Application::ScheduleViewSlotKey(rowNumber),
                    .classId = std::move(pending.classId),
                    .teacherId = std::move(pending.teacherId),
                    .startText = std::move(pending.start),
                    .endText = std::move(pending.end),
                    .meetingText = std::move(pending.meeting),
                    .roomText = std::move(pending.room),
                    .mode = Application::ScheduleViewMode::Regular
                };
                const std::string range = cell.startText + " - " + cell.endText;
                input.rows.push_back({
                    .order = Application::ScheduleViewRowOrder(rowNumber),
                    .day = pending.dayOrder,
                    .dayLabel = std::move(pending.day),
                    .rowLabel = range,
                    .cells = {std::move(cell)}
                });
            }

            const auto projection = Application::ScheduleViewProjection::create(
                std::move(input)
                );
            if (!projection)
            {
                return Application::MyWorkspaceScheduleReadResult::failure(
                    projection.error()
                    );
            }
            return Application::MyWorkspaceScheduleReadResult::success(
                std::move(projection.value())
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The workspace schedule could not be projected."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "The workspace schedule could not be projected."
                );
        }
    }

private:
    struct ClassMetadata final
    {
        int teacherId = -1;
        std::string grade;
        std::string level;
        std::string room;
        std::string preferredTeacherName;
        std::string englishTeacherName;
        std::string koreanTeacherName;
    };

    struct PendingRow final
    {
        std::string day;
        std::string start;
        std::string end;
        std::int32_t dayOrder = 0;
        Domain::ClassId classId;
        std::optional<Domain::TeacherId> teacherId;
        std::string meeting;
        std::string room;
    };

    [[nodiscard]] static int dayOrder(const QString& value)
    {
        const QString day = value.trimmed().toCaseFolded();
        if (day.startsWith(QStringLiteral("mon"))) return 0;
        if (day.startsWith(QStringLiteral("tue"))) return 1;
        if (day.startsWith(QStringLiteral("wed"))) return 2;
        if (day.startsWith(QStringLiteral("thu"))) return 3;
        if (day.startsWith(QStringLiteral("fri"))) return 4;
        if (day.startsWith(QStringLiteral("sat"))) return 5;
        if (day.startsWith(QStringLiteral("sun"))) return 6;
        return 7;
    }

    [[nodiscard]] static int startMinutes(const std::string& value)
    {
        const QString time = QString::fromUtf8(
            value.data(), static_cast<qsizetype>(value.size())
            );
        for (const QString& format : {
                 QStringLiteral("h:mm AP"), QStringLiteral("h:mm A"),
                 QStringLiteral("HH:mm"), QStringLiteral("H:mm")
             })
        {
            const QTime parsed = QTime::fromString(time, format);
            if (parsed.isValid())
            {
                return parsed.hour() * 60 + parsed.minute();
            }
        }
        return std::numeric_limits<int>::max();
    }

    [[nodiscard]] static QString classDisplayLabel(
        const int classId,
        const ClassMetadata& metadata
        )
    {
        QString label = QString::fromUtf8(
            metadata.grade.data(), static_cast<qsizetype>(metadata.grade.size())
            ) + QLatin1Char(' ') + QString::fromUtf8(
                metadata.level.data(), static_cast<qsizetype>(metadata.level.size())
                );
        label = label.simplified();
        if (label.isEmpty())
        {
            label = QStringLiteral("Class %1").arg(classId);
        }

        std::string_view teacher = metadata.preferredTeacherName;
        if (teacher.empty()) teacher = metadata.englishTeacherName;
        if (teacher.empty()) teacher = metadata.koreanTeacherName;
        if (!teacher.empty())
        {
            label += QStringLiteral(" · ") + QString::fromUtf8(
                teacher.data(), static_cast<qsizetype>(teacher.size())
                );
        }
        return label;
    }

    [[nodiscard]] static std::optional<std::string> boundedUtf8(
        const QString& value,
        const std::size_t maximumBytes,
        const bool required
        )
    {
        const QString normalized = value.trimmed();
        if (normalized.isEmpty())
        {
            return required ? std::nullopt : std::optional<std::string>{std::string{}};
        }
        const QByteArray bytes = normalized.toUtf8();
        if (static_cast<std::size_t>(bytes.size()) > maximumBytes)
        {
            return std::nullopt;
        }
        return bytes.toStdString();
    }

    [[nodiscard]] static RepositoryStatus repositoryFailure(QString message)
    {
        return std::unexpected(RepositoryError(
            RepositoryErrorCode::Validation,
            std::move(message)
            ));
    }

    [[nodiscard]] static Application::MyWorkspaceScheduleReadResult
    repositoryFailureResult(const RepositoryError& error)
    {
        const QByteArray diagnostic = error.diagnostic.toUtf8();
        std::string message = diagnostic.toStdString();
        if (message.empty())
        {
            message = "The workspace schedule could not be loaded.";
        }
        return failure(repositoryDomainErrorCode(error.code), std::move(message));
    }

    [[nodiscard]] static Application::MyWorkspaceScheduleReadResult unavailable()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active workspace schedule repository is unavailable."
            );
    }

    [[nodiscard]] static Application::MyWorkspaceScheduleReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::MyWorkspaceScheduleReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = true
        });
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
