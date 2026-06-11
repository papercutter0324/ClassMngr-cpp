#pragma once

#include "models/roster.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>

class RosterModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit RosterModel(
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

    void setRoster(
        const Roster& roster
        );

    Roster toRoster() const;

    QString columnName(
        int column
        ) const;

    bool isRequiredColumn(
        int column
        ) const;

    bool isRequiredColumn(
        const QString& name
        ) const;

    bool canAddColumn(
        const QString& name,
        QString* reason = nullptr
        ) const;

    bool insertCustomColumn(
        const QString& name
        );

    bool canRemoveColumn(
        int column,
        QString* reason = nullptr
        ) const;

    bool removeRosterColumn(
        int column
        );

    bool isDirty() const;

    void clearDirty();

    QStringList errorsForCell(
        int row,
        int column
        ) const;

signals:
    void dirtyChanged(
        bool dirty
        );

private:
    QString normalizedColumnName(
        const QString& name
        ) const;

    int findColumn(
        const QString& name,
        const QStringList& columns
        ) const;

    void rebuildRows(
        const Roster& roster
        );

    QString normalizeCell(
        const QString& value,
        int column
        ) const;

    QString normalizeEnglish(
        const QString& value
        ) const;

    QString normalizeEnglishToken(
        const QString& token
        ) const;

    QString normalizeKorean(
        const QString& value
        ) const;

    QStringList validateCell(
        const QString& value,
        int column
        ) const;

    void validateAll();

    void validateCellAt(
        int row,
        int column
        );

    void validateRawInput(
        int row,
        int column,
        const QString& rawValue
        );

    QString cellKey(
        int row,
        int column
        ) const;

    void setDirty(
        bool dirty
        );

private:
    QStringList m_columns;
    QList<QStringList> m_rows;
    QHash<QString, QStringList> m_validationErrors;
    QSet<QString> m_dirtyCells;
    bool m_dirty = false;
};
