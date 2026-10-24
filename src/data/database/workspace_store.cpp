#include "data/database/workspace_store.h"

#include "data/database/database_transaction.h"
#include "data/database/database_schema_manager.h"
#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/campus_record_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_transfer_repository.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/schedule_import_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "data/repositories/testing_class_repository.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QUrl>
#include <QUrlQuery>

#include <atomic>
#include <utility>

namespace
{
QString sqliteReadWriteUri(const QString& absolutePath)
{
#if defined(Q_OS_WIN)
    const QString slashPath = QDir::fromNativeSeparators(absolutePath);
    if (slashPath.startsWith(QStringLiteral("//")))
    {
        const QByteArray encodedPath = QUrl::toPercentEncoding(
            slashPath,
            QByteArrayLiteral("/")
            );
        return QStringLiteral("file://")
            + QString::fromLatin1(encodedPath)
            + QStringLiteral("?mode=rw");
    }
#endif

    QUrl uri = QUrl::fromLocalFile(absolutePath);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("mode"), QStringLiteral("rw"));
    uri.setQuery(query);
    return uri.toString(QUrl::FullyEncoded);
}

QString nextConnectionName(const WorkspaceStore* session)
{
    static std::atomic<quint64> sequence{0};
    const quint64 suffix = sequence.fetch_add(1, std::memory_order_relaxed);
    return QStringLiteral("classmngr-session-%1-%2")
        .arg(reinterpret_cast<quintptr>(session), 0, 16)
        .arg(suffix);
}
}

WorkspaceStore::WorkspaceStore()
    : m_connectionName(nextConnectionName(this))
{
}

WorkspaceStore::~WorkspaceStore()
{
    close();
}

Status WorkspaceStore::open(const QString& databasePath)
{
    WorkspaceStore candidate;
    const Status status = candidate.openCandidate(
        databasePath,
        OpenBehavior::CreateIfMissing
        );
    if (!status)
    {
        return status;
    }

    swapResources(candidate);
    return {};
}

Status WorkspaceStore::openExisting(const QString& databasePath)
{
    WorkspaceStore candidate;
    const Status status = candidate.openCandidate(
        databasePath,
        OpenBehavior::ExistingOnly
        );
    if (!status)
    {
        return status;
    }

    swapResources(candidate);
    return {};
}

Status WorkspaceStore::openCandidate(
    const QString& databasePath,
    const OpenBehavior behavior
    )
{
    if (databasePath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("No Teacher Profile path was provided.")
            );
    }

    const QFileInfo databaseInfo(databasePath);
    const bool existingDatabase = databaseInfo.exists();
    const QString normalizedPath = databaseInfo.absoluteFilePath();

    if (normalizedPath.trimmed().isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Teacher Profile path could not be resolved.")
            );
    }

    if (
        behavior == OpenBehavior::CreateIfMissing
        && !databaseInfo.absolutePath().isEmpty()
        && !QDir().mkpath(databaseInfo.absolutePath())
        )
    {
        return std::unexpected(
            QStringLiteral("Unable to create Teacher Profile directory:\n%1")
                .arg(databaseInfo.absolutePath())
            );
    }

    m_database = std::make_unique<QSqlDatabase>(
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            m_connectionName
            )
        );
    if (behavior == OpenBehavior::ExistingOnly)
    {
        m_database->setConnectOptions(QStringLiteral("QSQLITE_OPEN_URI"));
        m_database->setDatabaseName(sqliteReadWriteUri(normalizedPath));
    }
    else
    {
        m_database->setDatabaseName(normalizedPath);
    }

    if (!m_database->open())
    {
        const QString openError = m_database->lastError().text();
        m_database.reset();
        QSqlDatabase::removeDatabase(m_connectionName);
        return std::unexpected(
            QStringLiteral("Unable to open Teacher Profile:\n%1\n\n%2")
                .arg(normalizedPath, openError)
            );
    }

    const Status schemaStatus = DatabaseSchemaManager::ensureSchema(
        *m_database,
        normalizedPath,
        behavior == OpenBehavior::ExistingOnly || existingDatabase
            ? DatabaseSchemaManager::Initialization::ExistingDatabase
            : DatabaseSchemaManager::Initialization::AllowEmptyDatabase
        );
    if (!schemaStatus)
    {
        const QString schemaError = schemaStatus.error();
        close();
        return std::unexpected(
            QStringLiteral("Unable to initialize Teacher Profile:\n%1\n\n%2")
                .arg(normalizedPath, schemaError)
            );
    }

    m_databasePath = normalizedPath;
    m_settingsRepository = std::make_unique<SettingsRepository>(*m_database);
    m_campusRecordRepository = std::make_unique<CampusRecordRepository>(*m_database);
    m_teacherRepository = std::make_unique<TeacherRepository>(*m_database);
    m_nativeEnglishTeacherRepository =
        std::make_unique<NativeEnglishTeacherRepository>(*m_database);
    m_gsTeamRepository = std::make_unique<GsTeamRepository>(*m_database);
    m_teacherImportRepository = std::make_unique<TeacherImportRepository>(*m_database);
    m_classRepository = std::make_unique<ClassRepository>(*m_database);
    m_classTransferRepository = std::make_unique<ClassTransferRepository>(*m_database);
    m_scheduleImportRepository = std::make_unique<ScheduleImportRepository>(*m_database);
    m_classInfoRepository = std::make_unique<ClassInfoRepository>(*m_database);
    m_intensiveSlotStateRepository =
        std::make_unique<IntensiveSlotStateRepository>(*m_database);
    m_testingBlockRepository = std::make_unique<TestingBlockRepository>(*m_database);
    m_testingClassRepository = std::make_unique<TestingClassRepository>(*m_database);
    m_calendarEventRepository = std::make_unique<CalendarEventRepository>(*m_database);
    m_rosterRepository = std::make_unique<RosterRepository>(*m_database);
    m_speakingEvalRepository = std::make_unique<SpeakingEvalRepository>(*m_database);

    return {};
}

void WorkspaceStore::swapResources(WorkspaceStore& other) noexcept
{
    using std::swap;
    swap(m_databasePath, other.m_databasePath);
    swap(m_connectionName, other.m_connectionName);
    swap(m_database, other.m_database);
    swap(m_settingsRepository, other.m_settingsRepository);
    swap(m_campusRecordRepository, other.m_campusRecordRepository);
    swap(m_teacherRepository, other.m_teacherRepository);
    swap(m_nativeEnglishTeacherRepository, other.m_nativeEnglishTeacherRepository);
    swap(m_gsTeamRepository, other.m_gsTeamRepository);
    swap(m_teacherImportRepository, other.m_teacherImportRepository);
    swap(m_classRepository, other.m_classRepository);
    swap(m_classTransferRepository, other.m_classTransferRepository);
    swap(m_scheduleImportRepository, other.m_scheduleImportRepository);
    swap(m_classInfoRepository, other.m_classInfoRepository);
    swap(m_intensiveSlotStateRepository, other.m_intensiveSlotStateRepository);
    swap(m_testingBlockRepository, other.m_testingBlockRepository);
    swap(m_testingClassRepository, other.m_testingClassRepository);
    swap(m_calendarEventRepository, other.m_calendarEventRepository);
    swap(m_rosterRepository, other.m_rosterRepository);
    swap(m_speakingEvalRepository, other.m_speakingEvalRepository);
}

void WorkspaceStore::close()
{
    m_teacherImportRepository.reset();
    m_gsTeamRepository.reset();
    m_nativeEnglishTeacherRepository.reset();
    m_settingsRepository.reset();
    m_campusRecordRepository.reset();
    m_teacherRepository.reset();
    m_classRepository.reset();
    m_classTransferRepository.reset();
    m_scheduleImportRepository.reset();
    m_classInfoRepository.reset();
    m_intensiveSlotStateRepository.reset();
    m_testingBlockRepository.reset();
    m_testingClassRepository.reset();
    m_calendarEventRepository.reset();
    m_rosterRepository.reset();
    m_speakingEvalRepository.reset();

    if (m_database && m_database->isOpen())
    {
        m_database->close();
    }
    m_database.reset();

    if (QSqlDatabase::contains(m_connectionName))
    {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_databasePath.clear();
}

bool WorkspaceStore::isOpen() const
{
    return m_database && m_database->isValid() && m_database->isOpen();
}

QString WorkspaceStore::databasePath() const
{
    return m_databasePath;
}

QSqlDatabase WorkspaceStore::database() const
{
    return m_database ? *m_database : QSqlDatabase();
}

std::optional<WorkspaceStore::ReadTransactionFailure>
WorkspaceStore::withReadTransaction(
    const std::function<Status()>& operation
    ) const
{
    if (!m_database || !m_database->isOpen())
    {
        return ReadTransactionFailure{
            .stage = ReadTransactionFailure::Stage::Start,
            .diagnostic = {}
        };
    }

    DatabaseTransaction transaction(*m_database);
    if (!transaction.started())
    {
        return ReadTransactionFailure{
            .stage = ReadTransactionFailure::Stage::Start,
            .diagnostic = transaction.startError()
        };
    }

    const Status operationStatus = operation();
    if (!operationStatus)
    {
        return ReadTransactionFailure{
            .stage = ReadTransactionFailure::Stage::Operation,
            .diagnostic = operationStatus.error()
        };
    }

    if (!transaction.commit())
    {
        return ReadTransactionFailure{
            .stage = ReadTransactionFailure::Stage::Commit,
            .diagnostic = m_database->lastError().text()
        };
    }

    return std::nullopt;
}

#define CLASSMNGR_REPOSITORY_ACCESSOR(Type, name, member) \
    Type* WorkspaceStore::name() const { return member.get(); }

CLASSMNGR_REPOSITORY_ACCESSOR(SettingsRepository, settingsRepository, m_settingsRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(CampusRecordRepository, campusRecordRepository, m_campusRecordRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(TeacherRepository, teacherRepository, m_teacherRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(NativeEnglishTeacherRepository, nativeEnglishTeacherRepository, m_nativeEnglishTeacherRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(GsTeamRepository, gsTeamRepository, m_gsTeamRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(TeacherImportRepository, teacherImportRepository, m_teacherImportRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(ClassRepository, classRepository, m_classRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(ClassTransferRepository, classTransferRepository, m_classTransferRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(ScheduleImportRepository, scheduleImportRepository, m_scheduleImportRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(ClassInfoRepository, classInfoRepository, m_classInfoRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(IntensiveSlotStateRepository, intensiveSlotStateRepository, m_intensiveSlotStateRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(TestingBlockRepository, testingBlockRepository, m_testingBlockRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(TestingClassRepository, testingClassRepository, m_testingClassRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(CalendarEventRepository, calendarEventRepository, m_calendarEventRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(RosterRepository, rosterRepository, m_rosterRepository)
CLASSMNGR_REPOSITORY_ACCESSOR(SpeakingEvalRepository, speakingEvalRepository, m_speakingEvalRepository)

#undef CLASSMNGR_REPOSITORY_ACCESSOR
