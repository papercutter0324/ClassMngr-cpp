#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QCoreApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>

namespace
{
TestingClass testingClass(const QString& name)
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M1");
    value.level = QStringLiteral("Major");
    value.room = QStringLiteral("401");
    value.classColor = QStringLiteral("#336699");
    value.fontColor = QStringLiteral("#FFFFFF");
    return value;
}
}

class TestingClassesPageDeleteParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    void cleanSelectedClassDeletionKeepsOneSiblingAndEmitsOnce();
};

void TestingClassesPageDeleteParityTests::cleanSelectedClassDeletionKeepsOneSiblingAndEmitsOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("delete-parity.tps"))));
    auto* schedule = services.scheduleService();
    QVERIFY(schedule);
    const auto targetId = schedule->createTestingClass(testingClass(QStringLiteral("Alpha Delete Target")));
    const auto siblingId = schedule->createTestingClass(testingClass(QStringLiteral("Beta Survivor")));
    QVERIFY2(targetId, targetId ? "" : qPrintable(targetId.error()));
    QVERIFY2(siblingId, siblingId ? "" : qPrintable(siblingId.error()));
    QVERIFY(*targetId != *siblingId);
    const auto before = schedule->testingClasses();
    QVERIFY(before);
    QCOMPARE(before->size(), 2);

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    // Use only the constructor arguments shared with the pinned legacy page.
    TestingClassesPage page(&services);
    page.openTestingClass(*targetId);
    page.refresh();
    auto* list = page.findChild<QListWidget*>(QStringLiteral("testingClassesList"));
    auto* name = page.findChild<QLineEdit*>(QStringLiteral("testingClassNameEdit"));
    auto* remove = page.findChild<QPushButton*>(QStringLiteral("testingClassesDeleteButton"));
    QVERIFY(list && name && remove);
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *targetId);
    QCOMPARE(name->text(), QStringLiteral("Alpha Delete Target"));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(remove->isEnabled());
    QSignalSpy changed(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changed.isValid());

    remove->click();
    QCoreApplication::processEvents();

    QCOMPARE(prompts.confirmations.size(), 1);
    QVERIFY(prompts.confirmations[0].destructive);
    QVERIFY(prompts.scriptedChoices.isEmpty());
    const auto after = schedule->testingClasses();
    QVERIFY2(after, after ? "" : qPrintable(after.error()));
    const auto targetCount = std::count_if(after->cbegin(), after->cend(),
        [targetId](const TestingClass& value) { return value.classId == *targetId; });
    const auto siblingCount = std::count_if(after->cbegin(), after->cend(),
        [siblingId](const TestingClass& value) { return value.classId == *siblingId; });
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *siblingId);
    QVERIFY(list->currentItem()->text().contains(QStringLiteral("Beta Survivor")));
    const QStringList transcript{
        QStringLiteral("target-count=%1").arg(targetCount),
        QStringLiteral("sibling-count=%1").arg(siblingCount),
        QStringLiteral("persisted-class-count=%1").arg(after->size()),
        QStringLiteral("page-class-count=%1").arg(list->count()),
        QStringLiteral("selected-sibling=%1").arg(list->currentItem()->data(Qt::UserRole).toInt() == *siblingId),
        QStringLiteral("editor-name=%1").arg(name->text()),
        QStringLiteral("editor-dirty=%1").arg(page.hasUnsavedChanges()),
        QStringLiteral("change-signals=%1").arg(changed.size())
    };
    qInfo().noquote() << transcript.join(QStringLiteral("; "));
    const QStringList expected{
        QStringLiteral("target-count=0"),
        QStringLiteral("sibling-count=1"),
        QStringLiteral("persisted-class-count=1"),
        QStringLiteral("page-class-count=1"),
        QStringLiteral("selected-sibling=1"),
        QStringLiteral("editor-name=Beta Survivor"),
        QStringLiteral("editor-dirty=0"),
        QStringLiteral("change-signals=1")
    };
    QCOMPARE(transcript, expected);
}

QTEST_MAIN(TestingClassesPageDeleteParityTests)

#include "testing_classes_page_delete_parity_tests.moc"
