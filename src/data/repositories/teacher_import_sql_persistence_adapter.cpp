#include "teacher_import_sql_persistence_adapter.h"

#include "data/database/sql_query_utils.h"
#include "data/repositories/teacher_import_repository.h"
#include "features/teacher/import/teacher_import_name_utils.h"

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <memory>
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

class TeacherImportSqlPersistenceAdapter final : public TeacherImportPersistencePort
{
public:
    explicit TeacherImportSqlPersistenceAdapter(QSqlDatabase& database)
        : m_database(database)
    {
    }

    ~TeacherImportSqlPersistenceAdapter() override
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
}

std::unique_ptr<ClassMngr::Next::Application::TeacherImportPersistencePort>
makeTeacherImportSqlPersistenceAdapter(QSqlDatabase& database)
{
    return std::make_unique<TeacherImportSqlPersistenceAdapter>(database);
}
