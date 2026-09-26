#include "teacher_import_repository.h"

#include "data/database/database_transaction.h"
#include "data/database/sql_query_utils.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "next/application/import_review_session.h"
#include "next/application/gs_team_import_update.h"
#include "next/application/korean_teacher_import_update.h"
#include "next/application/native_english_teacher_import_update.h"
#include "next/application/teacher_import_plan_validation.h"

#include <QObject>
#include <QSqlError>
#include <QSqlQuery>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

QString koreanTeacherNameKey(const QString& value)
{
    return TeacherImportNameUtils::hangulOnly(value);
}

struct TeacherImportReviewRequest
{
    std::vector<ClassMngr::Next::Application::TeacherImportCandidateGroup> groups;
    std::vector<ClassMngr::Next::Application::TeacherImportGroupDecision> decisions;
};

TeacherImportReviewRequest reviewRequest(const TeacherImportReview& review)
{
    using namespace ClassMngr::Next::Application;
    TeacherImportReviewRequest request;
    request.groups.reserve(static_cast<std::size_t>(review.candidateGroups.size()));
    request.decisions.reserve(static_cast<std::size_t>(review.groupSelections.size()));

    for (const KoreanTeacherImportGroup& group : review.candidateGroups)
    {
        request.groups.push_back({
            group.level.toUtf8().toStdString(),
            static_cast<std::size_t>(group.candidates.size())
        });
    }
    for (const TeacherImportGroupSelection& selection : review.groupSelections)
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
        request.decisions.push_back(std::move(decision));
    }
    return request;
}

Status projectReviewPlan(
    const TeacherImportPlan& plan,
    ClassMngr::Next::Application::TeacherImportPlanValidationInput& input
    )
{
    if (!plan.review)
    {
        return {};
    }

    const TeacherImportReviewRequest request = reviewRequest(*plan.review);
    const auto resolution = ClassMngr::Next::Application::resolveTeacherImportReview(
        request.groups, request.decisions);
    if (!resolution.accepted())
    {
        return std::unexpected(QObject::tr("The teacher import review choices are invalid."));
    }

    auto& reviewedTeacherKeys = input.reviewedKoreanTeacherKeys.emplace();
    for (std::size_t groupIndex = 0;
         groupIndex < resolution.selectedCandidateIndexes.size();
         ++groupIndex)
    {
        const KoreanTeacherImportGroup& group =
            plan.review->candidateGroups.at(static_cast<qsizetype>(groupIndex));
        for (const std::size_t candidateIndex :
             resolution.selectedCandidateIndexes[groupIndex])
        {
            reviewedTeacherKeys.push_back(koreanTeacherNameKey(
                group.candidates.at(static_cast<qsizetype>(candidateIndex))
                    .teacher.teacherKr).toUtf8().toStdString());
        }
    }
    return {};
}

QString queryFailure(const QSqlQuery& query, const QString& action)
{
    return SqlQueryUtils::errorFor(query, action).userMessage();
}

QString validationMessage(
    const ClassMngr::Next::Application::TeacherImportPlanValidationIssue issue
    )
{
    using ClassMngr::Next::Application::TeacherImportPlanValidationIssue;
    switch (issue)
    {
    case TeacherImportPlanValidationIssue::ReviewSelectionMismatch:
        return QObject::tr(
            "The reviewed Korean teachers do not match the import selection.");
    case TeacherImportPlanValidationIssue::InvalidSourceDate:
        return QObject::tr("The teacher import date is invalid.");
    case TeacherImportPlanValidationIssue::MissingKoreanTeacherName:
        return QObject::tr("Every imported Korean teacher must have a name.");
    case TeacherImportPlanValidationIssue::DuplicateKoreanTeacherName:
        return QObject::tr("The import contains a duplicate Korean teacher name.");
    case TeacherImportPlanValidationIssue::MissingNativeEnglishTeacherName:
        return QObject::tr("Every imported Native English Teacher must have a name.");
    case TeacherImportPlanValidationIssue::DuplicateNativeEnglishTeacherName:
        return QObject::tr("The import contains a duplicate Native English Teacher name.");
    case TeacherImportPlanValidationIssue::MissingGsTeamMemberName:
        return QObject::tr("Every imported GS Team member must have a name.");
    case TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName:
        return QObject::tr("The import contains a duplicate GS Team name.");
    case TeacherImportPlanValidationIssue::None:
        break;
    }
    return {};
}

Status validatePlan(const TeacherImportPlan& plan)
{
    ClassMngr::Next::Application::TeacherImportPlanValidationInput input;
    const Status reviewStatus = projectReviewPlan(plan, input);
    if (!reviewStatus)
    {
        return reviewStatus;
    }

    input.sourceDateValid = plan.sourceDate.isValid();
    input.koreanTeacherKeys.reserve(
        static_cast<std::size_t>(plan.koreanTeachers.size()));
    for (const Teacher& teacher : plan.koreanTeachers)
    {
        input.koreanTeacherKeys.push_back(
            koreanTeacherNameKey(teacher.teacherKr).toUtf8().toStdString());
    }

    input.nativeEnglishTeacherKeys.reserve(
        static_cast<std::size_t>(plan.nativeEnglishTeachers.size()));
    for (const NativeEnglishTeacher& teacher : plan.nativeEnglishTeachers)
    {
        input.nativeEnglishTeacherKeys.push_back(
            normalizedName(teacher.name).toUtf8().toStdString());
    }

    input.gsTeamMemberKeys.reserve(
        static_cast<std::size_t>(plan.gsTeamMembers.size()));
    for (const GsTeamMember& member : plan.gsTeamMembers)
    {
        input.gsTeamMemberKeys.push_back({
            normalizedName(member.name).toUtf8().toStdString(),
            normalizedName(member.koreanName).toUtf8().toStdString()
        });
    }

    const auto issue =
        ClassMngr::Next::Application::validateTeacherImportPlan(input);
    if (issue == ClassMngr::Next::Application::TeacherImportPlanValidationIssue::None)
    {
        return {};
    }
    return std::unexpected(validationMessage(issue));
}

Result<QList<Teacher>> loadKoreanTeachers(QSqlDatabase& database)
{
    QList<Teacher> result;
    QSqlQuery query(database);
    if (!query.exec(R"(
        SELECT id, teacher_kr, teacher_en, preferred_romanization, preferred_name,
               room_number, birthday, phone_number, wifi_name, wifi_password,
               internet_type, zoom_id, zoom_password, projection_type, notes
        FROM teachers
    )"))
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading Korean teachers")));
    }

    while (query.next())
    {
        Teacher teacher;
        teacher.id = query.value(0).toInt();
        teacher.teacherKr = query.value(1).toString();
        teacher.teacherEn = query.value(2).toString();
        teacher.preferredRomanization = query.value(3).toString();
        teacher.preferredName = query.value(4).toString();
        teacher.roomNumber = query.value(5).toString();
        teacher.birthday = query.value(6).toString();
        teacher.phoneNumber = query.value(7).toString();
        teacher.wifiName = query.value(8).toString();
        teacher.wifiPassword = query.value(9).toString();
        teacher.internetType = query.value(10).toString();
        teacher.zoomId = query.value(11).toString();
        teacher.zoomPassword = query.value(12).toString();
        teacher.projectionType = query.value(13).toString();
        teacher.notes = query.value(14).toString();
        result.append(teacher);
    }
    return result;
}

Result<QList<NativeEnglishTeacher>> loadNativeTeachers(QSqlDatabase& database)
{
    QList<NativeEnglishTeacher> result;
    QSqlQuery query(database);
    if (!query.exec(R"(
        SELECT id, name, position, phone_number, birthday, nationality, email
        FROM native_english_teachers
    )"))
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading Native English Teachers")));
    }
    while (query.next())
    {
        result.append({
            query.value(0).toInt(), query.value(1).toString(),
            query.value(2).toString(), query.value(3).toString(),
            query.value(4).toString(), query.value(5).toString(),
            query.value(6).toString()
        });
    }
    return result;
}

Result<QList<GsTeamMember>> loadGsTeam(QSqlDatabase& database)
{
    QList<GsTeamMember> result;
    QSqlQuery query(database);
    if (!query.exec(R"(
        SELECT id, name, korean_name, position, phone_number, birthday
        FROM gs_team
    )"))
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading GS Team members")));
    }
    while (query.next())
    {
        result.append({
            query.value(0).toInt(), query.value(1).toString(),
            query.value(2).toString(), query.value(3).toString(),
            query.value(4).toString(), query.value(5).toString()
        });
    }
    return result;
}

template<typename T, typename Name>
QList<int> matchingIndexes(const QList<T>& values, const QString& key, Name name)
{
    QList<int> result;
    for (int index = 0; index < values.size(); ++index)
    {
        if (normalizedName(name(values.at(index))) == key)
        {
            result.append(index);
        }
    }
    return result;
}

QList<int> matchingKoreanTeacherIndexes(
    const QList<Teacher>& teachers,
    const QString& key
    )
{
    QList<int> result;
    for (int index = 0; index < teachers.size(); ++index)
    {
        if (koreanTeacherNameKey(teachers.at(index).teacherKr) == key)
        {
            result.append(index);
        }
    }
    return result;
}

Status updateLatestDate(QSqlDatabase& database, const QDate& sourceDate)
{
    QDate current;
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
    query.addBindValue(QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting));
    if (!query.exec())
    {
        return std::unexpected(queryFailure(query, QObject::tr("Loading the previous teacher import date")));
    }
    if (query.next())
    {
        current = QDate::fromString(query.value(0).toString(), Qt::ISODate);
    }

    if (current.isValid() && sourceDate <= current)
    {
        return {};
    }

    query.prepare(R"(
        INSERT INTO app_settings (key, value) VALUES (?, ?)
        ON CONFLICT(key) DO UPDATE SET value=excluded.value
    )");
    query.addBindValue(QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting));
    query.addBindValue(sourceDate.toString(Qt::ISODate));
    if (!query.exec())
    {
        return std::unexpected(queryFailure(query, QObject::tr("Saving the teacher import date")));
    }
    return {};
}
}

TeacherImportRepository::TeacherImportRepository(QSqlDatabase& database)
    : m_database(database)
{
}

Result<TeacherImportSummary> TeacherImportRepository::importTeachers(
    const TeacherImportPlan& plan
    )
{
    const Status valid = validatePlan(plan);
    if (!valid)
    {
        return std::unexpected(valid.error());
    }

    DatabaseTransaction transaction(m_database);
    if (!transaction.started())
    {
        return std::unexpected(QObject::tr("Unable to start the teacher import transaction."));
    }

    auto koreanResult = loadKoreanTeachers(m_database);
    auto nativeResult = loadNativeTeachers(m_database);
    auto gsResult = loadGsTeam(m_database);
    if (!koreanResult) return std::unexpected(koreanResult.error());
    if (!nativeResult) return std::unexpected(nativeResult.error());
    if (!gsResult) return std::unexpected(gsResult.error());

    QList<Teacher> korean = *koreanResult;
    QList<NativeEnglishTeacher> native = *nativeResult;
    QList<GsTeamMember> gs = *gsResult;
    TeacherImportSummary summary;

    for (const Teacher& source : plan.koreanTeachers)
    {
        const QString key = koreanTeacherNameKey(source.teacherKr);
        const QList<int> matches = matchingKoreanTeacherIndexes(korean, key);
        if (matches.size() > 1)
        {
            return std::unexpected(QObject::tr("More than one stored Korean teacher matches %1.").arg(source.teacherKr));
        }

        if (matches.isEmpty())
        {
            QSqlQuery query(m_database);
            query.prepare(R"(
                INSERT INTO teachers
                    (teacher_kr, teacher_en, preferred_romanization, preferred_name,
                     room_number, birthday, phone_number,
                     wifi_name, wifi_password, internet_type,
                     zoom_id, zoom_password, projection_type, notes)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");
            query.addBindValue(key);
            query.addBindValue(source.teacherEn.trimmed());
            query.addBindValue(source.preferredRomanization.trimmed());
            query.addBindValue(source.preferredName.trimmed());
            query.addBindValue(source.roomNumber.trimmed());
            query.addBindValue(source.birthday.trimmed());
            query.addBindValue(source.phoneNumber.trimmed());
            query.addBindValue(source.wifiName.trimmed());
            query.addBindValue(source.wifiPassword.trimmed());
            query.addBindValue(source.internetType.trimmed());
            query.addBindValue(source.zoomId.trimmed());
            query.addBindValue(source.zoomPassword.trimmed());
            query.addBindValue(source.projectionType.trimmed());
            query.addBindValue(source.notes.trimmed());
            if (!query.exec())
            {
                return std::unexpected(queryFailure(query, QObject::tr("Creating a Korean teacher")));
            }
            ++summary.koreanTeachers.created;
            continue;
        }

        const Teacher& existing = korean.at(matches.first());
        using ClassMngr::Next::Application::KoreanTeacherImportFields;
        using ClassMngr::Next::Application::KoreanTeacherImportProfile;
        using ClassMngr::Next::Application::mergeMatchedKoreanTeacherImport;
        using ClassMngr::Next::Domain::TeacherId;

        const auto existingId = TeacherId::fromString(
            std::to_string(existing.id));
        if (!existingId)
        {
            return std::unexpected(QObject::tr(
                "The matched Korean teacher has an invalid identifier."));
        }
        const std::u16string existingName =
            existing.teacherKr.toStdU16String();
        const std::u16string importedName =
            source.teacherKr.toStdU16String();
        const auto update = mergeMatchedKoreanTeacherImport(
            KoreanTeacherImportProfile{
                .teacherId = *existingId,
                .key = ClassMngr::Next::Domain::KoreanTeacherKey::fromName(
                    existingName),
                .teacherKr = existingName,
                .roomNumber = existing.roomNumber.toStdU16String(),
                .birthday = existing.birthday.toStdU16String(),
                .phoneNumber = existing.phoneNumber.toStdU16String()
            },
            KoreanTeacherImportFields{
                .key = ClassMngr::Next::Domain::KoreanTeacherKey::fromName(
                    importedName),
                .teacherKr = importedName,
                .roomNumber = source.roomNumber.trimmed().toStdU16String(),
                .birthday = source.birthday.trimmed().toStdU16String(),
                .phoneNumber = source.phoneNumber.trimmed().toStdU16String()
            }
            );
        if (!update)
        {
            return std::unexpected(QObject::tr(
                "The matched Korean teacher no longer matches the import key."));
        }
        if (update->profile.teacherId != *existingId
            || update->profile.teacherId.value() != std::to_string(existing.id))
        {
            return std::unexpected(QObject::tr(
                "The Korean teacher import update changed the stored identity."));
        }
        if (!update->changed)
        {
            ++summary.koreanTeachers.unchanged;
            continue;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE teachers
            SET teacher_kr=?, room_number=?, birthday=?, phone_number=?
            WHERE id=?
        )");
        query.addBindValue(QString::fromStdU16String(update->profile.teacherKr));
        query.addBindValue(QString::fromStdU16String(update->profile.roomNumber));
        query.addBindValue(QString::fromStdU16String(update->profile.birthday));
        query.addBindValue(QString::fromStdU16String(update->profile.phoneNumber));
        query.addBindValue(existing.id);
        if (!query.exec())
        {
            return std::unexpected(queryFailure(query, QObject::tr("Updating a Korean teacher")));
        }
        ++summary.koreanTeachers.updated;
    }

    for (const NativeEnglishTeacher& source : plan.nativeEnglishTeachers)
    {
        const QString key = normalizedName(source.name);
        const QList<int> matches = matchingIndexes(
            native, key, [](const NativeEnglishTeacher& value) { return value.name; });
        if (matches.size() > 1)
        {
            return std::unexpected(QObject::tr("More than one stored Native English Teacher matches %1.").arg(source.name));
        }

        if (matches.isEmpty())
        {
            QSqlQuery query(m_database);
            query.prepare(R"(
                INSERT INTO native_english_teachers
                    (name, position, phone_number, birthday, nationality, email)
                VALUES (?, ?, ?, ?, ?, ?)
            )");
            query.addBindValue(source.name.simplified());
            query.addBindValue(source.position.trimmed());
            query.addBindValue(source.phoneNumber.trimmed());
            query.addBindValue(source.birthday.trimmed());
            query.addBindValue(source.nationality.trimmed());
            query.addBindValue(source.email.trimmed());
            if (!query.exec())
            {
                return std::unexpected(queryFailure(query, QObject::tr("Creating a Native English Teacher")));
            }
            ++summary.nativeEnglishTeachers.created;
            continue;
        }

        const NativeEnglishTeacher& existing = native.at(matches.first());
        using ClassMngr::Next::Application::NativeEnglishTeacherImportFields;
        using ClassMngr::Next::Application::NativeEnglishTeacherImportProfile;
        using ClassMngr::Next::Application::mergeNativeEnglishTeacherImport;

        const auto update = mergeNativeEnglishTeacherImport(
            NativeEnglishTeacherImportProfile{
                .id = existing.id,
                .name = existing.name.toStdU16String(),
                .position = existing.position.toStdU16String(),
                .phoneNumber = existing.phoneNumber.toStdU16String(),
                .birthday = existing.birthday.toStdU16String(),
                .nationality = existing.nationality.toStdU16String(),
                .email = existing.email.toStdU16String()
            },
            NativeEnglishTeacherImportFields{
                .name = source.name.simplified().toStdU16String(),
                .position = source.position.trimmed().toStdU16String(),
                .phoneNumber = source.phoneNumber.trimmed().toStdU16String(),
                .birthday = source.birthday.trimmed().toStdU16String(),
                .nationality = source.nationality.trimmed().toStdU16String(),
                .email = source.email.trimmed().toStdU16String()
            }
            );
        if (!update.changed)
        {
            ++summary.nativeEnglishTeachers.unchanged;
            continue;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE native_english_teachers
            SET name=?, position=?, phone_number=?, birthday=?, nationality=?, email=?
            WHERE id=?
        )");
        query.addBindValue(QString::fromStdU16String(update.profile.name));
        query.addBindValue(QString::fromStdU16String(update.profile.position));
        query.addBindValue(QString::fromStdU16String(update.profile.phoneNumber));
        query.addBindValue(QString::fromStdU16String(update.profile.birthday));
        query.addBindValue(QString::fromStdU16String(update.profile.nationality));
        query.addBindValue(QString::fromStdU16String(update.profile.email));
        query.addBindValue(update.profile.id);
        if (!query.exec())
        {
            return std::unexpected(queryFailure(query, QObject::tr("Updating a Native English Teacher")));
        }
        ++summary.nativeEnglishTeachers.updated;
    }

    for (const GsTeamMember& source : plan.gsTeamMembers)
    {
        const bool useKorean = !source.koreanName.trimmed().isEmpty();
        const QString key = normalizedName(useKorean ? source.koreanName : source.name);
        const QList<int> matches = useKorean
            ? matchingIndexes(gs, key, [](const GsTeamMember& value) { return value.koreanName; })
            : matchingIndexes(gs, key, [](const GsTeamMember& value) { return value.name; });
        if (matches.size() > 1)
        {
            return std::unexpected(QObject::tr("More than one stored GS Team member matches %1.")
                .arg(useKorean ? source.koreanName : source.name));
        }

        if (matches.isEmpty())
        {
            QSqlQuery query(m_database);
            query.prepare(R"(
                INSERT INTO gs_team
                    (name, korean_name, position, phone_number, birthday)
                VALUES (?, ?, ?, ?, ?)
            )");
            query.addBindValue(source.name.simplified());
            query.addBindValue(source.koreanName.simplified());
            query.addBindValue(source.position.trimmed());
            query.addBindValue(source.phoneNumber.trimmed());
            query.addBindValue(source.birthday.trimmed());
            if (!query.exec())
            {
                return std::unexpected(queryFailure(query, QObject::tr("Creating a GS Team member")));
            }
            ++summary.gsTeamMembers.created;
            continue;
        }

        const GsTeamMember& existing = gs.at(matches.first());
        using ClassMngr::Next::Application::GsTeamImportFields;
        using ClassMngr::Next::Application::GsTeamImportProfile;
        using ClassMngr::Next::Application::mergeGsTeamImport;

        const auto update = mergeGsTeamImport(
            GsTeamImportProfile{
                .id = existing.id,
                .name = existing.name.toStdU16String(),
                .koreanName = existing.koreanName.toStdU16String(),
                .position = existing.position.toStdU16String(),
                .phoneNumber = existing.phoneNumber.toStdU16String(),
                .birthday = existing.birthday.toStdU16String()
            },
            GsTeamImportFields{
                .name = source.name.trimmed().isEmpty()
                    ? std::u16string{} : source.name.simplified().toStdU16String(),
                .koreanName = source.koreanName.trimmed().isEmpty()
                    ? std::u16string{} : source.koreanName.simplified().toStdU16String(),
                .position = source.position.trimmed().toStdU16String(),
                .phoneNumber = source.phoneNumber.trimmed().toStdU16String(),
                .birthday = source.birthday.trimmed().toStdU16String()
            }
            );
        if (!update.changed)
        {
            ++summary.gsTeamMembers.unchanged;
            continue;
        }

        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE gs_team
            SET name=?, korean_name=?, position=?, phone_number=?, birthday=?
            WHERE id=?
        )");
        query.addBindValue(QString::fromStdU16String(update.profile.name));
        query.addBindValue(QString::fromStdU16String(update.profile.koreanName));
        query.addBindValue(QString::fromStdU16String(update.profile.position));
        query.addBindValue(QString::fromStdU16String(update.profile.phoneNumber));
        query.addBindValue(QString::fromStdU16String(update.profile.birthday));
        query.addBindValue(update.profile.id);
        if (!query.exec())
        {
            return std::unexpected(queryFailure(query, QObject::tr("Updating a GS Team member")));
        }
        ++summary.gsTeamMembers.updated;
    }

    const Status dateStatus = updateLatestDate(m_database, plan.sourceDate);
    if (!dateStatus)
    {
        return std::unexpected(dateStatus.error());
    }

    if (!transaction.commit())
    {
        return std::unexpected(QObject::tr("Unable to commit the teacher import transaction."));
    }

    return summary;
}
