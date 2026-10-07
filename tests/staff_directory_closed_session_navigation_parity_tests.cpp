#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

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
        QStringLiteral("staff-directory-closed-session-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services, QString* error)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Frost Teacher");
    teacher.preferredRomanization = QStringLiteral("Frost Teacher");
    teacher.preferredName = QStringLiteral("Frost Teacher");
    teacher.roomNumber = QStringLiteral("Room 8");
    teacher.phoneNumber = QStringLiteral("010-9876-5432");
    teacher.wifiName = QStringLiteral("F386 Network");
    teacher.wifiPassword = QStringLiteral("F386 Password");
    teacher.internetType = QStringLiteral("LAN");
    teacher.zoomId = QStringLiteral("f386.zoom");
    teacher.zoomPassword = QStringLiteral("F386 Zoom Password");
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

NavigationData staffDirectoryRoute(const QString& routeKey)
{
    return {
        .path = {
            QStringLiteral("Teacher"),
            routeKey == QStringLiteral("native_english_teachers")
                ? QStringLiteral("Native English Teachers")
                : QStringLiteral("GS Team")
        },
        .keys = {routeKey},
        .routeKey = routeKey,
        .type = NodeType::Page
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

void emitTranscript(
    const QString& routeKey,
    const ApplicationServices& services,
    PageManager& pages,
    const DirtyTeacherPage& teacherState,
    const QString& expectedNotes,
    const FakeUserPromptService& prompts,
    const QVector<QString>& warnings
    )
{
    const PageType targetPageType =
        routeKey == QStringLiteral("native_english_teachers")
            ? PageType::NativeEnglishTeachers
            : PageType::GsTeam;
    const StaffDirectoryPage* const targetPage =
        routeKey == QStringLiteral("native_english_teachers")
            ? pages.nativeEnglishTeachersPage()
            : pages.gsTeamPage();

    QJsonObject row;
    row.insert(QStringLiteral("case"), routeKey);
    row.insert(QStringLiteral("workspaceOpen"), services.hasOpenDatabase());
    row.insert(QStringLiteral("teacherServiceAvailable"),
        services.teacherService()->isAvailable());
    row.insert(QStringLiteral("currentPageTeacherInfo"),
        pages.isCurrentPage(PageType::TeacherInfo));
    row.insert(QStringLiteral("requestedPageCreated"),
        targetPage != nullptr);
    row.insert(QStringLiteral("requestedPageCurrent"),
        pages.isCurrentPage(targetPageType));
    row.insert(QStringLiteral("teacherIdentityPreserved"),
        teacherState.page->teacher().id == teacherState.teacherId);
    row.insert(QStringLiteral("teacherNotes"),
        teacherState.notes->toPlainText());
    row.insert(QStringLiteral("teacherNotesPreserved"),
        teacherState.notes->toPlainText() == expectedNotes);
    row.insert(QStringLiteral("teacherDirty"),
        teacherState.page->hasUnsavedChanges());
    row.insert(QStringLiteral("promptCount"), promptCount(prompts));
    row.insert(QStringLiteral("leaveConfirmCount"),
        prompts.unsavedChangesConfirmations.size());
    row.insert(QStringLiteral("qtWarningCount"),
        static_cast<int>(warnings.size()));
    QByteArray bytes = QJsonDocument(row).toJson(QJsonDocument::Compact);
    bytes.append(char(10));
    std::fwrite(
        bytes.constData(),
        1,
        static_cast<std::size_t>(bytes.size()),
        stdout
        );
    std::fflush(stdout);
}

void verifyClosedSessionRoute(const QString& routeKey)
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const QString notesText = QStringLiteral(
        "Unsaved F386 current page content"
        );
    QString error;
    PageManager pages;
    auto teacherState = prepareDirtyTeacherPage(
        services,
        pages,
        notesText,
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
    QVERIFY(!services.teacherService()->isAvailable());
    navigation.handleNavigation(staffDirectoryRoute(routeKey));

    const PageType targetPageType =
        routeKey == QStringLiteral("native_english_teachers")
            ? PageType::NativeEnglishTeachers
            : PageType::GsTeam;
    const StaffDirectoryPage* const targetPage =
        routeKey == QStringLiteral("native_english_teachers")
            ? pages.nativeEnglishTeachersPage()
            : pages.gsTeamPage();
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QVERIFY(!pages.isCurrentPage(targetPageType));
    QVERIFY(!targetPage);
    QCOMPARE(teacherState->page->teacher().id, teacherState->teacherId);
    QCOMPARE(teacherState->notes->toPlainText(), notesText);
    QVERIFY(teacherState->page->hasUnsavedChanges());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
    QVERIFY(warnings.isEmpty());

    emitTranscript(
        routeKey,
        services,
        pages,
        *teacherState,
        notesText,
        prompts,
        warnings
        );
}

}

class StaffDirectoryClosedSessionNavigationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void closedSessionKeepsDirtyPageForNativeEnglishRoute();
    void closedSessionKeepsDirtyPageForGsTeamRoute();
};

void StaffDirectoryClosedSessionNavigationParityTests::
closedSessionKeepsDirtyPageForNativeEnglishRoute()
{
    verifyClosedSessionRoute(QStringLiteral("native_english_teachers"));
}

void StaffDirectoryClosedSessionNavigationParityTests::
closedSessionKeepsDirtyPageForGsTeamRoute()
{
    verifyClosedSessionRoute(QStringLiteral("gs_team"));
}

QTEST_MAIN(StaffDirectoryClosedSessionNavigationParityTests)

#include "staff_directory_closed_session_navigation_parity_tests.moc"
