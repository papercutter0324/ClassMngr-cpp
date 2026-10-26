#include "features/teacher/ui/staff_directory_page.h"
#include "staff_directory_test_utils.h"

#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QApplication>
#include <QAbstractItemDelegate>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QPointer>
#include <QTableWidget>
#include <QTableView>
#include <QtTest>

#include <memory>

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
    void largeDirectoriesKeepRowsInTheModelWithoutCellWidgets();
    void clearAndRecreationReleaseRowsAndActiveEditors();
};

void StaffDirectoryPageTests::nativeEnglishTeacherColumnsAndEditing()
{
    StaffDirectoryPage page(nullptr, StaffDirectoryKind::NativeEnglishTeachers);
    auto* table = page.findChild<QTableView*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QVERIFY(!table->isSortingEnabled());
    QCOMPARE(table->selectionBehavior(), QAbstractItemView::SelectRows);
    QCOMPARE(table->selectionMode(), QAbstractItemView::ExtendedSelection);
    QVERIFY(!table->horizontalHeader()->sectionsMovable());
    QVERIFY(!table->horizontalHeader()->sectionsClickable());
    QCOMPARE(
        table->horizontalHeader()->sectionResizeMode(0),
        QHeaderView::Stretch);
    QCOMPARE(table->model()->columnCount(), 6);
    QCOMPARE(table->model()->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Name"));
    QCOMPARE(table->model()->headerData(1, Qt::Horizontal).toString(), QStringLiteral("Position"));
    QCOMPARE(table->model()->headerData(2, Qt::Horizontal).toString(), QStringLiteral("Phone Number"));
    QCOMPARE(table->model()->headerData(3, Qt::Horizontal).toString(), QStringLiteral("Email"));
    QCOMPARE(table->model()->headerData(4, Qt::Horizontal).toString(), QStringLiteral("Birthday"));
    QCOMPARE(table->model()->headerData(5, Qt::Horizontal).toString(), QStringLiteral("Nationality"));

    QPushButton* add = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(add);
    add->click();
    QCOMPARE(table->model()->rowCount(), 1);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        staffDirectoryCellData(*table, 0, 0, Qt::TextAlignmentRole).toInt(),
        Qt::AlignCenter);

    table->setCurrentIndex(staffDirectoryCell(*table, 0, 1));
    table->edit(staffDirectoryCell(*table, 0, 1));
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
    QTRY_COMPARE(staffDirectoryCellText(*table, 0, 1), QStringLiteral("NET"));
    QTRY_VERIFY(table->findChildren<QComboBox*>().isEmpty());

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
    auto* table = page.findChild<QTableView*>(QStringLiteral("gsTeamTable"));
    QVERIFY(table);
    QVERIFY(!table->isSortingEnabled());
    QVERIFY(!table->horizontalHeader()->sectionsMovable());
    QVERIFY(!table->horizontalHeader()->sectionsClickable());
    QCOMPARE(table->model()->columnCount(), 5);
    QCOMPARE(table->model()->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Name"));
    QCOMPARE(table->model()->headerData(1, Qt::Horizontal).toString(), QStringLiteral("Korean Name"));
    QCOMPARE(table->model()->headerData(2, Qt::Horizontal).toString(), QStringLiteral("Position"));
    QCOMPARE(table->model()->headerData(3, Qt::Horizontal).toString(), QStringLiteral("Phone Number"));
    QCOMPARE(table->model()->headerData(4, Qt::Horizontal).toString(), QStringLiteral("Birthday"));
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
        auto* table = page.findChild<QTableView*>();
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
        QCOMPARE(table->selectionBehavior(), QAbstractItemView::SelectRows);
        QCOMPARE(table->selectionMode(), QAbstractItemView::ExtendedSelection);

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
        QCOMPARE(table->findChildren<QLineEdit*>().size(), 1);
        QVERIFY2(
            editor->height() >= editor->sizeHint().height(),
            "The in-cell editor must fit inside the directory row without clipping.");
    }

    qApp->setStyleSheet(previousStyleSheet);
}

void StaffDirectoryPageTests::
largeDirectoriesKeepRowsInTheModelWithoutCellWidgets()
{
    for (const auto kind : {
             StaffDirectoryKind::NativeEnglishTeachers,
             StaffDirectoryKind::GsTeam})
    {
        StaffDirectoryPage page(nullptr, kind);
        page.setSaveMode(SaveMode::Manual);
        auto* table = page.findChild<QTableView*>();
        QVERIFY(table);
        QVERIFY(page.findChildren<QTableWidget*>().isEmpty());

        const int rows = 96;
        const int columns = kind == StaffDirectoryKind::NativeEnglishTeachers
            ? 6
            : 5;
        const qsizetype widgetCountBefore =
            page.findChildren<QWidget*>().size();
        QVERIFY(table->model()->insertRows(0, rows));
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                QVERIFY(setStaffDirectoryCellText(
                    *table,
                    row,
                    column,
                    QStringLiteral("Row %1, column %2")
                        .arg(row)
                        .arg(column)));
            }
        }

        QCOMPARE(table->model()->rowCount(), rows);
        QCOMPARE(table->model()->columnCount(), columns);
        QCOMPARE(page.findChildren<QWidget*>().size(), widgetCountBefore);
        for (int row = 0; row < rows; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                QVERIFY(!table->indexWidget(
                    table->model()->index(row, column)));
            }
        }
    }
}

void StaffDirectoryPageTests::clearAndRecreationReleaseRowsAndActiveEditors()
{
    for (const auto kind : {
             StaffDirectoryKind::NativeEnglishTeachers,
             StaffDirectoryKind::GsTeam})
    {
        for (int generation = 0; generation < 2; ++generation)
        {
            auto page = std::make_unique<StaffDirectoryPage>(nullptr, kind);
            page->setSaveMode(SaveMode::Manual);
            page->resize(1000, 700);
            page->show();
            QApplication::processEvents();
            QPointer<QTableView> table = page->findChild<QTableView*>();
            QVERIFY(table);
            QPointer<QAbstractItemModel> model = table->model();
            QPointer<QAbstractItemDelegate> delegate =
                kind == StaffDirectoryKind::NativeEnglishTeachers
                ? table->itemDelegateForColumn(1)
                : table->itemDelegate();
            QVERIFY(delegate);
            QCOMPARE(model->parent(), page.get());
            QCOMPARE(delegate->parent(), table.data());
            QCOMPARE(model->rowCount(), 0);
            const qsizetype idleWidgetCount = page->findChildren<QWidget*>().size();

            for (int refresh = 0; refresh < 2; ++refresh)
            {
                QVERIFY(model->insertRows(0, 96));
                QCOMPARE(model->index(0, 0).data(Qt::EditRole).toString(), QString());
                QVERIFY(model->setData(model->index(0, 0), QStringLiteral("current generation")));
                table->setCurrentIndex(model->index(0, 0));
                table->edit(model->index(0, 0));
                QTRY_COMPARE(table->findChildren<QLineEdit*>().size(), 1);
                QPointer<QLineEdit> editor = table->findChild<QLineEdit*>();

                page->clearDatabaseState();
                QCOMPARE(table->model(), model.data());
                QCOMPARE(model->rowCount(), 0);
                QVERIFY(!page->hasUnsavedChanges());
                QTRY_VERIFY(editor.isNull());
                QTRY_COMPARE(page->findChildren<QWidget*>().size(), idleWidgetCount);
            }

            page.reset();
            QVERIFY(table.isNull());
            QVERIFY(model.isNull());
            QVERIFY(delegate.isNull());
        }
    }
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
