#pragma once

#include <QMargins>
#include <QScrollArea>

class QVBoxLayout;

class ScrollablePageBody final : public QScrollArea
{
    Q_OBJECT

public:
    explicit ScrollablePageBody(
        QWidget* parent = nullptr,
        const QMargins& margins = QMargins(-1, -1, -1, -1),
        int spacing = -1
        );

    QWidget* contentWidget() const;
    QVBoxLayout* contentLayout() const;
    void setContentMargins(const QMargins& margins);

private:
    QWidget* m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
};
