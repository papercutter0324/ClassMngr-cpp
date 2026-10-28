#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/my_classes_page_read.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <charconv>
#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Direct active-WorkspaceStore read boundary for the Next My Classes page.
// List reads batch compact class metadata and teacher display names; selected
// details query one class ID and never construct roster rows.
class WorkspaceStoreMyClassesPageReadPort final
    : public Application::MyClassesPageReadPort
{
public:
    explicit WorkspaceStoreMyClassesPageReadPort(WorkspaceStore& store) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::MyClassesPageSummaryResult readSummaries()
        const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return Application::MyClassesPageSummaryResult::failure(
                unavailableError()
                );
        }
        ClassRepository* const classes = m_store->classRepository();
        ClassInfoRepository* const classInfo = m_store->classInfoRepository();
        TeacherRepository* const teachers = m_store->teacherRepository();
        if (!classes || !classInfo || !teachers)
        {
            return Application::MyClassesPageSummaryResult::failure(
                unavailableError()
                );
        }

        try
        {
            const auto loadedClasses = classes->getClasses();
            if (!loadedClasses)
            {
                return Application::MyClassesPageSummaryResult::failure(
                    repositoryError(loadedClasses.error(),
                        "Classes could not be loaded.")
                    );
            }
            if (static_cast<std::size_t>(loadedClasses->size())
                > Application::kMyClassesPageMaximumClasses)
            {
                return Application::MyClassesPageSummaryResult::failure(
                    validationError(
                        "The My Classes summary list exceeds its bounded limit."
                        )
                    );
            }

            Application::MyClassesPageSummarySnapshot snapshot;
            snapshot.reserve(static_cast<std::size_t>(loadedClasses->size()));
            QList<int> classIds;
            classIds.reserve(loadedClasses->size());
            std::unordered_set<int> seenClassIds;
            seenClassIds.reserve(static_cast<std::size_t>(loadedClasses->size()));
            for (const Classroom& classroom : *loadedClasses)
            {
                if (classroom.id <= 0
                    || !seenClassIds.insert(classroom.id).second
                    || !bounded(classroom.name))
                {
                    return Application::MyClassesPageSummaryResult::failure(
                        validationError(
                            "The class list contains an invalid or duplicate identifier."
                            )
                        );
                }
                classIds.push_back(classroom.id);
                const auto id = Domain::ClassId::fromString(
                    std::to_string(classroom.id)
                    );
                if (!id)
                {
                    return Application::MyClassesPageSummaryResult::failure(
                        validationError("The class list contains an invalid identifier.")
                        );
                }
                snapshot.push_back({
                    .classId = *id,
                    .className = classroom.name.toStdU16String()
                });
            }
            if (classIds.empty())
            {
                return Application::MyClassesPageSummaryResult::success(
                    std::move(snapshot)
                    );
            }

            const auto loadedSummaries =
                classInfo->loadMyClassesClassSummaryRecords(classIds);
            if (!loadedSummaries)
            {
                // Legacy My Classes keeps the class list visible when optional
                // class-info enrichment is unavailable. The selected detail
                // read still reports its own inline failure.
                return Application::MyClassesPageSummaryResult::success(
                    std::move(snapshot)
                    );
            }
            if (loadedSummaries->size() != classIds.size())
            {
                return Application::MyClassesPageSummaryResult::failure(
                    validationError(
                        "The class-summary repository returned an incomplete list."
                        )
                    );
            }

            std::unordered_map<int, std::size_t> indexByClassId;
            indexByClassId.reserve(snapshot.size());
            for (std::size_t index = 0; index < snapshot.size(); ++index)
            {
                const auto parsed = Application::parseCanonicalPositiveDecimalId(
                    snapshot[index].classId.value()
                    );
                if (!parsed)
                {
                    return Application::MyClassesPageSummaryResult::failure(
                        validationError("The class list contains an invalid identifier.")
                        );
                }
                indexByClassId.emplace(*parsed, index);
            }

            std::unordered_set<qint64> referencedTeacherIds;
            for (const auto& entry : *loadedSummaries)
            {
                const auto index = indexByClassId.find(entry.classId);
                if (index == indexByClassId.end())
                {
                    return Application::MyClassesPageSummaryResult::failure(
                        validationError(
                            "The class-summary repository returned a different class identifier."
                            )
                        );
                }
                if (!entry.summary)
                {
                    continue;
                }
                const MyClassesClassSummaryReadRecord& record =
                    entry.summary.value();
                if (record.classId != entry.classId)
                {
                    return Application::MyClassesPageSummaryResult::failure(
                        validationError(
                            "The class-summary record returned a different class identifier."
                            )
                        );
                }
                if (!bounded(record.classGrade)
                    || !bounded(record.classLevel)
                    || !boundedSchedule(record.regularTimes)
                    || !boundedSchedule(record.intensiveTimes))
                {
                    return Application::MyClassesPageSummaryResult::failure(
                        validationError(
                            "The class-summary record contains unbounded fields."
                            )
                        );
                }
                auto& summary = snapshot[index->second];
                summary.grade = record.classGrade.toStdU16String();
                summary.level = record.classLevel.toStdU16String();
                summary.regularSchedule = schedule(record.regularTimes);
                summary.intensiveSchedule = schedule(record.intensiveTimes);
                if (record.teacherId > 0)
                {
                    const auto teacherId = Domain::TeacherId::fromString(
                        std::to_string(record.teacherId)
                        );
                    if (!teacherId)
                    {
                        return Application::MyClassesPageSummaryResult::failure(
                            validationError(
                                "A class references an invalid teacher identifier."
                                )
                            );
                    }
                    summary.teacherId = *teacherId;
                    referencedTeacherIds.insert(record.teacherId);
                }
            }

            if (!referencedTeacherIds.empty())
            {
                QList<qint64> teacherIds;
                teacherIds.reserve(static_cast<qsizetype>(referencedTeacherIds.size()));
                std::vector<qint64> sortedTeacherIds(
                    referencedTeacherIds.begin(), referencedTeacherIds.end()
                    );
                std::sort(sortedTeacherIds.begin(), sortedTeacherIds.end());
                for (const qint64 id : sortedTeacherIds)
                {
                    teacherIds.push_back(id);
                }

                const auto loadedTeacherNames =
                    teachers->loadMyClassesTeacherDisplayNameRecords(teacherIds);
                if (loadedTeacherNames)
                {
                    std::unordered_map<qint64, std::pair<std::u16string, std::u16string>>
                        namesById;
                    namesById.reserve(static_cast<std::size_t>(loadedTeacherNames->size()));
                    for (const auto& record : *loadedTeacherNames)
                    {
                        if (record.teacherId > 0)
                        {
                            if (!bounded(record.teacherEn)
                                || !bounded(record.teacherKr))
                            {
                                return Application::MyClassesPageSummaryResult::failure(
                                    validationError(
                                        "Teacher display names exceed their bounded limit."
                                        )
                                    );
                            }
                            namesById.emplace(
                                record.teacherId,
                                std::pair{
                                    record.teacherEn.toStdU16String(),
                                    record.teacherKr.toStdU16String()
                                }
                                );
                        }
                    }
                    for (auto& summary : snapshot)
                    {
                        if (!summary.teacherId)
                        {
                            continue;
                        }
                        const auto parsed = Application::parseCanonicalPositiveDecimalId64(
                            summary.teacherId->value()
                            );
                        if (parsed)
                        {
                            const auto names = namesById.find(*parsed);
                            if (names != namesById.end())
                            {
                                summary.teacherEnglishName = names->second.first;
                                summary.teacherKoreanName = names->second.second;
                            }
                        }
                    }
                }
            }

            return Application::MyClassesPageSummaryResult::success(
                std::move(snapshot)
                );
        }
        catch (...)
        {
            return Application::MyClassesPageSummaryResult::failure(
                technicalError("Class summaries could not be loaded.")
                );
        }
    }

    [[nodiscard]] Application::MyClassesPageSelectedDetailsResult
    readSelectedDetails(const Domain::ClassId& classId) const override
    {
        const auto parsedClassId =
            Application::parseCanonicalPositiveDecimalId(classId.value());
        if (!parsedClassId || *parsedClassId <= 0)
        {
            return Application::MyClassesPageSelectedDetailsResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "The selected class identifier is invalid.",
                .recoverable = false
            });
        }
        if (!m_store || !m_store->isOpen())
        {
            return Application::MyClassesPageSelectedDetailsResult::failure(
                unavailableError()
                );
        }

        ClassRepository* const classes = m_store->classRepository();
        ClassInfoRepository* const classInfo = m_store->classInfoRepository();
        RosterRepository* const rosters = m_store->rosterRepository();
        TeacherRepository* const teachers = m_store->teacherRepository();
        if (!classes || !classInfo || !rosters || !teachers)
        {
            return Application::MyClassesPageSelectedDetailsResult::failure(
                unavailableError()
                );
        }

        try
        {
            const auto classroom = classes->getClassById(*parsedClassId);
            if (!classroom)
            {
                return Application::MyClassesPageSelectedDetailsResult::failure(
                    repositoryError(classroom.error(),
                        "The selected class could not be loaded.")
                    );
            }
            if (!bounded(classroom->name))
            {
                return Application::MyClassesPageSelectedDetailsResult::failure(
                    validationError(
                        "The selected class name exceeds its bounded limit."
                        )
                    );
            }

            const QList<int> oneClass{*parsedClassId};
            const auto information =
                classInfo->loadMyClassesClassInformationRecords(oneClass);
            if (!information || information->size() != 1
                || information->front().parentMissing
                || !information->front().information)
            {
                const Domain::OperationError error = information
                    ? (information->size() == 1
                        && information->front().information
                        ? validationError(
                            "The selected class details are incomplete."
                            )
                        : information->size() == 1
                            ? repositoryError(
                                information->front().information.error(),
                                "The selected class details could not be loaded.")
                            : validationError(
                                "The selected class details are incomplete."))
                    : repositoryError(information.error(),
                        "The selected class details could not be loaded.");
                return Application::MyClassesPageSelectedDetailsResult::failure(error);
            }

            const MyClassesClassInformationReadRecord& fields =
                information->front().information.value();
            if (fields.classId != *parsedClassId)
            {
                return Application::MyClassesPageSelectedDetailsResult::failure(
                    validationError(
                        "The selected-class repository returned a different identifier."
                        )
                    );
            }
            if (!bounded(fields.classGrade)
                || !bounded(fields.classLevel)
                || !bounded(fields.notes)
                || !bounded(fields.timeFillerActivities)
                || !boundedSchedule(fields.regularTimes)
                || !boundedSchedule(fields.intensiveTimes))
            {
                return Application::MyClassesPageSelectedDetailsResult::failure(
                    validationError(
                        "The selected class details contain unbounded fields."
                        )
                    );
            }

            Application::MyClassesPageSelectedDetails details{
                .classId = classId,
                .className = classroom->name.toStdU16String(),
                .grade = fields.classGrade.toStdU16String(),
                .level = fields.classLevel.toStdU16String(),
                .regularSchedule = schedule(fields.regularTimes),
                .intensiveSchedule = schedule(fields.intensiveTimes),
                .classNotes = fields.notes.toStdU16String(),
                .timeFillerActivities = fields.timeFillerActivities.toStdU16String()
            };
            if (fields.teacherId > 0)
            {
                const auto teacherId = Domain::TeacherId::fromString(
                    std::to_string(fields.teacherId)
                    );
                if (!teacherId)
                {
                    return Application::MyClassesPageSelectedDetailsResult::failure(
                        validationError("The selected class has an invalid teacher identifier.")
                        );
                }
                details.teacherId = *teacherId;
            }

            const auto counts = rosters->loadMyClassesStudentCountRecords(oneClass);
            if (counts && counts->size() == 1
                && counts->front().classId == *parsedClassId
                && counts->front().studentCount)
            {
                details.studentCount = counts->front().studentCount.value();
            }

            if (details.teacherId)
            {
                const auto teacherId =
                    Application::parseCanonicalPositiveDecimalId64(
                        details.teacherId->value()
                        );
                if (!teacherId)
                {
                    return Application::MyClassesPageSelectedDetailsResult::failure(
                        validationError("The selected teacher identifier is invalid.")
                        );
                }
                const QList<qint64> teacherIds{*teacherId};
                const auto profiles =
                    teachers->loadMyClassesTeacherProfileRecords(teacherIds);
                if (!profiles)
                {
                    if (profiles.error().code == RepositoryErrorCode::NotFound
                        || profiles.error().code == RepositoryErrorCode::InvalidInput
                        || profiles.error().code == RepositoryErrorCode::Validation)
                    {
                        return Application::MyClassesPageSelectedDetailsResult::success(
                            std::move(details)
                            );
                    }
                    return Application::MyClassesPageSelectedDetailsResult::failure(
                        repositoryError(profiles.error(),
                            "The selected teacher details could not be loaded.")
                        );
                }
                if (profiles->size() != 1
                    || profiles->front().requestedTeacherId != *teacherId)
                {
                    return Application::MyClassesPageSelectedDetailsResult::failure(
                        validationError(
                            "The selected teacher details are incomplete."
                            )
                        );
                }
                if (!profiles->front().profile)
                {
                    if (profiles->front().profile.error().code
                            != RepositoryErrorCode::NotFound
                        && profiles->front().profile.error().code
                            != RepositoryErrorCode::InvalidInput
                        && profiles->front().profile.error().code
                            != RepositoryErrorCode::Validation)
                    {
                        return Application::MyClassesPageSelectedDetailsResult::failure(
                            repositoryError(profiles->front().profile.error(),
                                "The selected teacher details could not be loaded.")
                            );
                    }
                    return Application::MyClassesPageSelectedDetailsResult::success(
                        std::move(details)
                        );
                }
                const auto& profile = profiles->front().profile.value();
                if (profile.teacherId != *teacherId)
                {
                    return Application::MyClassesPageSelectedDetailsResult::success(
                        std::move(details)
                        );
                }
                if (!bounded(profile.teacherKr)
                    || !bounded(profile.teacherEn)
                    || !bounded(profile.preferredRomanization)
                    || !bounded(profile.preferredName)
                    || !bounded(profile.roomNumber)
                    || !bounded(profile.internetType)
                    || !bounded(profile.wifiName)
                    || !bounded(profile.wifiPassword)
                    || !bounded(profile.projectionType)
                    || !bounded(profile.zoomId)
                    || !bounded(profile.zoomPassword)
                    || !bounded(profile.notes))
                {
                    return Application::MyClassesPageSelectedDetailsResult::failure(
                        validationError(
                            "The selected teacher details contain unbounded fields."
                            )
                        );
                }
                details.teacher = Application::MyClassesPageTeacherDetails{
                    .koreanName = profile.teacherKr.toStdU16String(),
                    .englishName = profile.teacherEn.toStdU16String(),
                    .preferredRomanization = profile.preferredRomanization.toStdU16String(),
                    .preferredName = profile.preferredName.toStdU16String(),
                    .roomNumber = profile.roomNumber.toStdU16String(),
                    .internetType = profile.internetType.toStdU16String(),
                    .wifiName = profile.wifiName.toStdU16String(),
                    .wifiPassword = profile.wifiPassword.toStdU16String(),
                    .projectionType = profile.projectionType.toStdU16String(),
                    .zoomId = profile.zoomId.toStdU16String(),
                    .zoomPassword = profile.zoomPassword.toStdU16String(),
                    .notes = profile.notes.toStdU16String()
                };
            }

            return Application::MyClassesPageSelectedDetailsResult::success(
                std::move(details)
                );
        }
        catch (...)
        {
            return Application::MyClassesPageSelectedDetailsResult::failure(
                technicalError("The selected class details could not be loaded.")
                );
        }
    }

private:
    [[nodiscard]] static bool bounded(const QString& value) noexcept
    {
        return static_cast<std::size_t>(value.size())
            <= Application::kMyClassesPageMaximumTextLength;
    }

    [[nodiscard]] static bool boundedSchedule(const QList<ClassTime>& schedule)
        noexcept
    {
        return static_cast<std::size_t>(schedule.size())
                <= Application::kMyClassesPageMaximumScheduleRows
            && std::all_of(schedule.cbegin(), schedule.cend(), [](const auto& entry) {
                return boundedScheduleToken(entry.day)
                    && boundedScheduleToken(entry.startTime)
                    && boundedScheduleToken(entry.endTime);
            });
    }

    [[nodiscard]] static bool boundedScheduleToken(const QString& value) noexcept
    {
        return static_cast<std::size_t>(value.size())
            <= Application::kMyClassesPageMaximumScheduleTokenLength;
    }

    [[nodiscard]] static std::vector<Application::MyClassesPageScheduleEntry>
    schedule(const QList<ClassTime>& source)
    {
        std::vector<Application::MyClassesPageScheduleEntry> result;
        result.reserve(static_cast<std::size_t>(source.size()));
        for (const ClassTime& time : source)
        {
            result.push_back({
                .day = time.day.toStdU16String(),
                .startTime = time.startTime.toStdU16String(),
                .endTime = time.endTime.toStdU16String()
            });
        }
        return result;
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "The active workspace store is unavailable.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError validationError(
        const char* message
        )
    {
        return {
            .code = Domain::ErrorCode::Validation,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(
        const char* message
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = message,
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& error,
        const char* fallback
        )
    {
        const QByteArray bytes = error.diagnostic.toUtf8();
        return {
            .code = repositoryDomainErrorCode(error.code),
            .message = bytes.isEmpty()
                ? std::string(fallback)
                : std::string(bytes.constData(), static_cast<std::size_t>(bytes.size())),
            .recoverable = error.code != RepositoryErrorCode::InvalidInput
        };
    }

    WorkspaceStore* m_store = nullptr;
};

} // namespace ClassMngr::Next::Platform
