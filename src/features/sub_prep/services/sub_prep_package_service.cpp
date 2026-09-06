#include "features/sub_prep/services/sub_prep_package_service.h"

#include "app/services/feature_services.h"
#include "classmngr/engine/file_system.h"
#include "classmngr/engine/sub_prep_package.h"
#include "core/application_services.h"
#include "domain/models/teacher.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QPageSize>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SubPrepPackageService
{
namespace
{
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

class TemporaryDirectoryGuard final
{
public:
    TemporaryDirectoryGuard(
        const classmngr::engine::FileSystem& fileSystem,
        std::string path
        )
        : m_fileSystem(fileSystem)
        , m_path(std::move(path))
    {
    }

    ~TemporaryDirectoryGuard()
    {
        if (!m_path.empty())
        {
            (void)m_fileSystem.removeTemporaryDirectory(m_path);
        }
    }

    const std::string& path() const noexcept
    {
        return m_path;
    }

    void release() noexcept
    {
        m_path.clear();
    }

private:
    const classmngr::engine::FileSystem& m_fileSystem;
    std::string m_path;
};

using PortableCalendarDate = classmngr::engine::CalendarDate;
using PortableClassInfo = classmngr::engine::ClassInfo;
using PortableClassTime = classmngr::engine::ClassTime;
using PortablePackageBuildOptions =
    classmngr::engine::SubPrepPackageBuildOptions;
using PortablePackageClass = classmngr::engine::SubPrepPackageClass;
using PortablePackageSourceClass =
    classmngr::engine::SubPrepPackageSourceClass;
using PortableRosterTemplate = classmngr::engine::SubPrepRosterTemplate;
using PortableScheduleCell = classmngr::engine::SubPrepScheduleCell;
using PortableTeacher = classmngr::engine::Teacher;

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {encoded.constData(), static_cast<std::size_t>(encoded.size())};
}

PortableClassTime toPortable(const ClassTime& source)
{
    return {
        toUtf8(source.day),
        toUtf8(source.startTime),
        toUtf8(source.endTime)
    };
}

ClassTime fromPortable(const PortableClassTime& source)
{
    return {
        fromUtf8(source.day),
        fromUtf8(source.startTime),
        fromUtf8(source.endTime)
    };
}

PortableTeacher toPortable(const Teacher& source)
{
    PortableTeacher result;
    result.id = source.id;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.preferredRomanization = toUtf8(source.preferredRomanization);
    result.preferredName = toUtf8(source.preferredName);
    result.roomNumber = toUtf8(source.roomNumber);
    result.birthday = toUtf8(source.birthday);
    result.phoneNumber = toUtf8(source.phoneNumber);
    result.wifiName = toUtf8(source.wifiName);
    result.wifiPassword = toUtf8(source.wifiPassword);
    result.internetType = toUtf8(source.internetType);
    result.zoomId = toUtf8(source.zoomId);
    result.zoomPassword = toUtf8(source.zoomPassword);
    result.projectionType = toUtf8(source.projectionType);
    result.notes = toUtf8(source.notes);
    return result;
}

Teacher fromPortable(const PortableTeacher& source)
{
    Teacher result;
    result.id = source.id;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.preferredRomanization = fromUtf8(source.preferredRomanization);
    result.preferredName = fromUtf8(source.preferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.birthday = fromUtf8(source.birthday);
    result.phoneNumber = fromUtf8(source.phoneNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.notes = fromUtf8(source.notes);
    return result;
}

PortableClassInfo toPortable(const ClassInfo& source)
{
    PortableClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = toUtf8(source.teacherKr);
    result.teacherEn = toUtf8(source.teacherEn);
    result.teacherPreferredName = toUtf8(source.teacherPreferredName);
    result.roomNumber = toUtf8(source.roomNumber);
    result.wifiName = toUtf8(source.wifiName);
    result.wifiPassword = toUtf8(source.wifiPassword);
    result.internetType = toUtf8(source.internetType);
    result.zoomId = toUtf8(source.zoomId);
    result.zoomPassword = toUtf8(source.zoomPassword);
    result.projectionType = toUtf8(source.projectionType);
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.readingBook = toUtf8(source.readingBook);
    result.essayBook = toUtf8(source.essayBook);
    result.classColor = toUtf8(source.classColor);
    result.fontColor = toUtf8(source.fontColor);
    result.notes = toUtf8(source.notes);
    result.timeFillerActivities = toUtf8(source.timeFillerActivities);
    result.classTimes.reserve(source.classTimes.size());
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back(toPortable(time));
    }
    result.intensiveTimes.reserve(source.intensiveTimes.size());
    for (const ClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.push_back(toPortable(time));
    }
    return result;
}

ClassInfo fromPortable(const PortableClassInfo& source)
{
    ClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.teacherPreferredName = fromUtf8(source.teacherPreferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.classGrade = fromUtf8(source.classGrade);
    result.classLevel = fromUtf8(source.classLevel);
    result.readingBook = fromUtf8(source.readingBook);
    result.essayBook = fromUtf8(source.essayBook);
    result.classColor = fromUtf8(source.classColor);
    result.fontColor = fromUtf8(source.fontColor);
    result.notes = fromUtf8(source.notes);
    result.timeFillerActivities = fromUtf8(source.timeFillerActivities);
    result.classTimes.reserve(source.classTimes.size());
    for (const PortableClassTime& time : source.classTimes)
    {
        result.classTimes.append(fromPortable(time));
    }
    result.intensiveTimes.reserve(source.intensiveTimes.size());
    for (const PortableClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.append(fromPortable(time));
    }
    return result;
}

PortableCalendarDate toPortable(const QDate& source)
{
    if (!source.isValid())
    {
        return {};
    }
    return {
        std::chrono::year{source.year()},
        std::chrono::month{static_cast<unsigned>(source.month())},
        std::chrono::day{static_cast<unsigned>(source.day())}
    };
}

std::vector<PortableCalendarDate> toPortable(const QList<QDate>& source)
{
    std::vector<PortableCalendarDate> result;
    result.reserve(source.size());
    for (const QDate& date : source)
    {
        result.push_back(toPortable(date));
    }
    return result;
}

PortableRosterTemplate toPortable(
    RosterTemplatePrintService::TemplateId source
    )
{
    switch (source)
    {
    case RosterTemplatePrintService::TemplateId::ByDay:
        return PortableRosterTemplate::ByDay;
    case RosterTemplatePrintService::TemplateId::Daily:
        return PortableRosterTemplate::Daily;
    case RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo:
        return PortableRosterTemplate::PerClassWithExtraInfo;
    }
    return PortableRosterTemplate::ByDay;
}

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

QList<PackageClass> loadPackageClasses(
    const Request& request,
    QString* errorMessage,
    QStringList* relativeDocumentPaths
    )
{
    if (relativeDocumentPaths)
    {
        relativeDocumentPaths->clear();
    }

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

    std::map<int, PackageClass> loadedClasses;
    std::vector<PortablePackageSourceClass> sourceClasses;

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

        loadedClasses.emplace(packageClass.rosterData.classroom.id, packageClass);
        sourceClasses.push_back({
            {
                packageClass.rosterData.classroom.id,
                toUtf8(packageClass.rosterData.classroom.name)
            },
            toPortable(packageClass.rosterData.info),
            toPortable(packageClass.teacher)
        });
    }

    PortablePackageBuildOptions options;
    options.userName = toUtf8(request.userName);
    options.selectedDates = toPortable(request.selectedDates);
    options.classIds.reserve(request.classIds.size());
    for (const int classId : request.classIds)
    {
        options.classIds.push_back(classId);
    }
    options.useIntensiveSchedule = request.useIntensiveSchedule;
    options.rosterTemplate = toPortable(request.rosterTemplate);

    const auto plan = classmngr::engine::SubPrepPackageService::build(
        sourceClasses,
        options
        );
    if (!plan)
    {
        if (errorMessage)
        {
            *errorMessage = fromUtf8(plan.error().message);
        }
        return result;
    }

    if (relativeDocumentPaths)
    {
        for (const std::string& path : plan->relativeDocumentPaths)
        {
            relativeDocumentPaths->append(fromUtf8(path));
        }
    }

    result.reserve(static_cast<qsizetype>(plan->classes.size()));
    for (const PortablePackageClass& plannedClass : plan->classes)
    {
        const auto loaded = loadedClasses.find(plannedClass.classroom.id);
        if (loaded == loadedClasses.end())
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "The selected class could not be loaded."
                    );
            }
            result.clear();
            if (relativeDocumentPaths)
            {
                relativeDocumentPaths->clear();
            }
            return result;
        }

        PackageClass packageClass = loaded->second;
        packageClass.rosterData.info = fromPortable(plannedClass.info);
        packageClass.teacher = fromPortable(plannedClass.teacher);
        packageClass.folderName = fromUtf8(plannedClass.folderName);
        result.append(std::move(packageClass));
    }

    return result;
}

GeneratedPackage generateAt(
    const Request& request,
    const QString& packageDirectory
    )
{
    const classmngr::engine::StandardFileSystem fileSystem;
    if (!fileSystem.createDirectories(toUtf8(packageDirectory)).has_value())
    {
        return {
            false,
            QObject::tr("Unable to create the Sub Prep package folder."),
            {}
        };
    }

    QString classError;
    QStringList relativeDocumentPaths;
    QList<PackageClass> classes =
        loadPackageClasses(
            request,
            &classError,
            &relativeDocumentPaths
            );

    if (classes.isEmpty())
    {
        return {false, classError, {}};
    }

    if (relativeDocumentPaths.size() < 2)
    {
        return {
            false,
            QObject::tr("The Sub Prep package plan is incomplete."),
            {}
        };
    }

    const QString subPrepRelative = relativeDocumentPaths.first();
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

        if (!fileSystem.createDirectories(toUtf8(classDirectory)).has_value())
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
        for (int index = 0; index < classes.size(); ++index)
        {
            const PackageClass& packageClass = classes.at(index);
            const QString relativePath = relativeDocumentPaths.at(index + 1);
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
        const QString relativePath = relativeDocumentPaths.at(1);
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
    return fromUtf8(
        classmngr::engine::SubPrepPackageService::safePathComponent(
            toUtf8(value),
            toUtf8(fallback)
            )
        );
}

QString datedFolderName(
    const QString& userName,
    const QList<QDate>& selectedDates
    )
{
    return fromUtf8(
        classmngr::engine::SubPrepPackageService::datedFolderName(
            toUtf8(userName),
            toPortable(selectedDates)
            )
        );
}

QList<int> classIdsForDays(
    const ScheduleViewModel& schedule,
    const QStringList& selectedDays
    )
{
    std::vector<PortableScheduleCell> portableSchedule;
    for (const ScheduleRowView& row : schedule.rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            PortableScheduleCell portableCell;
            portableCell.day = toUtf8(cell.day);
            portableCell.classIds.reserve(cell.entries.size());
            for (const ScheduleEntry& entry : cell.entries)
            {
                portableCell.classIds.push_back(entry.classId);
            }
            portableSchedule.push_back(std::move(portableCell));
        }
    }

    std::vector<std::string> portableDays;
    portableDays.reserve(selectedDays.size());
    for (const QString& day : selectedDays)
    {
        portableDays.push_back(toUtf8(day));
    }

    const std::vector<int> portableIds =
        classmngr::engine::SubPrepPackageService::classIdsForDays(
            portableSchedule,
            portableDays
            );
    QList<int> result;
    for (const int classId : portableIds)
    {
        result.append(classId);
    }
    return result;
}

Result generate(
    const Request& request
    )
{
    if (!request.createFolder && !request.printPaperCopies)
    {
        return failed(QObject::tr("Choose a folder or paper-copy action."));
    }
    if (classmngr::engine::SubPrepPackageService::selectedDayNames(
            toPortable(request.selectedDates)
            ).empty())
    {
        return failed(QObject::tr("Select at least one day to include."));
    }
    if (request.classIds.isEmpty())
    {
        return failed(QObject::tr("No classes meet on the selected days."));
    }

    const classmngr::engine::StandardFileSystem fileSystem;
    QString packageDirectory;
    QStringList documentPaths;
    std::unique_ptr<TemporaryDirectoryGuard> printOnlyDirectory;
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
        if (!fileSystem.createDirectories(toUtf8(request.targetRoot)).has_value())
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
        const auto finalPathExists = fileSystem.exists(toUtf8(finalPath));
        if (!finalPathExists)
        {
            return failed(QObject::tr("Unable to inspect the target folder."));
        }
        if (*finalPathExists && !request.replaceExisting)
        {
            return failed(
                QObject::tr("The Sub Prep folder already exists.")
                );
        }

        const auto stagingPath = fileSystem.createTemporaryDirectory(
            toUtf8(request.targetRoot)
            );
        if (!stagingPath)
        {
            return failed(QObject::tr("Unable to create a staging folder."));
        }
        auto stagingDirectory = std::make_unique<TemporaryDirectoryGuard>(
            fileSystem,
            *stagingPath
            );

        const GeneratedPackage generated =
            generateAt(request, fromUtf8(stagingDirectory->path()));
        if (!generated.success)
        {
            return failed(generated.message);
        }

        const auto replaced = fileSystem.replaceDirectoryAtomically(
            stagingDirectory->path(),
            toUtf8(finalPath)
            );
        if (!replaced)
        {
            return failed(QObject::tr("Unable to commit the Sub Prep package."));
        }
        stagingDirectory->release();

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
        const auto temporaryPath = fileSystem.createTemporaryDirectory(
            toUtf8(QDir::tempPath())
            );
        if (!temporaryPath)
        {
            return failed(QObject::tr("Unable to create a temporary print folder."));
        }
        printOnlyDirectory = std::make_unique<TemporaryDirectoryGuard>(
            fileSystem,
            *temporaryPath
            );

        const GeneratedPackage generated =
            generateAt(request, fromUtf8(printOnlyDirectory->path()));
        if (!generated.success)
        {
            return failed(generated.message);
        }

        packageDirectory = fromUtf8(printOnlyDirectory->path());
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
