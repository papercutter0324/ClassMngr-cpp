#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/theme_service.h"
#include "data/data_service.h"
#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/campus_record_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_transfer_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/schedule_import_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "features/schedule/ui/schedule_builder.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/roster/services/roster_template_print_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QHash>
#include <QLabel>
#include <QSet>
#include <QTime>
#include <QTimer>

#include <utility>

namespace ScheduleWidgetTestStubs
{
QHash<QString, QVariant> settings;
int savedSlotStates = 0;
int printRequestCount = 0;
bool lastPrintRequestShowsEnglishNames = false;
bool databaseOpen = true;

void reset()
{
    settings.clear();
    savedSlotStates = 0;
    printRequestCount = 0;
    lastPrintRequestShowsEnglishNames = false;
    databaseOpen = true;
}

void setDatabaseOpen(
    bool open
    )
{
    databaseOpen = open;
}

QString settingValue(
    const QString& key
    )
{
    return settings.value(key).toString();
}
}

ApplicationServices::ApplicationServices()
{
    m_dataService =
        std::make_unique<DataService>();
}

ApplicationServices::~ApplicationServices() = default;

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

ThemeService* ApplicationServices::themeService() const
{
    return nullptr;
}

DataService::DataService(
    const QString&
    )
{
}

DataService::~DataService() = default;

bool DataService::isOpen() const
{
    return ScheduleWidgetTestStubs::databaseOpen;
}

void DataService::saveSetting(
    const QString& key,
    const QVariant& value
    )
{
    ScheduleWidgetTestStubs::settings.insert(
        key,
        value
        );
}

QVariant DataService::loadSetting(
    const QString& key,
    const QVariant& defaultValue
    )
{
    return ScheduleWidgetTestStubs::settings.value(
        key,
        defaultValue
        );
}

Result<ScheduleImportPreview> DataService::previewScheduleImport(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    ScheduleImportPreview preview;
    preview.kind = kind;
    preview.user = user;
    QSet<QString> teacherKeys;

    for (int index = 0; index < user.classes.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            user.classes[index];
        if (!teacherKeys.contains(candidate.teacherKey))
        {
            teacherKeys.insert(candidate.teacherKey);
            ScheduleImportTeacherPreview teacher;
            teacher.teacherKey = candidate.teacherKey;
            teacher.teacherKr = candidate.teacherKr;
            teacher.importedRooms = candidate.rooms;
            preview.teachers.append(teacher);
        }

        ScheduleImportClassPreview classroom;
        classroom.candidateIndex = index;
        preview.classes.append(classroom);
    }

    return preview;
}

Result<ScheduleImportSummary> DataService::importSchedule(
    const ScheduleImportPlan& plan
    )
{
    ScheduleImportSummary summary;
    summary.classesCreated = plan.candidates.size();
    summary.ignoredCells = plan.diagnostics.size();
    summary.profileNameUpdated =
        plan.saveProfileNameIfBlank;
    return summary;
}

QList<IntensiveSlotState> DataService::loadIntensiveSlotStates()
{
    return {};
}

QList<CalendarEvent> DataService::loadCalendarEventsInRange(
    const QDate&,
    const QDate&
    )
{
    return {};
}

void DataService::saveIntensiveSlotState(
    const QString&,
    const QString&,
    const QString&,
    const QString&
    )
{
    ++ScheduleWidgetTestStubs::savedSlotStates;
}

QList<Classroom> DataService::getClasses()
{
    Classroom classroom;
    classroom.id = 42;
    classroom.name = QStringLiteral("Hercules");
    return {classroom};
}

Classroom DataService::getClassById(
    int classId
    )
{
    for (const Classroom& classroom : getClasses())
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    return {};
}

ClassInfo DataService::loadClassInfo(
    int classId
    )
{
    ClassInfo info;
    info.classId = classId;
    info.teacherId = 7;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Hercules");
    info.notes = QStringLiteral("Read chapter three.");

    ClassTime meeting;
    meeting.day = QStringLiteral("Tuesday");
    meeting.startTime = QStringLiteral("4:00 PM");
    meeting.endTime = QStringLiteral("4:50 PM");
    info.classTimes.append(meeting);

    return info;
}

Roster DataService::loadRoster(
    int
    )
{
    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    return roster;
}

int DataService::getRosterStudentCount(
    int classId
    )
{
    return classId == 42
        ? 12
        : 0;
}

Teacher DataService::getTeacher(
    int teacherId
    )
{
    Teacher teacher;
    teacher.id = teacherId;
    teacher.teacherEn = QStringLiteral("Susan");
    teacher.teacherKr = QStringLiteral("김선생");
    teacher.roomNumber = QStringLiteral("413");
    teacher.wifiName = QStringLiteral("Susan WiFi");
    teacher.wifiPassword = QStringLiteral("wifi secret");
    teacher.zoomId = QStringLiteral("susan.zoom");
    teacher.zoomPassword = QStringLiteral("zoom secret");
    teacher.internetType = QStringLiteral("WiFi");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes = QStringLiteral("Call before class.");
    return teacher;
}

ResourcePackManager& ResourcePackManager::instance()
{
    static ResourcePackManager manager;
    return manager;
}

ResourcePackManager::ResourcePackManager(
    QString storageDirectory
    )
    : m_storageDirectory(std::move(storageDirectory))
{
}

QString ResourcePackManager::activeRoot(
    const QString&
    ) const
{
    return {};
}

Theme ThemeService::currentTheme() const
{
    return Theme::Dark;
}

QFont FontManager::getUiFont(
    int size,
    int weight,
    bool italic
    )
{
    QFont font;
    if (size > 0)
    {
        font.setPointSize(size);
    }
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setItalic(italic);
    return font;
}

QFont FontManager::getKoreanFont(
    int size,
    int weight,
    bool italic
    )
{
    return getUiFont(size, weight, italic);
}

int FontManager::adjustedPointSize(
    int baseSize
    )
{
    return baseSize;
}

void FontManager::setManagedRichText(
    QLabel* label,
    const QString& html
    )
{
    if (label)
    {
        label->setTextFormat(Qt::RichText);
        label->setText(html);
    }
}

ScheduleBuilder::ScheduleBuilder(
    DataService* dataService
    )
    : m_dataService(dataService)
{
}

ScheduleBuildResult ScheduleBuilder::build(
    bool,
    const QStringList& visibleDays
    ) const
{
    ScheduleBuildResult result;
    result.days = visibleDays;

    if (!m_dataService || !m_dataService->isOpen())
    {
        return result;
    }

    result.rows.append(
        {QStringLiteral("16:00")}
        );

    for (const QString& day : visibleDays)
    {
        result.schedule.insert(day, {});
    }

    if (visibleDays.contains(QStringLiteral("Tuesday")))
    {
        ScheduleEntry entry;
        entry.classId = 42;
        entry.teacherKr = QStringLiteral("김선생");
        entry.teacherEn = QStringLiteral("Susan");
        entry.roomNumber = QStringLiteral("413");
        entry.classGrade = QStringLiteral("E4");
        entry.classLevel = QStringLiteral("Hercules");

        result.schedule[QStringLiteral("Tuesday")]
            [QStringLiteral("16:00")]
                .append(entry);
    }

    return result;
}

QString scheduleEmptySlotState()
{
    return QStringLiteral("empty");
}

QString scheduleEssaySlotState()
{
    return QStringLiteral("essay");
}

QString scheduleLunchSlotState()
{
    return QStringLiteral("lunch");
}

QString nextScheduleSlotState(
    const QString& currentState
    )
{
    return currentState == scheduleEssaySlotState()
        ? scheduleLunchSlotState()
        : scheduleEssaySlotState();
}

QString scheduleSlotKey(
    const QString& day,
    const QString& timeLabel
    )
{
    return day + QLatin1Char('\x1f') + timeLabel;
}

QStringList visibleScheduleDays(
    bool includeWeekends
    )
{
    QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    if (includeWeekends)
    {
        days.append(QStringLiteral("Saturday"));
        days.append(QStringLiteral("Sunday"));
    }

    return days;
}

bool isScheduleWeekendDay(
    const QString& day
    )
{
    return day == QStringLiteral("Saturday")
        || day == QStringLiteral("Sunday");
}

QString scheduleDefaultSlotState(
    const QString&,
    const QString&,
    bool useIntensive
    )
{
    return useIntensive
        ? scheduleEssaySlotState()
        : scheduleEmptySlotState();
}

bool scheduleSlotTogglingEnabled(
    const QString&,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    )
{
    return useIntensive
        || regularWeekdaySlotTogglingEnabled;
}

QString scheduleSlotState(
    const QString&,
    const QString&,
    const QString& defaultState,
    const QMap<QString, QString>&
    )
{
    return defaultState;
}

ScheduleViewModel buildScheduleViewModel(
    const ScheduleBuildResult& result,
    const ScheduleViewRequest& request
    )
{
    ScheduleViewModel model;
    model.days = request.days;

    for (const ScheduleRow& sourceRow : result.rows)
    {
        ScheduleRowView row;
        row.timeLabel = sourceRow.label;
        row.timeRangeLabel = QStringLiteral("4pm - 4:50pm");

        for (const QString& day : request.days)
        {
            ScheduleCellView cell;
            cell.day = day;
            cell.timeLabel = sourceRow.label;
            cell.entries =
                result.schedule
                    .value(day)
                    .value(sourceRow.label);
            cell.defaultSlotState =
                scheduleDefaultSlotState(
                    day,
                    sourceRow.label,
                    request.useIntensive
                    );
            cell.slotState = cell.defaultSlotState;
            cell.slotTogglingEnabled =
                scheduleSlotTogglingEnabled(
                    day,
                    request.useIntensive,
                    request.regularWeekdaySlotTogglingEnabled
                    );
            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    return model;
}

ScheduleEditorDialog::ScheduleEditorDialog(
    ApplicationServices* services,
    int classId,
    QWidget* parent
    )
    : QDialog(parent)
    , m_services(services)
    , m_classId(classId)
{
}

void ScheduleEditorDialog::updateLevelOptions()
{
}

void ScheduleEditorDialog::chooseClassColor()
{
}

void ScheduleEditorDialog::chooseFontColor()
{
}

void ScheduleEditorDialog::saveChanges()
{
}

SchedulePrintDialog::SchedulePrintDialog(
    QWidget* parent
    )
    : QDialog(parent)
{
    QTimer::singleShot(
        0,
        this,
        &QDialog::accept
        );
}

SchedulePrintDialog::Action SchedulePrintDialog::selectedAction() const
{
    return Action::Print;
}

QString SchedulePrintDialog::selectedSavePath() const
{
    return {};
}

SchedulePrintStyle SchedulePrintDialog::selectedStyle() const
{
    return SchedulePrintStyle::CurrentAppearance;
}

QPageLayout::Orientation
SchedulePrintDialog::selectedOrientation() const
{
    return QPageLayout::Landscape;
}

SchedulePrintService::Result SchedulePrintService::printSchedule(
    const Request& request
    )
{
    ++ScheduleWidgetTestStubs::printRequestCount;
    ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames =
        request.showEnglishNames;

    return {Status::Canceled, {}};
}

SchedulePrintService::Result SchedulePrintService::saveSchedulePdf(
    const Request&,
    const QString&
    )
{
    return {Status::Canceled, {}};
}

PdfPrintService::Result PdfPrintService::printPdfDocument(
    const Request&
    )
{
    return {
        Status::Canceled,
        QString()
    };
}

PdfPrintService::Result PdfPrintService::printPdfDocuments(
    const BatchRequest&
    )
{
    return {
        Status::Canceled,
        QString()
    };
}

QList<RosterTemplatePrintService::TemplateId>
RosterTemplatePrintService::availableTemplateIds()
{
    return {
        TemplateId::ByDay,
        TemplateId::Daily,
        TemplateId::PerClassWithExtraInfo
    };
}

QString RosterTemplatePrintService::templateDisplayName(
    TemplateId templateId
    )
{
    switch (templateId)
    {
    case TemplateId::Daily:
        return QStringLiteral("Daily");

    case TemplateId::PerClassWithExtraInfo:
        return QStringLiteral("Per Class with Extra Info");

    case TemplateId::ByDay:
    default:
        return QStringLiteral("By Day");
    }
}

int RosterTemplatePrintService::perClassExtraInfoMaxExtraColumns(
    QPageLayout::Orientation orientation
    )
{
    return orientation == QPageLayout::Landscape ? 6 : 3;
}

QStringList RosterTemplatePrintService::availablePerClassExtraInfoColumns(
    const QList<RosterTemplatePrintService::RosterClassData>&
    )
{
    return {};
}

RosterTemplatePrintService::Result
RosterTemplatePrintService::saveRostersPdf(
    const QList<RosterTemplatePrintService::RosterClassData>&,
    const QString&,
    RosterTemplatePrintService::TemplateId,
    const QStringList&,
    QPageLayout::Orientation
    )
{
    return {
        Status::Sent,
        QString()
    };
}
