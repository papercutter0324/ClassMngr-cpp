#pragma once

#include "domain/models/teacher_import.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "next/application/teacher_import_use_case.h"

#include <QString>

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{
namespace Detail
{

[[nodiscard]] inline QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

[[nodiscard]] inline QString koreanTeacherNameKey(const QString& value)
{
    return TeacherImportNameUtils::hangulOnly(value);
}

[[nodiscard]] inline std::u16string trimmed16(const QString& value)
{
    return value.trimmed().toStdU16String();
}

[[nodiscard]] inline std::u16string simplifiedOrEmpty16(const QString& value)
{
    return value.trimmed().isEmpty()
        ? std::u16string{}
        : value.simplified().toStdU16String();
}

} // namespace Detail

inline TeacherImportUseCaseRequest teacherImportPlanToUseCaseRequest(const TeacherImportPlan& plan)
{
    TeacherImportUseCaseRequest request;
    request.sourceDateIso = plan.sourceDate.toString(Qt::ISODate).toStdString();
    request.validation.sourceDateValid = plan.sourceDate.isValid();
    request.koreanTeachers.reserve(
        static_cast<std::size_t>(plan.koreanTeachers.size()));
    request.validation.koreanTeacherKeys.reserve(
        static_cast<std::size_t>(plan.koreanTeachers.size()));
    for (const Teacher& teacher : plan.koreanTeachers)
    {
        const std::string validationKey =
            Detail::koreanTeacherNameKey(teacher.teacherKr).toUtf8().toStdString();
        request.validation.koreanTeacherKeys.push_back(validationKey);
        request.koreanTeachers.push_back({
            .validationKey = validationKey,
            .teacherKr = teacher.teacherKr.toStdU16String(),
            .teacherEn = Detail::trimmed16(teacher.teacherEn),
            .preferredRomanization = Detail::trimmed16(teacher.preferredRomanization),
            .preferredName = Detail::trimmed16(teacher.preferredName),
            .roomNumber = Detail::trimmed16(teacher.roomNumber),
            .birthday = Detail::trimmed16(teacher.birthday),
            .phoneNumber = Detail::trimmed16(teacher.phoneNumber),
            .wifiName = Detail::trimmed16(teacher.wifiName),
            .wifiPassword = Detail::trimmed16(teacher.wifiPassword),
            .internetType = Detail::trimmed16(teacher.internetType),
            .zoomId = Detail::trimmed16(teacher.zoomId),
            .zoomPassword = Detail::trimmed16(teacher.zoomPassword),
            .projectionType = Detail::trimmed16(teacher.projectionType),
            .notes = Detail::trimmed16(teacher.notes)
        });
    }

    request.nativeEnglishTeachers.reserve(
        static_cast<std::size_t>(plan.nativeEnglishTeachers.size()));
    request.validation.nativeEnglishTeacherKeys.reserve(
        static_cast<std::size_t>(plan.nativeEnglishTeachers.size()));
    for (const NativeEnglishTeacher& teacher : plan.nativeEnglishTeachers)
    {
        const QString matchKey = Detail::normalizedName(teacher.name);
        const std::string validationKey = matchKey.toUtf8().toStdString();
        request.validation.nativeEnglishTeacherKeys.push_back(validationKey);
        request.nativeEnglishTeachers.push_back({
            .validationKey = validationKey,
            .normalizedMatchKey = matchKey.toStdU16String(),
            .diagnosticName = teacher.name.toStdU16String(),
            .name = teacher.name.simplified().toStdU16String(),
            .position = Detail::trimmed16(teacher.position),
            .phoneNumber = Detail::trimmed16(teacher.phoneNumber),
            .birthday = Detail::trimmed16(teacher.birthday),
            .nationality = Detail::trimmed16(teacher.nationality),
            .email = Detail::trimmed16(teacher.email)
        });
    }

    request.gsTeamMembers.reserve(
        static_cast<std::size_t>(plan.gsTeamMembers.size()));
    request.validation.gsTeamMemberKeys.reserve(
        static_cast<std::size_t>(plan.gsTeamMembers.size()));
    for (const GsTeamMember& member : plan.gsTeamMembers)
    {
        const QString englishKey = Detail::normalizedName(member.name);
        const QString koreanKey = Detail::normalizedName(member.koreanName);
        request.validation.gsTeamMemberKeys.push_back({
            englishKey.toUtf8().toStdString(),
            koreanKey.toUtf8().toStdString()
        });
        request.gsTeamMembers.push_back({
            .englishValidationKey = englishKey.toUtf8().toStdString(),
            .koreanValidationKey = koreanKey.toUtf8().toStdString(),
            .normalizedEnglishMatchKey = englishKey.toStdU16String(),
            .normalizedKoreanMatchKey = koreanKey.toStdU16String(),
            .diagnosticEnglishName = member.name.toStdU16String(),
            .diagnosticKoreanName = member.koreanName.toStdU16String(),
            .name = Detail::simplifiedOrEmpty16(member.name),
            .koreanName = Detail::simplifiedOrEmpty16(member.koreanName),
            .position = Detail::trimmed16(member.position),
            .phoneNumber = Detail::trimmed16(member.phoneNumber),
            .birthday = Detail::trimmed16(member.birthday)
        });
    }

    if (plan.review)
    {
        ClassMngr::Next::Application::TeacherImportReview review;
        review.candidateGroups.reserve(
            static_cast<std::size_t>(plan.review->candidateGroups.size()));
        for (const KoreanTeacherImportGroup& group : plan.review->candidateGroups)
        {
            TeacherImportReviewGroup projected;
            projected.id = group.level.toUtf8().toStdString();
            projected.candidateValidationKeys.reserve(
                static_cast<std::size_t>(group.candidates.size()));
            for (const KoreanTeacherImportCandidate& candidate : group.candidates)
            {
                projected.candidateValidationKeys.push_back(
                    Detail::koreanTeacherNameKey(candidate.teacher.teacherKr)
                        .toUtf8().toStdString());
            }
            review.candidateGroups.push_back(std::move(projected));
        }

        review.decisions.reserve(
            static_cast<std::size_t>(plan.review->groupSelections.size()));
        for (const TeacherImportGroupSelection& selection :
             plan.review->groupSelections)
        {
            TeacherImportGroupMode mode;
            switch (selection.mode)
            {
            case TeacherImportSelectionMode::All:
                mode = TeacherImportGroupMode::All;
                break;
            case TeacherImportSelectionMode::Selected:
                mode = TeacherImportGroupMode::Selected;
                break;
            case TeacherImportSelectionMode::None:
                mode = TeacherImportGroupMode::None;
                break;
            default:
                mode = static_cast<TeacherImportGroupMode>(-1);
                break;
            }
            TeacherImportGroupDecision decision{
                selection.level.toUtf8().toStdString(), mode, {}};
            decision.selectedCandidateIndexes.reserve(
                static_cast<std::size_t>(selection.selectedCandidateIndexes.size()));
            for (const int index : selection.selectedCandidateIndexes)
            {
                decision.selectedCandidateIndexes.push_back(index);
            }
            review.decisions.push_back(std::move(decision));
        }
        request.review = std::move(review);
    }

    return request;
}

} // namespace ClassMngr::Next::Application
