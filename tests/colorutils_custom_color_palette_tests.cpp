#include "core/utils/colorutils.h"
#include "next/application/custom_color_palette_preferences.h"

#include <QColorDialog>
#include <QString>
#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace
{

using ClassMngr::Next::Application::CustomColorPalette;
using ClassMngr::Next::Application::CustomColorPalettePreferencesPort;

constexpr std::array<const char*, CustomColorPalette::EntryCount> ColorHexes = {{
    "#102030",
    "#213141",
    "#324252",
    "#435363",
    "#546474",
    "#657585",
    "#768696",
    "#8797A7",
    "#98A8B8",
    "#A9B9C9",
    "#BACADA",
    "#CBDDEB",
    "#DCEEFF",
    "#EDF0A1",
    "#FE12B3",
    "#0F24C5"
}};

CustomColorPalette paletteWithDistinctColors()
{
    CustomColorPalette palette;
    for (std::size_t index = 0; index < ColorHexes.size(); ++index)
    {
        palette.hexColors[index] = ColorHexes[index];
    }
    return palette;
}

class FakeCustomColorPalettePreferencesPort final
    : public CustomColorPalettePreferencesPort
{
public:
    explicit FakeCustomColorPalettePreferencesPort(
        CustomColorPalette palette =
            ClassMngr::Next::Application::defaultCustomColorPalette()
        )
        : m_palette(std::move(palette))
    {
    }

    [[nodiscard]] CustomColorPalette read() const override
    {
        ++m_readCount;
        return m_palette;
    }

    void write(const CustomColorPalette& palette) const override
    {
        ++m_writeCount;
        m_writtenPalette = palette;
    }

    [[nodiscard]] int readCount() const noexcept
    {
        return m_readCount;
    }

    [[nodiscard]] int writeCount() const noexcept
    {
        return m_writeCount;
    }

    [[nodiscard]] const CustomColorPalette& writtenPalette() const noexcept
    {
        return m_writtenPalette;
    }

private:
    CustomColorPalette m_palette;
    mutable int m_readCount = 0;
    mutable int m_writeCount = 0;
    mutable CustomColorPalette m_writtenPalette =
        ClassMngr::Next::Application::defaultCustomColorPalette();
};

} // namespace

class ColorUtilsCustomColorPaletteTests final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void loadCopiesEveryPaletteEntryToTheDialog();
    void saveWritesEveryDialogSlotAsOneCanonicalPalette();

private:
    std::array<QColor, CustomColorPalette::EntryCount> m_originalColors;
};

void ColorUtilsCustomColorPaletteTests::init()
{
    for (std::size_t index = 0; index < m_originalColors.size(); ++index)
    {
        m_originalColors[index] = QColorDialog::customColor(
            static_cast<int>(index)
            );
    }
}

void ColorUtilsCustomColorPaletteTests::cleanup()
{
    for (std::size_t index = 0; index < m_originalColors.size(); ++index)
    {
        QColorDialog::setCustomColor(
            static_cast<int>(index),
            m_originalColors[index]
            );
    }
}

void ColorUtilsCustomColorPaletteTests::
loadCopiesEveryPaletteEntryToTheDialog()
{
    const auto expected = paletteWithDistinctColors();
    FakeCustomColorPalettePreferencesPort port(expected);

    ColorUtils::loadCustomColors(port);

    QCOMPARE(port.readCount(), 1);
    QCOMPARE(port.writeCount(), 0);
    for (std::size_t index = 0; index < expected.hexColors.size(); ++index)
    {
        QCOMPARE(
            QColorDialog::customColor(static_cast<int>(index)),
            QColor(QString::fromStdString(expected.hexColors[index]))
            );
    }
}

void ColorUtilsCustomColorPaletteTests::
saveWritesEveryDialogSlotAsOneCanonicalPalette()
{
    const auto expected = paletteWithDistinctColors();
    for (std::size_t index = 0; index < expected.hexColors.size(); ++index)
    {
        QColorDialog::setCustomColor(
            static_cast<int>(index),
            QColor(QString::fromStdString(expected.hexColors[index]))
            );
    }
    FakeCustomColorPalettePreferencesPort port;

    ColorUtils::saveCustomColors(port);

    QCOMPARE(port.readCount(), 0);
    QCOMPARE(port.writeCount(), 1);
    for (std::size_t index = 0; index < expected.hexColors.size(); ++index)
    {
        const QColor color(QString::fromStdString(expected.hexColors[index]));
        QVERIFY(
            port.writtenPalette().hexColors[index]
            == color.name(QColor::HexRgb).toStdString()
            );
    }
}

QTEST_MAIN(ColorUtilsCustomColorPaletteTests)

#include "colorutils_custom_color_palette_tests.moc"
