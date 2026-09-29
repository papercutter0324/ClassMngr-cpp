#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/roster.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "features/roster/ui/roster_table_view.h"
#include "fakes/fake_user_prompt_service.h"
#include "next/application/schedule_testing_class_choices_query.h"
#include "next/application/testing_class_details_read_query.h"
#include "next/application/testing_teacher_choices_read_query.h"
#include "ui/shared/widgets/marquee_item_delegate.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QtTest>

#include <QApplication>
#include <QItemSelectionModel>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QSqlError>
#include <QSqlQuery>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTemporaryDir>
#include <QUuid>

#include <optional>
#include <string>

namespace ScheduleWidgetTestStubs
{
void reset();
void setTestingClass(const TestingClass& testingClass);
}

void RosterEditorWidget::importScores()
{
}

void RosterEditorWidget::outputRosters(bool print)
{
    Q_UNUSED(print);
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

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("testing-classes-page-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

ClassMngr::Next::Domain::ClassId typedClassId(const int value)
{
    const auto parsed = ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(value)
        );
    if (!parsed)
    {
        qFatal("Test class ID must have a typed representation.");
    }
    return *parsed;
}

ClassMngr::Next::Domain::TeacherId typedTeacherId(const int value)
{
    const auto parsed = ClassMngr::Next::Domain::TeacherId::fromString(
        std::to_string(value)
        );
    if (!parsed)
    {
        qFatal("Test teacher ID must have a typed representation.");
    }
    return *parsed;
}

ClassMngr::Next::Application::TestingClassDetailsSnapshot testingClassDetails(
    const int classId,
    const QString& name
    )
{
    return {
        .classId = typedClassId(classId),
        .name = name.toStdU16String(),
        .grade = u"M1",
        .level = u"Major",
        .room = u"401",
        .teacherId =
            ClassMngr::Next::Domain::TeacherId::fromString("7"),
        .classColor = u"#336699",
        .fontColor = u"#FFFFFF",
        .notes = u""
    };
}

class FixedTestingClassChoicesReadPort final
    : public ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadPort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult readTestingClassChoices(
            const ClassMngr::Next::Application::
                ScheduleTestingClassChoicesReadQuery& query
            ) const override
    {
        ++callCount;
        lastQuery = query;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadQuery> lastQuery;
    ClassMngr::Next::Application::ScheduleTestingClassChoicesReadResult
        result = ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadResult::success({});
};

class FixedTestingClassDetailsReadPort final
    : public ClassMngr::Next::Application::TestingClassDetailsReadPort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::
        TestingClassDetailsReadResult readTestingClassDetails(
            const ClassMngr::Next::Application::
                TestingClassDetailsReadQuery& query
            ) const override
    {
        ++callCount;
        lastQuery = query;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<ClassMngr::Next::Application::
        TestingClassDetailsReadQuery> lastQuery;
    ClassMngr::Next::Application::TestingClassDetailsReadResult result =
        ClassMngr::Next::Application::TestingClassDetailsReadResult::
            failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = "Unconfigured testing class details read port.",
                .recoverable = false
            });
};

class FixedTestingTeacherChoicesReadPort final
    : public ClassMngr::Next::Application::TestingTeacherChoicesReadPort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult readTestingTeacherChoices(
            const ClassMngr::Next::Application::
                TestingTeacherChoicesReadQuery& query
            ) const override
    {
        ++callCount;
        lastQuery = query;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<ClassMngr::Next::Application::
        TestingTeacherChoicesReadQuery> lastQuery;
    ClassMngr::Next::Application::TestingTeacherChoicesReadResult result =
        ClassMngr::Next::Application::TestingTeacherChoicesReadResult::
            success({});
};

void setSingleTestingClassChoice(
    FixedTestingClassChoicesReadPort& readPort,
    const int classId,
    const QString& name
    )
{
    readPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::success({
            .choices = {{
                .classId = typedClassId(classId),
                .name = name.toStdU16String(),
                .grade = u"M1",
                .level = u"Major",
                .room = u"401"
            }}
        });
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
    void cleanup();
    void rosterEditorOmitsRemoveButtonAndKeepsContextAction();
    void rosterTableSupportsMultiCellSelection();
    void rosterKeyboardTriggerOpensInAppPalette();
    void rosterKeyboardWritesToKoreanNameCell();
    void outputAvailabilityFollowsRosterTabAndLoadedClass();
    void testingClassesUsesNarrowNonCollapsibleNavigation();
    void testingClassChoicesQueryMapsActiveSessionAndPreferredSelection();
    void unavailableTestingClassChoicesQueryIsSilentAndEmpty();
    void testingClassChoicesQueryFailureShowsExactWarning();
    void testingTeacherChoicesPopulateOrderedTrimmedChoicesAndRestoreSelection();
    void unavailableTestingTeacherChoicesQueryIsSilent();
    void testingTeacherChoicesQueryFailureShowsExactWarning();
    void testingClassDetailsReadMapsEditorRosterAndCleanState();
    void unavailableTestingClassDetailsReadIsSilent();
    void missingTestingClassDetailsShowsWarning();
    void testingClassDetailsReadFailureShowsWarningWithoutFallback();
    void zeroTeacherIdKeepsNoneSelectedWithoutWarning();
    void testingRosterHidesEvaluationsWithoutLosingData();
};

void TestingClassesPageTests::init()
{
    ScheduleWidgetTestStubs::reset();
}

void TestingClassesPageTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void TestingClassesPageTests
    ::rosterEditorOmitsRemoveButtonAndKeepsContextAction()
{
    ApplicationServices services;
    QVERIFY(services.dataService()->saveRoster(
        42,
        rosterWithEvaluation()
        ).has_value());

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

void TestingClassesPageTests::rosterTableSupportsMultiCellSelection()
{
    QStandardItemModel model(2, 2);
    model.setData(model.index(0, 0), QStringLiteral("First"));
    model.setData(model.index(0, 1), QStringLiteral("Second"));

    RosterTableView table;
    table.setModel(&model);
    table.resize(320, 160);
    table.show();
    QApplication::processEvents();

    QCOMPARE(
        table.selectionMode(),
        QAbstractItemView::ExtendedSelection
        );

    const QModelIndex firstCell = model.index(0, 0);
    const QModelIndex secondCell = model.index(0, 1);

    QTest::mouseClick(
        table.viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        table.visualRect(firstCell).center()
        );
    QTest::mouseClick(
        table.viewport(),
        Qt::LeftButton,
        Qt::ControlModifier,
        table.visualRect(secondCell).center()
        );

    const QModelIndexList selectedIndexes =
        table.selectionModel()->selectedIndexes();
    QCOMPARE(selectedIndexes.size(), 2);
    QVERIFY(selectedIndexes.contains(firstCell));
    QVERIFY(selectedIndexes.contains(secondCell));
}

void TestingClassesPageTests
    ::rosterKeyboardTriggerOpensInAppPalette()
{
    ApplicationServices services;
    RosterEditorWidget editor(&services, true);
    editor.loadClass(
        Classroom(
            QStringLiteral("Athena"),
            43
            )
        );
    editor.resize(900, 500);
    editor.show();
    QApplication::processEvents();

    auto* trigger = editor.findChild<QPushButton*>(
        QStringLiteral("rosterKoreanKeyboardButton")
        );
    auto* keyboard = editor.findChild<OnScreenKeyboard*>();
    QVERIFY(trigger);
    QVERIFY(keyboard);
    QVERIFY(trigger->text().isEmpty());
    QVERIFY(!trigger->icon().isNull());
    QCOMPARE(
        trigger->accessibleName(),
        QStringLiteral("Korean Keyboard")
        );
    QVERIFY(trigger->toolTip().contains(QStringLiteral("on-screen")));

    trigger->click();
    QApplication::processEvents();
    QVERIFY(keyboard->isVisible());
    keyboard->close();
}

void TestingClassesPageTests
    ::rosterKeyboardWritesToKoreanNameCell()
{
    ApplicationServices services;
    RosterEditorWidget editor(&services, true);
    editor.loadClass(
        Classroom(
            QStringLiteral("Athena"),
            43
            )
        );
    editor.resize(900, 500);
    editor.show();
    QApplication::processEvents();

    auto* table = editor.findChild<RosterTableView*>();
    auto* trigger = editor.findChild<QPushButton*>(
        QStringLiteral("rosterKoreanKeyboardButton")
        );
    auto* keyboard = editor.findChild<OnScreenKeyboard*>();
    QVERIFY(table);
    QVERIFY(trigger);
    QVERIFY(keyboard);

    const int koreanColumn = columnByName(
        table->model(),
        QStringLiteral("Korean")
        );
    QVERIFY(koreanColumn >= 0);
    const QModelIndex koreanNameCell =
        table->model()->index(0, koreanColumn);
    QVERIFY(koreanNameCell.isValid());

    table->setCurrentIndex(koreanNameCell);
    trigger->click();
    QApplication::processEvents();
    QVERIFY(keyboard->target());

    keyboard->findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_r")
        )->click();
    keyboard->findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardKey_k")
        )->click();
    keyboard->findChild<QPushButton*>(
        QStringLiteral("onScreenKeyboardEnter")
        )->click();
    QApplication::processEvents();

    QCOMPARE(
        table->model()->data(koreanNameCell).toString(),
        QStringLiteral("가")
        );
    keyboard->close();
}

void TestingClassesPageTests
    ::outputAvailabilityFollowsRosterTabAndLoadedClass()
{
    ApplicationServices services;
    const Result<int> created =
        services.dataService()->createTestingClass(
            testingClass(QStringLiteral("Output Availability"))
            );
    QVERIFY(created);

    FixedTestingClassChoicesReadPort readPort;
    readPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::success({
            .choices = {{
                .classId = typedClassId(*created),
                .name = u"Output Availability",
                .grade = u"M1",
                .level = u"Major",
                .room = u"401"
            }}
        });
    FixedTestingClassDetailsReadPort detailsReadPort;
    detailsReadPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::success(testingClassDetails(
            *created,
            QStringLiteral("Output Availability")
            ));
    TestingClassesPage page(&services, nullptr, &readPort, &detailsReadPort);
    page.setDatabaseOpen(true);
    page.openTestingClass(*created);
    page.activate();

    auto* tabs = page.findChild<QTabWidget*>(
        QStringLiteral("testingClassesTabs")
        );
    QVERIFY(tabs);

    QVERIFY(!page.outputCapabilities().printEnabled);
    QVERIFY(!page.outputCapabilities().saveAsEnabled);

    tabs->setCurrentIndex(1);
    QVERIFY(page.outputCapabilities().printEnabled);
    QVERIFY(page.outputCapabilities().saveAsEnabled);

    tabs->setCurrentIndex(2);
    QVERIFY(!page.outputCapabilities().printEnabled);
    QVERIFY(!page.outputCapabilities().saveAsEnabled);
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

    FixedTestingClassChoicesReadPort readPort;
    readPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::success({
            .choices = {{
                .classId = typedClassId(*created),
                .name = longName.toStdU16String(),
                .grade = u"M1",
                .level = u"Major",
                .room = u"401"
            }}
        });
    FixedTestingClassDetailsReadPort detailsReadPort;
    detailsReadPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::success(testingClassDetails(
            *created,
            longName
            ));
    TestingClassesPage page(&services, nullptr, &readPort, &detailsReadPort);
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

void TestingClassesPageTests::
testingClassChoicesQueryMapsActiveSessionAndPreferredSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);

    TestingClass zuluMajor = testingClass(QStringLiteral("Zulu Major"));
    zuluMajor.grade = QStringLiteral("M1");
    zuluMajor.level = QStringLiteral("Major");
    zuluMajor.room = QStringLiteral("Room 411");
    zuluMajor.teacherId = -1;
    const auto zuluMajorId = repository->createTestingClass(zuluMajor);
    QVERIFY(zuluMajorId);

    TestingClass omegaM2 = testingClass(QStringLiteral("Omega M2"));
    omegaM2.grade = QStringLiteral("M2");
    omegaM2.level = QStringLiteral("Major");
    omegaM2.room = QStringLiteral("Room 414");
    omegaM2.teacherId = -1;
    const auto omegaM2Id = repository->createTestingClass(omegaM2);
    QVERIFY(omegaM2Id);

    TestingClass betaSongs = testingClass(QStringLiteral("Beta Songs"));
    betaSongs.grade = QStringLiteral("M1");
    betaSongs.level = QStringLiteral("Song's");
    betaSongs.room = QStringLiteral("Room 412");
    betaSongs.teacherId = -1;
    const auto betaSongsId = repository->createTestingClass(betaSongs);
    QVERIFY(betaSongsId);

    TestingClass alphaMajor = testingClass(QStringLiteral("Alpha Major"));
    alphaMajor.grade = QStringLiteral("M1");
    alphaMajor.level = QStringLiteral("Major");
    alphaMajor.room = QStringLiteral("Room 413");
    alphaMajor.teacherId = -1;
    const auto alphaMajorId = repository->createTestingClass(alphaMajor);
    QVERIFY(alphaMajorId);

    alphaMajor.classId = *alphaMajorId;
    ScheduleWidgetTestStubs::setTestingClass(alphaMajor);

    FixedTestingClassDetailsReadPort detailsReadPort;
    detailsReadPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::success(testingClassDetails(
            *alphaMajorId,
            alphaMajor.name
            ));
    TestingClassesPage page(&services, nullptr, nullptr, &detailsReadPort);
    page.resize(1000, 700);
    page.show();
    page.openTestingClass(*alphaMajorId);
    QApplication::processEvents();

    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    QVERIFY(list);
    QCOMPARE(list->count(), 4);
    QCOMPARE(
        list->item(0)->text(),
        QStringLiteral("Beta Songs — M1 — Song's")
        );
    QCOMPARE(
        list->item(1)->text(),
        QStringLiteral("Alpha Major — M1 — Major")
        );
    QCOMPARE(
        list->item(2)->text(),
        QStringLiteral("Zulu Major — M1 — Major")
        );
    QCOMPARE(
        list->item(3)->text(),
        QStringLiteral("Omega M2 — M2 — Major")
        );
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), *betaSongsId);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), *alphaMajorId);
    QCOMPARE(list->item(2)->data(Qt::UserRole).toInt(), *zuluMajorId);
    QCOMPARE(list->item(3)->data(Qt::UserRole).toInt(), *omegaM2Id);
    QCOMPARE(list->currentRow(), 1);
    QCOMPARE(
        list->currentItem()->data(Qt::UserRole).toInt(),
        *alphaMajorId
        );
}

void TestingClassesPageTests::
unavailableTestingClassChoicesQueryIsSilentAndEmpty()
{
    ApplicationServices services;
    FixedTestingClassChoicesReadPort readPort;
    readPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
            .message = "The active database session is unavailable.",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingClassesPage page(&services, nullptr, &readPort);
    page.resize(900, 650);
    page.show();
    page.openTestingClass();
    QApplication::processEvents();

    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    QVERIFY(list);
    QCOMPARE(readPort.callCount, 1);
    QVERIFY(readPort.lastQuery.has_value());
    QVERIFY(*readPort.lastQuery
        == ClassMngr::Next::Application::ScheduleTestingClassChoicesReadQuery{});
    QCOMPARE(list->count(), 0);
    QVERIFY(prompts.messages.isEmpty());
}

void TestingClassesPageTests::
testingClassChoicesQueryFailureShowsExactWarning()
{
    ApplicationServices services;
    FixedTestingClassChoicesReadPort readPort;
    readPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "injected testing class choices read failure",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingClassesPage page(&services, nullptr, &readPort);
    page.resize(900, 650);
    page.show();
    page.openTestingClass();
    QApplication::processEvents();

    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    QVERIFY(list);
    QCOMPARE(readPort.callCount, 1);
    QVERIFY(readPort.lastQuery.has_value());
    QVERIFY(*readPort.lastQuery
        == ClassMngr::Next::Application::ScheduleTestingClassChoicesReadQuery{});
    QCOMPARE(list->count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Testing Classes"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class choices read failure"));
}

void TestingClassesPageTests::
testingTeacherChoicesPopulateOrderedTrimmedChoicesAndRestoreSelection()
{
    ApplicationServices services;
    FixedTestingClassChoicesReadPort classChoicesReadPort;
    classChoicesReadPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::success({});
    FixedTestingTeacherChoicesReadPort teacherChoicesReadPort;
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::success({
            .choices = {
                {
                    .teacherId = typedTeacherId(21),
                    .name = u"  김선생 \t",
                    .room = u"  Room 21  "
                },
                {
                    .teacherId = typedTeacherId(22),
                    .name = u" \t\r\n",
                    .room = u"Room 22"
                },
                {
                    .teacherId = typedTeacherId(23),
                    .name = u"  이선생 ",
                    .room = u" \tRoom 23\t "
                }
            }
        });

    TestingClassesPage page(
        &services,
        nullptr,
        &classChoicesReadPort,
        nullptr,
        &teacherChoicesReadPort
        );
    page.refresh();

    auto* teacherCombo = page.findChild<QComboBox*>(
        QStringLiteral("testingClassTeacherCombo")
        );
    QVERIFY(teacherCombo);
    QCOMPARE(teacherChoicesReadPort.callCount, 1);
    QVERIFY(teacherChoicesReadPort.lastQuery.has_value());
    QVERIFY(*teacherChoicesReadPort.lastQuery
        == ClassMngr::Next::Application::TestingTeacherChoicesReadQuery{});

    // The blank Korean label is omitted, and the remaining choices keep the
    // supplied repository order after the built-in None row.
    QCOMPARE(teacherCombo->count(), 3);
    QCOMPARE(teacherCombo->itemText(0), QStringLiteral("None"));
    QCOMPARE(teacherCombo->itemData(0).toInt(), -1);
    QCOMPARE(teacherCombo->itemText(1), QStringLiteral("김선생"));
    QCOMPARE(teacherCombo->itemData(1).toInt(), 21);
    QCOMPARE(
        teacherCombo->itemData(1, Qt::UserRole + 1).toString(),
        QStringLiteral("Room 21")
        );
    QCOMPARE(teacherCombo->itemText(2), QStringLiteral("이선생"));
    QCOMPARE(teacherCombo->itemData(2).toInt(), 23);
    QCOMPARE(
        teacherCombo->itemData(2, Qt::UserRole + 1).toString(),
        QStringLiteral("Room 23")
        );

    teacherCombo->setCurrentIndex(teacherCombo->findData(23));
    QCOMPARE(teacherCombo->currentData().toInt(), 23);
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::success({
            .choices = {
                {
                    .teacherId = typedTeacherId(23),
                    .name = u"  이선생 ",
                    .room = u" \tRoom 23\t "
                },
                {
                    .teacherId = typedTeacherId(21),
                    .name = u"  김선생 \t",
                    .room = u"  Room 21  "
                },
                {
                    .teacherId = typedTeacherId(22),
                    .name = u" \t\r\n",
                    .room = u"Room 22"
                }
            }
        });
    page.refresh();

    QCOMPARE(teacherChoicesReadPort.callCount, 2);
    QCOMPARE(teacherCombo->count(), 3);
    QCOMPARE(teacherCombo->itemData(1).toInt(), 23);
    QCOMPARE(teacherCombo->currentData().toInt(), 23);
    QCOMPARE(teacherCombo->currentText(), QStringLiteral("이선생"));
}

void TestingClassesPageTests::
unavailableTestingTeacherChoicesQueryIsSilent()
{
    ApplicationServices services;
    FixedTestingClassChoicesReadPort classChoicesReadPort;
    classChoicesReadPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::success({});
    FixedTestingTeacherChoicesReadPort teacherChoicesReadPort;
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
            .message = "The active database session is unavailable.",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingClassesPage page(
        &services,
        nullptr,
        &classChoicesReadPort,
        nullptr,
        &teacherChoicesReadPort
        );
    page.refresh();

    auto* teacherCombo = page.findChild<QComboBox*>(
        QStringLiteral("testingClassTeacherCombo")
        );
    QVERIFY(teacherCombo);
    QCOMPARE(teacherChoicesReadPort.callCount, 1);
    QVERIFY(teacherChoicesReadPort.lastQuery.has_value());
    QVERIFY(*teacherChoicesReadPort.lastQuery
        == ClassMngr::Next::Application::TestingTeacherChoicesReadQuery{});
    QCOMPARE(teacherCombo->count(), 1);
    QCOMPARE(teacherCombo->currentText(), QStringLiteral("None"));
    QCOMPARE(teacherCombo->currentData().toInt(), -1);
    QVERIFY(prompts.messages.isEmpty());
}

void TestingClassesPageTests::
testingTeacherChoicesQueryFailureShowsExactWarning()
{
    ApplicationServices services;
    FixedTestingClassChoicesReadPort classChoicesReadPort;
    classChoicesReadPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::success({});
    FixedTestingTeacherChoicesReadPort teacherChoicesReadPort;
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "injected teacher choices repository detail",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingClassesPage page(
        &services,
        nullptr,
        &classChoicesReadPort,
        nullptr,
        &teacherChoicesReadPort
        );
    page.refresh();

    auto* teacherCombo = page.findChild<QComboBox*>(
        QStringLiteral("testingClassTeacherCombo")
        );
    QVERIFY(teacherCombo);
    QCOMPARE(teacherChoicesReadPort.callCount, 1);
    QCOMPARE(teacherCombo->count(), 1);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Load Teachers"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Teachers could not be loaded.")
        );
    QCOMPARE(
        prompts.messages.constFirst().details,
        QStringLiteral("injected teacher choices repository detail")
        );
}

void TestingClassesPageTests::
testingClassDetailsReadMapsEditorRosterAndCleanState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);

    TestingClass stored = testingClass(
        QStringLiteral("Repository Class Name")
        );
    stored.teacherId = -1;
    stored.grade = QStringLiteral("M1");
    stored.level = QStringLiteral("Major");
    stored.room = QStringLiteral("Repository Room");
    stored.classColor = QStringLiteral("#010203");
    stored.fontColor = QStringLiteral("#A0B0C0");
    stored.notes = QStringLiteral("Stored notes must not replace the query.");
    const auto createdClass = repository->createTestingClass(stored);
    QVERIFY(createdClass);
    RosterRepository* const rosterRepository =
        session->rosterRepository();
    QVERIFY(rosterRepository);
    QVERIFY(rosterRepository->saveRoster(
        *createdClass,
        rosterWithEvaluation()
        ).has_value());
    const auto savedRoster = rosterRepository->loadRoster(*createdClass);
    QVERIFY(savedRoster);
    QCOMPARE(savedRoster->rows.size(), 1);

    const auto parsedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(*createdClass)
            );
    const auto parsedTeacherId =
        ClassMngr::Next::Domain::TeacherId::fromString(
            "7"
            );
    QVERIFY(parsedClassId.has_value());
    QVERIFY(parsedTeacherId.has_value());

    FixedTestingClassDetailsReadPort readPort;
    const ClassMngr::Next::Application::TestingClassDetailsSnapshot expected{
        .classId = *parsedClassId,
        .name = u"Query Class \uC774\uB984",
        .grade = u"M2",
        .level = u"Ursa",
        .room = u"Query Room 509",
        .teacherId = *parsedTeacherId,
        .classColor = u"#123456",
        .fontColor = u"#FEDCBA",
        .notes = u"Query notes \uC790\uB8CC"
    };
    readPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::success(expected);

    FixedTestingClassChoicesReadPort choicesReadPort;
    setSingleTestingClassChoice(
        choicesReadPort,
        *createdClass,
        QStringLiteral("Stored choice")
        );
    FixedTestingTeacherChoicesReadPort teacherChoicesReadPort;
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::success({
            .choices = {{
                .teacherId = *parsedTeacherId,
                .name = u"Teacher 7",
                .room = u"Room 7"
            }}
        });
    TestingClassesPage page(
        &services,
        nullptr,
        &choicesReadPort,
        &readPort,
        &teacherChoicesReadPort
        );
    page.resize(1000, 700);
    page.openTestingClass(*createdClass);
    page.refresh();

    QCOMPARE(readPort.callCount, 1);
    QVERIFY(readPort.lastQuery.has_value());
    QCOMPARE(readPort.lastQuery->classId, *parsedClassId);
    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* gradeCombo = page.findChild<QComboBox*>(
        QStringLiteral("testingClassGradeCombo")
        );
    auto* levelCombo = page.findChild<QComboBox*>(
        QStringLiteral("testingClassLevelCombo")
        );
    auto* roomEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassRoomEdit")
        );
    auto* teacherCombo = page.findChild<QComboBox*>(
        QStringLiteral("testingClassTeacherCombo")
        );
    auto* notesEdit = page.findChild<QTextEdit*>(
        QStringLiteral("testingClassNotesEdit")
        );
    auto* classColorPreview = page.findChild<QWidget*>(
        QStringLiteral("testingClassColorPreview")
        );
    auto* fontColorPreview = page.findChild<QWidget*>(
        QStringLiteral("testingClassFontColorPreview")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
        );
    auto* rosterTable = page.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );

    QVERIFY(list);
    QVERIFY(nameEdit);
    QVERIFY(gradeCombo);
    QVERIFY(levelCombo);
    QVERIFY(roomEdit);
    QVERIFY(teacherCombo);
    QVERIFY(notesEdit);
    QVERIFY(classColorPreview);
    QVERIFY(fontColorPreview);
    QVERIFY(saveButton);
    QVERIFY(rosterTable);
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *createdClass);
    QCOMPARE(nameEdit->text(), QStringLiteral("Query Class \uC774\uB984"));
    QCOMPARE(gradeCombo->currentText(), QStringLiteral("M2"));
    QCOMPARE(levelCombo->currentText(), QStringLiteral("Ursa"));
    QCOMPARE(roomEdit->text(), QStringLiteral("Query Room 509"));
    QCOMPARE(teacherCombo->currentData().toInt(), 7);
    QCOMPARE(notesEdit->toPlainText(), QStringLiteral("Query notes \uC790\uB8CC"));
    QVERIFY(classColorPreview->styleSheet().contains(QStringLiteral("#123456")));
    QVERIFY(fontColorPreview->styleSheet().contains(QStringLiteral("#FEDCBA")));

    const int englishColumn = columnByName(
        rosterTable->model(),
        QStringLiteral("English")
        );
    const int koreanColumn = columnByName(
        rosterTable->model(),
        QStringLiteral("Korean")
        );
    QVERIFY(englishColumn >= 0);
    QVERIFY(koreanColumn >= 0);
    QCOMPARE(
        rosterTable->model()->index(0, englishColumn).data().toString(),
        QStringLiteral("Alex")
        );
    QCOMPARE(
        rosterTable->model()->index(0, koreanColumn).data().toString(),
        QStringLiteral("\uAE40\uD559\uC0DD")
        );
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveButton->isEnabled());

    nameEdit->setText(QStringLiteral("Temporary edit"));
    QVERIFY(page.hasUnsavedChanges());
    page.discardChanges();

    QCOMPARE(readPort.callCount, 2);
    QCOMPARE(nameEdit->text(), QStringLiteral("Query Class \uC774\uB984"));
    QCOMPARE(roomEdit->text(), QStringLiteral("Query Room 509"));
    QCOMPARE(notesEdit->toPlainText(), QStringLiteral("Query notes \uC790\uB8CC"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveButton->isEnabled());
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *createdClass);
}

void TestingClassesPageTests::unavailableTestingClassDetailsReadIsSilent()
{
    ApplicationServices services;
    const auto createdClass = services.dataService()->createTestingClass(
        testingClass(QStringLiteral("Stored Class"))
        );
    QVERIFY(createdClass);

    FixedTestingClassDetailsReadPort readPort;
    readPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
            .message = "The active database session is unavailable.",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    FixedTestingClassChoicesReadPort choicesReadPort;
    setSingleTestingClassChoice(
        choicesReadPort,
        *createdClass,
        QStringLiteral("Stored Class")
        );
    TestingClassesPage page(
        &services,
        nullptr,
        &choicesReadPort,
        &readPort
        );
    page.openTestingClass(*createdClass);
    page.refresh();

    QCOMPARE(readPort.callCount, 1);
    QVERIFY(readPort.lastQuery.has_value());
    QCOMPARE(
        readPort.lastQuery->classId.value(),
        std::to_string(*createdClass)
        );
    QVERIFY(prompts.messages.isEmpty());
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    QVERIFY(nameEdit);
    QVERIFY(nameEdit->text() != QStringLiteral("Stored Class"));
}

void TestingClassesPageTests::missingTestingClassDetailsShowsWarning()
{
    ApplicationServices services;
    const auto createdClass = services.dataService()->createTestingClass(
        testingClass(QStringLiteral("Existing Class"))
        );
    QVERIFY(createdClass);

    FixedTestingClassDetailsReadPort readPort;
    readPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "Testing class details were not found.",
            .recoverable = true
        });
    FixedTestingClassChoicesReadPort choicesReadPort;
    setSingleTestingClassChoice(
        choicesReadPort,
        *createdClass,
        QStringLiteral("Existing Class")
        );

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingClassesPage page(
        &services,
        nullptr,
        &choicesReadPort,
        &readPort
        );
    page.openTestingClass(*createdClass);
    page.refresh();

    QCOMPARE(readPort.callCount, 1);

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Testing Classes"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QVERIFY(prompts.messages.constFirst().message.contains(
        QStringLiteral("not found"),
        Qt::CaseInsensitive
        ));
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    QVERIFY(nameEdit);
    QVERIFY(nameEdit->text() != QStringLiteral("Existing Class"));
}

void TestingClassesPageTests::
testingClassDetailsReadFailureShowsWarningWithoutFallback()
{
    ApplicationServices services;
    const auto createdClass = services.dataService()->createTestingClass(
        testingClass(QStringLiteral("Stored Class"))
        );
    QVERIFY(createdClass);

    const auto parsedClassId =
        ClassMngr::Next::Domain::ClassId::fromString(
            std::to_string(*createdClass)
            );
    QVERIFY(parsedClassId.has_value());

    FixedTestingClassDetailsReadPort readPort;
    readPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "injected testing class details read failure",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    FixedTestingClassChoicesReadPort choicesReadPort;
    setSingleTestingClassChoice(
        choicesReadPort,
        *createdClass,
        QStringLiteral("Stored Class")
        );
    TestingClassesPage page(
        &services,
        nullptr,
        &choicesReadPort,
        &readPort
        );
    page.openTestingClass(*createdClass);
    page.refresh();

    QCOMPARE(readPort.callCount, 1);
    QVERIFY(readPort.lastQuery.has_value());
    QCOMPARE(readPort.lastQuery->classId, *parsedClassId);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Testing Classes"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class details read failure"));
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    QVERIFY(nameEdit);
    QVERIFY(nameEdit->text() != QStringLiteral("Stored Class"));
}

void TestingClassesPageTests::zeroTeacherIdKeepsNoneSelectedWithoutWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    TestingClassRepository* const repository =
        session->testingClassRepository();
    QVERIFY(repository);

    TestingClass stored = testingClass(QStringLiteral("Unassigned Class"));
    stored.teacherId = -1;
    const auto createdClass = repository->createTestingClass(stored);
    QVERIFY(createdClass);

    QSqlQuery addLegacyZeroTeacher(session->database());
    QVERIFY2(addLegacyZeroTeacher.exec(QStringLiteral(
        "INSERT INTO teachers (id, teacher_en) "
        "VALUES (0, 'Legacy Unassigned')"
        )), qPrintable(addLegacyZeroTeacher.lastError().text()));

    QSqlQuery setLegacyZeroTeacherId(session->database());
    setLegacyZeroTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=0 WHERE class_id=?"
        ));
    setLegacyZeroTeacherId.addBindValue(*createdClass);
    QVERIFY2(setLegacyZeroTeacherId.exec(),
             qPrintable(setLegacyZeroTeacherId.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    FixedTestingTeacherChoicesReadPort teacherChoicesReadPort;
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::success({});
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        &teacherChoicesReadPort
        );
    page.openTestingClass(*createdClass);
    page.refresh();

    auto* teacherCombo = page.findChild<QComboBox*>(
        QStringLiteral("testingClassTeacherCombo")
        );
    QVERIFY(teacherCombo);
    QCOMPARE(teacherCombo->currentText(), QStringLiteral("None"));
    QCOMPARE(teacherCombo->currentData().toInt(), -1);
    QVERIFY(prompts.messages.isEmpty());
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
    QVERIFY(services.dataService()->saveRoster(
        *created,
        rosterWithEvaluation()
        ).has_value());

    FixedTestingClassDetailsReadPort detailsReadPort;
    detailsReadPort.result = ClassMngr::Next::Application::
        TestingClassDetailsReadResult::success(testingClassDetails(
            *created,
            QStringLiteral("Writing Lab")
            ));
    TestingClassesPage page(&services, nullptr, nullptr, &detailsReadPort);
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

    const Result<Roster> savedResult =
        services.dataService()->loadRoster(*created);
    QVERIFY(savedResult);
    const Roster& saved = *savedResult;
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
