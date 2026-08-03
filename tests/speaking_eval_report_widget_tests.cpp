#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"

#include <QtTest>

#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRegion>

namespace
{
constexpr int RasterScale = 2;
const QSize RasterSize(1080, 1560);
const QSize StandardAuthoredBackgroundSize(3603, 5202);
const QSize AdvancedAuthoredBackgroundSize(2882, 4162);

SpeakingEvalReportData representativeData(
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

    data.englishName = QStringLiteral("Test");
    data.koreanName = QStringLiteral("각각각");
    data.classLabel = QStringLiteral("E4 Odysseus");
    data.nativeTeacher = QStringLiteral("Emma");
    data.koreanTeacher = QStringLiteral("홍승현");
    data.date = QStringLiteral("Aug. 2026");
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

QImage renderReport(
    const SpeakingEvalReportData& data,
    const QSize& size = RasterSize
    )
{
    SpeakingEvalReportWidget report;
    report.setReportData(data);

    QImage image(
        size,
        QImage::Format_ARGB32_Premultiplied
        );
    image.fill(Qt::white);
    QPainter painter(&image);
    report.paintReport(&painter, QRectF(QPointF(), size));
    painter.end();
    return image;
}

QImage renderStaticBackground(
    SpeakingEvalReportTemplate reportTemplate,
    const QSize& size = RasterSize
    )
{
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate);
    QImage image(
        size,
        QImage::Format_ARGB32_Premultiplied
        );
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(
        QRectF(QPointF(), size),
        assets.background
        );
    painter.end();
    return image;
}

QRect backgroundRect(
    const SpeakingEvalTemplateAssets& assets,
    const QRectF& logicalRect
    )
{
    const qreal horizontalScale =
        assets.backgroundPixelSize.width()
        / assets.logicalSize.width();
    const qreal verticalScale =
        assets.backgroundPixelSize.height()
        / assets.logicalSize.height();
    return QRectF(
        logicalRect.left() * horizontalScale,
        logicalRect.top() * verticalScale,
        logicalRect.width() * horizontalScale,
        logicalRect.height() * verticalScale
        ).toAlignedRect();
}

QRect exactBackgroundRect(
    const SpeakingEvalTemplateAssets& assets,
    const QRectF& logicalRect
    )
{
    const qreal horizontalScale =
        assets.backgroundPixelSize.width()
        / assets.logicalSize.width();
    const qreal verticalScale =
        assets.backgroundPixelSize.height()
        / assets.logicalSize.height();
    return QRect(
        qRound(logicalRect.left() * horizontalScale),
        qRound(logicalRect.top() * verticalScale),
        qRound(logicalRect.width() * horizontalScale),
        qRound(logicalRect.height() * verticalScale)
        );
}

QRect rasterRect(
    const QRectF& logicalRect
    )
{
    return QRectF(
        logicalRect.topLeft() * RasterScale,
        logicalRect.size() * RasterScale
        ).toAlignedRect();
}

QRegion mutableRegion(
    SpeakingEvalReportTemplate reportTemplate
    )
{
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate);
    QRegion region;

    for (const SpeakingEvalFieldAsset& field : assets.fields)
    {
        // Include the one-pixel coverage fringe produced when PowerPoint
        // rasterizes glyphs that begin exactly on a half-pixel boundary.
        region += rasterRect(field.rect).adjusted(-1, -1, 1, 1);
    }
    for (int metric = 0; metric < assets.scoreCells.size(); ++metric)
    {
        for (const QString& grade : {
                 QStringLiteral("A+"),
                 QStringLiteral("A"),
                 QStringLiteral("B+"),
                 QStringLiteral("B"),
                 QStringLiteral("C")
                 })
        {
            region += rasterRect(
                speakingEvalScoreCell(
                    reportTemplate,
                    metric,
                    grade
                    )
                );
        }
    }
    for (const QRectF& studentGradeCell : assets.studentGradeCells)
    {
        region += rasterRect(studentGradeCell);
    }
    for (const SpeakingEvalSpriteAsset& sprite : assets.overallGrades)
    {
        region += rasterRect(sprite.destination);
    }
    region += rasterRect(assets.signatureBounds);
    return region;
}

struct ImageDifference
{
    qsizetype changedPixels = 0;
    quint64 absoluteError = 0;
    qsizetype exactDifferencesOutsideMutableRegion = 0;
    QPoint firstDifferenceOutsideMutableRegion = QPoint(-1, -1);
};

ImageDifference compareImages(
    const QImage& actualImage,
    const QImage& expectedImage,
    const QRegion& mutablePixels
    )
{
    const QImage actual =
        actualImage.convertToFormat(QImage::Format_ARGB32);
    const QImage expected =
        expectedImage.convertToFormat(QImage::Format_ARGB32);
    ImageDifference result;

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
            result.absoluteError += red + green + blue;
            if (red > 16 || green > 16 || blue > 16)
            {
                ++result.changedPixels;
            }
            if ((red != 0 || green != 0 || blue != 0)
                && !mutablePixels.contains(QPoint(x, y)))
            {
                ++result.exactDifferencesOutsideMutableRegion;
                if (result.firstDifferenceOutsideMutableRegion.x() < 0)
                {
                    result.firstDifferenceOutsideMutableRegion =
                        QPoint(x, y);
                }
            }
        }
    }

    return result;
}

QByteArray signatureImage(
    const QSize& size
    )
{
    QImage signature(
        size,
        QImage::Format_ARGB32_Premultiplied
        );
    signature.fill(QColor(0, 255, 0));

    QByteArray data;
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly)
        || !signature.save(&buffer, "PNG"))
    {
        return {};
    }
    return data;
}
}

class SpeakingEvalReportWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void templateAssetsAreValidAndUseAuthoritativeGeometry();
    void standardMetadataUsesSharedFontAndHalfPointGradeFitting();
    void templateUsesPortraitSize_data();
    void templateUsesPortraitSize();
    void representativeReportPreservesAuthoredBackground_data();
    void representativeReportPreservesAuthoredBackground();
    void dynamicTextNeverEscapesManifestRectangles_data();
    void dynamicTextNeverEscapesManifestRectangles();
    void scoreLabelsAreCenteredInEveryCell_data();
    void scoreLabelsAreCenteredInEveryCell();
    void advancedStudentGradesAreCenteredInTheirMetricCells();
    void everyScoreHighlightAndInvalidScoreAreHandled_data();
    void everyScoreHighlightAndInvalidScoreAreHandled();
    void interactiveTemplateEditsScoresAndComments_data();
    void interactiveTemplateEditsScoresAndComments();
    void signaturesKeepAspectRatioWithinManifestBounds_data();
    void signaturesKeepAspectRatioWithinManifestBounds();
};

void SpeakingEvalReportWidgetTests::
    templateAssetsAreValidAndUseAuthoritativeGeometry()
{
    const SpeakingEvalTemplateAssets& standard =
        speakingEvalTemplateAssets(
            SpeakingEvalReportTemplate::Standard
            );
    const SpeakingEvalTemplateAssets& advanced =
        speakingEvalTemplateAssets(
            SpeakingEvalReportTemplate::Advanced
            );
    QVERIFY2(standard.valid, qPrintable(standard.error));
    QVERIFY2(advanced.valid, qPrintable(advanced.error));
    QCOMPARE(standard.logicalSize, QSizeF(540.0, 780.0));
    QCOMPARE(advanced.logicalSize, QSizeF(540.0, 780.0));
    QCOMPARE(
        standard.background.size(),
        StandardAuthoredBackgroundSize
        );
    QCOMPARE(
        advanced.background.size(),
        AdvancedAuthoredBackgroundSize
        );
    QCOMPARE(standard.scoreCells.size(), 6);
    QCOMPARE(advanced.scoreCells.size(), 6);
    QVERIFY(standard.studentGradeCells.isEmpty());
    QCOMPARE(advanced.studentGradeCells.size(), 6);
    QVERIFY(standard.studentGrades.isEmpty());
    QVERIFY(standard.studentGradeRects.isEmpty());
    QCOMPARE(advanced.studentGrades.size(), 5);
    QCOMPARE(advanced.studentGradeRects.size(), 6);
    QCOMPARE(standard.scoreLabels.size(), 5);
    QCOMPARE(advanced.scoreLabels.size(), 5);
    QCOMPARE(standard.scoreHighlights.size(), 5);
    QCOMPARE(standard.scoreHighlightRects.size(), 6);
    QCOMPARE(advanced.scoreHighlights.size(), 5);
    QCOMPARE(advanced.scoreHighlightRects.size(), 6);
    QCOMPARE(
        advanced.fields.value(QStringLiteral("classLabel")).fontRole,
        QStringLiteral("latinSemibold")
        );
    QCOMPARE(
        advanced.fields.value(QStringLiteral("date")).fontRole,
        QStringLiteral("latinSemibold")
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("koreanName")).rect,
        QRectF(302.05, 77.347, 65.95, 23.0227)
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("classLabel")).rect,
        QRectF(409.0001, 76.7365, 95.6, 26.64)
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("koreanTeacher")).rect,
        QRectF(302.05, 100.1782, 65.95, 23.0227)
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("date")).rect,
        QRectF(400.1999, 100.08, 113.2003, 25.92)
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("nativeTeacher")).baselineOffset,
        -2.0
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("koreanName")).baselineOffset,
        1.0
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("koreanTeacher")).baselineOffset,
        1.0
        );
    QCOMPARE(
        standard.fields.value(QStringLiteral("date")).baselineOffset,
        0.0
        );
    QCOMPARE(
        advanced.scoreCells.at(5).value(QStringLiteral("A+")).top(),
        553.619262
        );
    QCOMPARE(
        advanced.overallGradeBounds,
        QRectF(413.0, 102.325805, 109.0, 39.730899)
        );
    QCOMPARE(
        advanced.studentGradeCells.at(0),
        QRectF(102.0, 188.828058, 51.0, 64.721985)
        );
    QCOMPARE(
        advanced.studentGradeCells.at(4),
        QRectF(102.0, 508.100941, 51.0, 45.705731)
        );
    QVERIFY(
        advanced.studentGradeCells.at(0).height()
            != advanced.studentGradeCells.at(4).height()
        );
    QCOMPARE(
        standard.scoreHighlightColor,
        QColor(QStringLiteral("#FFFF00"))
        );
    QCOMPARE(
        advanced.scoreHighlightColor,
        QColor(QStringLiteral("#FFFF00"))
        );
    QCOMPARE(standard.scoreHighlightInset, 1.0);
    QCOMPARE(advanced.scoreHighlightInset, 1.0);
    const QHash<QString, int> advancedHighlightLeftPixels{
        { QStringLiteral("A+"), 819 },
        { QStringLiteral("A"), 1212 },
        { QStringLiteral("B+"), 1606 },
        { QStringLiteral("B"), 1999 },
        { QStringLiteral("C"), 2393 }
    };
    const QHash<QString, QSize> advancedOverallSourceSizes{
        { QStringLiteral("A+"), QSize(293, 153) },
        { QStringLiteral("A"), QSize(167, 153) },
        { QStringLiteral("B+"), QSize(278, 153) },
        { QStringLiteral("B"), QSize(141, 153) },
        { QStringLiteral("C"), QSize(149, 158) }
    };
    for (
        auto iterator = advancedOverallSourceSizes.cbegin();
        iterator != advancedOverallSourceSizes.cend();
        ++iterator
        )
    {
        const SpeakingEvalSpriteAsset sprite =
            advanced.overallGrades.value(iterator.key());
        QCOMPARE(sprite.source.size(), iterator.value());
        QVERIFY(
            qAbs(
                sprite.destination.center().x()
                    - advanced.overallGradeBounds.center().x()
                ) < 0.001
            );
        QVERIFY(
            qAbs(
                sprite.destination.center().y()
                    - advanced.overallGradeBounds.center().y()
                ) < 0.001
            );
    }

    for (const SpeakingEvalTemplateAssets* templateAssets : {
             &standard,
             &advanced
         })
    {
        for (int metric = 0; metric < 6; ++metric)
        {
            for (const QString& grade : {
                     QStringLiteral("A+"),
                     QStringLiteral("A"),
                     QStringLiteral("B+"),
                     QStringLiteral("B"),
                     QStringLiteral("C")
                     })
            {
                const QImage highlight =
                    templateAssets->scoreHighlights.value(grade);
                const QRect highlightPixels =
                    exactBackgroundRect(
                        *templateAssets,
                        templateAssets->scoreHighlightRects
                            .at(metric)
                            .value(grade)
                        );
                QVERIFY(!highlight.isNull());
                QVERIFY(
                    qAbs(highlightPixels.width() - highlight.width()) <= 4
                        && qAbs(highlightPixels.height() - highlight.height()) <= 4
                    );
                QVERIFY(
                    templateAssets->background.rect().contains(highlightPixels)
                    );
                if (templateAssets == &advanced)
                {
                    QCOMPARE(
                        highlightPixels.left(),
                        advancedHighlightLeftPixels.value(grade)
                        );
                    if (metric == 5)
                    {
                        QCOMPARE(highlightPixels.top(), 2956);
                    }
                    QCOMPARE(
                        highlightPixels.size(),
                        highlight.size()
                        );
                }

                if (templateAssets == &standard)
                {
                    QPoint nonGrayPixel(-1, -1);
                    QColor nonGrayColor;
                    for (
                        int y = highlightPixels.top();
                        y <= highlightPixels.bottom()
                            && nonGrayPixel.x() < 0;
                        ++y
                        )
                    {
                        for (
                            int x = highlightPixels.left();
                            x <= highlightPixels.right();
                            ++x
                            )
                        {
                            const QColor background =
                                templateAssets->background.pixelColor(x, y);
                            if (background.red() < 205
                                || background.red() > 225
                                || background.green() < 205
                                || background.green() > 225
                                || background.blue() < 205
                                || background.blue() > 225)
                            {
                                nonGrayPixel = QPoint(x, y);
                                nonGrayColor = background;
                                break;
                            }
                        }
                    }
                    QVERIFY2(
                        nonGrayPixel.x() < 0,
                        qPrintable(
                            QStringLiteral(
                                "%1 row %2 highlight covers non-gray "
                                "background pixel %3,%4 (#%5)."
                                )
                                .arg(grade)
                                .arg(metric)
                                .arg(nonGrayPixel.x())
                                .arg(nonGrayPixel.y())
                                .arg(
                                    nonGrayColor.rgba(),
                                    8,
                                    16,
                                    QLatin1Char('0')
                                    )
                            )
                        );
                }
            }
        }
    }

    const QRectF advancedAPlus =
        speakingEvalScoreCell(
            SpeakingEvalReportTemplate::Advanced,
            0,
            QStringLiteral("A+")
            );
    QCOMPARE(
        advancedAPlus,
        QRectF(
            153.0,
            167.142857,
            73.666667,
            21.685201
            )
        );
    const QRectF advancedC =
        speakingEvalScoreCell(
            SpeakingEvalReportTemplate::Advanced,
            0,
            QStringLiteral("C")
            );
    QCOMPARE(
        advancedC,
        QRectF(
            448.333333,
            167.142857,
            73.666667,
            21.685201
            )
        );
    QCOMPARE(
        advanced.signatureBounds,
        QRectF(408.5, 746.25, 114.0, 24.75)
        );
    QCOMPARE(
        standard.signatureBounds,
        QRectF(377.1, 722.0, 120.0, 36.0)
        );
    QCOMPARE(
        speakingEvalReportTemplateLayout(
            SpeakingEvalReportTemplate::Standard
            ).signatureBounds,
        standard.signatureBounds
        );
    QCOMPARE(
        speakingEvalReportTemplateLayout(
            SpeakingEvalReportTemplate::Advanced
            ).signatureBounds,
        advanced.signatureBounds
        );

    for (const QString& fieldName : {
             QStringLiteral("date"),
             QStringLiteral("classLabel"),
             QStringLiteral("comments")
             })
    {
        const SpeakingEvalFieldAsset& field =
            advanced.fields.value(fieldName);
        const QRectF clearedLogicalRect =
            fieldName == QStringLiteral("comments")
                ? field.rect.marginsRemoved(field.margins)
                : field.rect;
        const QRect clearedPixels =
            backgroundRect(advanced, clearedLogicalRect)
                .intersected(advanced.background.rect());
        QVERIFY(clearedPixels.isValid());
        for (int y = clearedPixels.top(); y <= clearedPixels.bottom(); ++y)
        {
            for (int x = clearedPixels.left(); x <= clearedPixels.right(); ++x)
            {
                const QColor pixel =
                    advanced.background.pixelColor(x, y);
                QVERIFY(
                    pixel.red() >= 245
                    && pixel.green() >= 245
                    && pixel.blue() >= 245
                    );
            }
        }
    }
}

void SpeakingEvalReportWidgetTests::
    standardMetadataUsesSharedFontAndHalfPointGradeFitting()
{
    const SpeakingEvalTemplateAssets& standard =
        speakingEvalTemplateAssets(
            SpeakingEvalReportTemplate::Standard
            );
    const SpeakingEvalFieldAsset englishName =
        standard.fields.value(QStringLiteral("englishName"));
    const SpeakingEvalFieldAsset nativeTeacher =
        standard.fields.value(QStringLiteral("nativeTeacher"));
    const SpeakingEvalFieldAsset koreanName =
        standard.fields.value(QStringLiteral("koreanName"));
    const SpeakingEvalFieldAsset koreanTeacher =
        standard.fields.value(QStringLiteral("koreanTeacher"));
    const SpeakingEvalFieldAsset classLabel =
        standard.fields.value(QStringLiteral("classLabel"));
    const SpeakingEvalFieldAsset date =
        standard.fields.value(QStringLiteral("date"));

    QCOMPARE(nativeTeacher.fontSizePoints, englishName.fontSizePoints);
    QCOMPARE(classLabel.fontSizePoints, englishName.fontSizePoints);
    QCOMPARE(date.fontSizePoints, englishName.fontSizePoints);
    QCOMPARE(koreanName.fontSizePoints, englishName.fontSizePoints);
    QCOMPARE(
        koreanTeacher.fontSizePoints,
        englishName.fontSizePoints
        );
    QCOMPARE(
        speakingEvalFittedFieldFontSize(
            koreanName,
            QStringLiteral("각각각")
            ),
        16.0
        );
    QCOMPARE(
        speakingEvalFittedFieldFontSize(
            koreanTeacher,
            QStringLiteral("홍승현")
            ),
        16.0
        );

    QCOMPARE(
        speakingEvalFittedFieldFontSize(
            englishName,
            QStringLiteral("Gildong")
            ),
        englishName.fontSizePoints
        );
    QCOMPARE(
        speakingEvalFittedFieldFontSize(
            nativeTeacher,
            QStringLiteral("Aristotle")
            ),
        englishName.fontSizePoints
        );
    QCOMPARE(
        speakingEvalFittedFieldFontSize(
            date,
            QStringLiteral("Aug. 2026")
            ),
        englishName.fontSizePoints
        );

    const qreal widestGradeSize =
        speakingEvalFittedFieldFontSize(
            classLabel,
            QStringLiteral("E4 Odysseus"),
            0.5
            );
    const qreal wholePointGradeSize =
        speakingEvalFittedFieldFontSize(
            classLabel,
            QStringLiteral("E4 Odysseus")
            );
    QVERIFY(widestGradeSize <= englishName.fontSizePoints);
    QVERIFY(
        widestGradeSize
            >= classLabel.fontSizePoints * classLabel.minimumScale
        );
    QCOMPARE(
        widestGradeSize * 2.0,
        qreal(qRound(widestGradeSize * 2.0))
        );
    QCOMPARE(widestGradeSize, wholePointGradeSize + 0.5);
}

void SpeakingEvalReportWidgetTests::templateUsesPortraitSize_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard")
        << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced")
        << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalReportWidgetTests::templateUsesPortraitSize()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);
    SpeakingEvalReportData data;
    data.reportTemplate = reportTemplate;
    SpeakingEvalReportWidget report;
    report.setReportData(data);

    QCOMPARE(report.sizeHint(), QSize(810, 1170));
    QCOMPARE(
        report.usesAdvancedTemplate(),
        reportTemplate == SpeakingEvalReportTemplate::Advanced
        );
}

void SpeakingEvalReportWidgetTests::
    representativeReportPreservesAuthoredBackground_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard")
        << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced")
        << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalReportWidgetTests::
    representativeReportPreservesAuthoredBackground()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);

    const QImage actual =
        renderReport(
            representativeData(reportTemplate)
            );
    const QString previewPath =
        qEnvironmentVariable(
            reportTemplate == SpeakingEvalReportTemplate::Advanced
                ? "CLASSMNGR_REPORT_PREVIEW_PATH"
                : "CLASSMNGR_REGULAR_REPORT_PREVIEW_PATH"
            );
    if (!previewPath.isEmpty())
    {
        QVERIFY(actual.save(previewPath));
    }

    const ImageDifference staticDifference =
        compareImages(
            actual,
            renderStaticBackground(reportTemplate),
            mutableRegion(reportTemplate)
            );

    QVERIFY2(
        staticDifference.exactDifferencesOutsideMutableRegion == 0,
        qPrintable(
            QStringLiteral(
                "%1 exact differences occurred outside mutable regions; first at %2,%3."
                )
                .arg(
                    staticDifference
                        .exactDifferencesOutsideMutableRegion
                    )
                .arg(
                    staticDifference
                        .firstDifferenceOutsideMutableRegion.x()
                    )
                .arg(
                    staticDifference
                        .firstDifferenceOutsideMutableRegion.y()
                    )
            )
        );
}

void SpeakingEvalReportWidgetTests::
    dynamicTextNeverEscapesManifestRectangles_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard")
        << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced")
        << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalReportWidgetTests::
    dynamicTextNeverEscapesManifestRectangles()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);

    SpeakingEvalReportData blank;
    blank.reportTemplate = reportTemplate;
    const QImage baseline = renderReport(blank);
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate);

    const QList<QPair<QString, QString>> cases{
        {
            QStringLiteral("englishName"),
            QStringLiteral(
                "An Exceptionally Long Latin Student Name That Must Be Clipped"
                )
        },
        {
            QStringLiteral("koreanName"),
            QStringLiteral(
                "대한민국한국어학생이름이아주길어서줄어들고잘려야합니다"
                )
        },
        {
            QStringLiteral("classLabel"),
            QStringLiteral(
                "An Extremely Long Advanced Class Assignment"
                )
        },
        {
            QStringLiteral("nativeTeacher"),
            QStringLiteral(
                "An Exceptionally Long Native Teacher Name"
                )
        },
        {
            QStringLiteral("koreanTeacher"),
            QStringLiteral(
                "한국인선생님이름이매우아주깁니다"
                )
        },
        {
            QStringLiteral("date"),
            QStringLiteral(
                "Wednesday, September 30, 2026"
                )
        },
        {
            QStringLiteral("comments"),
            QStringLiteral(
                "This deliberately long comment verifies wrapping and clipping "
                "inside the exact PowerPoint content region. 한국어 문장도 함께 "
                "여러 줄로 배치되며 어떠한 글자도 지정된 영역 밖으로 나가면 "
                "안 됩니다. "
                "Additional words force the renderer to shrink only when the "
                "available height requires it. "
                "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
                )
        }
    };

    for (const auto& [fieldName, value] : cases)
    {
        SpeakingEvalReportData data = blank;
        if (fieldName == QStringLiteral("englishName"))
        {
            data.englishName = value;
        }
        else if (fieldName == QStringLiteral("koreanName"))
        {
            data.koreanName = value;
        }
        else if (fieldName == QStringLiteral("classLabel"))
        {
            data.classLabel = value;
        }
        else if (fieldName == QStringLiteral("nativeTeacher"))
        {
            data.nativeTeacher = value;
        }
        else if (fieldName == QStringLiteral("koreanTeacher"))
        {
            data.koreanTeacher = value;
        }
        else if (fieldName == QStringLiteral("date"))
        {
            data.date = value;
        }
        else
        {
            data.comments = value;
        }

        const QImage rendered = renderReport(data);
        const QRect allowed =
            rasterRect(assets.fields.value(fieldName).rect);
        for (int y = 0; y < rendered.height(); ++y)
        {
            for (int x = 0; x < rendered.width(); ++x)
            {
                if (!allowed.contains(x, y)
                    && rendered.pixel(x, y) != baseline.pixel(x, y))
                {
                    QFAIL(
                        qPrintable(
                            QStringLiteral(
                                "%1 changed a pixel outside its manifest rectangle at %2,%3."
                                )
                                .arg(fieldName)
                                .arg(x)
                                .arg(y)
                            )
                        );
                }
            }
        }
    }
}

void SpeakingEvalReportWidgetTests::
    scoreLabelsAreCenteredInEveryCell_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard")
        << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced")
        << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalReportWidgetTests::
    scoreLabelsAreCenteredInEveryCell()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);

    SpeakingEvalReportData data;
    data.reportTemplate = reportTemplate;
    data.scores.fill(QStringLiteral("invalid"));
    const QImage rendered = renderReport(data);
    const QImage background =
        renderStaticBackground(reportTemplate);
    const QStringList grades{
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };

    for (int metric = 0; metric < 6; ++metric)
    {
        for (const QString& grade : grades)
        {
            const QRect cell =
                rasterRect(
                    speakingEvalScoreCell(
                        reportTemplate,
                        metric,
                        grade
                        )
                    ).adjusted(3, 3, -3, -3);
            int left = cell.right();
            int top = cell.bottom();
            int right = cell.left();
            int bottom = cell.top();
            for (int y = cell.top(); y <= cell.bottom(); ++y)
            {
                for (int x = cell.left(); x <= cell.right(); ++x)
                {
                    if (rendered.pixel(x, y) == background.pixel(x, y))
                    {
                        continue;
                    }
                    left = qMin(left, x);
                    top = qMin(top, y);
                    right = qMax(right, x);
                    bottom = qMax(bottom, y);
                }
            }

            QVERIFY2(
                right >= left && bottom >= top,
                qPrintable(
                    QStringLiteral(
                        "%1 row %2 has no visible grade label."
                        ).arg(grade).arg(metric)
                    )
                );
            const QPointF labelCenter(
                (left + right) / 2.0,
                (top + bottom) / 2.0
                );
            const QPointF cellCenter =
                QRectF(cell).center();
            QVERIFY2(
                qAbs(labelCenter.x() - cellCenter.x()) <= 2.0
                    && qAbs(labelCenter.y() - cellCenter.y()) <= 2.0,
                qPrintable(
                    QStringLiteral(
                        "%1 row %2 label center differs from its cell by %3,%4 pixels."
                        )
                        .arg(grade)
                        .arg(metric)
                        .arg(labelCenter.x() - cellCenter.x())
                        .arg(labelCenter.y() - cellCenter.y())
                    )
                );
        }
    }
}

void SpeakingEvalReportWidgetTests::
    advancedStudentGradesAreCenteredInTheirMetricCells()
{
    SpeakingEvalReportData data;
    data.reportTemplate = SpeakingEvalReportTemplate::Advanced;
    data.scores = {
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("A"),
        QStringLiteral("C")
    };
    SpeakingEvalReportData baselineData = data;
    baselineData.scores.fill(QStringLiteral("invalid"));

    const QImage rendered = renderReport(data);
    const QImage baseline = renderReport(baselineData);
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(
            SpeakingEvalReportTemplate::Advanced
            );

    for (int metric = 0; metric < assets.studentGradeCells.size(); ++metric)
    {
        const QRect cell =
            rasterRect(
                assets.studentGradeCells.at(metric)
                ).adjusted(2, 2, -2, -2);
        int left = cell.right();
        int top = cell.bottom();
        int right = cell.left();
        int bottom = cell.top();
        for (int y = cell.top(); y <= cell.bottom(); ++y)
        {
            for (int x = cell.left(); x <= cell.right(); ++x)
            {
                if (rendered.pixel(x, y) == baseline.pixel(x, y))
                {
                    continue;
                }
                left = qMin(left, x);
                top = qMin(top, y);
                right = qMax(right, x);
                bottom = qMax(bottom, y);
            }
        }

        QVERIFY2(
            right >= left && bottom >= top,
            qPrintable(
                QStringLiteral(
                    "Metric %1 has no visible student grade."
                    ).arg(metric)
                )
            );
        const QPointF gradeCenter(
            (left + right) / 2.0,
            (top + bottom) / 2.0
            );
        const QPointF cellCenter =
            QRectF(cell).center();
        QVERIFY2(
            qAbs(gradeCenter.x() - cellCenter.x()) <= 2.0
                && qAbs(gradeCenter.y() - cellCenter.y()) <= 2.0,
            qPrintable(
                QStringLiteral(
                    "Metric %1 student grade differs from its cell center by %2,%3 pixels."
                    )
                    .arg(metric)
                    .arg(gradeCenter.x() - cellCenter.x())
                    .arg(gradeCenter.y() - cellCenter.y())
                )
            );
    }
}

void SpeakingEvalReportWidgetTests::
    everyScoreHighlightAndInvalidScoreAreHandled_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard")
        << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced")
        << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalReportWidgetTests::
    everyScoreHighlightAndInvalidScoreAreHandled()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);
    SpeakingEvalReportData data;
    data.reportTemplate = reportTemplate;
    data.scores.fill(QStringLiteral("invalid"));

    const QImage invalid = renderReport(data);
    SpeakingEvalReportData blankData = data;
    blankData.scores.fill(QString());
    QCOMPARE(renderReport(blankData), invalid);
    const QStringList grades{
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };

    for (int metric = 0; metric < 6; ++metric)
    {
        for (const QString& grade : grades)
        {
            SpeakingEvalReportData highlightedData = data;
            highlightedData.scores[metric] = grade;
            const QImage highlighted =
                renderReport(highlightedData);
            const QRect cell =
                rasterRect(
                    speakingEvalScoreCell(
                        reportTemplate,
                        metric,
                        grade
                        )
                    );
            const QPoint center =
                cell.topLeft() + QPoint(8, 8);
            const QColor selected =
                highlighted.pixelColor(center);
            const QColor unselected =
                invalid.pixelColor(center);

            QVERIFY2(
                selected.red() > 235
                    && selected.green() > 220
                    && selected.blue() < 35,
                qPrintable(
                    QStringLiteral(
                        "%1 row %2 was not highlighted."
                        )
                        .arg(grade)
                        .arg(metric)
                    )
                );
            QVERIFY(
                unselected.red() > 190
                && unselected.green() > 190
                && unselected.blue() > 190
                );

            if (reportTemplate
                == SpeakingEvalReportTemplate::Advanced)
            {
                const QRect innerCell =
                    cell.adjusted(1, 1, -1, -1);
                int preservedBorderPixels = 0;
                for (int y = cell.top(); y <= cell.bottom(); ++y)
                {
                    for (int x = cell.left(); x <= cell.right(); ++x)
                    {
                        if (innerCell.contains(x, y))
                        {
                            continue;
                        }

                        const QColor border =
                            invalid.pixelColor(x, y);
                        if (border.red() < 160
                            && border.green() < 160
                            && border.blue() < 160)
                        {
                            const QColor highlightedBorder =
                                highlighted.pixelColor(x, y);
                            if (highlightedBorder.red() < 160
                                && highlightedBorder.green() < 160
                                && highlightedBorder.blue() < 160)
                            {
                                ++preservedBorderPixels;
                            }
                        }
                    }
                }
                QVERIFY2(
                    preservedBorderPixels > 0,
                    "The score cell did not contain a border pixel to verify."
                    );
            }
        }
    }

    if (reportTemplate == SpeakingEvalReportTemplate::Advanced)
    {
        constexpr int ZoomedRasterScale = 3;
        const QSize zoomedSize(
            540 * ZoomedRasterScale,
            780 * ZoomedRasterScale
            );
        const QImage zoomedBaseline =
            renderReport(data, zoomedSize);
        SpeakingEvalReportData selectedC = data;
        selectedC.scores[0] = QStringLiteral("C");
        const QImage zoomedHighlight =
            renderReport(selectedC, zoomedSize);
        const QRect zoomedCell =
            QRectF(
                speakingEvalScoreCell(
                    reportTemplate,
                    0,
                    QStringLiteral("C")
                    ).topLeft() * ZoomedRasterScale,
                speakingEvalScoreCell(
                    reportTemplate,
                    0,
                    QStringLiteral("C")
                    ).size() * ZoomedRasterScale
                ).toAlignedRect();
        const QRect zoomedInterior =
            zoomedCell.adjusted(1, 1, -1, -1);
        int preservedZoomedBorderPixels = 0;

        for (int y = zoomedCell.top(); y <= zoomedCell.bottom(); ++y)
        {
            for (int x = zoomedCell.left(); x <= zoomedCell.right(); ++x)
            {
                if (zoomedInterior.contains(x, y))
                {
                    continue;
                }

                const QColor border =
                    zoomedBaseline.pixelColor(x, y);
                if (border.red() < 160
                    && border.green() < 160
                    && border.blue() < 160)
                {
                    const QColor highlightedBorder =
                        zoomedHighlight.pixelColor(x, y);
                    if (highlightedBorder.red() < 160
                        && highlightedBorder.green() < 160
                        && highlightedBorder.blue() < 160)
                    {
                        ++preservedZoomedBorderPixels;
                    }
                }
            }
        }
        QVERIFY2(
            preservedZoomedBorderPixels > 0,
            "The 300% score cell did not contain a border pixel to verify."
            );
    }

    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate);
    const QRect overallRect =
        rasterRect(
            assets.overallGrades.value(
                QStringLiteral("N/A")
                ).destination
            );
    QList<QImage> overallImages{
        invalid.copy(overallRect)
    };
    QVERIFY(
        overallImages.constFirst()
        != renderStaticBackground(reportTemplate).copy(overallRect)
        );

    for (const QString& overall : {
             QStringLiteral("A+"),
             QStringLiteral("A"),
             QStringLiteral("B+"),
             QStringLiteral("B"),
             QStringLiteral("C")
             })
    {
        SpeakingEvalReportData overallData = data;
        overallData.scores.fill(overall);
        const QImage overallImage =
            renderReport(overallData).copy(overallRect);
        for (const QImage& existing : std::as_const(overallImages))
        {
            QVERIFY(overallImage != existing);
        }
        overallImages.append(overallImage);
    }
}

void SpeakingEvalReportWidgetTests::
    interactiveTemplateEditsScoresAndComments_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::newRow("standard")
        << SpeakingEvalReportTemplate::Standard;
    QTest::newRow("advanced")
        << SpeakingEvalReportTemplate::Advanced;
}

void SpeakingEvalReportWidgetTests::
    interactiveTemplateEditsScoresAndComments()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);

    SpeakingEvalReportData data;
    data.reportTemplate = reportTemplate;
    SpeakingEvalReportWidget report;
    report.setReportData(data);
    report.setInteractive(true);

    QSignalSpy scoreSpy(
        &report,
        &SpeakingEvalReportWidget::scoreEdited
        );
    const QPointF logicalCenter =
        speakingEvalScoreCell(
            reportTemplate,
            0,
            QStringLiteral("A+")
            ).center();
    const QPoint cellPoint =
        (logicalCenter * 1.5).toPoint();

    QTest::mouseClick(
        &report,
        Qt::LeftButton,
        Qt::NoModifier,
        cellPoint
        );
    QCOMPARE(scoreSpy.size(), 1);
    QCOMPARE(scoreSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(scoreSpy.at(0).at(1).toString(), QStringLiteral("A+"));

    QTest::mouseClick(
        &report,
        Qt::LeftButton,
        Qt::NoModifier,
        cellPoint
        );
    QCOMPARE(scoreSpy.size(), 2);
    QCOMPARE(scoreSpy.at(1).at(1).toString(), QString());

    auto* comments =
        report.findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalReportComments")
            );
    QVERIFY(comments);
    QVERIFY(!comments->isHidden());

    const SpeakingEvalFieldAsset* commentAsset =
        speakingEvalFieldAsset(
            reportTemplate,
            QStringLiteral("comments")
            );
    QVERIFY(commentAsset);
    const QRectF logicalEditor =
        commentAsset->rect.marginsRemoved(
            commentAsset->margins
            );
    const QRect expectedEditor =
        QRectF(
            logicalEditor.topLeft() * 1.5,
            logicalEditor.size() * 1.5
            ).toAlignedRect();
    QCOMPARE(comments->geometry(), expectedEditor);

    QSignalSpy commentsSpy(
        &report,
        &SpeakingEvalReportWidget::commentsEdited
        );
    comments->setPlainText(
        QStringLiteral("A freshly typed comment.")
        );
    QCOMPARE(commentsSpy.size(), 1);
    QCOMPARE(
        commentsSpy.at(0).at(0).toString(),
        QStringLiteral("A freshly typed comment.")
        );
}

void SpeakingEvalReportWidgetTests::
    signaturesKeepAspectRatioWithinManifestBounds_data()
{
    QTest::addColumn<SpeakingEvalReportTemplate>("reportTemplate");
    QTest::addColumn<QSize>("signatureSize");
    for (const auto reportTemplate : {
             SpeakingEvalReportTemplate::Standard,
             SpeakingEvalReportTemplate::Advanced
             })
    {
        const QByteArray prefix =
            reportTemplate == SpeakingEvalReportTemplate::Advanced
                ? QByteArray("advanced")
                : QByteArray("standard");
        QTest::newRow(prefix + "-wide")
            << reportTemplate
            << QSize(400, 100);
        QTest::newRow(prefix + "-tall")
            << reportTemplate
            << QSize(100, 400);
    }
}

void SpeakingEvalReportWidgetTests::
    signaturesKeepAspectRatioWithinManifestBounds()
{
    QFETCH(SpeakingEvalReportTemplate, reportTemplate);
    QFETCH(QSize, signatureSize);

    SpeakingEvalReportData data;
    data.reportTemplate = reportTemplate;
    data.signatureImage = signatureImage(signatureSize);
    QVERIFY(!data.signatureImage.isEmpty());
    const QImage rendered = renderReport(data);
    const QRect bounds =
        rasterRect(
            speakingEvalTemplateAssets(
                reportTemplate
                ).signatureBounds
            );

    int left = rendered.width();
    int top = rendered.height();
    int right = -1;
    int bottom = -1;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
    {
        for (int x = bounds.left(); x <= bounds.right(); ++x)
        {
            const QColor color =
                rendered.pixelColor(x, y);
            if (color.green() > 240
                && color.red() < 10
                && color.blue() < 10)
            {
                left = qMin(left, x);
                top = qMin(top, y);
                right = qMax(right, x);
                bottom = qMax(bottom, y);
            }
        }
    }

    QVERIFY(right >= left && bottom >= top);
    const qreal actualRatio =
        static_cast<qreal>(right - left + 1)
        / (bottom - top + 1);
    const qreal expectedRatio =
        static_cast<qreal>(signatureSize.width())
        / signatureSize.height();
    QVERIFY(qAbs(actualRatio - expectedRatio) < 0.08);
    QVERIFY(left >= bounds.left());
    QVERIFY(top >= bounds.top());
    QVERIFY(right <= bounds.right());
    QVERIFY(bottom <= bounds.bottom());
    if (reportTemplate == SpeakingEvalReportTemplate::Advanced)
    {
        QVERIFY(qAbs(left - bounds.left()) <= 1);
        QVERIFY(qAbs(bottom - bounds.bottom()) <= 1);
    }
    else
    {
        const QPointF actualCenter(
            (left + right) / 2.0,
            (top + bottom) / 2.0
            );
        QVERIFY(
            qAbs(actualCenter.x() - bounds.center().x()) <= 1.0
            );
        QVERIFY(
            qAbs(actualCenter.y() - bounds.center().y()) <= 1.0
            );
    }
}

QTEST_MAIN(SpeakingEvalReportWidgetTests)

#include "speaking_eval_report_widget_tests.moc"
