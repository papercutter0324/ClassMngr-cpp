#include "next/ui/schedule_import_request_mapper.h"

#include "next/application/schedule_import_overlap_projection.h"
#include "next/application/schedule_import_state_projection.h"
#include "next/domain/korean_teacher_key.h"

#include <QColor>
#include <QTime>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::u16string utf16(const QString& value)
{
    return value.toStdU16String();
}

std::u16string utf16FromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        ).toStdU16String();
}

QString qString(const std::u16string& value)
{
    return QString::fromStdU16String(value);
}

QString identity(const QString& value)
{
    return value.simplified().toCaseFolded();
}

QString errorText(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::optional<Domain::Weekday> weekday(const QString& value)
{
    if (value == QStringLiteral("Monday")) return Domain::Weekday::Monday;
    if (value == QStringLiteral("Tuesday")) return Domain::Weekday::Tuesday;
    if (value == QStringLiteral("Wednesday")) return Domain::Weekday::Wednesday;
    if (value == QStringLiteral("Thursday")) return Domain::Weekday::Thursday;
    if (value == QStringLiteral("Friday")) return Domain::Weekday::Friday;
    if (value == QStringLiteral("Saturday")) return Domain::Weekday::Saturday;
    if (value == QStringLiteral("Sunday")) return Domain::Weekday::Sunday;
    return std::nullopt;
}

int timeMinutes(const QString& value)
{
    static const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm")
    };
    for (const QString& format : formats)
    {
        const QTime time = QTime::fromString(value.trimmed(), format);
        if (time.isValid())
        {
            return time.hour() * 60 + time.minute();
        }
    }
    return -1;
}

Application::ScheduleImportStateTime stateTime(const ClassTime& time)
{
    return Application::makeScheduleImportStateTime(
        weekday(time.day),
        timeMinutes(time.startTime),
        timeMinutes(time.endTime),
        utf8(time.day),
        utf8(time.startTime),
        utf8(time.endTime)
        );
}

std::vector<Application::ScheduleImportStateTime> stateTimes(
    const QList<ClassTime>& times
    )
{
    std::vector<Application::ScheduleImportStateTime> result;
    result.reserve(static_cast<std::size_t>(times.size()));
    for (const ClassTime& time : times)
    {
        result.push_back(stateTime(time));
    }
    return result;
}

std::vector<Application::ScheduleImportMatchingTime> matchingTimes(
    const QList<ClassTime>& times
    )
{
    std::vector<Application::ScheduleImportMatchingTime> result;
    result.reserve(static_cast<std::size_t>(times.size()));
    for (const ClassTime& time : times)
    {
        result.push_back({utf8(time.day)});
    }
    return result;
}

std::vector<Application::ScheduleImportMatchingTime> matchingTimes(
    const std::vector<Application::ScheduleImportStateReadTime>& times
    )
{
    std::vector<Application::ScheduleImportMatchingTime> result;
    result.reserve(times.size());
    for (const auto& time : times)
    {
        result.push_back({utf8(qString(time.day))});
    }
    return result;
}

Application::ScheduleImportStateKind stateKind(ScheduleImportKind kind)
{
    return kind == ScheduleImportKind::Intensive
        ? Application::ScheduleImportStateKind::Intensive
        : Application::ScheduleImportStateKind::Normal;
}

Application::ScheduleImportStateIntensiveMode stateMode(
    Application::ScheduleImportPlanIntensiveMode mode
    )
{
    return mode == Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew
        ? Application::ScheduleImportStateIntensiveMode::ReplaceWithNew
        : Application::ScheduleImportStateIntensiveMode::UpdateExisting;
}

Application::ScheduleImportReviewTeacherAction reviewTeacherAction(
    ScheduleImportTeacherAction action
    )
{
    switch (action)
    {
    case ScheduleImportTeacherAction::Reuse:
        return Application::ScheduleImportReviewTeacherAction::Reuse;
    case ScheduleImportTeacherAction::UpdateRoom:
        return Application::ScheduleImportReviewTeacherAction::UpdateRoom;
    case ScheduleImportTeacherAction::Create:
        return Application::ScheduleImportReviewTeacherAction::Create;
    case ScheduleImportTeacherAction::Skip:
        return Application::ScheduleImportReviewTeacherAction::Skip;
    }
    return Application::ScheduleImportReviewTeacherAction::Unselected;
}

Application::ScheduleImportReviewClassAction reviewClassAction(
    ScheduleImportClassAction action
    )
{
    switch (action)
    {
    case ScheduleImportClassAction::UpdateExisting:
        return Application::ScheduleImportReviewClassAction::UpdateExisting;
    case ScheduleImportClassAction::CreateNew:
        return Application::ScheduleImportReviewClassAction::CreateNew;
    case ScheduleImportClassAction::Skip:
        return Application::ScheduleImportReviewClassAction::Skip;
    }
    return Application::ScheduleImportReviewClassAction::Unselected;
}

Application::ScheduleImportStateTeacherAction stateTeacherAction(
    Application::ScheduleImportReviewTeacherAction action
    )
{
    switch (action)
    {
    case Application::ScheduleImportReviewTeacherAction::Reuse:
        return Application::ScheduleImportStateTeacherAction::Reuse;
    case Application::ScheduleImportReviewTeacherAction::UpdateRoom:
        return Application::ScheduleImportStateTeacherAction::UpdateRoom;
    case Application::ScheduleImportReviewTeacherAction::Create:
        return Application::ScheduleImportStateTeacherAction::Create;
    case Application::ScheduleImportReviewTeacherAction::Skip:
    case Application::ScheduleImportReviewTeacherAction::Unselected:
    case Application::ScheduleImportReviewTeacherAction::Invalid:
        return Application::ScheduleImportStateTeacherAction::Skip;
    }
    return Application::ScheduleImportStateTeacherAction::Skip;
}

Application::ScheduleImportStateClassAction stateClassAction(
    Application::ScheduleImportReviewClassAction action
    )
{
    switch (action)
    {
    case Application::ScheduleImportReviewClassAction::UpdateExisting:
        return Application::ScheduleImportStateClassAction::UpdateExisting;
    case Application::ScheduleImportReviewClassAction::CreateNew:
        return Application::ScheduleImportStateClassAction::CreateNew;
    case Application::ScheduleImportReviewClassAction::Skip:
    case Application::ScheduleImportReviewClassAction::Unselected:
    case Application::ScheduleImportReviewClassAction::Invalid:
        return Application::ScheduleImportStateClassAction::Skip;
    }
    return Application::ScheduleImportStateClassAction::Skip;
}

std::optional<Domain::TeacherId> teacherIdForKey(
    const ScheduleImportReviewMapping& mapping,
    const std::vector<std::optional<Domain::TeacherId>>& ids,
    const Application::ScheduleImportReviewTeacherKey& key
    )
{
    for (std::size_t index = 0;
         index < mapping.matching.teachers.size() && index < ids.size();
         ++index)
    {
        if (utf8(qString(mapping.matching.teachers[index].teacherKey.value()))
                == key.value())
        {
            return ids[index];
        }
    }
    return std::nullopt;
}

std::optional<Application::ScheduleImportPlanEligibilityClassColors>
colorForCandidate(
    const std::vector<Application::ScheduleImportPlanEligibilityClassColors>& values,
    const std::size_t candidateIndex
    )
{
    const auto found = std::find_if(
        values.begin(),
        values.end(),
        [candidateIndex](const auto& value)
        {
            return value.candidateIndex.rawValue() == candidateIndex;
        }
        );
    return found == values.end()
        ? std::nullopt
        : std::optional<Application::ScheduleImportPlanEligibilityClassColors>{
              *found
          };
}

} // namespace

std::expected<ScheduleImportReviewMapping, QString> mapScheduleImportReview(
    const ScheduleImportUserBlock& user,
    const ScheduleImportKind kind,
    const Application::ScheduleImportStateSnapshot& snapshot
    )
{
    using namespace Application;

    ScheduleImportReviewMapping result;
    result.kind = kind;
    result.user = user;
    result.snapshot = snapshot;

    ScheduleImportMatchingInput matchingInput;
    matchingInput.kind = kind == ScheduleImportKind::Intensive
        ? ScheduleImportMatchingKind::Intensive
        : ScheduleImportMatchingKind::Normal;
    matchingInput.candidates.reserve(static_cast<std::size_t>(user.classes.size()));
    result.applyTemplate.intensiveSchedule =
        kind == ScheduleImportKind::Intensive;
    result.applyTemplate.selectedUserName = utf16(user.name);
    result.applyTemplate.candidates.reserve(
        static_cast<std::size_t>(user.classes.size())
        );
    result.decisions.candidates.reserve(
        static_cast<std::size_t>(user.classes.size())
        );

    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        const Domain::KoreanTeacherKey teacherKey =
            Domain::KoreanTeacherKey::fromName(candidate.teacherKey.toStdU16String());

        ScheduleImportMatchingCandidate matchingCandidate;
        matchingCandidate.teacherKey = teacherKey;
        matchingCandidate.teacherName = utf16(candidate.teacherKr);
        matchingCandidate.gradeMatchKey = utf16(identity(candidate.classGrade));
        matchingCandidate.levelMatchKey = utf16(identity(candidate.classLevel));
        matchingCandidate.rooms.reserve(static_cast<std::size_t>(candidate.rooms.size()));
        matchingCandidate.roomMatchKeys.reserve(
            static_cast<std::size_t>(candidate.rooms.size())
            );
        for (const QString& room : candidate.rooms)
        {
            matchingCandidate.rooms.push_back(utf16(room));
            matchingCandidate.roomMatchKeys.push_back(utf16(identity(room)));
        }
        matchingCandidate.times = matchingTimes(candidate.times);
        matchingInput.candidates.push_back(std::move(matchingCandidate));

        ScheduleImportApplyCandidate applyCandidate;
        applyCandidate.teacherKey = ScheduleImportApplyTeacherKey{
            utf16(QString::fromStdU16String(teacherKey.value()))
        };
        applyCandidate.teacherName = utf16(candidate.teacherKr);
        for (const QString& room : candidate.rooms)
        {
            applyCandidate.rooms.push_back(utf16(room));
        }
        for (const QString& color : candidate.importedColors)
        {
            applyCandidate.importedColors.push_back(utf16(color));
        }
        applyCandidate.grade = utf16(candidate.classGrade);
        applyCandidate.level = utf16(candidate.classLevel);
        for (const ClassTime& time : candidate.times)
        {
            applyCandidate.times.push_back({
                utf16(time.day),
                utf16(time.startTime),
                utf16(time.endTime)
            });
        }
        for (const QString& sourceCell : candidate.sourceCells)
        {
            applyCandidate.sourceCells.push_back(utf16(sourceCell));
        }
        applyCandidate.meetingPatternError = utf16(candidate.meetingPatternError);
        result.applyTemplate.candidates.push_back(std::move(applyCandidate));

        ScheduleImportReviewDecisionCandidate decisionCandidate;
        decisionCandidate.teacherKey = ScheduleImportReviewTeacherKey{
            utf8(QString::fromStdU16String(teacherKey.value()))
        };
        for (const QString& room : candidate.rooms)
        {
            if (!room.isEmpty())
            {
                decisionCandidate.importedRooms.emplace_back(utf8(room));
            }
        }
        result.decisions.candidates.push_back(std::move(decisionCandidate));
    }

    for (const IntensiveSlotState& state : user.intensiveSlotStates)
    {
        result.applyTemplate.intensiveSlotStates.push_back({
            utf16(state.day),
            utf16(state.startTime),
            utf16(state.state)
        });
    }
    for (const ScheduleImportDiagnostic& diagnostic : user.diagnostics)
    {
        result.applyTemplate.diagnostics.push_back({
            utf16(diagnostic.sheetName),
            utf16(diagnostic.userName),
            utf16(diagnostic.cellReference),
            utf16(diagnostic.value),
            utf16(diagnostic.message)
        });
    }

    matchingInput.teachers.reserve(snapshot.teachers.size());
    for (const auto& teacher : snapshot.teachers)
    {
        matchingInput.teachers.push_back({teacher.id, teacher.koreanName});
    }
    matchingInput.classes.reserve(snapshot.classes.size());
    for (const auto& classroom : snapshot.classes)
    {
        matchingInput.classes.push_back({
            classroom.id,
            classroom.teacherId,
            true,
            utf16(identity(qString(classroom.roomNumber))),
            utf16(identity(qString(classroom.grade))),
            utf16(identity(qString(classroom.level))),
            matchingTimes(classroom.normalTimes),
            matchingTimes(classroom.intensiveTimes)
        });
    }

    const auto projected = projectScheduleImportMatching(matchingInput);
    if (!projected)
    {
        return std::unexpected(QStringLiteral(
            "The selected workbook block could not be matched to this profile."
            ));
    }
    result.matching = *projected;
    if (user.classes.size() > static_cast<qsizetype>(
            std::numeric_limits<int>::max()
            ))
    {
        return std::unexpected(QStringLiteral(
            "The selected workbook block contains too many class candidates."
            ));
    }

    result.decisions.teachers.reserve(result.matching.teachers.size());
    result.selectedTeacherIds.reserve(result.matching.teachers.size());
    for (const ScheduleImportMatchingTeacherProjection& teacher :
         result.matching.teachers)
    {
        const ScheduleImportReviewTeacherKey key{
            utf8(QString::fromStdU16String(teacher.teacherKey.value()))
        };
        const ScheduleImportReviewTeacherAction action =
            teacher.matchingTeacherIds.size() == 1
                ? ScheduleImportReviewTeacherAction::Reuse
                : teacher.matchingTeacherIds.empty()
                    ? ScheduleImportReviewTeacherAction::Create
                    : ScheduleImportReviewTeacherAction::Unselected;
        result.decisions.teachers.push_back({
            key,
            action,
            ScheduleImportReviewRoomValue(
                teacher.importedRooms.size() == 1
                    ? utf8(QString::fromStdU16String(teacher.importedRooms.front()))
                    : std::string{}
                )
        });
        result.selectedTeacherIds.push_back(
            teacher.matchingTeacherIds.size() == 1
                ? std::optional<Domain::TeacherId>{
                      teacher.matchingTeacherIds.front()
                  }
                : std::nullopt
            );
    }

    result.decisions.classes.reserve(result.matching.classes.size());
    result.classColors.reserve(user.classes.size());
    for (const ScheduleImportMatchingClassProjection& classroom :
         result.matching.classes)
    {
        const bool exactTarget = classroom.exactMatch
            && classroom.suggestedClassId.has_value();
        const ScheduleImportReviewClassAction action = exactTarget
            ? ScheduleImportReviewClassAction::UpdateExisting
            : classroom.matchingClassIds.empty()
                ? ScheduleImportReviewClassAction::CreateNew
                : ScheduleImportReviewClassAction::Unselected;
        result.decisions.classes.push_back({
            ScheduleImportCandidateIndex{static_cast<int>(
                classroom.candidateIndex.rawValue()
                )},
            action,
            exactTarget ? classroom.suggestedClassId : std::nullopt
        });

        const std::size_t candidateIndex = classroom.candidateIndex.rawValue();
        const ScheduleImportClassCandidate& candidate =
            user.classes.at(static_cast<qsizetype>(candidateIndex));
        QString color = candidate.importedColors.isEmpty()
            ? QStringLiteral("#FFFFFF")
            : candidate.importedColors.front();
        if (!QColor::isValidColorName(color) || color.size() != 7)
        {
            color = QStringLiteral("#FFFFFF");
        }
        const QColor selectedColor(color);
        const QString fontColor = selectedColor.lightnessF() >= 0.5
            ? QStringLiteral("#000000")
            : QStringLiteral("#FFFFFF");
        result.classColors.push_back({
            ScheduleImportCandidateIndex{static_cast<int>(candidateIndex)},
            utf8(color),
            utf8(fontColor)
        });
    }

    return result;
}

Application::ScheduleImportApplyRequest makeScheduleImportApplyRequest(
    const ScheduleImportReviewMapping& mapping,
    const Application::ScheduleImportReviewDecisionRequest& decisions,
    const std::vector<std::optional<Domain::TeacherId>>& teacherIds,
    const std::vector<Application::ScheduleImportPlanEligibilityClassColors>&
        classColors,
    const Application::ScheduleImportPlanIntensiveMode intensiveMode,
    const bool diagnosticsAcknowledged,
    const bool saveProfileNameIfBlank,
    const bool updateProfileName
    )
{
    Application::ScheduleImportApplyRequest request = mapping.applyTemplate;
    request.intensiveSchedule = mapping.kind == ScheduleImportKind::Intensive;
    request.intensiveMode = intensiveMode;
    request.diagnosticsAcknowledged = diagnosticsAcknowledged;
    request.saveProfileNameIfBlank = saveProfileNameIfBlank;
    request.updateProfileName = updateProfileName;

    request.teachers.clear();
    request.teachers.reserve(decisions.teachers.size());
    for (const auto& teacher : decisions.teachers)
    {
        request.teachers.push_back({
            Application::ScheduleImportApplyTeacherKey{
                utf16FromUtf8(teacher.teacherKey.value())
            },
            teacher.action,
            teacherIdForKey(mapping, teacherIds, teacher.teacherKey),
            utf16FromUtf8(teacher.selectedRoom.value())
        });
    }

    request.classes.clear();
    request.classes.reserve(decisions.classes.size());
    for (const auto& classroom : decisions.classes)
    {
        const std::size_t candidateIndex =
            classroom.candidateIndex.rawValue();
        const auto colors = colorForCandidate(classColors, candidateIndex);
        request.classes.push_back({
            classroom.candidateIndex,
            classroom.action,
            classroom.targetClassId,
            colors
                ? utf16FromUtf8(colors->classColor)
                : std::u16string{},
            colors
                ? utf16FromUtf8(colors->fontColor)
                : std::u16string{}
        });
    }
    return request;
}

Application::ScheduleImportStateValidationRequest
makeScheduleImportStateValidationRequest(
    const ScheduleImportReviewMapping& mapping,
    const Application::ScheduleImportReviewDecisionRequest& decisions,
    const std::vector<std::optional<Domain::TeacherId>>& teacherIds,
    const Application::ScheduleImportPlanIntensiveMode intensiveMode
    )
{
    Application::ScheduleImportStateValidationRequest request;
    request.kind = stateKind(mapping.kind);
    request.intensiveMode = stateMode(intensiveMode);
    request.candidates.reserve(mapping.user.classes.size());
    for (std::size_t index = 0; index < mapping.user.classes.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            mapping.user.classes.at(static_cast<qsizetype>(index));
        const Domain::KoreanTeacherKey teacherKey =
            Domain::KoreanTeacherKey::fromName(candidate.teacherKey.toStdU16String());
        QString label = QStringLiteral("%1 %2")
            .arg(candidate.classGrade, candidate.classLevel);
        request.candidates.push_back({
            Application::ScheduleImportReviewTeacherKey{
                utf8(QString::fromStdU16String(teacherKey.value()))
            },
            utf8(identity(candidate.classGrade)),
            utf8(identity(candidate.classLevel)),
            utf8(label),
            stateTimes(candidate.times)
        });
    }

    request.teacherResolutions.reserve(decisions.teachers.size());
    for (const auto& teacher : decisions.teachers)
    {
        request.teacherResolutions.push_back({
            teacher.teacherKey,
            stateTeacherAction(teacher.action),
            teacherIdForKey(mapping, teacherIds, teacher.teacherKey),
            !teacher.selectedRoom.empty()
        });
    }
    request.classResolutions.reserve(decisions.classes.size());
    for (const auto& classroom : decisions.classes)
    {
        request.classResolutions.push_back({
            Application::ScheduleImportStateCandidateIndex{
                static_cast<std::size_t>(classroom.candidateIndex.rawValue())
            },
            stateClassAction(classroom.action),
            classroom.targetClassId
        });
    }

    request.existingTeachers.reserve(mapping.snapshot.teachers.size());
    for (const auto& teacher : mapping.snapshot.teachers)
    {
        const Domain::KoreanTeacherKey key =
            Domain::KoreanTeacherKey::fromName(teacher.koreanName);
        request.existingTeachers.push_back({
            teacher.id,
            Application::ScheduleImportReviewTeacherKey{
                utf8(QString::fromStdU16String(key.value()))
            }
        });
    }

    request.existingClasses.reserve(mapping.snapshot.classes.size());
    for (const auto& classroom : mapping.snapshot.classes)
    {
        Application::ScheduleImportStateClassSnapshot projected{
            classroom.id,
            classroom.teacherId,
            utf8(identity(qString(classroom.grade))),
            utf8(identity(qString(classroom.level))),
            utf8(qString(classroom.className)),
            {},
            {}
        };
        projected.normalTimes.reserve(classroom.normalTimes.size());
        for (const auto& time : classroom.normalTimes)
        {
            const ClassTime legacy{
                qString(time.day), qString(time.startTime), qString(time.endTime)
            };
            projected.normalTimes.push_back(stateTime(legacy));
        }
        projected.intensiveTimes.reserve(classroom.intensiveTimes.size());
        for (const auto& time : classroom.intensiveTimes)
        {
            const ClassTime legacy{
                qString(time.day), qString(time.startTime), qString(time.endTime)
            };
            projected.intensiveTimes.push_back(stateTime(legacy));
        }
        request.existingClasses.push_back(std::move(projected));
    }
    return request;
}

Application::ScheduleImportReviewReadinessResult evaluateScheduleImportReview(
    const ScheduleImportReviewMapping& mapping,
    const Application::ScheduleImportReviewDecisionRequest& decisions,
    const std::vector<std::optional<Domain::TeacherId>>& teacherIds,
    const Application::ScheduleImportPlanIntensiveMode intensiveMode
    )
{
    return Application::evaluateScheduleImportReviewReadiness(
        decisions,
        makeScheduleImportStateValidationRequest(
            mapping,
            decisions,
            teacherIds,
            intensiveMode
            )
        );
}

} // namespace ClassMngr::Next::Ui
