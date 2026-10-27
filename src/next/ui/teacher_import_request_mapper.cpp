#include "next/ui/teacher_import_request_mapper.h"

#include "features/teacher/import/teacher_import_name_utils.h"
#include "next/application/import_review_session.h"

#include <QDate>
#include <QString>

#include <cstddef>
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

std::u16string trimmed16(const QString& value)
{
    return value.trimmed().toStdU16String();
}

QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

Application::ImportSourceKey koreanValidationKey(const Teacher& teacher)
{
    return Application::ImportSourceKey{
        utf8(TeacherImportNameUtils::hangulOnly(teacher.teacherKr))
    };
}

}  // namespace

std::expected<
    Application::TeacherImportUseCaseRequest,
    Application::TeacherImportReviewIssue
    > teacherImportPreviewToUseCaseRequest(
        const TeacherImportPreview& preview,
        const std::vector<Application::TeacherImportGroupDecision>& decisions
        )
{
    using namespace Application;

    TeacherImportUseCaseRequest request;
    request.sourceDateIso = preview.sourceDate.toString(Qt::ISODate).toStdString();
    request.validation.sourceDateValid = preview.sourceDate.isValid();
    request.nativeEnglishTeachers.reserve(
        static_cast<std::size_t>(preview.nativeEnglishTeachers.size()));
    request.gsTeamMembers.reserve(
        static_cast<std::size_t>(preview.gsTeamMembers.size()));

    TeacherImportReview review;
    review.candidateGroups.reserve(
        static_cast<std::size_t>(preview.koreanGroups.size()));
    review.decisions = decisions;

    std::vector<TeacherImportCandidateGroup> candidateGroups;
    candidateGroups.reserve(
        static_cast<std::size_t>(preview.koreanGroups.size()));

    for (const KoreanTeacherImportGroup& group : preview.koreanGroups)
    {
        TeacherImportReviewGroup requestGroup;
        requestGroup.id = TeacherImportGroupKey(utf8(group.level));
        requestGroup.candidateValidationKeys.reserve(
            static_cast<std::size_t>(group.candidates.size()));

        for (const KoreanTeacherImportCandidate& candidate : group.candidates)
        {
            requestGroup.candidateValidationKeys.push_back(
                koreanValidationKey(candidate.teacher));
        }

        candidateGroups.push_back({
            requestGroup.id,
            TeacherImportCandidateCount{
                requestGroup.candidateValidationKeys.size()
            }
        });
        review.candidateGroups.push_back(std::move(requestGroup));
    }

    const TeacherImportReviewResolution resolution =
        resolveTeacherImportReview(candidateGroups, review.decisions);
    if (!resolution.accepted())
    {
        return std::unexpected(resolution.issue);
    }

    for (std::size_t groupIndex = 0;
         groupIndex < preview.koreanGroups.size();
         ++groupIndex)
    {
        const KoreanTeacherImportGroup& group =
            preview.koreanGroups.at(static_cast<qsizetype>(groupIndex));
        for (const TeacherImportResolvedCandidateIndex candidateIndex :
             resolution.selectedCandidateIndexes.at(groupIndex))
        {
            const Teacher& teacher = group.candidates.at(
                static_cast<qsizetype>(candidateIndex.rawValue())).teacher;
            const ImportSourceKey validationKey = koreanValidationKey(teacher);
            request.validation.koreanTeacherKeys.push_back(validationKey);
            request.koreanTeachers.push_back({
                .validationKey = validationKey,
                .teacherKr = teacher.teacherKr.toStdU16String(),
                .teacherEn = trimmed16(teacher.teacherEn),
                .preferredRomanization = trimmed16(teacher.preferredRomanization),
                .preferredName = trimmed16(teacher.preferredName),
                .roomNumber = trimmed16(teacher.roomNumber),
                .birthday = trimmed16(teacher.birthday),
                .phoneNumber = trimmed16(teacher.phoneNumber),
                .wifiName = trimmed16(teacher.wifiName),
                .wifiPassword = trimmed16(teacher.wifiPassword),
                .internetType = trimmed16(teacher.internetType),
                .zoomId = trimmed16(teacher.zoomId),
                .zoomPassword = trimmed16(teacher.zoomPassword),
                .projectionType = trimmed16(teacher.projectionType),
                .notes = trimmed16(teacher.notes)
            });
        }
    }

    for (const NativeEnglishTeacher& teacher : preview.nativeEnglishTeachers)
    {
        request.nativeEnglishTeachers.push_back({
            .diagnosticName = teacher.name.toStdU16String(),
            .name = teacher.name.simplified().toStdU16String(),
            .position = trimmed16(teacher.position),
            .phoneNumber = trimmed16(teacher.phoneNumber),
            .birthday = trimmed16(teacher.birthday),
            .nationality = trimmed16(teacher.nationality),
            .email = trimmed16(teacher.email)
        });
    }

    for (const GsTeamMember& member : preview.gsTeamMembers)
    {
        const QString englishKey = normalizedName(member.name);
        const QString koreanKey = normalizedName(member.koreanName);
        const ImportSourceKey englishValidationKey{utf8(englishKey)};
        const ImportSourceKey koreanValidationKey{utf8(koreanKey)};

        request.validation.gsTeamMemberKeys.push_back({
            englishValidationKey,
            koreanValidationKey
        });
        request.gsTeamMembers.push_back({
            .englishValidationKey = englishValidationKey,
            .koreanValidationKey = koreanValidationKey,
            .normalizedEnglishMatchKey = englishKey.toStdU16String(),
            .normalizedKoreanMatchKey = koreanKey.toStdU16String(),
            .diagnosticEnglishName = member.name.toStdU16String(),
            .diagnosticKoreanName = member.koreanName.toStdU16String(),
            .name = member.name.trimmed().isEmpty()
                ? std::u16string{}
                : member.name.simplified().toStdU16String(),
            .koreanName = member.koreanName.trimmed().isEmpty()
                ? std::u16string{}
                : member.koreanName.simplified().toStdU16String(),
            .position = trimmed16(member.position),
            .phoneNumber = trimmed16(member.phoneNumber),
            .birthday = trimmed16(member.birthday)
        });
    }

    request.review = std::move(review);
    return request;
}

}  // namespace ClassMngr::Next::Ui
