#pragma once

#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QString>

class QFont;
class QFontMetrics;
class QPainter;
class QStyle;
class QWidget;

namespace NavigationPillStyle
{

inline constexpr int HorizontalPadding = 14;
inline constexpr int VerticalPadding = 6;
inline constexpr int Gap = 6;
inline constexpr int ControlHeight = 36;

struct Colors
{
    QColor fill;
    QColor border;
    QColor hover;
    QColor selected;
    QColor text;
};

struct State
{
    bool enabled = true;
    bool selected = false;
    bool hovered = false;
    bool activeWindow = true;
};

QSize sizeHint(
    const QFontMetrics& metrics,
    const QIcon& icon,
    const QString& text,
    const QSize& iconSize,
    int trailingGap = 0
    );

void paint(
    QPainter* painter,
    const QRect& bounds,
    const QString& text,
    const QIcon& icon,
    const QFont& font,
    Qt::TextElideMode elideMode,
    QStyle* style,
    const QWidget* widget,
    const QPalette& palette,
    const State& state,
    const Colors& colors,
    int trailingGap = 0
    );

}
