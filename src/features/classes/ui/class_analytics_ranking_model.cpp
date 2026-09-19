#include "class_analytics_ranking_model.h"

#include "features/classes/ui/class_analytics_ranking_header.h"

#include <utility>

ClassAnalyticsRankingModel::ClassAnalyticsRankingModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
}

void ClassAnalyticsRankingModel::setRankings(
    QList<SpeakingAnalytics::StudentRank> rankings
    )
{
    beginResetModel();
    m_rankings = std::move(rankings);
    endResetModel();
}

void ClassAnalyticsRankingModel::setHeaderLabels(
    const QStringList& labels
    )
{
    if (m_headers == labels)
    {
        return;
    }

    m_headers = labels;
    emit headerDataChanged(
        Qt::Horizontal,
        0,
        qMax(0, ColumnCount - 1)
        );
}

int ClassAnalyticsRankingModel::rowCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid()
        ? 0
        : m_rankings.size();
}

int ClassAnalyticsRankingModel::columnCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid()
        ? 0
        : ColumnCount;
}

QVariant ClassAnalyticsRankingModel::data(
    const QModelIndex& index,
    int role
    ) const
{
    if (
        !index.isValid()
        || index.row() < 0
        || index.row() >= m_rankings.size()
        || index.column() < 0
        || index.column() >= ColumnCount
        )
    {
        return {};
    }

    const SpeakingAnalytics::StudentRank& student =
        m_rankings.at(index.row());
    const int column = index.column();

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }

    if (role == AnalyticsRankingRoles::NeedsAttention)
    {
        return column == AverageColumn
            && SpeakingAnalytics::gradeToNumber(student.overallLetter) <= 2;
    }

    if (role == AnalyticsRankingRoles::Grade)
    {
        if (column == AverageColumn)
        {
            return student.overallLetter;
        }

        if (column >= GrammarColumn && column <= EffortColumn)
        {
            return student.criterionLetters.value(
                column - GrammarColumn
                );
        }

        return {};
    }

    if (role != Qt::DisplayRole)
    {
        return {};
    }

    switch (column)
    {
    case RankColumn:
        return QString::number(index.row() + 1);
    case EnglishNameColumn:
        return student.englishName;
    case KoreanNameColumn:
        return student.koreanName;
    case AverageColumn:
        return SpeakingAnalytics::formatAverage(student.overall3);
    default:
        return QString();
    }
}

QVariant ClassAnalyticsRankingModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role
    ) const
{
    if (
        orientation != Qt::Horizontal
        || role != Qt::DisplayRole
        || section < 0
        || section >= ColumnCount
        )
    {
        return {};
    }

    return m_headers.value(section);
}
