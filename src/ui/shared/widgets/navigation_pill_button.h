#pragma once

#include <QColor>
#include <QPushButton>

class QEnterEvent;
class QEvent;
class QPaintEvent;

class NavigationPillButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(
        QColor navigationTabFillColor
        READ navigationTabFillColor
        WRITE setNavigationTabFillColor
        )
    Q_PROPERTY(
        QColor navigationTabBorderColor
        READ navigationTabBorderColor
        WRITE setNavigationTabBorderColor
        )
    Q_PROPERTY(
        QColor navigationTabHoverColor
        READ navigationTabHoverColor
        WRITE setNavigationTabHoverColor
        )
    Q_PROPERTY(
        QColor navigationTabSelectedColor
        READ navigationTabSelectedColor
        WRITE setNavigationTabSelectedColor
        )
    Q_PROPERTY(
        QColor navigationTabTextColor
        READ navigationTabTextColor
        WRITE setNavigationTabTextColor
        )

public:
    explicit NavigationPillButton(
        QWidget* parent = nullptr
        );

    QColor navigationTabFillColor() const;
    void setNavigationTabFillColor(const QColor& color);

    QColor navigationTabBorderColor() const;
    void setNavigationTabBorderColor(const QColor& color);

    QColor navigationTabHoverColor() const;
    void setNavigationTabHoverColor(const QColor& color);

    QColor navigationTabSelectedColor() const;
    void setNavigationTabSelectedColor(const QColor& color);

    QColor navigationTabTextColor() const;
    void setNavigationTabTextColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void changeEvent(QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_navigationTabFillColor;
    QColor m_navigationTabBorderColor;
    QColor m_navigationTabHoverColor;
    QColor m_navigationTabSelectedColor;
    QColor m_navigationTabTextColor;
};
