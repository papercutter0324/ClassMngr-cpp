#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_delegate.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_notes_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"

#include <QtTest>

#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPdfDocument>
#include <QPainter>
#include <QPdfSelection>
#include <QProcess>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUndoStack>

#include <functional>

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

QByteArray powerPointTestSignature()
{
    QImage signature(
        QSize(80, 20),
        QImage::Format_ARGB32_Premultiplied
        );
    signature.fill(QColor(255, 0, 255));

    QByteArray data;
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly)
        || !signature.save(&buffer, "PNG"))
    {
        return {};
    }
    return data;
}

int matchingPixelCount(
    const QImage& image,
    const QRect& bounds,
    const std::function<bool(QRgb)>& matches
    )
{
    int count = 0;
    const QRect clipped =
        bounds.intersected(image.rect());
    for (int y = clipped.top(); y <= clipped.bottom(); ++y)
    {
        const QRgb* line =
            reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = clipped.left(); x <= clipped.right(); ++x)
        {
            if (matches(line[x]))
            {
                ++count;
            }
        }
    }
    return count;
}

#ifdef Q_OS_MACOS
QString classMngrPowerPointWorkspace()
{
    return QDir(
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
            )
        ).filePath(QStringLiteral("PowerPointBatch"));
}

QString powerPointPrivateWorkspace()
{
    return QDir(
        QStandardPaths::writableLocation(
            QStandardPaths::HomeLocation
            )
        ).filePath(
            QStringLiteral(
                "Library/Containers/com.microsoft.Powerpoint/Data/Documents/ClassMngr/PowerPointBatch"
                )
            );
}

bool createStaleWorkspaceFile(
    const QString& directoryPath,
    const QString& fileName
    )
{
    QDir directory(directoryPath);
    if ((!directory.exists() && !QDir().mkpath(directoryPath)))
    {
        return false;
    }

    QFile staleFile(directory.filePath(fileName));
    return staleFile.open(QIODevice::WriteOnly)
        && staleFile.write("stale") == 5;
}
#endif
}

class SpeakingEvalBatchReportServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void safeFileNameUsesStudentNamesAndRemovesReservedCharacters();
    void defaultOutputDirectoryIncludesClassScheduleAndEvaluation();
    void reportDateUsesShortMonthsForStandardTemplate();
    void internalRendererCreatesReadablePdf();
    void internalPdfMatchesWidgetRendering_data();
    void internalPdfMatchesWidgetRendering();
    void singleReportCanBeSavedToAnExactFilePath();
    void overwriteExistingReportWhenAllowed();
    void previewDialogOffersActionsForTheSelectedReport();
    void selectedSourceRowMapsToItsFilteredReport();
    void previewDialogOnlyScrollsTheReport();
    void privateNotesAreSplitAndSaved();
    void privateNotesAutomaticallyContinueBullets();
    void notesDialogShowsNotesBesideEachOtherAndCommentBelow();
    void notesDialogPreservesUntouchedValuesAndFocusesClickedSection();
    void notesDialogEnforcesCommentLimitAndUpdatesCounter();
    void notesDialogClearsOnlyCommentAfterConfirmation();
    void commentsAndNotesCellsUseTheCombinedDialog();
    void powerPointAvailabilityMessageIsAvailable();
    void mixedPowerPointTemplatesAreRejected();
    void generatedAssetsMatchSvgSourcesWhenEnabled();
    void powerPointRendererCreatesReadablePdfWhenAvailable_data();
    void powerPointRendererCreatesReadablePdfWhenAvailable();
    void powerPointBatchCancellationLeavesNoFilesWhenAvailable();
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

void SpeakingEvalBatchReportServiceTests::
    reportDateUsesShortMonthsForStandardTemplate()
{
    const QStringList expected{
        QStringLiteral("Jan. 2026"),
        QStringLiteral("Feb. 2026"),
        QStringLiteral("Mar. 2026"),
        QStringLiteral("Apr. 2026"),
        QStringLiteral("May 2026"),
        QStringLiteral("June 2026"),
        QStringLiteral("July 2026"),
        QStringLiteral("Aug. 2026"),
        QStringLiteral("Sep. 2026"),
        QStringLiteral("Oct. 2026"),
        QStringLiteral("Nov. 2026"),
        QStringLiteral("Dec. 2026")
    };

    for (int month = 1; month <= 12; ++month)
    {
        QCOMPARE(
            speakingEvalReportDate(
                QDate(2026, month, 1),
                SpeakingEvalReportTemplate::Standard
                ),
            expected.at(month - 1)
            );
    }

    QCOMPARE(
        speakingEvalReportDate(
            QDate(2026, 7, 1),
            SpeakingEvalReportTemplate::Advanced
            ),
        QStringLiteral("Jul. 2026")
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

void SpeakingEvalBatchReportServiceTests::
    selectedSourceRowMapsToItsFilteredReport()
{
    SpeakingEvalRows rows(4);
    rows[0].resize(SpeakingEval::ColumnCount);
    rows[1].resize(SpeakingEval::ColumnCount);
    rows[2].resize(SpeakingEval::ColumnCount);
    rows[3].resize(SpeakingEval::ColumnCount);
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("First Student");
    rows[2][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Selected Student");
    rows[3][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("마지막 학생");

    const auto reports =
        buildSpeakingEvalStudentReports(rows, {});

    QCOMPARE(reports.size(), 3);
    QCOMPARE(speakingEvalReportIndexForSourceRow(reports, 2), 1);
    QCOMPARE(speakingEvalReportIndexForSourceRow(reports, 1), -1);
}

void SpeakingEvalBatchReportServiceTests::
    previewDialogOnlyScrollsTheReport()
{
    SpeakingEvalReportData reportData;
    reportData.englishName = QStringLiteral("Student");

    SpeakingEvalReportDialog dialog(
        { { QStringLiteral("Student"), reportData } },
        0,
        nullptr,
        true
        );

    auto* scrollArea = dialog.findChild<QScrollArea*>();
    auto* report = dialog.findChild<SpeakingEvalReportWidget*>();
    auto* studentSelector = dialog.findChild<QComboBox*>(
        QStringLiteral("speakingEvalReportStudentSelector")
        );

    QVERIFY(scrollArea);
    QVERIFY(report);
    QCOMPARE(scrollArea->widget(), report);
    QVERIFY(studentSelector);
    QVERIFY(!scrollArea->isAncestorOf(studentSelector));
    const auto notesEdits = dialog.findChildren<QPlainTextEdit*>(
        QRegularExpression(
            QStringLiteral("speakingEval(DidWell|NeedsImprovement)Notes")
            )
        );
    QCOMPARE(notesEdits.size(), 2);
    for (QPlainTextEdit* notesEdit : notesEdits)
    {
        QVERIFY(!scrollArea->isAncestorOf(notesEdit));
        QCOMPARE(notesEdit->height(), 144);
    }
}

void SpeakingEvalBatchReportServiceTests::
    privateNotesAreSplitAndSaved()
{
    SpeakingEvalReportData reportData;
    reportData.englishName = QStringLiteral("Student");
    reportData.notes =
        QStringLiteral(
            "[Did Well]\nClear pronunciation\n"
            "[Needs Improvement]\nUse longer answers"
            );

    SpeakingEvalReportDialog dialog(
        { { QStringLiteral("Student"), reportData, 4 } },
        0,
        nullptr,
        true
        );

    auto* notesFields = dialog.findChild<QWidget*>(
        QStringLiteral("speakingEvalPrivateNotesFields")
        );
    auto* didWellEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalDidWellNotes")
        );
    auto* needsImprovementEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalNeedsImprovementNotes")
        );

    QVERIFY(notesFields);
    QVERIFY(qobject_cast<QHBoxLayout*>(notesFields->layout()));
    QVERIFY(didWellEdit);
    QCOMPARE(
        didWellEdit->toPlainText(),
        QStringLiteral("• Clear pronunciation")
        );
    QVERIFY(needsImprovementEdit);
    QCOMPARE(
        needsImprovementEdit->toPlainText(),
        QStringLiteral("• Use longer answers")
        );

    int editedRow = -1;
    SpeakingEvalColumn editedColumn = SpeakingEvalColumn::Index;
    QString editedValue;
    connect(
        &dialog,
        &SpeakingEvalReportDialog::reportValueEdited,
        &dialog,
        [&](int row, SpeakingEvalColumn column, const QString& value)
        {
            editedRow = row;
            editedColumn = column;
            editedValue = value;
        }
        );

    needsImprovementEdit->setPlainText(
        QStringLiteral("Use complete sentences")
        );

    QCOMPARE(editedRow, 4);
    QCOMPARE(editedColumn, SpeakingEvalColumn::Notes);
    QCOMPARE(
        editedValue,
        QStringLiteral(
            "[Did Well]\n• Clear pronunciation\n"
            "[Needs Improvement]\nUse complete sentences"
            )
        );
}

void SpeakingEvalBatchReportServiceTests::
    privateNotesAutomaticallyContinueBullets()
{
    SpeakingEvalReportData reportData;
    reportData.englishName = QStringLiteral("Student");

    SpeakingEvalReportDialog dialog(
        { { QStringLiteral("Student"), reportData } },
        0,
        nullptr,
        true
        );

    auto* didWellEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalDidWellNotes")
        );
    QVERIFY(didWellEdit);

    didWellEdit->setFocus();
    QTest::keyClicks(
        didWellEdit,
        QStringLiteral("Strong voice")
        );
    QTest::keyClick(didWellEdit, Qt::Key_Return);
    QTest::keyClicks(
        didWellEdit,
        QStringLiteral("Clear pronunciation")
        );

    QCOMPARE(
        didWellEdit->toPlainText(),
        QStringLiteral(
            "• Strong voice\n"
            "• Clear pronunciation"
            )
        );
}

void SpeakingEvalBatchReportServiceTests::
    notesDialogShowsNotesBesideEachOtherAndCommentBelow()
{
    const QString originalNotes =
        QStringLiteral(
            "[Did Well]\nStrong voice\n"
            "[Needs Improvement]\nUse complete sentences"
            );
    SpeakingEvalNotesDialog dialog(
        originalNotes,
        QStringLiteral("The report comment.")
        );

    auto* notesFields = dialog.findChild<QWidget*>(
        QStringLiteral("speakingEvalPrivateNotesFields")
        );
    auto* didWellEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalDidWellNotes")
        );
    auto* needsImprovementEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalNeedsImprovementNotes")
        );
    auto* commentEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalNotesDialogComment")
        );

    QVERIFY(notesFields);
    QVERIFY(qobject_cast<QHBoxLayout*>(notesFields->layout()));
    QVERIFY(didWellEdit);
    QVERIFY(needsImprovementEdit);
    QCOMPARE(didWellEdit->height(), 144);
    QCOMPARE(needsImprovementEdit->height(), 144);
    QCOMPARE(
        didWellEdit->toPlainText(),
        QStringLiteral("• Strong voice")
        );
    QCOMPARE(
        needsImprovementEdit->toPlainText(),
        QStringLiteral("• Use complete sentences")
        );
    QVERIFY(commentEdit);
    QCOMPARE(
        commentEdit->toPlainText(),
        QStringLiteral("The report comment.")
        );
    QVERIFY(!dialog.hasNotesChanges());
    QVERIFY(!dialog.hasCommentChanges());
    QCOMPARE(dialog.notes(), originalNotes);

    dialog.show();
    QApplication::processEvents();
    QCOMPARE(QApplication::focusWidget(), didWellEdit);
    QVERIFY(commentEdit->width() > didWellEdit->width());
    QVERIFY(commentEdit->y() > notesFields->y());

    commentEdit->setPlainText(
        QStringLiteral("Updated report comment.")
        );
    QCOMPARE(
        dialog.comment(),
        QStringLiteral("Updated report comment.")
        );
    QVERIFY(dialog.hasCommentChanges());
    QVERIFY(!dialog.hasNotesChanges());
    QCOMPARE(dialog.notes(), originalNotes);
}

void SpeakingEvalBatchReportServiceTests::
    notesDialogPreservesUntouchedValuesAndFocusesClickedSection()
{
    const QString legacyNotes =
        QStringLiteral("Legacy unstructured notes");
    const QString originalComment =
        QStringLiteral("Original comment");
    SpeakingEvalNotesDialog dialog(
        legacyNotes,
        originalComment,
        SpeakingEvalNotesDialog::InitialSection::Comment
        );

    auto* commentEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalNotesDialogComment")
        );
    auto* didWellEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalDidWellNotes")
        );
    QVERIFY(commentEdit);
    QVERIFY(didWellEdit);
    QCOMPARE(
        didWellEdit->toPlainText(),
        QStringLiteral("• Legacy unstructured notes")
        );

    dialog.show();
    QApplication::processEvents();
    QCOMPARE(QApplication::focusWidget(), commentEdit);

    QCOMPARE(dialog.notes(), legacyNotes);
    QCOMPARE(dialog.comment(), originalComment);
    QVERIFY(!dialog.hasNotesChanges());
    QVERIFY(!dialog.hasCommentChanges());

    QTest::keyClicks(
        commentEdit,
        QStringLiteral(" updated")
        );
    QVERIFY(dialog.hasCommentChanges());
    QVERIFY(!dialog.hasNotesChanges());
    QCOMPARE(dialog.notes(), legacyNotes);
}

void SpeakingEvalBatchReportServiceTests::
    notesDialogEnforcesCommentLimitAndUpdatesCounter()
{
    SpeakingEvalNotesDialog dialog(
        {},
        {},
        SpeakingEvalNotesDialog::InitialSection::Comment
        );
    auto* commentEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalNotesDialogComment")
        );
    auto* counter = dialog.findChild<QLabel*>(
        QStringLiteral("speakingEvalNotesDialogCommentCounter")
        );
    QVERIFY(commentEdit);
    QVERIFY(counter);

    commentEdit->setPlainText(
        QString(
            SpeakingEval::CommentMaxLength * 9 / 10,
            QLatin1Char('a')
            )
        );
    QCOMPARE(
        commentEdit->toPlainText().size(),
        SpeakingEval::CommentMaxLength * 9 / 10
        );
    QCOMPARE(
        counter->text(),
        QStringLiteral("Characters: 405/450")
        );
    QVERIFY(counter->styleSheet().contains(QStringLiteral("orange")));

    commentEdit->setPlainText(
        QString(
            SpeakingEval::CommentMaxLength + 50,
            QLatin1Char('b')
            )
        );
    QCOMPARE(
        commentEdit->toPlainText().size(),
        SpeakingEval::CommentMaxLength
        );
    QCOMPARE(
        counter->text(),
        QStringLiteral("Characters: 450/450")
        );
    QVERIFY(counter->styleSheet().contains(QStringLiteral("red")));

    commentEdit->moveCursor(QTextCursor::End);
    QTest::keyClicks(commentEdit, QStringLiteral("blocked"));
    QCOMPARE(
        commentEdit->toPlainText().size(),
        SpeakingEval::CommentMaxLength
        );

    QTextCursor cursor =
        commentEdit->textCursor();
    cursor.setPosition(SpeakingEval::CommentMaxLength - 10);
    cursor.setPosition(
        SpeakingEval::CommentMaxLength,
        QTextCursor::KeepAnchor
        );
    commentEdit->setTextCursor(cursor);
    QApplication::clipboard()->setText(
        QString(20, QLatin1Char('c'))
        );
    QTest::keyClick(
        commentEdit,
        Qt::Key_V,
        Qt::ControlModifier
        );
    QCOMPARE(
        commentEdit->toPlainText().size(),
        SpeakingEval::CommentMaxLength
        );
    QVERIFY(dialog.hasCommentChanges());
    QVERIFY(!dialog.hasNotesChanges());
}

void SpeakingEvalBatchReportServiceTests::
    notesDialogClearsOnlyCommentAfterConfirmation()
{
    const QString originalNotes =
        QStringLiteral(
            "[Did Well]\n• Strong voice\n"
            "[Needs Improvement]\n• Add detail"
            );
    SpeakingEvalNotesDialog dialog(
        originalNotes,
        QStringLiteral("Clear this comment")
        );
    auto* commentEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalNotesDialogComment")
        );
    auto* clearButton = dialog.findChild<QPushButton*>(
        QStringLiteral("speakingEvalNotesDialogClearComment")
        );
    QVERIFY(commentEdit);
    QVERIFY(clearButton);
    QVERIFY(clearButton->isEnabled());

    dialog.show();
    QApplication::processEvents();
    QTimer::singleShot(
        50,
        []()
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                if (
                    auto* messageBox =
                        qobject_cast<QMessageBox*>(widget)
                    )
                {
                    messageBox->done(QMessageBox::Yes);
                    return;
                }
            }
        }
        );
    QTest::mouseClick(
        clearButton,
        Qt::LeftButton
        );

    QVERIFY(commentEdit->toPlainText().isEmpty());
    QVERIFY(!clearButton->isEnabled());
    QVERIFY(dialog.hasCommentChanges());
    QVERIFY(!dialog.hasNotesChanges());
    QCOMPARE(dialog.notes(), originalNotes);
}

void SpeakingEvalBatchReportServiceTests::
    commentsAndNotesCellsUseTheCombinedDialog()
{
    SpeakingEvalModel model;
    const QModelIndex notesIndex =
        model.index(
            0,
            SpeakingEval::toInt(SpeakingEvalColumn::Notes)
            );
    const QModelIndex commentsIndex =
        model.index(
            0,
            SpeakingEval::toInt(SpeakingEvalColumn::Comments)
            );
    const QString originalNotes =
        QStringLiteral("Legacy notes");
    const QString originalComment =
        QStringLiteral("Original comment");
    QVERIFY(model.setData(notesIndex, originalNotes, Qt::EditRole));
    QVERIFY(model.setData(commentsIndex, originalComment, Qt::EditRole));

    SpeakingEvalTableView table;
    QUndoStack undoStack;
    table.setModel(&model);
    table.setUndoStack(&undoStack);
    SpeakingEvalDelegate delegate(&table);
    QStyleOptionViewItem option;

    const auto clickCell =
        [&](
            const QModelIndex& index,
            const QString& expectedFocusObject,
            const std::function<void(SpeakingEvalNotesDialog*)>& edit
            )
        {
            bool openedCombinedDialog = false;
            bool focusedExpectedField = false;
            QTimer::singleShot(
                50,
                [&]()
                {
                    for (QWidget* widget : QApplication::topLevelWidgets())
                    {
                        auto* dialog =
                            qobject_cast<SpeakingEvalNotesDialog*>(widget);
                        if (!dialog)
                        {
                            continue;
                        }

                        openedCombinedDialog = true;
                        QWidget* expectedFocus =
                            dialog->findChild<QWidget*>(
                                expectedFocusObject
                                );
                        focusedExpectedField =
                            expectedFocus
                            && (
                                dialog->focusWidget()
                                    == expectedFocus
                                || expectedFocus->isAncestorOf(
                                    dialog->focusWidget()
                                    )
                                );
                        edit(dialog);
                        dialog->accept();
                        return;
                    }
                }
                );

            QMouseEvent releaseEvent(
                QEvent::MouseButtonRelease,
                QPointF(2, 2),
                QPointF(2, 2),
                QPointF(2, 2),
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier
                );
            QVERIFY(
                delegate.editorEvent(
                    &releaseEvent,
                    &model,
                    option,
                    index
                    )
                );
            QVERIFY(openedCombinedDialog);
            QVERIFY(focusedExpectedField);
        };

    clickCell(
        commentsIndex,
        QStringLiteral("speakingEvalNotesDialogComment"),
        [](SpeakingEvalNotesDialog* dialog)
        {
            auto* editor = dialog->findChild<QPlainTextEdit*>(
                QStringLiteral("speakingEvalNotesDialogComment")
                );
            QVERIFY(editor);
            editor->setPlainText(
                QStringLiteral("Updated comment")
                );
        }
        );
    QCOMPARE(
        commentsIndex.data(Qt::EditRole).toString(),
        QStringLiteral("Updated comment")
        );
    QCOMPARE(
        notesIndex.data(Qt::EditRole).toString(),
        originalNotes
        );

    clickCell(
        notesIndex,
        QStringLiteral("speakingEvalDidWellNotes"),
        [](SpeakingEvalNotesDialog* dialog)
        {
            auto* editor = dialog->findChild<QPlainTextEdit*>(
                QStringLiteral("speakingEvalDidWellNotes")
                );
            QVERIFY(editor);
            editor->setPlainText(
                QStringLiteral("• New strength")
                );
        }
        );
    QCOMPARE(
        notesIndex.data(Qt::EditRole).toString(),
        QStringLiteral(
            "[Did Well]\n• New strength\n"
            "[Needs Improvement]\n"
            )
        );
    QCOMPARE(
        commentsIndex.data(Qt::EditRole).toString(),
        QStringLiteral("Updated comment")
        );
    QCOMPARE(undoStack.count(), 2);

    undoStack.undo();
    QCOMPARE(
        notesIndex.data(Qt::EditRole).toString(),
        originalNotes
        );
    QCOMPARE(
        commentsIndex.data(Qt::EditRole).toString(),
        QStringLiteral("Updated comment")
        );
    undoStack.undo();
    QCOMPARE(
        commentsIndex.data(Qt::EditRole).toString(),
        originalComment
        );
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

void SpeakingEvalBatchReportServiceTests::mixedPowerPointTemplatesAreRejected()
{
    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData standard =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    SpeakingEvalReportData advanced =
        goldenReportData(SpeakingEvalReportTemplate::Advanced);

    SpeakingEvalBatchReportService::Request request;
    request.reports = {
        { standard.englishName, standard },
        { advanced.englishName, advanced }
    };
    request.renderer =
        SpeakingEvalBatchReportService::Renderer::PowerPoint;
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);

    QCOMPARE(
        result.status,
        SpeakingEvalBatchReportService::Status::Failed
        );
    QVERIFY(
        result.message.contains(
            QStringLiteral("same template"),
            Qt::CaseInsensitive
            )
        );
    QCOMPARE(
        QDir(outputDirectory.path()).entryList(
            { QStringLiteral("*.pdf") },
            QDir::Files
            ).size(),
        0
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

void SpeakingEvalBatchReportServiceTests::
    powerPointRendererCreatesReadablePdfWhenAvailable_data()
{
    QTest::addColumn<int>("reportTemplateValue");
    QTest::addColumn<bool>("includeSignature");

    QTest::newRow("standard")
        << static_cast<int>(SpeakingEvalReportTemplate::Standard)
        << true;
    QTest::newRow("advanced")
        << static_cast<int>(SpeakingEvalReportTemplate::Advanced)
        << false;
    QTest::newRow("advanced-signature")
        << static_cast<int>(SpeakingEvalReportTemplate::Advanced)
        << true;
}

void SpeakingEvalBatchReportServiceTests::
    powerPointRendererCreatesReadablePdfWhenAvailable()
{
    QFETCH(int, reportTemplateValue);
    QFETCH(bool, includeSignature);

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

#ifdef Q_OS_MACOS
    QVERIFY(
        createStaleWorkspaceFile(
            classMngrPowerPointWorkspace(),
            QStringLiteral("cancel-requested")
            )
        );
    QVERIFY(
        createStaleWorkspaceFile(
            powerPointPrivateWorkspace(),
            QStringLiteral("stale-file")
            )
        );
#endif

    const SpeakingEvalReportTemplate reportTemplate =
        static_cast<SpeakingEvalReportTemplate>(
            reportTemplateValue
            );
    const QString firstComposedKoreanName =
        QStringLiteral("박지혜");
    const QString secondComposedKoreanName =
        QStringLiteral("김철수");
    const QString composedKoreanTeacher =
        QStringLiteral("선생님");

    SpeakingEvalReportData first;
    first.englishName = QStringLiteral("First Student");
    first.koreanName =
        firstComposedKoreanName.normalized(
            QString::NormalizationForm_D
            );
    first.classLabel =
        reportTemplate == SpeakingEvalReportTemplate::Advanced
            ? QStringLiteral("E5 Athena")
            : QStringLiteral("E6 Gaia");
    first.nativeTeacher = QStringLiteral("Teacher");
    first.koreanTeacher =
        composedKoreanTeacher.normalized(
            QString::NormalizationForm_D
            );
    first.date =
        reportTemplate == SpeakingEvalReportTemplate::Advanced
            ? QStringLiteral("Jul. 2026")
            : QStringLiteral("July 2026");
    first.comments = QStringLiteral("First batch report comments.");
    first.scores.fill(QStringLiteral("A+"));
    first.reportTemplate = reportTemplate;
    if (includeSignature)
    {
        first.signatureImage = powerPointTestSignature();
        QVERIFY(!first.signatureImage.isEmpty());
    }

    SpeakingEvalReportData second = first;
    second.englishName = QStringLiteral("Second Student");
    second.koreanName =
        secondComposedKoreanName.normalized(
            QString::NormalizationForm_D
            );
    second.comments = QStringLiteral("Second batch report comments.");
    second.scores.fill(QStringLiteral("C"));

    SpeakingEvalBatchReportService::Request request;
    request.reports = {
        { first.englishName, first },
        { second.englishName, second }
    };
    request.renderer =
        SpeakingEvalBatchReportService::Renderer::PowerPoint;
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);

    QVERIFY2(
        result.status == SpeakingEvalBatchReportService::Status::Completed,
        qPrintable(result.message)
        );
    QCOMPARE(result.savedPdfPaths.size(), 2);

    QList<QImage> renderedPages;
    for (int index = 0; index < result.savedPdfPaths.size(); ++index)
    {
        const SpeakingEvalReportData& expected =
            request.reports.at(index).report;
        const SpeakingEvalReportData& other =
            request.reports.at(1 - index).report;
        const QString expectedComposedKoreanName =
            expected.koreanName.normalized(
                QString::NormalizationForm_C
                );

        QPdfDocument document;
        QCOMPARE(
            document.load(result.savedPdfPaths.at(index)),
            QPdfDocument::Error::None
            );
        QCOMPARE(document.status(), QPdfDocument::Status::Ready);
        QVERIFY(document.pageCount() >= 1);

        const QString pdfText =
            document.getAllText(0).text();
        QVERIFY2(
            pdfText.contains(expected.englishName),
            qPrintable(pdfText)
            );
        QVERIFY2(
            pdfText.contains(expectedComposedKoreanName),
            qPrintable(pdfText)
            );
        QVERIFY2(
            pdfText.contains(expected.comments),
            qPrintable(pdfText)
            );
        QVERIFY(!pdfText.contains(other.englishName));
        QVERIFY(!pdfText.contains(other.comments));
        QVERIFY(
            !pdfText.contains(
                expected.koreanName.normalized(
                    QString::NormalizationForm_D
                    )
                )
            );

        const QImage page =
            document.render(0, QSize(540, 780))
                .convertToFormat(QImage::Format_ARGB32);
        QVERIFY(!page.isNull());
        renderedPages.append(page);

        if (includeSignature)
        {
            const QRect signatureBounds =
                speakingEvalReportTemplateLayout(
                    reportTemplate
                    ).signatureBounds.toAlignedRect();
            int signatureLeft = signatureBounds.right() + 1;
            int signatureBottom = -1;
            const int magentaPixels =
                matchingPixelCount(
                    page,
                    signatureBounds,
                    [](QRgb pixel)
                    {
                        return qRed(pixel) > 180
                            && qGreen(pixel) < 120
                            && qBlue(pixel) > 180;
                    }
                    );
            for (
                int y = signatureBounds.top();
                y <= signatureBounds.bottom();
                ++y
                )
            {
                for (
                    int x = signatureBounds.left();
                    x <= signatureBounds.right();
                    ++x
                    )
                {
                    const QRgb pixel = page.pixel(x, y);
                    if (qRed(pixel) > 180
                        && qGreen(pixel) < 120
                        && qBlue(pixel) > 180)
                    {
                        signatureLeft = qMin(signatureLeft, x);
                        signatureBottom = qMax(signatureBottom, y);
                    }
                }
            }
            QVERIFY2(
                magentaPixels > 20,
                qPrintable(
                    QStringLiteral(
                        "The shared signature was not visible in report %1."
                        ).arg(index + 1)
                    )
                );
            if (reportTemplate
                == SpeakingEvalReportTemplate::Advanced)
            {
                QVERIFY(
                    qAbs(
                        signatureLeft
                            - qRound(
                                speakingEvalReportTemplateLayout(
                                    reportTemplate
                                    ).signatureBounds.left()
                                )
                        ) <= 2
                    );
                QVERIFY(
                    qAbs(
                        signatureBottom
                            - qRound(
                                speakingEvalReportTemplateLayout(
                                    reportTemplate
                                    ).signatureBounds.bottom()
                                )
                        ) <= 2
                    );
            }
        }
    }

    const auto yellowPixels =
        [](const QImage& image, const QRectF& bounds)
    {
        return matchingPixelCount(
            image,
            bounds.toAlignedRect(),
            [](QRgb pixel)
            {
                return qRed(pixel) > 180
                    && qGreen(pixel) > 180
                    && qBlue(pixel) < 120;
            }
            );
    };
    const QRectF firstGradeBounds =
        speakingEvalScoreCell(
            reportTemplate,
            0,
            QStringLiteral("A+")
            );
    const QRectF lastGradeBounds =
        speakingEvalScoreCell(
            reportTemplate,
            0,
            QStringLiteral("C")
            );
    QVERIFY(
        yellowPixels(renderedPages.at(0), firstGradeBounds)
        > yellowPixels(renderedPages.at(0), lastGradeBounds)
        );
    QVERIFY(
        yellowPixels(renderedPages.at(1), lastGradeBounds)
        > yellowPixels(renderedPages.at(1), firstGradeBounds)
        );

#ifdef Q_OS_MACOS
    QVERIFY(!QFileInfo::exists(classMngrPowerPointWorkspace()));
    QVERIFY(!QFileInfo::exists(powerPointPrivateWorkspace()));
#endif
}

void SpeakingEvalBatchReportServiceTests::
    powerPointBatchCancellationLeavesNoFilesWhenAvailable()
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

    SpeakingEvalReportData first =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    first.englishName = QStringLiteral("Cancellation Student One");
    SpeakingEvalReportData second = first;
    second.englishName = QStringLiteral("Cancellation Student Two");

    SpeakingEvalBatchReportService::Request request;
    request.reports = {
        { first.englishName, first },
        { second.englishName, second }
    };
    request.renderer =
        SpeakingEvalBatchReportService::Renderer::PowerPoint;
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();
    request.progressCallback =
        [](int completed, int, const QString&)
    {
        return completed < 1;
    };

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);

    QCOMPARE(
        result.status,
        SpeakingEvalBatchReportService::Status::Canceled
        );
    QCOMPARE(
        QDir(outputDirectory.path()).entryList(
            { QStringLiteral("*.pdf") },
            QDir::Files
            ).size(),
        0
        );

#ifdef Q_OS_MACOS
    QVERIFY(!QFileInfo::exists(classMngrPowerPointWorkspace()));
    QVERIFY(!QFileInfo::exists(powerPointPrivateWorkspace()));
#endif
}

QTEST_MAIN(SpeakingEvalBatchReportServiceTests)

#include "speaking_eval_batch_report_service_tests.moc"
