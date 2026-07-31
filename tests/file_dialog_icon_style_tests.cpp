#include "ui/shared/styles/file_dialog_icon_style.h"

#include <QColor>
#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QImage>
#include <QPalette>
#include <QStandardPaths>
#include <QStringList>
#include <QStyleOption>
#include <QTest>
#include <QUrl>

namespace
{
QColor firstOpaquePixel(
    const QPixmap& pixmap
    )
{
    const QImage image =
        pixmap.toImage().convertToFormat(
            QImage::Format_ARGB32
            );

    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor pixel =
                QColor::fromRgba(
                    image.pixel(x, y)
                    );

            if (pixel.alpha() > 220)
            {
                return pixel;
            }
        }
    }

    return {};
}

bool colorsAreNear(
    const QColor& actual,
    const QColor& expected
    )
{
    constexpr int ChannelTolerance = 2;

    return qAbs(actual.red() - expected.red()) <= ChannelTolerance
        && qAbs(actual.green() - expected.green()) <= ChannelTolerance
        && qAbs(actual.blue() - expected.blue()) <= ChannelTolerance;
}
}

class FileDialogIconStyleTests : public QObject
{
    Q_OBJECT

private slots:
    void standardIconsUseStaticSvgVariants_data();
    void standardIconsUseStaticSvgVariants();
    void fileDialogUsesStaticSvgIconProvider();
    void fileDialogIncludesCommonSidebarFolders();
    void fileProviderCoversEveryIconType_data();
    void fileProviderCoversEveryIconType();
    void unrelatedStandardIconsComeFromBaseStyle();
};

void FileDialogIconStyleTests::standardIconsUseStaticSvgVariants_data()
{
    QTest::addColumn<QStyle::StandardPixmap>("standardIcon");

    QTest::newRow("back") << QStyle::SP_ArrowBack;
    QTest::newRow("forward") << QStyle::SP_ArrowForward;
    QTest::newRow("parent") << QStyle::SP_FileDialogToParent;
    QTest::newRow("new-folder") << QStyle::SP_FileDialogNewFolder;
    QTest::newRow("details") << QStyle::SP_FileDialogDetailedView;
    QTest::newRow("list") << QStyle::SP_FileDialogListView;
    QTest::newRow("file") << QStyle::SP_FileIcon;
    QTest::newRow("open") << QStyle::SP_DialogOpenButton;
    QTest::newRow("save") << QStyle::SP_DialogSaveButton;
    QTest::newRow("close") << QStyle::SP_DialogCloseButton;
}

void FileDialogIconStyleTests::standardIconsUseStaticSvgVariants()
{
    QFETCH(QStyle::StandardPixmap, standardIcon);

    FileDialogIconStyle style;

    struct ThemeColors
    {
        QColor normal;
        QColor disabled;
    };
    const ThemeColors themes[]{
        {
            QColor("#232629"),
            QColor("#707d8a")
        },
        {
            QColor("#fcfcfc"),
            QColor("#777777")
        }
    };

    for (const ThemeColors& colors : themes)
    {
        QStyleOption option;
        option.palette.setColor(
            QPalette::Active,
            QPalette::ButtonText,
            colors.normal
            );
        option.palette.setColor(
            QPalette::Disabled,
            QPalette::ButtonText,
            colors.disabled
            );

        const QIcon icon =
            style.standardIcon(
                standardIcon,
                &option
                );

        QVERIFY(!icon.isNull());
        QVERIFY(
            colorsAreNear(
                firstOpaquePixel(
                    icon.pixmap(
                        QSize(32, 32),
                        QIcon::Normal
                        )
                    ),
                colors.normal
                )
            );
        QVERIFY(
            colorsAreNear(
                firstOpaquePixel(
                    icon.pixmap(
                        QSize(32, 32),
                        QIcon::Disabled
                        )
                    ),
                colors.disabled
                )
            );
    }
}

void FileDialogIconStyleTests::fileDialogUsesStaticSvgIconProvider()
{
    FileDialogIconStyle style;
    QFileDialog dialog;
    dialog.setOption(
        QFileDialog::DontUseNativeDialog,
        true
        );

    QAbstractFileIconProvider* originalProvider =
        dialog.iconProvider();
    QVERIFY(originalProvider);

    style.polish(&dialog);

    QVERIFY(dialog.iconProvider());
    QVERIFY(dialog.iconProvider() != originalProvider);
}

void FileDialogIconStyleTests::fileDialogIncludesCommonSidebarFolders()
{
    FileDialogIconStyle style;
    QFileDialog dialog;
    dialog.setOption(
        QFileDialog::DontUseNativeDialog,
        true
        );

    style.polish(&dialog);
    style.polish(&dialog);

    const QString documentsPath =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );
    const QStringList expectedPaths{
        QStandardPaths::writableLocation(
            QStandardPaths::HomeLocation
            ),
        QStandardPaths::writableLocation(
            QStandardPaths::DesktopLocation
            ),
        documentsPath,
        QDir(documentsPath).filePath(
            QStringLiteral("DYB/ClassMngr")
            )
    };
    const QList<QUrl> sidebarUrls =
        dialog.sidebarUrls();

    for (const QString& path : expectedPaths)
    {
        const QUrl url =
            QUrl::fromLocalFile(
                QDir::cleanPath(path)
                );

        QVERIFY2(
            sidebarUrls.contains(url),
            qPrintable(
                QStringLiteral(
                    "The file-dialog sidebar does not contain %1"
                    ).arg(path)
                )
            );
        QCOMPARE(sidebarUrls.count(url), 1);
    }
}

void FileDialogIconStyleTests::fileProviderCoversEveryIconType_data()
{
    QTest::addColumn<QFileIconProvider::IconType>("iconType");

    QTest::newRow("computer") << QFileIconProvider::Computer;
    QTest::newRow("desktop") << QFileIconProvider::Desktop;
    QTest::newRow("trash") << QFileIconProvider::Trashcan;
    QTest::newRow("network") << QFileIconProvider::Network;
    QTest::newRow("drive") << QFileIconProvider::Drive;
    QTest::newRow("folder") << QFileIconProvider::Folder;
    QTest::newRow("file") << QFileIconProvider::File;
}

void FileDialogIconStyleTests::fileProviderCoversEveryIconType()
{
    QFETCH(QFileIconProvider::IconType, iconType);

    struct ThemeColors
    {
        QColor normal;
        QColor disabled;
    };
    const ThemeColors themes[]{
        {
            QColor("#232629"),
            QColor("#707d8a")
        },
        {
            QColor("#fcfcfc"),
            QColor("#777777")
        }
    };

    for (const ThemeColors& colors : themes)
    {
        FileDialogIconStyle style;
        QFileDialog dialog;
        dialog.setOption(
            QFileDialog::DontUseNativeDialog,
            true
            );
        QPalette palette = dialog.palette();
        palette.setColor(
            QPalette::Active,
            QPalette::ButtonText,
            colors.normal
            );
        palette.setColor(
            QPalette::Disabled,
            QPalette::ButtonText,
            colors.disabled
            );
        dialog.setPalette(palette);

        style.polish(&dialog);

        const QIcon icon =
            dialog.iconProvider()->icon(iconType);

        QVERIFY(!icon.isNull());
        QVERIFY(
            colorsAreNear(
                firstOpaquePixel(
                    icon.pixmap(
                        QSize(32, 32),
                        QIcon::Normal
                        )
                    ),
                colors.normal
                )
            );
        QVERIFY(
            colorsAreNear(
                firstOpaquePixel(
                    icon.pixmap(
                        QSize(32, 32),
                        QIcon::Disabled
                        )
                    ),
                colors.disabled
                )
            );
    }
}

void FileDialogIconStyleTests::unrelatedStandardIconsComeFromBaseStyle()
{
    FileDialogIconStyle style;

    const QIcon icon =
        style.standardIcon(
            QStyle::SP_MessageBoxInformation
            );

    QVERIFY(!icon.isNull());
}

QTEST_MAIN(FileDialogIconStyleTests)

#include "file_dialog_icon_style_tests.moc"
