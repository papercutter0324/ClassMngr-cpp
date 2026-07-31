#pragma once

#include <QColor>
#include <QPoint>
#include <QTabBar>
#include <QTabWidget>

class QResizeEvent;
class QShowEvent;
class QEvent;
class QMouseEvent;
class QObject;
class QPaintEvent;
class QToolButton;
class QWheelEvent;

enum class UniformWidthTabKind
{
    Generic,
    Grade,
    Class,
    Section
};

enum class UniformWidthTabAppearance
{
    Platform,
    NavigationPill
};

class UniformWidthTabBar : public QTabBar
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
    explicit UniformWidthTabBar(
        QWidget* parent = nullptr
        );

    QSize tabSizeHint(
        int index
        ) const override;

    UniformWidthTabAppearance tabAppearance() const;

    void setTabAppearance(
        UniformWidthTabAppearance appearance
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

    int naturalWidth() const;

protected:
    void changeEvent(
        QEvent* event
        ) override;

    void paintEvent(
        QPaintEvent* event
        ) override;

    void mouseMoveEvent(
        QMouseEvent* event
        ) override;

    void mousePressEvent(
        QMouseEvent* event
        ) override;

    void mouseReleaseEvent(
        QMouseEvent* event
        ) override;

    void tabLayoutChange() override;

    void resizeEvent(
        QResizeEvent* event
        ) override;

    void showEvent(
        QShowEvent* event
        ) override;

    void wheelEvent(
        QWheelEvent* event
        ) override;

private:
    QSize naturalTabSizeHint(
        int index
        ) const;

    void paintNavigationPills();

    QToolButton* scrollButton(
        const char* objectName
        ) const;

    void scheduleScrollControlRefresh();
    void refreshScrollControls();
    void scrollForDragDistance(
        int horizontalDistance
        );
    void removeTrailingGap(
        QToolButton* leftButton,
        QToolButton* rightButton
        );

private:
    UniformWidthTabAppearance m_tabAppearance =
        UniformWidthTabAppearance::Platform;
    QColor m_navigationTabFillColor;
    QColor m_navigationTabBorderColor;
    QColor m_navigationTabHoverColor;
    QColor m_navigationTabSelectedColor;
    QColor m_navigationTabTextColor;
    bool m_scrollControlRefreshScheduled = false;
    bool m_dragScrollCandidate = false;
    bool m_dragScrolling = false;
    QPoint m_dragPressPosition;
    int m_lastDragX = 0;
    int m_dragScrollRemainder = 0;
};

class UniformWidthTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit UniformWidthTabWidget(
        const QString& tabBarObjectName,
        QWidget* parent = nullptr
        );

    explicit UniformWidthTabWidget(
        UniformWidthTabKind kind,
        const QString& tabBarObjectName,
        QWidget* parent = nullptr
        );

    UniformWidthTabKind tabKind() const;

    void setTabKind(
        UniformWidthTabKind kind
        );

    UniformWidthTabAppearance tabAppearance() const;

    void setTabAppearance(
        UniformWidthTabAppearance appearance
        );

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

    void resizeEvent(
        QResizeEvent* event
        ) override;

    void showEvent(
        QShowEvent* event
        ) override;

    void tabInserted(
        int index
        ) override;

    void tabRemoved(
        int index
        ) override;

private:
    static QString kindPropertyValue(
        UniformWidthTabKind kind
        );

    void scheduleCenterTabBar();
    void centerTabBar();

private:
    UniformWidthTabKind m_tabKind = UniformWidthTabKind::Generic;
    UniformWidthTabAppearance m_tabAppearance =
        UniformWidthTabAppearance::Platform;
    bool m_centerTabBarScheduled = false;
};
