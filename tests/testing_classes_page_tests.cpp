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
#include "next/application/testing_class_create.h"
#include "next/application/testing_class_delete.h"
#include "next/application/testing_class_details_update.h"
#include "next/application/testing_class_details_read_query.h"
#include "next/application/testing_teacher_choices_read_query.h"
#include "next/platform/application_services_testing_class_create_port.h"
#include "next/platform/application_services_testing_class_delete_port.h"
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
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>
#include <optional>
#include <string>

namespace ScheduleWidgetTestStubs
{
void reset();
void setTestingClass(const TestingClass& testingClass);
int testingClassCount();
bool hasTestingClassNamed(const QString& name);
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

class RecordingTestingClassDetailsUpdatePort final
    : public ClassMngr::Next::Application::TestingClassDetailsUpdatePort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::
        TestingClassDetailsUpdateResult updateTestingClassDetails(
            const ClassMngr::Next::Application::
                TestingClassDetailsUpdateRequest& request
            ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<ClassMngr::Next::Application::
        TestingClassDetailsUpdateRequest> lastRequest;
    ClassMngr::Next::Application::TestingClassDetailsUpdateResult result =
        ClassMngr::Next::Domain::Result<void>::success();
};

class RecordingTestingClassCreatePort final
    : public ClassMngr::Next::Application::TestingClassCreatePort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::TestingClassCreateResult
    createTestingClass(
        const ClassMngr::Next::Application::TestingClassCreateRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;

        ClassMngr::Next::Application::TestingClassCreateResult created =
            failuresRemaining > 0
                ? failureResult
                : delegate
                    ? delegate->createTestingClass(request)
                    : result;
        if (failuresRemaining > 0)
        {
            --failuresRemaining;
        }
        lastResult = created;
        return created;
    }

    const ClassMngr::Next::Application::TestingClassCreatePort* delegate =
        nullptr;
    mutable int callCount = 0;
    mutable int failuresRemaining = 0;
    mutable std::optional<ClassMngr::Next::Application::
        TestingClassCreateRequest> lastRequest;
    mutable std::optional<ClassMngr::Next::Application::
        TestingClassCreateResult> lastResult;
    ClassMngr::Next::Application::TestingClassCreateResult result =
        ClassMngr::Next::Application::TestingClassCreateResult::success(
            typedClassId(77)
            );
    ClassMngr::Next::Application::TestingClassCreateResult failureResult =
        ClassMngr::Next::Application::TestingClassCreateResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Conflict,
            .message = "injected testing class create failure",
            .recoverable = true
        });
};

class RecordingTestingClassDeletePort final
    : public ClassMngr::Next::Application::TestingClassDeletePort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::TestingClassDeleteResult
    deleteTestingClass(
        const ClassMngr::Next::Application::TestingClassDeleteRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        if (delegate)
        {
            return delegate->deleteTestingClass(request);
        }
        return result;
    }

    const ClassMngr::Next::Application::TestingClassDeletePort* delegate =
        nullptr;
    mutable int callCount = 0;
    mutable std::optional<ClassMngr::Next::Application::
        TestingClassDeleteRequest> lastRequest;
    ClassMngr::Next::Application::TestingClassDeleteResult result =
        ClassMngr::Next::Application::TestingClassDeleteResult::success();
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
    void emptySuccessfulTestingTeacherChoicesKeepNoneSelectedWithoutWarning();
    void unavailableTestingTeacherChoicesQueryIsSilent();
    void testingTeacherChoicesQueryFailureShowsExactWarning();
    void testingClassDetailsReadMapsEditorRosterAndCleanState();
    void unavailableTestingClassDetailsReadIsSilent();
    void missingTestingClassDetailsShowsWarning();
    void testingClassDetailsReadFailureShowsWarningWithoutFallback();
    void existingTestingClassUpdateRetainsSelectionAndRefreshesList();
    void testingClassDetailsUpdateFailureRetainsDraftAndShowsWarning();
    void newTestingClassCreationDoesNotUseDetailsUpdatePort();
    void newTestingClassCreateForwardsPendingSlotAndSelectsCreatedClass();
    void newTestingClassCreateFailureRetainsDraftAndPendingSlot();
    void testingClassDeletionWithoutSelectionDoesNothing();
    void testingClassDeletionCancelKeepsDraftAndDoesNotCallPort();
    void testingClassDeletionFailureRetainsDraftAndDoesNotSave();
    void testingClassDeletionSuccessSelectsSiblingAndEmitsOnce();
    void testingClassDeletionSuccessWithNoSiblingStartsNewDraft();
    void rosterFailureBlocksTestingClassDetailsUpdate();
    void savedRosterRemainsCleanWhenDetailsUpdateFails();
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
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::success({
            .choices = {
                {
                    .teacherId = typedTeacherId(21),
                    .name = u"Teacher 21",
                    .room = u"Room 21"
                }
            }
        });
    page.refresh();

    QCOMPARE(teacherChoicesReadPort.callCount, 3);
    QCOMPARE(teacherCombo->count(), 2);
    QCOMPARE(teacherCombo->currentText(), QStringLiteral("None"));
    QCOMPARE(teacherCombo->currentData().toInt(), -1);
}

void TestingClassesPageTests::
emptySuccessfulTestingTeacherChoicesKeepNoneSelectedWithoutWarning()
{
    ApplicationServices services;
    FixedTestingClassChoicesReadPort classChoicesReadPort;
    classChoicesReadPort.result = ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult::success({});
    FixedTestingTeacherChoicesReadPort teacherChoicesReadPort;
    teacherChoicesReadPort.result = ClassMngr::Next::Application::
        TestingTeacherChoicesReadResult::success({});
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
    QCOMPARE(teacherCombo->currentText(), QStringLiteral("None"));
    QCOMPARE(teacherCombo->currentData().toInt(), -1);
    QVERIFY(prompts.messages.isEmpty());
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
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    // Keep the old source populated so an accidental fallback would add rows.
    const auto legacyTeachers = services.teacherService()->teachers();
    QVERIFY(legacyTeachers);
    QVERIFY(!legacyTeachers->isEmpty());

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
    QCOMPARE(teacherCombo->currentText(), QStringLiteral("None"));
    QCOMPARE(teacherCombo->currentData().toInt(), -1);
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

void TestingClassesPageTests::
existingTestingClassUpdateRetainsSelectionAndRefreshesList()
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

    TestingClass target = testingClass(QStringLiteral("Alpha Target"));
    target.teacherId = -1;
    target.grade = QStringLiteral("M1");
    target.level = QStringLiteral("Major");
    const auto targetId = repository->createTestingClass(target);
    QVERIFY(targetId);

    TestingClass other = testingClass(QStringLiteral("Beta Stable"));
    other.teacherId = -1;
    other.grade = QStringLiteral("M1");
    other.level = QStringLiteral("Major");
    const auto otherId = repository->createTestingClass(other);
    QVERIFY(otherId);

    TestingClassesPage page(&services);
    page.openTestingClass(*targetId);
    page.refresh();

    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
        );
    QVERIFY(list);
    QVERIFY(nameEdit);
    QVERIFY(saveButton);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *targetId);
    QCOMPARE(list->currentRow(), 0);

    nameEdit->setText(QStringLiteral("Zulu Updated Target"));
    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy changedSpy(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changedSpy.isValid());

    QVERIFY(page.saveChanges());

    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), *otherId);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), *targetId);
    QCOMPARE(list->currentRow(), 1);
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *targetId);
    QVERIFY(list->item(1)->text().contains(QStringLiteral("Zulu Updated Target")));
    QCOMPARE(nameEdit->text(), QStringLiteral("Zulu Updated Target"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveButton->isEnabled());

    const auto saved = repository->loadTestingClass(*targetId);
    QVERIFY(saved);
    QCOMPARE(saved->name, QStringLiteral("Zulu Updated Target"));
}

void TestingClassesPageTests::
testingClassDetailsUpdateFailureRetainsDraftAndShowsWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass stored = testingClass(QStringLiteral("Stored Class"));
    stored.teacherId = -1;
    const auto classId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(stored);
    QVERIFY(classId);

    RecordingTestingClassDetailsUpdatePort updatePort;
    updatePort.result =
        ClassMngr::Next::Application::TestingClassDetailsUpdateResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "injected testing class details update failure",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort
        );
    page.openTestingClass(*classId);
    page.refresh();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
        );
    QVERIFY(nameEdit);
    QVERIFY(saveButton);

    nameEdit->setText(QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!page.saveChanges());

    QCOMPARE(updatePort.callCount, 1);
    QVERIFY(updatePort.lastRequest.has_value());
    QCOMPARE(updatePort.lastRequest->classId.value(), std::to_string(*classId));
    QCOMPARE(updatePort.lastRequest->name, std::u16string(u"Draft Must Remain"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Save Testing Class"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class details update failure"));

    const auto unchanged = services.databaseSession()
        ->testingClassRepository()->loadTestingClass(*classId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->name, QStringLiteral("Stored Class"));
}

void TestingClassesPageTests::
newTestingClassCreationDoesNotUseDetailsUpdatePort()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass existing = testingClass(QStringLiteral("Existing Class"));
    existing.teacherId = -1;
    const auto existingClassId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(existing);
    QVERIFY(existingClassId);
    QCOMPARE(ScheduleWidgetTestStubs::testingClassCount(), 0);

    RecordingTestingClassDetailsUpdatePort updatePort;
    updatePort.result =
        ClassMngr::Next::Application::TestingClassDetailsUpdateResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "update port must not handle class creation",
            .recoverable = false
        });
    ClassMngr::Next::Platform::ApplicationServicesTestingClassCreatePort
        persistedCreatePort(services);
    RecordingTestingClassCreatePort createPort;
    createPort.delegate = &persistedCreatePort;

    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort,
        &createPort
        );
    page.openTestingClass(*existingClassId);
    page.refresh();

    auto* addButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesAddButton")
        );
    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    QVERIFY(addButton);
    QVERIFY(list);
    addButton->click();

    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* editor = page.findChild<RosterEditorWidget*>();
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
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
    QVERIFY(nameEdit);
    QVERIFY(editor);
    QVERIFY(saveButton);
    QVERIFY(gradeCombo);
    QVERIFY(levelCombo);
    QVERIFY(roomEdit);
    nameEdit->setText(QStringLiteral("Created Through Existing Path"));
    gradeCombo->setCurrentText(QStringLiteral("M1"));
    levelCombo->setCurrentText(QStringLiteral("Major"));
    roomEdit->setText(QStringLiteral("Room F146"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(!editor->hasUnsavedChanges());
    QSignalSpy changedSpy(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changedSpy.isValid());
    QVERIFY(page.saveChanges());

    QCOMPARE(updatePort.callCount, 0);
    QCOMPARE(createPort.callCount, 1);
    QVERIFY(createPort.lastRequest.has_value());
    QVERIFY(createPort.lastResult.has_value());
    QVERIFY(*createPort.lastResult);
    QVERIFY(!createPort.lastRequest->assignmentDay.has_value());
    QVERIFY(!createPort.lastRequest->assignmentStartTime.has_value());
    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(ScheduleWidgetTestStubs::testingClassCount(), 0);
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveButton->isEnabled());

    const int createdClassId = std::stoi(
        createPort.lastResult->value().value()
        );
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), createdClassId);
    QVERIFY(list->currentItem()->text().contains(
        QStringLiteral("Created Through Existing Path")
        ));
    const auto saved = services.databaseSession()
        ->testingClassRepository()->loadTestingClass(createdClassId);
    QVERIFY(saved);
    QCOMPARE(saved->name, QStringLiteral("Created Through Existing Path"));
    QCOMPARE(services.databaseSession()->database().tables().contains(
        QStringLiteral("schedule_testing_blocks")
        ), true);

    QSqlQuery assignmentCount(services.databaseSession()->database());
    assignmentCount.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM schedule_testing_blocks WHERE class_id=?"
        ));
    assignmentCount.addBindValue(createdClassId);
    QVERIFY(assignmentCount.exec());
    QVERIFY(assignmentCount.next());
    QCOMPARE(assignmentCount.value(0).toInt(), 0);
}

void TestingClassesPageTests::
newTestingClassCreateForwardsPendingSlotAndSelectsCreatedClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass existing = testingClass(QStringLiteral("Existing Class"));
    existing.teacherId = -1;
    const auto existingClassId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(existing);
    QVERIFY(existingClassId);

    ClassMngr::Next::Platform::ApplicationServicesTestingClassCreatePort
        persistedCreatePort(services);
    RecordingTestingClassCreatePort createPort;
    createPort.delegate = &persistedCreatePort;
    RecordingTestingClassDetailsUpdatePort updatePort;
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort,
        &createPort
        );
    page.openTestingClass(-1, QStringLiteral("tuesday"), QStringLiteral("17:00"));
    page.refresh();

    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
        );
    auto* addButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesAddButton")
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
    QVERIFY(list);
    QVERIFY(nameEdit);
    QVERIFY(saveButton);
    QVERIFY(addButton);
    QVERIFY(gradeCombo);
    QVERIFY(levelCombo);
    QVERIFY(roomEdit);
    QCOMPARE(list->count(), 1);
    QVERIFY(!list->currentItem());

    nameEdit->setText(QStringLiteral("Created With Pending Slot"));
    gradeCombo->setCurrentText(QStringLiteral("M1"));
    levelCombo->setCurrentText(QStringLiteral("Major"));
    roomEdit->setText(QStringLiteral("Room F146"));
    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy changedSpy(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changedSpy.isValid());
    QVERIFY(page.saveChanges());

    QCOMPARE(updatePort.callCount, 0);
    QCOMPARE(createPort.callCount, 1);
    QVERIFY(createPort.lastRequest.has_value());
    QVERIFY(createPort.lastRequest->assignmentDay.has_value());
    QVERIFY(createPort.lastRequest->assignmentStartTime.has_value());
    QCOMPARE(*createPort.lastRequest->assignmentDay, std::u16string(u"tuesday"));
    QCOMPARE(*createPort.lastRequest->assignmentStartTime, std::u16string(u"17:00"));
    QVERIFY(createPort.lastResult.has_value());
    QVERIFY(*createPort.lastResult);
    const int createdClassId = std::stoi(
        createPort.lastResult->value().value()
        );
    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(list->count(), 2);
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), createdClassId);
    QVERIFY(list->currentItem()->text().contains(
        QStringLiteral("Created With Pending Slot")
        ));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!saveButton->isEnabled());

    const auto saved = services.databaseSession()
        ->testingClassRepository()->loadTestingClass(createdClassId);
    QVERIFY(saved);
    QCOMPARE(saved->name, QStringLiteral("Created With Pending Slot"));
    QSqlQuery assignment(services.databaseSession()->database());
    assignment.prepare(QStringLiteral(
        "SELECT day, start_time FROM schedule_testing_blocks WHERE class_id=?"
        ));
    assignment.addBindValue(createdClassId);
    QVERIFY(assignment.exec());
    QVERIFY(assignment.next());
    QCOMPARE(assignment.value(0).toString(), QStringLiteral("Tuesday"));
    QCOMPARE(assignment.value(1).toString(), QStringLiteral("17:00"));
    QVERIFY(!assignment.next());

    addButton->click();
    nameEdit->setText(QStringLiteral("Created After Pending Slot"));
    gradeCombo->setCurrentText(QStringLiteral("M1"));
    levelCombo->setCurrentText(QStringLiteral("Major"));
    roomEdit->setText(QStringLiteral("Room F147"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(createPort.callCount, 2);
    QVERIFY(createPort.lastRequest.has_value());
    QVERIFY(!createPort.lastRequest->assignmentDay.has_value());
    QVERIFY(!createPort.lastRequest->assignmentStartTime.has_value());
}

void TestingClassesPageTests::
newTestingClassCreateFailureRetainsDraftAndPendingSlot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    ClassMngr::Next::Platform::ApplicationServicesTestingClassCreatePort
        persistedCreatePort(services);
    RecordingTestingClassCreatePort createPort;
    createPort.delegate = &persistedCreatePort;
    createPort.failuresRemaining = 1;
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &createPort
        );
    page.openTestingClass(-1, QStringLiteral("Wednesday"), QStringLiteral("18:00"));
    page.refresh();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesSaveButton")
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
    QVERIFY(nameEdit);
    QVERIFY(saveButton);
    QVERIFY(gradeCombo);
    QVERIFY(levelCombo);
    QVERIFY(roomEdit);
    nameEdit->setText(QStringLiteral("Draft After Create Failure"));
    gradeCombo->setCurrentText(QStringLiteral("M1"));
    levelCombo->setCurrentText(QStringLiteral("Major"));
    roomEdit->setText(QStringLiteral("Room F146"));
    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy changedSpy(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changedSpy.isValid());

    QVERIFY(!page.saveChanges());

    QCOMPARE(createPort.callCount, 1);
    QVERIFY(createPort.lastRequest.has_value());
    QVERIFY(createPort.lastRequest->assignmentDay.has_value());
    QVERIFY(createPort.lastRequest->assignmentStartTime.has_value());
    QCOMPARE(*createPort.lastRequest->assignmentDay, std::u16string(u"Wednesday"));
    QCOMPARE(*createPort.lastRequest->assignmentStartTime, std::u16string(u"18:00"));
    QCOMPARE(nameEdit->text(), QStringLiteral("Draft After Create Failure"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(saveButton->isEnabled());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Save Testing Class"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class create failure"));
    QCOMPARE(changedSpy.size(), 0);
    QCOMPARE(ScheduleWidgetTestStubs::testingClassCount(), 0);

    QVERIFY(page.saveChanges());
    QCOMPARE(createPort.callCount, 2);
    QVERIFY(createPort.lastRequest.has_value());
    QVERIFY(createPort.lastRequest->assignmentDay.has_value());
    QVERIFY(createPort.lastRequest->assignmentStartTime.has_value());
    QCOMPARE(*createPort.lastRequest->assignmentDay, std::u16string(u"Wednesday"));
    QCOMPARE(*createPort.lastRequest->assignmentStartTime, std::u16string(u"18:00"));
    QVERIFY(createPort.lastResult.has_value());
    QVERIFY(*createPort.lastResult);
    QCOMPARE(changedSpy.size(), 1);
    QVERIFY(!page.hasUnsavedChanges());
}

void TestingClassesPageTests::
testingClassDeletionWithoutSelectionDoesNothing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RecordingTestingClassDeletePort deletePort;

    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &deletePort
        );
    page.refresh();
    auto* deleteButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesDeleteButton")
        );
    QVERIFY(deleteButton);
    QVERIFY(!deleteButton->isEnabled());

    deleteButton->click();

    QCOMPARE(prompts.confirmations.size(), 0);
    QCOMPARE(deletePort.callCount, 0);
}

void TestingClassesPageTests::
testingClassDeletionCancelKeepsDraftAndDoesNotCallPort()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass stored = testingClass(QStringLiteral("Delete Target"));
    stored.teacherId = -1;
    const auto classId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(stored);
    QVERIFY(classId);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RecordingTestingClassDetailsUpdatePort updatePort;
    RecordingTestingClassDeletePort deletePort;
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort,
        nullptr,
        &deletePort
        );
    page.openTestingClass(*classId);
    page.refresh();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* deleteButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesDeleteButton")
        );
    QVERIFY(nameEdit);
    QVERIFY(list);
    QVERIFY(deleteButton);
    nameEdit->setText(QStringLiteral("Uncommitted Draft"));
    QVERIFY(page.hasUnsavedChanges());

    deleteButton->click();

    QCOMPARE(prompts.confirmations.size(), 1);
    const PromptRequest& confirmation = prompts.confirmations.constFirst();
    QCOMPARE(confirmation.title, QStringLiteral("Delete Testing Class?"));
    QCOMPARE(
        confirmation.message,
        QStringLiteral(
            "This permanently deletes the testing class, its roster, notes, "
            "speaking evaluations, regular and intensive class times, and "
            "every schedule assignment."
            )
        );
    QCOMPARE(confirmation.acceptText, QStringLiteral("Delete"));
    QCOMPARE(confirmation.rejectText, QStringLiteral("Cancel"));
    QVERIFY(confirmation.destructive);
    QCOMPARE(deletePort.callCount, 0);
    QCOMPARE(updatePort.callCount, 0);
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *classId);
    QCOMPARE(nameEdit->text(), QStringLiteral("Uncommitted Draft"));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(prompts.messages.isEmpty());
    const auto stillStored = services.databaseSession()
        ->testingClassRepository()->loadTestingClass(*classId);
    QVERIFY(stillStored);
    QCOMPARE(stillStored->name, QStringLiteral("Delete Target"));
}

void TestingClassesPageTests::
testingClassDeletionFailureRetainsDraftAndDoesNotSave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass stored = testingClass(QStringLiteral("Delete Target"));
    stored.teacherId = -1;
    const auto classId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(stored);
    QVERIFY(classId);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RecordingTestingClassDetailsUpdatePort updatePort;
    RecordingTestingClassDeletePort deletePort;
    deletePort.result =
        ClassMngr::Next::Application::TestingClassDeleteResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "injected testing class delete failure",
            .recoverable = false
        });
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort,
        nullptr,
        &deletePort
        );
    page.openTestingClass(*classId);
    page.refresh();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* deleteButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesDeleteButton")
        );
    QVERIFY(nameEdit);
    QVERIFY(list);
    QVERIFY(deleteButton);
    nameEdit->setText(QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy changedSpy(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changedSpy.isValid());

    deleteButton->click();

    QCOMPARE(prompts.confirmations.size(), 1);
    const PromptRequest& confirmation = prompts.confirmations.constFirst();
    QCOMPARE(confirmation.title, QStringLiteral("Delete Testing Class?"));
    QCOMPARE(confirmation.acceptText, QStringLiteral("Delete"));
    QCOMPARE(confirmation.rejectText, QStringLiteral("Cancel"));
    QVERIFY(confirmation.destructive);
    QVERIFY(confirmation.message.contains(QStringLiteral("speaking evaluations")));
    QVERIFY(confirmation.message.contains(QStringLiteral("regular and intensive class times")));
    QVERIFY(confirmation.message.contains(QStringLiteral("every schedule assignment")));
    QCOMPARE(deletePort.callCount, 1);
    QVERIFY(deletePort.lastRequest.has_value());
    QCOMPARE(
        deletePort.lastRequest->classId.value(),
        std::to_string(*classId)
        );
    QCOMPARE(updatePort.callCount, 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Delete Testing Class"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class delete failure"));
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *classId);
    QCOMPARE(nameEdit->text(), QStringLiteral("Draft Must Remain"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(changedSpy.size(), 0);
    const auto unchanged = services.databaseSession()
        ->testingClassRepository()->loadTestingClass(*classId);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->name, QStringLiteral("Delete Target"));
}

void TestingClassesPageTests::
testingClassDeletionSuccessSelectsSiblingAndEmitsOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass target = testingClass(QStringLiteral("Alpha Delete Target"));
    target.teacherId = -1;
    const auto targetId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(target);
    QVERIFY(targetId);
    RosterRepository* const rosterRepository =
        services.databaseSession()->rosterRepository();
    QVERIFY(rosterRepository);
    QVERIFY(rosterRepository->saveRoster(
        *targetId,
        rosterWithEvaluation()
        ).has_value());
    TestingClass sibling = testingClass(QStringLiteral("Beta Sibling"));
    sibling.teacherId = -1;
    const auto siblingId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(sibling);
    QVERIFY(siblingId);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RecordingTestingClassDetailsUpdatePort updatePort;
    RecordingTestingClassCreatePort createPort;
    ClassMngr::Next::Platform::ApplicationServicesTestingClassDeletePort
        persistedDeletePort(services);
    RecordingTestingClassDeletePort deletePort;
    deletePort.delegate = &persistedDeletePort;
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort,
        &createPort,
        &deletePort
        );
    page.openTestingClass(*targetId);
    page.refresh();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* deleteButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesDeleteButton")
        );
    auto* rosterEditor = page.findChild<RosterEditorWidget*>();
    auto* rosterTable = page.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(nameEdit);
    QVERIFY(list);
    QVERIFY(deleteButton);
    QVERIFY(rosterEditor);
    QVERIFY(rosterTable);
    nameEdit->setText(QStringLiteral("Draft Is Not Saved Before Delete"));
    const int englishColumn = columnByName(
        rosterTable->model(),
        QStringLiteral("English")
        );
    QVERIFY(englishColumn >= 0);
    QVERIFY(rosterTable->model()->setData(
        rosterTable->model()->index(0, englishColumn),
        QStringLiteral("Unsaved Roster Draft"),
        Qt::EditRole
        ));
    QVERIFY(rosterEditor->hasUnsavedChanges());
    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy changedSpy(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changedSpy.isValid());

    deleteButton->click();

    QCOMPARE(deletePort.callCount, 1);
    QVERIFY(deletePort.lastRequest.has_value());
    QCOMPARE(deletePort.lastRequest->classId.value(), std::to_string(*targetId));
    QCOMPARE(updatePort.callCount, 0);
    QCOMPARE(createPort.callCount, 0);
    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->currentRow(), 0);
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *siblingId);
    QVERIFY(list->currentItem()->text().contains(QStringLiteral("Beta Sibling")));
    QCOMPARE(nameEdit->text(), QStringLiteral("Beta Sibling"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!rosterEditor->hasUnsavedChanges());
    const auto deleted = services.databaseSession()
        ->testingClassRepository()->isTestingClass(*targetId);
    QVERIFY(deleted);
    QVERIFY(!*deleted);
    const auto preserved = services.databaseSession()
        ->testingClassRepository()->loadTestingClass(*siblingId);
    QVERIFY(preserved);
    QCOMPARE(preserved->name, QStringLiteral("Beta Sibling"));
}

void TestingClassesPageTests::
testingClassDeletionSuccessWithNoSiblingStartsNewDraft()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass target = testingClass(QStringLiteral("Only Testing Class"));
    target.teacherId = -1;
    const auto targetId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(target);
    QVERIFY(targetId);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    ClassMngr::Next::Platform::ApplicationServicesTestingClassDeletePort
        persistedDeletePort(services);
    RecordingTestingClassDeletePort deletePort;
    deletePort.delegate = &persistedDeletePort;
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &deletePort
        );
    page.openTestingClass(*targetId);
    page.refresh();
    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* deleteButton = page.findChild<QPushButton*>(
        QStringLiteral("testingClassesDeleteButton")
        );
    QVERIFY(nameEdit);
    QVERIFY(list);
    QVERIFY(deleteButton);
    QSignalSpy changedSpy(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changedSpy.isValid());

    deleteButton->click();

    QCOMPARE(deletePort.callCount, 1);
    QCOMPARE(changedSpy.size(), 1);
    QCOMPARE(list->count(), 0);
    QVERIFY(!list->currentItem());
    QCOMPARE(nameEdit->text(), QStringLiteral("Testing Class"));
    QVERIFY(!page.hasUnsavedChanges());
}

void TestingClassesPageTests::
rosterFailureBlocksTestingClassDetailsUpdate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass stored = testingClass(QStringLiteral("Stored Class"));
    stored.teacherId = -1;
    const auto classId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(stored);
    QVERIFY(classId);
    RosterRepository* const rosterRepository =
        services.databaseSession()->rosterRepository();
    QVERIFY(rosterRepository);
    QVERIFY(rosterRepository->saveRoster(
        *classId,
        rosterWithEvaluation()
        ).has_value());

    RecordingTestingClassDetailsUpdatePort updatePort;
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort
        );
    page.openTestingClass(*classId);
    page.refresh();

    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* editor = page.findChild<RosterEditorWidget*>();
    auto* table = page.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(nameEdit);
    QVERIFY(editor);
    QVERIFY(table);
    const int englishColumn = columnByName(
        table->model(),
        QStringLiteral("English")
        );
    QVERIFY(englishColumn >= 0);
    QVERIFY(table->model()->setData(
        table->model()->index(0, englishColumn),
        QStringLiteral("Roster Draft"),
        Qt::EditRole
        ));
    nameEdit->setText(QStringLiteral("Metadata Draft"));
    QVERIFY(editor->hasUnsavedChanges());
    QVERIFY(page.hasUnsavedChanges());

    services.closeDatabase();
    QVERIFY(!page.saveChanges());

    QCOMPARE(updatePort.callCount, 0);
    QVERIFY(editor->hasUnsavedChanges());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(nameEdit->text(), QStringLiteral("Metadata Draft"));
    QVERIFY(!prompts.messages.isEmpty());
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Save Roster"));
}

void TestingClassesPageTests::savedRosterRemainsCleanWhenDetailsUpdateFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    TestingClass stored = testingClass(QStringLiteral("Stored Class"));
    stored.teacherId = -1;
    const auto classId = services.databaseSession()
        ->testingClassRepository()->createTestingClass(stored);
    QVERIFY(classId);
    RosterRepository* const rosterRepository =
        services.databaseSession()->rosterRepository();
    QVERIFY(rosterRepository);
    QVERIFY(rosterRepository->saveRoster(
        *classId,
        rosterWithEvaluation()
        ).has_value());

    RecordingTestingClassDetailsUpdatePort updatePort;
    updatePort.result =
        ClassMngr::Next::Application::TestingClassDetailsUpdateResult::failure({
            .code = ClassMngr::Next::Domain::ErrorCode::Technical,
            .message = "injected details failure after roster save",
            .recoverable = false
        });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TestingClassesPage page(
        &services,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &updatePort
        );
    page.openTestingClass(*classId);
    page.refresh();

    auto* nameEdit = page.findChild<QLineEdit*>(
        QStringLiteral("testingClassNameEdit")
        );
    auto* editor = page.findChild<RosterEditorWidget*>();
    auto* table = page.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(nameEdit);
    QVERIFY(editor);
    QVERIFY(table);
    const int englishColumn = columnByName(
        table->model(),
        QStringLiteral("English")
        );
    QVERIFY(englishColumn >= 0);
    QVERIFY(table->model()->setData(
        table->model()->index(0, englishColumn),
        QStringLiteral("Roster Edit"),
        Qt::EditRole
        ));
    const int koreanColumn = columnByName(
        table->model(),
        QStringLiteral("Korean")
        );
    QVERIFY(koreanColumn >= 0);
    QVERIFY(table->model()->setData(
        table->model()->index(0, koreanColumn),
        QStringLiteral("\uBC15\uD559\uC0DD"),
        Qt::EditRole
        ));
    nameEdit->setText(QStringLiteral("Retained Metadata Draft"));
    QVERIFY(editor->hasUnsavedChanges());

    QVERIFY(!page.saveChanges());

    QCOMPARE(updatePort.callCount, 1);
    QVERIFY(updatePort.lastRequest.has_value());
    QCOMPARE(updatePort.lastRequest->name,
             std::u16string(u"Retained Metadata Draft"));
    QVERIFY(!editor->hasUnsavedChanges());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(nameEdit->text(), QStringLiteral("Retained Metadata Draft"));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Save Testing Class"));
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected details failure after roster save"));

    const auto savedRoster = rosterRepository->loadRoster(*classId);
    QVERIFY(savedRoster);
    const int savedEnglishColumn = savedRoster->columns.indexOf(
        QStringLiteral("English")
        );
    QVERIFY(savedEnglishColumn >= 0);
    QCOMPARE(savedRoster->rows.value(0).value(savedEnglishColumn),
             QStringLiteral("Roster Edit"));
    const auto unchangedClass = services.databaseSession()
        ->testingClassRepository()->loadTestingClass(*classId);
    QVERIFY(unchangedClass);
    QCOMPARE(unchangedClass->name, QStringLiteral("Stored Class"));
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
