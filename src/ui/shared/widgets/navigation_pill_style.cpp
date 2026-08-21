#include "navigation_pill_style.h"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QTransform>
#include <QWidget>

namespace
{
constexpr qreal BorderWidth = 1.0;
constexpr qreal Radius = 7.0;
constexpr int BorderExtent = 2;
constexpr int IconSpacing = 4;
const QColor SelectedTextColor(Qt::black);

QColor paletteColor(
    const QPalette& palette,
    const NavigationPillStyle::State& state,
    QPalette::ColorRole role
    )
{
    const QPalette::ColorGroup colorGroup =
        state.enabled
            ? (
                state.activeWindow
                    ? QPalette::Active
                    : QPalette::Inactive
                )
            : QPalette::Disabled;

    return palette.color(colorGroup, role);
}

QColor configuredOrPalette(
    const QColor& configured,
    const QPalette& palette,
    const NavigationPillStyle::State& state,
    QPalette::ColorRole role
    )
{
    return configured.isValid()
        ? configured
        : paletteColor(palette, state, role);
}
}

namespace NavigationPillStyle
{

QSize sizeHint(
    const QFontMetrics& metrics,
    const QIcon& icon,
    const QString& text,
    const QSize& iconSize,
    int trailingGap
    )
{
    int contentWidth =
        metrics.horizontalAdvance(text);

    if (!icon.isNull() && !iconSize.isEmpty())
    {
        contentWidth += iconSize.width() + IconSpacing;
    }

    return QSize(
        contentWidth
            + (2 * HorizontalPadding)
            + trailingGap
            + BorderExtent,
        controlHeight(metrics)
        );
}

int controlHeight(const QFontMetrics& metrics)
{
    return metrics.height() + (2 * VerticalPadding);
}

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
    int trailingGap
    )
{
    if (!painter || !style || bounds.isEmpty())
    {
        return;
    }

    QColor fillColor =
        configuredOrPalette(
            colors.fill,
            palette,
            state,
            QPalette::Button
            );
    QColor borderColor =
        configuredOrPalette(
            colors.border,
            palette,
            state,
            QPalette::Mid
            );
    QColor textColor =
        configuredOrPalette(
            colors.text,
            palette,
            state,
            QPalette::ButtonText
            );

    if (state.selected)
    {
        fillColor =
            configuredOrPalette(
                colors.selected,
                palette,
                state,
                QPalette::Highlight
                );
        borderColor = fillColor;
        textColor = SelectedTextColor;
    }
    else if (state.hovered)
    {
        fillColor =
            configuredOrPalette(
                colors.hover,
                palette,
                state,
                QPalette::Light
                );
        borderColor =
            configuredOrPalette(
                colors.selected,
                palette,
                state,
                QPalette::Highlight
                );
    }

    const QTransform deviceTransform = painter->deviceTransform();
    const qreal horizontalScale =
        std::hypot(
            deviceTransform.m11(),
            deviceTransform.m12()
            );
    const qreal verticalScale =
        std::hypot(
            deviceTransform.m21(),
            deviceTransform.m22()
            );
    const qreal horizontalInset =
        horizontalScale > 0.0
            ? BorderWidth / (2.0 * horizontalScale)
            : BorderWidth / 2.0;
    const qreal verticalInset =
        verticalScale > 0.0
            ? BorderWidth / (2.0 * verticalScale)
            : BorderWidth / 2.0;
    const QRectF pillRect =
        QRectF(bounds.adjusted(0, 0, -trailingGap, 0)).adjusted(
            horizontalInset,
            verticalInset,
            -horizontalInset,
            -verticalInset
            );
    const QRect contentRect =
        pillRect.toAlignedRect().adjusted(
            HorizontalPadding,
            0,
            -HorizontalPadding,
            0
            );
    const QFontMetrics metrics(font);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setFont(font);
    painter->setBrush(fillColor);
    QPen borderPen(borderColor, BorderWidth);
    borderPen.setCosmetic(true);
    painter->setPen(borderPen);
    painter->drawRoundedRect(pillRect, Radius, Radius);
    painter->setPen(textColor);

    if (icon.isNull())
    {
        painter->drawText(
            contentRect,
            Qt::AlignCenter,
            metrics.elidedText(text, elideMode, contentRect.width())
            );
        painter->restore();
        return;
    }

    const int iconExtent =
        style->pixelMetric(
            QStyle::PM_SmallIconSize,
            nullptr,
            widget
            );
    const QSize iconSize =
        icon.actualSize(QSize(iconExtent, iconExtent));
    const int textWidth =
        metrics.horizontalAdvance(text);
    const int combinedWidth =
        iconSize.width() + IconSpacing + textWidth;
    const int iconLeft =
        contentRect.center().x() - (combinedWidth / 2);
    const QRect iconRect(
        iconLeft,
        contentRect.center().y() - (iconSize.height() / 2),
        iconSize.width(),
        iconSize.height()
        );

    icon.paint(
        painter,
        iconRect,
        Qt::AlignCenter,
        state.enabled ? QIcon::Normal : QIcon::Disabled,
        state.selected ? QIcon::On : QIcon::Off
        );

    const QRect textRect(
        iconRect.right() + 1 + IconSpacing,
        contentRect.top(),
        contentRect.right() - iconRect.right() - IconSpacing,
        contentRect.height()
        );
    painter->drawText(
        textRect,
        Qt::AlignVCenter | Qt::AlignLeft,
        metrics.elidedText(text, elideMode, textRect.width())
        );
    painter->restore();
}

}
