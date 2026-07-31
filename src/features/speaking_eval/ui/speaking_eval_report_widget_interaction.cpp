#include "speaking_eval_report_renderer_p.h"

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

void SpeakingEvalReportWidget::mousePressEvent(
    QMouseEvent* event
    )
{
    if (!m_interactive || !event || event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    const auto [metricIndex, score] =
        scoreAt(reportPoint(event->position()));

    if (metricIndex < 0 || metricIndex >= m_data.scores.size())
    {
        QWidget::mousePressEvent(event);
        return;
    }

    const QString selectedScore =
        m_data.scores[metricIndex] == score
            ? QString()
            : score;

    m_data.scores[metricIndex] = selectedScore;
    emit scoreEdited(metricIndex, selectedScore);
    update();
    event->accept();
}

void SpeakingEvalReportWidget::mouseMoveEvent(
    QMouseEvent* event
    )
{
    if (
        m_interactive
        && event
        && scoreAt(reportPoint(event->position())).first >= 0
        )
    {
        setCursor(Qt::PointingHandCursor);
    }
    else
    {
        unsetCursor();
    }

    QWidget::mouseMoveEvent(event);
}

void SpeakingEvalReportWidget::resizeEvent(
    QResizeEvent* event
    )
{
    QWidget::resizeEvent(event);
    updateCommentEditor();
}

QPointF SpeakingEvalReportWidget::reportPoint(
    const QPointF& widgetPoint
    ) const
{
    const QSizeF reportSize =
        speakingEvalReportTemplateLayout(
            reportTemplate()
            ).pageSize;
    const qreal scale =
        std::min(
            width() / reportSize.width(),
            height() / reportSize.height()
            );

    if (scale <= 0.0)
    {
        return QPointF(-1.0, -1.0);
    }

    const QPointF origin(
        (width() - (reportSize.width() * scale)) / 2.0,
        (height() - (reportSize.height() * scale)) / 2.0
        );

    return (widgetPoint - origin) / scale;
}

QRect SpeakingEvalReportWidget::reportRect(
    const QRectF& sourceRect
    ) const
{
    const QSizeF reportSize =
        speakingEvalReportTemplateLayout(
            reportTemplate()
            ).pageSize;
    const qreal scale =
        std::min(
            width() / reportSize.width(),
            height() / reportSize.height()
            );
    const QPointF origin(
        (width() - (reportSize.width() * scale)) / 2.0,
        (height() - (reportSize.height() * scale)) / 2.0
        );

    return QRectF(
        origin + (sourceRect.topLeft() * scale),
        sourceRect.size() * scale
        ).toAlignedRect();
}

std::pair<int, QString> SpeakingEvalReportWidget::scoreAt(
    const QPointF& point
    ) const
{
    static const std::array<QString, 5> grades{
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };

    if (usesAdvancedTemplate())
    {
        constexpr qreal tableLeft = 17.28;
        constexpr qreal criteriaWidth = 134.97;
        constexpr qreal titleHeight = 21.60;
        constexpr std::array<qreal, 5> gradeWidths{
            73.80,
            73.80,
            73.80,
            73.80,
            75.17
        };
        qreal top = 167.27;
        const QList<RubricSection> sections = advancedRubricSections();

        for (int metric = 0; metric < sections.size(); ++metric)
        {
            qreal left = tableLeft + criteriaWidth;
            for (int grade = 0; grade < gradeWidths.size(); ++grade)
            {
                if (QRectF(left, top, gradeWidths[grade], titleHeight).contains(point))
                {
                    return { metric, grades[grade] };
                }
                left += gradeWidths[grade];
            }
            top += sections[metric].height;
        }

        return { -1, {} };
    }

    qreal top = TableTop;
    const QList<RubricSection> sections = rubricSections();
    for (int metric = 0; metric < sections.size(); ++metric)
    {
        for (int grade = 0; grade < grades.size(); ++grade)
        {
            const QRectF rect(
                TableLeft + CriteriaWidth + (GradeWidth * grade),
                top,
                GradeWidth,
                GradeHeaderHeight
                );
            if (rect.contains(point))
            {
                return { metric, grades[grade] };
            }
        }
        top += sections[metric].height;
    }

    return { -1, {} };
}

void SpeakingEvalReportWidget::updateCommentEditor()
{
    if (!m_commentEditor)
    {
        return;
    }

    const QSignalBlocker blocker(m_commentEditor);
    m_commentEditor->setPlainText(m_data.comments);
    m_commentEditor->setVisible(m_interactive);

    if (!m_interactive)
    {
        return;
    }

    const QRectF sourceRect =
        usesAdvancedTemplate()
            ? QRectF(17.28, 650.15, 505.34, 93.90)
            : QRectF(34.87504, 594.75, 470.0625, 131.25);
    const QRect editorRect = reportRect(sourceRect).adjusted(2, 2, -2, -2);
    m_commentEditor->setGeometry(editorRect);

    QFont font(
        usesAdvancedTemplate()
            ? QStringLiteral("Just Another Hand")
            : QStringLiteral("Segoe UI Semibold")
        );
    const qreal scale =
        width()
        / speakingEvalReportTemplateLayout(
            reportTemplate()
            ).pageSize.width();
    font.setPixelSize(
        qRound(
            (usesAdvancedTemplate() ? 15.0 : 18.0) * scale
            )
        );
    m_commentEditor->setFont(font);
    m_commentEditor->setStyleSheet(
        QStringLiteral(
            "QPlainTextEdit { background: white; color: black; border: none; padding: %1px; }"
            ).arg(qMax(2, qRound(4.0 * scale)))
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
