#include "next/ui/selected_class_analytics_model.h"

#include "next/domain/speaking_evaluation_grade.h"

#include <QColor>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPolygonF>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>

namespace ClassMngr::Next::Ui
{
namespace
{

QString gradeText(const Domain::SpeakingEvaluationGrade grade)
{
    const std::optional<std::string_view> label =
        Domain::speakingEvaluationGradeLabel(grade);
    return label ? QString::fromLatin1(label->data(),
        static_cast<qsizetype>(label->size())) : QString{};
}

QString translated(const char* source)
{
    return QCoreApplication::translate("NextClassesPage", source);
}

QColor gradeColor(const Domain::SpeakingEvaluationGrade grade)
{
    switch (grade)
    {
    case Domain::SpeakingEvaluationGrade::APlus:
        return QColor(QStringLiteral("#16a765"));
    case Domain::SpeakingEvaluationGrade::A:
        return QColor(QStringLiteral("#4285d4"));
    case Domain::SpeakingEvaluationGrade::BPlus:
        return QColor(QStringLiteral("#e2aa16"));
    case Domain::SpeakingEvaluationGrade::B:
        return QColor(QStringLiteral("#ed6a25"));
    case Domain::SpeakingEvaluationGrade::C:
        return QColor(QStringLiteral("#cb3549"));
    }
    return QColor(QStringLiteral("#808080"));
}

QString joined(const std::vector<std::u16string>& values)
{
    QStringList parts;
    parts.reserve(static_cast<qsizetype>(values.size()));
    for (const auto& value : values)
    {
        parts.push_back(QString::fromStdU16String(value));
    }
    return parts.isEmpty() ? QStringLiteral("—")
                           : parts.join(QStringLiteral(", "));
}

} // namespace

SelectedClassAnalyticsModel::SelectedClassAnalyticsModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    setObjectName(QStringLiteral("selectedClassAnalyticsRankingModel"));
}

void SelectedClassAnalyticsModel::setRankings(
    const std::vector<Application::ClassAnalyticsStudentRank>* rankings
    )
{
    beginResetModel();
    m_rankings = rankings;
    endResetModel();
}

void SelectedClassAnalyticsModel::setHeaderLabels(const QStringList& labels)
{
    if (m_headers == labels)
    {
        return;
    }
    m_headers = labels;
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

int SelectedClassAnalyticsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() || !m_rankings
        ? 0 : static_cast<int>(m_rankings->size());
}

int SelectedClassAnalyticsModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant SelectedClassAnalyticsModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!index.isValid() || !m_rankings || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= m_rankings->size()
        || index.column() < 0 || index.column() >= ColumnCount)
    {
        return {};
    }
    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    const Application::ClassAnalyticsStudentRank& rank =
        m_rankings->at(static_cast<std::size_t>(index.row()));
    switch (index.column())
    {
    case Rank:
        return index.row() + 1;
    case EnglishName:
        return QString::fromStdU16String(rank.englishName);
    case KoreanName:
        return QString::fromStdU16String(rank.koreanName);
    case Average:
        return QString::number(rank.overall3.toDouble(), 'f', 3);
    case Grammar:
    case Pronunciation:
    case Fluency:
    case Manner:
    case Content:
    case OverallEffort:
    {
        const auto& grade = rank.criterionLetters[
            static_cast<std::size_t>(index.column() - Grammar)];
        return grade ? gradeText(*grade) : QString{};
    }
    case ColumnCount:
        break;
    }
    return {};
}

QVariant SelectedClassAnalyticsModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole
        || section < 0 || section >= ColumnCount)
    {
        return {};
    }
    return m_headers.value(section);
}

SelectedClassAnalyticsSummaryWidget::SelectedClassAnalyticsSummaryWidget(
    QWidget* parent
    )
    : QWidget(parent)
{
    setObjectName(QStringLiteral("classEditorAnalyticsSummary"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(296);
}

void SelectedClassAnalyticsSummaryWidget::setDashboard(
    const Application::ClassAnalyticsDashboard* dashboard
    )
{
    if (m_dashboard == dashboard)
    {
        update();
        return;
    }
    m_dashboard = dashboard;
    update();
}

QSize SelectedClassAnalyticsSummaryWidget::sizeHint() const
{
    return {620, 304};
}

void SelectedClassAnalyticsSummaryWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (!m_dashboard || !m_dashboard->selectedSnapshot.hasData)
    {
        return;
    }

    const Application::ClassAnalyticsDashboard& dashboard = *m_dashboard;
    const Application::ClassAnalyticsSnapshot& selected =
        dashboard.selectedSnapshot;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette colors = palette();
    const QColor textColor = colors.color(QPalette::WindowText);
    QColor mutedColor = textColor;
    mutedColor.setAlpha(184);
    const QColor borderColor = colors.color(QPalette::Mid);
    const QColor cardColor = colors.color(QPalette::Base);
    painter.fillRect(rect(), colors.color(QPalette::Window));

    const QRectF bounds = QRectF(rect()).adjusted(8, 6, -8, -4);
    constexpr qreal gap = 7.0;
    const qreal cardWidth = (bounds.width() - gap * 3.0) / 4.0;
    const qreal cardHeight = 55.0;
    QString average = QString::number(selected.classAverage3.toDouble(), 'f', 3);
    if (selected.classAverageLetter)
    {
        average = QStringLiteral("%1 · %2")
            .arg(gradeText(*selected.classAverageLetter), average);
    }
    const QString scored = QStringLiteral("%1 / %2")
        .arg(static_cast<qulonglong>(selected.fullyScoredCount.rawValue()))
        .arg(static_cast<qulonglong>(selected.rosterStudentCount.rawValue()));
    const QString strongestTitle = translated(selected.strongestLabels.size() > 1
        ? "Strongest Areas" : "Strongest Area");
    const QString focusTitle = translated(selected.focusLabels.size() > 1
        ? "Focus Areas" : "Focus Area");
    const std::array<std::pair<QString, QString>, 4> cards{{
        {translated("Class Average"), average},
        {translated("Students Fully Scored"), scored},
        {strongestTitle, joined(selected.strongestLabels)},
        {focusTitle, joined(selected.focusLabels)}
    }};
    for (std::size_t index = 0; index < cards.size(); ++index)
    {
        const QRectF card(bounds.left() + (cardWidth + gap)
                * static_cast<qreal>(index),
            bounds.top(), cardWidth, cardHeight);
        painter.setPen(QPen(borderColor, 1.0));
        painter.setBrush(cardColor);
        painter.drawRoundedRect(card, 4.0, 4.0);
        painter.setPen(mutedColor);
        painter.setFont(QFont(font().family(), 9));
        painter.drawText(card.adjusted(6, 4, -5, -cardHeight / 2.0),
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
            cards[index].first);
        painter.setPen(textColor);
        QFont valueFont(font().family(), index < 2 ? 12 : 10);
        valueFont.setBold(index < 2);
        painter.setFont(valueFont);
        painter.drawText(card.adjusted(6, cardHeight / 2.0 - 2.0, -5, -3),
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
            cards[index].second);
    }

    const qreal bodyTop = bounds.top() + cardHeight + 8.0;
    const qreal chartWidth = bounds.width() * 0.62;
    const QRectF criteriaRect(bounds.left(), bodyTop,
        chartWidth - gap, bounds.height() - (bodyTop - bounds.top()));
    const QRectF rightRect(criteriaRect.right() + gap, bodyTop,
        bounds.right() - criteriaRect.right() - gap,
        criteriaRect.height());
    painter.setPen(textColor);
    painter.setFont(QFont(font().family(), 10, QFont::DemiBold));
    painter.drawText(QRectF(criteriaRect.left(), bodyTop, criteriaRect.width(), 15),
        Qt::AlignLeft | Qt::AlignVCenter, translated("By Criterion"));

    const std::array<Domain::SpeakingEvaluationGrade, 5> grades{
        Domain::SpeakingEvaluationGrade::APlus,
        Domain::SpeakingEvaluationGrade::A,
        Domain::SpeakingEvaluationGrade::BPlus,
        Domain::SpeakingEvaluationGrade::B,
        Domain::SpeakingEvaluationGrade::C
    };
    qreal legendX = criteriaRect.left();
    const qreal legendY = bodyTop + 19.0;
    painter.setFont(QFont(font().family(), 8));
    for (const Domain::SpeakingEvaluationGrade grade : grades)
    {
        const QString label = gradeText(grade);
        painter.fillRect(QRectF(legendX, legendY + 3, 8, 8), gradeColor(grade));
        painter.setPen(textColor);
        painter.drawText(QRectF(legendX + 11, legendY, 28, 14),
            Qt::AlignLeft | Qt::AlignVCenter, label);
        legendX += 42.0;
    }

    const qreal firstRowY = bodyTop + 37.0;
    const qreal rowHeight = 29.0;
    const qreal nameWidth = std::min<qreal>(104.0, criteriaRect.width() * 0.27);
    const qreal averageWidth = 48.0;
    const qreal barLeft = criteriaRect.left() + nameWidth + averageWidth + 4.0;
    const qreal barWidth = std::max<qreal>(36.0,
        criteriaRect.right() - barLeft - 2.0);
    painter.setFont(QFont(font().family(), 8));
    for (std::size_t index = 0; index < selected.criteria.size(); ++index)
    {
        const Application::ClassAnalyticsCriterionSlice& criterion =
            selected.criteria[index];
        const qreal y = firstRowY + rowHeight * static_cast<qreal>(index);
        const QString criterionName = QString::fromStdU16String(criterion.name);
        painter.setPen(textColor);
        painter.drawText(QRectF(criteriaRect.left(), y, nameWidth, 14),
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
            QFontMetrics(painter.font()).elidedText(criterionName,
                Qt::ElideRight, static_cast<int>(nameWidth)));
        painter.setPen(mutedColor);
        const QString criterionAverage = criterion.hasData
            ? QString::number(criterion.average3.toDouble(), 'f', 2)
            : QStringLiteral("—");
        painter.drawText(QRectF(criteriaRect.left() + nameWidth, y,
                averageWidth, 14),
            Qt::AlignRight | Qt::AlignVCenter, criterionAverage);

        std::array<int, grades.size()> counts{};
        int total = 0;
        for (std::size_t gradeIndex = 0; gradeIndex < grades.size(); ++gradeIndex)
        {
            counts[gradeIndex] = criterion.distribution.countFor(
                grades[gradeIndex]).value_or(0);
            total += counts[gradeIndex];
        }
        const QRectF bar(barLeft, y + 2.0, barWidth, 14.0);
        painter.setPen(QPen(borderColor, 0.5));
        painter.setBrush(colors.color(QPalette::AlternateBase));
        painter.drawRoundedRect(bar, 3.0, 3.0);
        if (total == 0)
        {
            continue;
        }
        qreal x = bar.left();
        for (std::size_t gradeIndex = 0; gradeIndex < grades.size(); ++gradeIndex)
        {
            if (counts[gradeIndex] == 0)
            {
                continue;
            }
            const qreal segmentWidth = bar.width()
                * static_cast<qreal>(counts[gradeIndex])
                / static_cast<qreal>(total);
            const QRectF segment(x, bar.top(), segmentWidth, bar.height());
            painter.setPen(Qt::NoPen);
            painter.setBrush(gradeColor(grades[gradeIndex]));
            painter.drawRect(segment);
            if (segmentWidth >= 20.0)
            {
                painter.setPen(Qt::white);
                painter.drawText(segment, Qt::AlignCenter,
                    QString::number(counts[gradeIndex]));
            }
            x += segmentWidth;
        }
    }

    painter.setPen(textColor);
    painter.setFont(QFont(font().family(), 9, QFont::DemiBold));
    painter.drawText(QRectF(rightRect.left(), bodyTop, rightRect.width(), 14),
        Qt::AlignLeft | Qt::AlignVCenter,
        translated("Class Shape and Year to Date"));
    painter.setFont(QFont(font().family(), 8));
    const QString shapeName = dashboard.classShapeEvaluationName.empty()
        ? QStringLiteral("—")
        : QString::fromStdU16String(dashboard.classShapeEvaluationName);
    painter.setPen(mutedColor);
    painter.drawText(QRectF(rightRect.left(), bodyTop + 15.0,
            rightRect.width(), 13),
        Qt::AlignLeft | Qt::AlignVCenter,
        translated("Evaluation: %1").arg(shapeName));

    const QRectF histogram(rightRect.left() + 3.0, bodyTop + 32.0,
        rightRect.width() - 6.0, 70.0);
    const qreal baseline = histogram.bottom() - 13.0;
    painter.setPen(borderColor);
    painter.drawLine(QPointF(histogram.left(), baseline),
        QPointF(histogram.right(), baseline));
    const qreal slotWidth = histogram.width() / static_cast<qreal>(grades.size());
    const auto maximum = std::max<std::size_t>(1,
        selected.overallLetters.size());
    for (std::size_t index = 0; index < grades.size(); ++index)
    {
        const int count = static_cast<int>(std::count(
            selected.overallLetters.cbegin(), selected.overallLetters.cend(),
            grades[index]));
        const qreal barHeight = (baseline - histogram.top() - 5.0)
            * static_cast<qreal>(count) / static_cast<qreal>(maximum);
        const qreal barX = histogram.left() + slotWidth
            * static_cast<qreal>(index) + slotWidth * 0.22;
        const QRectF bar(barX, baseline - barHeight,
            slotWidth * 0.56, std::max<qreal>(1.0, barHeight));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradeColor(grades[index]));
        painter.drawRoundedRect(bar, 2.0, 2.0);
        painter.setPen(mutedColor);
        painter.drawText(QRectF(barX - 3.0, baseline - barHeight - 14.0,
                slotWidth + 6.0, 13),
            Qt::AlignHCenter | Qt::AlignVCenter, QString::number(count));
        painter.drawText(QRectF(histogram.left() + slotWidth * index,
                baseline + 1.0, slotWidth, 12),
            Qt::AlignHCenter | Qt::AlignVCenter, gradeText(grades[index]));
    }

    const qreal ytdTitleY = bodyTop + 111.0;
    painter.setPen(textColor);
    painter.setFont(QFont(font().family(), 9, QFont::DemiBold));
    painter.drawText(QRectF(rightRect.left(), ytdTitleY, rightRect.width(), 13),
        Qt::AlignLeft | Qt::AlignVCenter, translated("Year to Date"));
    if (dashboard.yearToDatePoints.empty())
    {
        painter.setPen(mutedColor);
        painter.setFont(QFont(font().family(), 8));
        painter.drawText(QRectF(rightRect.left(), ytdTitleY + 14.0,
                rightRect.width(), 30),
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
            translated("No year-to-date evaluations."));
        return;
    }

    const QRectF ytdPlot(rightRect.left() + 3.0, ytdTitleY + 18.0,
        rightRect.width() - 6.0, 38.0);
    painter.setPen(QPen(borderColor, 0.8));
    painter.drawLine(QPointF(ytdPlot.left(), ytdPlot.bottom()),
        QPointF(ytdPlot.right(), ytdPlot.bottom()));
    QPolygonF line;
    const std::size_t count = dashboard.yearToDatePoints.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        const double value = dashboard.yearToDatePoints[index]
            .classAverage3.toDouble();
        const qreal x = count == 1 ? ytdPlot.center().x()
            : ytdPlot.left() + ytdPlot.width() * static_cast<qreal>(index)
                / static_cast<qreal>(count - 1);
        const qreal y = ytdPlot.bottom() - ytdPlot.height()
            * static_cast<qreal>(std::clamp(value / 5.0, 0.0, 1.0));
        line << QPointF(x, y);
    }
    painter.setPen(QPen(colors.color(QPalette::Highlight), 2.0));
    painter.setBrush(colors.color(QPalette::Highlight));
    if (line.size() > 1)
    {
        painter.drawPolyline(line);
    }
    for (std::size_t index = 0; index < count; ++index)
    {
        painter.drawEllipse(line.at(static_cast<qsizetype>(index)), 3.0, 3.0);
        const qreal x = count == 1 ? ytdPlot.center().x()
            : ytdPlot.left() + ytdPlot.width() * static_cast<qreal>(index)
                / static_cast<qreal>(count - 1);
        const QString label = QString::fromStdU16String(
            dashboard.yearToDatePoints[index].evaluationName).left(3);
        painter.setPen(mutedColor);
        painter.setFont(QFont(font().family(), 7));
        painter.drawText(QRectF(x - 16.0, ytdPlot.bottom() + 1.0, 32.0, 10.0),
            Qt::AlignHCenter | Qt::AlignVCenter, label);
    }
}

} // namespace ClassMngr::Next::Ui
