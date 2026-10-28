#pragma once

#include "next/application/schedule_page_read.h"
#include "next/application/schedule_slot_state_save.h"

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <cstddef>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Ui
{

struct SchedulePageEntry final
{
    Domain::ClassId classId;
    QString className;
    QString teacherKoreanName;
    QString teacherEnglishName;
    QString teacherPreferredName;
    QString room;
    QString grade;
    QString level;
    QString classColor;
    QString fontColor;
    bool testingClass = false;
};

struct SchedulePageCell final
{
    QString day;
    int startMinute = 0;
    Application::ScheduleSlotState state =
        Application::ScheduleSlotState::Essay;
    Application::ScheduleSlotState defaultState =
        Application::ScheduleSlotState::Essay;
    bool slotTogglingEnabled = false;
    bool plainTestingAssignment = false;
    QString testingRoom;
    std::vector<SchedulePageEntry> entries;
};

struct SchedulePageRow final
{
    int startMinute = 0;
    QString timeRangeLabel;
    std::vector<SchedulePageCell> cells;
};

struct SchedulePageRuntimeMetrics final
{
    int rowCount = 0;
    int cellCount = 0;
    int entryCount = 0;
    int sourceClassCount = 0;
};

class SchedulePageModel final : public QAbstractTableModel
{
public:
    explicit SchedulePageModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    [[nodiscard]] Domain::Result<void> setSnapshot(
        Application::SchedulePageReadSnapshot snapshot);
    void clear();
    [[nodiscard]] const SchedulePageCell* cellAt(
        int row,
        int tableColumn) const noexcept;
    [[nodiscard]] const SchedulePageRow* rowAt(int row) const noexcept;
    [[nodiscard]] Application::ScheduleDisplayMode displayMode() const noexcept;
    [[nodiscard]] const Application::ScheduleDisplayPreferences& preferences()
        const noexcept;
    [[nodiscard]] SchedulePageRuntimeMetrics runtimeMetrics() const noexcept;

private:
    void rebuild();
    [[nodiscard]] QString accessibleCellText(
        const SchedulePageCell& cell) const;

    Application::SchedulePageReadSnapshot m_snapshot;
    QStringList m_days;
    std::vector<SchedulePageRow> m_rows;
    std::size_t m_entryCount = 0;
    QString m_projectionError;
};

} // namespace ClassMngr::Next::Ui
