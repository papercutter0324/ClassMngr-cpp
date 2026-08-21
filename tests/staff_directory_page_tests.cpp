#include "features/teacher/ui/staff_directory_page.h"

#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QApplication>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QtTest>

namespace
{
QPushButton* buttonWithText(QWidget& widget, const QString& text)
{
    for (QPushButton* button : widget.findChildren<QPushButton*>())
    {
        if (button->text() == text)
        {
            return button;
        }
    }
    return nullptr;
}
}

class StaffDirectoryPageTests : public QObject
{
    Q_OBJECT

private slots:
    void nativeEnglishTeacherColumnsAndEditing();
    void gsTeamColumns();
    void directoryHeadersUseSharedPageStyling();
    void directoryTablesProvideRoomForCellEditors();
    void directoryHeaderKeyboardOpensUntargeted();
};

void StaffDirectoryPageTests::nativeEnglishTeacherColumnsAndEditing()
{
    StaffDirectoryPage page(nullptr, StaffDirectoryKind::NativeEnglishTeachers);
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QVERIFY(!table->isSortingEnabled());
    QVERIFY(!table->horizontalHeader()->sectionsMovable());
    QVERIFY(!table->horizontalHeader()->sectionsClickable());
    QCOMPARE(table->columnCount(), 6);
    QCOMPARE(table->horizontalHeaderItem(0)->text(), QStringLiteral("Name"));
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral("Position"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Phone Number"));
    QCOMPARE(table->horizontalHeaderItem(3)->text(), QStringLiteral("Email"));
    QCOMPARE(table->horizontalHeaderItem(4)->text(), QStringLiteral("Birthday"));
    QCOMPARE(table->horizontalHeaderItem(5)->text(), QStringLiteral("Nationality"));

    QPushButton* add = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(add);
    add->click();
    QCOMPARE(table->rowCount(), 1);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(table->item(0, 0)->textAlignment(), Qt::AlignCenter);

    table->setCurrentCell(0, 1);
    table->editItem(table->item(0, 1));
    auto* position = table->findChild<QComboBox*>();
    QTRY_VERIFY(position);
    const QStringList expectedPositions{
        QStringLiteral("Coordinator"),
        QStringLiteral("Team Leader"),
        QStringLiteral("M3 Song's"),
        QStringLiteral("M2 Song's"),
        QStringLiteral("M1 Song's"),
        QStringLiteral("E6 Song's"),
        QStringLiteral("E5 Athena"),
        QStringLiteral("NET")
    };
    for (const QString& expectedPosition : expectedPositions)
    {
        QVERIFY(position->findText(expectedPosition) >= 0);
    }
    const int netPosition = position->findText(QStringLiteral("NET"));
    position->setCurrentIndex(netPosition);
    position->activated(netPosition);
    QTRY_COMPARE(table->item(0, 1)->text(), QStringLiteral("NET"));

    page.setSaveMode(SaveMode::Manual);
    QPushButton* save = buttonWithText(page, QStringLiteral("Save Changes *"));
    QVERIFY(save);
    QVERIFY(save->isVisibleTo(&page) || !page.isVisible());
    QVERIFY(save->isEnabled());
    QPushButton* discard = buttonWithText(page, QStringLiteral("Discard Changes"));
    QVERIFY(discard);
    QVERIFY(discard->isEnabled());
}

void StaffDirectoryPageTests::gsTeamColumns()
{
    StaffDirectoryPage page(nullptr, StaffDirectoryKind::GsTeam);
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QVERIFY(!table->isSortingEnabled());
    QVERIFY(!table->horizontalHeader()->sectionsMovable());
    QVERIFY(!table->horizontalHeader()->sectionsClickable());
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(0)->text(), QStringLiteral("Name"));
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral("Korean Name"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Position"));
    QCOMPARE(table->horizontalHeaderItem(3)->text(), QStringLiteral("Phone Number"));
    QCOMPARE(table->horizontalHeaderItem(4)->text(), QStringLiteral("Birthday"));
}

void StaffDirectoryPageTests::directoryHeadersUseSharedPageStyling()
{
    StaffDirectoryPage page(nullptr, StaffDirectoryKind::NativeEnglishTeachers);
    const auto* title = page.findChild<QLabel*>(QStringLiteral("pageTitle"));
    const auto* subtitle = page.findChild<QLabel*>(QStringLiteral("pageSubtitle"));
    QVERIFY(title);
    QVERIFY(subtitle);
    QCOMPARE(title->font(), FontManager::getUiFont(
        UiConstants::Pages::TitleFontSize, QFont::Bold));
    QCOMPARE(subtitle->font(), FontManager::getUiFont(
        UiConstants::Pages::SubtitleFontSize));
}

void StaffDirectoryPageTests::directoryTablesProvideRoomForCellEditors()
{
    const QString previousStyleSheet = qApp->styleSheet();
    qApp->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 2px solid #4b5563; "
        "border-radius: 6px; padding: 5px 8px; }"));

    for (const auto kind : {
             StaffDirectoryKind::NativeEnglishTeachers,
             StaffDirectoryKind::GsTeam})
    {
        StaffDirectoryPage page(nullptr, kind);
        auto* table = page.findChild<QTableWidget*>();
        QVERIFY(table);

        QLineEdit editorProbe;
        editorProbe.setFont(table->font());
        editorProbe.ensurePolished();

        QVERIFY(table->verticalHeader()->defaultSectionSize()
                >= editorProbe.sizeHint().height() + 8);
        QVERIFY(table->horizontalHeader()->height()
                >= table->horizontalHeader()->fontMetrics().height() + 16);
        QCOMPARE(
            table->verticalHeader()->sectionResizeMode(0),
            QHeaderView::Fixed);
        QVERIFY(table->alternatingRowColors());
        QVERIFY(!table->wordWrap());

        QFont enlargedFont = table->font();
        enlargedFont.setPointSize(enlargedFont.pointSize() + 4);
        table->setFont(enlargedFont);
        editorProbe.setFont(enlargedFont);
        editorProbe.ensurePolished();
        QVERIFY(table->verticalHeader()->defaultSectionSize()
                >= editorProbe.sizeHint().height() + 8);

        page.resize(1000, 700);
        page.show();
        QPushButton* add = buttonWithText(page, QStringLiteral("Add"));
        QVERIFY(add);
        add->click();

        auto* editor = table->findChild<QLineEdit*>();
        QTRY_VERIFY(editor);
        QVERIFY2(
            editor->height() >= editor->sizeHint().height(),
            "The in-cell editor must fit inside the directory row without clipping.");
    }

    qApp->setStyleSheet(previousStyleSheet);
}

void StaffDirectoryPageTests::directoryHeaderKeyboardOpensUntargeted()
{
    for (const auto kind : {
             StaffDirectoryKind::NativeEnglishTeachers,
             StaffDirectoryKind::GsTeam})
    {
        StaffDirectoryPage page(nullptr, kind);
        page.resize(1000, 700);
        page.show();
        QApplication::processEvents();

        auto* trigger = page.findChild<QPushButton*>(
            QStringLiteral("staffDirectoryKoreanKeyboardButton")
            );
        auto* keyboard = page.findChild<OnScreenKeyboard*>();
        QVERIFY(trigger);
        QVERIFY(keyboard);
        QVERIFY(!trigger->icon().isNull());
        QCOMPARE(trigger->accessibleName(), QStringLiteral("Korean Keyboard"));

        trigger->click();
        QApplication::processEvents();
        QVERIFY(keyboard->isVisible());
        QVERIFY(!keyboard->target());
        keyboard->close();
    }
}

QTEST_MAIN(StaffDirectoryPageTests)

#include "staff_directory_page_tests.moc"
