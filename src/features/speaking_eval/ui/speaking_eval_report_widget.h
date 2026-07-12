#pragma once

#include <QWidget>

#include <array>

class QPainter;

struct SpeakingEvalReportData
{
    QString englishName;
    QString koreanName;
    QString classLabel;
    QString nativeTeacher;
    QString koreanTeacher;
    QString date;
    QString comments;
    std::array<QString, 6> scores;
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

    QSize sizeHint() const override;

    QSize minimumSizeHint() const override;

    void paintReport(
        QPainter* painter,
        const QRectF& targetRect
        ) const;

protected:
    void paintEvent(
        QPaintEvent* event
        ) override;

private:
    QString overallGrade() const;

private:
    SpeakingEvalReportData m_data;
};
