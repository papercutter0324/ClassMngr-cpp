#include "next/platform/qt_schedule_output_adapter.h"
#include "core/fontmanager.h"

#include <QFileInfo>
#include <QPageSize>
#include <QPdfDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

struct PrintObservation final
{
    int factoryCalls = 0;
    int readyDocumentCalls = 0;
    int localDocumentCalls = 0;
    bool documentReadyDuringCall = false;
    bool temporaryPdfExistsDuringCall = false;
    QString documentPath;
    QString dialogTitle;
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    QPageLayout::Orientation pdfOrientation = QPageLayout::Portrait;
    int currentPageIndex = -1;
    bool fitToPageByDefault = true;
    std::optional<QPageSize::PageSizeId> preferredPageSize;
    bool lockPreferredPageSize = false;
    QFont uiFont;
    Platform::PdfPrintResult result{
        Platform::PdfPrintStatus::Sent,
        QStringLiteral("Fake print sent.")
    };
};

class FakePdfPrintOperation final : public Platform::PdfPrintOperation
{
public:
    explicit FakePdfPrintOperation(
        std::shared_ptr<PrintObservation> observation
        )
        : m_observation(std::move(observation))
    {
    }

    [[nodiscard]] Platform::PdfPrintResult printReadyDocument(
        const Platform::ReadyPdfPrintRequest& request
        ) override
    {
        ++m_observation->readyDocumentCalls;
        m_observation->documentPath = request.documentPath;
        m_observation->dialogTitle = request.dialogTitle;
        m_observation->orientation = request.pageOrientation;
        m_observation->currentPageIndex = request.currentPageIndex;
        m_observation->fitToPageByDefault = request.fitToPageByDefault;
        m_observation->preferredPageSize = request.preferredPageSize;
        m_observation->lockPreferredPageSize = request.lockPreferredPageSize;
        m_observation->uiFont = request.uiFont;
        m_observation->temporaryPdfExistsDuringCall =
            QFileInfo::exists(request.documentPath);
        if (request.document)
        {
            m_observation->documentReadyDuringCall =
                request.document->status() == QPdfDocument::Status::Ready
                && request.document->pageCount() == 1;
            const QSizeF pageSize = request.document->pagePointSize(0);
            m_observation->pdfOrientation = pageSize.width() > pageSize.height()
                ? QPageLayout::Landscape
                : QPageLayout::Portrait;
        }
        return m_observation->result;
    }

    [[nodiscard]] Platform::PdfPrintResult printLocalDocuments(
        const Platform::LocalPdfBatchPrintRequest& request
        ) override
    {
        Q_UNUSED(request);
        ++m_observation->localDocumentCalls;
        return m_observation->result;
    }

private:
    std::shared_ptr<PrintObservation> m_observation;
};

SchedulePrintService::Document sampleDocument()
{
    SchedulePrintService::Document document;
    document.days = {QStringLiteral("Monday")};
    document.summary = {1, 1};

    SchedulePrintService::Entry entry;
    entry.className = QStringLiteral("Reading Club");
    entry.teacherKoreanName = QStringLiteral("김선생");
    entry.teacherEnglishName = QStringLiteral("Ms Han");
    entry.teacherPreferredName = QStringLiteral("Sam");
    entry.room = QStringLiteral("Studio");
    entry.grade = QStringLiteral("E5");
    entry.level = QStringLiteral("Reading");
    entry.classColor = QStringLiteral("#C9D8A6");
    entry.fontColor = QStringLiteral("#102030");

    SchedulePrintService::Cell cell;
    cell.day = QStringLiteral("Monday");
    cell.timeLabel = QStringLiteral("10:00");
    cell.entries = {entry};

    SchedulePrintService::Row row;
    row.timeLabel = QStringLiteral("10:00");
    row.timeRangeLabel = QStringLiteral("10:00 - 10:50 AM");
    row.cells = {cell};
    document.rows = {row};
    return document;
}

Ui::ScheduleOutputRequest requestFor(
    Ui::ScheduleOutputKind kind,
    const QString& path = {}
    )
{
    Ui::ScheduleOutputRequest request;
    request.document = sampleDocument();
    request.kind = kind;
    request.style = SchedulePrintStyle::Excel;
    request.currentTheme = Theme::Light;
    request.englishFont = FontManager::getUiFont();
    request.koreanFont = FontManager::getKoreanFont();
    request.profileName = QStringLiteral("Alice");
    request.showEnglishNames = true;
    request.orientation = QPageLayout::Landscape;
    request.documentPath = path;
    return request;
}

Platform::PdfPrintOperationFactory fakeFactory(
    const std::shared_ptr<PrintObservation>& observation
    )
{
    return [observation]() {
        ++observation->factoryCalls;
        return std::make_unique<FakePdfPrintOperation>(observation);
    };
}

} // namespace

class QtScheduleOutputAdapterTests final : public QObject
{
    Q_OBJECT

private slots:
    void printBorrowsReadyTemporaryPdfWithScheduleOptions();
    void saveAsPdfDoesNotCreatePrintOperation();
    void printCancellationAndFailureAreMapped();
};

void QtScheduleOutputAdapterTests::
printBorrowsReadyTemporaryPdfWithScheduleOptions()
{
    const auto observation = std::make_shared<PrintObservation>();
    QWidget parent;
    const QFont expectedUiFont = FontManager::getUiFont();

    const Ui::ScheduleOutputResult result =
        Platform::performScheduleOutput(
            requestFor(Ui::ScheduleOutputKind::Print),
            &parent,
            fakeFactory(observation)
            );

    QCOMPARE(result.status, Ui::ScheduleOutputResult::Status::Sent);
    QCOMPARE(result.message, QStringLiteral("Fake print sent."));
    QCOMPARE(observation->factoryCalls, 1);
    QCOMPARE(observation->readyDocumentCalls, 1);
    QCOMPARE(observation->localDocumentCalls, 0);
    QVERIFY(observation->documentReadyDuringCall);
    QVERIFY(observation->temporaryPdfExistsDuringCall);
    QCOMPARE(observation->currentPageIndex, 0);
    QCOMPARE(observation->dialogTitle, QStringLiteral("Print Schedule"));
    QCOMPARE(observation->orientation, QPageLayout::Landscape);
    QCOMPARE(observation->pdfOrientation, QPageLayout::Landscape);
    QVERIFY(!observation->fitToPageByDefault);
    QVERIFY(observation->preferredPageSize.has_value());
    QCOMPARE(*observation->preferredPageSize, QPageSize::A4);
    QVERIFY(observation->lockPreferredPageSize);
    QCOMPARE(observation->uiFont, expectedUiFont);
    QVERIFY(!observation->documentPath.isEmpty());
    QVERIFY(!QFileInfo::exists(observation->documentPath));
    QVERIFY(!QFileInfo::exists(QFileInfo(observation->documentPath).absolutePath()));
}

void QtScheduleOutputAdapterTests::saveAsPdfDoesNotCreatePrintOperation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(
        QStringLiteral("schedule.pdf"));
    const auto observation = std::make_shared<PrintObservation>();

    const Ui::ScheduleOutputResult result =
        Platform::performScheduleOutput(
            requestFor(Ui::ScheduleOutputKind::SaveAsPdf, path),
            nullptr,
            fakeFactory(observation)
            );

    QCOMPARE(result.status, Ui::ScheduleOutputResult::Status::Sent);
    QCOMPARE(observation->factoryCalls, 0);
    QCOMPARE(observation->readyDocumentCalls, 0);
    QCOMPARE(observation->localDocumentCalls, 0);
    QVERIFY(QFileInfo::exists(path));

    QPdfDocument document;
    QCOMPARE(document.load(path), QPdfDocument::Error::None);
    QCOMPARE(document.status(), QPdfDocument::Status::Ready);
    QCOMPARE(document.pageCount(), 1);
    const QString text = document.getAllText(0).text();
    QVERIFY2(text.contains(QStringLiteral("Alice")), qPrintable(text));
    QVERIFY(text.contains(QStringLiteral("Sam")));
    QVERIFY(text.contains(QStringLiteral("Reading")));
    const QSizeF pageSize = document.pagePointSize(0);
    QVERIFY(pageSize.width() > pageSize.height());
}

void QtScheduleOutputAdapterTests::printCancellationAndFailureAreMapped()
{
    for (const auto [printStatus, outputStatus] : {
             std::pair{
                 Platform::PdfPrintStatus::Canceled,
                 Ui::ScheduleOutputResult::Status::Canceled
             },
             std::pair{
                 Platform::PdfPrintStatus::Failed,
                 Ui::ScheduleOutputResult::Status::Failed
             }
         })
    {
        const auto observation = std::make_shared<PrintObservation>();
        observation->result = {
            printStatus,
            QStringLiteral("Fake print result.")
        };

        const Ui::ScheduleOutputResult result =
            Platform::performScheduleOutput(
                requestFor(Ui::ScheduleOutputKind::Print),
                nullptr,
                fakeFactory(observation)
                );

        QCOMPARE(result.status, outputStatus);
        QCOMPARE(
            result.message,
            outputStatus == Ui::ScheduleOutputResult::Status::Canceled
                ? QString()
                : QStringLiteral("Fake print result.")
            );
        QCOMPARE(observation->readyDocumentCalls, 1);
        QVERIFY(!QFileInfo::exists(observation->documentPath));
    }
}

QTEST_MAIN(QtScheduleOutputAdapterTests)

#include "qt_schedule_output_adapter_tests.moc"
