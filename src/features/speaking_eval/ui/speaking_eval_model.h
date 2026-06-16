#pragma once

#include "domain/models/speaking_evaluation.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>
#include <QStringList>

class SpeakingEvalModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SpeakingEvalModel(
        QObject* parent = nullptr
        );

    int rowCount(
        const QModelIndex& parent = QModelIndex()
        ) const override;

    int columnCount(
        const QModelIndex& parent = QModelIndex()
        ) const override;

    QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;

    bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole
        ) override;

    Qt::ItemFlags flags(
        const QModelIndex& index
        ) const override;

    QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
        ) const override;

    void loadData(
        const SpeakingEvalRows& rows
        );

    SpeakingEvalRows rows() const;

    QList<SpeakingEvalCellChange> changedCells() const;

    QSet<QString> dirtyCellKeys() const;

    QStringList errorsForCell(
        int row,
        int column
        ) const;

    QList<int> duplicateNameRows(
        int row
        ) const;

    QString suggestedKoreanNameWithSuffix(
        int row
        ) const;

    bool containsNamePair(
        const QString& englishName,
        const QString& koreanName
        ) const;

    bool hasErrors() const;

    QStringList errorList() const;

    bool isDirty() const;

    void markSaved();

    void revalidateAll();

signals:
    void dirtyChanged(
        bool dirty
        );

    void dataModified();

private:
    struct ProcessedValue
    {
        QString normalized;
        QStringList errors;
    };

    SpeakingEvalRows normalizeStructure(
        const SpeakingEvalRows& rows
        ) const;

    ProcessedValue processValue(
        int row,
        int column,
        const QString& value
        ) const;

    QString normalizeEnglishName(
        const QString& value
        ) const;

    QString normalizeKoreanName(
        const QString& value
        ) const;

    QString baseKoreanName(
        const QString& value
        ) const;

    QString koreanNameSuffix(
        const QString& value
        ) const;

    QString namePairKey(
        const QString& englishName,
        const QString& koreanName
        ) const;

    QString normalizeScore(
        const QString& value
        ) const;

    QString normalizeComment(
        const QString& value
        ) const;

    QStringList validateValue(
        int row,
        int column,
        const QString& value
        ) const;

    QString cellKey(
        int row,
        int column
        ) const;

    void validateDuplicateNames();

    void appendValidationError(
        int row,
        int column,
        const QString& error
        );

    void setDirtyState(
        bool dirty
        );

private:
    SpeakingEvalRows m_rows;
    SpeakingEvalRows m_lastSaved;
    QHash<QString, QStringList> m_errors;
    QSet<QString> m_dirtyCells;
    bool m_dirty = false;
};
