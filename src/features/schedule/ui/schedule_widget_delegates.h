#pragma once

#include <QStyledItemDelegate>

namespace ScheduleWidgetDelegates
{
constexpr int TimeCellRole = Qt::UserRole + 1;
inline const QString TimeColumnDelegateObjectName =
    QStringLiteral("scheduleTimeColumnDelegate");

class TimeColumnDelegate final : public QStyledItemDelegate
{
public:
    explicit TimeColumnDelegate(QObject* parent);
    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;
};

enum DayCellRole
{
    HtmlRole = Qt::UserRole + 2,
    BackgroundRole,
    ForegroundRole,
    BorderRole,
    MarkerRole,
    MarkerColorRole,
    CompactRole,
    HtmlFontOffsetRole,
    TestingSlotRole
};

class DayCellDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
};
}
