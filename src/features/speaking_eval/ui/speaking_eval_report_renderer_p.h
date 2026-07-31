#include "speaking_eval_report_widget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFontMetricsF>
#include <QImage>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTextCursor>

#include <algorithm>

namespace
{

constexpr QRectF RegularBorderRect(
    17.71654,
    18.66228,
    504.5669,
    742.6771
    );
constexpr qreal TableLeft = 35.0263;
constexpr qreal TableTop = 125.0386;
constexpr qreal CriteriaWidth = 74.25504;
constexpr qreal GradeWidth = 78.57787;
constexpr qreal GradeHeaderHeight = 22.8;
constexpr qreal CriteriaMetricsTopInset = 18.5;
constexpr qreal CriteriaMetricHeight = 6.0;
constexpr int GrammarMetricCount = 8;
constexpr qreal GrammarMetricsCellHeight = 81.0;
// Grammar has 14.5 points remaining below its eight 6-point metric rows.
// Distributing that over its seven inter-metric gaps and the final bottom gap
// adds 1.8125 points to each gap, keeping the final metric clear of the cell
// border without changing the existing cell height.
constexpr qreal CriteriaMetricExtraSpacing =
    (GrammarMetricsCellHeight
     - CriteriaMetricsTopInset
     - (CriteriaMetricHeight * GrammarMetricCount))
    / GrammarMetricCount;
constexpr qreal CriteriaMetricPitch =
    CriteriaMetricHeight + CriteriaMetricExtraSpacing;

struct RubricSection
{
    QString title;
    QStringList criteria;
    std::array<QString, 5> descriptions;
    qreal height = 0.0;
    bool hasMergedDescriptions = false;
};

QFont standardFont(
    const QString& family,
    qreal pointSize,
    int weight = QFont::Normal
    )
{
    QFont font(family);
    // The PowerPoint reference uses the same 540-by-780 point grid as the
    // report canvas. Pixel sizes therefore map its font sizes one-to-one.
    font.setPixelSize(qRound(pointSize));
    font.setWeight(
        static_cast<QFont::Weight>(weight)
        );
    return font;
}

QFont advancedFont(
    const QString& family,
    qreal pointSize,
    int weight = QFont::Normal
    )
{
    QFont font(family);
    // The report canvas uses the PowerPoint slide's 540-by-780 point grid.
    // Pixel sizes therefore preserve the source font sizes when the canvas is
    // scaled for screen preview or printing.
    font.setPixelSize(qRound(pointSize));
    font.setWeight(
        static_cast<QFont::Weight>(weight)
        );
    return font;
}

QStringList wrappedLines(
    const QString& text,
    const QFontMetricsF& metrics,
    qreal maximumWidth
    )
{
    QStringList lines;
    QString currentLine;

    for (const QString& word : text.split(
             QLatin1Char(' '),
             Qt::SkipEmptyParts
             ))
    {
        const QString candidate =
            currentLine.isEmpty()
                ? word
                : currentLine + QLatin1Char(' ') + word;

        if (
            currentLine.isEmpty()
            || metrics.horizontalAdvance(candidate) <= maximumWidth
            )
        {
            currentLine = candidate;
            continue;
        }

        lines.append(currentLine);
        currentLine = word;
    }

    if (!currentLine.isEmpty())
    {
        lines.append(currentLine);
    }

    return lines;
}

void drawAdvancedBulletList(
    QPainter* painter,
    const QRectF& rect,
    const QString& text,
    const QFont& font
    )
{
    constexpr qreal bulletLeftInset = 2.0;
    constexpr qreal textLeftInset = 8.0;
    constexpr qreal rightInset = 2.0;

    const QFontMetricsF metrics(font);
    const qreal lineHeight =
        metrics.height();
    const qreal textWidth =
        rect.width() - textLeftInset - rightInset;
    QList<QStringList> bulletLines;
    int totalLineCount = 0;

    for (QString bullet : text.split(
             QLatin1Char('\n'),
             Qt::SkipEmptyParts
             ))
    {
        bullet.remove(
            QRegularExpression(
                QStringLiteral("^\\s*\u2022\\s*")
                )
            );

        const QStringList lines =
            wrappedLines(
                bullet,
                metrics,
                textWidth
                );

        bulletLines.append(lines);
        totalLineCount += lines.size();
    }

    qreal top =
        rect.center().y()
        - ((lineHeight * totalLineCount) / 2.0);

    painter->setFont(font);
    painter->setPen(Qt::black);

    for (const QStringList& lines : bulletLines)
    {
        if (lines.isEmpty())
        {
            continue;
        }

        painter->drawText(
            QRectF(
                rect.left() + bulletLeftInset,
                top,
                textLeftInset - bulletLeftInset,
                lineHeight
                ),
            Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("\u2022")
            );

        for (const QString& line : lines)
        {
            painter->drawText(
                QRectF(
                    rect.left() + textLeftInset,
                    top,
                    textWidth,
                    lineHeight
                    ),
                Qt::AlignLeft | Qt::AlignVCenter,
                line
                );
            top += lineHeight;
        }
    }
}

void drawBorder(
    QPainter* painter,
    const QRectF& rect,
    qreal width = 1.0
    )
{
    QPen pen(Qt::black);
    pen.setWidthF(width);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);
}

void drawCenteredText(
    QPainter* painter,
    const QRectF& rect,
    const QString& text,
    const QFont& font,
    int extraFlags = 0
    )
{
    painter->setFont(font);
    painter->setPen(Qt::black);
    painter->drawText(
        rect,
        (extraFlags == 0 ? Qt::AlignHCenter : extraFlags)
            | Qt::AlignVCenter
            | Qt::TextWordWrap,
        text
        );
}

void drawLabelAndValue(
    QPainter* painter,
    const QRectF& labelRect,
    const QRectF& valueCellRect,
    const QRectF& valueTextRect,
    const QString& label,
    const QString& value,
    bool koreanValue = false
    )
{
    painter->setFont(
        standardFont(
            QStringLiteral("Calibri"),
            12.0,
            QFont::Bold
            )
        );
    painter->setPen(Qt::black);
    painter->drawText(
        labelRect.adjusted(3.6, 0.0, -1.0, 0.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        label
        );

    QPen linePen(Qt::black);
    linePen.setWidthF(0.85);
    painter->setPen(linePen);
    const qreal underlineY =
        valueCellRect.bottom() - 4.8;
    painter->drawLine(
        QPointF(
            valueCellRect.left() + 3.6,
            underlineY
            ),
        QPointF(
            valueCellRect.right() - 3.6,
            underlineY
            )
        );

    painter->setFont(
        standardFont(
            koreanValue
                ? QStringLiteral("Kakao Big Sans")
                : QStringLiteral("Segoe UI Semibold"),
            16.0
            )
        );
    painter->setPen(Qt::black);
    const QFontMetricsF valueMetrics(painter->font());
    const qreal underlineTop =
        underlineY - (linePen.widthF() / 2.0);
    qreal valueBaseline =
        underlineTop;

    if (koreanValue)
    {
        valueBaseline -=
            std::max(
                0.0,
                valueMetrics.tightBoundingRect(value).bottom()
                );
    }

    painter->drawText(
        QPointF(
            valueTextRect.center().x()
                - (valueMetrics.horizontalAdvance(value) / 2.0),
            valueBaseline
            ),
        value
        );
}

QList<RubricSection> rubricSections()
{
    return {
        {
            QStringLiteral("Grammar"),
            {
                QStringLiteral("Subject-verb Agreement"),
                QStringLiteral("Verb Tenses"),
                QStringLiteral("Pronouns"),
                QStringLiteral("Prepositions"),
                QStringLiteral("Articles"),
                QStringLiteral("Plurals"),
                QStringLiteral("Correct Vocabulary Use"),
                QStringLiteral("Sentence Structure")
            },
            {
                QStringLiteral("Very well-structured\nsentences with little\nto no errors in\ngrammar."),
                QStringLiteral("Sentences are well\nconstructed with\nsome errors in\ncomplex grammar."),
                QStringLiteral("Use of full sentences,\nbut basic grammar\nmistakes."),
                QStringLiteral("Basic sentences\nbeginning to be\nformed. Still some\nfragmented ideas."),
                QStringLiteral("No use of full\nsentences. Single\nworded answers.")
            },
            81.0
        },
        {
            QStringLiteral("Pronunciation"),
            {
                QStringLiteral("Use of Konglish"),
                QStringLiteral("Clarity / Mumbling"),
                QStringLiteral("Difficult Vocabulary")
            },
            {
                QStringLiteral("Natural\npronunciation."),
                QStringLiteral("Good pronunciation,\nbut some odd words."),
                QStringLiteral("Generally good\npronunciation."),
                QStringLiteral("Pronunciation\nimproving, but some\nKonglish."),
                QStringLiteral("Excessive use of\nKonglish.")
            },
            73.2
        },
        {
            QStringLiteral("Fluency"),
            {
                QStringLiteral("Flow"),
                QStringLiteral("Varying Intonation"),
                QStringLiteral("Speaking Pace"),
                QStringLiteral("Long Pause")
            },
            {
                QStringLiteral("Natural flow and\nappropriate pausing\nand range of tone."),
                QStringLiteral("Some flow and\npausing. Some range\nof tone."),
                QStringLiteral("Too fast or too slow."),
                QStringLiteral("Overuse of filler\nwords\n(umm/like)."),
                QStringLiteral("Very long pauses.")
            },
            73.2
        },
        {
            QStringLiteral("Manner"),
            {
                QStringLiteral("Volume"),
                QStringLiteral("Eye Contact"),
                QStringLiteral("Energy"),
                QStringLiteral("Tone / Emotion"),
                QStringLiteral("Posture"),
                QStringLiteral("Body Language"),
                QStringLiteral("Gestures")
            },
            {
                QStringLiteral("Lots of eye contact\nand natural gestures\nand emotion."),
                QStringLiteral("Some eye contact\nand gestures."),
                QStringLiteral("Development of\nsome eye contact and\ngestures."),
                QStringLiteral("Still some nerves\nwhen speaking, but\ngenuine effort given."),
                QStringLiteral("No eye contact.\nToo soft to hear.")
            },
            73.8
        },
        {
            QStringLiteral("Content"),
            {
                QStringLiteral("Details"),
                QStringLiteral("Reasons & Examples"),
                QStringLiteral("Organization"),
                QStringLiteral("Logic & Relevance"),
                QStringLiteral("Level of Vocabulary")
            },
            {
                QStringLiteral("•  Creative and interesting ideas\n•  Logical arguments\n•  Effective use of details & examples\n•  Use of advanced vocabulary"),
                QString(),
                QStringLiteral("•  Relevant and accurate information\n•  Development of vocabulary"),
                QString(),
                QStringLiteral("No details or\nexamples (single\nworded answers)")
            },
            73.2,
            true
        },
        {
            QStringLiteral("Overall Effort"),
            {
                QStringLiteral("Effort"),
                QStringLiteral("Prepared"),
                QStringLiteral("Memorization"),
                QStringLiteral("Visual Aids")
            },
            {
                QStringLiteral("A great amount of\neffort has been put\nin."),
                QStringLiteral("A good amount of\neffort has been put\nin."),
                QStringLiteral("Some effort has been\nput in."),
                QStringLiteral("Little effort has been\nput in."),
                QStringLiteral("No effort has been\nput in.")
            },
            73.2
        }
    };
}

QList<RubricSection> advancedRubricSections()
{
    return {
        {
            QStringLiteral("Grammar  \uBB38\uBC95"),
            {
                QStringLiteral("Subject-Verb Agreement"),
                QStringLiteral("Verb Tenses"),
                QStringLiteral("Pronouns"),
                QStringLiteral("Prepositions"),
                QStringLiteral("Articles"),
                QStringLiteral("Plurals"),
                QStringLiteral("Correct Vocabulary Use"),
                QStringLiteral("Sentence Structure")
            },
            {
                QStringLiteral("Applies advanced grammar concepts accurately; little to no grammar errors"),
                QStringLiteral("Advanced grammar concepts are used, but with some errors; sometimes mistakes nuances in articles, prepositions, etc."),
                QStringLiteral("Unsure about how to use advanced grammar concepts correctly; frequently making mistakes in basic grammar"),
                QStringLiteral("Only basic sentence structures are used; many errors in basic concepts like verb tenses and agreement"),
                QStringLiteral("An abundance of grammar errors impedes comprehension")
            },
            86.4
        },
        {
            QStringLiteral("Pronunciation  \uBC1C\uC74C"),
            {
                QStringLiteral("Use of Konglish"),
                QStringLiteral("Clarity / Mumbling"),
                QStringLiteral("Difficult Vocabulary"),
                QStringLiteral("Accent"),
                QStringLiteral("Syllable Division"),
                QStringLiteral("Proper Syllable Emphasis")
            },
            {
                QStringLiteral("Natural pronunciation with no errors that impede vocabulary comprehension"),
                QStringLiteral("Good overall pronunciation, but some occasional odd words"),
                QStringLiteral("Understandable pronunciation, but some errors in specific sounds, letters, or syllable division"),
                QStringLiteral("Pronunciation errors in both advanced and basic vocabulary; beginning to impede understanding"),
                QStringLiteral("Excessive use of Konglish; many words are not comprehensible")
            },
            79.2
        },
        {
            QStringLiteral("Fluency  \uC720\uCC3D\uC131"),
            {
                QStringLiteral("Flow"),
                QStringLiteral("Varying Intonation"),
                QStringLiteral("Speaking Pace"),
                QStringLiteral("Pausing")
            },
            {
                QStringLiteral("Natural flow and appropriate pausing; dynamic range of tone; speaks with an engaging tone"),
                QStringLiteral("Decent flow with appropriate pausing; some range of tone"),
                QStringLiteral("Occasional awkward pauses in the middle of sentences; most sentences spoken with the same intonation"),
                QStringLiteral("Overuse of filler words (umm/like); some long pauses begin to impede listener understanding"),
                QStringLiteral("Stopped speaking suddenly and could not resume")
            },
            79.2
        },
        {
            QStringLiteral("Manner  \uD0DC\uB3C4"),
            {
                QStringLiteral("Volume"),
                QStringLiteral("Eye Contact"),
                QStringLiteral("Energy"),
                QStringLiteral("Emotion"),
                QStringLiteral("Posture"),
                QStringLiteral("Body Language"),
                QStringLiteral("Gestures")
            },
            {
                QStringLiteral("Consistent eye contact across the audience; natural gestures and emotion; confident body language"),
                QStringLiteral("Often makes eye contact and uses some gestures; developing relevant emotion in speech"),
                QStringLiteral("Occasional eye contact and an attempt at using gestures; lacking in relevant emotion"),
                QStringLiteral("Rarely makes eye contact; overreliance on notes; stiff body language; flat emotion throughout"),
                QStringLiteral("No eye contact; volume is too soft to hear well; no use of gestures; completely relies on script")
            },
            79.2
        },
        {
            QStringLiteral("Content  \uB0B4\uC6A9"),
            {
                QStringLiteral("Details"),
                QStringLiteral("Reasons & Examples"),
                QStringLiteral("Organization"),
                QStringLiteral("Logic & Relevance"),
                QStringLiteral("Level of Vocabulary")
            },
            {
                QStringLiteral("\u2022  Creative and interesting ideas\n\u2022  Logical arguments and organization of ideas\n\u2022  Effective use of details & examples\n\u2022  Use of advanced vocabulary"),
                QString(),
                QStringLiteral("\u2022  Relevant information, but lacking explanation, detail, or examples\n\u2022  Reliance on basic vocabulary\n\u2022  Organization needs improvement"),
                QString(),
                QStringLiteral("Content was irrelevant or unsupported; only very basic vocabulary used")
            },
            70.8,
            true
        },
        {
            QStringLiteral("Overall Effort  \uC900\uBE44\uB3C4"),
            {
                QStringLiteral("Effort"),
                QStringLiteral("Prepared"),
                QStringLiteral("Memorization"),
                QStringLiteral("Visual Aids")
            },
            {
                QStringLiteral("A great amount of effort has been put in; excellent and thorough preparation was done"),
                QStringLiteral("A good amount of effort has been put in; clear evidence of preparation"),
                QStringLiteral("Some effort has been put in; preparation was done, but sufficient"),
                QStringLiteral("Little effort has been put in; not well prepared for the evaluation"),
                QStringLiteral("No effort has been put in; fully unprepared for the evaluation")
            },
            70.8
        }
    };
}

void drawCriteriaCell(
    QPainter* painter,
    const QRectF& rect,
    const RubricSection& section
    )
{
    painter->fillRect(rect, QColor(QStringLiteral("#bfbfbf")));
    drawBorder(painter, rect);

    painter->setFont(
        standardFont(
            QStringLiteral("Calibri"),
            10.0,
            QFont::Bold
            )
        );
    painter->setPen(Qt::black);
    painter->drawText(
        rect.adjusted(2.0, 3.0, -2.0, 0.0),
        Qt::AlignHCenter | Qt::AlignTop,
        section.title
        );

    painter->setFont(
        standardFont(
            QStringLiteral("Calibri"),
            6.0
            )
        );
    const qreal startY =
        rect.top() + CriteriaMetricsTopInset;

    for (int index = 0; index < section.criteria.size(); ++index)
    {
        const QRectF lineRect(
            rect.left() + 4.0,
            startY + (CriteriaMetricPitch * index),
            rect.width() - 6.0,
            CriteriaMetricHeight
            );

        painter->drawText(
            lineRect,
            Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("•  %1")
                .arg(section.criteria[index])
            );
    }
}

void drawGradeCell(
    QPainter* painter,
    const QRectF& rect,
    const QString& grade,
    bool selected
    )
{
    painter->fillRect(
        rect,
        selected
            ? QColor(QStringLiteral("#ffff00"))
            : QColor(QStringLiteral("#d9d9d9"))
        );
    drawBorder(painter, rect);
    drawCenteredText(
        painter,
        rect,
        grade,
        standardFont(
            QStringLiteral("Times New Roman"),
            18.0,
            QFont::Bold
            )
        );
}

void drawLogo(
    QPainter* painter,
    const QRectF& target
    )
{
    const QPixmap logo(
        QStringLiteral(":/assets/images/dyb.png")
        );

    if (logo.isNull())
    {
        return;
    }

    painter->drawPixmap(
        target,
        logo,
        QRectF(logo.rect())
        );
}

void drawSignatureImage(
    QPainter* painter,
    const QByteArray& imageData,
    const QRectF& bounds
    )
{
    if (!painter || imageData.isEmpty() || !bounds.isValid())
    {
        return;
    }

    QImage signature;
    if (!signature.loadFromData(imageData) || signature.isNull())
    {
        return;
    }

    QSizeF fittedSize(signature.size());
    fittedSize.scale(bounds.size(), Qt::KeepAspectRatio);
    const QRectF fittedBounds(
        QPointF(
            bounds.right() - fittedSize.width(),
            bounds.bottom() - fittedSize.height()
            ),
        fittedSize
        );

    painter->drawImage(fittedBounds, signature);
}

void drawAdvancedInfoField(
    QPainter* painter,
    const QRectF& rect,
    const QString& label,
    const QString& value,
    qreal valueLeft
    )
{
    painter->fillRect(
        rect,
        QColor(QStringLiteral("#e7e6e6"))
        );
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Narrow"),
            10.0,
            QFont::Bold
            )
        );
    painter->setPen(
        QColor(QStringLiteral("#c00000"))
        );
    painter->drawText(
        QRectF(
            rect.left() + 12.0,
            rect.top(),
            valueLeft - rect.left() - 12.0,
            rect.height()
            ),
        Qt::AlignLeft | Qt::AlignVCenter,
        label
        );
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial"),
            14.0
            )
        );
    painter->setPen(Qt::black);
    painter->drawText(
        QRectF(
            valueLeft,
            rect.top(),
            rect.right() - valueLeft,
            rect.height()
            ),
        Qt::AlignLeft | Qt::AlignVCenter,
        value
        );
}

void drawAdvancedCriteriaCell(
    QPainter* painter,
    const QRectF& rect,
    const RubricSection& section,
    const QString& score,
    qreal titleHeight
    )
{
    const QRectF titleRect(
        rect.left(),
        rect.top(),
        rect.width(),
        titleHeight
        );
    const QRectF criteriaRect(
        rect.left(),
        titleRect.bottom(),
        rect.width(),
        rect.height() - titleHeight
        );

    painter->fillRect(
        titleRect,
        QColor(QStringLiteral("#c00000"))
        );
    painter->fillRect(
        criteriaRect,
        QColor(QStringLiteral("#e5e5e7"))
        );
    drawBorder(painter, titleRect, 0.5);
    drawBorder(painter, criteriaRect, 0.5);

    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Black"),
            10.0,
            QFont::Bold
            )
        );
    painter->setPen(Qt::white);
    painter->drawText(
        titleRect,
        Qt::AlignCenter,
        section.title
        );

    painter->setFont(
        advancedFont(
            QStringLiteral("Arial"),
            6.0
            )
        );
    painter->setPen(Qt::black);
    constexpr qreal criteriaTextWidth = 83.47;
    painter->drawLine(
        QPointF(criteriaRect.left() + criteriaTextWidth, criteriaRect.top()),
        QPointF(criteriaRect.left() + criteriaTextWidth, criteriaRect.bottom())
        );
    QStringList criteriaLines;

    for (const QString& criterion : section.criteria)
    {
        criteriaLines.append(
            QStringLiteral("\u2022  %1")
                .arg(criterion)
            );
    }

    painter->drawText(
        QRectF(
            criteriaRect.left() + 4.0,
            criteriaRect.top(),
            criteriaTextWidth - 7.0,
            criteriaRect.height()
            ),
        Qt::AlignLeft | Qt::AlignVCenter,
        criteriaLines.join(QLatin1Char('\n'))
        );

    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Black"),
            20.0
            )
        );
    painter->drawText(
        QRectF(
            criteriaRect.left() + criteriaTextWidth,
            criteriaRect.top(),
            criteriaRect.width() - criteriaTextWidth,
            criteriaRect.height()
            ),
        Qt::AlignCenter,
        score
        );
}

void drawAdvancedGradeCell(
    QPainter* painter,
    const QRectF& rect,
    const QString& grade,
    bool selected
    )
{
    painter->fillRect(
        rect,
        selected
            ? QColor(QStringLiteral("#ffff00"))
            : QColor(QStringLiteral("#e5e5e7"))
        );
    drawBorder(painter, rect, 0.5);
    drawCenteredText(
        painter,
        rect,
        grade,
        advancedFont(
            QStringLiteral("Arial Narrow"),
            12.0,
            QFont::Bold
            )
        );

}

void drawAdvancedReport(
    QPainter* painter,
    const SpeakingEvalReportData& data,
    const QString& overallGrade
    )
{
    constexpr qreal tableLeft = 17.28;
    constexpr qreal tableTop = 167.27;
    constexpr qreal tableWidth = 505.34;
    constexpr qreal criteriaWidth = 134.97;
    constexpr qreal titleHeight = 21.60;
    constexpr std::array<qreal, 5> gradeWidths{
        73.80,
        73.80,
        73.80,
        73.80,
        75.17
    };

    painter->fillRect(
        QRectF(
            QPointF(),
            speakingEvalReportTemplateLayout(
                SpeakingEvalReportTemplate::Advanced
                ).pageSize
            ),
        Qt::white
        );
    painter->fillRect(
        QRectF(0.0, 0.0, 102.0, 93.0),
        QColor(QStringLiteral("#e7e6e6"))
        );
    painter->fillRect(
        QRectF(102.0, 0.0, 438.0, 93.0),
        QColor(QStringLiteral("#bd0c17"))
        );
    drawLogo(
        painter,
        QRectF(8.0, 7.0, 85.0, 72.0)
        );

    painter->setPen(Qt::white);
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Black"),
            32.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(108.0, 0.0, 264.0, 43.0),
        Qt::AlignCenter,
        QStringLiteral("SPEAKING")
        );
    painter->drawText(
        QRectF(108.0, 43.0, 264.0, 43.0),
        Qt::AlignCenter,
        QStringLiteral("EVALUATION")
        );

    painter->fillRect(
        QRectF(385.60, 14.47, 125.10, 24.23),
        Qt::white
        );
    painter->fillRect(
        QRectF(414.33, 52.86, 96.43, 24.23),
        Qt::white
        );
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Narrow"),
            14.0,
            QFont::Bold
            )
        );
    painter->setPen(Qt::black);
    painter->drawText(
        QRectF(385.60, 14.47, 125.10, 24.23),
        Qt::AlignCenter,
        data.date
        );
    painter->drawText(
        QRectF(414.33, 52.86, 96.43, 24.23),
        Qt::AlignCenter,
        data.classLabel
        );

    drawAdvancedInfoField(
        painter,
        QRectF(18.03, 103.14, 217.0, 24.23),
        QStringLiteral("English Name:"),
        data.englishName,
        102.0
        );
    drawAdvancedInfoField(
        painter,
        QRectF(242.0, 103.14, 165.0, 24.23),
        QStringLiteral("Korean Name:"),
        data.koreanName,
        332.25
        );
    drawAdvancedInfoField(
        painter,
        QRectF(18.03, 132.88, 217.0, 24.23),
        QStringLiteral("Native Teacher:"),
        data.nativeTeacher,
        102.0
        );
    drawAdvancedInfoField(
        painter,
        QRectF(242.0, 132.88, 165.0, 24.23),
        QStringLiteral("Korean Teacher:"),
        data.koreanTeacher,
        332.25
        );

    painter->fillRect(
        QRectF(412.88, 103.14, 109.13, 53.97),
        QColor(QStringLiteral("#e7e6e6"))
        );
    painter->setPen(Qt::black);
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Black"),
            24.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(412.88, 105.94, 109.13, 36.35),
        Qt::AlignCenter,
        overallGrade
        );
    painter->setPen(
        QColor(QStringLiteral("#c00000"))
        );
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Narrow"),
            12.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(413.0, 135.31, 108.93, 21.81),
        Qt::AlignCenter,
        QStringLiteral("OVERALL GRADE")
        );

    const QStringList grades{
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };
    qreal sectionTop = tableTop;

    const QList<RubricSection> sections =
        advancedRubricSections();

    for (int sectionIndex = 0;
         sectionIndex < sections.size();
         ++sectionIndex)
    {
        const RubricSection& section =
            sections[sectionIndex];
        const QRectF criteriaRect(
            tableLeft,
            sectionTop,
            criteriaWidth,
            section.height
            );

        drawAdvancedCriteriaCell(
            painter,
            criteriaRect,
            section,
            data.scores[sectionIndex],
            titleHeight
            );

        qreal gradeLeft =
            tableLeft + criteriaWidth;

        for (int gradeIndex = 0; gradeIndex < grades.size(); ++gradeIndex)
        {
            drawAdvancedGradeCell(
                painter,
                QRectF(
                    gradeLeft,
                    sectionTop,
                    gradeWidths[gradeIndex],
                    titleHeight
                    ),
                grades[gradeIndex],
                data.scores[sectionIndex] == grades[gradeIndex]
                );
            gradeLeft +=
                gradeWidths[gradeIndex];
        }

        const qreal descriptionTop =
            sectionTop + titleHeight;
        const qreal descriptionHeight =
            section.height - titleHeight;

        if (section.hasMergedDescriptions)
        {
            const std::array<qreal, 3> widths{
                gradeWidths[0] + gradeWidths[1],
                gradeWidths[2] + gradeWidths[3],
                gradeWidths[4]
            };
            const std::array<int, 3> descriptionIndexes{ 0, 2, 4 };
            qreal left = tableLeft + criteriaWidth;

            for (int index = 0; index < widths.size(); ++index)
            {
                const QRectF rect(
                    left,
                    descriptionTop,
                    widths[index],
                    descriptionHeight
                    );
                painter->fillRect(rect, Qt::white);
                drawBorder(painter, rect, 0.5);
                const QFont descriptionFont =
                    advancedFont(
                        QStringLiteral("Arial"),
                        7.0
                        );

                if (index < 2)
                {
                    drawAdvancedBulletList(
                        painter,
                        rect.adjusted(1.0, 1.0, -1.0, -1.0),
                        section.descriptions[descriptionIndexes[index]],
                        descriptionFont
                        );
                }
                else
                {
                    drawCenteredText(
                        painter,
                        rect.adjusted(1.0, 1.0, -1.0, -1.0),
                        section.descriptions[descriptionIndexes[index]],
                        descriptionFont
                        );
                }
                left += widths[index];
            }
        }
        else
        {
            qreal descriptionLeft =
                tableLeft + criteriaWidth;

            for (int gradeIndex = 0; gradeIndex < grades.size(); ++gradeIndex)
            {
                const QRectF rect(
                    descriptionLeft,
                    descriptionTop,
                    gradeWidths[gradeIndex],
                    descriptionHeight
                    );
                painter->fillRect(rect, Qt::white);
                drawBorder(painter, rect, 0.5);
                drawCenteredText(
                    painter,
                    rect.adjusted(1.0, 1.0, -1.0, -1.0),
                    section.descriptions[gradeIndex],
                    advancedFont(
                        QStringLiteral("Arial"),
                        7.0
                        )
                    );
                descriptionLeft +=
                    gradeWidths[gradeIndex];
            }
        }

        sectionTop += section.height;
    }

    const QRectF commentTitleRect(
        tableLeft,
        632.87,
        tableWidth,
        17.28
        );
    const QRectF commentRect(
        tableLeft,
        650.15,
        tableWidth,
        93.90
        );
    painter->fillRect(
        commentTitleRect,
        QColor(QStringLiteral("#c00000"))
        );
    drawBorder(painter, commentTitleRect, 0.5);
    drawBorder(painter, commentRect, 0.5);
    painter->setPen(Qt::white);
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial Black"),
            10.0,
            QFont::Bold
            )
        );
    painter->drawText(
        commentTitleRect.adjusted(7.2, 0.0, 0.0, 0.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        QStringLiteral("Comments  \uCF54\uBA58\uD2B8")
        );
    painter->setPen(Qt::black);
    painter->setFont(
        advancedFont(
            QStringLiteral("Just Another Hand"),
            15.0
            )
        );
    painter->drawText(
        commentRect.adjusted(7.2, 5.0, -7.2, -5.0),
        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
        data.comments
        );
    painter->setPen(
        QColor(QStringLiteral("#c00000"))
        );
    painter->setFont(
        advancedFont(
            QStringLiteral("Arial"),
            12.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(252.99, 748.84, 160.00, 21.81),
        Qt::AlignCenter,
        QStringLiteral("Native Teacher Signature:")
        );
    drawSignatureImage(
        painter,
        data.signatureImage,
        speakingEvalReportTemplateLayout(
            data.reportTemplate
            ).signatureBounds
        );
}

} // namespace
