#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include <QFrame>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTextCursor>

SpeakingEvalReportWidget::SpeakingEvalReportWidget(
    QWidget* parent
    )
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
    setFixedSize(sizeHint());

    m_commentEditor =
        new QPlainTextEdit(this);
    m_commentEditor->setObjectName(
        QStringLiteral("speakingEvalReportComments")
        );
    m_commentEditor->setFrameShape(QFrame::NoFrame);
    m_commentEditor->setTabChangesFocus(true);
    m_commentEditor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_commentEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_commentEditor->setPlaceholderText(tr("Type the student's report comment…"));
    m_commentEditor->hide();

    connect(
        m_commentEditor,
        &QPlainTextEdit::textChanged,
        this,
        [this]()
        {
            if (!m_interactive || !m_commentEditor)
            {
                return;
            }

            QString comments = m_commentEditor->toPlainText();
            constexpr int maximumCommentLength = 900;
            if (comments.size() > maximumCommentLength)
            {
                comments.truncate(maximumCommentLength);
                const QSignalBlocker blocker(m_commentEditor);
                m_commentEditor->setPlainText(comments);
                m_commentEditor->moveCursor(QTextCursor::End);
            }

            m_data.comments = comments;
            emit commentsEdited(m_data.comments);
            update();
        }
        );
}

void SpeakingEvalReportWidget::setReportData(
    const SpeakingEvalReportData& data
    )
{
    m_data = data;
    setFixedSize(sizeHint());
    updateCommentEditor();
    update();
}

void SpeakingEvalReportWidget::setInteractive(
    bool interactive
    )
{
    if (m_interactive == interactive)
    {
        return;
    }

    m_interactive = interactive;
    updateCommentEditor();
    unsetCursor();
}

bool SpeakingEvalReportWidget::isInteractive() const
{
    return m_interactive;
}

QSize SpeakingEvalReportWidget::sizeHint() const
{
    const QSizeF reportSize =
        speakingEvalReportTemplateLayout(
            reportTemplate()
            ).pageSize;
    return (reportSize * 1.5).toSize();
}

QSize SpeakingEvalReportWidget::minimumSizeHint() const
{
    return sizeHint();
}

bool SpeakingEvalReportWidget::usesAdvancedTemplate() const
{
    return speakingEvalReportTemplateLayout(
        reportTemplate()
        ).usesAdvancedScoreTable;
}

SpeakingEvalReportTemplate SpeakingEvalReportWidget::reportTemplate() const
{
    return m_data.reportTemplate;
}
