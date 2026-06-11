#pragma once

#include <QObject>
#include <QVector>

class QTableView;
class RosterModel;

class RosterColumnLayoutController : public QObject
{
public:
    explicit RosterColumnLayoutController(
        QObject* parent = nullptr
        );

    void attach(
        QTableView* table,
        RosterModel* model
        );

    void applyResizeModes();

    void applyWidths(
        const QVector<int>& widths
        );

    QVector<int> currentWidths() const;

    QString columnGroup(
        int column
        ) const;

    int groupStart(
        int column
        ) const;

    int groupEnd(
        int column
        ) const;

    bool isGroupBoundaryAfter(
        int column
        ) const;

    bool isCustomColumn(
        int column
        ) const;

    int customColumnMinimumWidth() const;

    int studentInformationMinimumWidth() const;

    void initializeAddedCustomColumn(
        int column
        );

    void handleCustomColumnRemoved(
        int removedWidth
        );

    void handleSectionResized(
        int logicalIndex
        );

    void enforceStudentInformationMinimum();

private:
    int customColumnCount() const;

    int studentInformationStart() const;

    int studentInformationEnd() const;

    int studentInformationWidth() const;

    void enforceStudentInformationMinimumInternal();

    void enforceCustomColumnMinimumsInternal();

    void updateAttachedViews();

private:
    QTableView* m_table = nullptr;
    RosterModel* m_model = nullptr;
    bool m_enforcingWidths = false;
};
