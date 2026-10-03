#include "app/controllers/navigation_controller.h"
#include "app/controllers/sidebar_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("navigation-teacher-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher teacherFixture(
    const QString& koreanName,
    const QString& englishName,
    const QString& preferredName
    )
{
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.preferredName = preferredName;
    teacher.roomNumber = QStringLiteral("Room 2");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Teacher Wi-Fi");
    teacher.wifiPassword = QStringLiteral("Teacher password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("teacher.zoom");
    teacher.zoomPassword = QStringLiteral("Zoom password");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.notes = QStringLiteral("Teacher notes");
    return teacher;
}

int persistTeacher(ApplicationServices& services, Teacher& teacher)
{
    const auto saved = services.databaseSession()->teacherRepository()
        ->createTeacher(teacher);
    if (!saved)
    {
        return -1;
    }
    teacher.id = *saved;
    return *saved;
}

struct RecordSelectionObservation
{
    bool found = false;
    bool selected = false;
    bool accepted = false;
    int selectedId = -1;
    QStringList labels;
};

QString displayLabel(
    const QString& classFields,
    const QString& teacherName,
    const QString& schedule
    )
{
    const QChar separator(0x2022);
    QString label = classFields + QLatin1Char(' ') + separator
        + QLatin1Char(' ') + teacherName;
    if (!schedule.isEmpty())
    {
        label += QLatin1Char(' ') + separator + QLatin1Char(' ')
            + schedule;
    }
    return label;
}

void createClassWithSubtitle(
    ApplicationServices& services,
    const QString& name,
    const int teacherId,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& startTime,
    int& classId
    )
{
    const auto created = services.classService()->create(name);
    if (!created)
    {
        classId = -1;
        return;
    }

    classId = *created;
    ClassInfo info;
    info.classId = classId;
    info.teacherId = teacherId;
    info.classGrade = grade;
    info.classLevel = level;
    info.classTimes = {
        {day, startTime, QStringLiteral("5:00 PM")}
    };
    const auto saved = services.databaseSession()
        ->classInfoRepository()->saveClassInfo(info);
    if (!saved)
    {
        classId = -1;
    }
}

bool invokeDeleteClassSelecting(
    SidebarController& controller,
    const int classId,
    RecordSelectionObservation& observation
    )
{
    QTimer::singleShot(
        0,
        &controller,
        [&observation, classId]
        {
            QDialog* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                for (QWidget* widget : QApplication::topLevelWidgets())
                {
                    auto* candidate = qobject_cast<QDialog*>(widget);
                    if (candidate
                        && candidate->objectName()
                            == QStringLiteral("sidebarRecordSelectionDialog"))
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

            observation.found = true;
            auto* combo = dialog->findChild<QComboBox*>(
                QStringLiteral("sidebarRecordSelectionCombo")
                );
            auto* buttons = dialog->findChild<QDialogButtonBox*>(
                QStringLiteral("sidebarRecordSelectionButtonBox")
                );
            auto* acceptButton = buttons
                ? buttons->button(QDialogButtonBox::Ok)
                : nullptr;
            if (!combo || !acceptButton)
            {
                dialog->reject();
                return;
            }

            for (int index = 0; index < combo->count(); ++index)
            {
                observation.labels.append(combo->itemText(index));
            }

            const int selectedIndex = combo->findData(classId);
            if (selectedIndex >= 0)
            {
                combo->setCurrentIndex(selectedIndex);
                observation.selected = true;
                observation.selectedId = combo->currentData().toInt();
            }

            if (observation.selected && acceptButton->isEnabled())
            {
                acceptButton->click();
                observation.accepted = true;
            }
            else
            {
                dialog->reject();
            }
        }
        );

    return QMetaObject::invokeMethod(
        &controller,
        "deleteClass",
        Qt::DirectConnection
        );
}

}

class NavigationTeacherReadTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void rawNonpositiveAndMissingIdsReturnBeforeLeaveConfirmation();
    void successfulReadConfirmsBeforeLoadingAndShowingTeacher();
    void classDeleteChooserUsesSubtitleLabelsAndConfirmsSelectedClass();
    void classListReadFailureShowsWarningWithoutOpeningChooser();
    void classFieldsFailureUsesDefaultSubtitleAndNoTeacherFallback();
    void assignedTeacherFailureKeepsClassFieldsAndUsesNoTeacher();
    void classDeleteChooserRequiresAnActiveSession();
};

void NavigationTeacherReadTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void NavigationTeacherReadTests::
rawNonpositiveAndMissingIdsReturnBeforeLeaveConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Teacher selected = teacherFixture(
        QStringLiteral("김선택"),
        QStringLiteral("Selected Teacher"),
        QStringLiteral("Selected Display")
        );
    QVERIFY(persistTeacher(services, selected) > 0);

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    auto* page = pages.teacherPage();
    QVERIFY(page);
    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(selected);
    auto* notes = page->findChild<QTextEdit*>(QStringLiteral("teacherNotesEdit"));
    QVERIFY(notes);
    notes->setPlainText(QStringLiteral("Unsaved change"));
    QVERIFY(page->hasUnsavedChanges());

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    for (const int invalidId : {0, -2})
    {
        navigation.handleNavigation({
            .type = NodeType::Teacher,
            .teacherId = invalidId
        });
        QCOMPARE(prompts.unsavedChangesConfirmations.size(), 0);
        QVERIFY(page->hasUnsavedChanges());
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QCOMPARE(page->teacher().id, selected.id);
    }

    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = selected.id + 1000
    });
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 0);
    QVERIFY(page->hasUnsavedChanges());
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(page->teacher().id, selected.id);
}

void NavigationTeacherReadTests::
successfulReadConfirmsBeforeLoadingAndShowingTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    Teacher selected = teacherFixture(
        QStringLiteral("김선택"),
        QStringLiteral("Selected Teacher"),
        QStringLiteral("Selected Display")
        );
    Teacher target = teacherFixture(
        QStringLiteral("박대상"),
        QStringLiteral("Target Teacher"),
        QStringLiteral("Target Display")
        );
    QVERIFY(persistTeacher(services, selected) > 0);
    QVERIFY(persistTeacher(services, target) > 0);

    PageManager pages;
    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    auto* page = pages.teacherPage();
    QVERIFY(page);
    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(selected);
    auto* notes = page->findChild<QTextEdit*>(QStringLiteral("teacherNotesEdit"));
    QVERIFY(notes);
    notes->setPlainText(QStringLiteral("Unsaved change"));
    QVERIFY(page->hasUnsavedChanges());

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel);

    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = target.id
    });
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QCOMPARE(prompts.unsavedChangesConfirmations.first().title,
        QStringLiteral("Unsaved Teacher Changes"));
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QVERIFY(page->hasUnsavedChanges());
    QCOMPARE(page->teacher().id, selected.id);

    page->discardChanges();
    QVERIFY(!page->hasUnsavedChanges());
    pages.showPage(PageType::MyWorkspace);
    navigation.handleNavigation({
        .type = NodeType::Teacher,
        .teacherId = target.id
    });

    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(page->teacher().id, target.id);
    QCOMPARE(page->teacher().teacherEn, target.teacherEn);
    QCOMPARE(page->teacher().preferredName, target.preferredName);
    auto* header = page->findChild<PageHeader*>();
    QVERIFY(header);
    QVERIFY(header->title().contains(QStringLiteral("Target Display")));
    QVERIFY(!page->hasUnsavedChanges());
}

void NavigationTeacherReadTests::
classDeleteChooserUsesSubtitleLabelsAndConfirmsSelectedClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher alphaTeacher = teacherFixture(
        QStringLiteral("Alpha Teacher"),
        QStringLiteral("Alpha Teacher"),
        QStringLiteral("Alpha Display")
        );
    Teacher betaTeacher = teacherFixture(
        QStringLiteral("Beta Teacher"),
        QStringLiteral("Beta Teacher"),
        QStringLiteral("Beta Display")
        );
    QVERIFY(persistTeacher(services, alphaTeacher) > 0);
    QVERIFY(persistTeacher(services, betaTeacher) > 0);

    int alphaClassId = -1;
    int betaClassId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Alpha class"),
        alphaTeacher.id,
        QStringLiteral(" E4 "),
        QStringLiteral(" Orion "),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        alphaClassId
        );
    createClassWithSubtitle(
        services,
        QStringLiteral("Beta class"),
        betaTeacher.id,
        QStringLiteral("E5"),
        QStringLiteral("Vega"),
        QStringLiteral("Tuesday"),
        QStringLiteral("5:00 PM"),
        betaClassId
        );
    QVERIFY(alphaClassId > 0);
    QVERIFY(betaClassId > 0);

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    DialogServices::setUserPromptServiceForTesting(&prompts);

    RecordSelectionObservation observation;
    QVERIFY(invokeDeleteClassSelecting(
        controller, betaClassId, observation));

    QVERIFY(observation.found);
    QVERIFY(observation.selected);
    QVERIFY(observation.accepted);
    QCOMPARE(observation.selectedId, betaClassId);
    QCOMPARE(observation.labels.size(), 3);
    QCOMPARE(observation.labels.at(0), QString());
    QCOMPARE(observation.labels.at(1),
        displayLabel(
            QStringLiteral("E4 Orion"),
            QStringLiteral("Alpha Display"),
            QStringLiteral("Mon (4:00)")));
    QCOMPARE(observation.labels.at(2),
        displayLabel(
            QStringLiteral("E5 Vega"),
            QStringLiteral("Beta Display"),
            QStringLiteral("Tues (5:00)")));

    QCOMPARE(prompts.confirmations.size(), 1);
    const PromptRequest confirmation = prompts.confirmations.first();
    QCOMPARE(confirmation.title, QStringLiteral("Delete Class"));
    QCOMPARE(confirmation.message,
        QStringLiteral("Delete '%1'?").arg(displayLabel(
            QStringLiteral("E5 Vega"),
            QStringLiteral("Beta Display"),
            QStringLiteral("Tues (5:00)"))));
    QCOMPARE(confirmation.acceptText, QStringLiteral("Delete"));
    QCOMPARE(confirmation.rejectText, QStringLiteral("Cancel"));
    QVERIFY(confirmation.destructive);
    QVERIFY(services.classService()->classroom(alphaClassId));
    QVERIFY(!services.classService()->classroom(betaClassId));
}

void NavigationTeacherReadTests::
classListReadFailureShowsWarningWithoutOpeningChooser()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Stored class before list failure"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery dropClasses(services.databaseSession()->database());
    QVERIFY2(dropClasses.exec(QStringLiteral("DROP TABLE classes")),
             qPrintable(dropClasses.lastError().text()));
    QVERIFY(services.databaseSession()->isOpen());
    QVERIFY(services.classService()->isAvailable());

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    bool chooserOpened = false;
    QTimer::singleShot(
        0,
        &controller,
        [&chooserOpened]
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
                            == QStringLiteral("sidebarRecordSelectionDialog"))
                    {
                        dialog = candidate;
                        break;
                    }
                }
            }

            if (dialog)
            {
                chooserOpened = true;
                dialog->reject();
            }
        }
        );

    QVERIFY(QMetaObject::invokeMethod(
        &controller,
        "deleteClass",
        Qt::DirectConnection
        ));
    QCoreApplication::processEvents();

    QVERIFY(!chooserOpened);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Delete Class"));
    QCOMPARE(warning.message, QStringLiteral("Classes could not be loaded."));
    QVERIFY(!warning.details.trimmed().isEmpty());
    QVERIFY(warning.details.contains(QStringLiteral("classes"),
                                    Qt::CaseInsensitive));
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void NavigationTeacherReadTests::
classFieldsFailureUsesDefaultSubtitleAndNoTeacherFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher = teacherFixture(
        QStringLiteral("Teacher"),
        QStringLiteral("Teacher"),
        QStringLiteral("Should not appear")
        );
    QVERIFY(persistTeacher(services, teacher) > 0);

    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Trimmed class name"),
        teacher.id,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    QSqlQuery dropSchedule(services.databaseSession()->database());
    QVERIFY(dropSchedule.exec(QStringLiteral("DROP TABLE class_times")));

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    DialogServices::setUserPromptServiceForTesting(&prompts);

    RecordSelectionObservation observation;
    QVERIFY(invokeDeleteClassSelecting(controller, classId, observation));

    QVERIFY(observation.found);
    QVERIFY(observation.selected);
    QVERIFY(observation.accepted);
    QCOMPARE(observation.selectedId, classId);
    const QStringList expectedLabels{
        QString(),
        displayLabel(
            QStringLiteral("Unknown Class"),
            QStringLiteral("No Teacher"),
            QString())
    };
    QCOMPARE(observation.labels, expectedLabels);
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.confirmations.first().message,
        QStringLiteral("Delete '%1'?").arg(displayLabel(
            QStringLiteral("Unknown Class"),
            QStringLiteral("No Teacher"),
            QString())));
    QVERIFY(services.classService()->classroom(classId));
}

void NavigationTeacherReadTests::
assignedTeacherFailureKeepsClassFieldsAndUsesNoTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher = teacherFixture(
        QStringLiteral("Teacher"),
        QStringLiteral("Teacher"),
        QStringLiteral("Unavailable Teacher")
        );
    QVERIFY(persistTeacher(services, teacher) > 0);

    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Class with stale teacher"),
        teacher.id,
        QStringLiteral(" E4 "),
        QStringLiteral(" Orion "),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")));

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    DialogServices::setUserPromptServiceForTesting(&prompts);

    RecordSelectionObservation observation;
    QVERIFY(invokeDeleteClassSelecting(controller, classId, observation));

    QVERIFY(observation.found);
    QVERIFY(observation.selected);
    QVERIFY(observation.accepted);
    QCOMPARE(observation.selectedId, classId);
    const QStringList expectedLabels{
        QString(),
        displayLabel(
            QStringLiteral("E4 Orion"),
            QStringLiteral("No Teacher"),
            QStringLiteral("Mon (4:00)"))
    };
    QCOMPARE(observation.labels, expectedLabels);
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.confirmations.first().message,
        QStringLiteral("Delete '%1'?").arg(displayLabel(
            QStringLiteral("E4 Orion"),
            QStringLiteral("No Teacher"),
            QStringLiteral("Mon (4:00)"))));
    QVERIFY(services.classService()->classroom(classId));
}

void NavigationTeacherReadTests::
classDeleteChooserRequiresAnActiveSession()
{
    ApplicationServices services;
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(QMetaObject::invokeMethod(
        &controller,
        "deleteClass",
        Qt::DirectConnection
        ));
    QCOMPARE(prompts.confirmations.size(), 0);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

QTEST_MAIN(NavigationTeacherReadTests)

#include "navigation_teacher_read_tests.moc"
