#include "features/speaking_eval/services/speaking_eval_internal_pdf_renderer.h"

#include <QtTest>

#include <QGuiApplication>
#include <QImage>
#include <QPdfDocument>
#include <QTemporaryDir>

class SpeakingEvalWidgetlessPdfTests : public QObject
{
    Q_OBJECT

private slots:
    void rendersWithOnlyGuiApplication_data();
    void rendersWithOnlyGuiApplication();
};

void SpeakingEvalWidgetlessPdfTests::rendersWithOnlyGuiApplication_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard") << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced") << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalWidgetlessPdfTests::rendersWithOnlyGuiApplication()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);
    QCOMPARE(QString::fromLatin1(QCoreApplication::instance()->metaObject()->className()),
        QStringLiteral("QGuiApplication"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SpeakingEvalReportData data;
    data.reportTemplate = reportTemplate;
    data.englishName = QStringLiteral("Widgetless Student");
    data.classLabel = QStringLiteral("E5 Athena");
    data.date = QStringLiteral("Oct. 2026");
    data.comments = QStringLiteral("A report rendered with QGuiApplication.");
    data.scores.fill(QStringLiteral("A"));

    const QString path = directory.filePath(QStringLiteral("report.pdf"));
    QString error;
    QVERIFY2(SpeakingEvalInternalPdfRenderer::render(data, path, &error),
        qPrintable(error));
    QPdfDocument document;
    QCOMPARE(document.load(path), QPdfDocument::Error::None);
    QCOMPARE(document.pageCount(), 1);
    const QSizeF points = document.pagePointSize(0);
    QVERIFY(qAbs(points.width() - 540.0) < 1.0);
    QVERIFY(qAbs(points.height() - 780.0) < 1.0);
    const QImage raster = document.render(0, QSize(540, 780));
    QVERIFY(!raster.isNull());
}

int main(int argc, char** argv)
{
    // Constructing any QWidget here is fatal in Qt. Keep this explicit even
    // though the shared test runtime also links the application's Widgets code.
    QGuiApplication application(argc, argv);
    SpeakingEvalWidgetlessPdfTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "speaking_eval_widgetless_pdf_tests.moc"
