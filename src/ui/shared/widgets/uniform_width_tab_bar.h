#pragma once

#include <QTabBar>
#include <QTabWidget>

class QResizeEvent;
class QShowEvent;
class QEvent;
class QObject;
class QWheelEvent;

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
    void centerTabBar();
};
