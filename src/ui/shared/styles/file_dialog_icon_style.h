#pragma once

#include <QProxyStyle>

class FileDialogIconStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QIcon standardIcon(
        StandardPixmap standardIcon,
        const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr
        ) const override;

    void polish(
        QWidget* widget
        ) override;
};
