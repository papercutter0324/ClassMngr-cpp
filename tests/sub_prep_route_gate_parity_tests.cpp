#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "domain/models/teacher.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QJsonArray>
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

constexpr auto UnsavedNotes = "Unsaved F390 teacher notes";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("sub-prep-route-gate-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services, QString* error)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Route Teacher");
    teacher.preferredRomanization = QStringLiteral("Route Teacher");
    teacher.preferredName = QStringLiteral("Route Teacher");

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

    notes->setPlainText(QString::fromLatin1(UnsavedNotes));
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

NavigationData subPrepRoute(
    const bool notesRoute
    )
{
    QStringList path{QStringLiteral("sub_prep")};
    QStringList keys{QStringLiteral("sub_prep")};
    QString routeKey = QStringLiteral("sub_prep");

    if (notesRoute)
    {
        // The controller supports this Notes page key, although the current
        // sidebar does not generate it. F390 covers the supported payload.
        path.append(QStringLiteral("sub_prep_notes"));
        keys.append(QStringLiteral("sub_prep_notes"));
        routeKey = QStringLiteral("sub_prep_notes");
    }

    return {
        .path = path,
        .keys = keys,
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

int unrelatedPromptCount(const FakeUserPromptService& prompts)
{
    return static_cast<int>(
        prompts.messages.size()
        + prompts.asynchronousMessages.size()
        + prompts.confirmations.size()
        + prompts.actionPrompts.size()
        );
}

QJsonArray jsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values)
    {
        array.append(value);
    }
    return array;
}

QJsonObject observation(
    const QString& caseName,
    const NavigationData& route,
    const QString& expectedSection,
    const bool sessionOpen,
    QWidget* const currentPageBeforeRoute,
    PageManager& pages,
    const DirtyTeacherPage& teacherState,
    const FakeUserPromptService& prompts,
    const QVector<QString>& warnings
    )
{
    QJsonObject row;
    row.insert(QStringLiteral("case"), caseName);
    row.insert(QStringLiteral("expectedSection"), expectedSection);
    row.insert(QStringLiteral("sessionOpen"), sessionOpen);
    row.insert(QStringLiteral("routeKey"), route.routeKey);
    row.insert(QStringLiteral("routePath"), jsonArray(route.path));
    row.insert(QStringLiteral("routeKeys"), jsonArray(route.keys));
    row.insert(QStringLiteral("routeType"),
        route.type == NodeType::Page ? QStringLiteral("page")
                                     : QStringLiteral("other"));
    row.insert(QStringLiteral("sameCurrentPagePointer"),
        pages.currentWidget() == currentPageBeforeRoute);
    row.insert(QStringLiteral("currentPage"), pages.currentPageIdentifier());
    row.insert(QStringLiteral("teacherPagePreserved"),
        pages.teacherPage() == teacherState.page);
    row.insert(QStringLiteral("teacherIdentity"),
        teacherState.page->teacher().id);
    row.insert(QStringLiteral("teacherIdentityPreserved"),
        teacherState.page->teacher().id == teacherState.teacherId);
    row.insert(QStringLiteral("teacherNotes"),
        teacherState.notes->toPlainText());
    row.insert(QStringLiteral("teacherDirty"),
        teacherState.page->hasUnsavedChanges());
    row.insert(QStringLiteral("subPrepCreated"),
        pages.subPrepPage() != nullptr);
    row.insert(QStringLiteral("subPrepCurrent"),
        pages.isCurrentPage(PageType::SubPrep));
    row.insert(QStringLiteral("subPrepSection"),
        pages.subPrepPage()
            ? pages.subPrepPage()->currentSectionKey()
            : QString());
    row.insert(QStringLiteral("leaveConfirmCount"),
        prompts.unsavedChangesConfirmations.size());
    row.insert(QStringLiteral("unrelatedPromptCount"),
        unrelatedPromptCount(prompts));
    row.insert(QStringLiteral("qtWarningCount"), warnings.size());
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

void verifySubPrepRoute(
    const QString& caseName,
    const bool notesRoute,
    const bool sessionOpen
    )
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    PageManager pages;
    const auto teacherState = prepareDirtyTeacherPage(
        services,
        pages,
        &error
        );
    QVERIFY2(teacherState.has_value(), qPrintable(error));
    QVERIFY(pages.currentWidget() == teacherState->page);

    ResourcePackManager resources(
        directory.filePath(QStringLiteral("resources")),
        directory.filePath(QStringLiteral("baseline"))
        );
    Sidebar sidebar;
    NavigationController navigation(&services, &sidebar, &pages, resources);
    FakeUserPromptService prompts;
    if (sessionOpen)
    {
        prompts.scriptedUnsavedChangesChoices.enqueue(
            UnsavedChangesChoice::Discard
            );
    }
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    ScopedWarningCapture warningCapture(warnings);

    if (!sessionOpen)
    {
        services.closeDatabase();
        QVERIFY(!services.hasOpenDatabase());
    }

    const NavigationData route = subPrepRoute(notesRoute);
    const QString expectedSection = notesRoute
        ? QStringLiteral("sub_prep_notes")
        : QStringLiteral("sub_prep_important");
    QWidget* const currentPageBeforeRoute = pages.currentWidget();
    navigation.handleNavigation(route);

    const QJsonObject row = observation(
        caseName,
        route,
        expectedSection,
        sessionOpen,
        currentPageBeforeRoute,
        pages,
        *teacherState,
        prompts,
        warnings
        );
    emitTranscript(row);

    QVERIFY2(warnings.isEmpty(),
        qPrintable(warnings.isEmpty() ? QString() : warnings.constFirst()));
    QCOMPARE(unrelatedPromptCount(prompts), 0);

    if (!sessionOpen)
    {
        QVERIFY(pages.currentWidget() == currentPageBeforeRoute);
        QVERIFY(pages.currentWidget() == teacherState->page);
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QVERIFY(pages.teacherPage() == teacherState->page);
        QVERIFY(pages.subPrepPage() == nullptr);
        QVERIFY(!pages.isCurrentPage(PageType::SubPrep));
        QCOMPARE(teacherState->page->teacher().id, teacherState->teacherId);
        QCOMPARE(teacherState->notes->toPlainText(),
            QString::fromLatin1(UnsavedNotes));
        QVERIFY(teacherState->page->hasUnsavedChanges());
        QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
        return;
    }

    QVERIFY(pages.subPrepPage());
    QVERIFY(pages.currentWidget() == pages.subPrepPage());
    QVERIFY(pages.isCurrentPage(PageType::SubPrep));
    QCOMPARE(pages.subPrepPage()->currentSectionKey(), expectedSection);
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(prompts.scriptedUnsavedChangesChoices.isEmpty());
    QVERIFY(pages.teacherPage() == teacherState->page);
    QCOMPARE(teacherState->page->teacher().id, teacherState->teacherId);
    QCOMPARE(teacherState->notes->toPlainText(), QString());
    QVERIFY(!teacherState->page->hasUnsavedChanges());
}

}

class SubPrepRouteGateParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void routeAvailabilityMatrix_data();
    void routeAvailabilityMatrix();
};

void SubPrepRouteGateParityTests::routeAvailabilityMatrix_data()
{
    QTest::addColumn<QString>("caseName");
    QTest::addColumn<bool>("notesRoute");
    QTest::addColumn<bool>("sessionOpen");

    QTest::newRow("sidebar-root-closed")
        << QStringLiteral("sidebar-root-closed")
        << false
        << false;
    QTest::newRow("sidebar-root-open")
        << QStringLiteral("sidebar-root-open")
        << false
        << true;
    QTest::newRow("synthetic-notes-closed")
        << QStringLiteral("synthetic-notes-closed")
        << true
        << false;
    QTest::newRow("synthetic-notes-open")
        << QStringLiteral("synthetic-notes-open")
        << true
        << true;
}

void SubPrepRouteGateParityTests::routeAvailabilityMatrix()
{
    QFETCH(QString, caseName);
    QFETCH(bool, notesRoute);
    QFETCH(bool, sessionOpen);

    verifySubPrepRoute(caseName, notesRoute, sessionOpen);
}

QTEST_MAIN(SubPrepRouteGateParityTests)

#include "sub_prep_route_gate_parity_tests.moc"
