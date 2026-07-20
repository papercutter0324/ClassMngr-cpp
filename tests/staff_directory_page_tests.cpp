#include "features/teacher/ui/staff_directory_page.h"

#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"

#include <QFont>
#include <QLabel>
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
};

void StaffDirectoryPageTests::nativeEnglishTeacherColumnsAndEditing()
{
    StaffDirectoryPage page(nullptr, StaffDirectoryKind::NativeEnglishTeachers);
    auto* table = page.findChild<QTableWidget*>(
        QStringLiteral("nativeEnglishTeachersTable"));
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(0)->text(), QStringLiteral("Name"));
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral("Position"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Phone Number"));
    QCOMPARE(table->horizontalHeaderItem(3)->text(), QStringLiteral("Birthday"));
    QCOMPARE(table->horizontalHeaderItem(4)->text(), QStringLiteral("Nationality"));

    QPushButton* add = buttonWithText(page, QStringLiteral("Add"));
    QVERIFY(add);
    add->click();
    QCOMPARE(table->rowCount(), 1);
    QVERIFY(page.hasUnsavedChanges());

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

QTEST_MAIN(StaffDirectoryPageTests)

#include "staff_directory_page_tests.moc"
