#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QtTest/QtTest>

#include <algorithm>

namespace
{
bool fillDraft(TestingClassesPage& page, const QString& name,
    const QString& room, const QString& notes)
{
    auto* nameEdit = page.findChild<QLineEdit*>(QStringLiteral("testingClassNameEdit"));
    auto* grade = page.findChild<QComboBox*>(QStringLiteral("testingClassGradeCombo"));
    auto* level = page.findChild<QComboBox*>(QStringLiteral("testingClassLevelCombo"));
    auto* roomEdit = page.findChild<QLineEdit*>(QStringLiteral("testingClassRoomEdit"));
    auto* teacher = page.findChild<QComboBox*>(QStringLiteral("testingClassTeacherCombo"));
    auto* notesEdit = page.findChild<QTextEdit*>(QStringLiteral("testingClassNotesEdit"));
    if (!nameEdit || !grade || !level || !roomEdit || !teacher || !notesEdit)
        return false;
    const int none = teacher->findData(-1);
    if (none < 0)
        return false;
    teacher->setCurrentIndex(none);
    nameEdit->setText(name);
    grade->setCurrentText(QStringLiteral("M1"));
    level->setCurrentText(QStringLiteral("Major"));
    roomEdit->setText(room);
    notesEdit->setPlainText(notes);
    return true;
}

QStringList storedFields(const TestingClass& value)
{
    return {value.name, value.grade, value.level, value.room,
        value.teacherId == -1 ? QStringLiteral("None") : QStringLiteral("Unexpected teacher"),
        value.classColor, value.fontColor, value.notes};
}

QStringList expectedFields(const QString& name, const QString& room,
    const QString& notes, const QString& color = QStringLiteral("#FFFFFF"),
    const QString& font = QStringLiteral("#000000"))
{
    return {name, QStringLiteral("M1"), QStringLiteral("Major"), room,
        QStringLiteral("None"), color, font, notes};
}

TestingClass seedClass(const QString& name)
{
    TestingClass value;
    value.name = name;
    value.grade = QStringLiteral("M1");
    value.level = QStringLiteral("Major");
    value.room = QStringLiteral("401");
    value.classColor = QStringLiteral("#336699");
    value.fontColor = QStringLiteral("#FFFFFF");
    value.notes = QStringLiteral("Stored notes");
    return value;
}

void logTranscript(const QStringList& transcript)
{
    qInfo().noquote() << transcript.join(QStringLiteral("; "));
}
}

class TestingClassesPagePersistenceParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    void unassignedCreationPersistsFieldsAndCleanSaveDoesNothing();
    void pendingAssignmentCreationDoesNotLeakIntoNextDraft();
    void updateKeepsOriginalIdSiblingAndSelectionAfterReordering();
};

void TestingClassesPagePersistenceParityTests::unassignedCreationPersistsFieldsAndCleanSaveDoesNothing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("create-parity.tps"))));
    auto* schedule = services.scheduleService();
    QVERIFY(schedule);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TestingClassesPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass();
    page.refresh();
    QVERIFY(!page.hasUnsavedChanges());
    const QString notes = QStringLiteral("First line\nSecond line");
    QVERIFY(fillDraft(page, QStringLiteral("  Created Target  "), QStringLiteral("  402  "), notes));
    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy changed(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changed.isValid());
    QVERIFY(page.saveChanges());
    QCoreApplication::processEvents();

    auto* list = page.findChild<QListWidget*>(QStringLiteral("testingClassesList"));
    auto* save = page.findChild<QPushButton*>(QStringLiteral("testingClassesSaveButton"));
    QVERIFY(list && save && list->currentItem());
    const auto classes = schedule->testingClasses();
    QVERIFY(classes);
    QCOMPARE(classes->size(), 1);
    const int targetId = classes->first().classId;
    QVERIFY(targetId > 0);
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), targetId);
    QVERIFY(list->currentItem()->text().contains(QStringLiteral("Created Target")));
    QCOMPARE(storedFields(classes->first()), expectedFields(QStringLiteral("Created Target"), QStringLiteral("402"), notes));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!save->isEnabled());
    QCOMPARE(changed.size(), 1);
    QVERIFY(page.saveChanges());
    QCoreApplication::processEvents();
    const auto repeated = schedule->testingClasses();
    const auto assignments = schedule->testingAssignments();
    QVERIFY(repeated && assignments);
    QCOMPARE(repeated->size(), 1);
    QCOMPARE(repeated->first().classId, targetId);
    QCOMPARE(storedFields(repeated->first()), storedFields(classes->first()));
    QCOMPARE(assignments->size(), 0);
    QCOMPARE(changed.size(), 1);
    const QStringList transcript{
        QStringLiteral("case=create-unassigned"),
        QStringLiteral("stored-name=%1").arg(repeated->first().name),
        QStringLiteral("stored-room=%1").arg(repeated->first().room),
        QStringLiteral("fields-match=%1").arg(storedFields(repeated->first()) == expectedFields(QStringLiteral("Created Target"), QStringLiteral("402"), notes)),
        QStringLiteral("classes=%1").arg(repeated->size()),
        QStringLiteral("assignments=%1").arg(assignments->size()),
        QStringLiteral("selected-target=%1").arg(list->currentItem()->data(Qt::UserRole).toInt() == targetId),
        QStringLiteral("editor-dirty=%1").arg(page.hasUnsavedChanges()),
        QStringLiteral("change-signals=%1").arg(changed.size())
    };
    logTranscript(transcript);
    QCOMPARE(transcript, QStringList({QStringLiteral("case=create-unassigned"), QStringLiteral("stored-name=Created Target"),
        QStringLiteral("stored-room=402"), QStringLiteral("fields-match=1"), QStringLiteral("classes=1"),
        QStringLiteral("assignments=0"), QStringLiteral("selected-target=1"), QStringLiteral("editor-dirty=0"), QStringLiteral("change-signals=1")}));
}

void TestingClassesPagePersistenceParityTests::pendingAssignmentCreationDoesNotLeakIntoNextDraft()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("assignment-parity.tps"))));
    auto* schedule = services.scheduleService();
    QVERIFY(schedule);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TestingClassesPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass(-1, QStringLiteral("tuesday"), QStringLiteral("17:00"));
    page.refresh();
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(fillDraft(page, QStringLiteral("Assigned Target"), QStringLiteral("402"), QStringLiteral("Assigned notes")));
    QSignalSpy changed(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changed.isValid());
    QVERIFY(page.saveChanges());
    auto* list = page.findChild<QListWidget*>(QStringLiteral("testingClassesList"));
    auto* add = page.findChild<QPushButton*>(QStringLiteral("testingClassesAddButton"));
    auto* save = page.findChild<QPushButton*>(QStringLiteral("testingClassesSaveButton"));
    QVERIFY(list && add && save && list->currentItem());
    const int firstId = list->currentItem()->data(Qt::UserRole).toInt();
    const auto first = schedule->testingClass(firstId);
    const auto firstAssignments = schedule->testingAssignments();
    QVERIFY(first && firstAssignments);
    QCOMPARE(storedFields(*first), expectedFields(QStringLiteral("Assigned Target"), QStringLiteral("402"), QStringLiteral("Assigned notes")));
    QCOMPARE(firstAssignments->size(), 1);
    QCOMPARE(firstAssignments->first().classId, firstId);
    QCOMPARE(firstAssignments->first().kind, TestingAssignmentKind::SpecialClass);
    QCOMPARE(firstAssignments->first().day, QStringLiteral("Tuesday"));
    QCOMPARE(firstAssignments->first().startTime, QStringLiteral("17:00"));
    QVERIFY(list->currentItem()->text().contains(QStringLiteral("Assigned Target")));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!save->isEnabled());
    QCOMPARE(changed.size(), 1);

    add->click();
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!list->currentItem());
    QVERIFY(fillDraft(page, QStringLiteral("Next Target"), QStringLiteral("403"), QStringLiteral("Next notes")));
    QVERIFY(page.saveChanges());
    QCoreApplication::processEvents();
    QVERIFY(list->currentItem());
    const int secondId = list->currentItem()->data(Qt::UserRole).toInt();
    QVERIFY(secondId > 0 && secondId != firstId);
    const auto classes = schedule->testingClasses();
    const auto second = schedule->testingClass(secondId);
    const auto assignments = schedule->testingAssignments();
    QVERIFY(classes && second && assignments);
    QCOMPARE(classes->size(), 2);
    QCOMPARE(list->count(), 2);
    QCOMPARE(storedFields(*second), expectedFields(QStringLiteral("Next Target"), QStringLiteral("403"), QStringLiteral("Next notes")));
    QCOMPARE(assignments->size(), 1);
    QCOMPARE(assignments->first().classId, firstId);
    QVERIFY(list->currentItem()->text().contains(QStringLiteral("Next Target")));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!save->isEnabled());
    QCOMPARE(changed.size(), 2);
    const QStringList transcript{
        QStringLiteral("case=create-pending-then-unassigned"), QStringLiteral("first-fields-match=1"),
        QStringLiteral("second-fields-match=1"), QStringLiteral("classes=%1").arg(classes->size()),
        QStringLiteral("assignments=%1").arg(assignments->size()),
        QStringLiteral("assignment=%1/%2").arg(assignments->first().day, assignments->first().startTime),
        QStringLiteral("assigned-first=%1").arg(assignments->first().classId == firstId),
        QStringLiteral("selected-second=%1").arg(list->currentItem()->data(Qt::UserRole).toInt() == secondId),
        QStringLiteral("editor-dirty=%1").arg(page.hasUnsavedChanges()), QStringLiteral("change-signals=%1").arg(changed.size())
    };
    logTranscript(transcript);
    QCOMPARE(transcript, QStringList({QStringLiteral("case=create-pending-then-unassigned"), QStringLiteral("first-fields-match=1"),
        QStringLiteral("second-fields-match=1"), QStringLiteral("classes=2"), QStringLiteral("assignments=1"),
        QStringLiteral("assignment=Tuesday/17:00"), QStringLiteral("assigned-first=1"), QStringLiteral("selected-second=1"),
        QStringLiteral("editor-dirty=0"), QStringLiteral("change-signals=2")}));
}

void TestingClassesPagePersistenceParityTests::updateKeepsOriginalIdSiblingAndSelectionAfterReordering()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath(QStringLiteral("update-parity.tps"))));
    auto* schedule = services.scheduleService();
    QVERIFY(schedule);
    const auto targetId = schedule->createTestingClass(seedClass(QStringLiteral("Alpha Target")));
    const auto siblingId = schedule->createTestingClass(seedClass(QStringLiteral("Beta Sibling")));
    QVERIFY(targetId && siblingId);
    const auto siblingBefore = schedule->testingClass(*siblingId);
    QVERIFY(siblingBefore);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    TestingClassesPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass(*targetId);
    page.refresh();
    auto* list = page.findChild<QListWidget*>(QStringLiteral("testingClassesList"));
    auto* save = page.findChild<QPushButton*>(QStringLiteral("testingClassesSaveButton"));
    QVERIFY(list && save && list->currentItem());
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->currentRow(), 0);
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *targetId);
    QVERIFY(!page.hasUnsavedChanges());
    const QString notes = QStringLiteral("Updated notes\nSecond line");
    QVERIFY(fillDraft(page, QStringLiteral("  Zulu Updated  "), QStringLiteral("  405  "), notes));
    QVERIFY(page.hasUnsavedChanges());
    QSignalSpy changed(&page, &TestingClassesPage::testingDataChanged);
    QVERIFY(changed.isValid());
    QVERIFY(page.saveChanges());
    QCoreApplication::processEvents();
    const auto classes = schedule->testingClasses();
    const auto target = schedule->testingClass(*targetId);
    const auto sibling = schedule->testingClass(*siblingId);
    QVERIFY(classes && target && sibling);
    QCOMPARE(classes->size(), 2);
    QCOMPARE(target->classId, *targetId);
    QCOMPARE(sibling->classId, *siblingId);
    QCOMPARE(storedFields(*target), expectedFields(QStringLiteral("Zulu Updated"), QStringLiteral("405"), notes,
        QStringLiteral("#336699"), QStringLiteral("#FFFFFF")));
    QCOMPARE(storedFields(*sibling), storedFields(*siblingBefore));
    QCOMPARE(std::count_if(classes->cbegin(), classes->cend(), [targetId](const TestingClass& value) { return value.classId == *targetId; }), 1);
    QCOMPARE(std::count_if(classes->cbegin(), classes->cend(), [siblingId](const TestingClass& value) { return value.classId == *siblingId; }), 1);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), *siblingId);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), *targetId);
    QCOMPARE(list->currentRow(), 1);
    QVERIFY(list->currentItem()->text().contains(QStringLiteral("Zulu Updated")));
    QVERIFY(!page.hasUnsavedChanges());
    QVERIFY(!save->isEnabled());
    QCOMPARE(changed.size(), 1);
    const QStringList transcript{
        QStringLiteral("case=update-with-sibling"), QStringLiteral("original-id-retained=%1").arg(target->classId == *targetId),
        QStringLiteral("stored-name=%1").arg(target->name), QStringLiteral("stored-room=%1").arg(target->room),
        QStringLiteral("sibling-unchanged=%1").arg(storedFields(*sibling) == storedFields(*siblingBefore)),
        QStringLiteral("classes=%1").arg(classes->size()), QStringLiteral("selected-target=%1").arg(list->currentItem()->data(Qt::UserRole).toInt() == *targetId),
        QStringLiteral("selected-row=%1").arg(list->currentRow()), QStringLiteral("editor-dirty=%1").arg(page.hasUnsavedChanges()),
        QStringLiteral("change-signals=%1").arg(changed.size())
    };
    logTranscript(transcript);
    QCOMPARE(transcript, QStringList({QStringLiteral("case=update-with-sibling"), QStringLiteral("original-id-retained=1"),
        QStringLiteral("stored-name=Zulu Updated"), QStringLiteral("stored-room=405"), QStringLiteral("sibling-unchanged=1"),
        QStringLiteral("classes=2"), QStringLiteral("selected-target=1"), QStringLiteral("selected-row=1"),
        QStringLiteral("editor-dirty=0"), QStringLiteral("change-signals=1")}));
}

QTEST_MAIN(TestingClassesPagePersistenceParityTests)

#include "testing_classes_page_persistence_parity_tests.moc"
