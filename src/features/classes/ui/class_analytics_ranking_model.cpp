#include "class_analytics_ranking_model.h"

#include "features/classes/ui/class_analytics_ranking_header.h"

#include <utility>

ClassAnalyticsRankingModel::ClassAnalyticsRankingModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
    MemoryUsageDiagnostics::registerMemoryBreakdownProvider(this, this);
}

void ClassAnalyticsRankingModel::setRankings(
    QList<SpeakingAnalytics::StudentRank> rankings
    )
{
    beginResetModel();
    m_rankings = std::move(rankings);
    updateEstimatedRetainedBytes();
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
    updateEstimatedRetainedBytes();
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

QList<MemoryBreakdownEntry> ClassAnalyticsRankingModel::memoryBreakdown() const
{
    return {
        {
            QStringLiteral("Analytics ranking model"),
            QStringLiteral("Classes"),
            m_estimatedRetainedBytes,
            static_cast<quint64>(m_rankings.size()),
            QStringLiteral("rows=%1; columns=%2")
                .arg(m_rankings.size())
                .arg(ColumnCount),
            true
        }
    };
}

void ClassAnalyticsRankingModel::updateEstimatedRetainedBytes()
{
    quint64 bytes = static_cast<quint64>(m_rankings.capacity())
        * sizeof(SpeakingAnalytics::StudentRank);

    const auto addString = [&bytes](const QString& text)
    {
        bytes += static_cast<quint64>(text.capacity()) * sizeof(QChar);
    };

    for (const SpeakingAnalytics::StudentRank& rank : m_rankings)
    {
        addString(rank.englishName);
        addString(rank.koreanName);
        addString(rank.overallLetter);
        bytes += static_cast<quint64>(rank.criterionLetters.capacity())
            * sizeof(QString);

        for (const QString& grade : rank.criterionLetters)
        {
            addString(grade);
        }
    }

    bytes += static_cast<quint64>(m_headers.capacity()) * sizeof(QString);
    for (const QString& header : m_headers)
    {
        addString(header);
    }

    m_estimatedRetainedBytes = bytes;
}
