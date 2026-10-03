#include "app/controllers/navigation_controller.h"
#include "app/controllers/sidebar_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "next/application/class_teacher_assignments_read_query.h"
#include "next/application/gs_team_directory_read_query.h"
#include "next/application/initial_setup_teacher_choices_read_query.h"
#include "next/application/korean_teacher_birthday_directory_read_query.h"
#include "next/application/native_english_teacher_directory_read_query.h"
#include "next/platform/application_services_gs_team_directory_read_port.h"
#include "next/platform/application_services_class_teacher_assignments_read_port.h"
#include "next/platform/application_services_initial_setup_teacher_choices_read_port.h"
#include "next/platform/application_services_korean_teacher_birthday_directory_read_port.h"
#include "next/platform/application_services_native_english_teacher_directory_read_port.h"
#include "ui/shared/actions/action_registry.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QtTest/QtTest>

#include <functional>

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

struct BirthdayDialogObservation
{
    bool modalOpened = false;
    bool upcomingBirthdaysOpened = false;
    QStringList entryNames;
    QStringList entryDetails;
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

QTreeWidgetItem* treeItemWithKey(
    QTreeWidget* tree,
    const QString& key
    )
{
    if (!tree)
    {
        return nullptr;
    }
    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* const item = tree->topLevelItem(index);
        if (item
            && item->data(0, Qt::UserRole + 4).toString() == key)
        {
            return item;
        }
    }
    return nullptr;
}

QTreeWidgetItem* childItemWithKey(
    QTreeWidgetItem* parent,
    const QString& key
    )
{
    if (!parent)
    {
        return nullptr;
    }
    for (int index = 0; index < parent->childCount(); ++index)
    {
        QTreeWidgetItem* const item = parent->child(index);
        if (item
            && item->data(0, Qt::UserRole + 4).toString() == key)
        {
            return item;
        }
    }
    return nullptr;
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
    RecordSelectionObservation& observation,
    const std::function<void()>& beforeAccept = {}
    )
{
    QTimer::singleShot(
        0,
        &controller,
        [&observation, classId, beforeAccept]
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
                if (beforeAccept)
                {
                    beforeAccept();
                }
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

bool invokeDeleteTeacher(SidebarController& controller)
{
    return QMetaObject::invokeMethod(
        &controller,
        "deleteTeacher",
        Qt::DirectConnection
        );
}

bool invokeDeleteTeacherSelecting(
    SidebarController& controller,
    const int teacherId,
    RecordSelectionObservation& observation
    )
{
    QTimer::singleShot(
        0,
        &controller,
        [&observation, teacherId]
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

            const int selectedIndex = combo->findData(teacherId);
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

    return invokeDeleteTeacher(controller);
}

bool invokeUpcomingBirthdays(
    SidebarController& controller,
    BirthdayDialogObservation& observation
    )
{
    QTimer::singleShot(
        0,
        &controller,
        [&observation]
        {
            QDialog* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                for (QWidget* widget : QApplication::topLevelWidgets())
                {
                    auto* candidate = qobject_cast<QDialog*>(widget);
                    if (candidate && candidate->isVisible())
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

            observation.modalOpened = true;
            observation.upcomingBirthdaysOpened =
                dialog->objectName()
                    == QStringLiteral("upcomingBirthdaysDialog");
            if (observation.upcomingBirthdaysOpened)
            {
                const auto labels = dialog->findChildren<QLabel*>();
                for (QLabel* label : labels)
                {
                    if (label->objectName().endsWith(QStringLiteral("Name")))
                    {
                        observation.entryNames.append(label->text());
                        QString detailObjectName = label->objectName();
                        detailObjectName.replace(
                            QStringLiteral("Name"),
                            QStringLiteral("Detail")
                            );
                        if (auto* detail = dialog->findChild<QLabel*>(
                                detailObjectName))
                        {
                            observation.entryDetails.append(detail->text());
                        }
                    }
                }
            }

            dialog->reject();
        }
        );

    return QMetaObject::invokeMethod(
        &controller,
        "showUpcomingBirthdays",
        Qt::DirectConnection
        );
}

QString birthdayForOffset(const int daysFromToday)
{
    return QDate::currentDate().addDays(daysFromToday)
        .toString(QStringLiteral("MM-dd"));
}

QString directoryReadDetails(
    ApplicationServices& services,
    const bool nativeEnglish
    )
{
    if (nativeEnglish)
    {
        ClassMngr::Next::Platform::
            ApplicationServicesNativeEnglishTeacherDirectoryReadPort port(
                &services);
        const ClassMngr::Next::Application::
            NativeEnglishTeacherDirectoryReadQuery query(port);
        const auto result = query.execute();
        return result
            ? QString()
            : QString::fromUtf8(
                result.error().message.data(),
                static_cast<qsizetype>(result.error().message.size())
                );
    }

    ClassMngr::Next::Platform::
        ApplicationServicesGsTeamDirectoryReadPort port(&services);
    const ClassMngr::Next::Application::GsTeamDirectoryReadQuery query(port);
    const auto result = query.execute();
    return result
        ? QString()
        : QString::fromUtf8(
            result.error().message.data(),
            static_cast<qsizetype>(result.error().message.size())
            );
}

QString koreanBirthdayDirectoryReadDetails(ApplicationServices& services)
{
    ClassMngr::Next::Platform::
        ApplicationServicesKoreanTeacherBirthdayDirectoryReadPort port(
            &services);
    const ClassMngr::Next::Application::
        KoreanTeacherBirthdayDirectoryReadQuery query(port);
    const auto result = query.execute();
    return result
        ? QString()
        : QString::fromUtf8(
            result.error().message.data(),
            static_cast<qsizetype>(result.error().message.size())
            );
}

QString classTeacherAssignmentsReadDetails(ApplicationServices& services)
{
    ClassMngr::Next::Platform::
        ApplicationServicesClassTeacherAssignmentsReadPort port(&services);
    const ClassMngr::Next::Application::
        ClassTeacherAssignmentsReadQuery query(port);
    const auto result = query.execute();
    return result
        ? QString()
        : QString::fromUtf8(
            result.error().message.data(),
            static_cast<qsizetype>(result.error().message.size())
            );
}

QString initialSetupTeacherChoicesReadDetails(ApplicationServices& services)
{
    ClassMngr::Next::Platform::
        ApplicationServicesInitialSetupTeacherChoicesReadPort port(&services);
    const ClassMngr::Next::Application::
        InitialSetupTeacherChoicesReadQuery query(port);
    const auto result = query.execute();
    return result
        ? QString()
        : QString::fromUtf8(
            result.error().message.data(),
            static_cast<qsizetype>(result.error().message.size())
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
    void selectedTeacherDeleteConfirmsProfileDisplayNameAndCanBeCanceled();
    void selectedTeacherProfileReadFailureWarnsWithoutConfirmation();
    void teacherDeleteChooserUsesTeacherChoiceLabelAndCancelsSelectedId();
    void teacherChoiceReadFailureWarnsWithoutChooserOrConfirmation();
    void refreshTeacherSidebarShowsAssignedAndUnassignedTeachers();
    void refreshTeacherSidebarKeepsUnassignedClassActionEnabled();
    void refreshTeacherSidebarFailureWarnsClearsNodesAndUpdatesActions();
    void refreshTeacherSidebarAssignmentFailureWarnsAndUpdatesActions();
    void refreshTeacherSidebarReturnsSilentlyWithoutAnActiveSession();
    void updateActionStatesClassListFailureDisablesClassActionsOnly();
    void classDeleteChooserUsesSubtitleLabelsAndConfirmsSelectedClass();
    void classListReadFailureShowsWarningWithoutOpeningChooser();
    void classListReloadFailureAfterChooserWarnsBeforeConfirmation();
    void selectedClassMissingFromReloadWarnsBeforeConfirmation();
    void classFieldsFailureUsesDefaultSubtitleAndNoTeacherFallback();
    void assignedTeacherFailureKeepsClassFieldsAndUsesNoTeacher();
    void classDeleteChooserRequiresAnActiveSession();
    void upcomingBirthdaysActionShowsEntriesFromAllStaffDirectories();
    void upcomingBirthdaysActionWarnsOnKoreanDirectoryFailureFirst();
    void upcomingBirthdaysActionReturnsSilentlyWithoutAnActiveSession();
    void upcomingBirthdaysActionShowsWarningWhenGsDirectoryReadFails();
    void upcomingBirthdaysActionPrefersNativeEnglishErrorWhenBothReadsFail();
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
selectedTeacherDeleteConfirmsProfileDisplayNameAndCanBeCanceled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher selected = teacherFixture(
        QStringLiteral("Selected Teacher Korean"),
        QStringLiteral("Selected Teacher English"),
        QStringLiteral("Selected Display Name")
        );
    QVERIFY(persistTeacher(services, selected) > 0);

    PageManager pages;
    Sidebar sidebar;
    sidebar.addTeacherNode(
        QStringLiteral("Stale Sidebar Label"),
        selected.id,
        false
        );
    sidebar.selectTeacher(selected.id);
    QCOMPARE(sidebar.getSelectedTeacherId(), selected.id);

    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(invokeDeleteTeacher(controller));

    QCOMPARE(prompts.confirmations.size(), 1);
    const PromptRequest& confirmation = prompts.confirmations.constFirst();
    QCOMPARE(confirmation.title, QStringLiteral("Delete Teacher"));
    QCOMPARE(
        confirmation.message,
        QStringLiteral("Delete 'Selected Display Name'?")
        );
    QCOMPARE(confirmation.acceptText, QStringLiteral("Delete"));
    QCOMPARE(confirmation.rejectText, QStringLiteral("Cancel"));
    QVERIFY(confirmation.destructive);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());

    const auto remaining = services.teacherService()->teacher(selected.id);
    QVERIFY(remaining);
    QCOMPARE(remaining->id, selected.id);
    QCOMPARE(
        remaining->preferredName,
        QStringLiteral("Selected Display Name")
        );
}

void NavigationTeacherReadTests::
selectedTeacherProfileReadFailureWarnsWithoutConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher selected = teacherFixture(
        QStringLiteral("Selected Teacher Korean"),
        QStringLiteral("Selected Teacher English"),
        QStringLiteral("Selected Display Name")
        );
    QVERIFY(persistTeacher(services, selected) > 0);

    Sidebar sidebar;
    sidebar.addTeacherNode(
        QStringLiteral("Selected Display Name"),
        selected.id,
        false
        );
    sidebar.selectTeacher(selected.id);
    QCOMPARE(sidebar.getSelectedTeacherId(), selected.id);

    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));
    const Result<Teacher> expectedProfileRead =
        services.teacherService()->teacher(selected.id);
    QVERIFY(!expectedProfileRead);
    QVERIFY(!expectedProfileRead.error().trimmed().isEmpty());

    PageManager pages;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    QVERIFY(invokeDeleteTeacher(controller));

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Delete Teacher"));
    QCOMPARE(
        warning.message,
        QStringLiteral("The teacher could not be loaded.")
        );
    QCOMPARE(warning.details, expectedProfileRead.error());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QCOMPARE(sidebar.getSelectedTeacherId(), selected.id);
}

void NavigationTeacherReadTests::
teacherDeleteChooserUsesTeacherChoiceLabelAndCancelsSelectedId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher = teacherFixture(
        QStringLiteral("Chooser Teacher Korean"),
        QStringLiteral("Chooser Teacher English"),
        QStringLiteral("Teacher Chooser Display")
        );
    QVERIFY(persistTeacher(services, teacher) > 0);

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    DialogServices::setUserPromptServiceForTesting(&prompts);

    RecordSelectionObservation observation;
    QVERIFY(invokeDeleteTeacherSelecting(
        controller,
        teacher.id,
        observation
        ));

    QVERIFY(observation.found);
    QVERIFY(observation.selected);
    QVERIFY(observation.accepted);
    QCOMPARE(observation.selectedId, teacher.id);
    QCOMPARE(
        observation.labels,
        QStringList({QString(), QStringLiteral("Teacher Chooser Display")})
        );
    QCOMPARE(prompts.confirmations.size(), 1);
    const PromptRequest& confirmation = prompts.confirmations.constFirst();
    QCOMPARE(confirmation.title, QStringLiteral("Delete Teacher"));
    QCOMPARE(
        confirmation.message,
        QStringLiteral("Delete 'Teacher Chooser Display'?")
        );
    QCOMPARE(confirmation.acceptText, QStringLiteral("Delete"));
    QCOMPARE(confirmation.rejectText, QStringLiteral("Cancel"));
    QVERIFY(confirmation.destructive);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(services.teacherService()->teacher(teacher.id));
}

void NavigationTeacherReadTests::
teacherChoiceReadFailureWarnsWithoutChooserOrConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher = teacherFixture(
        QStringLiteral("Chooser Teacher Korean"),
        QStringLiteral("Chooser Teacher English"),
        QStringLiteral("Teacher Chooser Display")
        );
    QVERIFY(persistTeacher(services, teacher) > 0);

    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));
    const Result<QList<Teacher>> expectedChoicesRead =
        services.teacherService()->teachers();
    QVERIFY(!expectedChoicesRead);
    QVERIFY(!expectedChoicesRead.error().trimmed().isEmpty());

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    RecordSelectionObservation observation;
    QVERIFY(invokeDeleteTeacherSelecting(
        controller,
        teacher.id,
        observation
        ));
    QCoreApplication::processEvents();

    QVERIFY(!observation.found);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Delete Teacher"));
    QCOMPARE(warning.message, QStringLiteral("Teachers could not be loaded."));
    QCOMPARE(warning.details, expectedChoicesRead.error());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void NavigationTeacherReadTests::
refreshTeacherSidebarShowsAssignedAndUnassignedTeachers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher zulu = teacherFixture(
        QStringLiteral("Zulu Korean"),
        QStringLiteral("Zulu English"),
        QStringLiteral("Zulu Display")
        );
    Teacher alpha = teacherFixture(
        QStringLiteral("Alpha Korean"),
        QStringLiteral("alpha English"),
        QStringLiteral("Alpha Display")
        );
    Teacher middle = teacherFixture(
        QStringLiteral("Middle Korean"),
        QStringLiteral("middle English"),
        QStringLiteral("Middle Display")
        );
    QVERIFY(persistTeacher(services, zulu) > 0);
    QVERIFY(persistTeacher(services, alpha) > 0);
    QVERIFY(persistTeacher(services, middle) > 0);

    int zuluClassId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("A class for Zulu"),
        zulu.id,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        zuluClassId
        );
    QVERIFY(zuluClassId > 0);
    int alphaClassId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Z class for Alpha"),
        alpha.id,
        QStringLiteral("E5"),
        QStringLiteral("Vega"),
        QStringLiteral("Tuesday"),
        QStringLiteral("5:00 PM"),
        alphaClassId
        );
    QVERIFY(alphaClassId > 0);
    QVERIFY(services.classService()->create(
        QStringLiteral("M class without an assigned teacher")
        ));

    ClassMngr::Next::Platform::
        ApplicationServicesInitialSetupTeacherChoicesReadPort
            teacherChoicesPort(&services);
    const ClassMngr::Next::Application::
        InitialSetupTeacherChoicesReadQuery teacherChoicesQuery(
            teacherChoicesPort);
    const auto repositoryTeacherChoices = teacherChoicesQuery.execute();
    QVERIFY(repositoryTeacherChoices);
    QCOMPARE(repositoryTeacherChoices.value().teachers.size(), std::size_t(3));
    QCOMPARE(
        repositoryTeacherChoices.value().teachers[0].teacherId.value(),
        std::to_string(zulu.id)
        );
    QCOMPARE(
        repositoryTeacherChoices.value().teachers[1].teacherId.value(),
        std::to_string(alpha.id)
        );
    QCOMPARE(
        repositoryTeacherChoices.value().teachers[2].teacherId.value(),
        std::to_string(middle.id)
        );

    ClassMngr::Next::Platform::
        ApplicationServicesClassTeacherAssignmentsReadPort assignmentsPort(
            &services);
    const ClassMngr::Next::Application::
        ClassTeacherAssignmentsReadQuery assignmentsQuery(assignmentsPort);
    const auto repositoryAssignments = assignmentsQuery.execute();
    QVERIFY(repositoryAssignments);
    QCOMPARE(repositoryAssignments.value().assignments.size(), std::size_t(3));
    QCOMPARE(
        repositoryAssignments.value().assignments[0].teacherId->value(),
        std::to_string(zulu.id)
        );
    QVERIFY(!repositoryAssignments.value().assignments[1].teacherId);
    QCOMPARE(
        repositoryAssignments.value().assignments[2].teacherId->value(),
        std::to_string(alpha.id)
        );

    ActionRegistry actions;
    actions.createActions();
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    controller.refreshTeacherSidebar();

    auto* const tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    auto* const coTeachers = treeItemWithKey(
        tree,
        QStringLiteral("co_teachers")
        );
    auto* const campusStaff = treeItemWithKey(
        tree,
        QStringLiteral("campus_staff")
        );
    auto* const koreanTeachers = childItemWithKey(
        campusStaff,
        QStringLiteral("teachers_all_korean")
        );
    QVERIFY(coTeachers);
    QVERIFY(koreanTeachers);

    QCOMPARE(coTeachers->childCount(), 2);
    QStringList coTeacherLabels;
    QList<int> coTeacherIds;
    for (int index = 0; index < coTeachers->childCount(); ++index)
    {
        QTreeWidgetItem* const item = coTeachers->child(index);
        coTeacherLabels.append(item->text(0));
        coTeacherIds.append(item->data(0, Qt::UserRole + 3).toInt());
    }
    QCOMPARE(
        coTeacherLabels,
        QStringList({
            QStringLiteral("Alpha Display"),
            QStringLiteral("Zulu Display")
        })
        );
    QCOMPARE(coTeacherIds, QList<int>({alpha.id, zulu.id}));
    QVERIFY(coTeacherIds.contains(alpha.id));
    QVERIFY(coTeacherIds.contains(zulu.id));

    QCOMPARE(koreanTeachers->childCount(), 3);
    QStringList allTeacherLabels;
    QList<int> allTeacherIds;
    for (int index = 0; index < koreanTeachers->childCount(); ++index)
    {
        QTreeWidgetItem* const item = koreanTeachers->child(index);
        allTeacherLabels.append(item->text(0));
        allTeacherIds.append(item->data(0, Qt::UserRole + 3).toInt());
    }
    QCOMPARE(
        allTeacherLabels,
        QStringList({
            QStringLiteral("Alpha Display"),
            QStringLiteral("Middle Display"),
            QStringLiteral("Zulu Display")
        })
        );
    QCOMPARE(allTeacherIds, QList<int>({alpha.id, middle.id, zulu.id}));
    QVERIFY(allTeacherIds.contains(alpha.id));
    QVERIFY(allTeacherIds.contains(middle.id));
    QVERIFY(allTeacherIds.contains(zulu.id));
    QVERIFY(actions.deleteTeacher->isEnabled());
    QVERIFY(actions.deleteClass->isEnabled());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void NavigationTeacherReadTests::
refreshTeacherSidebarFailureWarnsClearsNodesAndUpdatesActions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher assigned = teacherFixture(
        QStringLiteral("Assigned Korean"),
        QStringLiteral("Assigned English"),
        QStringLiteral("Assigned Display")
        );
    QVERIFY(persistTeacher(services, assigned) > 0);

    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Assigned teacher class"),
        assigned.id,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    ActionRegistry actions;
    actions.createActions();
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    controller.connectActions(actions);
    controller.refreshTeacherSidebar();

    auto* const tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    auto* const coTeachers = treeItemWithKey(
        tree,
        QStringLiteral("co_teachers")
        );
    auto* const campusStaff = treeItemWithKey(
        tree,
        QStringLiteral("campus_staff")
        );
    auto* const koreanTeachers = childItemWithKey(
        campusStaff,
        QStringLiteral("teachers_all_korean")
        );
    QVERIFY(coTeachers);
    QVERIFY(koreanTeachers);
    QCOMPARE(coTeachers->childCount(), 1);
    QCOMPARE(koreanTeachers->childCount(), 1);
    QVERIFY(actions.deleteTeacher->isEnabled());
    QVERIFY(actions.deleteClass->isEnabled());

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery dropClasses(services.databaseSession()->database());
    QVERIFY2(dropClasses.exec(QStringLiteral("DROP TABLE classes")),
             qPrintable(dropClasses.lastError().text()));
    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));
    const QString expectedTeacherChoicesRead =
        initialSetupTeacherChoicesReadDetails(services);
    const QString expectedAssignmentsRead =
        classTeacherAssignmentsReadDetails(services);
    QVERIFY(!expectedTeacherChoicesRead.isEmpty());
    QVERIFY(!expectedAssignmentsRead.isEmpty());
    QVERIFY(expectedTeacherChoicesRead != expectedAssignmentsRead);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    controller.refreshTeacherSidebar();

    QCOMPARE(coTeachers->childCount(), 0);
    QCOMPARE(koreanTeachers->childCount(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Load Teachers"));
    QCOMPARE(
        warning.message,
        QStringLiteral("Teachers and their classes could not be loaded.")
        );
    QCOMPARE(warning.details, expectedTeacherChoicesRead);
    QVERIFY(warning.details != expectedAssignmentsRead);
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(!actions.deleteTeacher->isEnabled());
    QVERIFY(!actions.deleteClass->isEnabled());
    QVERIFY(actions.importTeachers->isEnabled());
}

void NavigationTeacherReadTests::
refreshTeacherSidebarKeepsUnassignedClassActionEnabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(services.classService()->create(
        QStringLiteral("Unassigned only class")
        ));

    ActionRegistry actions;
    actions.createActions();
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    controller.refreshTeacherSidebar();

    QVERIFY(actions.deleteClass->isEnabled());
    QVERIFY(!actions.deleteTeacher->isEnabled());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void NavigationTeacherReadTests::
refreshTeacherSidebarAssignmentFailureWarnsAndUpdatesActions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher = teacherFixture(
        QStringLiteral("Available Teacher Korean"),
        QStringLiteral("Available Teacher English"),
        QStringLiteral("Available Teacher Display")
        );
    QVERIFY(persistTeacher(services, teacher) > 0);

    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Assignment Failure Class"),
        teacher.id,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    ActionRegistry actions;
    actions.createActions();
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    controller.connectActions(actions);
    controller.refreshTeacherSidebar();
    QVERIFY(actions.deleteTeacher->isEnabled());
    QVERIFY(actions.deleteClass->isEnabled());

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery dropClasses(services.databaseSession()->database());
    QVERIFY2(dropClasses.exec(QStringLiteral("DROP TABLE classes")),
             qPrintable(dropClasses.lastError().text()));
    const QString expectedDetails =
        classTeacherAssignmentsReadDetails(services);
    QVERIFY(!expectedDetails.isEmpty());

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    controller.refreshTeacherSidebar();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Load Teachers"));
    QCOMPARE(
        warning.message,
        QStringLiteral("Teachers and their classes could not be loaded.")
        );
    QCOMPARE(warning.details, expectedDetails);
    QVERIFY(warning.details.contains(QStringLiteral("Loading class teacher assignments")));
    QVERIFY(!actions.deleteClass->isEnabled());
    QVERIFY(actions.deleteTeacher->isEnabled());
    QVERIFY(actions.importTeachers->isEnabled());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void NavigationTeacherReadTests::
refreshTeacherSidebarReturnsSilentlyWithoutAnActiveSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher = teacherFixture(
        QStringLiteral("Session Teacher Korean"),
        QStringLiteral("Session Teacher English"),
        QStringLiteral("Session Teacher Display")
        );
    QVERIFY(persistTeacher(services, teacher) > 0);

    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Session Class"),
        teacher.id,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    ActionRegistry actions;
    actions.createActions();
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    controller.connectActions(actions);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    controller.refreshTeacherSidebar();
    QVERIFY(actions.deleteTeacher->isEnabled());
    QVERIFY(actions.deleteClass->isEnabled());
    services.closeDatabase();

    controller.refreshTeacherSidebar();

    auto* const tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);
    auto* const coTeachers = treeItemWithKey(
        tree,
        QStringLiteral("co_teachers")
        );
    auto* const campusStaff = treeItemWithKey(
        tree,
        QStringLiteral("campus_staff")
        );
    auto* const koreanTeachers = childItemWithKey(
        campusStaff,
        QStringLiteral("teachers_all_korean")
        );
    QVERIFY(coTeachers);
    QVERIFY(koreanTeachers);
    QCOMPARE(coTeachers->childCount(), 0);
    QCOMPARE(koreanTeachers->childCount(), 0);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(!actions.deleteTeacher->isEnabled());
    QVERIFY(!actions.deleteClass->isEnabled());
    QVERIFY(!actions.importTeachers->isEnabled());
    QVERIFY(!actions.importClasses->isEnabled());
}

void NavigationTeacherReadTests::
updateActionStatesClassListFailureDisablesClassActionsOnly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher teacher = teacherFixture(
        QStringLiteral("Available Teacher Korean"),
        QStringLiteral("Available Teacher English"),
        QStringLiteral("Available Teacher Display")
        );
    QVERIFY(persistTeacher(services, teacher) > 0);
    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Class List Failure Fixture"),
        teacher.id,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    ActionRegistry actions;
    actions.createActions();
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    controller.connectActions(actions);
    QVERIFY(actions.deleteClass->isEnabled());
    QVERIFY(actions.exportClasses->isEnabled());
    QVERIFY(actions.deleteTeacher->isEnabled());

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
    QSqlQuery dropClasses(services.databaseSession()->database());
    QVERIFY2(dropClasses.exec(QStringLiteral("DROP TABLE classes")),
             qPrintable(dropClasses.lastError().text()));

    const Result<QList<Classroom>> expectedClassesRead =
        services.classService()->classes();
    QVERIFY(!expectedClassesRead);
    QVERIFY(!expectedClassesRead.error().trimmed().isEmpty());
    const Result<QList<Teacher>> expectedTeachersRead =
        services.teacherService()->teachers();
    QVERIFY(expectedTeachersRead);
    QCOMPARE(expectedTeachersRead->size(), 1);
    QCOMPARE(expectedTeachersRead->first().id, teacher.id);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    controller.handleClassInfoSaved(classId);

    QVERIFY(!actions.deleteClass->isEnabled());
    QVERIFY(!actions.exportClasses->isEnabled());
    QVERIFY(actions.deleteTeacher->isEnabled());
    QVERIFY(actions.importClasses->isEnabled());
    QVERIFY(actions.importTeachers->isEnabled());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
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
classListReloadFailureAfterChooserWarnsBeforeConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    int classId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Class before reload failure"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        classId
        );
    QVERIFY(classId > 0);

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    bool classesTableDropped = false;
    QString mutationError;
    const auto dropClassesAfterChooserPopulation = [&]
    {
        QSqlQuery disableForeignKeys(
            services.databaseSession()->database());
        if (!disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")))
        {
            mutationError = disableForeignKeys.lastError().text();
            return;
        }

        QSqlQuery dropClasses(services.databaseSession()->database());
        if (!dropClasses.exec(QStringLiteral("DROP TABLE classes")))
        {
            mutationError = dropClasses.lastError().text();
            return;
        }
        classesTableDropped = true;
    };

    RecordSelectionObservation observation;
    QVERIFY(invokeDeleteClassSelecting(
        controller,
        classId,
        observation,
        dropClassesAfterChooserPopulation
        ));

    QVERIFY(observation.found);
    QVERIFY(observation.selected);
    QVERIFY(observation.accepted);
    QCOMPARE(observation.selectedId, classId);
    QVERIFY2(classesTableDropped, qPrintable(mutationError));

    const auto expectedClassesRead = services.classService()->classes();
    QVERIFY(!expectedClassesRead);
    QVERIFY(!expectedClassesRead.error().trimmed().isEmpty());
    const auto expectedClassRead = services.classService()->classroom(classId);
    QVERIFY(!expectedClassRead);
    QVERIFY(!expectedClassRead.error().trimmed().isEmpty());
    QVERIFY(expectedClassesRead.error() != expectedClassRead.error());

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Delete Class"));
    QCOMPARE(warning.message, QStringLiteral("The class could not be loaded."));
    QCOMPARE(warning.details, expectedClassesRead.error());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void NavigationTeacherReadTests::
selectedClassMissingFromReloadWarnsBeforeConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    int retainedClassId = -1;
    int selectedClassId = -1;
    createClassWithSubtitle(
        services,
        QStringLiteral("Retained class"),
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        retainedClassId
        );
    createClassWithSubtitle(
        services,
        QStringLiteral("Selected class to remove"),
        -1,
        QStringLiteral("E5"),
        QStringLiteral("Vega"),
        QStringLiteral("Tuesday"),
        QStringLiteral("5:00 PM"),
        selectedClassId
        );
    QVERIFY(retainedClassId > 0);
    QVERIFY(selectedClassId > 0);

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    bool selectedClassRemoved = false;
    QString mutationError;
    const auto removeSelectedClassAfterChooserPopulation = [&]
    {
        const Status removed = services.classService()->remove(selectedClassId);
        if (!removed)
        {
            mutationError = removed.error();
            return;
        }
        selectedClassRemoved = true;
    };

    RecordSelectionObservation observation;
    QVERIFY(invokeDeleteClassSelecting(
        controller,
        selectedClassId,
        observation,
        removeSelectedClassAfterChooserPopulation
        ));

    QVERIFY(observation.found);
    QVERIFY(observation.selected);
    QVERIFY(observation.accepted);
    QCOMPARE(observation.selectedId, selectedClassId);
    QCOMPARE(observation.labels.size(), 3);
    QVERIFY2(selectedClassRemoved, qPrintable(mutationError));

    const auto refreshedClasses = services.classService()->classes();
    QVERIFY(refreshedClasses);
    QCOMPARE(refreshedClasses->size(), 1);
    QCOMPARE(refreshedClasses->first().id, retainedClassId);
    QVERIFY(!services.classService()->classroom(selectedClassId));

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Delete Class"));
    QCOMPARE(warning.message, QStringLiteral("The class could not be loaded."));
    QCOMPARE(warning.details, QStringLiteral("The selected class could not be found."));
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
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

void NavigationTeacherReadTests::
upcomingBirthdaysActionShowsEntriesFromAllStaffDirectories()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Teacher koreanTeacher = teacherFixture(
        QStringLiteral("Korean Teacher"),
        QStringLiteral("Korean Teacher"),
        QStringLiteral("Korean Birthday Entry")
        );
    koreanTeacher.birthday = birthdayForOffset(0);
    QVERIFY(persistTeacher(services, koreanTeacher) > 0);

    const auto nativeEnglishSaved =
        services.teacherService()->saveNativeEnglishTeacherDirectory(
            {{
                .name = QStringLiteral("Native English Birthday Entry"),
                .position = QStringLiteral("NET"),
                .birthday = birthdayForOffset(1)
            }},
            {}
            );
    QVERIFY(nativeEnglishSaved);

    const auto gsTeamSaved = services.teacherService()->saveGsTeamDirectory(
        {{
            .name = QStringLiteral("GS Birthday Entry"),
            .position = QStringLiteral("M1"),
            .birthday = birthdayForOffset(2)
        }},
        {}
        );
    QVERIFY(gsTeamSaved);

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    BirthdayDialogObservation observation;
    QVERIFY(invokeUpcomingBirthdays(controller, observation));

    QVERIFY(observation.modalOpened);
    QVERIFY(observation.upcomingBirthdaysOpened);
    QCOMPARE(observation.entryNames.size(), 3);
    QVERIFY(observation.entryNames.contains(
        QStringLiteral("Korean Birthday Entry")));
    QVERIFY(observation.entryNames.contains(
        QStringLiteral("Native English Birthday Entry")));
    QVERIFY(observation.entryNames.contains(
        QStringLiteral("GS Birthday Entry")));
    QCOMPARE(observation.entryDetails.size(), 3);
    const QString details = observation.entryDetails.join(QLatin1Char('\n'));
    QVERIFY(details.contains(QStringLiteral("Korean Teacher")));
    QVERIFY(details.contains(QStringLiteral("Native English Teacher")));
    QVERIFY(details.contains(QStringLiteral("GS Team")));
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
}

void NavigationTeacherReadTests::
upcomingBirthdaysActionWarnsOnKoreanDirectoryFailureFirst()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery dropTeachers(services.databaseSession()->database());
    QVERIFY(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")));
    const QString expectedDetails =
        koreanBirthdayDirectoryReadDetails(services);
    QVERIFY(!expectedDetails.isEmpty());

    QSqlQuery dropNativeEnglish(services.databaseSession()->database());
    QVERIFY(dropNativeEnglish.exec(
        QStringLiteral("DROP TABLE native_english_teachers")));
    QSqlQuery dropGsTeam(services.databaseSession()->database());
    QVERIFY(dropGsTeam.exec(QStringLiteral("DROP TABLE gs_team")));

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    BirthdayDialogObservation observation;
    QVERIFY(invokeUpcomingBirthdays(controller, observation));
    QApplication::processEvents();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest warning = prompts.messages.first();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Upcoming Birthdays"));
    QCOMPARE(warning.message, QStringLiteral("Birthdays could not be loaded."));
    QCOMPARE(warning.details, expectedDetails);
    QVERIFY(!observation.modalOpened);
    QVERIFY(!observation.upcomingBirthdaysOpened);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void NavigationTeacherReadTests::
upcomingBirthdaysActionReturnsSilentlyWithoutAnActiveSession()
{
    ApplicationServices services;
    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    BirthdayDialogObservation observation;
    QVERIFY(invokeUpcomingBirthdays(controller, observation));
    QApplication::processEvents();

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(!observation.modalOpened);
    QVERIFY(!observation.upcomingBirthdaysOpened);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void NavigationTeacherReadTests::
upcomingBirthdaysActionShowsWarningWhenGsDirectoryReadFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery dropGsTeam(services.databaseSession()->database());
    QVERIFY(dropGsTeam.exec(QStringLiteral("DROP TABLE gs_team")));
    const QString expectedDetails = directoryReadDetails(services, false);
    QVERIFY(!expectedDetails.isEmpty());

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    BirthdayDialogObservation observation;
    QVERIFY(invokeUpcomingBirthdays(controller, observation));
    QApplication::processEvents();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest warning = prompts.messages.first();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Upcoming Birthdays"));
    QCOMPARE(warning.message, QStringLiteral("Birthdays could not be loaded."));
    QCOMPARE(warning.details, expectedDetails);
    QVERIFY(warning.details.contains(QStringLiteral("GS Team")));
    QVERIFY(!observation.modalOpened);
    QVERIFY(!observation.upcomingBirthdaysOpened);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void NavigationTeacherReadTests::
upcomingBirthdaysActionPrefersNativeEnglishErrorWhenBothReadsFail()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QSqlQuery dropNativeEnglish(
        services.databaseSession()->database());
    QVERIFY(dropNativeEnglish.exec(
        QStringLiteral("DROP TABLE native_english_teachers")));
    QSqlQuery dropGsTeam(services.databaseSession()->database());
    QVERIFY(dropGsTeam.exec(QStringLiteral("DROP TABLE gs_team")));

    const QString nativeEnglishDetails = directoryReadDetails(services, true);
    const QString gsTeamDetails = directoryReadDetails(services, false);
    QVERIFY(!nativeEnglishDetails.isEmpty());
    QVERIFY(!gsTeamDetails.isEmpty());
    QVERIFY(nativeEnglishDetails != gsTeamDetails);

    PageManager pages;
    Sidebar sidebar;
    SidebarController controller(&services, &sidebar, &pages);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    BirthdayDialogObservation observation;
    QVERIFY(invokeUpcomingBirthdays(controller, observation));
    QApplication::processEvents();

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest warning = prompts.messages.first();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Upcoming Birthdays"));
    QCOMPARE(warning.message, QStringLiteral("Birthdays could not be loaded."));
    QCOMPARE(warning.details, nativeEnglishDetails);
    QVERIFY(warning.details != gsTeamDetails);
    QVERIFY(!observation.modalOpened);
    QVERIFY(!observation.upcomingBirthdaysOpened);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

QTEST_MAIN(NavigationTeacherReadTests)

#include "navigation_teacher_read_tests.moc"
