#include "speaking_eval_report_widget.h"

#include "core/fontmanager.h"

#include <QPainter>
#include <QPaintEvent>
#include <QFontMetricsF>
#include <QPixmap>
#include <QRegularExpression>

#include <algorithm>

namespace
{

constexpr QSizeF RegularReportSize(540.0, 780.0);
constexpr QSizeF AdvancedReportSize(540.0, 780.0);
constexpr QRectF PageRect(16.0, 16.0, 508.0, 748.0);
constexpr qreal TableLeft = 34.0;
constexpr qreal TableTop = 125.0;
constexpr qreal TableWidth = 472.0;
constexpr qreal CriteriaWidth = 76.0;
constexpr qreal GradeWidth = (TableWidth - CriteriaWidth) / 5.0;
constexpr qreal GradeHeaderHeight = 23.0;

struct RubricSection
{
    QString title;
    QStringList criteria;
    std::array<QString, 5> descriptions;
    qreal height = 0.0;
    bool hasMergedDescriptions = false;
};

QFont reportFont(
    qreal pointSize,
    int weight = QFont::Normal
    )
{
    QFont font =
        FontManager::getUiFont(
            qRound(pointSize),
            weight
            );

    font.setPointSizeF(pointSize);
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
    const QRectF& valueRect,
    const QString& label,
    const QString& value,
    bool koreanValue = false
    )
{
    painter->setFont(
        reportFont(
            8.2,
            QFont::DemiBold
            )
        );
    painter->setPen(Qt::black);
    painter->drawText(
        labelRect,
        Qt::AlignLeft | Qt::AlignVCenter,
        label
        );

    QPen linePen(Qt::black);
    linePen.setWidthF(0.85);
    painter->setPen(linePen);
    painter->drawLine(
        valueRect.bottomLeft(),
        valueRect.bottomRight()
        );

    painter->setFont(
        koreanValue
            ? FontManager::getKoreanFont(11)
            : reportFont(10.5)
        );
    painter->setPen(Qt::black);
    painter->drawText(
        valueRect.adjusted(2.0, 0.0, -2.0, -1.5),
        Qt::AlignCenter | Qt::AlignVCenter,
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
            73.0
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
            73.0
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
            73.0
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
            73.0,
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
            73.0
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
    painter->fillRect(rect, QColor(QStringLiteral("#b7b7b7")));
    drawBorder(painter, rect);

    painter->setFont(
        reportFont(
            8.6,
            QFont::DemiBold
            )
        );
    painter->setPen(Qt::black);
    painter->drawText(
        rect.adjusted(2.0, 3.0, -2.0, 0.0),
        Qt::AlignHCenter | Qt::AlignTop,
        section.title
        );

    painter->setFont(reportFont(4.2));
    const qreal startY =
        rect.top() + 18.5;

    const qreal lineHeight =
        5.65;

    for (int index = 0; index < section.criteria.size(); ++index)
    {
        const QRectF lineRect(
            rect.left() + 4.0,
            startY + (lineHeight * index),
            rect.width() - 6.0,
            lineHeight
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
    painter->fillRect(rect, QColor(QStringLiteral("#d9d9d9")));
    drawBorder(painter, rect);
    drawCenteredText(
        painter,
        rect,
        grade,
        reportFont(
            17.0,
            QFont::DemiBold
            )
        );

    if (!selected)
    {
        return;
    }

    QPen selectionPen(QColor(QStringLiteral("#c00000")));
    selectionPen.setWidthF(1.8);
    painter->setPen(selectionPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect.adjusted(1.5, 1.5, -1.5, -1.5));
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
        QRectF(QPointF(), AdvancedReportSize),
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
}

} // namespace

SpeakingEvalReportWidget::SpeakingEvalReportWidget(
    QWidget* parent
    )
    : QWidget(parent)
{
    setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
    setFixedSize(sizeHint());
}

void SpeakingEvalReportWidget::setReportData(
    const SpeakingEvalReportData& data
    )
{
    m_data = data;
    setFixedSize(sizeHint());
    update();
}

QSize SpeakingEvalReportWidget::sizeHint() const
{
    if (usesAdvancedTemplate())
    {
        return QSize(810, 1170);
    }

    return QSize(810, 1170);
}

QSize SpeakingEvalReportWidget::minimumSizeHint() const
{
    return sizeHint();
}

bool SpeakingEvalReportWidget::usesAdvancedTemplate() const
{
    return m_data.useAdvancedTemplate;
}

void SpeakingEvalReportWidget::paintReport(
    QPainter* painter,
    const QRectF& targetRect
    ) const
{
    if (!painter || !targetRect.isValid())
    {
        return;
    }

    const QSizeF reportSize =
        usesAdvancedTemplate()
            ? AdvancedReportSize
            : RegularReportSize;

    const qreal scale =
        std::min(
            targetRect.width() / reportSize.width(),
            targetRect.height() / reportSize.height()
            );

    const QPointF origin(
        targetRect.left() + ((targetRect.width() - (reportSize.width() * scale)) / 2.0),
        targetRect.top() + ((targetRect.height() - (reportSize.height() * scale)) / 2.0)
        );

    painter->save();
    painter->translate(origin);
    painter->scale(scale, scale);
    painter->fillRect(QRectF(QPointF(), reportSize), Qt::white);

    if (usesAdvancedTemplate())
    {
        drawAdvancedReport(
            painter,
            m_data,
            overallGrade()
            );
        painter->restore();
        return;
    }

    QPen borderPen(Qt::black);
    borderPen.setWidthF(2.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(PageRect);

    QPen innerBorderPen(Qt::black);
    innerBorderPen.setWidthF(0.7);
    painter->setPen(innerBorderPen);
    painter->drawRect(PageRect.adjusted(3.0, 3.0, -3.0, -3.0));

    painter->setPen(QColor(QStringLiteral("#bd0000")));
    painter->setFont(
        reportFont(
            22.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(155.0, 27.0, 315.0, 33.0),
        Qt::AlignCenter,
        tr("Speaking Evaluation")
        );

    drawLogo(
        painter,
        QRectF(33.0, 24.0, 80.0, 52.0)
        );

    QPen titleLinePen(QColor(QStringLiteral("#6f6f6f")));
    titleLinePen.setWidthF(1.3);
    painter->setPen(titleLinePen);
    painter->drawLine(QPointF(118.0, 66.0), QPointF(505.0, 66.0));

    drawLabelAndValue(
        painter,
        QRectF(36.0, 82.0, 89.0, 18.0),
        QRectF(121.0, 82.0, 91.0, 18.0),
        tr("English Name:"),
        m_data.englishName
        );
    drawLabelAndValue(
        painter,
        QRectF(220.0, 82.0, 90.0, 18.0),
        QRectF(307.0, 82.0, 72.0, 18.0),
        tr("Korean Name:"),
        m_data.koreanName,
        true
        );
    drawLabelAndValue(
        painter,
        QRectF(387.0, 82.0, 44.0, 18.0),
        QRectF(429.0, 82.0, 75.0, 18.0),
        tr("Grade:"),
        m_data.classLabel
        );
    drawLabelAndValue(
        painter,
        QRectF(36.0, 105.0, 92.0, 18.0),
        QRectF(121.0, 105.0, 91.0, 18.0),
        tr("Native Teacher:"),
        m_data.nativeTeacher
        );
    drawLabelAndValue(
        painter,
        QRectF(220.0, 105.0, 90.0, 18.0),
        QRectF(307.0, 105.0, 72.0, 18.0),
        tr("Korean Teacher:"),
        m_data.koreanTeacher,
        true
        );
    drawLabelAndValue(
        painter,
        QRectF(387.0, 105.0, 44.0, 18.0),
        QRectF(429.0, 105.0, 75.0, 18.0),
        tr("Date:"),
        m_data.date
        );

    const QStringList grades{
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };

    qreal sectionTop =
        TableTop;

    const QList<RubricSection> sections =
        rubricSections();

    for (int sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
    {
        const RubricSection& section =
            sections[sectionIndex];

        const QRectF criteriaRect(
            TableLeft,
            sectionTop,
            CriteriaWidth,
            section.height
            );

        drawCriteriaCell(
            painter,
            criteriaRect,
            section
            );

        for (int gradeIndex = 0; gradeIndex < grades.size(); ++gradeIndex)
        {
            const QRectF gradeRect(
                TableLeft + CriteriaWidth + (GradeWidth * gradeIndex),
                sectionTop,
                GradeWidth,
                GradeHeaderHeight
                );

            drawGradeCell(
                painter,
                gradeRect,
                grades[gradeIndex],
                m_data.scores[sectionIndex] == grades[gradeIndex]
                );
        }

        const qreal descriptionTop =
            sectionTop + GradeHeaderHeight;
        const qreal descriptionHeight =
            section.height - GradeHeaderHeight;

        if (section.hasMergedDescriptions)
        {
            const QRectF firstRect(
                TableLeft + CriteriaWidth,
                descriptionTop,
                GradeWidth * 2.0,
                descriptionHeight
                );
            const QRectF secondRect(
                firstRect.right(),
                descriptionTop,
                GradeWidth * 2.0,
                descriptionHeight
                );
            const QRectF thirdRect(
                secondRect.right(),
                descriptionTop,
                GradeWidth,
                descriptionHeight
                );

            for (const QRectF& rect : { firstRect, secondRect, thirdRect })
            {
                painter->fillRect(rect, Qt::white);
                drawBorder(painter, rect);
            }

            drawCenteredText(
                painter,
                firstRect.adjusted(3.0, 2.0, -3.0, -2.0),
                section.descriptions[0],
                reportFont(5.1),
                Qt::AlignLeft
                );
            drawCenteredText(
                painter,
                secondRect.adjusted(3.0, 2.0, -3.0, -2.0),
                section.descriptions[2],
                reportFont(5.1),
                Qt::AlignLeft
                );
            drawCenteredText(
                painter,
                thirdRect.adjusted(2.0, 2.0, -2.0, -2.0),
                section.descriptions[4],
                reportFont(5.1)
                );
        }
        else
        {
            for (int gradeIndex = 0; gradeIndex < grades.size(); ++gradeIndex)
            {
                const QRectF descriptionRect(
                    TableLeft + CriteriaWidth + (GradeWidth * gradeIndex),
                    descriptionTop,
                    GradeWidth,
                    descriptionHeight
                    );

                painter->fillRect(descriptionRect, Qt::white);
                drawBorder(painter, descriptionRect);
                drawCenteredText(
                    painter,
                    descriptionRect.adjusted(3.0, 2.0, -3.0, -2.0),
                    section.descriptions[gradeIndex],
                    reportFont(5.1)
                    );
            }
        }

        sectionTop +=
            section.height;
    }

    painter->setPen(Qt::black);
    painter->setFont(
        reportFont(
            12.0,
            QFont::DemiBold
            )
        );
    painter->drawText(
        QRectF(37.0, 576.0, 135.0, 17.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("Comments:")
        );

    const QRectF commentsRect(34.0, 594.0, 472.0, 132.0);
    painter->fillRect(commentsRect, Qt::white);
    drawBorder(painter, commentsRect);

    painter->setPen(Qt::black);
    painter->setFont(reportFont(9.5));
    painter->drawText(
        commentsRect.adjusted(5.0, 5.0, -5.0, -5.0),
        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
        m_data.comments
        );

    painter->setFont(
        reportFont(
            12.0,
            QFont::DemiBold
            )
        );
    painter->drawText(
        QRectF(37.0, 736.0, 111.0, 18.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("Overall Grade:")
        );

    painter->setFont(
        reportFont(
            14.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(147.0, 736.0, 53.0, 18.0),
        Qt::AlignCenter | Qt::AlignVCenter,
        overallGrade()
        );

    painter->setFont(
        reportFont(
            12.0,
            QFont::DemiBold
            )
        );
    painter->drawText(
        QRectF(224.0, 736.0, 210.0, 18.0),
        Qt::AlignCenter | Qt::AlignVCenter,
        tr("Native Teacher Signature:")
        );

    painter->restore();
}

void SpeakingEvalReportWidget::paintEvent(
    QPaintEvent* event
    )
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    paintReport(
        &painter,
        rect()
        );
}

QString SpeakingEvalReportWidget::overallGrade() const
{
    const QHash<QString, int> gradeValues{
        { QStringLiteral("C"), 1 },
        { QStringLiteral("B"), 2 },
        { QStringLiteral("B+"), 3 },
        { QStringLiteral("A"), 4 },
        { QStringLiteral("A+"), 5 }
    };
    const QStringList grades{
        QStringLiteral("C"),
        QStringLiteral("B"),
        QStringLiteral("B+"),
        QStringLiteral("A"),
        QStringLiteral("A+")
    };

    int sum = 0;

    for (const QString& score : m_data.scores)
    {
        if (!gradeValues.contains(score))
        {
            return QStringLiteral("N/A");
        }

        sum +=
            gradeValues.value(score);
    }

    const double average =
        static_cast<double>(sum) / m_data.scores.size();

    int rounded =
        static_cast<int>(average);

    if (average - rounded >= 0.4)
    {
        ++rounded;
    }

    return grades.value(
        qBound(1, rounded, 5) - 1,
        QStringLiteral("N/A")
        );
}
