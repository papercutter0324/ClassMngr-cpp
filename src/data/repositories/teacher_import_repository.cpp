#include "teacher_import_repository.h"

#include "data/database/sql_query_utils.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "next/application/teacher_import_use_case.h"

#include <QObject>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <cstdint>
#include <string>
#include <utility>

namespace
{
using namespace ClassMngr::Next::Application;

QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}

QString koreanTeacherNameKey(const QString& value)
{
    return TeacherImportNameUtils::hangulOnly(value);
}

std::u16string trimmed16(const QString& value)
{
    return value.trimmed().toStdU16String();
}

std::u16string simplifiedOrEmpty16(const QString& value)
{
    return value.trimmed().isEmpty()
        ? std::u16string{}
        : value.simplified().toStdU16String();
}

TeacherImportPersistenceFailure persistenceFailure(
    const TeacherImportPersistenceOperation operation,
    const QString& message = {}
    )
{
    return {
        .operation = operation,
        .message = message.toStdU16String()
    };
}

TeacherImportPersistenceResult<void> queryFailureResult(
    const TeacherImportPersistenceOperation operation,
    const QSqlQuery& query,
    const QString& action
    )
{
    return std::unexpected(persistenceFailure(
        operation,
        SqlQueryUtils::errorFor(query, action).userMessage()
        ));
}

TeacherImportUseCaseRequest useCaseRequest(const TeacherImportPlan& plan)
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
            koreanTeacherNameKey(teacher.teacherKr).toUtf8().toStdString();
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

    request.nativeEnglishTeachers.reserve(
        static_cast<std::size_t>(plan.nativeEnglishTeachers.size()));
    request.validation.nativeEnglishTeacherKeys.reserve(
        static_cast<std::size_t>(plan.nativeEnglishTeachers.size()));
    for (const NativeEnglishTeacher& teacher : plan.nativeEnglishTeachers)
    {
        const QString matchKey = normalizedName(teacher.name);
        const std::string validationKey = matchKey.toUtf8().toStdString();
        request.validation.nativeEnglishTeacherKeys.push_back(validationKey);
        request.nativeEnglishTeachers.push_back({
            .validationKey = validationKey,
            .normalizedMatchKey = matchKey.toStdU16String(),
            .diagnosticName = teacher.name.toStdU16String(),
            .name = teacher.name.simplified().toStdU16String(),
            .position = trimmed16(teacher.position),
            .phoneNumber = trimmed16(teacher.phoneNumber),
            .birthday = trimmed16(teacher.birthday),
            .nationality = trimmed16(teacher.nationality),
            .email = trimmed16(teacher.email)
        });
    }

    request.gsTeamMembers.reserve(
        static_cast<std::size_t>(plan.gsTeamMembers.size()));
    request.validation.gsTeamMemberKeys.reserve(
        static_cast<std::size_t>(plan.gsTeamMembers.size()));
    for (const GsTeamMember& member : plan.gsTeamMembers)
    {
        const QString englishKey = normalizedName(member.name);
        const QString koreanKey = normalizedName(member.koreanName);
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
            .name = simplifiedOrEmpty16(member.name),
            .koreanName = simplifiedOrEmpty16(member.koreanName),
            .position = trimmed16(member.position),
            .phoneNumber = trimmed16(member.phoneNumber),
            .birthday = trimmed16(member.birthday)
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
                    koreanTeacherNameKey(candidate.teacher.teacherKr)
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

class TeacherImportSqlPersistence final : public TeacherImportPersistencePort
{
public:
    explicit TeacherImportSqlPersistence(QSqlDatabase& database)
        : m_database(database)
    {
    }

    ~TeacherImportSqlPersistence() override
    {
        if (m_transactionActive)
        {
            m_database.rollback();
        }
    }

    TeacherImportPersistenceResult<void> beginTransaction() override
    {
        if (!m_database.transaction())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::BeginTransaction));
        }
        m_transactionActive = true;
        return {};
    }

    TeacherImportPersistenceResult<TeacherImportPersistenceSnapshot>
    loadSnapshot() override
    {
        TeacherImportPersistenceSnapshot snapshot;
        QSqlQuery query(m_database);
        if (!query.exec(R"(
            SELECT id, teacher_kr, room_number, birthday, phone_number
            FROM teachers
        )"))
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(query, QObject::tr("Loading Korean teachers"))
                    .userMessage()));
        }
        while (query.next())
        {
            const int id = query.value(0).toInt();
            const std::u16string teacherKr = query.value(1).toString().toStdU16String();
            const auto typedId = ClassMngr::Next::Domain::TeacherId::fromString(
                std::to_string(id));
            if (!typedId)
            {
                return std::unexpected(persistenceFailure(
                    TeacherImportPersistenceOperation::LoadSnapshot,
                    QObject::tr("The matched Korean teacher has an invalid identifier.")));
            }
            snapshot.koreanTeachers.push_back({
                .teacherId = *typedId,
                .key = ClassMngr::Next::Domain::KoreanTeacherKey::fromName(teacherKr),
                .teacherKr = teacherKr,
                .roomNumber = query.value(2).toString().toStdU16String(),
                .birthday = query.value(3).toString().toStdU16String(),
                .phoneNumber = query.value(4).toString().toStdU16String()
            });
        }
        if (query.lastError().isValid())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(query, QObject::tr("Loading Korean teachers"))
                    .userMessage()));
        }

        query = QSqlQuery(m_database);
        if (!query.exec(R"(
            SELECT id, name, position, phone_number, birthday, nationality, email
            FROM native_english_teachers
        )"))
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(
                    query, QObject::tr("Loading Native English Teachers"))
                    .userMessage()));
        }
        while (query.next())
        {
            NativeEnglishTeacherImportProfile profile{
                .id = query.value(0).toInt(),
                .name = query.value(1).toString().toStdU16String(),
                .position = query.value(2).toString().toStdU16String(),
                .phoneNumber = query.value(3).toString().toStdU16String(),
                .birthday = query.value(4).toString().toStdU16String(),
                .nationality = query.value(5).toString().toStdU16String(),
                .email = query.value(6).toString().toStdU16String()
            };
            snapshot.nativeEnglishTeachers.push_back({
                .profile = profile,
                .normalizedMatchKey = normalizedName(
                    QString::fromStdU16String(profile.name)).toStdU16String()
            });
        }
        if (query.lastError().isValid())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(
                    query, QObject::tr("Loading Native English Teachers"))
                    .userMessage()));
        }

        query = QSqlQuery(m_database);
        if (!query.exec(R"(
            SELECT id, name, korean_name, position, phone_number, birthday
            FROM gs_team
        )"))
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(query, QObject::tr("Loading GS Team members"))
                    .userMessage()));
        }
        while (query.next())
        {
            GsTeamImportProfile profile{
                .id = query.value(0).toInt(),
                .name = query.value(1).toString().toStdU16String(),
                .koreanName = query.value(2).toString().toStdU16String(),
                .position = query.value(3).toString().toStdU16String(),
                .phoneNumber = query.value(4).toString().toStdU16String(),
                .birthday = query.value(5).toString().toStdU16String()
            };
            snapshot.gsTeamMembers.push_back({
                .profile = profile,
                .normalizedEnglishMatchKey = normalizedName(
                    QString::fromStdU16String(profile.name)).toStdU16String(),
                .normalizedKoreanMatchKey = normalizedName(
                    QString::fromStdU16String(profile.koreanName)).toStdU16String()
            });
        }
        if (query.lastError().isValid())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(query, QObject::tr("Loading GS Team members"))
                    .userMessage()));
        }

        query = QSqlQuery(m_database);
        query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
        query.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        if (!query.exec())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(
                    query, QObject::tr("Loading the previous teacher import date"))
                    .userMessage()));
        }
        if (query.next())
        {
            const QDate current = QDate::fromString(
                query.value(0).toString(), Qt::ISODate);
            if (current.isValid())
            {
                snapshot.latestSourceDateIso = current.toString(Qt::ISODate).toStdString();
            }
        }
        if (query.lastError().isValid())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::LoadSnapshot,
                SqlQueryUtils::errorFor(
                    query, QObject::tr("Loading the previous teacher import date"))
                    .userMessage()));
        }
        return snapshot;
    }

    TeacherImportPersistenceResult<void> createKoreanTeacher(
        const TeacherImportKoreanSource& source,
        const std::u16string& canonicalTeacherKr
        ) override
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
        query.addBindValue(QString::fromStdU16String(canonicalTeacherKr));
        query.addBindValue(QString::fromStdU16String(source.teacherEn));
        query.addBindValue(QString::fromStdU16String(source.preferredRomanization));
        query.addBindValue(QString::fromStdU16String(source.preferredName));
        query.addBindValue(QString::fromStdU16String(source.roomNumber));
        query.addBindValue(QString::fromStdU16String(source.birthday));
        query.addBindValue(QString::fromStdU16String(source.phoneNumber));
        query.addBindValue(QString::fromStdU16String(source.wifiName));
        query.addBindValue(QString::fromStdU16String(source.wifiPassword));
        query.addBindValue(QString::fromStdU16String(source.internetType));
        query.addBindValue(QString::fromStdU16String(source.zoomId));
        query.addBindValue(QString::fromStdU16String(source.zoomPassword));
        query.addBindValue(QString::fromStdU16String(source.projectionType));
        query.addBindValue(QString::fromStdU16String(source.notes));
        if (!query.exec())
        {
            return queryFailureResult(
                TeacherImportPersistenceOperation::CreateKoreanTeacher,
                query,
                QObject::tr("Creating a Korean teacher"));
        }
        return {};
    }

    TeacherImportPersistenceResult<void> updateKoreanTeacher(
        const KoreanTeacherImportProfile& profile
        ) override
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE teachers
            SET teacher_kr=?, room_number=?, birthday=?, phone_number=?
            WHERE id=?
        )");
        query.addBindValue(QString::fromStdU16String(profile.teacherKr));
        query.addBindValue(QString::fromStdU16String(profile.roomNumber));
        query.addBindValue(QString::fromStdU16String(profile.birthday));
        query.addBindValue(QString::fromStdU16String(profile.phoneNumber));
        query.addBindValue(QString::fromStdString(profile.teacherId.value()).toInt());
        if (!query.exec())
        {
            return queryFailureResult(
                TeacherImportPersistenceOperation::UpdateKoreanTeacher,
                query,
                QObject::tr("Updating a Korean teacher"));
        }
        return {};
    }

    TeacherImportPersistenceResult<void> createNativeEnglishTeacher(
        const TeacherImportNativeEnglishSource& source
        ) override
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
            INSERT INTO native_english_teachers
                (name, position, phone_number, birthday, nationality, email)
            VALUES (?, ?, ?, ?, ?, ?)
        )");
        query.addBindValue(QString::fromStdU16String(source.name));
        query.addBindValue(QString::fromStdU16String(source.position));
        query.addBindValue(QString::fromStdU16String(source.phoneNumber));
        query.addBindValue(QString::fromStdU16String(source.birthday));
        query.addBindValue(QString::fromStdU16String(source.nationality));
        query.addBindValue(QString::fromStdU16String(source.email));
        if (!query.exec())
        {
            return queryFailureResult(
                TeacherImportPersistenceOperation::CreateNativeEnglishTeacher,
                query,
                QObject::tr("Creating a Native English Teacher"));
        }
        return {};
    }

    TeacherImportPersistenceResult<void> updateNativeEnglishTeacher(
        const NativeEnglishTeacherImportProfile& profile
        ) override
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE native_english_teachers
            SET name=?, position=?, phone_number=?, birthday=?, nationality=?, email=?
            WHERE id=?
        )");
        query.addBindValue(QString::fromStdU16String(profile.name));
        query.addBindValue(QString::fromStdU16String(profile.position));
        query.addBindValue(QString::fromStdU16String(profile.phoneNumber));
        query.addBindValue(QString::fromStdU16String(profile.birthday));
        query.addBindValue(QString::fromStdU16String(profile.nationality));
        query.addBindValue(QString::fromStdU16String(profile.email));
        query.addBindValue(profile.id);
        if (!query.exec())
        {
            return queryFailureResult(
                TeacherImportPersistenceOperation::UpdateNativeEnglishTeacher,
                query,
                QObject::tr("Updating a Native English Teacher"));
        }
        return {};
    }

    TeacherImportPersistenceResult<void> createGsTeamMember(
        const TeacherImportGsTeamSource& source
        ) override
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
            INSERT INTO gs_team
                (name, korean_name, position, phone_number, birthday)
            VALUES (?, ?, ?, ?, ?)
        )");
        query.addBindValue(QString::fromStdU16String(source.name));
        query.addBindValue(QString::fromStdU16String(source.koreanName));
        query.addBindValue(QString::fromStdU16String(source.position));
        query.addBindValue(QString::fromStdU16String(source.phoneNumber));
        query.addBindValue(QString::fromStdU16String(source.birthday));
        if (!query.exec())
        {
            return queryFailureResult(
                TeacherImportPersistenceOperation::CreateGsTeamMember,
                query,
                QObject::tr("Creating a GS Team member"));
        }
        return {};
    }

    TeacherImportPersistenceResult<void> updateGsTeamMember(
        const GsTeamImportProfile& profile
        ) override
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
            UPDATE gs_team
            SET name=?, korean_name=?, position=?, phone_number=?, birthday=?
            WHERE id=?
        )");
        query.addBindValue(QString::fromStdU16String(profile.name));
        query.addBindValue(QString::fromStdU16String(profile.koreanName));
        query.addBindValue(QString::fromStdU16String(profile.position));
        query.addBindValue(QString::fromStdU16String(profile.phoneNumber));
        query.addBindValue(QString::fromStdU16String(profile.birthday));
        query.addBindValue(profile.id);
        if (!query.exec())
        {
            return queryFailureResult(
                TeacherImportPersistenceOperation::UpdateGsTeamMember,
                query,
                QObject::tr("Updating a GS Team member"));
        }
        return {};
    }

    TeacherImportPersistenceResult<void> saveLatestSourceDate(
        const std::string_view sourceDateIso
        ) override
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
            INSERT INTO app_settings (key, value) VALUES (?, ?)
            ON CONFLICT(key) DO UPDATE SET value=excluded.value
        )");
        query.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        query.addBindValue(QString::fromStdString(std::string(sourceDateIso)));
        if (!query.exec())
        {
            return queryFailureResult(
                TeacherImportPersistenceOperation::SaveLatestSourceDate,
                query,
                QObject::tr("Saving the teacher import date"));
        }
        return {};
    }

    TeacherImportPersistenceResult<void> commitTransaction() override
    {
        if (!m_transactionActive || !m_database.commit())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::CommitTransaction));
        }
        m_transactionActive = false;
        return {};
    }

    TeacherImportPersistenceResult<void> rollbackTransaction() override
    {
        if (!m_transactionActive)
        {
            return {};
        }
        if (!m_database.rollback())
        {
            return std::unexpected(persistenceFailure(
                TeacherImportPersistenceOperation::RollbackTransaction,
                QObject::tr("Unable to roll back the teacher import transaction.")));
        }
        m_transactionActive = false;
        return {};
    }

private:
    QSqlDatabase& m_database;
    bool m_transactionActive = false;
};

QString validationMessage(
    const TeacherImportPlanValidationIssue issue
    )
{
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
        return QObject::tr(
            "The import contains a duplicate Native English Teacher name.");
    case TeacherImportPlanValidationIssue::MissingGsTeamMemberName:
        return QObject::tr("Every imported GS Team member must have a name.");
    case TeacherImportPlanValidationIssue::DuplicateGsTeamMemberName:
        return QObject::tr("The import contains a duplicate GS Team name.");
    case TeacherImportPlanValidationIssue::None:
        break;
    }
    return {};
}

QString persistenceMessage(const TeacherImportUseCaseError& error)
{
    if (!error.persistenceFailure.message.empty())
    {
        return QString::fromStdU16String(error.persistenceFailure.message);
    }
    switch (error.persistenceFailure.operation)
    {
    case TeacherImportPersistenceOperation::BeginTransaction:
        return QObject::tr("Unable to start the teacher import transaction.");
    case TeacherImportPersistenceOperation::CommitTransaction:
        return QObject::tr("Unable to commit the teacher import transaction.");
    case TeacherImportPersistenceOperation::RollbackTransaction:
        return QObject::tr("Unable to roll back the teacher import transaction.");
    case TeacherImportPersistenceOperation::LoadSnapshot:
    case TeacherImportPersistenceOperation::CreateKoreanTeacher:
    case TeacherImportPersistenceOperation::UpdateKoreanTeacher:
    case TeacherImportPersistenceOperation::CreateNativeEnglishTeacher:
    case TeacherImportPersistenceOperation::UpdateNativeEnglishTeacher:
    case TeacherImportPersistenceOperation::CreateGsTeamMember:
    case TeacherImportPersistenceOperation::UpdateGsTeamMember:
    case TeacherImportPersistenceOperation::SaveLatestSourceDate:
        return QObject::tr("The teacher import could not be completed.");
    }
    return QObject::tr("The teacher import could not be completed.");
}

QString useCaseErrorMessage(const TeacherImportUseCaseError& error)
{
    switch (error.kind)
    {
    case TeacherImportUseCaseErrorKind::InvalidRequest:
    case TeacherImportUseCaseErrorKind::InvalidReview:
        return QObject::tr("The teacher import review choices are invalid.");
    case TeacherImportUseCaseErrorKind::InvalidPlan:
        return validationMessage(error.validationIssue);
    case TeacherImportUseCaseErrorKind::AmbiguousStoredMatch:
        switch (error.recordNamespace)
        {
        case TeacherImportRecordNamespace::KoreanTeacher:
            return QObject::tr("More than one stored Korean teacher matches %1.")
                .arg(QString::fromStdU16String(error.matchLabel));
        case TeacherImportRecordNamespace::NativeEnglishTeacher:
            return QObject::tr(
                "More than one stored Native English Teacher matches %1.")
                .arg(QString::fromStdU16String(error.matchLabel));
        case TeacherImportRecordNamespace::GsTeamMember:
            return QObject::tr("More than one stored GS Team member matches %1.")
                .arg(QString::fromStdU16String(error.matchLabel));
        }
        break;
    case TeacherImportUseCaseErrorKind::KoreanMatchPolicyRejected:
        return QObject::tr(
            "The matched Korean teacher no longer matches the import key.");
    case TeacherImportUseCaseErrorKind::PersistenceFailure:
        return persistenceMessage(error);
    }
    return QObject::tr("The teacher import could not be completed.");
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
    const TeacherImportUseCaseRequest request = useCaseRequest(plan);
    TeacherImportSqlPersistence persistence(m_database);
    const auto applied = TeacherImportUseCase::execute(request, persistence);
    if (!applied)
    {
        if (applied.error().rollbackFailure)
        {
            QString rollbackMessage = QString::fromStdU16String(
                applied.error().rollbackFailure->message);
            if (rollbackMessage.isEmpty())
            {
                rollbackMessage = QObject::tr(
                    "Unable to roll back the teacher import transaction.");
            }
            qWarning().noquote()
                << QObject::tr("Teacher import rollback also failed: %1")
                       .arg(rollbackMessage);
        }
        return std::unexpected(useCaseErrorMessage(applied.error()));
    }

    return TeacherImportSummary{
        .koreanTeachers = {
            .created = applied->koreanTeachers.created,
            .updated = applied->koreanTeachers.updated,
            .unchanged = applied->koreanTeachers.unchanged
        },
        .nativeEnglishTeachers = {
            .created = applied->nativeEnglishTeachers.created,
            .updated = applied->nativeEnglishTeachers.updated,
            .unchanged = applied->nativeEnglishTeachers.unchanged
        },
        .gsTeamMembers = {
            .created = applied->gsTeamMembers.created,
            .updated = applied->gsTeamMembers.updated,
            .unchanged = applied->gsTeamMembers.unchanged
        }
    };
}
