#include "app/controllers/sidebar_controller.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_repository.h"
#include "features/classes/ui/class_notes_page.h"
#include "features/classes/ui/classes_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

#include <memory>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sidebar-class-delete-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

QString jsonBool(const bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

class DeleteFixture final
{
public:
    [[nodiscard]] bool open()
    {
        if (!services.openDatabase(databasePath(directory)))
        {
            return false;
        }

        ClassRepository* const repository =
            services.databaseSession()->classRepository();
        if (!repository)
        {
            return false;
        }
        const auto target = repository->createClass(
            QStringLiteral("F362 Delete Target")
            );
        const auto sibling = repository->createClass(
            QStringLiteral("F362 Keep Sibling")
            );
        if (!target || !sibling)
        {
            return false;
        }
        targetId = *target;
        siblingId = *sibling;
        if (!insertPageRows(targetId, QStringLiteral("Monday"))
            || !insertPageRows(siblingId, QStringLiteral("Tuesday")))
        {
            return false;
        }

        pages.initialize(&services, false);
        pages.setDatabaseOpen(true);
        pages.showPage(PageType::Classes);
        page = pages.classesPage();
        if (!page || !page->openClass(targetId))
        {
            return false;
        }

        sidebar.selectByKeys({QStringLiteral("my_workspace")});
        controller = std::make_unique<SidebarController>(
            &services,
            &sidebar,
            &pages
            );
        DialogServices::setUserPromptServiceForTesting(&prompts);
        return true;
    }

    [[nodiscard]] bool insertPageRows(
        const int classId,
        const QString& day
        ) const
    {
        QSqlDatabase database = services.databaseSession()->database();
        QSqlQuery info(database);
        info.prepare(QStringLiteral(
            "INSERT INTO class_info (class_id, class_grade, class_level) "
            "VALUES (?, 'E4', 'F362 Parity')"
            ));
        info.addBindValue(classId);
        if (!info.exec())
        {
            return false;
        }

        QSqlQuery schedule(database);
        schedule.prepare(QStringLiteral(
            "INSERT INTO class_times "
            "(class_id, day, start_time, end_time) VALUES (?, ?, '9:00 AM', '10:00 AM')"
            ));
        schedule.addBindValue(classId);
        schedule.addBindValue(day);
        return schedule.exec();
    }

    [[nodiscard]] ClassRepository* repository() const
    {
        return services.databaseSession()->classRepository();
    }

    [[nodiscard]] bool invokeDeleteSelectingTarget()
    {
        bool dialogFound = false;
        bool selected = false;
        bool accepted = false;
        QTimer::singleShot(
            0,
            controller.get(),
            [this, &dialogFound, &selected, &accepted]
            {
                QDialog* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget()
                    );
                if (!dialog)
                {
                    for (QWidget* widget : QApplication::topLevelWidgets())
                    {
                        auto* const candidate = qobject_cast<QDialog*>(widget);
                        if (candidate
                            && candidate->objectName()
                                == QStringLiteral(
                                    "sidebarRecordSelectionDialog"
                                    ))
                        {
                            dialog = candidate;
                            break;
                        }
                    }
                }
                if (!dialog)
                {
                    return;
                }

                dialogFound = true;
                auto* const combo = dialog->findChild<QComboBox*>(
                    QStringLiteral("sidebarRecordSelectionCombo")
                    );
                auto* const buttons = dialog->findChild<QDialogButtonBox*>(
                    QStringLiteral("sidebarRecordSelectionButtonBox")
                    );
                auto* const acceptButton = buttons
                    ? buttons->button(QDialogButtonBox::Ok)
                    : nullptr;
                if (!combo || !acceptButton)
                {
                    dialog->reject();
                    return;
                }

                const int selectedIndex = combo->findData(targetId);
                if (selectedIndex < 0)
                {
                    dialog->reject();
                    return;
                }

                combo->setCurrentIndex(selectedIndex);
                selected = true;
                if (!acceptButton->isEnabled())
                {
                    dialog->reject();
                    return;
                }
                acceptButton->click();
                accepted = true;
            }
            );

        const bool invoked = QMetaObject::invokeMethod(
            controller.get(),
            "deleteClass",
            Qt::DirectConnection
            );
        QCoreApplication::processEvents();
        return invoked && dialogFound && selected && accepted;
    }

    QTemporaryDir directory;
    ApplicationServices services;
    PageManager pages;
    Sidebar sidebar;
    FakeUserPromptService prompts;
    std::unique_ptr<SidebarController> controller;
    ClassesPage* page = nullptr;
    int targetId = -1;
    int siblingId = -1;
};

int classCount(ClassRepository* repository)
{
    if (!repository)
    {
        return -1;
    }
    const auto classes = repository->getClasses();
    return classes ? classes->size() : -1;
}

void emitTranscript(const QString& transcript)
{
    qInfo().noquote() << QStringLiteral("F362_TRANSCRIPT=") + transcript;
}

}

class SidebarClassDeleteParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void confirmationCancelHasNoSideEffects();
    void leaveGuardCancelPreservesDirtyPage();
    void writeFailureWarnsWithoutRefreshingOrNavigating();
    void successRefreshesPageAndSelectsClasses();
};

void SidebarClassDeleteParityTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void SidebarClassDeleteParityTests::confirmationCancelHasNoSideEffects()
{
    DeleteFixture fixture;
    QVERIFY(fixture.open());
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    const QStringList selectionBefore = fixture.sidebar.selectedKeys();
    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Rejected);

    QVERIFY(fixture.invokeDeleteSelectingTarget());

    const int queryDelta =
        fixture.page->runtimeMetrics().classQueryCount - queriesBefore;
    const QStringList selectionAfter = fixture.sidebar.selectedKeys();
    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(fixture.prompts.messages.isEmpty());
    QCOMPARE(classCount(fixture.repository()), 2);
    QCOMPARE(fixture.page->currentClassId(), fixture.targetId);
    QCOMPARE(queryDelta, 0);
    QCOMPARE(selectionAfter, selectionBefore);

    emitTranscript(QStringLiteral(
        "{\"case\":\"confirmation_cancel\",\"confirmations\":1,"
        "\"leave_prompts\":0,\"class_count\":2,\"warnings\":0,"
        "\"page_query_delta\":0,\"sidebar_unchanged\":%1}"
        ).arg(jsonBool(selectionAfter == selectionBefore)));
}

void SidebarClassDeleteParityTests::leaveGuardCancelPreservesDirtyPage()
{
    DeleteFixture fixture;
    QVERIFY(fixture.open());
    QVERIFY(fixture.page->openClass(fixture.targetId, ClassesSection::Notes));
    const auto editors = fixture.page->findChildren<QTextEdit*>();
    QVERIFY(!editors.isEmpty());
    editors.first()->setPlainText(QStringLiteral("Unsaved F362 content"));
    QVERIFY(fixture.page->hasUnsavedChanges());
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    const QStringList selectionBefore = fixture.sidebar.selectedKeys();
    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    fixture.prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );

    QVERIFY(fixture.invokeDeleteSelectingTarget());

    const int queryDelta =
        fixture.page->runtimeMetrics().classQueryCount - queriesBefore;
    const QStringList selectionAfter = fixture.sidebar.selectedKeys();
    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QCOMPARE(fixture.prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(fixture.prompts.messages.isEmpty());
    QCOMPARE(classCount(fixture.repository()), 2);
    QVERIFY(fixture.page->hasUnsavedChanges());
    QCOMPARE(fixture.page->currentClassId(), fixture.targetId);
    QCOMPARE(queryDelta, 0);
    QCOMPARE(selectionAfter, selectionBefore);

    emitTranscript(QStringLiteral(
        "{\"case\":\"leave_guard_cancel\",\"confirmations\":1,"
        "\"leave_prompts\":1,\"class_count\":2,\"warnings\":0,"
        "\"page_dirty\":%1,\"page_query_delta\":0,"
        "\"sidebar_unchanged\":%2}"
        ).arg(jsonBool(fixture.page->hasUnsavedChanges()),
              jsonBool(selectionAfter == selectionBefore)));
}

void SidebarClassDeleteParityTests::
writeFailureWarnsWithoutRefreshingOrNavigating()
{
    DeleteFixture fixture;
    QVERIFY(fixture.open());
    QSqlQuery trigger(fixture.services.databaseSession()->database());
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_sidebar_class_delete "
        "BEFORE DELETE ON classes WHEN OLD.id=%1 "
        "BEGIN SELECT RAISE(ABORT, 'F362 sidebar delete failure'); END"
        ).arg(fixture.targetId)));

    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    const QStringList selectionBefore = fixture.sidebar.selectedKeys();
    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Destructive);

    QVERIFY(fixture.invokeDeleteSelectingTarget());

    const int queryDelta =
        fixture.page->runtimeMetrics().classQueryCount - queriesBefore;
    const QStringList selectionAfter = fixture.sidebar.selectedKeys();
    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QCOMPARE(fixture.prompts.messages.size(), 1);
    const PromptRequest& warning = fixture.prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Delete Class"));
    QCOMPARE(warning.message, QStringLiteral("The class could not be deleted."));
    QVERIFY(!warning.details.trimmed().isEmpty());
    QCOMPARE(classCount(fixture.repository()), 2);
    QCOMPARE(fixture.page->currentClassId(), fixture.targetId);
    QCOMPARE(queryDelta, 0);
    QCOMPARE(selectionAfter, selectionBefore);

    emitTranscript(QStringLiteral(
        "{\"case\":\"write_failure\",\"confirmations\":1,"
        "\"leave_prompts\":0,\"class_count\":2,\"warnings\":1,"
        "\"warning\":\"The class could not be deleted.\","
        "\"warning_details\":%1,\"page_query_delta\":0,"
        "\"sidebar_unchanged\":%2}"
        ).arg(jsonBool(!warning.details.trimmed().isEmpty()),
              jsonBool(selectionAfter == selectionBefore)));
}

void SidebarClassDeleteParityTests::successRefreshesPageAndSelectsClasses()
{
    DeleteFixture fixture;
    QVERIFY(fixture.open());
    const int queriesBefore = fixture.page->runtimeMetrics().classQueryCount;
    fixture.prompts.scriptedChoices.enqueue(PromptChoice::Destructive);

    QVERIFY(fixture.invokeDeleteSelectingTarget());

    const int queryDelta =
        fixture.page->runtimeMetrics().classQueryCount - queriesBefore;
    const QStringList selectionAfter = fixture.sidebar.selectedKeys();
    const auto remaining = fixture.repository()->getClasses();
    QVERIFY(remaining);
    QCOMPARE(remaining->size(), 1);
    QCOMPARE(remaining->first().id, fixture.siblingId);
    QCOMPARE(fixture.prompts.confirmations.size(), 1);
    QVERIFY(fixture.prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(fixture.prompts.messages.isEmpty());
    QCOMPARE(fixture.page->runtimeMetrics().sourceClassCount, 1);
    QCOMPARE(fixture.page->currentClassId(), fixture.siblingId);
    QCOMPARE(queryDelta, 1);
    QCOMPARE(selectionAfter, QStringList{QStringLiteral("classes")});

    emitTranscript(QStringLiteral(
        "{\"case\":\"success\",\"confirmations\":1,"
        "\"leave_prompts\":0,\"class_count\":1,"
        "\"target_removed\":true,\"sibling_preserved\":true,"
        "\"page_refreshes\":1,\"page_class_count\":1,"
        "\"selected_sibling\":true,\"sidebar\":\"classes\"}"
        ));
}

QTEST_MAIN(SidebarClassDeleteParityTests)

#include "sidebar_class_delete_parity_tests.moc"
