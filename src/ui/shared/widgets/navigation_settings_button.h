#pragma once

#include <QColor>
#include <QToolButton>

class QFocusEvent;
class QPaintEvent;

class NavigationSettingsButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(
        QColor navigationSettingsHoverColor
        READ navigationSettingsHoverColor
        WRITE setNavigationSettingsHoverColor
        )
    Q_PROPERTY(
        QColor navigationSettingsPressedColor
        READ navigationSettingsPressedColor
        WRITE setNavigationSettingsPressedColor
        )
    Q_PROPERTY(
        QColor navigationSettingsTextColor
        READ navigationSettingsTextColor
        WRITE setNavigationSettingsTextColor
        )
    Q_PROPERTY(
        QColor navigationSettingsFocusColor
        READ navigationSettingsFocusColor
        WRITE setNavigationSettingsFocusColor
        )

public:
    explicit NavigationSettingsButton(QWidget* parent = nullptr);

    QColor navigationSettingsHoverColor() const;
    void setNavigationSettingsHoverColor(const QColor& color);

    QColor navigationSettingsPressedColor() const;
    void setNavigationSettingsPressedColor(const QColor& color);

    QColor navigationSettingsTextColor() const;
    void setNavigationSettingsTextColor(const QColor& color);

    QColor navigationSettingsFocusColor() const;
    void setNavigationSettingsFocusColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_hoverColor;
    QColor m_pressedColor;
    QColor m_textColor;
    QColor m_focusColor;
    bool m_keyboardFocus = false;
};
