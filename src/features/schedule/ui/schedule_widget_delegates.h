#pragma once

#include <QLabel>
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

class CornerMarkerLabel final : public QLabel
{
public:
    using QLabel::QLabel;

protected:
    void paintEvent(QPaintEvent* event) override;
};
}
