#include "core/application_services.h"
#include "data/data_service.h"
#include "domain/models/roster.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "features/roster/ui/roster_table_view.h"
#include "ui/shared/widgets/marquee_item_delegate.h"

#include <algorithm>

#include <QtTest>

#include <QApplication>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>

namespace ScheduleWidgetTestStubs
{
void reset();
}

void RosterEditorWidget::importScores()
{
}

void RosterEditorWidget::printRosters()
{
}

namespace
{

TestingClass testingClass(
    const QString& name
    )
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M1");
    value.level = QStringLiteral("Major");
    value.room = QStringLiteral("401");
    value.teacherId = 7;
    value.classColor = QStringLiteral("#336699");
    value.fontColor = QStringLiteral("#FFFFFF");
    return value;
}

Roster rosterWithEvaluation()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columnWidths = {
        170,
        120,
        130,
        130,
        130,
        130
    };
    roster.rows.append(
        {
            QStringLiteral("Alex"),
            QStringLiteral("김학생"),
            QStringLiteral("A"),
            QStringLiteral("B"),
            QStringLiteral("C"),
            QStringLiteral("D")
        }
        );
    return roster;
}

int columnByName(
    const QAbstractItemModel* model,
    const QString& name
    )
{
    if (!model)
    {
        return -1;
    }

    for (int column = 0; column < model->columnCount(); ++column)
    {
        if (
            model
                ->headerData(
                    column,
                    Qt::Horizontal,
                    Qt::DisplayRole
                    )
                .toString()
                .compare(name, Qt::CaseInsensitive) == 0
            )
        {
            return column;
        }
    }

    return -1;
}

} // namespace

class TestingClassesPageTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void rosterEditorOmitsRemoveButtonAndKeepsContextAction();
    void testingClassesUsesNarrowNonCollapsibleNavigation();
    void testingRosterHidesEvaluationsWithoutLosingData();
};

void TestingClassesPageTests::init()
{
    ScheduleWidgetTestStubs::reset();
}

void TestingClassesPageTests
    ::rosterEditorOmitsRemoveButtonAndKeepsContextAction()
{
    ApplicationServices services;
    services.dataService()->saveRoster(
        42,
        rosterWithEvaluation()
        );

    RosterEditorWidget editor(
        &services,
        true
        );
    editor.loadClass(
        Classroom(
            QStringLiteral("Hercules"),
            42
            )
        );
    editor.resize(900, 500);
    editor.show();
    QApplication::processEvents();

    const auto buttons =
        editor.findChildren<QPushButton*>();

    QVERIFY(
        std::none_of(
            buttons.cbegin(),
            buttons.cend(),
            [](const QPushButton* button)
            {
                return button->text()
                    == QStringLiteral("Remove Student");
            }
            )
        );

    auto* table =
        editor.findChild<RosterTableView*>(
            QStringLiteral("rosterTable")
            );
    QVERIFY(table);
    QCOMPARE(
        table->contextMenuPolicy(),
        Qt::CustomContextMenu
        );

    bool foundRemoveAction = false;
    QTimer::singleShot(
        0,
        this,
        [&foundRemoveAction]()
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                auto* menu =
                    qobject_cast<QMenu*>(widget);
                if (!menu || !menu->isVisible())
                {
                    continue;
                }

                for (QAction* action : menu->actions())
                {
                    if (
                        action->text()
                            == QStringLiteral("Remove Student")
                        )
                    {
                        foundRemoveAction = true;
                        break;
                    }
                }

                menu->close();
            }
        }
        );

    const QModelIndex firstCell =
        table->model()->index(0, 0);
    QVERIFY(firstCell.isValid());
    QVERIFY(
        QMetaObject::invokeMethod(
            &editor,
            "showRosterContextMenu",
            Qt::DirectConnection,
            Q_ARG(
                QPoint,
                table->visualRect(firstCell).center()
                )
            )
        );
    QVERIFY(foundRemoveAction);
}

void TestingClassesPageTests
    ::testingClassesUsesNarrowNonCollapsibleNavigation()
{
    ApplicationServices services;
    const QString longName =
        QStringLiteral(
            "Exceptionally Long Testing Class Name for Marquee Verification"
            );
    const Result<int> created =
        services.dataService()->createTestingClass(
            testingClass(longName)
            );
    QVERIFY(created);

    TestingClassesPage page(&services);
    page.resize(1100, 720);
    page.show();
    page.openTestingClass(*created);
    QApplication::processEvents();

    auto* splitter =
        page.findChild<QSplitter*>(
            QStringLiteral("testingClassesSplitter")
            );
    auto* list =
        page.findChild<QListWidget*>(
            QStringLiteral("testingClassesList")
            );
    auto* addButton =
        page.findChild<QPushButton*>(
            QStringLiteral("testingClassesAddButton")
            );
    auto* deleteButton =
        page.findChild<QPushButton*>(
            QStringLiteral("testingClassesDeleteButton")
            );

    QVERIFY(splitter);
    QVERIFY(list);
    QVERIFY(addButton);
    QVERIFY(deleteButton);
    QVERIFY(!splitter->childrenCollapsible());
    QVERIFY(!splitter->isCollapsible(0));
    QVERIFY(!splitter->isCollapsible(1));

    const QList<int> sizes =
        splitter->sizes();
    QCOMPARE(sizes.size(), 2);
    QVERIFY(sizes.first() >= 220);
    QVERIFY(sizes.first() <= 280);
    QVERIFY(sizes.last() > sizes.first() * 2);

    QCOMPARE(
        list->horizontalScrollBarPolicy(),
        Qt::ScrollBarAsNeeded
        );
    QCOMPARE(
        list->horizontalScrollMode(),
        QAbstractItemView::ScrollPerPixel
        );
    QCOMPARE(list->textElideMode(), Qt::ElideNone);
    QVERIFY(!list->wordWrap());
    QVERIFY(
        dynamic_cast<MarqueeItemDelegate*>(
            list->itemDelegate()
            )
        );
    QCOMPARE(list->count(), 1);
    QVERIFY(!list->item(0)->text().contains(QLatin1Char('\n')));
    QVERIFY(list->item(0)->text().startsWith(longName));
    QVERIFY(
        list->item(0)->text().contains(
            QStringLiteral(" — M1 — Major")
            )
        );
    QTRY_VERIFY(
        list->horizontalScrollBar()->maximum() > 0
        );
    QVERIFY(
        deleteButton->geometry().top()
        > addButton->geometry().bottom()
        );
}

void TestingClassesPageTests
    ::testingRosterHidesEvaluationsWithoutLosingData()
{
    ApplicationServices services;
    const Result<int> created =
        services.dataService()->createTestingClass(
            testingClass(
                QStringLiteral("Writing Lab")
                )
            );
    QVERIFY(created);
    services.dataService()->saveRoster(
        *created,
        rosterWithEvaluation()
        );

    TestingClassesPage page(&services);
    page.resize(1000, 700);
    page.show();
    page.openTestingClass(*created);
    QApplication::processEvents();

    auto* testingEditor =
        page.findChild<RosterEditorWidget*>();
    auto* testingTable =
        page.findChild<RosterTableView*>(
            QStringLiteral("rosterTable")
            );
    QVERIFY(testingEditor);
    QVERIFY(testingTable);

    const QStringList evaluationColumns{
        QStringLiteral("Winter"),
        QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };

    for (const QString& name : evaluationColumns)
    {
        const int column =
            columnByName(
                testingTable->model(),
                name
                );
        QVERIFY(column >= 0);
        QVERIFY(testingTable->isColumnHidden(column));
    }

    const int englishColumn =
        columnByName(
            testingTable->model(),
            QStringLiteral("English")
            );
    const int koreanColumn =
        columnByName(
            testingTable->model(),
            QStringLiteral("Korean")
            );
    QVERIFY(englishColumn >= 0);
    QVERIFY(koreanColumn >= 0);
    QVERIFY(!testingTable->isColumnHidden(englishColumn));
    QVERIFY(!testingTable->isColumnHidden(koreanColumn));

    QVERIFY(
        testingTable->model()->setData(
            testingTable->model()->index(
                0,
                englishColumn
                ),
            QStringLiteral("Alex Updated"),
            Qt::EditRole
            )
        );
    testingEditor->saveData();

    const Roster saved =
        services.dataService()->loadRoster(*created);
    const int winterColumn =
        saved.columns.indexOf(
            QStringLiteral("Winter")
            );
    QVERIFY(winterColumn >= 0);
    QCOMPARE(
        saved.rows.value(0).value(winterColumn),
        QStringLiteral("A")
        );

    RosterEditorWidget ordinaryEditor(
        &services,
        true
        );
    ordinaryEditor.loadClass(
        Classroom(
            QStringLiteral("Writing Lab"),
            *created
            )
        );
    auto* ordinaryTable =
        ordinaryEditor.findChild<RosterTableView*>(
            QStringLiteral("rosterTable")
            );
    QVERIFY(ordinaryTable);

    for (const QString& name : evaluationColumns)
    {
        const int column =
            columnByName(
                ordinaryTable->model(),
                name
                );
        QVERIFY(column >= 0);
        QVERIFY(!ordinaryTable->isColumnHidden(column));
    }
}

QTEST_MAIN(TestingClassesPageTests)

#include "testing_classes_page_tests.moc"
