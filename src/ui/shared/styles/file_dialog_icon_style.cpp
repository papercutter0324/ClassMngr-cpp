#include "file_dialog_icon_style.h"

#include "ui/shared/styles/themed_icon_utils.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QPalette>
#include <QStyleOption>

namespace
{
constexpr auto FileIconPaletteProperty =
    "classMngrFileIconPalette";

class ThemedFileIconProvider final : public QFileIconProvider
{
public:
    explicit ThemedFileIconProvider(
        const QPalette& palette
        )
        : m_palette(palette)
    {
    }

    QIcon icon(
        IconType type
        ) const override
    {
        return ThemedIconUtils::recolor(
            QFileIconProvider::icon(type),
            m_palette,
            ThemedIconUtils::RecolorMode::LightNeutralPixels
            );
    }

    QIcon icon(
        const QFileInfo& info
        ) const override
    {
        return ThemedIconUtils::recolor(
            QFileIconProvider::icon(info),
            m_palette,
            ThemedIconUtils::RecolorMode::LightNeutralPixels
            );
    }

private:
    QPalette m_palette;
};

void installThemedFileIconProvider(
    QFileDialog* fileDialog
    )
{
    if (!fileDialog || !fileDialog->iconProvider())
    {
        return;
    }

    const QColor buttonText =
        fileDialog->palette().color(
            QPalette::ButtonText
            );

    if (
        fileDialog->property(FileIconPaletteProperty)
            == buttonText
        )
    {
        return;
    }

    fileDialog->setIconProvider(
        new ThemedFileIconProvider(
            fileDialog->palette()
            )
        );
    fileDialog->setProperty(
        FileIconPaletteProperty,
        buttonText
        );
}

bool isFileDialogToolbarIcon(
    QStyle::StandardPixmap standardIcon
    )
{
    switch (standardIcon)
    {
    case QStyle::SP_ArrowBack:
    case QStyle::SP_ArrowForward:
    case QStyle::SP_FileDialogToParent:
    case QStyle::SP_FileDialogNewFolder:
    case QStyle::SP_FileDialogDetailedView:
    case QStyle::SP_FileDialogListView:
    case QStyle::SP_FileIcon:
    case QStyle::SP_DialogOpenButton:
    case QStyle::SP_DialogSaveButton:
    case QStyle::SP_DialogCloseButton:
        return true;

    default:
        return false;
    }
}

QPalette iconPalette(
    const QStyleOption* option,
    const QWidget* widget
    )
{
    if (widget)
    {
        return widget->palette();
    }

    if (option)
    {
        return option->palette;
    }

    return QApplication::palette();
}

ThemedIconUtils::RecolorMode toolbarIconRecolorMode(
    QStyle::StandardPixmap standardIcon
    )
{
#if defined(Q_OS_WIN)
    if (
        standardIcon == QStyle::SP_FileDialogDetailedView
        || standardIcon == QStyle::SP_FileDialogListView
        )
    {
        // These Windows bitmaps contain a dark glyph on an opaque
        // light background. Convert that background into transparency.
        return ThemedIconUtils::RecolorMode::DarkGlyphOnLightBackground;
    }

    return ThemedIconUtils::RecolorMode::LightNeutralPixels;
#else
    Q_UNUSED(standardIcon)
    return ThemedIconUtils::RecolorMode::AllPixels;
#endif
}

}

QIcon FileDialogIconStyle::standardIcon(
    StandardPixmap standardIcon,
    const QStyleOption* option,
    const QWidget* widget
    ) const
{
    if (auto* fileDialog = qobject_cast<QFileDialog*>(
            const_cast<QWidget*>(widget)); fileDialog)
    {
        installThemedFileIconProvider(fileDialog);
    }

    const QIcon icon =
        QProxyStyle::standardIcon(
            standardIcon,
            option,
            widget
            );

    if (
        icon.isNull()
        || !isFileDialogToolbarIcon(standardIcon)
        )
    {
        return icon;
    }

    return ThemedIconUtils::recolor(
        icon,
        iconPalette(option, widget),
        toolbarIconRecolorMode(standardIcon)
        );
}

void FileDialogIconStyle::polish(
    QWidget* widget
    )
{
    QProxyStyle::polish(widget);

    auto* fileDialog =
        qobject_cast<QFileDialog*>(widget);

    installThemedFileIconProvider(fileDialog);
}
