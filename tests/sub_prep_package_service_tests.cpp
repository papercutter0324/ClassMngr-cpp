#include "features/sub_prep/services/sub_prep_package_service.h"

#include "domain/models/teacher.h"
#include "next/application/sub_prep_roster_output_source_query.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QtTest>

#include <QDir>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPdfDocument>
#include <QTemporaryDir>

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace
{
constexpr int HeavyClassCount = 96;
constexpr int HeavyClassesPerTeacher = 4;
constexpr int HeavyTeacherCount =
    HeavyClassCount / HeavyClassesPerTeacher;

QList<Classroom> g_classes;
QHash<int, ClassInfo> g_classInfo;
QHash<int, Roster> g_rosters;
QHash<int, Teacher> g_teachers;
PdfPrintService::Status g_batchPrintStatus =
    PdfPrintService::Status::Canceled;
QStringList g_batchPrintPaths;

ClassMngr::Next::Domain::ClassId typedClassId(const int id)
{
    return *ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(id)
        );
}

class FakeRosterOutputSourceReadPort final
    : public ClassMngr::Next::Application::
        SubPrepRosterOutputSourceReadPort
{
public:
    using Request = ClassMngr::Next::Application::
        SubPrepRosterOutputSourceRequest;
    using Result = ClassMngr::Next::Application::
        SubPrepRosterOutputSourceReadResult;

    void reset()
    {
        loadCount = 0;
        lastRequest = {};
        failReads = false;
        injectInvalidUtf8 = false;
        typedSource.reset();
    }

    [[nodiscard]] Result loadSource(const Request& request) override;

    int loadCount = 0;
    Request lastRequest;
    bool failReads = false;
    bool injectInvalidUtf8 = false;
    std::optional<
        ClassMngr::Next::Application::SubPrepRosterOutputSourceInput
        > typedSource;
};

FakeRosterOutputSourceReadPort g_rosterOutputSourceReadPort;

void resetData()
{
    g_rosterOutputSourceReadPort.reset();
    g_classes.clear();
    g_classInfo.clear();
    g_rosters.clear();
    g_teachers.clear();
    g_batchPrintStatus = PdfPrintService::Status::Canceled;
    g_batchPrintPaths.clear();

    Classroom classroom;
    classroom.id = 42;
    classroom.name = QStringLiteral("Hercules");
    g_classes.append(classroom);

    ClassInfo info;
    info.classId = 42;
    info.teacherId = 7;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Hercules");
    info.classTimes.append(
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
        );
    g_classInfo.insert(42, info);

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Allergies")
    };
    roster.rows.append(
        {
            QStringLiteral("Alex"),
            QStringLiteral("알렉스"),
            QStringLiteral("None")
        }
        );
    g_rosters.insert(42, roster);

    Teacher teacher;
    teacher.id = 7;
    teacher.teacherEn = QStringLiteral("Susan");
    g_teachers.insert(7, teacher);
}

SubPrepPrintService::Request subPrepRequest()
{
    SubPrepPrintService::Request request;
    request.schedule.days = {QStringLiteral("Tuesday")};

    ScheduleRowView row;
    row.timeLabel = QStringLiteral("16:00");
    row.timeRangeLabel = QStringLiteral("4:00 - 4:50");

    ScheduleCellView cell;
    cell.day = QStringLiteral("Tuesday");
    cell.timeLabel = row.timeLabel;
    ScheduleEntry entry;
    entry.classId = 42;
    entry.teacherEn = QStringLiteral("Susan");
    entry.classGrade = QStringLiteral("E4");
    entry.classLevel = QStringLiteral("Hercules");
    cell.entries.append(entry);
    row.cells.append(cell);
    request.schedule.rows.append(row);
    request.subNotes = QStringLiteral("Please leave a handover note.");
    return request;
}

SubPrepPackageService::Request packageRequest(
    const QString& targetRoot,
    const QString& userName
    )
{
    SubPrepPackageService::Request request;
    request.rosterOutputSourceReadPort = &g_rosterOutputSourceReadPort;
    request.subPrep = subPrepRequest();
    request.selectedDates = {QDate(2026, 7, 21)};
    request.selectedClassIds = {typedClassId(42)};
    request.createFolder = true;
    request.targetRoot = targetRoot;
    request.userName = userName;
    request.openFolderAfterGeneration = false;
    return request;
}

QList<int> heavyClassIds()
{
    QList<int> ids;
    ids.reserve(HeavyClassCount);

    for (int index = 0; index < HeavyClassCount; ++index)
    {
        ids.append(1000 + index);
    }

    return ids;
}

QString heavyClassLabel(
    int classIndex
    )
{
    return QStringLiteral("E%1 Level %2")
        .arg((classIndex / 16) + 1)
        .arg((classIndex % 4) + 1);
}

void populateHeavyOutputData()
{
    g_classes.clear();
    g_classInfo.clear();
    g_rosters.clear();
    g_teachers.clear();
    g_batchPrintStatus = PdfPrintService::Status::Canceled;
    g_batchPrintPaths.clear();
    g_rosterOutputSourceReadPort.reset();

    for (int classIndex = 0; classIndex < HeavyClassCount; ++classIndex)
    {
        const int classId = 1000 + classIndex;
        const int teacherIndex = classIndex / HeavyClassesPerTeacher;
        const int teacherId = 200 + teacherIndex;
        const QString label = heavyClassLabel(classIndex);
        const QString teacherName = QStringLiteral("Teacher %1")
            .arg(teacherIndex + 1, 2, 10, QLatin1Char('0'));
        const QString startTime =
            QStringLiteral("%1:00 PM").arg(4 + (classIndex % 8));
        const QString endTime =
            QStringLiteral("%1:50 PM").arg(4 + (classIndex % 8));

        g_classes.append(
            Classroom(
                QStringLiteral("Heavy Class %1").arg(classIndex + 1),
                classId
                )
            );

        Teacher teacher;
        teacher.id = teacherId;
        teacher.teacherEn = teacherName;
        teacher.roomNumber = QStringLiteral("%1").arg(401 + teacherIndex);
        teacher.wifiName = QStringLiteral("%1 WiFi").arg(teacherName);
        teacher.wifiPassword = QStringLiteral("wifi-%1").arg(teacherIndex + 1);
        teacher.internetType = QStringLiteral("WiFi");
        teacher.zoomId = QStringLiteral("zoom.%1").arg(teacherIndex + 1);
        teacher.zoomPassword = QStringLiteral("zoom-%1").arg(teacherIndex + 1);
        teacher.projectionType = QStringLiteral("HDMI");
        teacher.notes = QStringLiteral("Call %1 before class.").arg(teacherName);
        g_teachers.insert(teacherId, teacher);

        ClassInfo info;
        info.classId = classId;
        info.teacherId = teacherId;
        info.teacherEn = teacherName;
        info.roomNumber = teacher.roomNumber;
        info.wifiName = teacher.wifiName;
        info.wifiPassword = teacher.wifiPassword;
        info.internetType = teacher.internetType;
        info.zoomId = teacher.zoomId;
        info.zoomPassword = teacher.zoomPassword;
        info.projectionType = teacher.projectionType;
        info.classGrade = QStringLiteral("E%1").arg((classIndex / 16) + 1);
        info.classLevel = QStringLiteral("Level %1").arg((classIndex % 4) + 1);
        info.classColor = classIndex % 2 == 0
            ? QStringLiteral("#dbeafe")
            : QStringLiteral("#dcfce7");
        info.fontColor = QStringLiteral("#1e3a5f");
        info.notes = QStringLiteral(
            "Read chapter %1 and discuss the vocabulary."
            ).arg((classIndex % 6) + 1);
        info.classTimes.append(
            {
                QStringLiteral("Tuesday"),
                startTime,
                endTime
            }
            );
        info.intensiveTimes = info.classTimes;
        g_classInfo.insert(classId, info);

        Roster roster;
        roster.columns = {
            QStringLiteral("English"),
            QStringLiteral("Korean"),
            QStringLiteral("Allergies")
        };
        for (int studentIndex = 0; studentIndex < 25; ++studentIndex)
        {
            roster.rows.append(
                {
                    QStringLiteral("Student %1-%2")
                        .arg(classIndex + 1)
                        .arg(studentIndex + 1),
                    QStringLiteral("Student Korean %1-%2")
                        .arg(classIndex + 1)
                        .arg(studentIndex + 1),
                    studentIndex % 5 == 0
                        ? QStringLiteral("Check allergy plan")
                        : QStringLiteral("None")
                }
                );
        }
        g_rosters.insert(classId, roster);
    }
}

namespace
{
std::string utf8String(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::optional<int> legacyId(const std::string& value)
{
    int parsed = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
        );
    if (error != std::errc{} || end != value.data() + value.size()
        || parsed <= 0 || std::to_string(parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

std::optional<ClassMngr::Next::Application::SubPrepWeekday> appWeekday(
    const QString& value
    )
{
    using ClassMngr::Next::Application::SubPrepWeekday;
    if (value == QStringLiteral("Monday"))
    {
        return SubPrepWeekday::Monday;
    }
    if (value == QStringLiteral("Tuesday"))
    {
        return SubPrepWeekday::Tuesday;
    }
    if (value == QStringLiteral("Wednesday"))
    {
        return SubPrepWeekday::Wednesday;
    }
    if (value == QStringLiteral("Thursday"))
    {
        return SubPrepWeekday::Thursday;
    }
    if (value == QStringLiteral("Friday"))
    {
        return SubPrepWeekday::Friday;
    }
    if (value == QStringLiteral("Saturday"))
    {
        return SubPrepWeekday::Saturday;
    }
    if (value == QStringLiteral("Sunday"))
    {
        return SubPrepWeekday::Sunday;
    }
    return std::nullopt;
}
} // namespace

FakeRosterOutputSourceReadPort::Result
FakeRosterOutputSourceReadPort::loadSource(const Request& request)
{
    ++loadCount;
    lastRequest = request;
    if (failReads)
    {
        return Result::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "Synthetic roster-output read failure.",
            .recoverable = false
        });
    }

    if (typedSource.has_value())
    {
        return Result::success(*typedSource);
    }

    ClassMngr::Next::Application::SubPrepRosterOutputSourceInput input;
    std::unordered_set<int> copiedTeacherIds;

    QStringList requestedColumns{
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    for (const std::string& encodedColumn : request.selectedExtraColumns)
    {
        const QString column = QString::fromUtf8(
            encodedColumn.data(),
            static_cast<qsizetype>(encodedColumn.size())
            );
        if (column.compare(QStringLiteral("English"), Qt::CaseInsensitive) != 0
            && column.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) != 0
            && !requestedColumns.contains(column, Qt::CaseInsensitive))
        {
            requestedColumns.append(column);
        }
    }

    for (const auto& requestedId : request.selectedClassIds)
    {
        const auto classId = legacyId(requestedId.value());
        if (!classId)
        {
            return Result::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::InvalidInput,
                .message = "A selected class identifier is invalid.",
                .recoverable = false
            });
        }

        const auto classroom = std::find_if(
            g_classes.cbegin(),
            g_classes.cend(),
            [classId](const Classroom& value)
            {
                return value.id == *classId;
            }
            );
        if (classroom == g_classes.cend())
        {
            return Result::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
                .message = "A selected class was not found.",
                .recoverable = false
            });
        }

        const auto info = g_classInfo.constFind(*classId);
        if (info == g_classInfo.cend())
        {
            continue;
        }

        const QList<ClassTime>& schedule =
            request.mode == ClassMngr::Next::Application::
                ScheduleViewMode::Intensive
            ? info->intensiveTimes
            : info->classTimes;
        ClassMngr::Next::Application::SubPrepRosterOutputClass outputClass{
            requestedId};
        for (const ClassTime& meeting : schedule)
        {
            const auto weekday = appWeekday(meeting.day);
            if (weekday
                && std::find(
                    request.selectedDays.cbegin(),
                    request.selectedDays.cend(),
                    *weekday
                    ) != request.selectedDays.cend())
            {
                outputClass.meetings.push_back(
                    {*weekday,
                     utf8String(meeting.startTime),
                     utf8String(meeting.endTime)}
                    );
            }
        }
        if (outputClass.meetings.empty())
        {
            continue;
        }

        outputClass.classroomName = utf8String(classroom->name);
        outputClass.grade = utf8String(info->classGrade);
        outputClass.level = utf8String(info->classLevel);

        const Teacher* teacher = nullptr;
        if (info->teacherId > 0)
        {
            const auto foundTeacher = g_teachers.constFind(info->teacherId);
            if (foundTeacher == g_teachers.cend())
            {
                return Result::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
                    .message = "A selected class teacher was not found.",
                    .recoverable = false
                });
            }
            teacher = &foundTeacher.value();
            outputClass.teacherId =
                *ClassMngr::Next::Domain::TeacherId::fromString(
                    std::to_string(info->teacherId)
                    );

            if (copiedTeacherIds.insert(info->teacherId).second)
            {
                input.teachers.push_back({
                    *outputClass.teacherId,
                    utf8String(teacher->teacherEn),
                    utf8String(teacher->teacherKr),
                    utf8String(teacher->preferredName),
                    utf8String(teacher->preferredRomanization)
                });
            }
        }

        outputClass.classTeacherEnglishName = utf8String(
            teacher ? teacher->teacherEn : info->teacherEn
            );
        outputClass.classTeacherKoreanName = utf8String(
            teacher ? teacher->teacherKr : info->teacherKr
            );
        outputClass.room = utf8String(
            teacher ? teacher->roomNumber : info->roomNumber
            );
        outputClass.wifiName = utf8String(
            teacher ? teacher->wifiName : info->wifiName
            );
        outputClass.wifiPassword = utf8String(
            teacher ? teacher->wifiPassword : info->wifiPassword
            );
        outputClass.zoomId = utf8String(
            teacher ? teacher->zoomId : info->zoomId
            );
        outputClass.zoomPassword = utf8String(
            teacher ? teacher->zoomPassword : info->zoomPassword
            );

        const auto roster = g_rosters.constFind(*classId);
        std::vector<int> sourceColumnIndexes;
        if (roster != g_rosters.cend())
        {
            for (const QString& column : requestedColumns)
            {
                for (int index = 0; index < roster->columns.size(); ++index)
                {
                    if (roster->columns.at(index).compare(
                            column,
                            Qt::CaseInsensitive
                            ) == 0)
                    {
                        outputClass.rosterColumns.push_back(
                            utf8String(column)
                            );
                        sourceColumnIndexes.push_back(index);
                        break;
                    }
                }
            }

            for (const QStringList& sourceRow : roster->rows)
            {
                std::vector<std::string> row;
                row.reserve(sourceColumnIndexes.size());
                for (const int index : sourceColumnIndexes)
                {
                    row.push_back(utf8String(sourceRow.value(index)));
                }
                outputClass.rosterRows.push_back(std::move(row));
            }
        }
        input.classes.push_back(std::move(outputClass));
    }

    if (injectInvalidUtf8 && !input.classes.empty())
    {
        input.classes.front().classroomName = std::string(1, '\xFF');
    }

    return Result::success(std::move(input));
}

SubPrepPrintService::Request heavySubPrepRequest()
{
    SubPrepPrintService::Request request;
    request.campus = {
        QStringLiteral("02-555-1234"),
        QStringLiteral("DYB-Staff"),
        QStringLiteral("campus-password"),
        QStringLiteral("5678")
    };
    request.zoom = {
        QStringLiteral("teacher@example.com"),
        QStringLiteral("zoom-password")
    };
    request.classMaterials = QStringLiteral(
        "Materials are in the blue folder.\n"
        "Use the projector remote from the desk."
        );
    request.gradingInstructions = QStringLiteral(
        "Score each book report out of 100 and leave actionable feedback."
        );
    request.specialInstructions = QStringLiteral(
        "Collect completed reports before students leave."
        );
    request.schedule.days = {QStringLiteral("Tuesday")};

    for (int rowIndex = 0; rowIndex < 8; ++rowIndex)
    {
        ScheduleRowView row;
        row.timeLabel = QStringLiteral("%1:00").arg(16 + rowIndex);
        row.timeRangeLabel = QStringLiteral(
            "%1:00 -\n%1:50"
            ).arg(4 + rowIndex);

        ScheduleCellView cell;
        cell.day = QStringLiteral("Tuesday");
        cell.timeLabel = row.timeLabel;

        for (int classIndex = rowIndex * 12;
             classIndex < (rowIndex + 1) * 12;
             ++classIndex)
        {
            const int classId = 1000 + classIndex;
            const Teacher teacher =
                g_teachers.value(200 + classIndex / HeavyClassesPerTeacher);
            const ClassInfo info = g_classInfo.value(classId);

            ScheduleEntry entry;
            entry.classId = classId;
            entry.teacherEn = teacher.teacherEn;
            entry.roomNumber = teacher.roomNumber;
            entry.classGrade = info.classGrade;
            entry.classLevel = info.classLevel;
            entry.classColor = info.classColor;
            entry.fontColor = info.fontColor;
            cell.entries.append(entry);
        }

        row.cells.append(cell);
        request.schedule.rows.append(row);
    }

    for (int teacherIndex = 0;
         teacherIndex < HeavyTeacherCount;
         ++teacherIndex)
    {
        const Teacher teacher = g_teachers.value(200 + teacherIndex);
        SubPrepClassInformation::TeacherGroup group;
        group.teacher = teacher;
        group.displayName = teacher.preferredDisplayName();

        QStringList classLabels;
        for (int offset = 0; offset < HeavyClassesPerTeacher; ++offset)
        {
            const int classIndex =
                teacherIndex * HeavyClassesPerTeacher + offset;
            const int classId = 1000 + classIndex;
            const ClassInfo info = g_classInfo.value(classId);

            SubPrepClassInformation::ClassDetails details;
            details.classId = classId;
            details.info = info;
            details.studentCount = g_rosters.value(classId).rows.size();
            details.classLabel = heavyClassLabel(classIndex);
            details.timeText = QStringLiteral("Tues %1")
                .arg(info.classTimes.first().startTime);
            group.classes.append(details);
            classLabels.append(details.classLabel);
        }
        group.classListText = classLabels.join(QStringLiteral(" / "));
        request.classInformation.append(group);
    }

    request.subNotes = QStringLiteral(
        "Thank you for covering these classes. Please leave a short handover note."
        );
    return request;
}

SubPrepPackageService::Request heavyPackageRequest(
    const QString& targetRoot
    )
{
    SubPrepPackageService::Request request;
    request.rosterOutputSourceReadPort = &g_rosterOutputSourceReadPort;
    request.subPrep = heavySubPrepRequest();
    request.selectedDates = {QDate(2026, 7, 21)};
    for (const int id : heavyClassIds())
    {
        request.selectedClassIds.push_back(typedClassId(id));
    }
    request.createFolder = true;
    request.targetRoot = targetRoot;
    request.userName = QStringLiteral("96-class reference");
    request.replaceExisting = true;
    request.openFolderAfterGeneration = false;
    request.rosterTemplate = RosterTemplatePrintService::TemplateId::Daily;
    return request;
}

bool inspectGeneratedPdf(
    const QString& pdfPath,
    const QString& capturePath,
    QJsonObject* metadata,
    QString* errorMessage
    )
{
    QPdfDocument document;
    if (document.load(pdfPath) != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Generated PDF could not be loaded.");
        }
        return false;
    }

    const QSizeF pagePoints = document.pagePointSize(0);
    const QSize renderSize(
        std::max(1, qRound(pagePoints.width() * 150.0 / 72.0)),
        std::max(1, qRound(pagePoints.height() * 150.0 / 72.0))
        );
    const QImage firstPage = document.render(0, renderSize);
    if (firstPage.isNull())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Generated PDF first page could not be rendered.");
        }
        return false;
    }

    if (!capturePath.isEmpty() && !firstPage.save(capturePath, "PNG"))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Generated PDF first page could not be captured.");
        }
        return false;
    }

    if (metadata)
    {
        metadata->insert(QStringLiteral("pageCount"), document.pageCount());
        metadata->insert(QStringLiteral("fileSizeBytes"), QFileInfo(pdfPath).size());
        metadata->insert(QStringLiteral("firstPageWidth"), firstPage.width());
        metadata->insert(QStringLiteral("firstPageHeight"), firstPage.height());
    }

    return true;
}

bool comparePdfToReference(
    const QString& generatedPath,
    const QString& referencePath,
    QString* errorMessage
    )
{
    QPdfDocument generated;
    QPdfDocument reference;
    if (generated.load(generatedPath) != QPdfDocument::Error::None
        || generated.status() != QPdfDocument::Status::Ready
        || reference.load(referencePath) != QPdfDocument::Error::None
        || reference.status() != QPdfDocument::Status::Ready)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral(
                "Generated or reference PDF could not be loaded."
                );
        }
        return false;
    }

    if (generated.pageCount() != reference.pageCount())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral(
                "Generated PDF page count differs from the reference."
                );
        }
        return false;
    }

    for (int pageIndex = 0; pageIndex < generated.pageCount(); ++pageIndex)
    {
        if (generated.pagePointSize(pageIndex)
            != reference.pagePointSize(pageIndex))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Generated PDF page %1 has different dimensions."
                    ).arg(pageIndex + 1);
            }
            return false;
        }

        // PDF text extraction can insert different line breaks and spacing
        // when a platform uses different font metrics. Preserve the full
        // character sequence while ignoring that layout-only whitespace.
        if (generated.getAllText(pageIndex).text().simplified()
            != reference.getAllText(pageIndex).text().simplified())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Generated PDF page %1 text differs from the reference."
                    ).arg(pageIndex + 1);
            }
            return false;
        }
    }

#if defined(Q_OS_WIN)
    for (int pageIndex = 0; pageIndex < generated.pageCount(); ++pageIndex)
    {
        const QSizeF pagePoints = generated.pagePointSize(pageIndex);
        const QSize renderSize(
            std::max(1, qRound(pagePoints.width() * 150.0 / 72.0)),
            std::max(1, qRound(pagePoints.height() * 150.0 / 72.0))
            );
        const QImage generatedPage = generated.render(pageIndex, renderSize)
            .convertToFormat(QImage::Format_ARGB32);
        const QImage referencePage = reference.render(pageIndex, renderSize)
            .convertToFormat(QImage::Format_ARGB32);
        if (generatedPage.isNull() || referencePage.isNull()
            || generatedPage != referencePage)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "Generated PDF page %1 rendering differs from the Windows reference."
                    ).arg(pageIndex + 1);
            }
            return false;
        }
    }
#endif

    return true;
}
}

namespace PdfPrintService
{
Result printPdfDocument(
    const Request&
    )
{
    return {Status::Canceled, {}};
}

Result printPdfDocuments(
    const BatchRequest& request
    )
{
    g_batchPrintPaths = request.documentPaths;
    return {
        g_batchPrintStatus,
        g_batchPrintStatus == Status::Failed
            ? QStringLiteral("Printer failed")
            : QString()
    };
}
}

class SubPrepPackageServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void rejectsInvalidPackagePreflightBeforeFilesystemEffects();
    void generationConsumesTypedPlanForRosterQueryAndFolderOutput();
    void byDayPackageCreatesExpectedTreeAndReplacesTransactionally();
    void folderOpenOutcomePreservesGeneratedPackageArtifacts();
    void acceptsWideTeacherIdAndUsesPreferredDisplayName();
    void dailyPackageUsesDailyRosterTemplate();
    void perClassPackagePlacesRosterInsideClassFolder();
    void classIdsForDaysProjectsScheduleOrderAndAllEntryKinds();
    void intensiveScheduleControlsSelectedDayFiltering();
    void printOnlyUsesPacketOrderAndReportsCancellation();
    void rosterSourceFailuresDoNotCommitPartialPackages();
    void largePackageGeneratesOutputReferenceWhenConfigured();
};

void SubPrepPackageServiceTests::init()
{
    resetData();
}

void SubPrepPackageServiceTests
    ::rejectsInvalidPackagePreflightBeforeFilesystemEffects()
{
    QTemporaryDir temporaryRoot;
    QVERIFY(temporaryRoot.isValid());

    const QString targetPath =
        QDir(temporaryRoot.path()).filePath(QStringLiteral("package-root"));
    const auto failsBeforeFilesystemEffects =
        [&targetPath](
            SubPrepPackageService::Request request,
            const QString& expectedMessage
            )
    {
        const SubPrepPackageService::Result result =
            SubPrepPackageService::generate(std::move(request));
        return result.status == SubPrepPackageService::Status::Failed
            && result.message == expectedMessage
            && !QFileInfo::exists(targetPath)
            && g_rosterOutputSourceReadPort.loadCount == 0;
    };

    auto request = packageRequest(targetPath, QStringLiteral("Jamie"));
    request.createFolder = false;
    request.printPaperCopies = false;
    request.selectedDates = {QDate()};
    request.selectedClassIds.clear();
    request.targetRoot.clear();
    request.userName.clear();
    QVERIFY(failsBeforeFilesystemEffects(
        request,
        QStringLiteral("Choose a folder or paper-copy action.")
        ));

    request = packageRequest(targetPath, QStringLiteral("Jamie"));
    request.selectedDates = {QDate()};
    request.selectedClassIds.clear();
    request.userName.clear();
    QVERIFY(failsBeforeFilesystemEffects(
        request,
        QStringLiteral("Select at least one day to include.")
        ));

    request = packageRequest(targetPath, QStringLiteral("Jamie"));
    request.selectedClassIds.clear();
    request.targetRoot.clear();
    request.userName.clear();
    QVERIFY(failsBeforeFilesystemEffects(
        request,
        QStringLiteral("No classes meet on the selected days.")
        ));

    request = packageRequest(targetPath, QStringLiteral("Jamie"));
    request.targetRoot = QStringLiteral(" \t ");
    request.userName.clear();
    QVERIFY(failsBeforeFilesystemEffects(
        request,
        QStringLiteral("Choose a target folder.")
        ));

    request = packageRequest(targetPath, QStringLiteral("Jamie"));
    request.userName = QStringLiteral(" \t ");
    QVERIFY(failsBeforeFilesystemEffects(
        request,
        QStringLiteral("Enter your name for the Sub Prep folder.")
        ));

    request = packageRequest(targetPath, QStringLiteral("Jamie"));
    request.subPrep.schedule.days = {QStringLiteral("Not a weekday")};
    QVERIFY(failsBeforeFilesystemEffects(
        request,
        QStringLiteral("A selected schedule weekday is invalid.")
        ));
}

void SubPrepPackageServiceTests
    ::generationConsumesTypedPlanForRosterQueryAndFolderOutput()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    const auto selectedDate =
        ClassMngr::Next::Domain::GregorianDate::fromComponents(
            2026,
            7,
            21
            );
    QVERIFY(selectedDate);

    using namespace ClassMngr::Next::Application;
    SubPrepGenerationPlanRequest planRequest;
    planRequest.selectedDates = {*selectedDate};
    planRequest.selectedClassIds = {typedClassId(42)};
    planRequest.selectedScheduleWeekdays = {
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Monday
    };
    planRequest.mode = ScheduleViewMode::Intensive;
    planRequest.createFolder = true;
    planRequest.requestedRosterColumns = {"Allergies"};
    planRequest.folderLabel = "Planned Label";
    planRequest.hasNonEmptyFolderLabel = true;
    planRequest.targetRootReady = true;
    const auto generationPlan = planSubPrepGeneration(planRequest);
    QVERIFY(generationPlan.isValid());
    QVERIFY(generationPlan.plan);

    ClassInfo info = g_classInfo.value(42);
    info.intensiveTimes = info.classTimes;
    g_classInfo.insert(42, info);

    SubPrepPackageService::Request request = packageRequest(
        targetRoot.path(),
        QStringLiteral("Planned Label")
        );
    request.selectedExtraColumns = {QStringLiteral("Allergies")};
    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(
            std::move(request),
            *generationPlan.plan
            );

    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    QCOMPARE(g_rosterOutputSourceReadPort.loadCount, 1);
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedClassIds,
        std::vector<ClassMngr::Next::Domain::ClassId>{typedClassId(42)}
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedDays,
        std::vector<SubPrepWeekday>{SubPrepWeekday::Tuesday}
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.mode,
        ScheduleViewMode::Intensive
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedExtraColumns,
        std::vector<std::string>{"Allergies"}
        );
    QCOMPARE(
        QFileInfo(result.outputDirectory).fileName(),
        SubPrepPackageService::datedFolderName(
            QStringLiteral("Planned Label"),
            {QDate(2026, 7, 21)}
            )
        );
}

void SubPrepPackageServiceTests
    ::byDayPackageCreatesExpectedTreeAndReplacesTransactionally()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request request =
        packageRequest(targetRoot.path(), QStringLiteral("Alex"));
    const SubPrepPackageService::Result first =
        SubPrepPackageService::generate(request);

    QCOMPARE(first.status, SubPrepPackageService::Status::Completed);
    QVERIFY(first.folderCreated);
    QCOMPARE(
        QFileInfo(first.outputDirectory).fileName(),
        QStringLiteral("Alex (21 Jul 2026)")
        );
    QVERIFY(QFileInfo::exists(
        QDir(first.outputDirectory).filePath(QStringLiteral("Sub Prep.pdf"))
        ));
    QVERIFY(QFileInfo::exists(
        QDir(first.outputDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
        ));

    const QStringList classFolders =
        QDir(first.outputDirectory).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            );
    QCOMPARE(classFolders.size(), 1);
    QVERIFY(classFolders.front().contains(QStringLiteral("E4 Hercules")));
    QVERIFY(classFolders.front().contains(QStringLiteral("Susan")));
    QVERIFY2(
        classFolders.front().contains(QStringLiteral("Tues (4.00)")),
        qPrintable(classFolders.front())
        );
    QCOMPARE(g_rosterOutputSourceReadPort.loadCount, 1);
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedClassIds.size(),
        std::size_t(1)
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedClassIds.front(),
        typedClassId(42)
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedDays.size(),
        std::size_t(1)
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedDays.front(),
        ClassMngr::Next::Application::SubPrepWeekday::Tuesday
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.mode,
        ClassMngr::Next::Application::ScheduleViewMode::Regular
        );

    QFile sentinel(
        QDir(first.outputDirectory).filePath(QStringLiteral("keep-me.txt"))
        );
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    sentinel.write("old");
    sentinel.close();

    const SubPrepPackageService::Result declined =
        SubPrepPackageService::generate(request);
    QCOMPARE(declined.status, SubPrepPackageService::Status::Failed);
    QVERIFY(QFileInfo::exists(sentinel.fileName()));

    request.replaceExisting = true;
    const SubPrepPackageService::Result replaced =
        SubPrepPackageService::generate(request);
    QCOMPARE(replaced.status, SubPrepPackageService::Status::Completed);
    QVERIFY(!QFileInfo::exists(sentinel.fileName()));
    QVERIFY(QFileInfo::exists(
        QDir(replaced.outputDirectory).filePath(QStringLiteral("Sub Prep.pdf"))
        ));
}

void SubPrepPackageServiceTests
    ::folderOpenOutcomePreservesGeneratedPackageArtifacts()
{
    const auto verifyPackageArtifacts =
        [](const SubPrepPackageService::Result& result,
            const QString& expectedDirectory)
        {
            QCOMPARE(result.outputDirectory, expectedDirectory);
            QVERIFY(result.folderCreated);
            QCOMPARE(result.documentPaths.size(), 2);
            for (const QString& documentPath : result.documentPaths)
            {
                QVERIFY(QFileInfo(documentPath).isFile());
                QCOMPARE(
                    QFileInfo(documentPath).absolutePath(),
                    QDir(expectedDirectory).absolutePath()
                    );
            }
        };

    QTemporaryDir failedOpenRoot;
    QVERIFY(failedOpenRoot.isValid());
    const QString failedOpenName = QStringLiteral("Open failed");
    const QString expectedFailedOpenDirectory = QDir(failedOpenRoot.path())
        .filePath(SubPrepPackageService::datedFolderName(
            failedOpenName,
            {QDate(2026, 7, 21)}
            ));
    int failedOpenCalls = 0;
    QUrl failedOpenUrl;
    SubPrepPackageService::Request failedOpenRequest = packageRequest(
        failedOpenRoot.path(),
        failedOpenName
        );
    failedOpenRequest.openFolderAfterGeneration = true;
    failedOpenRequest.folderOpener =
        [&failedOpenCalls, &failedOpenUrl](const QUrl& url)
        {
            ++failedOpenCalls;
            failedOpenUrl = url;
            return false;
        };

    const SubPrepPackageService::Result failedOpen =
        SubPrepPackageService::generate(std::move(failedOpenRequest));
    QCOMPARE(failedOpen.status, SubPrepPackageService::Status::Failed);
    QCOMPARE(
        failedOpen.message,
        QStringLiteral(
            "Sub Prep package created, but the folder could not be opened."
            )
        );
    QCOMPARE(failedOpenCalls, 1);
    QCOMPARE(
        failedOpenUrl,
        QUrl::fromLocalFile(expectedFailedOpenDirectory)
        );
    QVERIFY(!failedOpen.printCanceled);
    verifyPackageArtifacts(failedOpen, expectedFailedOpenDirectory);

    QTemporaryDir successfulOpenRoot;
    QVERIFY(successfulOpenRoot.isValid());
    const QString successfulOpenName = QStringLiteral("Open succeeded");
    const QString expectedSuccessfulOpenDirectory = QDir(
        successfulOpenRoot.path()
        ).filePath(SubPrepPackageService::datedFolderName(
            successfulOpenName,
            {QDate(2026, 7, 21)}
            ));
    int successfulOpenCalls = 0;
    QUrl successfulOpenUrl;
    SubPrepPackageService::Request successfulOpenRequest = packageRequest(
        successfulOpenRoot.path(),
        successfulOpenName
        );
    successfulOpenRequest.openFolderAfterGeneration = true;
    successfulOpenRequest.folderOpener =
        [&successfulOpenCalls, &successfulOpenUrl](const QUrl& url)
        {
            ++successfulOpenCalls;
            successfulOpenUrl = url;
            return true;
        };

    const SubPrepPackageService::Result successfulOpen =
        SubPrepPackageService::generate(std::move(successfulOpenRequest));
    QCOMPARE(successfulOpen.status, SubPrepPackageService::Status::Completed);
    QCOMPARE(successfulOpen.message, QStringLiteral("Sub Prep package created."));
    QCOMPARE(successfulOpenCalls, 1);
    QCOMPARE(
        successfulOpenUrl,
        QUrl::fromLocalFile(expectedSuccessfulOpenDirectory)
        );
    QVERIFY(!successfulOpen.printCanceled);
    verifyPackageArtifacts(successfulOpen, expectedSuccessfulOpenDirectory);

    QTemporaryDir disabledOpenRoot;
    QVERIFY(disabledOpenRoot.isValid());
    const QString disabledOpenName = QStringLiteral("Open disabled");
    const QString expectedDisabledOpenDirectory = QDir(disabledOpenRoot.path())
        .filePath(SubPrepPackageService::datedFolderName(
            disabledOpenName,
            {QDate(2026, 7, 21)}
            ));
    int disabledOpenCalls = 0;
    SubPrepPackageService::Request disabledOpenRequest = packageRequest(
        disabledOpenRoot.path(),
        disabledOpenName
        );
    disabledOpenRequest.openFolderAfterGeneration = false;
    disabledOpenRequest.folderOpener =
        [&disabledOpenCalls](const QUrl&)
        {
            ++disabledOpenCalls;
            return false;
        };

    const SubPrepPackageService::Result disabledOpen =
        SubPrepPackageService::generate(std::move(disabledOpenRequest));
    QCOMPARE(disabledOpen.status, SubPrepPackageService::Status::Completed);
    QCOMPARE(disabledOpen.message, QStringLiteral("Sub Prep package created."));
    QCOMPARE(disabledOpenCalls, 0);
    QVERIFY(!disabledOpen.printCanceled);
    verifyPackageArtifacts(disabledOpen, expectedDisabledOpenDirectory);
}

void SubPrepPackageServiceTests::
acceptsWideTeacherIdAndUsesPreferredDisplayName()
{
    const auto wideTeacherId =
        *ClassMngr::Next::Domain::TeacherId::fromString("2147483648");
    ClassMngr::Next::Application::SubPrepRosterOutputSourceInput typedSource;
    typedSource.teachers.push_back({
        wideTeacherId,
        "Wide Teacher English",
        "Wide Teacher Korean",
        "Preferred Wide Teacher",
        "Wide Teacher Romanization"
    });

    ClassMngr::Next::Application::SubPrepRosterOutputClass sourceClass{
        typedClassId(42)
    };
    sourceClass.teacherId = wideTeacherId;
    sourceClass.classroomName = "Wide ID class";
    sourceClass.grade = "E4";
    sourceClass.level = "Hercules";
    sourceClass.classTeacherEnglishName = "Wide Teacher English";
    sourceClass.classTeacherKoreanName = "Wide Teacher Korean";
    sourceClass.room = "Wide Teacher Room";
    sourceClass.wifiName = "Wide Teacher Network";
    sourceClass.wifiPassword = "Wide Teacher WiFi Password";
    sourceClass.zoomId = "wide-teacher-zoom-id";
    sourceClass.zoomPassword = "wide-teacher-zoom-password";
    sourceClass.meetings.push_back({
        ClassMngr::Next::Application::SubPrepWeekday::Tuesday,
        "4:00 PM",
        "4:50 PM"
    });
    sourceClass.rosterColumns = {"English", "Korean"};
    sourceClass.rosterRows = {{"Alex", "Alex Korean"}};
    typedSource.classes.push_back(std::move(sourceClass));
    g_rosterOutputSourceReadPort.typedSource = std::move(typedSource);

    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());
    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(
            packageRequest(targetRoot.path(), QStringLiteral("Wide teacher"))
            );

    QVERIFY2(
        result.status == SubPrepPackageService::Status::Completed,
        qPrintable(result.message)
        );
    QVERIFY(result.folderCreated);
    QCOMPARE(g_rosterOutputSourceReadPort.loadCount, 1);
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedClassIds.size(),
        std::size_t(1)
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedClassIds.front(),
        typedClassId(42)
        );

    const QStringList classFolders =
        QDir(result.outputDirectory).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            );
    QCOMPARE(classFolders.size(), 1);
    QVERIFY2(
        classFolders.front().contains(QStringLiteral("Preferred Wide Teacher")),
        qPrintable(classFolders.front())
        );
    QVERIFY(classFolders.front().contains(QStringLiteral("E4 Hercules")));
    QVERIFY(classFolders.front().contains(QStringLiteral("Tues (4.00)")));
    QVERIFY(!classFolders.front().contains(QStringLiteral("Wide Teacher English")));
    QVERIFY(!classFolders.front().contains(QStringLiteral("Wide Teacher Romanization")));
    QVERIFY(QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
        ));
}

void SubPrepPackageServiceTests::dailyPackageUsesDailyRosterTemplate()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request request =
        packageRequest(targetRoot.path(), QStringLiteral("Daily"));
    request.rosterTemplate = RosterTemplatePrintService::TemplateId::Daily;

    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(request);

    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    QVERIFY(QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QStringLiteral("Rosters - Daily.pdf")
            )
        ));
    QVERIFY(!QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
        ));
}

void SubPrepPackageServiceTests
    ::perClassPackagePlacesRosterInsideClassFolder()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request request =
        packageRequest(targetRoot.path(), QStringLiteral("Jamie"));
    request.rosterTemplate =
        RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo;
    request.selectedExtraColumns = {QStringLiteral("Allergies")};

    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(request);
    QCOMPARE(result.status, SubPrepPackageService::Status::Completed);
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.selectedExtraColumns,
        (std::vector<std::string>{"Allergies"})
        );
    QVERIFY(!QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
        ));

    const QStringList classFolders =
        QDir(result.outputDirectory).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            );
    QCOMPARE(classFolders.size(), 1);
    QVERIFY(QFileInfo::exists(
        QDir(result.outputDirectory).filePath(
            QDir(classFolders.first()).filePath(QStringLiteral("Roster.pdf"))
            )
        ));
}

void SubPrepPackageServiceTests
    ::classIdsForDaysProjectsScheduleOrderAndAllEntryKinds()
{
    ScheduleViewModel schedule;
    ScheduleRowView firstRow;
    ScheduleCellView monday;
    monday.day = QStringLiteral("Monday");
    monday.entries = {
        ScheduleEntry{
            .classId = 4,
            .kind = ScheduleEntryKind::RegularClass
        },
        ScheduleEntry{
            .classId = 8,
            .kind = ScheduleEntryKind::TestingClass
        },
        ScheduleEntry{
            .classId = 4,
            .kind = ScheduleEntryKind::RegularClass
        }
    };
    ScheduleCellView wednesday;
    wednesday.day = QStringLiteral("Wednesday");
    wednesday.entries = {
        ScheduleEntry{
            .classId = 9,
            .kind = ScheduleEntryKind::RegularClass
        }
    };
    ScheduleCellView nonCanonicalMonday;
    nonCanonicalMonday.day = QStringLiteral(" Monday");
    nonCanonicalMonday.entries = {
        ScheduleEntry{
            .classId = 77,
            .kind = ScheduleEntryKind::TestingClass
        }
    };
    firstRow.cells = {monday, wednesday, nonCanonicalMonday};

    ScheduleRowView secondRow;
    ScheduleCellView laterMonday;
    laterMonday.day = QStringLiteral("Monday");
    laterMonday.entries = {
        ScheduleEntry{
            .classId = 8,
            .kind = ScheduleEntryKind::TestingClass
        },
        ScheduleEntry{
            .classId = -1,
            .kind = ScheduleEntryKind::RegularClass
        },
        ScheduleEntry{
            .classId = 12,
            .kind = ScheduleEntryKind::RegularClass
        }
    };
    secondRow.cells = {laterMonday};
    schedule.rows = {firstRow, secondRow};

    QCOMPARE(
        SubPrepPackageService::classIdsForDays(
            schedule,
            {QStringLiteral("Wednesday"), QStringLiteral("Monday")}
            ),
        (QList<int>{4, 8, 9, 12})
        );
}

void SubPrepPackageServiceTests
    ::intensiveScheduleControlsSelectedDayFiltering()
{
    ClassInfo info = g_classInfo.value(42);
    info.intensiveTimes = info.classTimes;
    info.classTimes.first().day = QStringLiteral("Monday");
    g_classInfo.insert(42, info);

    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request intensiveRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Intensive"));
    intensiveRequest.useIntensiveSchedule = true;
    QCOMPARE(
        SubPrepPackageService::generate(intensiveRequest).status,
        SubPrepPackageService::Status::Completed
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.mode,
        ClassMngr::Next::Application::ScheduleViewMode::Intensive
        );

    SubPrepPackageService::Request regularRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Regular"));
    regularRequest.useIntensiveSchedule = false;
    QCOMPARE(
        SubPrepPackageService::generate(regularRequest).status,
        SubPrepPackageService::Status::Failed
        );
    QCOMPARE(
        g_rosterOutputSourceReadPort.lastRequest.mode,
        ClassMngr::Next::Application::ScheduleViewMode::Regular
        );
}

void SubPrepPackageServiceTests
    ::printOnlyUsesPacketOrderAndReportsCancellation()
{
    SubPrepPackageService::Request request =
        packageRequest(QString(), QString());
    request.createFolder = false;
    request.printPaperCopies = true;

    g_batchPrintStatus = PdfPrintService::Status::Sent;
    const SubPrepPackageService::Result sent =
        SubPrepPackageService::generate(request);
    QCOMPARE(sent.status, SubPrepPackageService::Status::Completed);
    QVERIFY(!sent.folderCreated);
    QCOMPARE(g_batchPrintPaths.size(), 2);
    QCOMPARE(
        QFileInfo(g_batchPrintPaths.at(0)).fileName(),
        QStringLiteral("Sub Prep.pdf")
        );
    QCOMPARE(
        QFileInfo(g_batchPrintPaths.at(1)).fileName(),
        QStringLiteral("Rosters - By Day.pdf")
        );

    g_batchPrintStatus = PdfPrintService::Status::Canceled;
    const SubPrepPackageService::Result canceled =
        SubPrepPackageService::generate(request);
    QCOMPARE(canceled.status, SubPrepPackageService::Status::Canceled);
    QVERIFY(canceled.printCanceled);
    QVERIFY(!canceled.folderCreated);
    QVERIFY(canceled.outputDirectory.isEmpty());
    QCOMPARE(canceled.documentPaths.size(), 2);
    for (const QString& canceledDocumentPath : canceled.documentPaths)
    {
        QVERIFY(!QFileInfo::exists(canceledDocumentPath));
    }

    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());
    SubPrepPackageService::Request failureRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Printer Failure"));
    failureRequest.printPaperCopies = true;
    int folderOpenCalls = 0;
    failureRequest.openFolderAfterGeneration = true;
    failureRequest.folderOpener =
        [&folderOpenCalls](const QUrl&)
        {
            ++folderOpenCalls;
            return false;
        };
    g_batchPrintStatus = PdfPrintService::Status::Failed;
    const SubPrepPackageService::Result failedPrint =
        SubPrepPackageService::generate(failureRequest);
    QCOMPARE(failedPrint.status, SubPrepPackageService::Status::Failed);
    QCOMPARE(failedPrint.message, QStringLiteral("Printer failed"));
    QCOMPARE(folderOpenCalls, 1);
    QVERIFY(failedPrint.folderCreated);
    const QString expectedFailedPrintDirectory = QDir(targetRoot.path())
        .filePath(SubPrepPackageService::datedFolderName(
            QStringLiteral("Printer Failure"),
            {QDate(2026, 7, 21)}
            ));
    QCOMPARE(failedPrint.outputDirectory, expectedFailedPrintDirectory);
    const QStringList expectedFailedPrintDocuments{
        QDir(expectedFailedPrintDirectory).filePath(
            QStringLiteral("Sub Prep.pdf")
            ),
        QDir(expectedFailedPrintDirectory).filePath(
            QStringLiteral("Rosters - By Day.pdf")
            )
    };
    QCOMPARE(failedPrint.documentPaths, expectedFailedPrintDocuments);
    for (const QString& documentPath : failedPrint.documentPaths)
    {
        QVERIFY(QFileInfo(documentPath).isFile());
        QCOMPARE(
            QFileInfo(documentPath).absolutePath(),
            QDir(expectedFailedPrintDirectory).absolutePath()
            );
    }
}

void SubPrepPackageServiceTests::
rosterSourceFailuresDoNotCommitPartialPackages()
{
    QTemporaryDir targetRoot;
    QVERIFY(targetRoot.isValid());

    SubPrepPackageService::Request readFailureRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Read failure"));
    g_rosterOutputSourceReadPort.failReads = true;
    const SubPrepPackageService::Result readFailure =
        SubPrepPackageService::generate(readFailureRequest);
    QCOMPARE(readFailure.status, SubPrepPackageService::Status::Failed);
    QCOMPARE(
        readFailure.message,
        QStringLiteral("Synthetic roster-output read failure.")
        );
    QVERIFY(!readFailure.folderCreated);
    QVERIFY(readFailure.outputDirectory.isEmpty());

    g_rosterOutputSourceReadPort.failReads = false;
    g_rosterOutputSourceReadPort.injectInvalidUtf8 = true;
    SubPrepPackageService::Request mappingFailureRequest =
        packageRequest(targetRoot.path(), QStringLiteral("Mapping failure"));
    const SubPrepPackageService::Result mappingFailure =
        SubPrepPackageService::generate(mappingFailureRequest);
    QCOMPARE(mappingFailure.status, SubPrepPackageService::Status::Failed);
    QVERIFY(mappingFailure.message.contains(
        QStringLiteral("cannot be mapped to roster output")
        ));
    QVERIFY(!mappingFailure.folderCreated);
    QVERIFY(mappingFailure.outputDirectory.isEmpty());
    QCOMPARE(
        QDir(targetRoot.path()).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            ).size(),
        0
        );
}

void SubPrepPackageServiceTests
    ::largePackageGeneratesOutputReferenceWhenConfigured()
{
    populateHeavyOutputData();

    QTemporaryDir temporaryRoot;
    QVERIFY(temporaryRoot.isValid());

    const QString configuredRoot =
        qEnvironmentVariable(
            "CLASSMNGR_SUB_PREP_OUTPUT_REFERENCE_DIR"
            ).trimmed();
    const QString artifactRoot =
        configuredRoot.isEmpty()
            ? temporaryRoot.path()
            : QFileInfo(configuredRoot).absoluteFilePath();
    const QString retentionRoot =
        configuredRoot.isEmpty()
            ? QString()
            : QDir(artifactRoot).filePath(QStringLiteral("reference"));

    if (!configuredRoot.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(artifactRoot),
            qPrintable(
                QStringLiteral("Could not create output reference root: %1")
                    .arg(artifactRoot)
                )
            );
        QVERIFY2(
            QDir().mkpath(retentionRoot),
            qPrintable(
                QStringLiteral("Could not create output reference directory: %1")
                    .arg(retentionRoot)
                )
            );
    }

    const QString packageRoot =
        QDir(temporaryRoot.path()).filePath(QStringLiteral("package"));
    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(
            heavyPackageRequest(packageRoot)
            );

    QVERIFY2(
        result.status == SubPrepPackageService::Status::Completed,
        qPrintable(result.message)
        );
    QVERIFY(result.folderCreated);
    QCOMPARE(result.documentPaths.size(), 2);
    QCOMPARE(
        QDir(result.outputDirectory).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
            ).size(),
        HeavyClassCount
        );

    QJsonArray documents;
    for (const QString& documentPath : result.documentPaths)
    {
        QVERIFY2(
            QFileInfo(documentPath).size() > 0,
            qPrintable(
                QStringLiteral("Generated document is empty: %1")
                    .arg(documentPath)
                )
            );

        QJsonObject documentMetadata;
        const QString captureName =
            QFileInfo(documentPath).completeBaseName()
            + QStringLiteral("-first-page.png");
        const QString retainedDocumentPath =
            configuredRoot.isEmpty()
                ? QString()
                : QDir(retentionRoot).filePath(
                    QFileInfo(documentPath).fileName()
                    );
        if (!retainedDocumentPath.isEmpty())
        {
            if (QFileInfo::exists(retainedDocumentPath))
            {
                QVERIFY(QFile::remove(retainedDocumentPath));
            }
            QVERIFY(QFile::copy(documentPath, retainedDocumentPath));
        }
        const QString capturePath =
            configuredRoot.isEmpty()
                ? QString()
                : QDir(retentionRoot).filePath(captureName);
        QString inspectError;
        QVERIFY2(
            inspectGeneratedPdf(
                documentPath,
                capturePath,
                &documentMetadata,
                &inspectError
            ),
            qPrintable(inspectError)
            );

        const QString baselinePdfPath =
            QDir::current().filePath(
                QStringLiteral(
                    "docs/qt-rewrite/visual-baseline/release/"
                    "sub-prep-output/reference/%1"
                    ).arg(QFileInfo(documentPath).fileName())
                );
        QString parityError;
        QVERIFY2(
            comparePdfToReference(
                documentPath,
                baselinePdfPath,
                &parityError
                ),
            qPrintable(parityError)
            );

        documentMetadata.insert(
            QStringLiteral("relativePath"),
            QDir(artifactRoot)
                .relativeFilePath(
                    retainedDocumentPath.isEmpty()
                        ? documentPath
                        : retainedDocumentPath
                    )
                .replace(QLatin1Char('\\'), QLatin1Char('/'))
            );
        if (!capturePath.isEmpty())
        {
            documentMetadata.insert(
                QStringLiteral("firstPageCapture"),
                QDir(artifactRoot)
                    .relativeFilePath(capturePath)
                    .replace(QLatin1Char('\\'), QLatin1Char('/'))
                );
        }
        documents.append(documentMetadata);
    }

    if (!configuredRoot.isEmpty())
    {
        QJsonObject manifest;
        manifest.insert(
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_startup_cardinality")
            );
        manifest.insert(QStringLiteral("teacherCount"), HeavyTeacherCount);
        manifest.insert(QStringLiteral("classCount"), HeavyClassCount);
        manifest.insert(
            QStringLiteral("rosterRowCount"),
            HeavyClassCount * 25
            );
        manifest.insert(
            QStringLiteral("rosterCellCount"),
            HeavyClassCount * 25 * 3
            );
        manifest.insert(
            QStringLiteral("selectedDate"),
            QStringLiteral("2026-07-21")
            );
        manifest.insert(QStringLiteral("documentCount"), documents.size());
        manifest.insert(QStringLiteral("documents"), documents);

        QFile manifestFile(
            QDir(artifactRoot).filePath(QStringLiteral("manifest.json"))
            );
        QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(
            manifestFile.write(
                QJsonDocument(manifest).toJson(QJsonDocument::Indented)
                ) > 0
            );
        manifestFile.close();
    }
}

QTEST_MAIN(SubPrepPackageServiceTests)

#include "sub_prep_package_service_tests.moc"
