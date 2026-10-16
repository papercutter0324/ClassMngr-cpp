#pragma once

#include "features/speaking_eval/services/speaking_eval_report_data.h"
#include "features/speaking_eval/ui/speaking_eval_report_painter.h"

#include <QWidget>

#include <utility>

class QPainter;
class SpeakingEvalCommentEdit;

class SpeakingEvalReportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpeakingEvalReportWidget(
        QWidget* parent = nullptr
        );

    void setReportData(
        const SpeakingEvalReportData& data
        );

    void setInteractive(
        bool interactive
        );

    [[nodiscard]] bool isInteractive() const;

    QSize sizeHint() const override;

    QSize minimumSizeHint() const override;

    void paintReport(
        QPainter* painter,
        const QRectF& targetRect
        ) const;

    bool usesAdvancedTemplate() const;

    [[nodiscard]] SpeakingEvalReportTemplate reportTemplate() const;

signals:
    void scoreEdited(
        int metricIndex,
        const QString& score
        );

    void commentsEdited(
        const QString& comments
        );

protected:
    void paintEvent(
        QPaintEvent* event
        ) override;

    void mousePressEvent(
        QMouseEvent* event
        ) override;

    void mouseMoveEvent(
        QMouseEvent* event
        ) override;

    void resizeEvent(
        QResizeEvent* event
        ) override;

private:
    [[nodiscard]] QPointF reportPoint(
        const QPointF& widgetPoint
        ) const;

    [[nodiscard]] QRect reportRect(
        const QRectF& reportRect
        ) const;

    [[nodiscard]] std::pair<int, QString> scoreAt(
        const QPointF& reportPoint
        ) const;

    void updateCommentEditor();

private:
    SpeakingEvalReportData m_data;
    SpeakingEvalCommentEdit* m_commentEditor = nullptr;
    bool m_interactive = false;
    SpeakingEvalReportPainter m_reportPainter;
};
