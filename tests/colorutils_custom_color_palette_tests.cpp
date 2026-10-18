#include "core/utils/colorutils.h"
#include "next/application/custom_color_palette_preferences.h"

#include <QApplication>
#include <QColorDialog>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace
{

using ClassMngr::Next::Application::CustomColorPalette;
using ClassMngr::Next::Application::CustomColorPalettePreferencesPort;
using ClassMngr::Next::Domain::ErrorCode;
using ClassMngr::Next::Domain::OperationError;
using ClassMngr::Next::Domain::Result;

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

bool dialogShowsPalette(const CustomColorPalette& expected)
{
    for (std::size_t index = 0; index < expected.hexColors.size(); ++index)
    {
        if (QColorDialog::customColor(static_cast<int>(index))
            != QColor(QString::fromStdString(expected.hexColors[index])))
        {
            return false;
        }
    }
    return true;
}

class FakeCustomColorPalettePreferencesPort final
    : public CustomColorPalettePreferencesPort
{
public:
    explicit FakeCustomColorPalettePreferencesPort(
        CustomColorPalette palette =
            ClassMngr::Next::Application::defaultCustomColorPalette(),
        std::optional<OperationError> writeFailure = std::nullopt,
        std::optional<OperationError> readFailure = std::nullopt
        )
        : m_palette(std::move(palette))
        , m_writeFailure(std::move(writeFailure))
        , m_readFailure(std::move(readFailure))
    {
    }

    [[nodiscard]] Result<CustomColorPalette> read() const override
    {
        ++m_readCount;
        if (m_readFailure)
        {
            return Result<CustomColorPalette>::failure(*m_readFailure);
        }
        return Result<CustomColorPalette>::success(m_palette);
    }

    [[nodiscard]] Result<void> write(
        const CustomColorPalette& palette
        ) const override
    {
        ++m_writeCount;
        m_writtenPalette = palette;
        if (m_writeFailure)
        {
            return Result<void>::failure(*m_writeFailure);
        }
        return Result<void>::success();
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
    std::optional<OperationError> m_writeFailure;
    std::optional<OperationError> m_readFailure;
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
    void loadFailureAppliesFallbackColors();
    void saveWritesEveryDialogSlotAsOneCanonicalPalette();
    void saveCustomColorsReturnsTypedFailure();
    void getColorReportsFailureAfterAcceptedDialogCloses();
    void getColorReportsFailureAfterCanceledDialogCloses();
    void getColorReadFailureSkipsWriteAfterAcceptance();
    void getColorReadFailureSkipsWriteAfterCancellation();
    void getColorPersistsDefaultsAfterCanceledMissingRead();

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

void ColorUtilsCustomColorPaletteTests::
loadFailureAppliesFallbackColors()
{
    const OperationError failure{
        .code = ErrorCode::Technical,
        .message = "forced custom palette read failure",
        .recoverable = false
    };
    FakeCustomColorPalettePreferencesPort port(
        paletteWithDistinctColors(),
        std::nullopt,
        failure
        );

    ColorUtils::loadCustomColors(port);

    QCOMPARE(port.readCount(), 1);
    QCOMPARE(port.writeCount(), 0);
    QVERIFY(dialogShowsPalette(
        ClassMngr::Next::Application::defaultCustomColorPalette()
        ));
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

    const auto saved = ColorUtils::saveCustomColors(port);

    QVERIFY(saved);
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

void ColorUtilsCustomColorPaletteTests::
saveCustomColorsReturnsTypedFailure()
{
    const OperationError failure{
        .code = ErrorCode::Technical,
        .message = "forced custom palette failure",
        .recoverable = false
    };
    FakeCustomColorPalettePreferencesPort port(
        ClassMngr::Next::Application::defaultCustomColorPalette(),
        failure
        );

    const auto saved = ColorUtils::saveCustomColors(port);

    QVERIFY(!saved);
    QCOMPARE(saved.error(), failure);
    QCOMPARE(port.writeCount(), 1);
}

void ColorUtilsCustomColorPaletteTests::
getColorReportsFailureAfterAcceptedDialogCloses()
{
    const OperationError failure{
        .code = ErrorCode::Technical,
        .message = "forced custom palette failure",
        .recoverable = false
    };
    FakeCustomColorPalettePreferencesPort port(
        ClassMngr::Next::Application::defaultCustomColorPalette(),
        failure
        );
    const QColor selectedColor(QStringLiteral("#345678"));
    QPointer<QColorDialog> observedDialog;
    bool dialogWasHandled = false;
    int callbackCount = 0;
    bool callbackRanAfterDialogClosed = false;
    std::optional<OperationError> callbackError;

    QTimer::singleShot(0, [&observedDialog, &dialogWasHandled, selectedColor]
    {
        auto* const dialog = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        observedDialog = dialog;
        dialogWasHandled = true;
        dialog->setCurrentColor(selectedColor);
        dialog->accept();
    });

    const QColor result = ColorUtils::getColor(
        QColor(QStringLiteral("#ffffff")),
        nullptr,
        QStringLiteral("Pick a color"),
        port,
        [&]
        (
            const OperationError& error
            )
        {
            ++callbackCount;
            callbackError = error;
            callbackRanAfterDialogClosed =
                observedDialog && !observedDialog->isVisible();
        }
        );

    QVERIFY(dialogWasHandled);
    QCOMPARE(result, selectedColor);
    QCOMPARE(callbackCount, 1);
    QVERIFY(callbackRanAfterDialogClosed);
    QVERIFY(callbackError);
    QCOMPARE(*callbackError, failure);
    QCOMPARE(port.readCount(), 1);
    QCOMPARE(port.writeCount(), 1);
}

void ColorUtilsCustomColorPaletteTests::
getColorReportsFailureAfterCanceledDialogCloses()
{
    const OperationError failure{
        .code = ErrorCode::Technical,
        .message = "forced custom palette failure",
        .recoverable = false
    };
    FakeCustomColorPalettePreferencesPort port(
        ClassMngr::Next::Application::defaultCustomColorPalette(),
        failure
        );
    bool dialogWasHandled = false;
    QPointer<QColorDialog> observedDialog;
    int callbackCount = 0;
    bool callbackRanAfterDialogClosed = false;
    std::optional<OperationError> callbackError;

    QTimer::singleShot(0, [&observedDialog, &dialogWasHandled]
    {
        auto* const dialog = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        observedDialog = dialog;
        dialogWasHandled = true;
        dialog->reject();
    });

    const QColor result = ColorUtils::getColor(
        QColor(QStringLiteral("#ffffff")),
        nullptr,
        QStringLiteral("Pick a color"),
        port,
        [&]
        (
            const OperationError& error
            )
        {
            ++callbackCount;
            callbackError = error;
            callbackRanAfterDialogClosed =
                observedDialog && !observedDialog->isVisible();
        }
        );

    QVERIFY(dialogWasHandled);
    QVERIFY(!result.isValid());
    QCOMPARE(callbackCount, 1);
    QVERIFY(callbackRanAfterDialogClosed);
    QVERIFY(callbackError);
    QCOMPARE(*callbackError, failure);
    QCOMPARE(port.readCount(), 1);
    QCOMPARE(port.writeCount(), 1);
}

void ColorUtilsCustomColorPaletteTests::
getColorReadFailureSkipsWriteAfterAcceptance()
{
    const OperationError failure{
        .code = ErrorCode::Technical,
        .message = "forced custom palette read failure",
        .recoverable = false
    };
    FakeCustomColorPalettePreferencesPort port(
        paletteWithDistinctColors(),
        std::nullopt,
        failure
        );
    const QColor selectedColor(QStringLiteral("#456789"));
    bool dialogWasHandled = false;
    bool fallbackColorsWereDisplayed = false;
    int callbackCount = 0;

    QTimer::singleShot(0, [&]
    {
        auto* const dialog = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialogWasHandled = true;
        fallbackColorsWereDisplayed = dialogShowsPalette(
            ClassMngr::Next::Application::defaultCustomColorPalette()
            );
        dialog->setCurrentColor(selectedColor);
        dialog->accept();
    });

    const QColor result = ColorUtils::getColor(
        QColor(QStringLiteral("#ffffff")),
        nullptr,
        QStringLiteral("Pick a color"),
        port,
        [&](const OperationError&)
        {
            ++callbackCount;
        }
        );

    QVERIFY(dialogWasHandled);
    QVERIFY(fallbackColorsWereDisplayed);
    QCOMPARE(result, selectedColor);
    QCOMPARE(callbackCount, 0);
    QCOMPARE(port.readCount(), 1);
    QCOMPARE(port.writeCount(), 0);
}

void ColorUtilsCustomColorPaletteTests::
getColorReadFailureSkipsWriteAfterCancellation()
{
    const OperationError failure{
        .code = ErrorCode::Technical,
        .message = "forced custom palette read failure",
        .recoverable = false
    };
    FakeCustomColorPalettePreferencesPort port(
        paletteWithDistinctColors(),
        std::nullopt,
        failure
        );
    bool dialogWasHandled = false;
    bool fallbackColorsWereDisplayed = false;
    int callbackCount = 0;

    QTimer::singleShot(0, [&]
    {
        auto* const dialog = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialogWasHandled = true;
        fallbackColorsWereDisplayed = dialogShowsPalette(
            ClassMngr::Next::Application::defaultCustomColorPalette()
            );
        dialog->reject();
    });

    const QColor result = ColorUtils::getColor(
        QColor(QStringLiteral("#ffffff")),
        nullptr,
        QStringLiteral("Pick a color"),
        port,
        [&](const OperationError&)
        {
            ++callbackCount;
        }
        );

    QVERIFY(dialogWasHandled);
    QVERIFY(fallbackColorsWereDisplayed);
    QVERIFY(!result.isValid());
    QCOMPARE(callbackCount, 0);
    QCOMPARE(port.readCount(), 1);
    QCOMPARE(port.writeCount(), 0);
}

void ColorUtilsCustomColorPaletteTests::
getColorPersistsDefaultsAfterCanceledMissingRead()
{
    const CustomColorPalette defaults =
        ClassMngr::Next::Application::defaultCustomColorPalette();
    FakeCustomColorPalettePreferencesPort port(defaults);
    bool dialogWasHandled = false;
    bool fallbackColorsWereDisplayed = false;
    const QColor changedCustomColor(QStringLiteral("#aabbcc"));

    QTimer::singleShot(0, [&]
    {
        auto* const dialog = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialogWasHandled = true;
        fallbackColorsWereDisplayed = dialogShowsPalette(defaults);
        QColorDialog::setCustomColor(0, changedCustomColor);
        dialog->reject();
    });

    const QColor result = ColorUtils::getColor(
        QColor(QStringLiteral("#ffffff")),
        nullptr,
        QStringLiteral("Pick a color"),
        port,
        [](const OperationError&) {}
        );

    QVERIFY(dialogWasHandled);
    QVERIFY(fallbackColorsWereDisplayed);
    QVERIFY(!result.isValid());
    QCOMPARE(port.readCount(), 1);
    QCOMPARE(port.writeCount(), 1);
    QCOMPARE(
        port.writtenPalette().hexColors[0],
        changedCustomColor.name(QColor::HexRgb).toStdString()
        );
}

QTEST_MAIN(ColorUtilsCustomColorPaletteTests)

#include "colorutils_custom_color_palette_tests.moc"
