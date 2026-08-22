#include "features/sub_prep/services/sub_prep_package_service.h"

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/teacher.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QPageSize>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <memory>
#include <utility>

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

QString weekdayName(
    const QDate& date
    )
{
    switch (date.dayOfWeek())
    {
    case Qt::Monday:
        return QStringLiteral("Monday");
    case Qt::Tuesday:
        return QStringLiteral("Tuesday");
    case Qt::Wednesday:
        return QStringLiteral("Wednesday");
    case Qt::Thursday:
        return QStringLiteral("Thursday");
    case Qt::Friday:
        return QStringLiteral("Friday");
    case Qt::Saturday:
        return QStringLiteral("Saturday");
    case Qt::Sunday:
        return QStringLiteral("Sunday");
    default:
        return {};
    }
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

QStringList selectedDayNames(
    const QList<QDate>& dates
    )
{
    QStringList days;

    for (const QDate& date : normalizedDates(dates))
    {
        const QString day = weekdayName(date);

        if (!day.isEmpty() && !days.contains(day))
        {
            days.append(day);
        }
    }

    return days;
}

QList<ClassTime> filteredTimes(
    const QList<ClassTime>& times,
    const QStringList& selectedDays
    )
{
    QList<ClassTime> filtered;

    for (const ClassTime& time : times)
    {
        if (selectedDays.contains(time.day.trimmed()))
        {
            filtered.append(time);
        }
    }

    return filtered;
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

    ClassService* classService =
        request.services
            ? request.services->classService()
            : nullptr;
    TeacherService* teacherService =
        request.services
            ? request.services->teacherService()
            : nullptr;
    RosterService* rosterService =
        request.services
            ? request.services->rosterService()
            : nullptr;

    if (
        !classService
        || !classService->isAvailable()
        || !teacherService
        || !teacherService->isAvailable()
        || !rosterService
        || !rosterService->isAvailable()
        )
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("No Teacher Profile is open.");
        }
        return result;
    }

    const QStringList selectedDays = selectedDayNames(request.selectedDates);

    for (int classId : request.classIds)
    {
        if (classId <= 0)
        {
            continue;
        }

        PackageClass packageClass;
        const ::Result<Classroom> classroom =
            classService->classroom(classId);
        if (!classroom)
        {
            if (errorMessage)
            {
                *errorMessage = classroom.error();
            }
            return {};
        }
        packageClass.rosterData.classroom = *classroom;

        if (packageClass.rosterData.classroom.id <= 0)
        {
            continue;
        }

        packageClass.rosterData.info =
            classService->classInfo(classId).value_or(ClassInfo{});
        packageClass.rosterData.roster =
            rosterService->roster(classId).value_or(Roster{});

        if (packageClass.rosterData.info.teacherId > 0)
        {
            const ::Result<Teacher> teacher = teacherService->teacher(
                packageClass.rosterData.info.teacherId);
            if (!teacher)
            {
                if (errorMessage)
                {
                    *errorMessage = teacher.error();
                }
                return {};
            }
            packageClass.teacher = *teacher;
        }

        const QList<ClassTime>& sourceTimes =
            request.useIntensiveSchedule
                ? packageClass.rosterData.info.intensiveTimes
                : packageClass.rosterData.info.classTimes;
        packageClass.rosterData.info.classTimes =
            filteredTimes(sourceTimes, selectedDays);

        if (packageClass.rosterData.info.classTimes.isEmpty())
        {
            continue;
        }

        result.append(packageClass);
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
    }

    if (result.isEmpty() && errorMessage)
    {
        *errorMessage =
            QObject::tr("No classes meet on the selected days.");
    }

    return result;
}

GeneratedPackage generateAt(
    const Request& request,
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

    QString classError;
    QList<PackageClass> classes =
        loadPackageClasses(request, &classError);

    if (classes.isEmpty())
    {
        return {false, classError, {}};
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

    QStringList documents{subPrepRelative};
    QList<RosterTemplatePrintService::RosterClassData> rosterClasses;
    rosterClasses.reserve(classes.size());

    for (const PackageClass& packageClass : std::as_const(classes))
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

        rosterClasses.append(packageClass.rosterData);
    }

    if (
        request.rosterTemplate
        == RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
        )
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
    const Request& request
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
    if (request.classIds.isEmpty())
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
