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

private:
    void enforceStudentInformationMinimum();

private:
    QTableView* m_table = nullptr;
    RosterModel* m_model = nullptr;
};
