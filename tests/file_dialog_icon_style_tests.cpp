#include "ui/shared/styles/file_dialog_icon_style.h"
#include "ui/shared/styles/themed_icon_utils.h"

#include <QColor>
#include <QFileDialog>
#include <QImage>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOption>
#include <QTest>

namespace
{
class WhiteFileDialogIconStyle final : public QProxyStyle
{
public:
    QIcon standardIcon(
        StandardPixmap standardIcon,
        const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr
        ) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)

        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.fillRect(4, 4, 8, 8, Qt::white);

        return QIcon(pixmap);
    }
};

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

            if (pixel.alpha() > 0)
            {
                return pixel;
            }
        }
    }

    return {};
}
}

class FileDialogIconStyleTests : public QObject
{
    Q_OBJECT

private slots:
    void toolbarIconsFollowTheApplicationPalette_data();
    void toolbarIconsFollowTheApplicationPalette();
    void fileDialogUsesThemedIconProvider();
    void lightNeutralFileIconsAreRecolored();
    void coloredFileIconsArePreserved();
};

void FileDialogIconStyleTests::toolbarIconsFollowTheApplicationPalette_data()
{
    QTest::addColumn<QStyle::StandardPixmap>("standardIcon");

    QTest::newRow("back")
        << QStyle::SP_ArrowBack;
    QTest::newRow("forward")
        << QStyle::SP_ArrowForward;
    QTest::newRow("parent")
        << QStyle::SP_FileDialogToParent;
    QTest::newRow("new-folder")
        << QStyle::SP_FileDialogNewFolder;
    QTest::newRow("details")
        << QStyle::SP_FileDialogDetailedView;
    QTest::newRow("list")
        << QStyle::SP_FileDialogListView;
}

void FileDialogIconStyleTests::fileDialogUsesThemedIconProvider()
{
    FileDialogIconStyle style(
        new WhiteFileDialogIconStyle()
        );
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

void FileDialogIconStyleTests::lightNeutralFileIconsAreRecolored()
{
    QPixmap sourcePixmap(16, 16);
    sourcePixmap.fill(Qt::white);

    QIcon source(sourcePixmap);
    QPalette palette;
    const QColor iconColor("#27313a");
    palette.setColor(
        QPalette::ButtonText,
        iconColor
        );

    const QIcon recolored =
        ThemedIconUtils::recolor(
            source,
            palette,
            ThemedIconUtils::RecolorMode::LightNeutralPixels
            );

    QCOMPARE(
        firstOpaquePixel(
            recolored.pixmap(QSize(16, 16))
            ).rgb(),
        iconColor.rgb()
        );
}

void FileDialogIconStyleTests::coloredFileIconsArePreserved()
{
    QPixmap sourcePixmap(16, 16);
    const QColor sourceColor("#1d99f3");
    sourcePixmap.fill(sourceColor);

    QIcon source(sourcePixmap);
    QPalette palette;
    palette.setColor(
        QPalette::ButtonText,
        QColor("#27313a")
        );

    const QIcon recolored =
        ThemedIconUtils::recolor(
            source,
            palette,
            ThemedIconUtils::RecolorMode::LightNeutralPixels
            );

    QCOMPARE(
        firstOpaquePixel(
            recolored.pixmap(QSize(16, 16))
            ).rgb(),
        sourceColor.rgb()
        );
}

void FileDialogIconStyleTests::toolbarIconsFollowTheApplicationPalette()
{
    QFETCH(QStyle::StandardPixmap, standardIcon);

    auto* baseStyle =
        new WhiteFileDialogIconStyle();
    FileDialogIconStyle style(baseStyle);

    QStyleOption option;
    const QColor activeColor("#27313a");
    const QColor disabledColor("#66727a");

    option.palette.setColor(
        QPalette::Active,
        QPalette::ButtonText,
        activeColor
        );
    option.palette.setColor(
        QPalette::Disabled,
        QPalette::ButtonText,
        disabledColor
        );

    const QIcon icon =
        style.standardIcon(
            standardIcon,
            &option
            );

    QCOMPARE(
        firstOpaquePixel(
            icon.pixmap(
                QSize(16, 16),
                QIcon::Normal
                )
            ).rgb(),
        activeColor.rgb()
        );
    QCOMPARE(
        firstOpaquePixel(
            icon.pixmap(
                QSize(16, 16),
                QIcon::Disabled
                )
            ).rgb(),
        disabledColor.rgb()
        );
}

QTEST_MAIN(FileDialogIconStyleTests)

#include "file_dialog_icon_style_tests.moc"
