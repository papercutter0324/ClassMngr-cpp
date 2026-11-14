#include "core/qt_pdf_generation_adapter.h"
#include "features/schedule/ui/schedule_builder.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "features/sub_prep/services/sub_prep_print_service.h"
#include "features/sub_prep/ui/sub_prep_print_source_mapper.h"
#include "next/application/sub_prep_weekday.h"
#include "next/platform/qt_font_family_availability_adapter.h"
#include "next/platform/sub_prep_campus_directory_query_adapter.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/ui/sub_prep_output_font_policy.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QLocale>
#include <QPdfDocument>
#include <QRegularExpression>
#include <QSysInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace ClassMngr;
using namespace ClassMngr::Next;

constexpr std::array<const char*, 5> Weekdays{
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"
};

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(), static_cast<qsizetype>(value.size()));
}

QString valueOrNotAvailable(const QString& value)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty()
        ? QCoreApplication::translate("SubPrepPage", "N/A")
        : trimmed;
}

QString operationError(const Next::Domain::OperationError& error)
{
    return fromUtf8(error.message);
}

QString sha256File(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
    {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString imageSha256(const QImage& image)
{
    const QImage normalized = image.convertToFormat(QImage::Format_RGBA8888);
    const auto bytes = QByteArray(
        reinterpret_cast<const char*>(normalized.constBits()),
        static_cast<qsizetype>(normalized.sizeInBytes()));
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool saveJson(const QString& path, const QJsonObject& value, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        *error = file.errorString();
        return false;
    }
    const QByteArray bytes = QJsonDocument(value).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit())
    {
        *error = file.errorString();
        return false;
    }
    return true;
}

bool saveText(const QString& path, const QString& value, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        *error = file.errorString();
        return false;
    }
    const QByteArray bytes = value.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.flush())
    {
        *error = file.errorString();
        return false;
    }
    return true;
}

bool loadPdfText(
    const QString& path,
    QPdfDocument* document,
    QString* text,
    QString* error
    )
{
    const auto status = document->load(path);
    if (status != QPdfDocument::Error::None
        || document->status() != QPdfDocument::Status::Ready
        || document->pageCount() <= 0)
    {
        *error = QStringLiteral("Could not load PDF: %1 (status %2)")
            .arg(path)
            .arg(static_cast<int>(status));
        return false;
    }
    for (int page = 0; page < document->pageCount(); ++page)
    {
        *text += document->getAllText(page).text();
        *text += QLatin1Char('\n');
    }
    return true;
}

int exactOccurrences(const QString& text, const QString& expected)
{
    const QRegularExpression expression(
        QStringLiteral("(?<![A-Za-z0-9-])%1(?![A-Za-z0-9-])")
            .arg(QRegularExpression::escape(expected)));
    auto matches = expression.globalMatch(text);
    int count = 0;
    while (matches.hasNext())
    {
        matches.next();
        ++count;
    }
    return count;
}

QJsonArray stringArray(const QJsonValue& value)
{
    QJsonArray array;
    for (const QJsonValue& item : value.toArray())
    {
        array.append(item.toString());
    }
    return array;
}

struct RasterReport
{
    QJsonArray pages;
    bool dimensionsMatch = true;
    bool pixelMatch = true;
    int changedPixelTotal = 0;
};

QImage renderPage(QPdfDocument& document, int page, const QSize& size)
{
    const QImage rendered = document.render(page, size);
    return rendered.convertToFormat(QImage::Format_RGBA8888);
}

bool writeRaster(const QImage& image, const QString& path, QString* error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        *error = QStringLiteral("Could not create raster directory for %1")
            .arg(path);
        return false;
    }
    if (!image.save(path, "PNG"))
    {
        *error = QStringLiteral("Could not save raster artifact: %1")
            .arg(path);
        return false;
    }
    return true;
}

RasterReport compareAllPages(
    QPdfDocument& legacy,
    QPdfDocument& next,
    const QString& documentKey,
    const QString& outputRoot,
    QString* error
    )
{
    RasterReport report;
    const int legacyPages = legacy.pageCount();
    const int nextPages = next.pageCount();
    const int commonPages = std::min(legacyPages, nextPages);

    for (int page = 0; page < legacyPages; ++page)
    {
        const QSizeF points = legacy.pagePointSize(page);
        const QSize pixels(
            qRound(points.width() * 150.0 / 72.0),
            qRound(points.height() * 150.0 / 72.0));
        const QImage image = renderPage(legacy, page, pixels);
        if (image.isNull()
            || !writeRaster(
                image,
                QDir(outputRoot).filePath(
                    QStringLiteral("legacy-raster/%1/page-%2.png")
                        .arg(documentKey)
                        .arg(page + 1, 3, 10, QLatin1Char('0'))),
                error))
        {
            if (error->isEmpty())
            {
                *error = QStringLiteral("Legacy page %1 could not be rendered")
                    .arg(page + 1);
            }
            return report;
        }
    }
    for (int page = 0; page < nextPages; ++page)
    {
        const QSizeF points = next.pagePointSize(page);
        const QSize pixels(
            qRound(points.width() * 150.0 / 72.0),
            qRound(points.height() * 150.0 / 72.0));
        const QImage image = renderPage(next, page, pixels);
        if (image.isNull()
            || !writeRaster(
                image,
                QDir(outputRoot).filePath(
                    QStringLiteral("next-raster/%1/page-%2.png")
                        .arg(documentKey)
                        .arg(page + 1, 3, 10, QLatin1Char('0'))),
                error))
        {
            if (error->isEmpty())
            {
                *error = QStringLiteral("Next page %1 could not be rendered")
                    .arg(page + 1);
            }
            return report;
        }
    }

    if (legacyPages != nextPages)
    {
        report.dimensionsMatch = false;
        report.pixelMatch = false;
    }

    for (int page = 0; page < commonPages; ++page)
    {
        const QSizeF legacyPoints = legacy.pagePointSize(page);
        const QSizeF nextPoints = next.pagePointSize(page);
        const QSize legacyPixels(
            qRound(legacyPoints.width() * 150.0 / 72.0),
            qRound(legacyPoints.height() * 150.0 / 72.0));
        const QSize nextPixels(
            qRound(nextPoints.width() * 150.0 / 72.0),
            qRound(nextPoints.height() * 150.0 / 72.0));
        const QImage legacyImage = renderPage(legacy, page, legacyPixels);
        const QImage nextImage = renderPage(next, page, nextPixels);
        const bool pointDimensionsMatch = legacyPoints == nextPoints;
        const bool pixelDimensionsMatch = legacyPixels == nextPixels
            && legacyImage.size() == nextImage.size();
        report.dimensionsMatch = report.dimensionsMatch
            && pointDimensionsMatch && pixelDimensionsMatch;

        int changedPixels = 0;
        QImage difference;
        if (pixelDimensionsMatch)
        {
            difference = QImage(
                legacyPixels, QImage::Format_RGBA8888);
            difference.fill(Qt::transparent);
            for (int y = 0; y < legacyPixels.height(); ++y)
            {
                auto* diffLine = difference.scanLine(y);
                for (int x = 0; x < legacyPixels.width(); ++x)
                {
                    const QRgb legacyPixel = legacyImage.pixel(x, y);
                    const QRgb nextPixel = nextImage.pixel(x, y);
                    if (legacyPixel != nextPixel)
                    {
                        ++changedPixels;
                        diffLine[x * 4] = static_cast<uchar>(qRed(nextPixel));
                        diffLine[x * 4 + 1] = static_cast<uchar>(qGreen(nextPixel));
                        diffLine[x * 4 + 2] = static_cast<uchar>(qBlue(nextPixel));
                        diffLine[x * 4 + 3] = 255;
                    }
                }
            }
            if (!writeRaster(
                difference,
                QDir(outputRoot).filePath(
                    QStringLiteral("diff/%1/page-%2.png")
                        .arg(documentKey)
                        .arg(page + 1, 3, 10, QLatin1Char('0'))),
                error))
            {
                return report;
            }
        }
        else
        {
            report.pixelMatch = false;
            changedPixels = -1;
        }

        report.changedPixelTotal += qMax(0, changedPixels);
        report.pixelMatch = report.pixelMatch && changedPixels == 0;
        report.pages.append(QJsonObject{
            {QStringLiteral("page"), page + 1},
            {QStringLiteral("legacyPagePoints"), QJsonArray{
                legacyPoints.width(), legacyPoints.height()}},
            {QStringLiteral("nextPagePoints"), QJsonArray{
                nextPoints.width(), nextPoints.height()}},
            {QStringLiteral("legacyRasterPixels"), QJsonArray{
                legacyPixels.width(), legacyPixels.height()}},
            {QStringLiteral("nextRasterPixels"), QJsonArray{
                nextPixels.width(), nextPixels.height()}},
            {QStringLiteral("pointDimensionsMatch"), pointDimensionsMatch},
            {QStringLiteral("pixelDimensionsMatch"), pixelDimensionsMatch},
            {QStringLiteral("changedPixels"), changedPixels},
            {QStringLiteral("legacyRasterSha256"), imageSha256(legacyImage)},
            {QStringLiteral("nextRasterSha256"), imageSha256(nextImage)},
            {QStringLiteral("differenceRaster"), QStringLiteral(
                "diff/%1/page-%2.png").arg(documentKey)
                    .arg(page + 1, 3, 10, QLatin1Char('0'))}
        });
    }
    return report;
}

bool makeNextPackage(
    const QString& databasePath,
    const QJsonObject& oracle,
    const QString& outputRoot,
    QString* nextSubPrepPdf,
    QString* nextRosterPdf,
    QJsonObject* output,
    QString* error
    )
{
    const QString beforeGenerationHash = sha256File(databasePath);
    const QString expectedHash = oracle.value(
        QStringLiteral("databaseSha256BeforeLegacyGeneration")).toString();
    if (beforeGenerationHash.size() != 64
        || beforeGenerationHash.compare(expectedHash, Qt::CaseInsensitive) != 0)
    {
        *error = QStringLiteral(
            "SQL snapshot SHA-256 differs from the hash recorded before Legacy generation");
        return false;
    }

    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.openWorkspace({
        .location = Application::WorkspaceLocation(
            QFileInfo(databasePath).absoluteFilePath().toUtf8().toStdString())
    });
    if (!opened)
    {
        *error = operationError(opened.error());
        return false;
    }

    const QStringList allDays{
        QStringLiteral("Monday"), QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"), QStringLiteral("Thursday"),
        QStringLiteral("Friday"), QStringLiteral("Saturday"),
        QStringLiteral("Sunday")
    };
    QStringList selectedDays;
    for (const char* day : Weekdays)
    {
        selectedDays.append(QString::fromLatin1(day));
    }
    const auto scheduleRead = gateway.readSubPrepSchedulePage();
    if (!scheduleRead)
    {
        *error = operationError(scheduleRead.error());
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }

    const auto allSchedule = ScheduleBuilder{}.build(
        scheduleRead.value().source, false, allDays);
    const auto allScheduleView = buildScheduleViewModel(
        allSchedule,
        ScheduleViewRequest{
            .days = allDays,
            .displayMode = Next::Application::ScheduleDisplayMode::Regular
        });
    const QList<int> legacyClassIds = SubPrepPackageService::classIdsForDays(
        allScheduleView, selectedDays);
    if (legacyClassIds.isEmpty())
    {
        *error = QStringLiteral("The SQL weekday selection produced no classes");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    QList<int> expectedClassIds;
    for (const QJsonValue& value : oracle.value(
             QStringLiteral("selection")).toObject()
             .value(QStringLiteral("selectedClassIds")).toArray())
    {
        expectedClassIds.append(value.toInt());
    }
    QList<int> actualClassIds = legacyClassIds;
    std::sort(expectedClassIds.begin(), expectedClassIds.end());
    std::sort(actualClassIds.begin(), actualClassIds.end());
    if (expectedClassIds.isEmpty() || actualClassIds != expectedClassIds)
    {
        *error = QStringLiteral(
            "Next weekday class selection differs from the independent SQL oracle");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }

    std::vector<Next::Domain::ClassId> classIds;
    classIds.reserve(static_cast<std::size_t>(legacyClassIds.size()));
    for (const int id : legacyClassIds)
    {
        const auto typed = Next::Domain::ClassId::fromString(
            std::to_string(id));
        if (!typed)
        {
            *error = QStringLiteral("The SQL route produced an invalid class id");
            static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
            return false;
        }
        classIds.push_back(*typed);
    }

    std::vector<Next::Application::SubPrepWeekday> selectedWeekdays{
        Next::Application::SubPrepWeekday::Monday,
        Next::Application::SubPrepWeekday::Tuesday,
        Next::Application::SubPrepWeekday::Wednesday,
        Next::Application::SubPrepWeekday::Thursday,
        Next::Application::SubPrepWeekday::Friday
    };
    const Next::Application::SubPrepPrintSourceRequest printRequest{
        .selectedClassIds = classIds,
        .selectedDays = selectedWeekdays,
        .mode = Next::Application::ScheduleViewMode::Regular
    };
    const auto printSource = gateway.readSubPrepPrintSource(printRequest);
    if (!printSource)
    {
        *error = operationError(printSource.error());
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    const auto classInformation = SubPrepPrintSourceMapper::toClassInformation(
        printSource.value(), printRequest);
    if (!classInformation)
    {
        *error = operationError(classInformation.error());
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }

    const auto selectedSchedule = ScheduleBuilder{}.build(
        scheduleRead.value().source, false, selectedDays);
    SubPrepPrintService::Request subPrep;
    subPrep.classInformation = classInformation.value();
    subPrep.schedule = buildScheduleViewModel(
        selectedSchedule,
        ScheduleViewRequest{
            .days = selectedDays,
            .displayMode = Next::Application::ScheduleDisplayMode::Regular
        });

    const auto storedPreferences = gateway.readSubPrepPreferences();
    if (!storedPreferences)
    {
        *error = operationError(storedPreferences.error());
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    const auto& preferences = storedPreferences.value();
    subPrep.classMaterials = fromUtf8(preferences.classMaterials);
    subPrep.subNotes = fromUtf8(preferences.subComments);
    if (preferences.bookReportGrading)
    {
        subPrep.gradingInstructions = fromUtf8(*preferences.bookReportGrading);
        subPrep.specialInstructions = preferences.bookReportSpecialInstructions
            ? fromUtf8(*preferences.bookReportSpecialInstructions)
            : QString();
    }
    else
    {
        // Match the Legacy page's loadStoredSettings default when no grading
        // value is persisted in the prepared workspace.
        subPrep.gradingInstructions = QCoreApplication::translate(
            "SubPrepPage",
            "Scoring: 0 / 20 / 40 / 60 / 80 / 100\n"
            "Comments: Please leave a comment about what the student did well "
            "and what they need to work on.");
        subPrep.specialInstructions = preferences.bookReportSpecialInstructions
            ? fromUtf8(*preferences.bookReportSpecialInstructions)
            : QCoreApplication::translate("SubPrepPage", "N/A");
    }

    const auto personalZoom = gateway.readSubPrepPersonalZoomPreferences();
    if (!personalZoom)
    {
        *error = operationError(personalZoom.error());
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    if (personalZoom.value().unavailable)
    {
        subPrep.zoom.loginId = QCoreApplication::translate("SubPrepPage", "N/A");
        subPrep.zoom.password = QCoreApplication::translate("SubPrepPage", "N/A");
    }
    else
    {
        subPrep.zoom.loginId = valueOrNotAvailable(fromUtf8(
            personalZoom.value().loginId.value()));
        subPrep.zoom.password = valueOrNotAvailable(fromUtf8(
            personalZoom.value().password.value()));
    }

    const auto currentCampus = gateway.readSubPrepCurrentCampus();
    if (!currentCampus)
    {
        *error = operationError(currentCampus.error());
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    // Use the same baseline campus resource directory as the Legacy and Next
    // pages; their current-campus and fallback selection rules are mirrored.
    const auto campuses = Next::Platform::SubPrepCampusDirectoryQueryAdapter(
        QStringLiteral(CLASSMNGR_SOURCE_DIR "/resources/assets/campuses"))
        .loadCampuses();
    const QString savedCampus = fromUtf8(currentCampus.value().value());
    const Next::Application::SubPrepCampusMetadata* selectedCampus = nullptr;
    for (const auto& campus : campuses)
    {
        const QString campusId = fromUtf8(campus.id.value());
        const QString displayName = fromUtf8(campus.displayName);
        if (campusId.compare(savedCampus.trimmed(), Qt::CaseInsensitive) == 0
            || displayName.compare(savedCampus.trimmed(), Qt::CaseInsensitive) == 0)
        {
            selectedCampus = &campus;
            break;
        }
    }
    if (!selectedCampus && !campuses.empty())
    {
        selectedCampus = &campuses.front();
    }
    if (selectedCampus)
    {
        subPrep.campus.officeNumber = valueOrNotAvailable(fromUtf8(
            selectedCampus->officeNumber));
        subPrep.campus.officeWifi = valueOrNotAvailable(fromUtf8(
            selectedCampus->wifiName));
        subPrep.campus.officeWifiPassword = valueOrNotAvailable(fromUtf8(
            selectedCampus->wifiPassword));
        subPrep.campus.photocopierCode = valueOrNotAvailable(fromUtf8(
            selectedCampus->photocopierCode));
    }

    auto rosterSource = gateway.createSubPrepRosterOutputSourceReadPort();
    if (!rosterSource)
    {
        *error = QStringLiteral("Next roster output source is unavailable");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }

    const QList<QDate> selectedDates{
        QDate(2026, 9, 14), QDate(2026, 9, 15), QDate(2026, 9, 16),
        QDate(2026, 9, 17), QDate(2026, 9, 18)
    };
    SubPrepPackageService::Request request;
    request.rosterOutputSourceReadPort = rosterSource.get();
    request.subPrep = std::move(subPrep);
    request.selectedDates = selectedDates;
    request.selectedClassIds = std::move(classIds);
    request.useIntensiveSchedule = false;
    request.createFolder = true;
    request.targetRoot = QDir(outputRoot).filePath(QStringLiteral("next-output"));
    request.userName = QStringLiteral("Teacher 01");
    request.printPaperCopies = false;
    request.openFolderAfterGeneration = false;
    request.rosterTemplate = RosterTemplateRenderer::TemplateId::ByDay;

    if (!QDir().mkpath(request.targetRoot))
    {
        *error = QStringLiteral("Could not create Next package output root");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    const QString expectedFolder = QDir(request.targetRoot).filePath(
        SubPrepPackageService::datedFolderName(request.userName, selectedDates));
    if (QFileInfo::exists(expectedFolder))
    {
        *error = QStringLiteral("Next package output folder already exists");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }

    const QString immediatelyBeforeNextGenerationHash = sha256File(databasePath);
    if (immediatelyBeforeNextGenerationHash.compare(
            expectedHash, Qt::CaseInsensitive) != 0)
    {
        *error = QStringLiteral(
            "Opening the SQL snapshot changed its bytes before Next generation");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }

    // This standalone capture has no ApplicationBootstrap, so register the
    // same bundled font files through Qt before composing Next output fonts.
    const QString interFontPath = QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/resources/assets/fonts/Inter.ttc");
    const QString pretendardFontPath = QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/resources/assets/fonts/PretendardVariable.ttf");
    const int interFontRegistration =
        QFontDatabase::addApplicationFont(interFontPath);
    const int pretendardFontRegistration =
        QFontDatabase::addApplicationFont(pretendardFontPath);
    if (interFontRegistration < 0 || pretendardFontRegistration < 0)
    {
        *error = QStringLiteral("Could not register the bundled Sub Prep fonts.");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    const QStringList registeredFamilies =
        QFontDatabase::applicationFontFamilies(interFontRegistration)
        + QFontDatabase::applicationFontFamilies(pretendardFontRegistration);
    Platform::QtFontFamilyAvailabilityAdapter fontPresentation;
    const auto outputFonts = Ui::makeSubPrepRosterOutputFonts(
        registeredFamilies,
        QString::fromStdU16String(
            fontPresentation.outputFallbackFontFamily()),
        fontPresentation.applicationBaseFontPointSize());
    const auto pdfGeneration = makeQtPdfGenerationPort();
    const auto generated = SubPrepPackageService::generateForNext(
        *pdfGeneration, std::move(request), outputFonts);
    if (generated.status != SubPrepPackageService::Status::Completed
        || generated.documentPaths.size() != 2)
    {
        *error = generated.message.isEmpty()
            ? QStringLiteral("Next SQL package did not produce two PDFs")
            : generated.message;
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    *nextSubPrepPdf = generated.documentPaths.first();
    *nextRosterPdf = generated.documentPaths.last();
    if (QFileInfo(*nextRosterPdf).fileName()
            != QStringLiteral("Rosters - By Day.pdf"))
    {
        *error = QStringLiteral("Next output did not use By Day roster template");
        static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
        return false;
    }
    const auto closed = gateway.closeWorkspace({.session = opened.value()});
    if (!closed)
    {
        *error = operationError(closed.error());
        return false;
    }
    const QString afterNextGenerationHash = sha256File(databasePath);
    if (afterNextGenerationHash.compare(expectedHash, Qt::CaseInsensitive) != 0)
    {
        *error = QStringLiteral(
            "Next generation changed the retained prepared SQL snapshot");
        return false;
    }

    const QFont& uiFont = outputFonts.ui;
    const QFont& koreanFont = outputFonts.korean;
    *output = QJsonObject{
        {QStringLiteral("status"), QStringLiteral("generated")},
        {QStringLiteral("entryPoint"), QStringLiteral(
            "SubPrepPackageService::generateForNext")},
        {QStringLiteral("databaseSha256BeforeGeneration"),
            immediatelyBeforeNextGenerationHash},
        {QStringLiteral("databaseSha256AfterGeneration"),
            afterNextGenerationHash},
        {QStringLiteral("databaseSha256MatchesPreLegacyGeneration"), true},
        {QStringLiteral("databaseUnchangedByNextGeneration"), true},
        {QStringLiteral("packageDirectory"), expectedFolder},
        {QStringLiteral("documents"), stringArray(QJsonArray::fromStringList(
            {QFileInfo(generated.documentPaths.at(0)).fileName(),
             QFileInfo(generated.documentPaths.at(1)).fileName()}))},
        {QStringLiteral("selection"), QJsonObject{
            {QStringLiteral("classIds"), QJsonArray::fromVariantList(
                [&legacyClassIds] {
                    QVariantList values;
                    for (const int id : legacyClassIds) values.append(id);
                    return values;
                }())},
            {QStringLiteral("weekdays"), QJsonArray::fromStringList(selectedDays)},
            {QStringLiteral("startDate"), QStringLiteral("2026-09-14")},
            {QStringLiteral("endDate"), QStringLiteral("2026-09-18")},
            {QStringLiteral("locale"), QStringLiteral("en_US")},
            {QStringLiteral("rosterTemplate"), QStringLiteral("By Day")},
            {QStringLiteral("mode"), QStringLiteral("Regular")}
        }},
        {QStringLiteral("fontResources"), QJsonObject{
            {QStringLiteral("uiFamily"), uiFont.family()},
            {QStringLiteral("uiStyle"), uiFont.styleName()},
            {QStringLiteral("koreanFamily"), koreanFont.family()},
            {QStringLiteral("koreanStyle"), koreanFont.styleName()},
            {QStringLiteral("uiFontSha256"), sha256File(
                QStringLiteral(CLASSMNGR_SOURCE_DIR "/resources/assets/fonts/Inter.ttc"))},
            {QStringLiteral("koreanFontSha256"), sha256File(
                QStringLiteral(CLASSMNGR_SOURCE_DIR "/resources/assets/fonts/PretendardVariable.ttf"))},
            {QStringLiteral("loading"), QStringLiteral(
                "Next explicit output font policy with registered resource families")}
        }}
    };
    return true;
}

bool auditNextText(
    QPdfDocument& next,
    const QJsonObject& oracle,
    const QString& outputRoot,
    QJsonObject* report,
    QString* extractedText,
    QString* error
    )
{
    for (int page = 0; page < next.pageCount(); ++page)
    {
        *extractedText += next.getAllText(page).text();
        *extractedText += QLatin1Char('\n');
    }

    const QJsonArray expectedNames = oracle.value(
        QStringLiteral("expectedStudentNames")).toArray();
    const QJsonArray expectedHeaderText = oracle.value(
        QStringLiteral("expectedHeaderText")).toArray();
    const QJsonArray expectedHeaderRecords = oracle.value(
        QStringLiteral("expectedHeaderRecords")).toArray();
    QJsonArray nameResults;
    bool namesMatch = expectedNames.size() == oracle.value(
        QStringLiteral("expectedStudentCount")).toInt()
        && expectedNames.size() == 750;
    QSet<QString> uniqueExpectedNames;
    for (const QJsonValue& value : expectedNames)
    {
        const QString expected = value.toString();
        const int count = exactOccurrences(*extractedText, expected);
        const bool sourceNameValid = !expected.isEmpty()
            && !uniqueExpectedNames.contains(expected);
        uniqueExpectedNames.insert(expected);
        namesMatch = namesMatch && sourceNameValid && count == 1;
        nameResults.append(QJsonObject{
            {QStringLiteral("expected"), expected},
            {QStringLiteral("exactOccurrences"), count},
            {QStringLiteral("matches"), count == 1}
        });
    }

    QJsonArray headerResults;
    bool headersMatch = expectedHeaderRecords.size() == oracle.value(
        QStringLiteral("expectedHeaderRecordCount")).toInt()
        && expectedHeaderRecords.size() == 30
        && !expectedHeaderText.isEmpty();
    for (const QJsonValue& value : expectedHeaderText)
    {
        const QJsonObject expectation = value.toObject();
        const QString expected = expectation.value(
            QStringLiteral("value")).toString();
        const int expectedOccurrences = expectation.value(
            QStringLiteral("expectedOccurrences")).toInt(-1);
        const int count = exactOccurrences(*extractedText, expected);
        const bool matches = !expected.isEmpty()
            && expectedOccurrences >= 0 && count == expectedOccurrences;
        headersMatch = headersMatch && matches;
        headerResults.append(QJsonObject{
            {QStringLiteral("field"), expectation.value(
                QStringLiteral("field"))},
            {QStringLiteral("expected"), expected},
            {QStringLiteral("expectedOccurrences"), expectedOccurrences},
            {QStringLiteral("exactOccurrences"), count},
            {QStringLiteral("matches"), matches}
        });
    }
    const int bulletCount = extractedText->count(QChar(0x2022));
    const int expectedBulletCount = oracle.value(
        QStringLiteral("expectedBulletCount")).toInt();
    const bool bulletExpectationValid = expectedBulletCount == 0;
    bool forbiddenControl = false;
    QStringList controls;
    for (int index = 0; index < extractedText->size(); ++index)
    {
        const QChar character = extractedText->at(index);
        if ((character.unicode() < 32 && character != QChar('\n')
                && character != QChar('\r'))
            || (character.unicode() >= 0x7f && character.unicode() <= 0x9f))
        {
            forbiddenControl = true;
            controls.append(QStringLiteral("U+%1").arg(
                static_cast<int>(character.unicode()),
                4, 16, QLatin1Char('0')).toUpper());
        }
    }
    controls.removeDuplicates();

    *report = QJsonObject{
        {QStringLiteral("oracleSchema"), oracle.value(QStringLiteral("schema"))},
        {QStringLiteral("databaseSha256"), oracle.value(
            QStringLiteral("databaseSha256BeforeLegacyGeneration"))},
        {QStringLiteral("expectedStudentCount"), expectedNames.size()},
        {QStringLiteral("foundStudentCount"), namesMatch ? expectedNames.size() : -1},
        {QStringLiteral("studentsMatchExactlyOnce"), namesMatch},
        {QStringLiteral("students"), nameResults},
        {QStringLiteral("expectedHeaderRecordCount"), expectedHeaderRecords.size()},
        {QStringLiteral("foundHeaderRecordCount"), headersMatch
            ? expectedHeaderRecords.size() : -1},
        {QStringLiteral("headerTextMatchesExpectedOccurrences"), headersMatch},
        {QStringLiteral("headerText"), headerResults},
        {QStringLiteral("expectedBulletCount"), expectedBulletCount},
        {QStringLiteral("foundBulletCount"), bulletCount},
        {QStringLiteral("bulletExpectationValid"), bulletExpectationValid},
        {QStringLiteral("bulletCountMatches"), bulletCount == expectedBulletCount},
        {QStringLiteral("forbiddenControlsFound"), forbiddenControl},
        {QStringLiteral("forbiddenControlCodePoints"),
            QJsonArray::fromStringList(controls)},
        {QStringLiteral("pdfTextNormalization"), false},
        {QStringLiteral("pdfToPdfTextComparison"), false}
    };
    if (!saveText(
            QDir(outputRoot).filePath(QStringLiteral("next-extracted-text.txt")),
            *extractedText, error))
    {
        return false;
    }
    const bool success = namesMatch && headersMatch
        && bulletExpectationValid && bulletCount == expectedBulletCount
        && !forbiddenControl;
    if (!success)
    {
        *error = QStringLiteral("Next PDF text did not match the SQL source oracle");
    }
    return success;
}

int runMatchedSql(const QStringList& arguments)
{
    if (arguments.size() != 7 || arguments.at(1) != QStringLiteral("--matched-sql"))
    {
        qCritical().noquote()
            << "Usage: ClassMngrNextSubPrepSqlParityCapture --matched-sql"
               " PREPARED_TPS LEGACY_ROSTER_PDF LEGACY_CAPTURE_JSON"
               " SQL_ORACLE_JSON OUTPUT_DIR";
        return 2;
    }
    const QString databasePath = arguments.at(2);
    const QString legacyPdfPath = arguments.at(3);
    const QString legacyManifestPath = arguments.at(4);
    const QString oraclePath = arguments.at(5);
    const QString outputRoot = arguments.at(6);
    if (!QFileInfo::exists(databasePath) || !QFileInfo::exists(legacyPdfPath)
        || !QFileInfo::exists(legacyManifestPath)
        || !QFileInfo::exists(oraclePath))
    {
        qCritical("Matched SQL capture input is missing");
        return 2;
    }
    if (!QDir().mkpath(outputRoot)
        || !QDir(outputRoot).entryList(
            QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
    {
        qCritical("Matched SQL capture output directory must be empty");
        return 2;
    }

    QFile oracleFile(oraclePath);
    if (!oracleFile.open(QIODevice::ReadOnly))
    {
        qCritical().noquote() << oracleFile.errorString();
        return 2;
    }
    QJsonParseError parseError;
    const QJsonDocument oracleDocument = QJsonDocument::fromJson(
        oracleFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !oracleDocument.isObject()
        || oracleDocument.object().value(QStringLiteral("schema")).toString()
            != QStringLiteral("classmngr-sub-prep-c53-sql-oracle-v2"))
    {
        qCritical("SQL source oracle is malformed or has an unsupported schema");
        return 2;
    }
    QFile legacyManifestFile(legacyManifestPath);
    if (!legacyManifestFile.open(QIODevice::ReadOnly))
    {
        qCritical().noquote() << legacyManifestFile.errorString();
        return 2;
    }
    QJsonParseError legacyManifestParseError;
    const QJsonDocument legacyManifestDocument = QJsonDocument::fromJson(
        legacyManifestFile.readAll(), &legacyManifestParseError);
    if (legacyManifestParseError.error != QJsonParseError::NoError
        || !legacyManifestDocument.isObject()
        || legacyManifestDocument.object().value(QStringLiteral("schema")).toString()
            != QStringLiteral("classmngr-sub-prep-c53-legacy-sql-capture-v1"))
    {
        qCritical("Legacy SQL capture manifest is malformed or has an unsupported schema");
        return 2;
    }
    const QJsonObject legacyManifest = legacyManifestDocument.object();
    const QJsonObject oracle = oracleDocument.object();
    const QJsonObject legacyWorkspaceAudit = oracle.value(
        QStringLiteral("legacyWorkspaceAfterGeneration")).toObject();
    const QSet<QString> expectedSourceTables{
        QStringLiteral("classes"), QStringLiteral("class_info"),
        QStringLiteral("teachers"), QStringLiteral("class_times"),
        QStringLiteral("roster_columns"), QStringLiteral("roster_data")
    };
    QSet<QString> auditedSourceTables;
    for (const QJsonValue& value : legacyWorkspaceAudit.value(
             QStringLiteral("tablesChecked")).toArray())
    {
        auditedSourceTables.insert(value.toString());
    }
    if (!legacyWorkspaceAudit.value(QStringLiteral(
            "rosterSourceTablesUnchanged")).toBool()
        || auditedSourceTables != expectedSourceTables
        || oracle.value(QStringLiteral("expectedStudentCount")).toInt() != 750
        || oracle.value(QStringLiteral("expectedHeaderRecordCount")).toInt() != 30
        || oracle.value(QStringLiteral("expectedBulletCount")).toInt() != 0)
    {
        qCritical("SQL source oracle does not prove the expected roster inputs");
        return 2;
    }
    const QJsonObject selection = oracle.value(
        QStringLiteral("selection")).toObject();
    if (selection.value(QStringLiteral("startDate")).toString()
            != QStringLiteral("2026-09-14")
        || selection.value(QStringLiteral("endDate")).toString()
            != QStringLiteral("2026-09-18")
        || selection.value(QStringLiteral("locale")).toString()
            != QStringLiteral("en_US")
        || selection.value(QStringLiteral("rosterTemplate")).toString()
            != QStringLiteral("By Day"))
    {
        qCritical("SQL source oracle selection does not match the C53 gate");
        return 2;
    }
    const QJsonObject legacySelection = legacyManifest.value(
        QStringLiteral("selection")).toObject();
    const QJsonObject legacyPreparedDatabase = legacyManifest.value(
        QStringLiteral("preparedDatabase")).toObject();
    const QJsonObject legacySourceFixture = legacyManifest.value(
        QStringLiteral("sourceFixture")).toObject();
    const QString legacyAfterWorkspacePath = QDir(
        QFileInfo(legacyManifestPath).absolutePath()).filePath(
            legacyPreparedDatabase.value(QStringLiteral(
                "workspaceAfterGenerationPath")).toString());
    const QString oracleAfterWorkspacePath = legacyWorkspaceAudit.value(
        QStringLiteral("databasePath")).toString();
    if (legacyPreparedDatabase.value(
            QStringLiteral("sha256BeforeLegacyGeneration")).toString()
            .compare(oracle.value(QStringLiteral(
                "databaseSha256BeforeLegacyGeneration")).toString(),
                Qt::CaseInsensitive) != 0
        || legacySourceFixture.value(QStringLiteral("sha256")).toString()
            .compare(oracle.value(QStringLiteral("sourceFixtureSha256")).toString(),
                Qt::CaseInsensitive) != 0
        || legacyPreparedDatabase.value(QStringLiteral(
                "sha256AfterLegacyGeneration")).toString()
            .compare(legacyWorkspaceAudit.value(QStringLiteral(
                "databaseSha256")).toString(), Qt::CaseInsensitive) != 0
        || QFileInfo(oracleAfterWorkspacePath).fileName()
            != QFileInfo(legacyAfterWorkspacePath).fileName()
        || sha256File(legacyAfterWorkspacePath).compare(
            legacyWorkspaceAudit.value(QStringLiteral(
                "databaseSha256")).toString(), Qt::CaseInsensitive) != 0
        || legacySelection.value(QStringLiteral("teacherFolderName")).toString()
            != selection.value(QStringLiteral("teacherFolderName")).toString()
        || legacySelection.value(QStringLiteral("startDate")).toString()
            != selection.value(QStringLiteral("startDate")).toString()
        || legacySelection.value(QStringLiteral("endDate")).toString()
            != selection.value(QStringLiteral("endDate")).toString()
        || legacySelection.value(QStringLiteral("locale")).toString()
            != selection.value(QStringLiteral("locale")).toString()
        || legacySelection.value(QStringLiteral("rosterTemplate")).toString()
            != selection.value(QStringLiteral("rosterTemplate")).toString()
        || legacySelection.value(QStringLiteral("mode")).toString()
            != selection.value(QStringLiteral("mode")).toString()
        || legacySelection.value(QStringLiteral("weekdays")).toArray()
            != selection.value(QStringLiteral("weekdays")).toArray())
    {
        qCritical("Legacy capture and SQL oracle do not describe the same snapshot and selection");
        return 2;
    }

    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    QString error;
    QString nextSubPrepPdf;
    QString nextRosterPdf;
    QJsonObject nextGeneration;
    if (!makeNextPackage(
            databasePath, oracle, outputRoot,
            &nextSubPrepPdf, &nextRosterPdf, &nextGeneration, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    const QString legacyDirectory = QFileInfo(legacyPdfPath).absolutePath();
    const QString legacySubPrepPdf = QDir(legacyDirectory).filePath(
        QStringLiteral("Sub Prep.pdf"));
    if (QFileInfo(legacySubPrepPdf).fileName()
            != QStringLiteral("Sub Prep.pdf"))
    {
        qCritical("Legacy Sub Prep PDF path is invalid");
        return 1;
    }

    QPdfDocument legacySubPrep;
    QPdfDocument nextSubPrep;
    QPdfDocument legacyRoster;
    QPdfDocument nextRoster;
    QString legacySubPrepText;
    QString nextSubPrepText;
    QString legacyRosterText;
    QString nextRosterText;
    if (!loadPdfText(legacySubPrepPdf, &legacySubPrep,
            &legacySubPrepText, &error)
        || !loadPdfText(nextSubPrepPdf, &nextSubPrep,
            &nextSubPrepText, &error)
        || !loadPdfText(legacyPdfPath, &legacyRoster,
            &legacyRosterText, &error)
        || !loadPdfText(nextRosterPdf, &nextRoster,
            &nextRosterText, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    if (!saveText(
            QDir(outputRoot).filePath(
                QStringLiteral("legacy-roster-extracted-text.txt")),
            legacyRosterText, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    QJsonObject sqlTextAudit;
    QString independentlyExtractedNextText;
    const bool textPassed = auditNextText(
        nextRoster, oracle, outputRoot, &sqlTextAudit,
        &independentlyExtractedNextText, &error);
    if (!saveJson(
            QDir(outputRoot).filePath(QStringLiteral("sql-text-audit.json")),
            sqlTextAudit, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    const RasterReport subPrepRaster = compareAllPages(
        legacySubPrep, nextSubPrep, QStringLiteral("sub-prep"),
        outputRoot, &error);
    const RasterReport rosterRaster = compareAllPages(
        legacyRoster, nextRoster, QStringLiteral("roster"),
        outputRoot, &error);
    if (!error.isEmpty())
    {
        qCritical().noquote() << error;
        return 1;
    }

    const QString sourceRoot = QStringLiteral(CLASSMNGR_SOURCE_DIR);
    const QString fontInter = sourceRoot
        + QStringLiteral("/resources/assets/fonts/Inter.ttc");
    const QString fontPretendard = sourceRoot
        + QStringLiteral("/resources/assets/fonts/PretendardVariable.ttf");
    const bool pageCountsMatch =
        legacySubPrep.pageCount() == nextSubPrep.pageCount()
        && legacyRoster.pageCount() == nextRoster.pageCount();
    const bool pageDimensionsMatch = subPrepRaster.dimensionsMatch
        && rosterRaster.dimensionsMatch;
    const bool allPagesPixelIdentical = subPrepRaster.pixelMatch
        && rosterRaster.pixelMatch;
    const int changedPixelTotal = subPrepRaster.changedPixelTotal
        + rosterRaster.changedPixelTotal;
    const QJsonObject legacyFonts = legacyManifest.value(
        QStringLiteral("fonts")).toObject();
    const QJsonObject nextFonts = nextGeneration.value(
        QStringLiteral("fontResources")).toObject();
    const bool sameFontResourceProvenance =
        !legacyFonts.value(QStringLiteral("InterSha256")).toString().isEmpty()
        && legacyFonts.value(QStringLiteral("InterSha256")).toString()
            .compare(nextFonts.value(QStringLiteral("uiFontSha256")).toString(),
                Qt::CaseInsensitive) == 0
        && !legacyFonts.value(QStringLiteral("PretendardSha256")).toString().isEmpty()
        && legacyFonts.value(QStringLiteral("PretendardSha256")).toString()
            .compare(nextFonts.value(QStringLiteral("koreanFontSha256")).toString(),
                Qt::CaseInsensitive) == 0;
    const bool legacyPdfHashMatchesManifest = [&] {
        for (const QJsonValue& value : legacyManifest.value(
                 QStringLiteral("pdfs")).toArray())
        {
            const QJsonObject pdf = value.toObject();
            if (pdf.value(QStringLiteral("name")).toString()
                    == QStringLiteral("Rosters - By Day.pdf"))
            {
                return pdf.value(QStringLiteral("sha256")).toString()
                    .compare(sha256File(legacyPdfPath), Qt::CaseInsensitive) == 0;
            }
        }
        return false;
    }();
    const QJsonArray documents{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Sub Prep.pdf")},
            {QStringLiteral("legacyPath"), QDir(outputRoot).relativeFilePath(
                legacySubPrepPdf)},
            {QStringLiteral("legacySha256"), sha256File(legacySubPrepPdf)},
            {QStringLiteral("legacyPageCount"), legacySubPrep.pageCount()},
            {QStringLiteral("nextPath"), QDir(outputRoot).relativeFilePath(
                nextSubPrepPdf)},
            {QStringLiteral("nextSha256"), sha256File(nextSubPrepPdf)},
            {QStringLiteral("nextPageCount"), nextSubPrep.pageCount()},
            {QStringLiteral("pageCountsMatch"),
                legacySubPrep.pageCount() == nextSubPrep.pageCount()},
            {QStringLiteral("pageDimensionsMatch"),
                subPrepRaster.dimensionsMatch},
            {QStringLiteral("allPagesPixelIdentical"),
                subPrepRaster.pixelMatch},
            {QStringLiteral("changedPixels"),
                subPrepRaster.changedPixelTotal},
            {QStringLiteral("pages"), subPrepRaster.pages}
        },
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Rosters - By Day.pdf")},
            {QStringLiteral("legacyPath"), QDir(outputRoot).relativeFilePath(
                legacyPdfPath)},
            {QStringLiteral("legacySha256"), sha256File(legacyPdfPath)},
            {QStringLiteral("legacyPageCount"), legacyRoster.pageCount()},
            {QStringLiteral("nextPath"), QDir(outputRoot).relativeFilePath(
                nextRosterPdf)},
            {QStringLiteral("nextSha256"), sha256File(nextRosterPdf)},
            {QStringLiteral("nextPageCount"), nextRoster.pageCount()},
            {QStringLiteral("pageCountsMatch"),
                legacyRoster.pageCount() == nextRoster.pageCount()},
            {QStringLiteral("pageDimensionsMatch"),
                rosterRaster.dimensionsMatch},
            {QStringLiteral("allPagesPixelIdentical"),
                rosterRaster.pixelMatch},
            {QStringLiteral("changedPixels"),
                rosterRaster.changedPixelTotal},
            {QStringLiteral("pages"), rosterRaster.pages}
        }
    };
    const QJsonObject report{
        {QStringLiteral("taskId"), QStringLiteral(
            "phase7_c53_subprep_finalizer_implementation_02")},
        {QStringLiteral("schema"), QStringLiteral(
            "classmngr-sub-prep-c53-matched-sql-parity-v3")},
        {QStringLiteral("result"), textPassed && pageCountsMatch
                && pageDimensionsMatch && allPagesPixelIdentical
                && sameFontResourceProvenance && legacyPdfHashMatchesManifest
            ? QStringLiteral("pass") : QStringLiteral("fail")},
        {QStringLiteral("selection"), selection},
        {QStringLiteral("legacyCaptureManifest"), QFileInfo(
            legacyManifestPath).fileName()},
        {QStringLiteral("legacyPdfHashMatchesManifest"),
            legacyPdfHashMatchesManifest},
        {QStringLiteral("sameFontResourceProvenance"),
            sameFontResourceProvenance},
        {QStringLiteral("database"), QJsonObject{
            {QStringLiteral("path"), QFileInfo(databasePath).fileName()},
            {QStringLiteral("sha256BeforeLegacyGeneration"), oracle.value(
                QStringLiteral("databaseSha256BeforeLegacyGeneration"))},
            {QStringLiteral("sha256BeforeNextGeneration"),
                nextGeneration.value(QStringLiteral("databaseSha256BeforeGeneration"))},
            {QStringLiteral("sha256AfterNextGeneration"),
                nextGeneration.value(QStringLiteral("databaseSha256AfterGeneration"))},
            {QStringLiteral("legacyWorkspaceAfterGeneration"),
                oracle.value(QStringLiteral("legacyWorkspaceAfterGeneration"))},
            {QStringLiteral("samePreparedBytes"), nextGeneration.value(
                QStringLiteral("databaseSha256MatchesPreLegacyGeneration"))},
            {QStringLiteral("unchangedByNextGeneration"), nextGeneration.value(
                QStringLiteral("databaseUnchangedByNextGeneration"))}
        }},
        {QStringLiteral("nextEntryPoint"), nextGeneration.value(
            QStringLiteral("entryPoint"))},
        {QStringLiteral("documents"), documents},
        {QStringLiteral("renderer"), QJsonObject{
            {QStringLiteral("engine"), QStringLiteral("Qt QPdfDocument")},
            {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
            {QStringLiteral("os"), QSysInfo::prettyProductName()},
            {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()},
            {QStringLiteral("dpi"), 150},
            {QStringLiteral("strictChangedPixelTolerance"), 0}
        }},
        {QStringLiteral("fontAndResourceProvenance"), QJsonObject{
            {QStringLiteral("interFile"), QStringLiteral(
                "resources/assets/fonts/Inter.ttc")},
            {QStringLiteral("interSha256"), sha256File(fontInter)},
            {QStringLiteral("pretendardFile"), QStringLiteral(
                "resources/assets/fonts/PretendardVariable.ttf")},
            {QStringLiteral("pretendardSha256"), sha256File(fontPretendard)},
            {QStringLiteral("fontEmbedding"), QStringLiteral(
                "same ClassMngrSubPrepShared embedded qrc resources in both runtime paths")},
            {QStringLiteral("sameFontResourceProvenance"),
                sameFontResourceProvenance},
            {QStringLiteral("legacyCapture"), legacyFonts},
            {QStringLiteral("nextGeneration"), nextGeneration.value(
                QStringLiteral("fontResources"))}
        }},
        {QStringLiteral("sqlTextAuditPassed"), textPassed},
        {QStringLiteral("pageCountsMatch"), pageCountsMatch},
        {QStringLiteral("pageDimensionsMatch"), pageDimensionsMatch},
        {QStringLiteral("allPagesPixelIdentical"), allPagesPixelIdentical},
        {QStringLiteral("changedPixelTotal"), changedPixelTotal}
    };
    if (!saveJson(
            QDir(outputRoot).filePath(QStringLiteral("pdf-comparison.json")),
            report, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }
    if (!textPassed || !pageCountsMatch || !pageDimensionsMatch
        || !allPagesPixelIdentical || !sameFontResourceProvenance
        || !legacyPdfHashMatchesManifest)
    {
        qCritical("Matched SQL C53 comparison failed; evidence was retained");
        return 1;
    }
    qInfo().noquote()
        << "Matched SQL C53 comparison passed:"
        << subPrepRaster.pages.size() + rosterRaster.pages.size()
        << "pages across both PDFs,"
        << changedPixelTotal << "changed pixels";
    return 0;
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    return runMatchedSql(QCoreApplication::arguments());
}
