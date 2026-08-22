#pragma once

#include "domain/models/roster.h"
#include "domain/validation/validation_result.h"

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

    QStringList columnNames() const;

    QString columnName(
        int column
        ) const;

    QStringList rowValues(
        int row
        ) const;

    int firstEmptyRow() const;

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

    bool canRemoveRow(
        int row,
        QString* reason = nullptr
        ) const;

    bool removeRosterRow(
        int row
        );

    bool canMoveRow(
        int sourceRow,
        int destinationRow,
        QString* reason = nullptr
        ) const;

    bool moveRosterRow(
        int sourceRow,
        int destinationRow
        );

    bool hasDuplicateTransferredStudent(
        const QStringList& sourceColumns,
        const QStringList& sourceRow,
        QString* reason = nullptr
        ) const;

    bool canInsertTransferredRow(
        const QStringList& sourceColumns,
        const QStringList& sourceRow,
        QString* reason = nullptr
        ) const;

    bool insertTransferredRow(
        const QStringList& sourceColumns,
        const QStringList& sourceRow,
        QString* reason = nullptr
        );

    bool isDirty() const;

    void clearDirty();

    QStringList errorsForCell(
        int row,
        int column
        ) const;
    void setDomainValidation(const ValidationResult& validation);

    int englishNameColumn() const;

    int koreanNameColumn() const;

    bool isNameColumn(
        int column
        ) const;

    QList<int> duplicateNameRows(
        int row
        ) const;

    QString suggestedKoreanNameWithSuffix(
        int row
        ) const;

    bool hasDuplicateNameErrors() const;

    QStringList duplicateNameErrorList() const;

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

    QStringList mappedTransferRow(
        const QStringList& sourceColumns,
        const QStringList& sourceRow
        ) const;

    bool rowHasData(
        const QStringList& row
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

    void validateDuplicateNames();

    void clearDuplicateNameErrors();

    void appendValidationError(
        int row,
        int column,
        const QString& error,
        bool duplicateNameError = false
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
    QHash<QString, QStringList> m_domainValidationErrors;
    QSet<QString> m_duplicateNameErrorCells;
    QSet<QString> m_dirtyCells;
    bool m_dirty = false;
};
