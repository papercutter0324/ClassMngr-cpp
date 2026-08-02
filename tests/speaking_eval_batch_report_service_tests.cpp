#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"

#include <QtTest>

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPdfDocument>
#include <QPainter>
#include <QPdfSelection>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace
{
SpeakingEvalReportData goldenReportData(
    SpeakingEvalReportTemplate reportTemplate
    )
{
    SpeakingEvalReportData data;
    data.reportTemplate = reportTemplate;
    data.koreanName = QStringLiteral("홍길동");
    if (reportTemplate == SpeakingEvalReportTemplate::Advanced)
    {
        data.englishName = QStringLiteral("Athena Student");
        data.classLabel = QStringLiteral("E5 Athena");
        data.nativeTeacher = QStringLiteral("Teacher");
        data.koreanTeacher = QStringLiteral("선생님");
        data.date = QStringLiteral("Jul. 2026");
        data.comments = QStringLiteral("Advanced speaking evaluation.");
        data.scores = {
            QStringLiteral("A+"),
            QStringLiteral("A"),
            QStringLiteral("B+"),
            QStringLiteral("B"),
            QStringLiteral("A"),
            QStringLiteral("A+")
        };
        return data;
    }

    data.englishName = QStringLiteral("Gildong");
    data.classLabel = QStringLiteral("E6 Gaia");
    data.nativeTeacher = QStringLiteral("Aristotle");
    data.koreanTeacher = QStringLiteral("송오현");
    data.date = QStringLiteral("June 2025");
    data.comments = QStringLiteral("Comment text");
    data.scores = {
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C"),
        QStringLiteral("A+")
    };
    return data;
}

QPair<qreal, qreal> imageError(
    const QImage& actualImage,
    const QImage& expectedImage
    )
{
    const QImage actual =
        actualImage.convertToFormat(QImage::Format_ARGB32);
    const QImage expected =
        expectedImage.convertToFormat(QImage::Format_ARGB32);
    qsizetype changedPixels = 0;
    quint64 absoluteError = 0;

    for (int y = 0; y < actual.height(); ++y)
    {
        const QRgb* actualLine =
            reinterpret_cast<const QRgb*>(actual.constScanLine(y));
        const QRgb* expectedLine =
            reinterpret_cast<const QRgb*>(expected.constScanLine(y));
        for (int x = 0; x < actual.width(); ++x)
        {
            const int red =
                qAbs(qRed(actualLine[x]) - qRed(expectedLine[x]));
            const int green =
                qAbs(qGreen(actualLine[x]) - qGreen(expectedLine[x]));
            const int blue =
                qAbs(qBlue(actualLine[x]) - qBlue(expectedLine[x]));
            absoluteError += red + green + blue;
            if (red > 16 || green > 16 || blue > 16)
            {
                ++changedPixels;
            }
        }
    }

    const qreal pixels =
        actual.width() * actual.height();
    return {
        changedPixels / pixels,
        absoluteError / (pixels * 3.0)
    };
}
}

class SpeakingEvalBatchReportServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void safeFileNameUsesStudentNamesAndRemovesReservedCharacters();
    void defaultOutputDirectoryIncludesClassScheduleAndEvaluation();
    void internalRendererCreatesReadablePdf();
    void internalPdfMatchesWidgetRendering_data();
    void internalPdfMatchesWidgetRendering();
    void singleReportCanBeSavedToAnExactFilePath();
    void overwriteExistingReportWhenAllowed();
    void previewDialogOffersActionsForTheSelectedReport();
    void powerPointAvailabilityMessageIsAvailable();
    void generatedAssetsMatchSvgSourcesWhenEnabled();
    void powerPointRendererCreatesReadablePdfWhenAvailable();
};

void SpeakingEvalBatchReportServiceTests::safeFileNameUsesStudentNamesAndRemovesReservedCharacters()
{
    QCOMPARE(
        SpeakingEvalBatchReportService::safeFileName(
            QStringLiteral("Jane: Doe"),
            QStringLiteral("김/철수")
            ),
        QStringLiteral("Jane- Doe (김-철수).pdf")
        );
}

void SpeakingEvalBatchReportServiceTests::defaultOutputDirectoryIncludesClassScheduleAndEvaluation()
{
    ClassInfo classInfo;
    classInfo.classGrade = QStringLiteral("E5");
    classInfo.classLevel = QStringLiteral("Zeus");
    classInfo.classTimes = {
        { QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM") },
        { QStringLiteral("Wednesday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM") }
    };

    QCOMPARE(
        SpeakingEvalBatchReportService::defaultOutputDirectory(
            classInfo,
            QStringLiteral("Winter"),
            QStringLiteral("C:/Documents")
            ),
        QDir::cleanPath(
            QStringLiteral("C:/Documents/DYB/SpeakingEvals/E5 Zeus (MW - 4pm)/Winter")
            )
        );
}

void SpeakingEvalBatchReportServiceTests::internalRendererCreatesReadablePdf()
{
    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData data;
    data.englishName = QStringLiteral("Gildong");
    data.koreanName = QStringLiteral("\uD64D\uAE38\uB3D9");
    data.classLabel = QStringLiteral("E6 Gaia");
    data.nativeTeacher = QStringLiteral("Aristotle");
    data.koreanTeacher = QStringLiteral("\uC1A1\uC624\uD604");
    data.date = QStringLiteral("July 2026");
    data.comments = QStringLiteral("Prepared and confident.");
    data.scores = {
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C"),
        QStringLiteral("A+")
    };

    SpeakingEvalBatchReportService::Request request;
    SpeakingEvalReportData advancedData = data;
    advancedData.englishName = QStringLiteral("Athena Student");
    advancedData.classLabel = QStringLiteral("E5 Athena");
    advancedData.date = QStringLiteral("Jul. 2026");
    advancedData.reportTemplate =
        SpeakingEvalReportTemplate::Advanced;
    request.reports = {
        { QStringLiteral("Gildong (\uD64D\uAE38\uB3D9)"), data },
        { QStringLiteral("Athena Student"), advancedData }
    };
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);

    QVERIFY2(
        result.status == SpeakingEvalBatchReportService::Status::Completed,
        qPrintable(result.message)
        );
    QCOMPARE(result.savedPdfPaths.size(), 2);
    for (const QString& pdfPath : result.savedPdfPaths)
    {
        QVERIFY(QFileInfo::exists(pdfPath));

        QPdfDocument document;
        QCOMPARE(document.load(pdfPath), QPdfDocument::Error::None);
        QCOMPARE(document.status(), QPdfDocument::Status::Ready);
        QCOMPARE(document.pageCount(), 1);
    }
}

void SpeakingEvalBatchReportServiceTests::
    internalPdfMatchesWidgetRendering_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard")
        << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced")
        << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalBatchReportServiceTests::
    internalPdfMatchesWidgetRendering()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);

    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());
    const SpeakingEvalReportData data =
        goldenReportData(reportTemplate);

    SpeakingEvalBatchReportService::Request request;
    request.reports = {
        { data.englishName, data }
    };
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    QVERIFY2(
        result.status == SpeakingEvalBatchReportService::Status::Completed,
        qPrintable(result.message)
        );
    QCOMPARE(result.savedPdfPaths.size(), 1);

    QPdfDocument document;
    QCOMPARE(
        document.load(result.savedPdfPaths.constFirst()),
        QPdfDocument::Error::None
        );
    QCOMPARE(document.pageCount(), 1);

    const QSize pathComparisonSize(1620, 2340);
    const QImage pathPdfImage =
        document.render(0, pathComparisonSize);
    QVERIFY(!pathPdfImage.isNull());
    SpeakingEvalReportWidget widget;
    widget.setReportData(data);
    QImage widgetImage(
        pathComparisonSize,
        QImage::Format_ARGB32_Premultiplied
        );
    widgetImage.fill(Qt::white);
    QPainter painter(&widgetImage);
    widget.paintReport(
        &painter,
        QRectF(QPointF(), pathComparisonSize)
        );
    painter.end();
    const bool isAdvanced =
        reportTemplate == SpeakingEvalReportTemplate::Advanced;
    const QString previewPrefix =
        isAdvanced
            ? QStringLiteral("CLASSMNGR_ADVANCED")
            : QStringLiteral("CLASSMNGR_STANDARD");
    const QString pdfPreviewPath =
        qEnvironmentVariable(
            (previewPrefix + QStringLiteral("_PDF_RASTER_PATH"))
                .toUtf8()
                .constData()
            );
    const QString widgetPreviewPath =
        qEnvironmentVariable(
            (previewPrefix + QStringLiteral("_WIDGET_RASTER_PATH"))
                .toUtf8()
                .constData()
            );
    if (!pdfPreviewPath.isEmpty())
    {
        QVERIFY(pathPdfImage.save(pdfPreviewPath));
    }
    if (!widgetPreviewPath.isEmpty())
    {
        QVERIFY(widgetImage.save(widgetPreviewPath));
    }
    const auto [pathRatio, pathMeanError] =
        imageError(pathPdfImage, widgetImage);
    const qreal maximumPathChangedRatio =
        isAdvanced
            ? 0.07
            : 0.05;
    QVERIFY2(
        pathRatio <= maximumPathChangedRatio,
        qPrintable(
            QStringLiteral(
                "PDF/widget changed-pixel ratio was %1%."
                )
                .arg(pathRatio * 100.0, 0, 'f', 3)
            )
        );
    const qreal maximumPathMeanError =
        isAdvanced
            ? 3.5
            : 3.0;
    QVERIFY2(
        pathMeanError <= maximumPathMeanError,
        qPrintable(
            QStringLiteral(
                "PDF/widget mean absolute RGB error was %1."
                )
                .arg(pathMeanError, 0, 'f', 3)
            )
        );
}

void SpeakingEvalBatchReportServiceTests::overwriteExistingReportWhenAllowed()
{
    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData data;
    data.englishName = QStringLiteral("Existing Student");
    data.classLabel = QStringLiteral("E5 Zeus");
    data.date = QStringLiteral("July 2026");
    data.scores.fill(QStringLiteral("A"));

    const QString targetPath = QDir(outputDirectory.path()).filePath(
        SpeakingEvalBatchReportService::safeFileName(
            data.englishName,
            data.koreanName
            )
        );
    QFile existingFile(targetPath);
    QVERIFY(existingFile.open(QIODevice::WriteOnly));
    QCOMPARE(existingFile.write("old"), 3);
    existingFile.close();

    SpeakingEvalBatchReportService::Request request;
    request.reports = { { QStringLiteral("Existing Student"), data } };
    request.savePdf = true;
    request.overwriteExisting = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    QCOMPARE(result.status, SpeakingEvalBatchReportService::Status::Completed);

    QPdfDocument document;
    QCOMPARE(document.load(targetPath), QPdfDocument::Error::None);
    QCOMPARE(document.pageCount(), 1);
    QVERIFY(!QFileInfo::exists(targetPath + QStringLiteral(".classmngr-backup")));
}

void SpeakingEvalBatchReportServiceTests::singleReportCanBeSavedToAnExactFilePath()
{
    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData data;
    data.englishName = QStringLiteral("Selected Student");
    data.classLabel = QStringLiteral("E5 Zeus");
    data.date = QStringLiteral("July 2026");
    data.scores.fill(QStringLiteral("A"));

    const QString targetPath =
        QDir(outputDirectory.path()).filePath(
            QStringLiteral("Custom report name.pdf")
            );

    SpeakingEvalBatchReportService::Request request;
    request.reports = { { QStringLiteral("Selected Student"), data } };
    request.savePdf = true;
    request.outputFilePath = targetPath;

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);

    QVERIFY2(
        result.status == SpeakingEvalBatchReportService::Status::Completed,
        qPrintable(result.message)
        );
    QCOMPARE(result.savedPdfPaths, QStringList{ targetPath });

    QPdfDocument document;
    QCOMPARE(document.load(targetPath), QPdfDocument::Error::None);
    QCOMPARE(document.pageCount(), 1);
}

void SpeakingEvalBatchReportServiceTests::
    previewDialogOffersActionsForTheSelectedReport()
{
    SpeakingEvalReportData firstReport;
    firstReport.englishName = QStringLiteral("First Student");
    SpeakingEvalReportData secondReport;
    secondReport.englishName = QStringLiteral("Second Student");

    SpeakingEvalReportDialog dialog(
        {
            { QStringLiteral("First Student"), firstReport },
            { QStringLiteral("Second Student"), secondReport }
        },
        1
        );

    auto* studentSelector = dialog.findChild<QComboBox*>(
        QStringLiteral("speakingEvalReportStudentSelector")
        );
    auto* printButton = dialog.findChild<QPushButton*>(
        QStringLiteral("speakingEvalReportPrintButton")
        );
    auto* saveAsPdfButton = dialog.findChild<QPushButton*>(
        QStringLiteral("speakingEvalReportSaveAsPdfButton")
        );

    QVERIFY(studentSelector);
    QCOMPARE(studentSelector->currentIndex(), 1);
    QCOMPARE(studentSelector->currentText(), QStringLiteral("Second Student"));
    QVERIFY(printButton);
    QCOMPARE(printButton->text(), QStringLiteral("Print"));
    QVERIFY(printButton->isEnabled());
    QVERIFY(saveAsPdfButton);
    QCOMPARE(saveAsPdfButton->text(), QStringLiteral("Save As PDF"));
    QVERIFY(saveAsPdfButton->isEnabled());
}

void SpeakingEvalBatchReportServiceTests::powerPointAvailabilityMessageIsAvailable()
{
    QCOMPARE(
        SpeakingEvalBatchReportService::rendererDisplayName(
            SpeakingEvalBatchReportService::Renderer::Internal
            ),
        QStringLiteral("Internal Template (Beta)")
        );
    QVERIFY(
        !SpeakingEvalBatchReportService::powerPointRendererAvailabilityMessage()
             .trimmed()
             .isEmpty()
        );
}

void SpeakingEvalBatchReportServiceTests::
    generatedAssetsMatchSvgSourcesWhenEnabled()
{
    if (!qEnvironmentVariableIsSet(
            "CLASSMNGR_ENABLE_POWERPOINT_INTEGRATION_TESTS"
            ))
    {
        QSKIP(
            "Set CLASSMNGR_ENABLE_POWERPOINT_INTEGRATION_TESTS to run template asset checks."
            );
    }

    QString powerShell =
        QStandardPaths::findExecutable(
            QStringLiteral("pwsh")
            );
    if (powerShell.isEmpty())
    {
        powerShell =
            QStandardPaths::findExecutable(
                QStringLiteral("powershell")
                );
    }
    QVERIFY2(
        !powerShell.isEmpty(),
        "PowerShell is required for the speaking-evaluation asset check."
        );

    const QString sourceDirectory =
        QStringLiteral(CLASSMNGR_SOURCE_DIR);
    const QString scriptPath =
        QDir(sourceDirectory).filePath(
            QStringLiteral(
                "scripts/speaking_eval/generate_internal_template_assets.ps1"
                )
            );
    QVERIFY(QFileInfo::exists(scriptPath));

    QProcess process;
    process.setWorkingDirectory(sourceDirectory);
    process.start(
        powerShell,
        {
            QStringLiteral("-NoProfile"),
            QStringLiteral("-File"),
            scriptPath,
            QStringLiteral("--check")
        }
        );
    QVERIFY(process.waitForStarted());
    QVERIFY2(
        process.waitForFinished(120000),
        "The SVG template asset check timed out."
        );
    const QString output =
        QString::fromUtf8(process.readAllStandardOutput())
        + QString::fromUtf8(process.readAllStandardError());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        process.exitCode() == 0,
        qPrintable(output)
        );
    QVERIFY(
        output.contains(
            QStringLiteral(
                "Speaking-evaluation SVG template assets are current."
                )
            )
        );
}

void SpeakingEvalBatchReportServiceTests::powerPointRendererCreatesReadablePdfWhenAvailable()
{
    if (!qEnvironmentVariableIsSet(
            "CLASSMNGR_ENABLE_POWERPOINT_INTEGRATION_TESTS"
            ))
    {
        QSKIP("Set CLASSMNGR_ENABLE_POWERPOINT_INTEGRATION_TESTS to run Office automation tests.");
    }

    if (!SpeakingEvalBatchReportService::isPowerPointRendererAvailable())
    {
        QSKIP("PowerPoint automation is not available on this machine.");
    }

    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData data;
    data.englishName = QStringLiteral("PowerPoint Student");
    data.koreanName = QStringLiteral("\uD64D\uAE38\uB3D9");
    data.classLabel = QStringLiteral("E6 Gaia");
    data.nativeTeacher = QStringLiteral("Teacher");
    data.koreanTeacher = QStringLiteral("\uC120\uC0DD\uB2D8");
    data.date = QStringLiteral("July 2026");
    data.comments = QStringLiteral("PowerPoint integration test.");
    data.scores = {
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C"),
        QStringLiteral("A+")
    };

    SpeakingEvalBatchReportService::Request request;
    SpeakingEvalReportData secondStandardData = data;
    secondStandardData.englishName = QStringLiteral("Second PowerPoint Student");
    secondStandardData.koreanName = QStringLiteral("Second Student");
    secondStandardData.comments = QStringLiteral("Second report from the same open template.");
    secondStandardData.scores.fill(QStringLiteral("C"));
    SpeakingEvalReportData advancedData = data;
    advancedData.englishName = QStringLiteral("Advanced Student");
    advancedData.classLabel = QStringLiteral("E5 Athena");
    advancedData.date = QStringLiteral("Jul. 2026");
    advancedData.reportTemplate =
        SpeakingEvalReportTemplate::Advanced;
    request.reports = {
        { QStringLiteral("PowerPoint Student"), data },
        { QStringLiteral("Second PowerPoint Student"), secondStandardData },
        { QStringLiteral("Advanced Student"), advancedData }
    };
    request.renderer = SpeakingEvalBatchReportService::Renderer::PowerPoint;
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);

    QVERIFY2(
        result.status == SpeakingEvalBatchReportService::Status::Completed,
        qPrintable(result.message)
        );
    QCOMPARE(result.savedPdfPaths.size(), 3);

    for (int index = 0; index < result.savedPdfPaths.size(); ++index)
    {
        const QString& pdfPath = result.savedPdfPaths.at(index);
        QPdfDocument document;
        QCOMPARE(document.load(pdfPath), QPdfDocument::Error::None);
        QCOMPARE(document.status(), QPdfDocument::Status::Ready);
        QVERIFY(document.pageCount() >= 1);
        QVERIFY2(
            document.getAllText(0).text().contains(
                request.reports.at(index).report.englishName
                ),
            qPrintable(
                QStringLiteral("The PDF did not contain the current student's name: %1")
                    .arg(request.reports.at(index).report.englishName)
                )
            );
    }
}

QTEST_MAIN(SpeakingEvalBatchReportServiceTests)

#include "speaking_eval_batch_report_service_tests.moc"
