#pragma once

#include <QTabBar>
#include <QTabWidget>

class QResizeEvent;
class QShowEvent;
class QEvent;
class QObject;
class QWheelEvent;

enum class UniformWidthTabKind
{
    Generic,
    Grade,
    Class,
    Section
};

class UniformWidthTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit UniformWidthTabBar(
        QWidget* parent = nullptr
        );

    QSize tabSizeHint(
        int index
        ) const override;

protected:
    void wheelEvent(
        QWheelEvent* event
        ) override;
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

    void centerTabBar();

private:
    UniformWidthTabKind m_tabKind = UniformWidthTabKind::Generic;
};
