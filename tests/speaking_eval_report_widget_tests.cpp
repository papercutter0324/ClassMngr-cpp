#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <QtTest>

#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QPlainTextEdit>

class SpeakingEvalReportWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void regularTemplateUsesPortraitSize();
    void regularTemplateMatchesReferenceAndHighlightsScores();
    void advancedTemplateUsesPortraitSizeAndEmbeddedLogo();
    void interactiveTemplateEditsScoresAndComments();
    void signatureUsesTemplateBoundsAndKeepsAspectRatio();
};

void SpeakingEvalReportWidgetTests::regularTemplateUsesPortraitSize()
{
    SpeakingEvalReportWidget report;

    QCOMPARE(
        report.sizeHint(),
        QSize(810, 1170)
        );
    QVERIFY(!report.usesAdvancedTemplate());
}

void SpeakingEvalReportWidgetTests::interactiveTemplateEditsScoresAndComments()
{
    SpeakingEvalReportWidget report;
    report.setInteractive(true);

    QSignalSpy scoreSpy(
        &report,
        &SpeakingEvalReportWidget::scoreEdited
        );
    const QPoint grammarAPlusCell(
        qRound((35.0263 + 74.25504 + 39.288935) * 1.5),
        qRound((125.0386 + 11.4) * 1.5)
        );

    QTest::mouseClick(
        &report,
        Qt::LeftButton,
        Qt::NoModifier,
        grammarAPlusCell
        );
    QCOMPARE(scoreSpy.size(), 1);
    QCOMPARE(scoreSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(scoreSpy.at(0).at(1).toString(), QStringLiteral("A+"));

    QTest::mouseClick(
        &report,
        Qt::LeftButton,
        Qt::NoModifier,
        grammarAPlusCell
        );
    QCOMPARE(scoreSpy.size(), 2);
    QCOMPARE(scoreSpy.at(1).at(1).toString(), QString());

    SpeakingEvalReportData advancedData;
    advancedData.reportTemplate =
        SpeakingEvalReportTemplate::Advanced;
    report.setReportData(advancedData);
    const QPoint advancedGrammarAPlusCell(
        qRound((17.28 + 134.97 + 36.9) * 1.5),
        qRound((167.27 + 10.8) * 1.5)
        );
    QTest::mouseClick(
        &report,
        Qt::LeftButton,
        Qt::NoModifier,
        advancedGrammarAPlusCell
        );
    QCOMPARE(scoreSpy.size(), 3);
    QCOMPARE(scoreSpy.at(2).at(0).toInt(), 0);
    QCOMPARE(scoreSpy.at(2).at(1).toString(), QStringLiteral("A+"));

    auto* comments =
        report.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalReportComments")
            );
    QVERIFY(comments);
    QVERIFY(!comments->isHidden());

    QSignalSpy commentsSpy(
        &report,
        &SpeakingEvalReportWidget::commentsEdited
        );
    comments->setPlainText(QStringLiteral("A freshly typed comment."));
    QCOMPARE(commentsSpy.size(), 1);
    QCOMPARE(
        commentsSpy.at(0).at(0).toString(),
        QStringLiteral("A freshly typed comment.")
        );
}

void SpeakingEvalReportWidgetTests::regularTemplateMatchesReferenceAndHighlightsScores()
{
    SpeakingEvalReportWidget report;
    SpeakingEvalReportData data;

    data.englishName = QStringLiteral("Gildong");
    data.koreanName = QStringLiteral("\uD64D\uAE38\uB3D9");
    data.classLabel = QStringLiteral("E6 Gaia");
    data.nativeTeacher = QStringLiteral("Aristotle");
    data.koreanTeacher = QStringLiteral("\uC1A1\uC624\uD604");
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
    report.setReportData(data);

    QImage image(
        QSize(540, 780),
        QImage::Format_ARGB32_Premultiplied
        );
    image.fill(Qt::white);

    QPainter painter(&image);
    report.paintReport(
        &painter,
        QRectF(image.rect())
        );
    painter.end();

    const QString previewPath =
        qEnvironmentVariable("CLASSMNGR_REGULAR_REPORT_PREVIEW_PATH");

    if (!previewPath.isEmpty())
    {
        QVERIFY(image.save(previewPath));
    }

    int logoRedPixels = 0;

    for (int y = 27; y < 80; ++y)
    {
        for (int x = 29; x < 92; ++x)
        {
            const QColor color =
                image.pixelColor(x, y);

            if (
                color.red() > 120
                && color.red() > color.green() * 2
                && color.red() > color.blue() * 2
                )
            {
                ++logoRedPixels;
            }
        }
    }

    QVERIFY2(
        logoRedPixels > 75,
        "The standard report did not render the embedded DYB PNG."
        );

    const QColor selectedGrammarScore =
        image.pixelColor(115, 130);
    const QColor unselectedGrammarScore =
        image.pixelColor(195, 130);
    const QColor selectedPronunciationScore =
        image.pixelColor(195, 211);

    for (const QColor& selected : {
             selectedGrammarScore,
             selectedPronunciationScore
             })
    {
        QVERIFY2(
            selected.red() > 240
                && selected.green() > 240
                && selected.blue() < 20,
            "An imported standard score is not highlighted yellow."
            );
    }

    QVERIFY2(
        unselectedGrammarScore.red() > 205
            && unselectedGrammarScore.red() < 230
            && unselectedGrammarScore.green() > 205
            && unselectedGrammarScore.green() < 230
            && unselectedGrammarScore.blue() > 205
            && unselectedGrammarScore.blue() < 230,
        "An unselected standard score does not retain the reference grey."
        );

    const QColor criteriaShade =
        image.pixelColor(40, 130);

    QVERIFY2(
        criteriaShade.red() > 180
            && criteriaShade.red() < 200
            && criteriaShade.green() > 180
            && criteriaShade.green() < 200
            && criteriaShade.blue() > 180
            && criteriaShade.blue() < 200,
        "The criteria column does not use the reference #BFBFBF shade."
        );

    const QColor finalTableCell =
        image.pixelColor(501, 510);
    const QColor outsideTable =
        image.pixelColor(504, 510);
    const QColor finalEffortCell =
        image.pixelColor(50, 571);
    const QColor belowTable =
        image.pixelColor(50, 574);

    QVERIFY2(
        finalTableCell.red() < 250,
        "The standard table is narrower than the PowerPoint reference."
        );
    QVERIFY2(
        outsideTable.red() > 250
            && outsideTable.green() > 250
            && outsideTable.blue() > 250,
        "The standard table exceeds the PowerPoint reference width."
        );
    QVERIFY2(
        finalEffortCell.red() > 180
            && finalEffortCell.red() < 200,
        "The standard table ends above the PowerPoint reference position."
        );
    QVERIFY2(
        belowTable.red() > 245
            && belowTable.green() > 245
            && belowTable.blue() > 245,
        "The standard table extends below the PowerPoint reference position."
        );
}

void SpeakingEvalReportWidgetTests::advancedTemplateUsesPortraitSizeAndEmbeddedLogo()
{
    SpeakingEvalReportWidget report;
    SpeakingEvalReportData data;

    data.englishName = QStringLiteral("Athena Student");
    data.koreanName = QStringLiteral("\uD64D\uAE38\uB3D9");
    data.classLabel = QStringLiteral("E5 Athena");
    data.nativeTeacher = QStringLiteral("Teacher");
    data.koreanTeacher = QStringLiteral("\uC120\uC0DD\uB2D8");
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
    data.reportTemplate =
        SpeakingEvalReportTemplate::Advanced;
    report.setReportData(data);

    QCOMPARE(
        report.sizeHint(),
        QSize(810, 1170)
        );
    QVERIFY(report.usesAdvancedTemplate());

    QImage image(
        QSize(540, 780),
        QImage::Format_ARGB32_Premultiplied
        );
    image.fill(Qt::white);

    QPainter painter(&image);
    report.paintReport(
        &painter,
        QRectF(image.rect())
        );
    painter.end();

    const QString previewPath =
        qEnvironmentVariable("CLASSMNGR_REPORT_PREVIEW_PATH");

    if (!previewPath.isEmpty())
    {
        QVERIFY(image.save(previewPath));
    }

    int logoRedPixels = 0;

    for (int y = 6; y < 75; ++y)
    {
        for (int x = 7; x < 103; ++x)
        {
            const QColor color =
                image.pixelColor(x, y);

            if (
                color.red() > 120
                && color.red() > color.green() * 2
                && color.red() > color.blue() * 2
                )
            {
                ++logoRedPixels;
            }
        }
    }

    QVERIFY2(
        logoRedPixels > 100,
        "The advanced report did not render the embedded DYB PNG."
        );

    const QColor overallGradeTop =
        image.pixelColor(500, 103);
    const QColor overallGradeBottom =
        image.pixelColor(500, 156);
    const QColor belowOverallGrade =
        image.pixelColor(500, 158);

    QVERIFY2(
        overallGradeTop.red() > 210
            && overallGradeTop.green() > 210
            && overallGradeTop.blue() > 210,
        "The overall-grade box is not top-aligned with the Korean-name box."
        );
    QVERIFY2(
        overallGradeBottom.red() > 210
            && overallGradeBottom.green() > 210
            && overallGradeBottom.blue() > 210,
        "The overall-grade box is not bottom-aligned with the Korean-teacher box."
        );
    QVERIFY2(
        belowOverallGrade.red() > 245
            && belowOverallGrade.green() > 245
            && belowOverallGrade.blue() > 245,
        "The overall-grade box extends below the Korean-teacher box."
        );

    int overallGradeScorePixels = 0;

    for (int y = 108; y <= 137; ++y)
    {
        for (int x = 440; x <= 495; ++x)
        {
            const QColor color =
                image.pixelColor(x, y);

            if (
                color.red() < 80
                && color.green() < 80
                && color.blue() < 80
                )
            {
                ++overallGradeScorePixels;
            }
        }
    }

    QVERIFY2(
        overallGradeScorePixels > 120,
        "The overall-grade score does not use the reference 24-point Arial Black styling."
        );

    int studentValuePixels = 0;

    for (const QRect& valueRect : {
             QRect(102, 105, 128, 21),
             QRect(332, 105, 73, 21),
             QRect(102, 135, 128, 21),
             QRect(332, 135, 73, 21)
             })
    {
        for (int y = valueRect.top(); y <= valueRect.bottom(); ++y)
        {
            for (int x = valueRect.left(); x <= valueRect.right(); ++x)
            {
                const QColor color =
                    image.pixelColor(x, y);

                if (
                    color.red() < 80
                    && color.green() < 80
                    && color.blue() < 80
                    )
                {
                    ++studentValuePixels;
                }
            }
        }
    }

    QVERIFY2(
        studentValuePixels > 350,
        "The name and teacher values do not use the reference 14-point Arial styling."
        );

    const QColor selectedGradeCell =
        image.pixelColor(157, 171);

    QVERIFY2(
        selectedGradeCell.red() > 220
            && selectedGradeCell.green() > 200
            && selectedGradeCell.blue() < 30,
        "The selected grade cell is not highlighted yellow."
        );

    const QColor pronunciationBody =
        image.pixelColor(50, 331);
    const QColor fluencyHeader =
        image.pixelColor(50, 334);

    QVERIFY2(
        pronunciationBody.red() > 210
            && pronunciationBody.green() > 210
            && pronunciationBody.blue() > 210,
        "The pronunciation row does not use the reference height."
        );
    QVERIFY2(
        fluencyHeader.red() > 120
            && fluencyHeader.green() < 40
            && fluencyHeader.blue() < 40,
        "The fluency header does not begin at the reference position."
        );

    const QColor lastTableColumn =
        image.pixelColor(521, 171);
    const QColor outsideTable =
        image.pixelColor(524, 171);

    QVERIFY2(
        lastTableColumn.red() > 210
            && lastTableColumn.green() > 210
            && lastTableColumn.blue() > 210,
        "The reference table's final column is too narrow."
        );
    QVERIFY2(
        outsideTable.red() > 245
            && outsideTable.green() > 245
            && outsideTable.blue() > 245,
        "The advanced table exceeds the reference width."
        );

    const QColor effortBody =
        image.pixelColor(50, 631);
    const QColor commentsHeader =
        image.pixelColor(50, 634);

    QVERIFY2(
        effortBody.red() > 210
            && effortBody.green() > 210
            && effortBody.blue() > 210,
        "The overall-effort row does not use the reference height."
        );
    QVERIFY2(
        commentsHeader.red() > 120
            && commentsHeader.green() < 40
            && commentsHeader.blue() < 40,
        "The comments section does not follow the table at the reference position."
        );

    int scorePixels = 0;

    for (int y = 205; y < 236; ++y)
    {
        for (int x = 110; x < 145; ++x)
        {
            const QColor color =
                image.pixelColor(x, y);

            if (
                color.red() < 80
                && color.green() < 80
                && color.blue() < 80
                )
            {
                ++scorePixels;
            }
        }
    }

    QVERIFY2(
        scorePixels > 10,
        "The student's score was not rendered in the adjacent criteria cell."
        );
}

void SpeakingEvalReportWidgetTests::
signatureUsesTemplateBoundsAndKeepsAspectRatio()
{
    QImage sourceSignature(
        QSize(400, 100),
        QImage::Format_ARGB32_Premultiplied
        );
    sourceSignature.fill(QColor(0, 255, 0));

    QByteArray signatureData;
    QBuffer signatureBuffer(&signatureData);
    QVERIFY(signatureBuffer.open(QIODevice::WriteOnly));
    QVERIFY(sourceSignature.save(&signatureBuffer, "PNG"));

    for (
        const SpeakingEvalReportTemplate reportTemplate : {
            SpeakingEvalReportTemplate::Standard,
            SpeakingEvalReportTemplate::Advanced
            }
        )
    {
        SpeakingEvalReportData data;
        data.reportTemplate = reportTemplate;
        data.signatureImage = signatureData;

        SpeakingEvalReportWidget report;
        report.setReportData(data);

        QImage rendered(
            QSize(540, 780),
            QImage::Format_ARGB32_Premultiplied
            );
        rendered.fill(Qt::white);
        QPainter painter(&rendered);
        report.paintReport(&painter, QRectF(rendered.rect()));
        painter.end();

        const QRectF configuredBounds =
            speakingEvalReportTemplateLayout(
                reportTemplate
                ).signatureBounds;
        int left = rendered.width();
        int top = rendered.height();
        int right = -1;
        int bottom = -1;
        for (
            int y = qFloor(configuredBounds.top());
            y < qCeil(configuredBounds.bottom());
            ++y
            )
        {
            for (
                int x = qFloor(configuredBounds.left());
                x < qCeil(configuredBounds.right());
                ++x
                )
            {
                const QColor color = rendered.pixelColor(x, y);
                if (
                    color.green() > 240
                    && color.red() < 10
                    && color.blue() < 10
                    )
                {
                    left = qMin(left, x);
                    top = qMin(top, y);
                    right = qMax(right, x);
                    bottom = qMax(bottom, y);
                }
            }
        }

        QVERIFY2(
            right >= left && bottom >= top,
            "The configured signature image was not rendered."
            );
        const int renderedWidth = right - left + 1;
        const int renderedHeight = bottom - top + 1;
        QVERIFY(
            qAbs(
                (static_cast<qreal>(renderedWidth) / renderedHeight)
                - 4.0
                )
            < 0.2
            );
        QVERIFY(
            qAbs(
                (right + 1)
                - qRound(configuredBounds.right())
                )
            <= 1
            );
        QVERIFY(
            qAbs(
                (bottom + 1)
                - qRound(configuredBounds.bottom())
                )
            <= 1
            );
    }
}

QTEST_MAIN(SpeakingEvalReportWidgetTests)

#include "speaking_eval_report_widget_tests.moc"
