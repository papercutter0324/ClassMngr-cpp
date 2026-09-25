#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include "features/speaking_eval/ui/speaking_eval_comment_edit.h"
#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"
#include "next/domain/speaking_evaluation_grade.h"

#include <QByteArray>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTextDocument>

#include <algorithm>
#include <string_view>

namespace NextDomain = ClassMngr::Next::Domain;

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

    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate());
    if (!assets.valid)
    {
        return { -1, {} };
    }

    for (int metric = 0; metric < assets.scoreCells.size(); ++metric)
    {
        for (int grade = 0; grade < grades.size(); ++grade)
        {
            const QRectF rect(
                speakingEvalScoreCell(
                    reportTemplate(),
                    metric,
                    grades.at(grade)
                    )
                );
            if (rect.contains(point))
            {
                return { metric, grades[grade] };
            }
        }
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
    m_commentEditor->setStudentNames(
        m_data.englishName,
        m_data.koreanName
        );
    m_commentEditor->setPlainText(m_data.comments);
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate());
    const SpeakingEvalFieldAsset* comments =
        speakingEvalFieldAsset(
            reportTemplate(),
            QStringLiteral("comments")
            );
    m_commentEditor->setVisible(
        m_interactive
        && assets.valid
        && comments
        );

    if (!m_interactive || !assets.valid || !comments)
    {
        return;
    }

    const QRectF sourceRect =
        comments->rect.marginsRemoved(comments->margins);
    const QRect editorRect =
        reportRect(sourceRect);
    m_commentEditor->setGeometry(editorRect);

    const QSizeF reportSize =
        speakingEvalReportTemplateLayout(
            reportTemplate()
            ).pageSize;
    const qreal scale =
        std::min(
            width() / reportSize.width(),
            height() / reportSize.height()
        );
    QFont font =
        speakingEvalTemplateFont(
            comments->fontRole,
            comments->fontSizePoints * scale
            );
    font.setLetterSpacing(
        QFont::AbsoluteSpacing,
        comments->letterSpacing * scale
        );
    font.setWordSpacing(comments->wordSpacing * scale);
    m_commentEditor->setFont(font);
    m_commentEditor->document()->setDocumentMargin(0.0);
    m_commentEditor->setStyleSheet(
        QStringLiteral(
            "QPlainTextEdit { background: white; color: black; border: none; padding: 0px; }"
            )
        );
}

QString SpeakingEvalReportWidget::overallGrade() const
{
    NextDomain::SpeakingEvaluationComponentScores componentScores{};
    for (std::size_t index = 0; index < m_data.scores.size(); ++index)
    {
        const QByteArray label = m_data.scores[index].toLatin1();
        componentScores[index] =
            NextDomain::speakingEvaluationGradeFromLabel(
                std::string_view(
                    label.constData(),
                    static_cast<std::size_t>(label.size())
                    )
            );
    }

    const auto overallGrade =
        NextDomain::calculateOverallSpeakingEvaluationGrade(componentScores);
    if (!overallGrade)
    {
        return QStringLiteral("N/A");
    }

    const std::string_view label =
        NextDomain::speakingEvaluationGradeLabel(*overallGrade);
    return QString::fromLatin1(
        label.data(),
        static_cast<qsizetype>(label.size())
        );
}
