#pragma once

#include "core/enums/schedule_type.h"
#include "models/class_info.h"

#include <QWidget>
#include <QList>
#include <QVariantList>

class QGridLayout;
class QPushButton;
class ClassTimeRow;

class ClassScheduleSection : public QWidget
{
    Q_OBJECT

public:
    explicit ClassScheduleSection(QWidget* parent = nullptr);

    QList<ClassTimeRow*> regularRows() const;
    QList<ClassTimeRow*> intensiveRows() const;

    void loadSchedules(
        const QVariantList& regular,
        const QVariantList& intensive
        );

    void loadSchedules(
        const QList<ClassTime>& regular,
        const QList<ClassTime>& intensive
        );

    QVariantList serializeRegular() const;
    QVariantList serializeIntensive() const;

    QList<ClassTime> regularTimes() const;
    QList<ClassTime> intensiveTimes() const;

signals:
    void dataChanged();

private:
    // PUBLIC-FACING HELPERS (these MUST exist if cpp uses them)
    ClassTimeRow* addRegularRow(bool markDirty = true);
    ClassTimeRow* addIntensiveRow(bool markDirty = true);

    void removeRow(
        QGridLayout* grid,
        QList<ClassTimeRow*>& container,
        ClassTimeRow* row,
        bool markDirty = true
        );

    void rebuildGrid(
        QGridLayout* grid,
        const QList<ClassTimeRow*>& rows
        );

    void connectRowSignals(ClassTimeRow* row);

private:
    // CORE FACTORY (internal only)
    ClassTimeRow* addRow(
        QGridLayout* grid,
        QList<ClassTimeRow*>& container,
        ScheduleType type,
        bool markDirty = true
        );

private:
    QGridLayout* m_regularGrid = nullptr;
    QGridLayout* m_intensiveGrid = nullptr;

    QPushButton* m_addRegularButton = nullptr;
    QPushButton* m_addIntensiveButton = nullptr;

    QList<ClassTimeRow*> m_regularRows;
    QList<ClassTimeRow*> m_intensiveRows;
};
