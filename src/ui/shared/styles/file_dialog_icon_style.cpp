#include "file_dialog_icon_style.h"

#include <QApplication>
#include <QColor>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QPalette>
#include <QString>
#include <QStyleOption>

namespace
{
constexpr auto FileIconThemeProperty =
    "classMngrFileIconTheme";

QString themeName(
    const QPalette& palette
    )
{
    return palette.color(
        QPalette::Active,
        QPalette::ButtonText
        ).lightness() > 127
        ? QStringLiteral("dark")
        : QStringLiteral("light");
}

QIcon fileDialogIcon(
    const QString& name,
    const QPalette& palette
    )
{
    const QString theme = themeName(palette);
    const QString pathPrefix =
        QStringLiteral(":/assets/icons/file_dialog/%1_%2")
            .arg(name, theme);

    QIcon icon;
    const QString normalPath =
        pathPrefix + QStringLiteral(".svg");
    const QString disabledPath =
        pathPrefix + QStringLiteral("_disabled.svg");

    constexpr QIcon::Mode normalModes[]{
        QIcon::Normal,
        QIcon::Active,
        QIcon::Selected
    };
    constexpr QIcon::State states[]{
        QIcon::Off,
        QIcon::On
    };

    for (const QIcon::Mode mode : normalModes)
    {
        for (const QIcon::State state : states)
        {
            icon.addFile(
                normalPath,
                {},
                mode,
                state
                );
        }
    }

    for (const QIcon::State state : states)
    {
        icon.addFile(
            disabledPath,
            {},
            QIcon::Disabled,
            state
            );
    }

    return icon;
}

QString fileProviderIconName(
    QFileIconProvider::IconType type
    )
{
    switch (type)
    {
    case QFileIconProvider::Computer:
        return QStringLiteral("computer");
    case QFileIconProvider::Desktop:
        return QStringLiteral("desktop");
    case QFileIconProvider::Trashcan:
        return QStringLiteral("trash");
    case QFileIconProvider::Network:
        return QStringLiteral("network");
    case QFileIconProvider::Drive:
        return QStringLiteral("drive");
    case QFileIconProvider::Folder:
        return QStringLiteral("folder");
    case QFileIconProvider::File:
        return QStringLiteral("file");
    }

    return QStringLiteral("file");
}

class StaticFileIconProvider final : public QFileIconProvider
{
public:
    explicit StaticFileIconProvider(
        const QPalette& palette
        )
        : m_palette(palette)
    {
    }

    QIcon icon(
        IconType type
        ) const override
    {
        return fileDialogIcon(
            fileProviderIconName(type),
            m_palette
            );
    }

    QIcon icon(
        const QFileInfo& info
        ) const override
    {
        const QString name =
            info.isRoot()
                ? QStringLiteral("drive")
                : info.isDir()
                    ? QStringLiteral("folder")
                    : QStringLiteral("file");

        return fileDialogIcon(
            name,
            m_palette
            );
    }

private:
    QPalette m_palette;
};

void installStaticFileIconProvider(
    QFileDialog* fileDialog
    )
{
    if (!fileDialog || !fileDialog->iconProvider())
    {
        return;
    }

    const QString theme =
        themeName(fileDialog->palette());

    if (
        fileDialog->property(FileIconThemeProperty)
            == theme
        )
    {
        return;
    }

    fileDialog->setIconProvider(
        new StaticFileIconProvider(
            fileDialog->palette()
            )
        );
    fileDialog->setProperty(
        FileIconThemeProperty,
        theme
        );
}

QString standardIconName(
    QStyle::StandardPixmap standardIcon
    )
{
    switch (standardIcon)
    {
    case QStyle::SP_ArrowBack:
        return QStringLiteral("back");
    case QStyle::SP_ArrowForward:
        return QStringLiteral("forward");
    case QStyle::SP_FileDialogToParent:
        return QStringLiteral("parent");
    case QStyle::SP_FileDialogNewFolder:
        return QStringLiteral("new_folder");
    case QStyle::SP_FileDialogDetailedView:
        return QStringLiteral("details");
    case QStyle::SP_FileDialogListView:
        return QStringLiteral("list");
    case QStyle::SP_FileIcon:
        return QStringLiteral("file");
    case QStyle::SP_DialogOpenButton:
        return QStringLiteral("open");
    case QStyle::SP_DialogSaveButton:
        return QStringLiteral("save");
    case QStyle::SP_DialogCloseButton:
        return QStringLiteral("close");

    default:
        return {};
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
        installStaticFileIconProvider(fileDialog);
    }

    const QString iconName =
        standardIconName(standardIcon);

    if (iconName.isEmpty())
    {
        return QProxyStyle::standardIcon(
            standardIcon,
            option,
            widget
            );
    }

    return fileDialogIcon(
        iconName,
        iconPalette(option, widget)
        );
}

void FileDialogIconStyle::polish(
    QWidget* widget
    )
{
    QProxyStyle::polish(widget);

    auto* fileDialog =
        qobject_cast<QFileDialog*>(widget);

    installStaticFileIconProvider(fileDialog);
}
