#include "features/sub_prep/services/sub_prep_package_service.h"

#include "core/utils/sidebar_node_naming.h"
#include "domain/models/teacher.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QByteArray>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QPageSize>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace SubPrepPackageService
{
namespace
{
constexpr int MaximumPathComponentLength = 120;

struct PackageClass
{
    RosterTemplatePrintService::RosterClassData rosterData;
    Teacher teacher;
    QString folderName;
};

struct GeneratedPackage
{
    bool success = false;
    QString message;
    QStringList relativeDocumentPaths;
};

Result failed(
    const QString& message
    )
{
    return {
        Status::Failed,
        message.trimmed().isEmpty()
            ? QObject::tr("Sub Prep package generation failed.")
            : message
    };
}

QList<QDate> normalizedDates(
    const QList<QDate>& dates
    )
{
    QList<QDate> result;

    for (const QDate& date : dates)
    {
        if (date.isValid() && !result.contains(date))
        {
            result.append(date);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::optional<ClassMngr::Next::Application::SubPrepWeekday>
weekdayForDate(const QDate& date)
{
    using ClassMngr::Next::Application::SubPrepWeekday;
    switch (date.dayOfWeek())
    {
    case Qt::Monday:
        return SubPrepWeekday::Monday;
    case Qt::Tuesday:
        return SubPrepWeekday::Tuesday;
    case Qt::Wednesday:
        return SubPrepWeekday::Wednesday;
    case Qt::Thursday:
        return SubPrepWeekday::Thursday;
    case Qt::Friday:
        return SubPrepWeekday::Friday;
    case Qt::Saturday:
        return SubPrepWeekday::Saturday;
    case Qt::Sunday:
        return SubPrepWeekday::Sunday;
    default:
        return std::nullopt;
    }
}

std::optional<QString> weekdayLabel(
    const ClassMngr::Next::Application::SubPrepWeekday weekday
    )
{
    using ClassMngr::Next::Application::SubPrepWeekday;
    switch (weekday)
    {
    case SubPrepWeekday::Monday:
        return QStringLiteral("Monday");
    case SubPrepWeekday::Tuesday:
        return QStringLiteral("Tuesday");
    case SubPrepWeekday::Wednesday:
        return QStringLiteral("Wednesday");
    case SubPrepWeekday::Thursday:
        return QStringLiteral("Thursday");
    case SubPrepWeekday::Friday:
        return QStringLiteral("Friday");
    case SubPrepWeekday::Saturday:
        return QStringLiteral("Saturday");
    case SubPrepWeekday::Sunday:
        return QStringLiteral("Sunday");
    }
    return std::nullopt;
}

std::optional<int> legacyId(
    const std::string& value
    )
{
    if (value.empty())
    {
        return std::nullopt;
    }

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

std::optional<QString> decodedText(const std::string& value)
{
    const QByteArray bytes(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
    const QString decoded = QString::fromUtf8(bytes);
    if (decoded.toUtf8() != bytes)
    {
        return std::nullopt;
    }
    return decoded;
}

QString uniqueFolderName(
    const QString& preferred,
    QSet<QString>* usedNames
    )
{
    const QString base = safePathComponent(preferred, QStringLiteral("Class"));
    QString candidate = base;
    int suffix = 2;

    while (usedNames && usedNames->contains(candidate.toCaseFolded()))
    {
        candidate = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
    }

    if (usedNames)
    {
        usedNames->insert(candidate.toCaseFolded());
    }

    return candidate;
}

QList<PackageClass> loadPackageClasses(
    const Request& request,
    QString* errorMessage
    )
{
    QList<PackageClass> result;
    if (!request.rosterOutputSourceReadPort)
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("No Teacher Profile is open.");
        }
        return result;
    }

    ClassMngr::Next::Application::SubPrepRosterOutputSourceRequest
        sourceRequest;
    sourceRequest.selectedClassIds = request.selectedClassIds;
    sourceRequest.mode = request.useIntensiveSchedule
        ? ClassMngr::Next::Application::ScheduleViewMode::Intensive
        : ClassMngr::Next::Application::ScheduleViewMode::Regular;
    for (const QDate& date : normalizedDates(request.selectedDates))
    {
        const auto weekday = weekdayForDate(date);
        if (weekday
            && std::find(
                sourceRequest.selectedDays.cbegin(),
                sourceRequest.selectedDays.cend(),
                *weekday
                ) == sourceRequest.selectedDays.cend())
        {
            sourceRequest.selectedDays.push_back(*weekday);
        }
    }
    sourceRequest.selectedExtraColumns.reserve(
        static_cast<std::size_t>(request.selectedExtraColumns.size())
        );
    for (const QString& column : request.selectedExtraColumns)
    {
        const QByteArray bytes = column.toUtf8();
        sourceRequest.selectedExtraColumns.emplace_back(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    ClassMngr::Next::Application::SubPrepRosterOutputSourceQuery query(
        *request.rosterOutputSourceReadPort
        );
    const auto loadedSource = query.execute(sourceRequest);
    if (!loadedSource)
    {
        if (errorMessage)
        {
            const std::string& message = loadedSource.error().message;
            *errorMessage = message.empty()
                ? QObject::tr("Selected Sub Prep rosters could not be loaded.")
                : QString::fromUtf8(
                    message.data(),
                    static_cast<qsizetype>(message.size())
                    );
        }
        return result;
    }

    const ClassMngr::Next::Application::SubPrepRosterOutputSource& source =
        loadedSource.value();
    QHash<int, Teacher> teachersById;
    teachersById.reserve(static_cast<qsizetype>(source.teachers().size()));
    for (const auto& sourceTeacher : source.teachers())
    {
        const auto teacherId = legacyId(sourceTeacher.id.value());
        const auto englishName = decodedText(sourceTeacher.englishName);
        const auto koreanName = decodedText(sourceTeacher.koreanName);
        const auto preferredName = decodedText(sourceTeacher.preferredName);
        const auto preferredRomanization = decodedText(
            sourceTeacher.preferredRomanization
            );
        if (!teacherId || !englishName || !koreanName || !preferredName
            || !preferredRomanization)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "A selected Sub Prep teacher cannot be mapped to roster output."
                    );
            }
            return {};
        }

        Teacher teacher;
        teacher.id = *teacherId;
        teacher.teacherEn = *englishName;
        teacher.teacherKr = *koreanName;
        teacher.preferredName = *preferredName;
        teacher.preferredRomanization = *preferredRomanization;
        teachersById.insert(*teacherId, std::move(teacher));
    }

    for (const auto& sourceClass : source.classes())
    {
        if (sourceClass.meetings.empty())
        {
            continue;
        }

        const auto classId = legacyId(sourceClass.id.value());
        const auto classroomName = decodedText(sourceClass.classroomName);
        const auto grade = decodedText(sourceClass.grade);
        const auto level = decodedText(sourceClass.level);
        const auto classTeacherEnglishName = decodedText(
            sourceClass.classTeacherEnglishName
            );
        const auto classTeacherKoreanName = decodedText(
            sourceClass.classTeacherKoreanName
            );
        const auto room = decodedText(sourceClass.room);
        const auto wifiName = decodedText(sourceClass.wifiName);
        const auto wifiPassword = decodedText(sourceClass.wifiPassword);
        const auto zoomId = decodedText(sourceClass.zoomId);
        const auto zoomPassword = decodedText(sourceClass.zoomPassword);
        if (!classId || !classroomName || !grade || !level
            || !classTeacherEnglishName || !classTeacherKoreanName || !room
            || !wifiName || !wifiPassword || !zoomId || !zoomPassword)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "A selected Sub Prep class cannot be mapped to roster output."
                    );
            }
            return {};
        }

        PackageClass packageClass;
        packageClass.rosterData.classroom.id = *classId;
        packageClass.rosterData.classroom.name = *classroomName;

        ClassInfo& info = packageClass.rosterData.info;
        info.classId = *classId;
        info.teacherId = -1;
        info.classGrade = *grade;
        info.classLevel = *level;
        info.teacherEn = *classTeacherEnglishName;
        info.teacherKr = *classTeacherKoreanName;
        info.roomNumber = *room;
        info.wifiName = *wifiName;
        info.wifiPassword = *wifiPassword;
        info.zoomId = *zoomId;
        info.zoomPassword = *zoomPassword;

        if (sourceClass.teacherId.has_value())
        {
            const auto teacherId = legacyId(sourceClass.teacherId->value());
            if (!teacherId || !teachersById.contains(*teacherId))
            {
                if (errorMessage)
                {
                    *errorMessage = QObject::tr(
                        "A selected Sub Prep class references an unavailable teacher."
                        );
                }
                return {};
            }
            info.teacherId = *teacherId;
            packageClass.teacher = teachersById.value(*teacherId);
        }
        else
        {
            packageClass.teacher.id = -1;
        }

        for (const auto& sourceMeeting : sourceClass.meetings)
        {
            const auto day = weekdayLabel(sourceMeeting.weekday);
            const auto startTime = decodedText(sourceMeeting.startTime);
            const auto endTime = decodedText(sourceMeeting.endTime);
            if (!day || !startTime || !endTime)
            {
                if (errorMessage)
                {
                    *errorMessage = QObject::tr(
                        "A selected Sub Prep meeting cannot be mapped to roster output."
                        );
                }
                return {};
            }
            info.classTimes.append({*day, *startTime, *endTime});
        }

        Roster& roster = packageClass.rosterData.roster;
        for (const std::string& sourceColumn : sourceClass.rosterColumns)
        {
            const auto column = decodedText(sourceColumn);
            if (!column)
            {
                if (errorMessage)
                {
                    *errorMessage = QObject::tr(
                        "A selected roster column cannot be mapped to renderer data."
                        );
                }
                return {};
            }
            roster.columns.append(*column);
            roster.columnWidths.append(0);
        }

        roster.rows.reserve(
            static_cast<qsizetype>(sourceClass.rosterRows.size())
            );
        for (const auto& sourceRow : sourceClass.rosterRows)
        {
            QStringList row;
            row.reserve(static_cast<qsizetype>(sourceRow.size()));
            for (const std::string& sourceCell : sourceRow)
            {
                const auto cell = decodedText(sourceCell);
                if (!cell)
                {
                    if (errorMessage)
                    {
                        *errorMessage = QObject::tr(
                            "A selected roster cell cannot be mapped to renderer data."
                            );
                    }
                    return {};
                }
                row.append(*cell);
            }
            roster.rows.append(std::move(row));
        }

        result.append(std::move(packageClass));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const PackageClass& left, const PackageClass& right)
        {
            const QString leftName =
                SidebarNodeNaming::formatClassDisplayName(
                    left.rosterData.info,
                    left.teacher
                    );
            const QString rightName =
                SidebarNodeNaming::formatClassDisplayName(
                    right.rosterData.info,
                    right.teacher
                    );
            const int comparison =
                QString::localeAwareCompare(leftName, rightName);

            return comparison != 0
                ? comparison < 0
                : left.rosterData.classroom.id
                    < right.rosterData.classroom.id;
        }
        );

    QSet<QString> usedNames;
    for (PackageClass& packageClass : result)
    {
        packageClass.folderName =
            uniqueFolderName(
                SidebarNodeNaming::formatClassDisplayName(
                    packageClass.rosterData.info,
                    packageClass.teacher
                    ),
                &usedNames
                );
        packageClass.teacher = {};
    }

    if (result.isEmpty() && errorMessage)
    {
        *errorMessage =
            QObject::tr("No classes meet on the selected days.");
    }

    return result;
}

GeneratedPackage generateAt(
    Request& request,
    const QString& packageDirectory
    )
{
    if (!QDir().mkpath(packageDirectory))
    {
        return {
            false,
            QObject::tr("Unable to create the Sub Prep package folder."),
            {}
        };
    }

    const QString subPrepRelative = QStringLiteral("Sub Prep.pdf");
    const QString subPrepPath =
        QDir(packageDirectory).filePath(subPrepRelative);
    const SubPrepPrintService::Result subPrepResult =
        SubPrepPrintService::saveSubPrepPdf(
            request.subPrep,
            subPrepPath
            );

    if (subPrepResult.status != SubPrepPrintService::Status::Sent)
    {
        return {false, subPrepResult.message, {}};
    }

    // The information-sheet inputs are not needed by roster generation.
    // Release the schedule and class/teacher projection before loading full
    // roster records for the next output stage.
    request.subPrep = {};

    QString classError;
    QList<PackageClass> classes =
        loadPackageClasses(request, &classError);

    if (classes.isEmpty())
    {
        return {false, classError, {}};
    }

    QStringList documents{subPrepRelative};
    QList<RosterTemplatePrintService::RosterClassData> rosterClasses;
    const bool perClassRoster = request.rosterTemplate
        == RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo;
    if (!perClassRoster)
    {
        rosterClasses.reserve(classes.size());
    }

    for (PackageClass& packageClass : classes)
    {
        const QString classDirectory =
            QDir(packageDirectory).filePath(packageClass.folderName);

        if (!QDir().mkpath(classDirectory))
        {
            return {
                false,
                QObject::tr("Unable to create the class folder \"%1\".")
                    .arg(packageClass.folderName),
                {}
            };
        }

        if (!perClassRoster)
        {
            rosterClasses.append(std::move(packageClass.rosterData));
        }
    }

    if (perClassRoster)
    {
        for (const PackageClass& packageClass : std::as_const(classes))
        {
            const QString relativePath =
                QDir(packageClass.folderName).filePath(
                    QStringLiteral("Roster.pdf")
                    );
            const auto rosterResult =
                RosterTemplatePrintService::saveRostersPdf(
                    {packageClass.rosterData},
                    QDir(packageDirectory).filePath(relativePath),
                    request.rosterTemplate,
                    request.selectedExtraColumns,
                    request.perClassOrientation
                    );

            if (rosterResult.status != RosterTemplatePrintService::Status::Sent)
            {
                return {false, rosterResult.message, {}};
            }

            documents.append(relativePath);
        }
    }
    else
    {
        const QString relativePath =
            QStringLiteral("Rosters - %1.pdf")
                .arg(
                    RosterTemplatePrintService::templateDisplayName(
                        request.rosterTemplate
                        )
                    );
        const auto rosterResult =
            RosterTemplatePrintService::saveRostersPdf(
                rosterClasses,
                QDir(packageDirectory).filePath(relativePath),
                request.rosterTemplate
                );

        if (rosterResult.status != RosterTemplatePrintService::Status::Sent)
        {
            return {false, rosterResult.message, {}};
        }

        documents.append(relativePath);
    }

    return {true, QString(), documents};
}

QStringList absoluteDocumentPaths(
    const QString& directory,
    const QStringList& relativePaths
    )
{
    QStringList paths;
    paths.reserve(relativePaths.size());

    for (const QString& relativePath : relativePaths)
    {
        paths.append(QDir(directory).filePath(relativePath));
    }

    return paths;
}

bool isDirectChildPath(
    const QString& parent,
    const QString& child
    )
{
    const QString cleanParent =
        QDir::cleanPath(QFileInfo(parent).absoluteFilePath());
    const QString cleanChild =
        QDir::cleanPath(QFileInfo(child).absoluteFilePath());

    return QFileInfo(cleanChild).absolutePath().compare(
               cleanParent,
               Qt::CaseInsensitive
               ) == 0
        && cleanChild.compare(cleanParent, Qt::CaseInsensitive) != 0;
}

QString backupName()
{
    return QStringLiteral(".classmngr-sub-prep-backup-%1")
        .arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            );
}
}

QString defaultTargetRoot()
{
    QString documentsPath =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );

    if (documentsPath.trimmed().isEmpty())
    {
        QString homePath =
            QStandardPaths::writableLocation(
                QStandardPaths::HomeLocation
                );

        if (homePath.trimmed().isEmpty())
        {
            homePath = QDir::homePath();
        }

        documentsPath = QDir(homePath).filePath(QStringLiteral("Documents"));
    }

    return QDir(documentsPath).filePath(
        QStringLiteral("DYB/Sub_Prep")
        );
}

QString safePathComponent(
    const QString& value,
    const QString& fallback
    )
{
    QString result = value.trimmed();
    result.replace(QChar(0x2022), QStringLiteral(" - "));

    const QString invalid = QStringLiteral("<>\"/\\|?*");
    for (qsizetype index = 0; index < result.size(); ++index)
    {
        const QChar character = result.at(index);

        if (character == QLatin1Char(':'))
        {
            result[index] = QLatin1Char('.');
        }
        else if (invalid.contains(character) || character.unicode() < 32)
        {
            result[index] = QLatin1Char('-');
        }
    }

    result.replace(
        QRegularExpression(QStringLiteral("\\s+")),
        QStringLiteral(" ")
        );
    result.replace(
        QRegularExpression(QStringLiteral("\\s*-+\\s*")),
        QStringLiteral(" - ")
        );
    result = result.trimmed();

    while (result.endsWith(QLatin1Char('.'))
           || result.endsWith(QLatin1Char(' ')))
    {
        result.chop(1);
    }

    if (result.isEmpty())
    {
        result = fallback.trimmed();
    }
    if (result.isEmpty())
    {
        result = QStringLiteral("Sub Prep");
    }

    static const QSet<QString> reservedNames{
        QStringLiteral("CON"), QStringLiteral("PRN"),
        QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"),
        QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"),
        QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")
    };

    if (reservedNames.contains(result.toUpper()))
    {
        result.prepend(QLatin1Char('_'));
    }

    if (result.size() > MaximumPathComponentLength)
    {
        result = result.left(MaximumPathComponentLength).trimmed();
        while (result.endsWith(QLatin1Char('.')))
        {
            result.chop(1);
        }
    }

    return result;
}

QString datedFolderName(
    const QString& userName,
    const QList<QDate>& selectedDates
    )
{
    const QList<QDate> dates = normalizedDates(selectedDates);

    if (dates.isEmpty())
    {
        return {};
    }

    const QDate first = dates.first();
    const QDate last = dates.last();
    QString datePart;

    if (first == last)
    {
        datePart = first.toString(QStringLiteral("dd MMM yyyy"));
    }
    else if (first.year() != last.year())
    {
        datePart = QStringLiteral("%1 - %2")
            .arg(
                first.toString(QStringLiteral("dd MMM yyyy")),
                last.toString(QStringLiteral("dd MMM yyyy"))
                );
    }
    else if (first.month() != last.month())
    {
        datePart = QStringLiteral("%1 - %2")
            .arg(
                first.toString(QStringLiteral("dd MMM")),
                last.toString(QStringLiteral("dd MMM yyyy"))
                );
    }
    else
    {
        datePart = QStringLiteral("%1 - %2")
            .arg(
                first.toString(QStringLiteral("dd")),
                last.toString(QStringLiteral("dd MMM yyyy"))
                );
    }

    return QStringLiteral("%1 (%2)")
        .arg(
            safePathComponent(userName, QStringLiteral("Sub Prep")),
            datePart
            );
}

QList<int> classIdsForDays(
    const ScheduleViewModel& schedule,
    const QStringList& selectedDays
    )
{
    QList<int> ids;

    for (const ScheduleRowView& row : schedule.rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            if (!selectedDays.contains(cell.day))
            {
                continue;
            }

            for (const ScheduleEntry& entry : cell.entries)
            {
                if (entry.classId > 0 && !ids.contains(entry.classId))
                {
                    ids.append(entry.classId);
                }
            }
        }
    }

    return ids;
}

Result generate(
    Request request
    )
{
    if (!request.createFolder && !request.printPaperCopies)
    {
        return failed(QObject::tr("Choose a folder or paper-copy action."));
    }
    if (normalizedDates(request.selectedDates).isEmpty())
    {
        return failed(QObject::tr("Select at least one day to include."));
    }
    if (request.selectedClassIds.empty())
    {
        return failed(QObject::tr("No classes meet on the selected days."));
    }

    QString packageDirectory;
    QStringList documentPaths;
    std::unique_ptr<QTemporaryDir> printOnlyDirectory;
    bool folderCreated = false;

    if (request.createFolder)
    {
        if (request.targetRoot.trimmed().isEmpty())
        {
            return failed(QObject::tr("Choose a target folder."));
        }
        if (request.userName.trimmed().isEmpty())
        {
            return failed(QObject::tr("Enter your name for the Sub Prep folder."));
        }
        if (!QDir().mkpath(request.targetRoot))
        {
            return failed(QObject::tr("Unable to create the target folder."));
        }

        const QString finalName =
            datedFolderName(request.userName, request.selectedDates);
        const QString finalPath =
            QDir(request.targetRoot).filePath(finalName);

        if (!isDirectChildPath(request.targetRoot, finalPath))
        {
            return failed(QObject::tr("The generated folder path is not safe."));
        }
        if (QFileInfo::exists(finalPath) && !request.replaceExisting)
        {
            return failed(
                QObject::tr("The Sub Prep folder already exists.")
                );
        }

        QTemporaryDir stagingDirectory(
            QDir(request.targetRoot).filePath(
                QStringLiteral(".classmngr-sub-prep-XXXXXX")
                )
            );
        if (!stagingDirectory.isValid())
        {
            return failed(QObject::tr("Unable to create a staging folder."));
        }

        const GeneratedPackage generated =
            generateAt(request, stagingDirectory.path());
        if (!generated.success)
        {
            return failed(generated.message);
        }

        QString backupPath;
        if (QFileInfo::exists(finalPath))
        {
            backupPath =
                QDir(request.targetRoot).filePath(backupName());

            if (!QDir().rename(finalPath, backupPath))
            {
                return failed(
                    QObject::tr("Unable to preserve the existing Sub Prep folder.")
                    );
            }
        }

        stagingDirectory.setAutoRemove(false);
        if (!QDir().rename(stagingDirectory.path(), finalPath))
        {
            stagingDirectory.setAutoRemove(true);

            if (!backupPath.isEmpty())
            {
                if (!QDir().rename(backupPath, finalPath))
                {
                    return failed(
                        QObject::tr(
                            "Unable to commit the Sub Prep package. "
                            "The previous folder remains at:\n%1"
                            )
                            .arg(backupPath)
                        );
                }
            }

            return failed(QObject::tr("Unable to commit the Sub Prep package."));
        }

        if (!backupPath.isEmpty())
        {
            QDir(backupPath).removeRecursively();
        }

        packageDirectory = finalPath;
        documentPaths =
            absoluteDocumentPaths(
                packageDirectory,
                generated.relativeDocumentPaths
                );
        folderCreated = true;
    }
    else
    {
        printOnlyDirectory = std::make_unique<QTemporaryDir>();

        if (!printOnlyDirectory->isValid())
        {
            return failed(QObject::tr("Unable to create a temporary print folder."));
        }

        const GeneratedPackage generated =
            generateAt(request, printOnlyDirectory->path());
        if (!generated.success)
        {
            return failed(generated.message);
        }

        packageDirectory = printOnlyDirectory->path();
        documentPaths =
            absoluteDocumentPaths(
                packageDirectory,
                generated.relativeDocumentPaths
                );
    }

    bool printCanceled = false;
    QString printFailure;
    QString completionMessage =
        folderCreated
            ? QObject::tr("Sub Prep package created.")
            : QString();

    if (request.printPaperCopies)
    {
        const PdfPrintService::Result printResult =
            PdfPrintService::printPdfDocuments(
                {
                    request.parent,
                    documentPaths,
                    QObject::tr("Print Sub Prep Package"),
                    QPageLayout::Portrait,
                    QPageSize::A4,
                    true
                }
                );

        if (printResult.status == PdfPrintService::Status::Failed)
        {
            printFailure = printResult.message;
        }
        else
        {
            printCanceled =
                printResult.status == PdfPrintService::Status::Canceled;
        }

        if (printFailure.isEmpty() && !printCanceled && !folderCreated)
        {
            completionMessage = QObject::tr("Sub Prep print job sent.");
        }
    }

    if (
        folderCreated
        && request.openFolderAfterGeneration
        && !QDesktopServices::openUrl(QUrl::fromLocalFile(packageDirectory))
        )
    {
        completionMessage =
            QObject::tr("Sub Prep package created, but the folder could not be opened.");
    }

    if (!printFailure.isEmpty())
    {
        Result result = failed(printFailure);
        result.outputDirectory = folderCreated ? packageDirectory : QString();
        result.documentPaths = documentPaths;
        result.folderCreated = folderCreated;
        return result;
    }

    return {
        folderCreated || !printCanceled
            ? Status::Completed
            : Status::Canceled,
        completionMessage,
        folderCreated ? packageDirectory : QString(),
        documentPaths,
        folderCreated,
        printCanceled
    };
}
}
