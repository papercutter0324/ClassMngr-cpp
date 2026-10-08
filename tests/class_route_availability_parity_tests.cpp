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

#include <QComboBox>
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

constexpr auto UnsavedNotes = "Unsaved route-matrix notes";

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-route-availability-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services, QString* error)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Route Matrix Teacher");
    teacher.preferredRomanization = QStringLiteral("Route Matrix Teacher");
    teacher.preferredName = QStringLiteral("Route Matrix Teacher");

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

int createClass(ApplicationServices& services, QString* error)
{
    const auto created = services.classService()->create(
        QStringLiteral("Route Matrix Class")
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

NavigationData classRoute(
    const QString& routeKey,
    const int classId
    )
{
    QStringList keys{
        QStringLiteral("classes")
    };
    QStringList path{
        QStringLiteral("classes")
    };

    if (routeKey == QStringLiteral("speaking_winter"))
    {
        keys.append(QStringLiteral("student_evaluations"));
        path.append(QStringLiteral("student_evaluations"));
    }
    else
    {
        keys.append(routeKey);
        path.append(routeKey);
    }

    return {
        .path = path,
        .keys = keys,
        .routeKey = routeKey,
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

QJsonObject observation(
    const QString& route,
    const QString& expectedSection,
    const bool sessionOpen,
    const int classId,
    PageManager& pages,
    const DirtyTeacherPage& teacherState,
    const FakeUserPromptService& prompts,
    const QVector<QString>& warnings
    )
{
    QJsonObject row;
    row.insert(QStringLiteral("route"), route);
    row.insert(QStringLiteral("expectedSection"), expectedSection);
    row.insert(QStringLiteral("sessionOpen"), sessionOpen);
    row.insert(QStringLiteral("requestedClassId"), classId);
    row.insert(QStringLiteral("teacherInfoCurrent"),
        pages.isCurrentPage(PageType::TeacherInfo));
    row.insert(QStringLiteral("classesCurrent"),
        pages.isCurrentPage(PageType::Classes));
    row.insert(QStringLiteral("classesPageCreated"),
        pages.classesPage() != nullptr);
    row.insert(QStringLiteral("teacherId"), teacherState.page->teacher().id);
    row.insert(QStringLiteral("teacherNotes"), teacherState.notes->toPlainText());
    row.insert(QStringLiteral("teacherDirty"),
        teacherState.page->hasUnsavedChanges());

    const ClassesPage* const classes = pages.classesPage();
    row.insert(QStringLiteral("currentClassId"),
        classes ? classes->currentClassId() : -1);
    row.insert(QStringLiteral("detailsSection"),
        classes && classes->currentSection() == ClassesSection::Details);
    row.insert(QStringLiteral("notesSection"),
        classes && classes->currentSection() == ClassesSection::Notes);
    row.insert(QStringLiteral("evaluationsSection"),
        classes && classes->currentSection() == ClassesSection::Evaluations);

    QString selectedEvaluation;
    if (classes)
    {
        if (const QComboBox* const combo = classes->findChild<QComboBox*>(
                QStringLiteral("classEvaluationsEvaluationCombo")))
        {
            selectedEvaluation = combo->currentData().toString();
        }
    }
    row.insert(QStringLiteral("selectedEvaluation"), selectedEvaluation);
    row.insert(QStringLiteral("leaveConfirmCount"),
        prompts.unsavedChangesConfirmations.size());
    row.insert(QStringLiteral("otherPromptCount"),
        prompts.messages.size()
            + prompts.asynchronousMessages.size()
            + prompts.confirmations.size()
            + prompts.actionPrompts.size());
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

}

class ClassRouteAvailabilityParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void routeAvailabilityMatrix_data();
    void routeAvailabilityMatrix();
};

void ClassRouteAvailabilityParityTests::routeAvailabilityMatrix_data()
{
    QTest::addColumn<QString>("route");
    QTest::addColumn<QString>("expectedSection");
    QTest::addColumn<bool>("sessionOpen");

    QTest::newRow("details-closed")
        << QStringLiteral("class_details")
        << QStringLiteral("details")
        << false;
    QTest::newRow("details-open")
        << QStringLiteral("class_details")
        << QStringLiteral("details")
        << true;
    QTest::newRow("notes-closed")
        << QStringLiteral("class_notes")
        << QStringLiteral("notes")
        << false;
    QTest::newRow("notes-open")
        << QStringLiteral("class_notes")
        << QStringLiteral("notes")
        << true;
    QTest::newRow("student-evaluation-closed")
        << QStringLiteral("speaking_winter")
        << QStringLiteral("evaluations")
        << false;
    QTest::newRow("student-evaluation-open")
        << QStringLiteral("speaking_winter")
        << QStringLiteral("evaluations")
        << true;
}

void ClassRouteAvailabilityParityTests::routeAvailabilityMatrix()
{
    QFETCH(QString, route);
    QFETCH(QString, expectedSection);
    QFETCH(bool, sessionOpen);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    QString error;
    const int classId = createClass(services, &error);
    QVERIFY2(classId > 0, qPrintable(error));
    PageManager pages;
    const auto teacherState = prepareDirtyTeacherPage(
        services,
        pages,
        &error
        );
    QVERIFY2(teacherState.has_value(), qPrintable(error));

    NavigationController navigation(
        &services,
        nullptr,
        &pages,
        ResourcePackManager::instance()
        );
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

    navigation.handleNavigation(classRoute(route, classId));

    const QJsonObject row = observation(
        route,
        expectedSection,
        sessionOpen,
        classId,
        pages,
        *teacherState,
        prompts,
        warnings
        );
    emitTranscript(row);

    QVERIFY(warnings.isEmpty());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());

    if (!sessionOpen)
    {
        QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
        QVERIFY(!pages.isCurrentPage(PageType::Classes));
        QVERIFY(!pages.classesPage());
        QCOMPARE(teacherState->page->teacher().id, teacherState->teacherId);
        QCOMPARE(teacherState->notes->toPlainText(),
            QString::fromLatin1(UnsavedNotes));
        QVERIFY(teacherState->page->hasUnsavedChanges());
        QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
        QVERIFY(!ResourcePackManager::instance().isMounted(
            QStringLiteral("templates")));
        return;
    }

    QVERIFY(pages.isCurrentPage(PageType::Classes));
    QVERIFY(pages.classesPage());
    QCOMPARE(pages.classesPage()->currentClassId(), classId);
    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    QVERIFY(!teacherState->page->hasUnsavedChanges());

    if (expectedSection == QStringLiteral("details"))
    {
        QCOMPARE(pages.classesPage()->currentSection(), ClassesSection::Details);
        QVERIFY(!ResourcePackManager::instance().isMounted(
            QStringLiteral("templates")));
    }
    else if (expectedSection == QStringLiteral("notes"))
    {
        QCOMPARE(pages.classesPage()->currentSection(), ClassesSection::Notes);
        QVERIFY(!ResourcePackManager::instance().isMounted(
            QStringLiteral("templates")));
    }
    else
    {
        QCOMPARE(pages.classesPage()->currentSection(),
            ClassesSection::Evaluations);
        const QComboBox* const evaluationCombo =
            pages.classesPage()->findChild<QComboBox*>(
                QStringLiteral("classEvaluationsEvaluationCombo")
                );
        QVERIFY(evaluationCombo);
        QCOMPARE(evaluationCombo->currentData().toString(),
            QStringLiteral("Winter"));
        QVERIFY(ResourcePackManager::instance().isMounted(
            QStringLiteral("templates")));
    }
}

QTEST_MAIN(ClassRouteAvailabilityParityTests)

#include "class_route_availability_parity_tests.moc"
