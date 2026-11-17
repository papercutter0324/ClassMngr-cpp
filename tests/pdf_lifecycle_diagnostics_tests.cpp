#include "core/process_memory_diagnostics.h"
#include "core/qt_pdf_generation_adapter.h"
#include "next/platform/roster_pdf_preview_session.h"
#include "ui/shared/printing/pdf_print_service_internal.h"

#include <QBuffer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPdfDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <stdexcept>

namespace
{
class FixedMemory final : public ProcessMemorySnapshotProvider
{
public:
    ProcessMemorySnapshot snapshot() const override
    {
        ProcessMemorySnapshot memory;
        memory.platform = QStringLiteral("windows");
        memory.isAvailable = true;
        memory.workingSetBytes = 4096;
        memory.workingSetBytesAvailable = true;
        memory.handleCountAvailable = true;
        memory.capturedAt = QDateTime::fromString(
            QStringLiteral("2026-11-17T12:00:00.000Z"), Qt::ISODateWithMs);
        return memory;
    }
};

QVector<QJsonObject>* capturedRecords = nullptr;
void captureMessage(QtMsgType, const QMessageLogContext&, const QString& message)
{
    const QString prefix = QStringLiteral("ClassMngr lifecycle checkpoint=");
    if (capturedRecords && message.startsWith(prefix))
    {
        capturedRecords->append(QJsonDocument::fromJson(
            message.mid(prefix.size()).toUtf8()).object());
    }
}
class Capture final
{
public:
    explicit Capture(QVector<QJsonObject>& records)
    {
        capturedRecords = &records;
        previous = qInstallMessageHandler(captureMessage);
    }
    ~Capture()
    {
        qInstallMessageHandler(previous);
        capturedRecords = nullptr;
    }
private:
    QtMessageHandler previous;
};
}

class PdfLifecycleDiagnosticsTests final : public QObject
{
    Q_OBJECT
private slots:
    void serializationPreservesMetricMeaningAndAvailability();
    void writerReleaseFinalizesThePdfAndRecordsTheObjectLifetime();
    void writerPageDetailsAreBoundedWhileTheSummaryCountsEveryPage();
    void previewRenderAndPrintReleaseTheirDocuments();
};

void PdfLifecycleDiagnosticsTests::serializationPreservesMetricMeaningAndAvailability()
{
    auto memory = FixedMemory().snapshot();
    const auto record = processMemoryDiagnosticObject(
        QStringLiteral("probe"), QStringLiteral("test"), 42, {}, memory);
    QCOMPARE(QJsonDocument(record).toJson(), QJsonDocument(processMemoryDiagnosticObject(
        QStringLiteral("probe"), QStringLiteral("test"), 42, {}, memory)).toJson());
    QCOMPARE(record.value(QStringLiteral("instance_id")).toInteger(), 42);
    auto metrics = record.value(QStringLiteral("memory")).toObject();
    QCOMPARE(metrics.value(QStringLiteral("working_set_bytes")).toInteger(), 4096);
    QCOMPARE(metrics.value(QStringLiteral("handle_count")).toInteger(), 0);
    QVERIFY(metrics.value(QStringLiteral("private_usage_bytes")).isNull());
    QVERIFY(!metrics.contains(QStringLiteral("rss_bytes")));

    memory.platform = QStringLiteral("macos");
    metrics = processMemoryDiagnosticObject({}, {}, 1, {}, memory)
        .value(QStringLiteral("memory")).toObject();
    QCOMPARE(metrics.value(QStringLiteral("resident_bytes")).toInteger(), 4096);
    QVERIFY(!metrics.contains(QStringLiteral("working_set_bytes")));
    QVERIFY(metrics.value(QStringLiteral("physical_footprint_bytes")).isNull());
    memory.platform = QStringLiteral("linux");
    metrics = processMemoryDiagnosticObject({}, {}, 1, {}, memory)
        .value(QStringLiteral("memory")).toObject();
    QCOMPARE(metrics.value(QStringLiteral("rss_bytes")).toInteger(), 4096);
    QVERIFY(metrics.value(QStringLiteral("pss_bytes")).isNull());

    ProcessMemoryDiagnostics throwing(std::make_unique<FixedMemory>(),
        [](const QByteArray&) { throw std::runtime_error("writer failure"); });
    throwing.record(QStringLiteral("probe"), QStringLiteral("test"), 1);
}

void PdfLifecycleDiagnosticsTests::writerReleaseFinalizesThePdfAndRecordsTheObjectLifetime()
{
    QVector<QJsonObject> events;
    auto diagnostics = std::make_shared<ProcessMemoryDiagnostics>(
        std::make_unique<FixedMemory>(), [&](const QByteArray& bytes) {
            events.append(QJsonDocument::fromJson(bytes).object());
        });
    const auto port = makeQtPdfGenerationPort(diagnostics);
    QByteArray bytes;
    QBuffer output(&bytes);
    QVERIFY(output.open(QIODevice::ReadWrite));
    auto session = port->create(output, {});
    QVERIFY(session);
    QPainter painter(&session->paintDevice());
    QVERIFY(painter.isActive());
    painter.drawText(QPoint(30, 30), QStringLiteral("first page"));
    QVERIFY(session->newPage());
    painter.drawText(QPoint(30, 30), QStringLiteral("second page"));
    painter.end();
    session.reset();
    QStringList names;
    for (const auto& event : events) names.append(event.value(QStringLiteral("event")).toString());
    QCOMPARE(names, QStringList({QStringLiteral("pdf-writer-create"),
        QStringLiteral("pdf-writer-new-page"), QStringLiteral("pdf-writer-release-before"),
        QStringLiteral("pdf-writer-release-after")}));
    const auto firstId = events.front().value(QStringLiteral("instance_id")).toInteger();
    for (const auto& event : events)
    {
        QCOMPARE(event.value(QStringLiteral("instance_id")).toInteger(), firstId);
        QVERIFY(!event.value(QStringLiteral("memory")).toObject()
            .value(QStringLiteral("working_set_bytes")).isNull());
    }
    const auto end = events.back().value(QStringLiteral("details")).toObject();
    QVERIFY(!end.value(QStringLiteral("writer_present")).toBool());
    QCOMPARE(end.value(QStringLiteral("successful_new_page_count")).toInt(), 1);
    QVERIFY(output.seek(0));
    QPdfDocument document;
    document.load(&output);
    QCOMPARE(document.status(), QPdfDocument::Status::Ready);
    QCOMPARE(document.pageCount(), 2);
    document.close();
}

void PdfLifecycleDiagnosticsTests::previewRenderAndPrintReleaseTheirDocuments()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QVector<QJsonObject> events;
    Capture capture(events);
    auto preview = ClassMngr::Next::Platform::makeQtRosterPdfPreviewSession();
    QVERIFY(preview->isValid());
    const QString path = preview->path();
    auto writer = makeQtPdfGenerationPort()->create(path, {});
    QVERIFY(writer);
    QPainter painter(&writer->paintDevice());
    QVERIFY(painter.isActive());
    painter.drawText(QPoint(20, 20), QStringLiteral("Lifecycle fixture"));
    for (int page = 1; page < 18; ++page)
    {
        QVERIFY(writer->newPage());
        painter.drawText(QPoint(20, 20), QStringLiteral("Lifecycle fixture"));
    }
    painter.end();
    writer.reset();
    QVERIFY(preview->loadGeneratedPdf());
    QCOMPARE(preview->pageCount(), 18);
    QImage image = preview->renderPage(0, QSize(320, 240));
    QVERIFY(!image.isNull());
    QVERIFY(image.width() <= 320 && image.height() <= 240);
    image = {};
    for (int index = 1; index < 35; ++index)
        QVERIFY(!preview->renderPage(0, QSize(64, 48)).isNull());

    PdfPrintService::Internal::BatchPrintTestHooks hooks;
    hooks.executeDialog = [&](QPrintDialog&, QPrinter& printer) {
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setResolution(72);
        printer.setOutputFileName(temporary.filePath(QStringLiteral("private-output-name.pdf")));
        return static_cast<int>(QDialog::Accepted);
    };
    const auto result = PdfPrintService::Internal::printPdfDocumentsWithHooks({
        .documentPaths = {path}, .dialogTitle = QStringLiteral("private-title")}, hooks);
    QCOMPARE(result.status, PdfPrintService::Status::Sent);
    preview->release();
    preview.reset();

    QStringList names;
    int loaded = 0;
    int released = 0;
    bool renderedPreview = false;
    bool renderedPrint = false;
    bool printImageReleased = false;
    int previewRenderDetails = 0;
    int printRenderDetails = 0;
    bool boundedPrintSummary = false;
    bool boundedPreviewSummary = false;
    for (const auto& event : events)
    {
        const QString name = event.value(QStringLiteral("event")).toString();
        names.append(name);
        const auto details = event.value(QStringLiteral("details")).toObject();
        if (name == QStringLiteral("print-document-load-before")) ++loaded;
        if (name == QStringLiteral("print-document-release"))
        {
            ++released;
            QVERIFY(!details.value(QStringLiteral("document_present")).toBool());
        }
        if (name == QStringLiteral("qpdf-preview-render")
            && details.value(QStringLiteral("phase")) == QStringLiteral("after"))
        {
            ++previewRenderDetails;
            renderedPreview = details.value(QStringLiteral("decoded_pixel_bytes")).toInteger() > 0;
        }
        if (name == QStringLiteral("qpdf-print-render")
            && details.value(QStringLiteral("phase")) == QStringLiteral("after"))
        {
            ++printRenderDetails;
            renderedPrint = details.value(QStringLiteral("decoded_pixel_bytes")).toInteger() > 0;
        }
        if ((name == QStringLiteral("qpdf-preview-render") || name == QStringLiteral("qpdf-print-render"))
            && details.value(QStringLiteral("phase")) == QStringLiteral("before"))
            QVERIFY(details.value(QStringLiteral("decoded_pixel_bytes")).isNull());
        if (name == QStringLiteral("qpdf-print-render-summary"))
            boundedPrintSummary = details.value(QStringLiteral("render_count")).toInt() == 18
                && details.value(QStringLiteral("page_details_suppressed")).toInt() == 2;
        if (name == QStringLiteral("preview-close-after"))
            boundedPreviewSummary = details.value(QStringLiteral("render_count")).toInt() == 35
                && details.value(QStringLiteral("render_details_suppressed")).toInt() == 3;
        if (name == QStringLiteral("qpdf-print-render")
            && details.value(QStringLiteral("phase")) == QStringLiteral("image-released"))
            printImageReleased = !details.value(QStringLiteral("image_present")).toBool();
        const QByteArray serialized = QJsonDocument(event).toJson();
        QVERIFY(!serialized.contains(path.toUtf8()));
        QVERIFY(!serialized.contains("private-output-name"));
        QVERIFY(!serialized.contains("private-title"));
    }
    QVERIFY(renderedPreview);
    QVERIFY(renderedPrint);
    QVERIFY(printImageReleased);
    QCOMPARE(previewRenderDetails, 32);
    QCOMPARE(printRenderDetails, 16);
    QVERIFY(boundedPrintSummary);
    QVERIFY(boundedPreviewSummary);
    QVERIFY(loaded >= 2);
    QCOMPARE(released, loaded);
    QVERIFY(names.contains(QStringLiteral("preview-document-destroyed")));
}

void PdfLifecycleDiagnosticsTests::writerPageDetailsAreBoundedWhileTheSummaryCountsEveryPage()
{
    QVector<QJsonObject> events;
    auto diagnostics = std::make_shared<ProcessMemoryDiagnostics>(
        std::make_unique<FixedMemory>(), [&](const QByteArray& bytes) {
            events.append(QJsonDocument::fromJson(bytes).object());
        });
    QByteArray bytes;
    QBuffer output(&bytes);
    QVERIFY(output.open(QIODevice::WriteOnly));
    auto writer = makeQtPdfGenerationPort(diagnostics)->create(output, {});
    QVERIFY(writer);
    QPainter painter(&writer->paintDevice());
    QVERIFY(painter.isActive());
    for (int page = 0; page < 36; ++page)
    {
        painter.drawLine(10, 10, 20, 20);
        QVERIFY(writer->newPage());
    }
    painter.end();
    writer.reset();
    QCOMPARE(events.size(), 35); // create + first 32 pages + before/after release
    const auto summary = events.back().value(QStringLiteral("details")).toObject();
    QCOMPARE(summary.value(QStringLiteral("new_page_attempt_count")).toInt(), 36);
    QCOMPARE(summary.value(QStringLiteral("successful_new_page_count")).toInt(), 36);
    QCOMPARE(summary.value(QStringLiteral("new_page_details_suppressed")).toInt(), 4);
}

QTEST_MAIN(PdfLifecycleDiagnosticsTests)
#include "pdf_lifecycle_diagnostics_tests.moc"
