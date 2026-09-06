#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/services/speaking_eval_ai_prompt.h"
#include "features/speaking_eval/services/speaking_eval_powerpoint_job_model.h"
#include "features/speaking_eval/services/speaking_eval_report_content_adapter.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_delegate.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_notes_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"
#include "core/settingsmanager.h"
#include "core/zip_archive_writer.h"
#include "ui/shared/state/option_state_keys.h"

#include <QtTest>

#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonArray>
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
#include <QTableWidget>
#include <QUndoStack>

#include <functional>

namespace
{
quint16 readLe16(
    const QByteArray& data,
    qsizetype offset
    )
{
    return static_cast<quint16>(
        static_cast<quint8>(data.at(offset))
        | (static_cast<quint16>(
               static_cast<quint8>(data.at(offset + 1))
               ) << 8)
        );
}

quint32 readLe32(
    const QByteArray& data,
    qsizetype offset
    )
{
    return static_cast<quint32>(readLe16(data, offset))
        | (static_cast<quint32>(readLe16(data, offset + 2)) << 16);
}

QHash<QString, QByteArray> storedZipEntries(
    const QString& archivePath
    )
{
    QFile archive(archivePath);
    if (!archive.open(QIODevice::ReadOnly))
    {
        return {};
    }

    const QByteArray data = archive.readAll();
    QHash<QString, QByteArray> entries;
    qsizetype offset = 0;
    while (offset + 30 <= data.size()
           && readLe32(data, offset) == 0x04034b50)
    {
        const quint16 compressionMethod =
            readLe16(data, offset + 8);
        const quint32 compressedSize =
            readLe32(data, offset + 18);
        const quint16 fileNameLength =
            readLe16(data, offset + 26);
        const quint16 extraLength =
            readLe16(data, offset + 28);
        const qsizetype dataOffset =
            offset + 30 + fileNameLength + extraLength;
        if (compressionMethod != 0
            || dataOffset < offset
            || dataOffset + compressedSize > data.size())
        {
            return {};
        }

        const QString fileName =
            QString::fromUtf8(
                data.mid(offset + 30, fileNameLength)
                );
        entries.insert(
            fileName,
            data.mid(dataOffset, compressedSize)
            );
        offset = dataOffset + compressedSize;
    }

    if (entries.isEmpty()
        || offset + 4 > data.size()
        || readLe32(data, offset) != 0x02014b50)
    {
        return {};
    }
    return entries;
}

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
    void initTestCase();
    void safeFileNameUsesStudentNamesAndRemovesReservedCharacters();
    void defaultOutputDirectoryIncludesClassScheduleAndEvaluation();
    void batchArchivePathUsesOutputFolderName();
    void zipWriterRejectsMissingAndDuplicateEntries();
    void reportDateUsesShortMonthsForStandardTemplate();
    void internalRendererCreatesReadablePdf();
    void batchCanKeepIndividualPdfFiles();
    void existingBatchArchiveRequiresOverwritePermission();
    void duplicateBatchFileNamesAreRejected();
    void internalPdfMatchesWidgetRendering_data();
    void internalPdfMatchesWidgetRendering();
    void singleReportCanBeSavedToAnExactFilePath();
    void overwriteExistingReportWhenAllowed();
    void previewDialogOffersActionsForTheSelectedReport();
    void selectedSourceRowMapsToItsFilteredReport();
    void previewDialogOnlyScrollsTheReport();
    void privateNotesAreSplitAndSaved();
    void privateNotesAutomaticallyContinueBullets();
    void aiPromptBuilderUsesObservationsAndSelectedVoice();
    void aiBatchPromptAnonymizesUpToFullClass();
    void aiBatchResponseParserHandlesPartialAndMalformedBlocks();
    void aiBatchDialogSelectsEligibleStudentsAndReviewsValidComments();
    void aiPromptButtonsRequireCompleteInput();
    void aiPromptPreviewCopiesAnAnonymousPrompt();
    void pastedAiCommentsReplaceStudentPlaceholder();
    void notesDialogShowsNotesBesideEachOtherAndCommentBelow();
    void notesDialogPreservesUntouchedValuesAndFocusesClickedSection();
    void notesDialogEnforcesCommentLimitAndUpdatesCounter();
    void notesDialogClearsOnlyCommentAfterConfirmation();
    void commentsAndNotesCellsUseTheCombinedDialog();
    void powerPointAvailabilityMessageIsAvailable();
    void reportContentAdapterPreservesEditedValues();
    void powerPointJobModelRejectsInvalidInputs();
    void powerPointJobModelPreservesValidMapping();
    void powerPointJobModelNormalizesUnicodeAndFitsComments();
    void mixedPowerPointTemplatesAreRejected();
    void generatedAssetsMatchSvgSourcesWhenEnabled();
    void powerPointRendererCreatesReadablePdfWhenAvailable_data();
    void powerPointRendererCreatesReadablePdfWhenAvailable();
    void powerPointBatchCancellationLeavesNoFilesWhenAvailable();

private:
    QTemporaryDir m_settingsRoot;
};

void SpeakingEvalBatchReportServiceTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    SettingsManager::instance().clear();
}

void SpeakingEvalBatchReportServiceTests::safeFileNameUsesStudentNamesAndRemovesReservedCharacters()
{
    QCOMPARE(
        SpeakingEvalBatchReportService::safeFileName(
            QStringLiteral("Jane: Doe"),
            QStringLiteral("김/철수")
            ),
        QStringLiteral("Jane- Doe (김-철수).pdf")
        );
    QCOMPARE(
        SpeakingEvalBatchReportService::safeFileName(
            QStringLiteral("김:학생"),
            QStringLiteral("🧑‍🏫/학생")
            ),
        QStringLiteral("김-학생 (🧑‍🏫-학생).pdf")
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

void SpeakingEvalBatchReportServiceTests::batchArchivePathUsesOutputFolderName()
{
    QCOMPARE(
        SpeakingEvalBatchReportService::batchArchivePath(
            QStringLiteral("C:/Documents/Speaking Evals/Winter")
            ),
        QDir::cleanPath(
            QStringLiteral(
                "C:/Documents/Speaking Evals/Winter/Winter.zip"
                )
            )
        );
}

void SpeakingEvalBatchReportServiceTests::
    zipWriterRejectsMissingAndDuplicateEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString archivePath =
        QDir(directory.path()).filePath(
            QStringLiteral("Reports.zip")
            );
    QString errorMessage;
    QVERIFY(
        !ZipArchiveWriter::writeArchive(
            archivePath,
            {
                {
                    QDir(directory.path()).filePath(
                        QStringLiteral("missing.pdf")
                        ),
                    QStringLiteral("Missing.pdf")
                }
            },
            &errorMessage
            )
        );
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(!QFileInfo::exists(archivePath));

    const QString sourcePath =
        QDir(directory.path()).filePath(
            QStringLiteral("report.pdf")
            );
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QVERIFY(source.write("%PDF-test") > 0);
    source.close();

    errorMessage.clear();
    QVERIFY(
        !ZipArchiveWriter::writeArchive(
            archivePath,
            {
                { sourcePath, QStringLiteral("Same.pdf") },
                { sourcePath, QStringLiteral("Same.pdf") }
            },
            &errorMessage
            )
        );
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(!QFileInfo::exists(archivePath));
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
    QVERIFY(result.savedPdfPaths.isEmpty());
    QCOMPARE(
        result.savedArchivePath,
        SpeakingEvalBatchReportService::batchArchivePath(
            outputDirectory.path()
            )
        );
    QVERIFY(QFileInfo::exists(result.savedArchivePath));
    QCOMPARE(
        QDir(outputDirectory.path()).entryList(
            QDir::Files | QDir::NoDotAndDotDot
            ),
        QStringList{
            QFileInfo(result.savedArchivePath).fileName()
        }
        );

    const QHash<QString, QByteArray> entries =
        storedZipEntries(result.savedArchivePath);
    QCOMPARE(entries.size(), 2);
    const QStringList expectedNames{
        SpeakingEvalBatchReportService::safeFileName(
            data.englishName,
            data.koreanName
            ),
        SpeakingEvalBatchReportService::safeFileName(
            advancedData.englishName,
            advancedData.koreanName
            )
    };
    for (const QString& fileName : expectedNames)
    {
        QVERIFY(entries.contains(fileName));
        QVERIFY(entries.value(fileName).startsWith("%PDF"));

        QBuffer pdfBuffer;
        pdfBuffer.setData(entries.value(fileName));
        QVERIFY(pdfBuffer.open(QIODevice::ReadOnly));
        QPdfDocument document;
        document.load(&pdfBuffer);
        QCOMPARE(document.error(), QPdfDocument::Error::None);
        QCOMPARE(document.status(), QPdfDocument::Status::Ready);
        QCOMPARE(document.pageCount(), 1);
    }
}

void SpeakingEvalBatchReportServiceTests::batchCanKeepIndividualPdfFiles()
{
    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData first =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    first.englishName = QStringLiteral("First Student");
    first.koreanName = QStringLiteral("첫학생");
    SpeakingEvalReportData second = first;
    second.englishName = QStringLiteral("Second Student");
    second.koreanName = QStringLiteral("둘째학생");

    SpeakingEvalBatchReportService::Request request;
    request.reports = {
        { first.englishName, first },
        { second.englishName, second }
    };
    request.savePdf = true;
    request.keepIndividualPdfFiles = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    QVERIFY2(
        result.status == SpeakingEvalBatchReportService::Status::Completed,
        qPrintable(result.message)
        );
    QCOMPARE(result.savedPdfPaths.size(), 2);
    QVERIFY(QFileInfo::exists(result.savedArchivePath));

    const QHash<QString, QByteArray> entries =
        storedZipEntries(result.savedArchivePath);
    QCOMPARE(entries.size(), 2);
    for (const QString& pdfPath : result.savedPdfPaths)
    {
        QFile pdf(pdfPath);
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(
            entries.value(QFileInfo(pdfPath).fileName()),
            pdf.readAll()
            );
    }
}

void SpeakingEvalBatchReportServiceTests::
    existingBatchArchiveRequiresOverwritePermission()
{
    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData first =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    first.englishName = QStringLiteral("First Student");
    SpeakingEvalReportData second = first;
    second.englishName = QStringLiteral("Second Student");

    const QString archivePath =
        SpeakingEvalBatchReportService::batchArchivePath(
            outputDirectory.path()
            );
    QFile existingArchive(archivePath);
    QVERIFY(existingArchive.open(QIODevice::WriteOnly));
    QCOMPARE(existingArchive.write("old"), 3);
    existingArchive.close();
    const QString existingPdfPath =
        QDir(outputDirectory.path()).filePath(
            SpeakingEvalBatchReportService::safeFileName(
                first.englishName,
                first.koreanName
                )
            );
    QFile existingPdf(existingPdfPath);
    QVERIFY(existingPdf.open(QIODevice::WriteOnly));
    QCOMPARE(existingPdf.write("old-pdf"), 7);
    existingPdf.close();

    SpeakingEvalBatchReportService::Request request;
    request.reports = {
        { first.englishName, first },
        { second.englishName, second }
    };
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();

    SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    QCOMPARE(
        result.status,
        SpeakingEvalBatchReportService::Status::Failed
        );
    existingArchive.setFileName(archivePath);
    QVERIFY(existingArchive.open(QIODevice::ReadOnly));
    QCOMPARE(existingArchive.readAll(), QByteArray("old"));
    existingArchive.close();

    request.overwriteExisting = true;
    result =
        SpeakingEvalBatchReportService::exportReports(request);
    QVERIFY2(
        result.status == SpeakingEvalBatchReportService::Status::Completed,
        qPrintable(result.message)
        );
    QCOMPARE(result.savedArchivePath, archivePath);
    QCOMPARE(storedZipEntries(archivePath).size(), 2);
    existingPdf.setFileName(existingPdfPath);
    QVERIFY(existingPdf.open(QIODevice::ReadOnly));
    QCOMPARE(existingPdf.readAll(), QByteArray("old-pdf"));
    QVERIFY(
        !QFileInfo::exists(
            archivePath + QStringLiteral(".classmngr-backup")
            )
        );
}

void SpeakingEvalBatchReportServiceTests::
    duplicateBatchFileNamesAreRejected()
{
    QTemporaryDir outputDirectory;
    QVERIFY(outputDirectory.isValid());

    SpeakingEvalReportData data =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    data.englishName = QStringLiteral("Same Student");

    SpeakingEvalBatchReportService::Request request;
    request.reports = {
        { QStringLiteral("First"), data },
        { QStringLiteral("Second"), data }
    };
    request.savePdf = true;
    request.outputDirectory = outputDirectory.path();

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    QCOMPARE(
        result.status,
        SpeakingEvalBatchReportService::Status::Failed
        );
    QVERIFY(
        QDir(outputDirectory.path()).entryList(
            QDir::Files | QDir::NoDotAndDotDot
            ).isEmpty()
        );
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
            "[Needs Improvement]\n• Use complete sentences"
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
    auto* needsImprovementEdit = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("speakingEvalNeedsImprovementNotes")
        );
    QVERIFY(didWellEdit);
    QVERIFY(needsImprovementEdit);

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

    needsImprovementEdit->setFocus();
    QTest::keyClicks(
        needsImprovementEdit,
        QStringLiteral("Use longer answers")
        );
    QTest::keyClick(needsImprovementEdit, Qt::Key_Return);
    QTest::keyClicks(
        needsImprovementEdit,
        QStringLiteral("Add more detail")
        );

    QCOMPARE(
        needsImprovementEdit->toPlainText(),
        QStringLiteral(
            "• Use longer answers\n"
            "• Add more detail"
            )
        );
}

void SpeakingEvalBatchReportServiceTests::
    aiPromptBuilderUsesObservationsAndSelectedVoice()
{
    SpeakingEvalAiPromptInput input;
    input.grade = 5;
    input.englishName = QStringLiteral("Alice");
    input.koreanName = QStringLiteral("김민지");
    input.didWell =
        QStringLiteral(
            "• Alice showed strong memorization\n"
            "• vocabulary\n\n"
            "- 김민지 maintained eye contact"
            );
    input.needsImprovement =
        QStringLiteral(
            "• pronunciation of complex words\n"
            "• supporting ideas"
            );

    const QString directPrompt =
        buildSpeakingEvalAiCommentPrompt(input);
    QVERIFY(!directPrompt.isEmpty());
    QVERIFY(
        directPrompt.contains(
            QStringLiteral("5th-grade elementary ESL student")
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "between 100 and 420 characters"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(QStringLiteral("exactly 3 short sentences"))
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "notes provide enough relevant detail, aim for 300 to 350 "
                "characters"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "without inventing or repeating information"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(QStringLiteral("simple, common English"))
        );
    QVERIFY(
        directPrompt.contains(QStringLiteral("parent with limited English"))
        );
    QVERIFY(
        directPrompt.contains(QStringLiteral("Avoid long sentences"))
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "Sentence 1 must give positive feedback, sentence 2 must "
                "give constructive advice, and sentence 3 must give positive "
                "feedback"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "If sentence 1 emphasizes presentation skills, sentence 3 "
                "must emphasize grammar-related skills"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "If sentence 1 emphasizes grammar-related skills, sentence "
                "3 must emphasize presentation skills"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "Aim to praise at least two submitted skills in each "
                "positive sentence"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "supplement it with another submitted strength"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral("Never invent a skill to reach two items")
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral("You do not need to mention every item")
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "End with a short final sentence that praises specific items "
                "from the Did well list"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "Address the student directly as \"you\""
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "- STD_NAME showed strong memorization\n"
                "- vocabulary\n"
                "- STD_NAME maintained eye contact"
                )
            )
        );
    QVERIFY(
        directPrompt.contains(
            QStringLiteral(
                "- pronunciation of complex words\n"
                "- supporting ideas"
                )
            )
        );
    QVERIFY(directPrompt.contains(QStringLiteral("STD_NAME")));
    QVERIFY(!directPrompt.contains(QStringLiteral("Alice")));
    QVERIFY(!directPrompt.contains(QStringLiteral("김민지")));

    input.voice = AiCommentVoice::ThirdPerson;
    const QString thirdPersonPrompt =
        buildSpeakingEvalAiCommentPrompt(input);
    QVERIFY(
        thirdPersonPrompt.contains(
            QStringLiteral("Write for a parent or guardian")
            )
        );
    QVERIFY(
        thirdPersonPrompt.contains(
            QStringLiteral("use they/their")
            )
        );

    input.grade = 3;
    QVERIFY(buildSpeakingEvalAiCommentPrompt(input).isEmpty());
    input.grade = 6;
    input.needsImprovement.clear();
    QVERIFY(buildSpeakingEvalAiCommentPrompt(input).isEmpty());

    QCOMPARE(
        speakingEvalElementaryGrade(
            QStringLiteral("E4")
            ),
        4
        );
    QCOMPARE(
        speakingEvalElementaryGrade(
            QStringLiteral("e6")
            ),
        6
        );
    QCOMPARE(
        speakingEvalElementaryGrade(
            QStringLiteral("M1")
            ),
        0
        );
}

void SpeakingEvalBatchReportServiceTests::
    aiBatchPromptAnonymizesUpToFullClass()
{
    for (const int studentCount : { 1, 20, 25 })
    {
        SpeakingEvalAiBatchPromptInput input;
        input.voice = AiCommentVoice::ThirdPerson;
        for (int index = 0; index < studentCount; ++index)
        {
            const QString id =
                QStringLiteral("STUDENT_%1")
                    .arg(
                        index + 1,
                        2,
                        10,
                        QLatin1Char('0')
                        );
            const QString englishName =
                QStringLiteral("PrivateName%1")
                    .arg(index + 1);
            const QString koreanName =
                QStringLiteral("학생이름%1")
                    .arg(index + 1);
            input.students.append(
                {
                    id,
                    4 + (index % 3),
                    englishName,
                    koreanName,
                    QStringLiteral(
                        "%1 used strong vocabulary\n"
                        "%2 maintained eye contact"
                        )
                        .arg(
                            englishName,
                            koreanName
                            ),
                    QStringLiteral(
                        "%1 should add supporting details"
                        )
                        .arg(englishName)
                }
                );
        }
        if (studentCount > 1)
        {
            input.students[0].didWell +=
                QStringLiteral(
                    "\nPrivateName2 collaborated helpfully"
                    );
        }

        const QString prompt =
            buildSpeakingEvalAiBatchCommentPrompt(input);
        QVERIFY(!prompt.isEmpty());
        QVERIFY(
            prompt.contains(
                QStringLiteral("Write for a parent or guardian")
                )
            );
        QCOMPARE(
            prompt.count(
                QStringLiteral("exactly 3 short sentences")
                ),
            1
            );
        QCOMPARE(
            prompt.count(
                QRegularExpression(
                    QStringLiteral(
                        R"(Student ID: STUDENT_[0-9]{2})"
                        )
                    )
                ),
            studentCount
            );
        for (int index = 0; index < studentCount; ++index)
        {
            const QString id =
                QStringLiteral("STUDENT_%1")
                    .arg(
                        index + 1,
                        2,
                        10,
                        QLatin1Char('0')
                        );
            QVERIFY(
                prompt.contains(
                    QStringLiteral("<<<%1>>>").arg(id)
                    )
                );
            QVERIFY(
                prompt.contains(
                    QStringLiteral("<<<END_%1>>>").arg(id)
                    )
                );
            QVERIFY(
                !prompt.contains(
                    QStringLiteral("PrivateName%1")
                        .arg(index + 1)
                    )
                );
            QVERIFY(
                !prompt.contains(
                    QStringLiteral("학생이름%1")
                        .arg(index + 1)
                    )
                );
        }
    }
}

void SpeakingEvalBatchReportServiceTests::
    aiBatchResponseParserHandlesPartialAndMalformedBlocks()
{
    const QString response =
        QStringLiteral(
            "The requested comments follow.\n"
            "```text\n"
            "<<<STUDENT_02>>>\n"
            "Great work, STD_NAME.\n"
            "<<<END_STUDENT_02>>>\n"
            "<<<STUDENT_01>>>\n"
            "First duplicate.\n"
            "<<<END_STUDENT_01>>>\n"
            "<<<STUDENT_99>>>\n"
            "Unknown student.\n"
            "<<<END_STUDENT_99>>>\n"
            "<<<STUDENT_01>>>\n"
            "Second duplicate.\n"
            "<<<END_STUDENT_01>>>\n"
            "<<<STUDENT_03>>>\n"
            "This block is truncated.\n"
            "```"
            );

    const SpeakingEvalAiBatchParseResult result =
        parseSpeakingEvalAiBatchResponse(
            response,
            {
                QStringLiteral("STUDENT_01"),
                QStringLiteral("STUDENT_02"),
                QStringLiteral("STUDENT_03"),
                QStringLiteral("STUDENT_04")
            }
            );

    QCOMPARE(result.comments.size(), 1);
    QCOMPARE(
        result.comments.first().id,
        QStringLiteral("STUDENT_02")
        );
    QCOMPARE(
        result.comments.first().comment,
        QStringLiteral("Great work, STD_NAME.")
        );
    QVERIFY(result.comments.first().hadNamePlaceholder);
    QCOMPARE(
        result.duplicateIds,
        QStringList{ QStringLiteral("STUDENT_01") }
        );
    QCOMPARE(
        result.malformedIds,
        QStringList{ QStringLiteral("STUDENT_03") }
        );
    QCOMPARE(
        result.unknownIds,
        QStringList{ QStringLiteral("STUDENT_99") }
        );
}

void SpeakingEvalBatchReportServiceTests::
    aiBatchDialogSelectsEligibleStudentsAndReviewsValidComments()
{
    SpeakingEvalReportData ready;
    ready.englishName = QStringLiteral("Alice");
    ready.grade = 5;
    ready.notes =
        QStringLiteral(
            "[Did Well]\nClear pronunciation\n"
            "[Needs Improvement]\nAdd supporting details"
            );

    SpeakingEvalReportData existing = ready;
    existing.englishName = QStringLiteral("Bob");
    existing.comments = QStringLiteral("Existing comment");

    SpeakingEvalReportData missingNotes = ready;
    missingNotes.englishName = QStringLiteral("Carol");
    missingNotes.notes =
        QStringLiteral("[Did Well]\n\n[Needs Improvement]\nPractice fluency");

    SpeakingEvalReportData unsupportedGrade = ready;
    unsupportedGrade.englishName = QStringLiteral("David");
    unsupportedGrade.grade = 0;

    SpeakingEvalAiBatchDialog dialog(
        {
            { QStringLiteral("Alice"), ready, 0 },
            { QStringLiteral("Bob"), existing, 1 },
            { QStringLiteral("Carol"), missingNotes, 2 },
            { QStringLiteral("David"), unsupportedGrade, 3 }
        }
        );

    auto* selection =
        dialog.findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchSelectionTable")
            );
    auto* createPromptButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCreatePrompt")
            );
    auto* promptEdit =
        dialog.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchPrompt")
            );
    auto* responseEdit =
        dialog.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchResponse")
            );
    auto* parseButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchParse")
            );
    auto* review =
        dialog.findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchReviewTable")
            );
    auto* applyButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchApply")
            );

    QVERIFY(selection);
    QVERIFY(createPromptButton);
    QVERIFY(promptEdit);
    QVERIFY(responseEdit);
    QVERIFY(parseButton);
    QVERIFY(review);
    QVERIFY(applyButton);
    QCOMPARE(selection->rowCount(), 4);
    QCOMPARE(selection->item(0, 0)->checkState(), Qt::Checked);
    QCOMPARE(selection->item(1, 0)->checkState(), Qt::Unchecked);
    QVERIFY(selection->item(1, 0)->flags() & Qt::ItemIsEnabled);
    QVERIFY(!(selection->item(2, 0)->flags() & Qt::ItemIsEnabled));
    QVERIFY(!(selection->item(3, 0)->flags() & Qt::ItemIsEnabled));

    createPromptButton->click();
    QVERIFY(
        promptEdit->toPlainText().contains(
            QStringLiteral("Student ID: STUDENT_01")
            )
        );
    QVERIFY(
        !promptEdit->toPlainText().contains(
            QStringLiteral("Alice")
            )
        );

    responseEdit->setPlainText(
        QStringLiteral(
            "<<<STUDENT_01>>>\n"
            "STD_NAME spoke clearly and used strong vocabulary. "
            "Keep adding supporting details and practice difficult sounds. "
            "Your eye contact and confident voice made the presentation "
            "engaging.\n"
            "<<<END_STUDENT_01>>>"
            )
        );
    QVERIFY(parseButton->isEnabled());
    parseButton->click();

    QCOMPARE(review->rowCount(), 1);
    QCOMPARE(review->item(0, 0)->checkState(), Qt::Checked);
    QCOMPARE(
        review->item(0, 2)->text(),
        QStringLiteral("Ready")
        );
    QVERIFY(
        review->item(0, 4)->text().startsWith(
            QStringLiteral("Alice spoke clearly")
            )
        );
    QVERIFY(applyButton->isEnabled());

    applyButton->click();
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(dialog.acceptedComments().size(), 1);
    QCOMPARE(dialog.acceptedComments().first().sourceRow, 0);
    QVERIFY(
        dialog.acceptedComments().first().newComment.contains(
            QStringLiteral("Alice")
            )
        );
    QVERIFY(
        !dialog.acceptedComments().first().newComment.contains(
            QStringLiteral("STD_NAME")
            )
        );

    SpeakingEvalRows rows =
        SpeakingEval::emptyRows();
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::EnglishName
        )] = QStringLiteral("Alice");
    SpeakingEvalModel model;
    model.loadData(rows);
    SpeakingEvalTableView table;
    QUndoStack undoStack;
    table.setModel(&model);
    table.setUndoStack(&undoStack);
    QSignalSpy modifiedSpy(
        &model,
        &SpeakingEvalModel::dataModified
        );

    const SpeakingEvalAiBatchAcceptedComment accepted =
        dialog.acceptedComments().first();
    table.applyChanges(
        {
            {
                accepted.sourceRow,
                SpeakingEval::toInt(
                    SpeakingEvalColumn::Comments
                    ),
                accepted.oldComment,
                accepted.newComment
            }
        },
        QStringLiteral("Apply AI Comments")
        );
    QCOMPARE(undoStack.count(), 1);
    QCOMPARE(
        model.rows()
            .at(0)
            .at(
                SpeakingEval::toInt(
                    SpeakingEvalColumn::Comments
                    )
                ),
        accepted.newComment
        );
    QVERIFY(!modifiedSpy.isEmpty());

    undoStack.undo();
    QCOMPARE(
        model.rows()
            .at(0)
            .at(
                SpeakingEval::toInt(
                    SpeakingEvalColumn::Comments
                    )
                ),
        QString()
        );
}

void SpeakingEvalBatchReportServiceTests::
    aiPromptButtonsRequireCompleteInput()
{
    SpeakingEvalReportData reportData;
    reportData.englishName = QStringLiteral("Student");
    reportData.grade = 5;
    reportData.notes =
        QStringLiteral(
            "[Did Well]\n• Clear pronunciation\n"
            "[Needs Improvement]\n• Add supporting ideas"
            );

    SpeakingEvalReportDialog dialog(
        { { QStringLiteral("Student"), reportData } },
        0,
        nullptr,
        true
        );

    auto* previewButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalPreviewAiPromptButton")
            );
    auto* copyOpenButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
    auto* didWellEdit =
        dialog.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalDidWellNotes")
            );
    auto* needsImprovementEdit =
        dialog.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalNeedsImprovementNotes")
            );

    QVERIFY(previewButton);
    QVERIFY(copyOpenButton);
    QVERIFY(didWellEdit);
    QVERIFY(needsImprovementEdit);
    QVERIFY(previewButton->isEnabled());
    QVERIFY(copyOpenButton->isEnabled());

    didWellEdit->clear();
    QVERIFY(!previewButton->isEnabled());
    QVERIFY(
        previewButton->toolTip().contains(
            QStringLiteral("Did Well")
            )
        );

    didWellEdit->setPlainText(
        QStringLiteral("• Strong effort")
        );
    QVERIFY(previewButton->isEnabled());

    needsImprovementEdit->clear();
    QVERIFY(!copyOpenButton->isEnabled());
    QVERIFY(
        copyOpenButton->toolTip().contains(
            QStringLiteral("Needs Improvement")
            )
        );

    reportData.grade = 0;
    SpeakingEvalReportDialog unknownGradeDialog(
        { { QStringLiteral("Student"), reportData } },
        0,
        nullptr,
        true
        );
    auto* unknownGradeButton =
        unknownGradeDialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalPreviewAiPromptButton")
            );
    QVERIFY(unknownGradeButton);
    QVERIFY(!unknownGradeButton->isEnabled());
    QVERIFY(
        unknownGradeButton->toolTip().contains(
            QStringLiteral("E4 through E6")
            )
        );
}

void SpeakingEvalBatchReportServiceTests::
    aiPromptPreviewCopiesAnAnonymousPrompt()
{
    SettingsManager::instance().set(
        QString::fromUtf8(
            OptionKeys::AiCommentProvider
            ),
        std::to_underlying(
            AiCommentProvider::Gemini
            )
        );

    SpeakingEvalReportData reportData;
    reportData.englishName = QStringLiteral("Alice");
    reportData.koreanName = QStringLiteral("김민지");
    reportData.grade = 4;
    reportData.notes =
        QStringLiteral(
            "[Did Well]\n• memorization\n"
            "[Needs Improvement]\n• posture"
            );
    SpeakingEvalReportDialog dialog(
        { { QStringLiteral("Alice (김민지)"), reportData } },
        0,
        nullptr,
        true
        );

    auto* previewButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalPreviewAiPromptButton")
            );
    auto* copyOpenButton =
        dialog.findChild<QPushButton*>(
            QStringLiteral("speakingEvalCopyOpenAiPromptButton")
            );
    QVERIFY(previewButton);
    QVERIFY(copyOpenButton);
    QVERIFY(
        copyOpenButton->text().contains(
            QStringLiteral("Gemini")
            )
        );

    bool previewFound = false;
    bool promptEditFound = false;
    bool promptWasReadOnly = false;
    bool copyButtonFound = false;
    bool clipboardMatched = false;
    bool promptWasAnonymous = false;
    QApplication::clipboard()->clear();
    QTimer modalSafetyTimer(&dialog);
    modalSafetyTimer.setSingleShot(true);
    connect(
        &modalSafetyTimer,
        &QTimer::timeout,
        &dialog,
        [&dialog]()
        {
            if (
                auto* preview =
                    dialog.findChild<QDialog*>(
                        QStringLiteral(
                            "speakingEvalAiPromptPreviewDialog"
                            )
                        )
                )
            {
                preview->reject();
            }
        }
        );
    modalSafetyTimer.start(2000);
    QTimer::singleShot(
        50,
        [&]()
        {
            auto* preview =
                dialog.findChild<QDialog*>(
                    QStringLiteral(
                        "speakingEvalAiPromptPreviewDialog"
                        )
                    );
            if (!preview)
            {
                return;
            }

            previewFound = true;
            auto* promptEdit =
                preview->findChild<QPlainTextEdit*>(
                    QStringLiteral(
                        "speakingEvalAiPromptPreviewText"
                        )
                    );
            auto* copyButton =
                preview->findChild<QPushButton*>(
                    QStringLiteral(
                        "speakingEvalAiPromptPreviewCopy"
                        )
                    );
            promptEditFound = promptEdit != nullptr;
            promptWasReadOnly =
                promptEdit && promptEdit->isReadOnly();
            copyButtonFound = copyButton != nullptr;

            if (promptEdit && copyButton)
            {
                const QString prompt =
                    promptEdit->toPlainText();
                promptWasAnonymous =
                    prompt.contains(QStringLiteral("STD_NAME"))
                    && !prompt.contains(QStringLiteral("Alice"))
                    && !prompt.contains(QStringLiteral("김민지"));
                QTest::mouseClick(
                    copyButton,
                    Qt::LeftButton
                    );
                clipboardMatched =
                    QApplication::clipboard()->text() == prompt;
            }
            preview->accept();
        }
        );

    QTest::mouseClick(
        previewButton,
        Qt::LeftButton
        );
    modalSafetyTimer.stop();
    QVERIFY(previewFound);
    QVERIFY(promptEditFound);
    QVERIFY(promptWasReadOnly);
    QVERIFY(copyButtonFound);
    QVERIFY(clipboardMatched);
    QVERIFY(promptWasAnonymous);
}

void SpeakingEvalBatchReportServiceTests::
    pastedAiCommentsReplaceStudentPlaceholder()
{
    SpeakingEvalReportData reportData;
    reportData.englishName = QStringLiteral("Alex");

    SpeakingEvalReportWidget report;
    report.setReportData(reportData);
    report.setInteractive(true);
    auto* reportComment =
        report.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalReportComments")
            );
    QVERIFY(reportComment);
    reportComment->setFocus();
    QApplication::clipboard()->setText(
        QStringLiteral(
            "Great work, STD_NAME! STD_NAME can improve. "
            "std_name should remain."
            )
        );
    QTest::keyClick(
        reportComment,
        Qt::Key_V,
        Qt::ControlModifier
        );
    QCOMPARE(
        reportComment->toPlainText(),
        QStringLiteral(
            "Great work, Alex! Alex can improve. "
            "std_name should remain."
            )
        );

    SpeakingEvalNotesDialog koreanNameDialog(
        {},
        {},
        SpeakingEvalNotesDialog::InitialSection::Comment,
        nullptr,
        {},
        QStringLiteral("김민준")
        );
    auto* notesDialogComment =
        koreanNameDialog.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalNotesDialogComment")
            );
    QVERIFY(notesDialogComment);
    notesDialogComment->setFocus();
    QApplication::clipboard()->setText(
        QStringLiteral("Keep going, STD_NAME!")
        );
    QTest::keyClick(
        notesDialogComment,
        Qt::Key_V,
        Qt::ControlModifier
        );
    QCOMPARE(
        notesDialogComment->toPlainText(),
        QStringLiteral("Keep going, 김민준!")
        );

    notesDialogComment->clear();
    QApplication::clipboard()->setText(
        QStringLiteral("STD_NAME ").repeated(200)
        );
    QTest::keyClick(
        notesDialogComment,
        Qt::Key_V,
        Qt::ControlModifier
        );
    QCOMPARE(
        notesDialogComment->toPlainText().size(),
        SpeakingEval::CommentMaxLength
        );

    SpeakingEvalNotesDialog missingNameDialog(
        {},
        {},
        SpeakingEvalNotesDialog::InitialSection::Comment
        );
    auto* missingNameComment =
        missingNameDialog.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalNotesDialogComment")
            );
    QVERIFY(missingNameComment);
    missingNameComment->setFocus();
    QApplication::clipboard()->setText(
        QStringLiteral("Well done, STD_NAME!")
        );
    QTest::keyClick(
        missingNameComment,
        Qt::Key_V,
        Qt::ControlModifier
        );
    QCOMPARE(
        missingNameComment->toPlainText(),
        QStringLiteral("Well done, STD_NAME!")
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
                    auto* destructiveButton =
                        messageBox->findChild<QPushButton*>(
                            QStringLiteral("promptDestructiveButton")
                            );
                    QVERIFY(destructiveButton);
                    destructiveButton->click();
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
        QStringLiteral("Internal Template (Default)")
        );
    QCOMPARE(
        SpeakingEvalBatchReportService::rendererDisplayName(
            SpeakingEvalBatchReportService::Renderer::PowerPoint
            ),
        QStringLiteral("PowerPoint (Fallback)")
        );
    QVERIFY(
        !SpeakingEvalBatchReportService::powerPointRendererAvailabilityMessage()
             .trimmed()
             .isEmpty()
        );
}

void SpeakingEvalBatchReportServiceTests::
    reportContentAdapterPreservesEditedValues()
{
    SpeakingEvalReportData data =
        goldenReportData(SpeakingEvalReportTemplate::Advanced);
    data.englishName = QStringLiteral("Edited English Name");
    data.koreanName = QStringLiteral("편집된 이름");
    data.classLabel = QStringLiteral("Edited Class");
    data.nativeTeacher = QStringLiteral("Edited Native Teacher");
    data.koreanTeacher = QStringLiteral("편집된 한국인 선생님");
    data.date = QStringLiteral("Edited Date");
    data.comments = QStringLiteral("Edited comments");
    data.notes = QStringLiteral("Edited private notes");
    data.grade = 5;
    data.scores = {
        QStringLiteral("C"),
        QStringLiteral("B"),
        QStringLiteral("B+"),
        QStringLiteral("A"),
        QStringLiteral("A+"),
        QStringLiteral("A+")
    };

    const QList<SpeakingEvalBatchReportService::StudentReport> reports{
        {
            QStringLiteral("Edited display name"),
            data,
            23
        }
    };
    const std::vector<SpeakingEvalReportContentAdapter::ReportContent>
        contents = SpeakingEvalReportContentAdapter::toEngine(reports);

    QCOMPARE(contents.size(), std::size_t(1));
    const auto& content = contents.front();
    QCOMPARE(QString::fromStdString(content.displayName), reports.front().displayName);
    QCOMPARE(QString::fromStdString(content.englishName), data.englishName);
    QCOMPARE(QString::fromStdString(content.koreanName), data.koreanName);
    QCOMPARE(QString::fromStdString(content.classLabel), data.classLabel);
    QCOMPARE(QString::fromStdString(content.nativeTeacher), data.nativeTeacher);
    QCOMPARE(QString::fromStdString(content.koreanTeacher), data.koreanTeacher);
    QCOMPARE(QString::fromStdString(content.date), data.date);
    QCOMPARE(QString::fromStdString(content.comments), data.comments);
    QCOMPARE(QString::fromStdString(content.notes), data.notes);
    QCOMPARE(content.grade, data.grade);
    QCOMPARE(content.reportTemplate, data.reportTemplate);
    QCOMPARE(content.sourceRow, reports.front().sourceRow);
    for (std::size_t index = 0; index < content.scores.size(); ++index)
    {
        QCOMPARE(
            QString::fromStdString(content.scores[index]),
            data.scores[index]
            );
    }
}

void SpeakingEvalBatchReportServiceTests::powerPointJobModelRejectsInvalidInputs()
{
    const QString workingDirectory = QStringLiteral("C:/PowerPointWork");
    const QString documentsRoot = QStringLiteral("C:/Documents");

    const auto noReports =
        SpeakingEvalPowerPointJobModel::build(
            {},
            {},
            workingDirectory,
            documentsRoot,
            {}
            );
    QVERIFY(!noReports);
    QVERIFY(
        noReports.error().contains(
            QStringLiteral("student reports"),
            Qt::CaseInsensitive
            )
        );

    const SpeakingEvalReportData reportData =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    const QList<SpeakingEvalBatchReportService::StudentReport> sourceReports{
        {
            reportData.englishName,
            reportData,
            0
        }
    };
    const auto reports =
        SpeakingEvalReportContentAdapter::toEngine(sourceReports);
    const auto mismatchedPdfPaths =
        SpeakingEvalPowerPointJobModel::build(
            reports,
            {},
            workingDirectory,
            documentsRoot,
            reportData.signatureImage
            );
    QVERIFY(!mismatchedPdfPaths);
    QVERIFY(
        mismatchedPdfPaths.error().contains(
            QStringLiteral("PDF"),
            Qt::CaseInsensitive
            )
        );
}

void SpeakingEvalBatchReportServiceTests::
    powerPointJobModelPreservesValidMapping()
{
    const QString workingDirectory = QStringLiteral("C:/PowerPointWork");
    const QString documentsRoot = QStringLiteral("C:/Documents");
    const QString pdfPath =
        QDir(workingDirectory).filePath(QStringLiteral("report.pdf"));
    const QString expectedCompletionPath =
        QDir(workingDirectory).filePath(QStringLiteral("completed-000000"));
    const SpeakingEvalReportData reportData =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    const QList<SpeakingEvalBatchReportService::StudentReport> sourceReports{
        {
            QStringLiteral("Gildong (홍길동)"),
            reportData,
            0
        }
    };
    const auto reports =
        SpeakingEvalReportContentAdapter::toEngine(sourceReports);

    const auto result =
        SpeakingEvalPowerPointJobModel::build(
            reports,
            { pdfPath },
            workingDirectory,
            documentsRoot,
            reportData.signatureImage
            );
    QVERIFY(result);

    const SpeakingEvalPowerPointJobModel::BatchJob& batch = *result;
    QCOMPARE(
        batch.templateProfile.reportTemplate,
        SpeakingEvalReportTemplate::Standard
        );
    QCOMPARE(
        batch.templateProfile.scoreTableName,
        QStringLiteral("Grades & Scores")
        );
    QCOMPARE(batch.students.size(), 1);
    QCOMPARE(
        batch.students.constFirst().displayName,
        QString::fromStdString(reports.front().displayName)
        );
    QCOMPARE(batch.students.constFirst().pdfPath, pdfPath);
    QCOMPARE(
        batch.students.constFirst().completionPath,
        expectedCompletionPath
        );

    const QJsonObject json =
        SpeakingEvalPowerPointJobModel::toJson(
            batch,
            QStringLiteral("C:/PowerPointWork/output.pptx"),
            QStringLiteral("C:/PowerPointWork/signature.png"),
            QStringLiteral("C:/PowerPointWork/cancel")
            );
    QCOMPARE(
        json.value(QStringLiteral("scoreTableName")).toString(),
        QStringLiteral("Grades & Scores")
        );
    const QJsonArray students =
        json.value(QStringLiteral("students")).toArray();
    QCOMPARE(students.size(), 1);
    const QJsonObject student = students.at(0).toObject();
    QCOMPARE(
        student.value(QStringLiteral("displayName")).toString(),
        QString::fromStdString(reports.front().displayName)
        );
    QCOMPARE(
        student.value(QStringLiteral("englishName")).toString(),
        reportData.englishName
        );
    QCOMPARE(
        student.value(QStringLiteral("pdfPath")).toString(),
        QDir::toNativeSeparators(pdfPath)
        );
    QCOMPARE(
        student.value(QStringLiteral("completionPath")).toString(),
        QDir::toNativeSeparators(expectedCompletionPath)
        );
    QCOMPARE(
        student.value(QStringLiteral("overallGrade")).toString(),
        QStringLiteral("B+")
        );
    QCOMPARE(student.value(QStringLiteral("scores")).toArray().size(), 6);
}

void SpeakingEvalBatchReportServiceTests::
    powerPointJobModelNormalizesUnicodeAndFitsComments()
{
    const QString decomposedKorean =
        QStringLiteral("\u1112\u1169\u11BC\u1100\u1175\u11AF\u1103\u1169\u11BC");
    const QString composedKorean = QStringLiteral("홍길동");
    SpeakingEvalReportData reportData =
        goldenReportData(SpeakingEvalReportTemplate::Standard);
    reportData.koreanName = decomposedKorean;
    reportData.comments = decomposedKorean;
    const QList<SpeakingEvalBatchReportService::StudentReport> sourceReports{
        {
            decomposedKorean,
            reportData,
            0
        }
    };
    const auto reports =
        SpeakingEvalReportContentAdapter::toEngine(sourceReports);

    const auto result =
        SpeakingEvalPowerPointJobModel::build(
            reports,
            { QStringLiteral("C:/PowerPointWork/report.pdf") },
            QStringLiteral("C:/PowerPointWork"),
            QStringLiteral("C:/Documents"),
            reportData.signatureImage
            );
    QVERIFY(result);

    const SpeakingEvalPowerPointJobModel::StudentJob& student =
        result->students.constFirst();
    QCOMPARE(student.displayName, composedKorean);
    QCOMPARE(student.koreanName, composedKorean);
    QCOMPARE(student.comments, composedKorean);

    const SpeakingEvalFieldAsset* commentsField =
        speakingEvalFieldAsset(
            SpeakingEvalReportTemplate::Standard,
            QStringLiteral("comments")
            );
    QVERIFY(commentsField != nullptr);
    QCOMPARE(
        student.commentsFontSizePoints,
        speakingEvalFittedFieldFontSize(
            *commentsField,
            student.comments,
            1.0
            )
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
    request.keepIndividualPdfFiles = true;
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
    request.keepIndividualPdfFiles = true;
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
            QDir::Files | QDir::NoDotAndDotDot
            ),
        QStringList()
        );

#ifdef Q_OS_MACOS
    QVERIFY(!QFileInfo::exists(classMngrPowerPointWorkspace()));
    QVERIFY(!QFileInfo::exists(powerPointPrivateWorkspace()));
#endif
}

QTEST_MAIN(SpeakingEvalBatchReportServiceTests)

#include "speaking_eval_batch_report_service_tests.moc"
