#pragma once

#include "features/classes/services/speaking_analytics.h"

#include <QAbstractTableModel>
#include <QStringList>

// Compact read-only backing model for the analytics ranking table.  Keeping
// the ranking records here avoids allocating one QTableWidgetItem per cell.
class ClassAnalyticsRankingModel : public QAbstractTableModel
{
public:
    enum Column
    {
        RankColumn,
        EnglishNameColumn,
        KoreanNameColumn,
        AverageColumn,
        GrammarColumn,
        PronunciationColumn,
        FluencyColumn,
        MannerColumn,
        ContentColumn,
        EffortColumn,
        ColumnCount
    };

    explicit ClassAnalyticsRankingModel(
        QObject* parent = nullptr
        );

    void setRankings(
        QList<SpeakingAnalytics::StudentRank> rankings
        );
    void setHeaderLabels(
        const QStringList& labels
        );

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = {}
        ) const override;
    [[nodiscard]] int columnCount(
        const QModelIndex& parent = {}
        ) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
        ) const override;

private:
    QList<SpeakingAnalytics::StudentRank> m_rankings;
    QStringList m_headers;
};
