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
class TwoToneFileDialogIconStyle final : public QProxyStyle
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
        painter.fillRect(6, 6, 4, 4, Qt::black);

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

QColor pixelAt(
    const QPixmap& pixmap,
    int x,
    int y
    )
{
    const QImage image =
        pixmap.toImage().convertToFormat(
            QImage::Format_ARGB32
            );

    return QColor::fromRgba(
        image.pixel(x, y)
        );
}

bool colorsAreNear(
    const QColor& actual,
    const QColor& expected
    )
{
    constexpr int ChannelTolerance = 1;

    return qAbs(actual.red() - expected.red()) <= ChannelTolerance
        && qAbs(actual.green() - expected.green()) <= ChannelTolerance
        && qAbs(actual.blue() - expected.blue()) <= ChannelTolerance;
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
    void darkGlyphOnLightBackgroundBecomesTintedMask();
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
        new TwoToneFileDialogIconStyle()
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

void FileDialogIconStyleTests::darkGlyphOnLightBackgroundBecomesTintedMask()
{
    QPixmap sourcePixmap(16, 16);
    sourcePixmap.fill(Qt::white);

    QPainter painter(&sourcePixmap);
    painter.fillRect(6, 6, 4, 4, Qt::black);
    painter.end();

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
            ThemedIconUtils::RecolorMode::DarkGlyphOnLightBackground
            );
    const QPixmap recoloredPixmap =
        recolored.pixmap(QSize(16, 16));

    QCOMPARE(
        pixelAt(recoloredPixmap, 0, 0).alpha(),
        0
        );
    QCOMPARE(
        pixelAt(recoloredPixmap, 8, 8).rgb(),
        iconColor.rgb()
        );
    QCOMPARE(
        pixelAt(recoloredPixmap, 8, 8).alpha(),
        255
        );
}

void FileDialogIconStyleTests::toolbarIconsFollowTheApplicationPalette()
{
    QFETCH(QStyle::StandardPixmap, standardIcon);

#if defined(Q_OS_WIN)
    const bool usesLightBitmapBackground =
        standardIcon == QStyle::SP_FileDialogDetailedView
        || standardIcon == QStyle::SP_FileDialogListView;
#endif

    auto* baseStyle =
        new TwoToneFileDialogIconStyle();
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
#if defined(Q_OS_WIN)
    if (usesLightBitmapBackground)
    {
        QVERIFY(
            colorsAreNear(
                pixelAt(
                    icon.pixmap(
                        QSize(16, 16),
                        QIcon::Disabled
                        ),
                    8,
                    8
                    ),
                disabledColor
                )
            );
        QCOMPARE(
            pixelAt(
                icon.pixmap(
                    QSize(16, 16),
                    QIcon::Normal
                    ),
                4,
                4
                ).alpha(),
            0
            );
        QCOMPARE(
            pixelAt(
                icon.pixmap(
                    QSize(16, 16),
                    QIcon::Normal
                    ),
                8,
                8
                ).rgb(),
            activeColor.rgb()
            );
    }
    else
    {
        QCOMPARE(
            firstOpaquePixel(
                icon.pixmap(
                    QSize(16, 16),
                    QIcon::Disabled
                    )
                ).rgb(),
            disabledColor.rgb()
            );
        QCOMPARE(
            pixelAt(
                icon.pixmap(
                    QSize(16, 16),
                    QIcon::Normal
                    ),
                8,
                8
                ).rgb(),
            QColor(Qt::black).rgb()
            );
    }
#else
    QCOMPARE(
        firstOpaquePixel(
            icon.pixmap(
                QSize(16, 16),
                QIcon::Disabled
                )
            ).rgb(),
        disabledColor.rgb()
        );
    QCOMPARE(
        pixelAt(
            icon.pixmap(
                QSize(16, 16),
                QIcon::Normal
                ),
            8,
            8
            ).rgb(),
        activeColor.rgb()
        );
#endif
}

QTEST_MAIN(FileDialogIconStyleTests)

#include "file_dialog_icon_style_tests.moc"
