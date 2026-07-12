#include "speaking_eval_report_widget.h"

#include "core/fontmanager.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace
{

constexpr QSizeF ReportSize(540.0, 780.0);
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
    update();
}

QSize SpeakingEvalReportWidget::sizeHint() const
{
    return QSize(810, 1170);
}

QSize SpeakingEvalReportWidget::minimumSizeHint() const
{
    return sizeHint();
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

    const qreal scale =
        std::min(
            targetRect.width() / ReportSize.width(),
            targetRect.height() / ReportSize.height()
            );

    const QPointF origin(
        targetRect.left() + ((targetRect.width() - (ReportSize.width() * scale)) / 2.0),
        targetRect.top() + ((targetRect.height() - (ReportSize.height() * scale)) / 2.0)
        );

    painter->save();
    painter->translate(origin);
    painter->scale(scale, scale);
    painter->fillRect(QRectF(QPointF(), ReportSize), Qt::white);

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

    painter->setFont(
        reportFont(
            23.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(49.0, 25.0, 52.0, 38.0),
        Qt::AlignCenter,
        QStringLiteral("DYR")
        );

    painter->setFont(
        reportFont(
            6.5,
            QFont::DemiBold
            )
        );
    painter->drawText(
        QRectF(30.0, 64.0, 82.0, 10.0),
        Qt::AlignCenter,
        tr("Do Your Best!")
        );

    QPen titleLinePen(QColor(QStringLiteral("#6f6f6f")));
    titleLinePen.setWidthF(1.3);
    painter->setPen(titleLinePen);
    painter->drawLine(QPointF(99.0, 66.0), QPointF(505.0, 66.0));

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
