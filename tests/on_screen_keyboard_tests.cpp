#include "ui/shared/input/hangul_composer.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QImage>
#include <QDateEdit>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QtTest>

class OnScreenKeyboardTests : public QObject
{
    Q_OBJECT

private slots:
    void composesSimpleSyllableAndMovesFinal();
    void composesCompoundVowelsAndFinals();
    void backspaceUnwindsComposition();
    void widgetTypesKoreanAndEnglish();
    void widgetTypesShiftedKorean();
    void widgetCommitsBeforeRetargeting();
    void selectedTableCellBecomesTarget();
    void editingControlsReachTarget();
    void tableCellCommitsWhenEnterPressed();
    void mouseClicksKeepTableCellTargeted();
    void characterKeysHaveUniformCompactWidth();
    void triggerUsesIconAndAccessibleText();
    void triggerIconUsesThemePropertyBeforePaletteIsPolished();
    void focusScopeStartsUntargetedAndTracksEditableFields();
    void focusScopeCommitsEditableTableCells();
    void focusScopeRejectsReadOnlyAndSpinBoxFields();
};

void OnScreenKeyboardTests::composesSimpleSyllableAndMovesFinal()
{
    HangulComposer composer;

    QCOMPARE(composer.input(u'ㄱ').preedit, QStringLiteral("ㄱ"));
    QCOMPARE(composer.input(u'ㅏ').preedit, QStringLiteral("가"));
    QCOMPARE(composer.input(u'ㄴ').preedit, QStringLiteral("간"));

    const HangulCompositionResult moved = composer.input(u'ㅏ');
    QCOMPARE(moved.committed, QStringLiteral("가"));
    QCOMPARE(moved.preedit, QStringLiteral("나"));
}

void OnScreenKeyboardTests::composesCompoundVowelsAndFinals()
{
    HangulComposer composer;

    QVERIFY(composer.input(u'ㄱ').consumed);
    QVERIFY(composer.input(u'ㅗ').consumed);
    QCOMPARE(composer.input(u'ㅏ').preedit, QStringLiteral("과"));
    QCOMPARE(composer.input(u'ㅐ').preedit, QStringLiteral("괘"));

    composer.reset();
    QVERIFY(composer.input(u'ㄷ').consumed);
    QVERIFY(composer.input(u'ㅏ').consumed);
    QVERIFY(composer.input(u'ㄹ').consumed);
    QCOMPARE(composer.input(u'ㄱ').preedit, QStringLiteral("닭"));

    const HangulCompositionResult split = composer.input(u'ㅏ');
    QCOMPARE(split.committed, QStringLiteral("달"));
    QCOMPARE(split.preedit, QStringLiteral("가"));
}

void OnScreenKeyboardTests::backspaceUnwindsComposition()
{
    HangulComposer composer;

    QVERIFY(composer.input(u'ㄱ').consumed);
    QVERIFY(composer.input(u'ㅗ').consumed);
    QVERIFY(composer.input(u'ㅏ').consumed);
    QVERIFY(composer.input(u'ㅐ').consumed);
    QCOMPARE(composer.preedit(), QStringLiteral("괘"));
    QCOMPARE(composer.backspace().preedit, QStringLiteral("과"));
    QCOMPARE(composer.backspace().preedit, QStringLiteral("고"));
    QCOMPARE(composer.backspace().preedit, QStringLiteral("ㄱ"));
    QCOMPARE(composer.backspace().preedit, QString{});
    QVERIFY(!composer.backspace().consumed);
}

void OnScreenKeyboardTests::widgetTypesKoreanAndEnglish()
{
    OnScreenKeyboard keyboard;
    QLineEdit editor;
    keyboard.setTarget(&editor);

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardSpace")
        )->click();
    QCOMPARE(editor.text(), QStringLiteral("가 "));

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardLanguage")
        )->click();
    QVERIFY(!keyboard.isKoreanLayout());
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardShift")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_q")
        )->click();
    QCOMPARE(editor.text(), QStringLiteral("가 Q"));
}

void OnScreenKeyboardTests::widgetTypesShiftedKorean()
{
    OnScreenKeyboard keyboard;
    QLineEdit editor;
    keyboard.setTarget(&editor);

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardShift")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardSpace")
        )->click();

    QCOMPARE(editor.text(), QStringLiteral("까 "));
}

void OnScreenKeyboardTests::widgetCommitsBeforeRetargeting()
{
    OnScreenKeyboard keyboard;
    QLineEdit first;
    QLineEdit second;
    keyboard.setTarget(&first);

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();
    keyboard.setTarget(&second);

    QCOMPARE(first.text(), QStringLiteral("가"));
    QVERIFY(second.text().isEmpty());
    QCOMPARE(keyboard.target(), &second);
}

void OnScreenKeyboardTests::selectedTableCellBecomesTarget()
{
    QStandardItemModel model(1, 2);
    model.setData(model.index(0, 0), QStringLiteral("First"));
    model.setData(model.index(0, 1), QStringLiteral("Second"));

    QTableView table;
    table.setModel(&model);
    table.resize(420, 180);
    table.show();
    table.setCurrentIndex(model.index(0, 0));
    QApplication::processEvents();

    OnScreenKeyboard keyboard;
    keyboard.showFor(&table);
    QApplication::processEvents();
    QVERIFY(keyboard.target());
    QVERIFY(table.isAncestorOf(keyboard.target()));

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();

    QTest::mouseClick(
        table.viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        table.visualRect(model.index(0, 1)).center()
        );
    QApplication::processEvents();
    QCOMPARE(
        model.data(model.index(0, 0)).toString(),
        QStringLiteral("가")
        );
    QVERIFY(keyboard.target());
    QVERIFY(table.isAncestorOf(keyboard.target()));
    keyboard.close();
}

void OnScreenKeyboardTests::editingControlsReachTarget()
{
    OnScreenKeyboard keyboard;
    QLineEdit editor(QStringLiteral("AB"));
    editor.setCursorPosition(editor.text().size());
    QSignalSpy returnSpy(&editor, &QLineEdit::returnPressed);
    keyboard.setTarget(&editor);

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardBackspace")
        )->click();
    QCOMPARE(editor.text(), QStringLiteral("A"));

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardEnter")
        )->click();
    QCOMPARE(returnSpy.count(), 1);
}

void OnScreenKeyboardTests::tableCellCommitsWhenEnterPressed()
{
    QStandardItemModel model(1, 1);
    QTableView table;
    table.setModel(&model);
    table.resize(300, 180);
    table.show();
    table.setCurrentIndex(model.index(0, 0));
    QApplication::processEvents();

    OnScreenKeyboard keyboard;
    keyboard.showFor(&table);
    QApplication::processEvents();

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardEnter")
        )->click();

    QCOMPARE(
        model.data(model.index(0, 0)).toString(),
        QStringLiteral("가")
        );
    keyboard.close();
}

void OnScreenKeyboardTests::mouseClicksKeepTableCellTargeted()
{
    QStandardItemModel model(1, 1);
    QTableView table;
    table.setModel(&model);
    table.resize(300, 180);
    table.show();
    table.setCurrentIndex(model.index(0, 0));
    QApplication::processEvents();

    OnScreenKeyboard keyboard;
    keyboard.showFor(&table);
    QApplication::processEvents();

    auto* consonant = keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        );
    auto* vowel = keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        );
    QVERIFY(consonant);
    QVERIFY(vowel);

    QTest::mouseClick(consonant, Qt::LeftButton);
    QTest::mouseClick(vowel, Qt::LeftButton);
    QVERIFY(keyboard.target());
    QVERIFY(table.isAncestorOf(keyboard.target()));

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardEnter")
        )->click();
    QCOMPARE(
        model.data(model.index(0, 0)).toString(),
        QStringLiteral("가")
        );
    keyboard.close();
}

void OnScreenKeyboardTests::characterKeysHaveUniformCompactWidth()
{
    OnScreenKeyboard keyboard;
    const auto characterKeys = keyboard.findChildren<QPushButton*>(
        QRegularExpression(
            QStringLiteral("^onScreenKeyboardKey_")
            )
        );

    QVERIFY(!characterKeys.isEmpty());

    const int keyWidth = characterKeys.first()->minimumWidth();
    QVERIFY(keyWidth > 0);

    for (QPushButton* key : characterKeys)
    {
        QCOMPARE(key->minimumWidth(), keyWidth);
        QCOMPARE(key->maximumWidth(), keyWidth);
    }
}

void OnScreenKeyboardTests::triggerUsesIconAndAccessibleText()
{
    OnScreenKeyboard keyboard;
    QPushButton trigger;
    trigger.setAccessibleName(QStringLiteral("Korean Keyboard"));
    keyboard.setTriggerButton(&trigger);

    QVERIFY(!trigger.icon().isNull());
    QCOMPARE(
        trigger.accessibleName(),
        QStringLiteral("Korean Keyboard")
        );
}

void OnScreenKeyboardTests
    ::triggerIconUsesThemePropertyBeforePaletteIsPolished()
{
    OnScreenKeyboard keyboard;
    QPushButton trigger;
    QPalette staleLightPalette = trigger.palette();
    staleLightPalette.setColor(
        QPalette::ButtonText,
        QColor(Qt::black)
        );
    trigger.setPalette(staleLightPalette);
    keyboard.setTriggerButton(&trigger);

    trigger.setProperty("theme", QStringLiteral("dark"));

    const QImage icon = trigger
        .icon()
        .pixmap(trigger.iconSize())
        .toImage()
        .convertToFormat(QImage::Format_ARGB32);
    bool foundWhiteStroke = false;

    for (int y = 0; y < icon.height() && !foundWhiteStroke; ++y)
    {
        for (int x = 0; x < icon.width(); ++x)
        {
            const QColor pixel = icon.pixelColor(x, y);

            if (pixel.alpha() > 220 && pixel.lightness() > 220)
            {
                foundWhiteStroke = true;
                break;
            }
        }
    }

    QVERIFY(foundWhiteStroke);
}

void OnScreenKeyboardTests
    ::focusScopeStartsUntargetedAndTracksEditableFields()
{
    QWidget scope;
    auto* layout = new QVBoxLayout(&scope);
    auto* editor = new QLineEdit(&scope);
    layout->addWidget(editor);
    scope.show();
    QApplication::processEvents();

    OnScreenKeyboard keyboard(&scope);
    keyboard.showForFocusScope(&scope);
    QApplication::processEvents();

    QVERIFY(keyboard.isVisible());
    QVERIFY(!keyboard.target());

    editor->setFocus();
    QFocusEvent editorFocusIn(QEvent::FocusIn, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(editor, &editorFocusIn);
    QCOMPARE(keyboard.target(), editor);

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardEnter")
        )->click();

    QCOMPARE(editor->text(), QStringLiteral("가"));
    keyboard.close();
}

void OnScreenKeyboardTests::focusScopeCommitsEditableTableCells()
{
    QStandardItemModel model(1, 1);
    QWidget scope;
    auto* layout = new QVBoxLayout(&scope);
    auto* table = new QTableView(&scope);
    table->setModel(&model);
    layout->addWidget(table);
    scope.resize(420, 240);
    scope.show();
    QApplication::processEvents();

    OnScreenKeyboard keyboard(&scope);
    keyboard.showForFocusScope(&scope);
    QApplication::processEvents();
    QVERIFY(!keyboard.target());

    const QModelIndex cell = model.index(0, 0);
    table->setCurrentIndex(cell);
    table->edit(cell);
    QApplication::processEvents();

    auto* editor = table->findChild<QLineEdit*>();
    QVERIFY(editor);
    editor->setFocus();
    QFocusEvent editorFocusIn(QEvent::FocusIn, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(editor, &editorFocusIn);
    QCOMPARE(keyboard.target(), editor);

    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();
    keyboard.findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardEnter")
        )->click();
    QApplication::processEvents();

    QCOMPARE(model.data(cell).toString(), QStringLiteral("가"));
    keyboard.close();
}

void OnScreenKeyboardTests::focusScopeRejectsReadOnlyAndSpinBoxFields()
{
    QWidget scope;
    auto* layout = new QVBoxLayout(&scope);
    auto* readOnly = new QLineEdit(&scope);
    readOnly->setReadOnly(true);
    auto* date = new QDateEdit(&scope);
    layout->addWidget(readOnly);
    layout->addWidget(date);
    scope.show();
    QApplication::processEvents();

    OnScreenKeyboard keyboard(&scope);
    keyboard.showForFocusScope(&scope);

    readOnly->setFocus();
    QFocusEvent readOnlyFocusIn(QEvent::FocusIn, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(readOnly, &readOnlyFocusIn);
    QVERIFY(!keyboard.target());

    auto* dateEditor = date->findChild<QLineEdit*>();
    QVERIFY(dateEditor);
    QFocusEvent dateFocusIn(QEvent::FocusIn, Qt::OtherFocusReason);
    QCoreApplication::sendEvent(dateEditor, &dateFocusIn);
    QVERIFY(!keyboard.target());
    keyboard.close();
}

QTEST_MAIN(OnScreenKeyboardTests)
#include "on_screen_keyboard_tests.moc"
