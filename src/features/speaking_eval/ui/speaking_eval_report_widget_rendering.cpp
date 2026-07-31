#include "speaking_eval_report_renderer_p.h"

void SpeakingEvalReportWidget::paintReport(
    QPainter* painter,
    const QRectF& targetRect
    ) const
{
    if (!painter || !targetRect.isValid())
    {
        return;
    }

    const SpeakingEvalReportTemplateLayout& templateLayout =
        speakingEvalReportTemplateLayout(
            reportTemplate()
            );
    const QSizeF reportSize =
        templateLayout.pageSize;

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
    borderPen.setWidthF(1.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(
        RegularBorderRect.adjusted(-1.5, -1.5, 1.5, 1.5)
        );
    painter->drawRect(
        RegularBorderRect.adjusted(1.5, 1.5, -1.5, -1.5)
        );

    painter->setPen(QColor(QStringLiteral("#c00000")));
    painter->setFont(
        standardFont(
            QStringLiteral("Calibri"),
            28.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(99.29984, 20.43748, 402.8709, 45.37504),
        Qt::AlignCenter | Qt::AlignVCenter,
        tr("Speaking Evaluation")
        );

    drawLogo(
        painter,
        QRectF(29.71654, 27.20937, 61.24598, 51.74795)
        );

    QPen titleLinePen(QColor(QStringLiteral("#7f7f7f")));
    titleLinePen.setWidthF(1.5);
    painter->setPen(titleLinePen);
    painter->drawLine(
        QPointF(99.29984, 66.0),
        QPointF(503.97084, 66.0)
        );

    drawLabelAndValue(
        painter,
        QRectF(32.48087, 78.53858, 84.18748, 24.0),
        QRectF(116.66835, 78.53858, 98.1911, 24.0),
        QRectF(116.9999, 76.73653, 95.80008, 26.65299),
        tr("English Name:"),
        m_data.englishName
        );
    drawLabelAndValue(
        painter,
        QRectF(214.85945, 78.53858, 89.49732, 24.0),
        QRectF(304.35677, 78.53858, 77.56433, 24.0),
        QRectF(304.05, 77.34701, 76.95, 23.02268),
        tr("Korean Name:"),
        m_data.koreanName,
        true
        );
    drawLabelAndValue(
        painter,
        QRectF(381.9211, 78.53858, 41.30976, 24.0),
        QRectF(423.23086, 78.53858, 81.58197, 24.0),
        QRectF(423.0001, 76.73653, 81.6, 26.64),
        tr("Grade:"),
        m_data.classLabel
        );
    drawLabelAndValue(
        painter,
        QRectF(32.48087, 102.53858, 84.18748, 21.6),
        QRectF(116.66835, 102.53858, 98.1911, 21.6),
        QRectF(117.7499, 102.1399, 95.80008, 23.74969),
        tr("Native Teacher:"),
        m_data.nativeTeacher
        );
    drawLabelAndValue(
        painter,
        QRectF(214.85945, 102.53858, 89.49732, 21.6),
        QRectF(304.35677, 102.53858, 77.56433, 21.6),
        QRectF(304.05, 100.1782, 76.95, 23.02268),
        tr("Korean Teacher:"),
        m_data.koreanTeacher,
        true
        );
    drawLabelAndValue(
        painter,
        QRectF(381.9211, 102.53858, 41.30976, 21.6),
        QRectF(423.23086, 102.53858, 81.58197, 21.6),
        QRectF(414.1999, 100.08, 99.20032, 25.92),
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
                standardFont(
                    QStringLiteral("Calibri"),
                    8.0
                    ),
                Qt::AlignLeft
                );
            drawCenteredText(
                painter,
                secondRect.adjusted(3.0, 2.0, -3.0, -2.0),
                section.descriptions[2],
                standardFont(
                    QStringLiteral("Calibri"),
                    8.0
                    ),
                Qt::AlignLeft
                );
            drawCenteredText(
                painter,
                thirdRect.adjusted(2.0, 2.0, -2.0, -2.0),
                section.descriptions[4],
                standardFont(
                    QStringLiteral("Calibri"),
                    8.0
                    )
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
                    standardFont(
                        QStringLiteral("Calibri"),
                        8.0
                        )
                    );
            }
        }

        sectionTop +=
            section.height;
    }

    painter->setPen(Qt::black);
    painter->setFont(
        standardFont(
            QStringLiteral("Calibri"),
            14.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(38.1374, 575.7059, 87.8626, 20.63441),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("Comments:")
        );

    const QRectF commentsRect(
        34.87504,
        594.75,
        470.0625,
        131.25
        );
    painter->fillRect(commentsRect, Qt::white);
    drawBorder(painter, commentsRect);

    painter->setPen(Qt::black);
    painter->setFont(
        standardFont(
            QStringLiteral("Segoe UI Semibold"),
            18.0
            )
        );
    painter->drawText(
        commentsRect.adjusted(5.0, 5.0, -5.0, -5.0),
        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
        m_data.comments
        );

    painter->setFont(
        standardFont(
            QStringLiteral("Calibri"),
            14.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(38.1374, 734.0623, 93.8626, 24.23441),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("Overall Grade:")
        );

    painter->setFont(
        standardFont(
            QStringLiteral("Times New Roman"),
            18.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(120.9843, 735.0, 43.2, 21.56252),
        Qt::AlignCenter | Qt::AlignVCenter,
        overallGrade()
        );

    painter->setFont(
        standardFont(
            QStringLiteral("Calibri"),
            14.0,
            QFont::Bold
            )
        );
    painter->drawText(
        QRectF(223.2, 734.0623, 160.8, 24.23441),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("Native Teacher Signature:")
        );
    drawSignatureImage(
        painter,
        m_data.signatureImage,
        templateLayout.signatureBounds
        );

    painter->restore();
}
