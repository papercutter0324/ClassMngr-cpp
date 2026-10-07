#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/classes_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstdio>
#include <optional>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("navigation-roster-session-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services, QString* error)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Roster State Teacher");
    teacher.preferredRomanization = QStringLiteral("Roster State Teacher");
    teacher.preferredName = QStringLiteral("Roster State Teacher");
    teacher.roomNumber = QStringLiteral("Room 8");
    teacher.phoneNumber = QStringLiteral("010-9876-5432");
    teacher.wifiName = QStringLiteral("Roster Network");
    teacher.wifiPassword = QStringLiteral("Roster Password");
    teacher.internetType = QStringLiteral("LAN");
    teacher.zoomId = QStringLiteral("roster.zoom");
    teacher.zoomPassword = QStringLiteral("Roster Zoom Password");
    teacher.projectionType = QStringLiteral("HDMI");

    const auto created = services.teacherService()->create(teacher);
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return -1;
    }
    return *created;
}

int createScheduledClass(ApplicationServices& services, QString* error)
{
    const auto created = services.classService()->create(
        QStringLiteral("Roster Route Class")
        );
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return -1;
    }

    auto info = services.classService()->classInfo(*created);
    if (!info)
    {
        if (error)
        {
            *error = info.error();
        }
        return -1;
    }

    info->classGrade = QStringLiteral("E4");
    info->classLevel = QStringLiteral("Theseus");
    info->classTimes = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        }
    };
    const auto saved = services.classService()->saveClassInfo(*info);
    if (!saved)
    {
        if (error)
        {
            *error = saved.error();
        }
        return -1;
    }
    return *created;
}

struct DirtyTeacherPage final
{
    int teacherId = -1;
    TeacherInfoPage* page = nullptr;
    QTextEdit* notes = nullptr;
};

std::optional<DirtyTeacherPage> prepareDirtyTeacherPage(
    ApplicationServices& services,
    PageManager& pages,
    const QString& notesText,
    QString* error
    )
{
    const int teacherId = createTeacher(services, error);
    if (teacherId <= 0)
    {
        return std::nullopt;
    }

    const auto teacher = services.teacherService()->teacher(teacherId);
    if (!teacher)
    {
        if (error)
        {
            *error = QStringLiteral("Created teacher could not be reloaded.");
        }
        return std::nullopt;
    }

    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    TeacherInfoPage* const page = pages.teacherPage();
    if (!page)
    {
        if (error)
        {
            *error = QStringLiteral("Teacher page was not initialized.");
        }
        return std::nullopt;
    }

    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(*teacher);
    QTextEdit* const notes = page->findChild<QTextEdit*>(
        QStringLiteral("teacherNotesEdit")
        );
    if (!notes)
    {
        if (error)
        {
            *error = QStringLiteral("Teacher notes editor was not found.");
        }
        return std::nullopt;
    }

    notes->setPlainText(notesText);
    if (!page->hasUnsavedChanges())
    {
        if (error)
        {
            *error = QStringLiteral("Teacher notes edit was not marked dirty.");
        }
        return std::nullopt;
    }

    return DirtyTeacherPage{
        .teacherId = teacherId,
        .page = page,
        .notes = notes
    };
}

NavigationData rosterRoute(const int classId)
{
    return {
        .path = {
            QStringLiteral("classes"),
            QStringLiteral("roster")
        },
        .keys = {
            QStringLiteral("classes"),
            QStringLiteral("class_roster")
        },
        .routeKey = QStringLiteral("class_roster"),
        .type = NodeType::Page,
        .classId = classId
    };
}

QVector<QString>* capturedWarnings = nullptr;

void captureQtWarning(
    const QtMsgType type,
    const QMessageLogContext&,
    const QString& message
    )
{
    if (capturedWarnings
        && (type == QtWarningMsg || type == QtCriticalMsg))
    {
        capturedWarnings->append(message);
    }
}

class ScopedWarningCapture final
{
public:
    explicit ScopedWarningCapture(QVector<QString>& warnings)
        : m_previous(qInstallMessageHandler(captureQtWarning))
    {
        capturedWarnings = &warnings;
    }

    ~ScopedWarningCapture()
    {
        qInstallMessageHandler(m_previous);
        capturedWarnings = nullptr;
    }

    ScopedWarningCapture(const ScopedWarningCapture&) = delete;
    ScopedWarningCapture& operator=(const ScopedWarningCapture&) = delete;

private:
    QtMessageHandler m_previous = nullptr;
};

class ScopedPromptService final
{
public:
    explicit ScopedPromptService(IUserPromptService& service)
    {
        DialogServices::setUserPromptServiceForTesting(&service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    ScopedPromptService(const ScopedPromptService&) = delete;
    ScopedPromptService& operator=(const ScopedPromptService&) = delete;
};

int promptCount(const FakeUserPromptService& prompts)
{
    return static_cast<int>(
        prompts.messages.size()
        + prompts.asynchronousMessages.size()
        + prompts.confirmations.size()
        + prompts.unsavedChangesConfirmations.size()
        + prompts.actionPrompts.size()
        );
}

QJsonObject routeObservation(
    const QString& scenario,
    const int requestedClassId,
    PageManager& pages,
    const DirtyTeacherPage& teacherState,
    const FakeUserPromptService& prompts,
    const QVector<QString>& warnings
    )
{
    QJsonObject row;
    row.insert(QStringLiteral("case"), scenario);
    row.insert(QStringLiteral("isTeacherInfo"),
        pages.isCurrentPage(PageType::TeacherInfo));
    row.insert(QStringLiteral("isClasses"),
        pages.isCurrentPage(PageType::Classes));
    row.insert(QStringLiteral("classesPageCreated"),
        pages.classesPage() != nullptr);
    row.insert(QStringLiteral("requestedClassId"), requestedClassId);
    row.insert(QStringLiteral("teacherId"), teacherState.page->teacher().id);
    row.insert(QStringLiteral("teacherNotes"),
        teacherState.notes->toPlainText());
    row.insert(QStringLiteral("teacherDirty"),
        teacherState.page->hasUnsavedChanges());
    const ClassesPage* const classes = pages.classesPage();
    row.insert(QStringLiteral("currentPageClassId"),
        classes ? classes->currentClassId() : -1);
    row.insert(QStringLiteral("rosterSection"),
        classes
            && classes->currentSection() == ClassesSection::Roster);
    row.insert(QStringLiteral("promptCount"), promptCount(prompts));
    row.insert(QStringLiteral("leaveConfirmCount"),
        prompts.unsavedChangesConfirmations.size());
    row.insert(QStringLiteral("qtWarningCount"),
        static_cast<int>(warnings.size()));
    return row;
}

void emitTranscript(const QJsonObject& row)
{
    QByteArray bytes = QJsonDocument(row).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    std::fwrite(
        bytes.constData(),
        1,
        static_cast<std::size_t>(bytes.size()),
        stdout
        );
    std::fflush(stdout);
}

}

class NavigationRosterSessionParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void closedSessionKeepsDirtyCurrentPageQuiet();
    void openSessionRoutesToRosterAfterLeaveConfirmation();
    void invalidClassIdReturnsBeforeLeaveConfirmation();
};

void NavigationRosterSessionParityTests::
closedSessionKeepsDirtyCurrentPageQuiet()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    const int classId = createScheduledClass(services, &error);
    QVERIFY2(classId > 0, qPrintable(error));
    PageManager pages;
    auto teacherState = prepareDirtyTeacherPage(
        services,
        pages,
        QStringLiteral("Unsaved current page state"),
        &error
        );
    QVERIFY2(teacherState.has_value(), qPrintable(error));

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    ScopedWarningCapture warningCapture(warnings);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    navigation.handleNavigation(rosterRoute(classId));

    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QVERIFY(!pages.isCurrentPage(PageType::Classes));
    QVERIFY(!pages.classesPage());
    QCOMPARE(teacherState->page->teacher().id, teacherState->teacherId);
    QCOMPARE(teacherState->notes->toPlainText(),
        QStringLiteral("Unsaved current page state"));
    QVERIFY(teacherState->page->hasUnsavedChanges());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(warnings.isEmpty());

    emitTranscript(routeObservation(
        QStringLiteral("closed-session-valid-roster"),
        classId,
        pages,
        *teacherState,
        prompts,
        warnings
        ));
}

void NavigationRosterSessionParityTests::
openSessionRoutesToRosterAfterLeaveConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    const int classId = createScheduledClass(services, &error);
    QVERIFY2(classId > 0, qPrintable(error));
    PageManager pages;
    auto teacherState = prepareDirtyTeacherPage(
        services,
        pages,
        QStringLiteral("Unsaved before roster navigation"),
        &error
        );
    QVERIFY2(teacherState.has_value(), qPrintable(error));

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    ScopedWarningCapture warningCapture(warnings);

    navigation.handleNavigation(rosterRoute(classId));

    emitTranscript(routeObservation(
        QStringLiteral("open-session-valid-roster"),
        classId,
        pages,
        *teacherState,
        prompts,
        warnings
        ));

    QVERIFY(pages.isCurrentPage(PageType::Classes));
    QVERIFY(pages.classesPage());
    QCOMPARE(pages.classesPage()->currentClassId(), classId);
    QCOMPARE(pages.classesPage()->currentSection(), ClassesSection::Roster);
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(!teacherState->page->hasUnsavedChanges());
    QVERIFY(warnings.isEmpty());
}

void NavigationRosterSessionParityTests::
invalidClassIdReturnsBeforeLeaveConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    PageManager pages;
    auto teacherState = prepareDirtyTeacherPage(
        services,
        pages,
        QStringLiteral("Unsaved before invalid route"),
        &error
        );
    QVERIFY2(teacherState.has_value(), qPrintable(error));

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    ScopedWarningCapture warningCapture(warnings);

    navigation.handleNavigation(rosterRoute(0));

    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QVERIFY(!pages.classesPage());
    QCOMPARE(teacherState->page->teacher().id, teacherState->teacherId);
    QCOMPARE(teacherState->notes->toPlainText(),
        QStringLiteral("Unsaved before invalid route"));
    QVERIFY(teacherState->page->hasUnsavedChanges());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(warnings.isEmpty());

    emitTranscript(routeObservation(
        QStringLiteral("invalid-class-id"),
        0,
        pages,
        *teacherState,
        prompts,
        warnings
        ));
}

QTEST_MAIN(NavigationRosterSessionParityTests)

#include "navigation_roster_session_parity_tests.moc"
