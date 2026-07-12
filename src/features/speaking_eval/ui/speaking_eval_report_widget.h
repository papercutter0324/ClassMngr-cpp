#pragma once

#include <QWidget>

#include <array>
#include <utility>

class QPainter;
class QPlainTextEdit;

struct SpeakingEvalReportData
{
    QString englishName;
    QString koreanName;
    QString classLabel;
    QString nativeTeacher;
    QString koreanTeacher;
    QString date;
    QString comments;
    QString notes;
    std::array<QString, 6> scores;
    bool useAdvancedTemplate = false;
};

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

    QString overallGrade() const;

private:
    SpeakingEvalReportData m_data;
    QPlainTextEdit* m_commentEditor = nullptr;
    bool m_interactive = false;
};
