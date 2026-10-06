#include "app/controllers/sidebar_controller.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/teacher_import_repository.h"
#include "features/setup/ui/initial_setup_wizard.h"
#include "features/teacher/ui/teacher_import_dialog.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QSignalSpy>
#include <QtTest/QtTest>

#include <cstdio>
#include <cstddef>
#include <functional>

namespace
{
QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("teacher-import-ui-parity-%1.tps")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

QString fixturePath()
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath(
        QStringLiteral("fixtures/teacher_import/sectioned_review.xlsx"));
}

int countRows(QSqlDatabase database, const QString& table)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !query.next())
    {
        return -1;
    }
    return query.value(0).toInt();
}

bool installLateWriteFailure(ApplicationServices& services)
{
    QSqlQuery query(services.databaseSession()->database());
    return query.exec(QStringLiteral(
        "CREATE TRIGGER parity_reject_gs BEFORE INSERT ON gs_team "
        "BEGIN SELECT RAISE(ABORT, 'forced parity failure'); END"));
}

struct DialogRun final
{
    bool found = false;
    bool accepted = false;
};

bool runWithTeacherDialog(
    QWidget* owner,
    const bool accept,
    const std::function<void()>& invoke,
    DialogRun& observation
    )
{
    QTimer poll;
    poll.setInterval(10);
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&poll, &QTimer::timeout, owner, [&]
    {
        auto* dialog = qobject_cast<TeacherImportDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                dialog = qobject_cast<TeacherImportDialog*>(widget);
                if (dialog)
                {
                    break;
                }
            }
        }
        if (!dialog)
        {
            return;
        }

        observation.found = true;
        if (!accept)
        {
            poll.stop();
            dialog->reject();
            return;
        }

        auto* path = dialog->findChild<QLineEdit*>(
            QStringLiteral("teacherImportFilePath"));
        auto* button = dialog->findChild<QPushButton*>(
            QStringLiteral("teacherImportAcceptButton"));
        if (!path || !button)
        {
            poll.stop();
            dialog->reject();
            return;
        }
        if (path->text().isEmpty())
        {
            dialog->setFilePath(fixturePath());
        }
        if (button->isEnabled())
        {
            observation.accepted = true;
            poll.stop();
            button->click();
        }
    });
    QObject::connect(&timeout, &QTimer::timeout, owner, [&]
    {
        if (auto* dialog = qobject_cast<TeacherImportDialog*>(
                QApplication::activeModalWidget()))
        {
            dialog->reject();
        }
    });

    poll.start();
    timeout.start(15000);
    invoke();
    poll.stop();
    timeout.stop();
    return observation.found && (!accept || observation.accepted);
}

QString tableCounts(ApplicationServices& services)
{
    const QSqlDatabase database = services.databaseSession()->database();
    return QStringLiteral("teachers=%1,native=%2,gs=%3")
        .arg(countRows(database, QStringLiteral("teachers")))
        .arg(countRows(database, QStringLiteral("native_english_teachers")))
        .arg(countRows(database, QStringLiteral("gs_team")));
}

int sidebarKoreanTeacherCount(Sidebar& sidebar)
{
    auto* const tree = sidebar.findChild<QTreeWidget*>();
    if (!tree)
    {
        return -1;
    }
    QTreeWidgetItem* campusStaff = nullptr;
    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        auto* const item = tree->topLevelItem(index);
        if (item->data(0, Qt::UserRole + 4).toString()
            == QStringLiteral("campus_staff"))
        {
            campusStaff = item;
            break;
        }
    }
    if (!campusStaff)
    {
        return -1;
    }
    for (int index = 0; index < campusStaff->childCount(); ++index)
    {
        auto* const item = campusStaff->child(index);
        if (item->data(0, Qt::UserRole + 4).toString()
            == QStringLiteral("teachers_all_korean"))
        {
            return item->childCount();
        }
    }
    return -1;
}

QString singleLine(QString value)
{
    value.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
    value.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    return value;
}

struct ScopedPromptService final
{
    explicit ScopedPromptService(IUserPromptService& service)
    {
        DialogServices::setUserPromptServiceForTesting(&service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

void emitTranscript(const QString& entry, const QString& scenario,
    const QString& result)
{
    const QByteArray row = QStringLiteral("TEACHER_IMPORT_UI|%1|%2|%3")
        .arg(entry, scenario, result).toUtf8();
    std::fwrite(row.constData(), 1, static_cast<std::size_t>(row.size()), stdout);
    std::fwrite("\n", 1, 1, stdout);
    std::fflush(stdout);
}
}

class TeacherImportUiApplyParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void sidebarSuccessTranscript();
    void sidebarCancelTranscript();
    void sidebarErrorTranscript();
    void wizardSuccessTranscript();
    void wizardCancelTranscript();
    void wizardErrorTranscript();
};

void TeacherImportUiApplyParityTests::sidebarSuccessTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFileInfo::exists(fixturePath()));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, true,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Information);
    const QString summary = singleLine(prompts.messages.first().message);
    const QString counts = tableCounts(services);
    QVERIFY(counts.contains(QStringLiteral("teachers=")));
    const int refreshedKoreanTeachers = sidebarKoreanTeacherCount(sidebar);
    QVERIFY(refreshedKoreanTeachers > 0);
    emitTranscript(QStringLiteral("sidebar"), QStringLiteral("success"),
        counts + QStringLiteral("|sidebarKoreanTeachers=%1|notice=information|summary=")
            .arg(refreshedKoreanTeachers) + summary);
}

void TeacherImportUiApplyParityTests::sidebarCancelTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, false,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QVERIFY(prompts.messages.isEmpty());
    emitTranscript(QStringLiteral("sidebar"), QStringLiteral("cancel"),
        tableCounts(services) + QStringLiteral("|notice=none"));
}

void TeacherImportUiApplyParityTests::sidebarErrorTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(installLateWriteFailure(services));
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    ActionRegistry actions;
    actions.createActions();
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&sidebar, true,
        [&] { actions.importTeachers->trigger(); }, dialog));

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    emitTranscript(QStringLiteral("sidebar"), QStringLiteral("error"),
        tableCounts(services) + QStringLiteral("|notice=warning|text=")
            + singleLine(prompts.messages.first().message));
}

void TeacherImportUiApplyParityTests::wizardSuccessTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFileInfo::exists(fixturePath()));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::TeacherImportPage);
    wizard.show();
    QApplication::processEvents();
    auto* button = wizard.findChild<QPushButton*>(
        QStringLiteral("setupImportTeachersButton"));
    auto* status = wizard.findChild<QLabel*>(
        QStringLiteral("setupTeacherImportStatus"));
    QVERIFY(button);
    QVERIFY(status);
    QWizardPage* const importPage = wizard.currentPage();
    QVERIFY(importPage);
    QSignalSpy completeChanged(importPage, &QWizardPage::completeChanged);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&wizard, true,
        [&] { button->click(); }, dialog));

    QVERIFY(status->text().startsWith(QStringLiteral("Import complete:")));
    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(completeChanged.size(), 1);
    QVERIFY(importPage->isComplete());
    wizard.next();
    QVERIFY(wizard.currentId() != InitialSetupWizard::TeacherImportPage);
    emitTranscript(QStringLiteral("wizard"), QStringLiteral("success"),
        tableCounts(services) + QStringLiteral("|complete=true|completeChanged=%1|status=")
            .arg(completeChanged.size())
            + singleLine(status->text()));
    wizard.reject();
}

void TeacherImportUiApplyParityTests::wizardCancelTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::TeacherImportPage);
    wizard.show();
    QApplication::processEvents();
    auto* button = wizard.findChild<QPushButton*>(
        QStringLiteral("setupImportTeachersButton"));
    QVERIFY(button);
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&wizard, false,
        [&] { button->click(); }, dialog));

    QVERIFY(!wizard.isVisible());
    QVERIFY(prompts.messages.isEmpty());
    emitTranscript(QStringLiteral("wizard"), QStringLiteral("cancel"),
        tableCounts(services) + QStringLiteral("|result=rejected"));
}

void TeacherImportUiApplyParityTests::wizardErrorTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(installLateWriteFailure(services));
    InitialSetupWizard wizard(&services);
    wizard.setStartId(InitialSetupWizard::TeacherImportPage);
    wizard.show();
    QApplication::processEvents();
    auto* button = wizard.findChild<QPushButton*>(
        QStringLiteral("setupImportTeachersButton"));
    auto* status = wizard.findChild<QLabel*>(
        QStringLiteral("setupTeacherImportStatus"));
    QVERIFY(button);
    QVERIFY(status);
    QWizardPage* const importPage = wizard.currentPage();
    QVERIFY(importPage);
    QSignalSpy completeChanged(importPage, &QWizardPage::completeChanged);
    const QString initialStatus = status->text();
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(prompts);

    DialogRun dialog;
    QVERIFY(runWithTeacherDialog(&wizard, true,
        [&] { button->click(); }, dialog));

    QCOMPARE(status->text(), initialStatus);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.first().severity, PromptSeverity::Warning);
    QCOMPARE(completeChanged.size(), 0);
    wizard.next();
    QCOMPARE(wizard.currentId(), InitialSetupWizard::TeacherImportPage);
    emitTranscript(QStringLiteral("wizard"), QStringLiteral("error"),
        tableCounts(services) + QStringLiteral("|notice=warning|text=")
            + singleLine(prompts.messages.first().message)
            + QStringLiteral("|status=unchanged|completeChanged=%1")
                .arg(completeChanged.size()));
    wizard.reject();
}

QTEST_MAIN(TeacherImportUiApplyParityTests)

#include "teacher_import_ui_apply_parity_tests.moc"
