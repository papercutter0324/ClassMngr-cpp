#include "next/platform/pdf_print_operation.h"

#include <QApplication>
#include <QFileInfo>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrinterInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <QtGlobal>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

class NextPlatformLinuxPdfPrintOperationAdapterTests final : public QObject
{
    Q_OBJECT

private slots:
    void cancelsOnePagePdfWithoutSubmittingAJob();
};

void NextPlatformLinuxPdfPrintOperationAdapterTests::
cancelsOnePagePdfWithoutSubmittingAJob()
{
#if !defined(Q_OS_LINUX)
    QSKIP("The system-dialog adapter test covers Linux behavior only.");
#else
    if (QPrinterInfo::availablePrinterNames().isEmpty())
    {
        QSKIP("A configured printer is required to open the Linux print dialog.");
    }

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString documentPath = temporaryDirectory.filePath(
        QStringLiteral("one-page.pdf")
        );
    {
        QPdfWriter writer(documentPath);
        QPainter painter(&writer);
        QVERIFY(painter.isActive());
        painter.drawText(QPoint(24, 36), QStringLiteral("Print adapter test"));
        QVERIFY(painter.end());
    }
    QVERIFY(QFileInfo::exists(documentPath));

    QPdfDocument document;
    QCOMPARE(document.load(documentPath), QPdfDocument::Error::None);
    QCOMPARE(document.status(), QPdfDocument::Status::Ready);
    QCOMPARE(document.pageCount(), 1);
    document.close();

    const QString wouldBeSubmittedJobPath = temporaryDirectory.filePath(
        QStringLiteral("must-not-be-submitted.pdf")
        );
    bool printDialogObserved = false;
    bool dialogPrinterObserved = false;
    QTimer cancelDialogTimer;
    cancelDialogTimer.setInterval(10);
    connect(
        &cancelDialogTimer,
        &QTimer::timeout,
        this,
        [&]() {
            for (QWidget* const widget : QApplication::topLevelWidgets())
            {
                auto* const dialog = qobject_cast<QPrintDialog*>(widget);
                if (!dialog)
                {
                    continue;
                }

                printDialogObserved = true;
                if (QPrinter* const printer = dialog->printer())
                {
                    dialogPrinterObserved = true;
                    printer->setOutputFormat(QPrinter::PdfFormat);
                    printer->setOutputFileName(wouldBeSubmittedJobPath);
                }
                dialog->reject();
                cancelDialogTimer.stop();
                return;
            }
        }
        );
    cancelDialogTimer.start();

    QWidget parent;
    std::unique_ptr<Platform::PdfPrintOperation> operation =
        Platform::makeQtPdfPrintOperation();
    const Platform::PdfPrintResult result = operation->printLocalDocuments({
        .parent = &parent,
        .documentPaths = {documentPath},
        .dialogTitle = QStringLiteral("Cancel adapter print"),
        .fitToPage = true
    });
    cancelDialogTimer.stop();

    QVERIFY(printDialogObserved);
    QVERIFY(dialogPrinterObserved);
    QCOMPARE(result.status, Platform::PdfPrintStatus::Canceled);
    QVERIFY(result.message.isEmpty());
    QVERIFY(!QFileInfo::exists(wouldBeSubmittedJobPath));
#endif
}

QTEST_MAIN(NextPlatformLinuxPdfPrintOperationAdapterTests)

#include "next_platform_linux_pdf_print_operation_adapter_tests.moc"
