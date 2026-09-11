#include "database_session.h"

#include "classmngr/engine/open_database.h"

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

#include <cstddef>
#include <string_view>

DatabaseSession::DatabaseSession() = default;

DatabaseSession::~DatabaseSession()
{
    close();
}

Status DatabaseSession::open(const QString& databasePath)
{
    if (databasePath.trimmed().isEmpty())
    {
        close();
        return std::unexpected(
            QStringLiteral("No Teacher Profile path was provided.")
            );
    }

    close();

    if (databasePath == QStringLiteral(":memory:"))
    {
        close();
        return std::unexpected(
            QStringLiteral(
                "In-memory sessions are not owned by DatabaseSession. "
                "Use classmngr::engine::OpenDatabase::execute(\":memory:\") "
                "and pass the engine handle to the portable service."
                )
            );
    }

    const QByteArray utf8Path = databasePath.toUtf8();
    auto engineDatabase = classmngr::engine::OpenDatabase::execute(
        std::string_view(
            utf8Path.constData(),
            static_cast<std::size_t>(utf8Path.size())
            )
        );

    if (!engineDatabase)
    {
        const std::string& engineError = engineDatabase.error().message;
        return std::unexpected(
            QStringLiteral("Unable to initialize Teacher Profile:\n%1\n\n%2")
                .arg(
                    databasePath,
                    QString::fromUtf8(
                        engineError.data(),
                        static_cast<qsizetype>(engineError.size())
                        )
                    )
            );
    }

    const std::string_view enginePath = (*engineDatabase)->databasePath();
    m_databasePath = QString::fromUtf8(
        enginePath.data(),
        static_cast<qsizetype>(enginePath.size())
        );

    m_settingsRepository = std::make_unique<SettingsRepository>(m_databasePath);
    m_campusRecordRepository = std::make_unique<CampusRecordRepository>(m_databasePath);
    m_teacherRepository = std::make_unique<TeacherRepository>(m_databasePath);
    m_nativeEnglishTeacherRepository =
        std::make_unique<NativeEnglishTeacherRepository>(m_databasePath);
    m_gsTeamRepository = std::make_unique<GsTeamRepository>(m_databasePath);
    m_teacherImportRepository = std::make_unique<TeacherImportRepository>(m_databasePath);
    m_classRepository = std::make_unique<ClassRepository>(m_databasePath);
    m_classTransferRepository = std::make_unique<ClassTransferRepository>(m_databasePath);
    m_scheduleImportRepository = std::make_unique<ScheduleImportRepository>(m_databasePath);
    m_classInfoRepository = std::make_unique<ClassInfoRepository>(m_databasePath);
    m_intensiveSlotStateRepository =
        std::make_unique<IntensiveSlotStateRepository>(m_databasePath);
    m_testingBlockRepository = std::make_unique<TestingBlockRepository>(m_databasePath);
    m_testingClassRepository = std::make_unique<TestingClassRepository>(m_databasePath);
    m_calendarEventRepository = std::make_unique<CalendarEventRepository>(m_databasePath);
    m_rosterRepository = std::make_unique<RosterRepository>(m_databasePath);
    m_speakingEvalRepository = std::make_unique<SpeakingEvalRepository>(m_databasePath);

    return {};
}

void DatabaseSession::close()
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

    m_databasePath.clear();
}

bool DatabaseSession::isOpen() const
{
    return !m_databasePath.isEmpty();
}

bool DatabaseSession::isEngineBacked() const
{
    return isOpen() && m_settingsRepository != nullptr;
}

QString DatabaseSession::databasePath() const
{
    return m_databasePath;
}

#define CLASSMNGR_REPOSITORY_ACCESSOR(Type, name, member) \
    Type* DatabaseSession::name() const { return member.get(); }

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
