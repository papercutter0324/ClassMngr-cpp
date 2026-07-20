#include "features/teacher/ui/teacher_info_page.h"

#include <QCalendarWidget>
#include <QCoreApplication>
#include <QGridLayout>
#include <QLineEdit>
#include <QLocale>
#include <QMetaObject>
#include <QtTest>

namespace
{
QGridLayout* containingGrid(
    const TeacherInfoPage& page,
    QWidget* widget
    )
{
    const QList<QGridLayout*> grids =
        page.findChildren<QGridLayout*>();

    for (QGridLayout* grid : grids)
    {
        if (grid->indexOf(widget) >= 0)
        {
            return grid;
        }
    }

    return nullptr;
}

void verifyPosition(
    QGridLayout* grid,
    QWidget* widget,
    int expectedRow,
    int expectedColumn
    )
{
    QVERIFY(grid);
    QVERIFY(widget);

    int row = -1;
    int column = -1;
    int rowSpan = -1;
    int columnSpan = -1;
    grid->getItemPosition(
        grid->indexOf(widget),
        &row,
        &column,
        &rowSpan,
        &columnSpan
        );

    QCOMPARE(row, expectedRow);
    QCOMPARE(column, expectedColumn);
}
} // namespace

class TeacherInfoPageTests : public QObject
{
    Q_OBJECT

private slots:
    void personalDetailsUseRequestedTwoRowOrder();
    void birthdayUsesCalendarMonthAndDayOnly();
};

void TeacherInfoPageTests::personalDetailsUseRequestedTwoRowOrder()
{
    TeacherInfoPage page(nullptr);

    auto* english = page.findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit"));
    auto* korean = page.findChild<QLineEdit*>(
        QStringLiteral("teacherKrEdit"));
    auto* romanization = page.findChild<QLineEdit*>(
        QStringLiteral("preferredRomanizationEdit"));
    auto* room = page.findChild<QLineEdit*>(
        QStringLiteral("roomNumberEdit"));
    auto* birthday = page.findChild<QLineEdit*>(
        QStringLiteral("birthdayEdit"));
    auto* phone = page.findChild<QLineEdit*>(
        QStringLiteral("phoneNumberEdit"));

    QGridLayout* grid = containingGrid(page, english);
    verifyPosition(grid, english, 1, 0);
    verifyPosition(grid, korean, 1, 1);
    verifyPosition(grid, romanization, 1, 2);
    verifyPosition(grid, room, 4, 0);
    verifyPosition(grid, birthday, 4, 1);
    verifyPosition(grid, phone, 4, 2);

    page.resize(640, 480);
    page.show();
    QCoreApplication::processEvents();
    QCOMPARE(birthday->width(), korean->width());
}

void TeacherInfoPageTests::birthdayUsesCalendarMonthAndDayOnly()
{
    TeacherInfoPage page(nullptr);

    Teacher teacher;
    teacher.id = 1;
    teacher.teacherEn = QStringLiteral("Alex");
    teacher.birthday = QStringLiteral("02-29");
    page.loadTeacher(teacher);

    auto* birthday = page.findChild<QLineEdit*>(
        QStringLiteral("birthdayEdit"));
    QVERIFY(birthday);
    QVERIFY(birthday->isReadOnly());
    QCOMPARE(
        birthday->text(),
        QLocale().toString(
            QDate(2000, 2, 29),
            QStringLiteral("MMMM d")
            )
        );

    auto* calendar = birthday->findChild<QCalendarWidget*>();
    QVERIFY(calendar);
    QCOMPARE(calendar->minimumDate(), QDate(2000, 1, 1));
    QCOMPARE(calendar->maximumDate(), QDate(2000, 12, 31));
    QCOMPARE(
        calendar->verticalHeaderFormat(),
        QCalendarWidget::NoVerticalHeader
        );
    QVERIFY(!page.hasUnsavedChanges());

    const QDate selectedDate(2000, 12, 31);
    QVERIFY(
        QMetaObject::invokeMethod(
            calendar,
            "clicked",
            Qt::DirectConnection,
            Q_ARG(QDate, selectedDate)
            )
        );
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(
        birthday->text(),
        QLocale().toString(selectedDate, QStringLiteral("MMMM d"))
        );

    birthday->clear();
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(birthday->text().isEmpty());
    QCOMPARE(birthday->placeholderText(), QStringLiteral("Not set"));
}

QTEST_MAIN(TeacherInfoPageTests)

#include "teacher_info_page_tests.moc"
