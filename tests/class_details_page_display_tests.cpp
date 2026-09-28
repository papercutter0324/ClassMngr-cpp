#include "core/application_services.h"
#include "data/database/database_session.h"
#include "app/services/feature_services.h"
#include "features/classes/ui/class_details_page.h"
#include "next/application/class_details_save_use_case.h"
#include "next/application/class_details_page_read_port.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sections/class_details_section.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"

#include <QComboBox>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-display-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto result = services.classService()->create(name);
    return result ? *result : -1;
}

Application::ClassDetailsPageReadSnapshot makeSnapshot(
    const int id,
    Application::ClassDetailsPageFields fields,
    std::string teacherName,
    const int studentCount
    )
{
    return {
        classId(id),
        Domain::Result<Application::ClassDetailsPageFields>::success(
            std::move(fields)
            ),
        Domain::Result<std::string>::success(std::move(teacherName)),
        Domain::Result<int>::success(studentCount)
    };
}

class FakeReadPort final : public Application::ClassDetailsPageReadPort
{
public:
    Domain::Result<Application::ClassDetailsPageReadSnapshot> result =
        Domain::Result<Application::ClassDetailsPageReadSnapshot>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured test read port",
            .recoverable = false
        });
    std::vector<Application::ClassDetailsPageReadResult> responses;
    std::vector<Domain::ClassId> requests;

    [[nodiscard]] Application::ClassDetailsPageReadResult
    readClassDetailsPage(const Domain::ClassId& id) override
    {
        requests.push_back(id);
        const std::size_t responseIndex = requests.size() - 1;
        if (responseIndex < responses.size())
        {
            return responses[responseIndex];
        }
        return result;
    }
};

class FakeSavePort final : public Application::ClassDetailsSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return Domain::Result<void>::success();
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassDetailsSaveRequest> lastRequest;
};

Application::ClassDetailsPageFields firstFields()
{
    Application::ClassDetailsPageFields fields;
    fields.classGrade = "E4";
    fields.classLevel = "Theseus";
    fields.readingBook = "Reading Explorer 1";
    fields.essayBook = "4A";
    fields.classColor = "#112233";
    fields.fontColor = "#445566";
    fields.regularSchedule = {
        {"Friday", "3:00 PM", "3:55 PM"},
        {"Monday", "9:00 AM", "9:55 AM"}
    };
    fields.intensiveSchedule = {
        {"Tuesday", "12:00 PM", "12:55 PM"}
    };
    return fields;
}

QLineEdit* studentCountEdit(ClassDetailsPage& page)
{
    const auto edits = page.findChildren<QLineEdit*>();
    return edits.isEmpty() ? nullptr : edits.front();
}

}

class ClassDetailsPageDisplayTests final : public QObject
{
    Q_OBJECT

private slots:
    void loadRetranslateDiscardAndClearUseSnapshot();
    void independentFailuresUseThePageFallbacks();
    void successfulSaveRefreshesTeacherAndKeepsSavedFields();
};

void ClassDetailsPageDisplayTests::loadRetranslateDiscardAndClearUseSnapshot()
{
    ApplicationServices services;
    FakeReadPort readPort;
    readPort.result = Application::ClassDetailsPageReadResult::success(
        makeSnapshot(42, firstFields(), "Teacher One", 18)
        );

    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        nullptr,
        &readPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Selected Fallback"), 42));

    auto* details = page.findChild<ClassDetailsSection*>();
    auto* schedule = page.findChild<ClassScheduleSection*>();
    auto* header = page.findChild<PageHeader*>();
    auto* countEdit = studentCountEdit(page);
    QVERIFY(details);
    QVERIFY(schedule);
    QVERIFY(header);
    QVERIFY(countEdit);

    QCOMPARE(details->grade(), QStringLiteral("E4"));
    QCOMPARE(details->level(), QStringLiteral("Theseus"));
    QCOMPARE(details->readingBook(), QStringLiteral("Reading Explorer 1"));
    QCOMPARE(details->essayBook(), QStringLiteral("4A"));
    QCOMPARE(details->classColor(), QStringLiteral("#112233"));
    QCOMPARE(details->fontColor(), QStringLiteral("#445566"));
    QCOMPARE(countEdit->text(), QStringLiteral("18"));
    QCOMPARE(schedule->regularRows().size(), 2);
    QCOMPARE(schedule->intensiveRows().size(), 1);
    QCOMPARE(schedule->regularRows().at(0)->day(), QStringLiteral("Friday"));
    QCOMPARE(schedule->regularRows().at(0)->startTime(),
             QStringLiteral("3:00 PM"));
    QCOMPARE(schedule->regularRows().at(1)->day(), QStringLiteral("Monday"));
    QCOMPARE(schedule->regularRows().at(1)->startTime(),
             QStringLiteral("9:00 AM"));
    QCOMPARE(schedule->intensiveRows().at(0)->day(), QStringLiteral("Tuesday"));
    QVERIFY(header->subtitle().contains(QStringLiteral("E4 Theseus")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Teacher One")));
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(readPort.requests.size(), std::size_t(1));

    details->gradeEditor()->setCurrentText(QStringLiteral("E5"));
    QVERIFY(page.hasUnsavedChanges());

    auto revisedFields = firstFields();
    revisedFields.classGrade = "E5";
    revisedFields.classLevel = "Artemis";
    revisedFields.readingBook = "Reading Explorer 2";
    revisedFields.essayBook.clear();
    readPort.result = Application::ClassDetailsPageReadResult::success(
        makeSnapshot(42, std::move(revisedFields), "Teacher Two", 21)
        );
    page.retranslateUi();

    QVERIFY(header->subtitle().contains(QStringLiteral("E5 Artemis")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Teacher Two")));
    QCOMPARE(details->grade(), QStringLiteral("E5"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(readPort.requests.size(), std::size_t(2));

    page.discardChanges();
    QCOMPARE(details->grade(), QStringLiteral("E5"));
    QCOMPARE(details->level(), QStringLiteral("Artemis"));
    QCOMPARE(countEdit->text(), QStringLiteral("21"));
    QCOMPARE(schedule->regularRows().size(), 2);
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(readPort.requests.size(), std::size_t(3));

    page.clearDatabaseState();
    QVERIFY(details->grade().isEmpty());
    QCOMPARE(countEdit->text(), QStringLiteral("0"));
    QVERIFY(schedule->regularRows().isEmpty());
    QVERIFY(schedule->intensiveRows().isEmpty());
    QCOMPARE(header->subtitle(), QStringLiteral("No class selected"));
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(readPort.requests.size(), std::size_t(3));
}

void ClassDetailsPageDisplayTests::independentFailuresUseThePageFallbacks()
{
    ApplicationServices services;
    FakeReadPort readPort;
    readPort.result = Application::ClassDetailsPageReadResult::success({
        classId(73),
        Domain::Result<Application::ClassDetailsPageFields>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "class information unavailable",
            .recoverable = true
        }),
        Domain::Result<std::string>::success("Teacher name still read"),
        Domain::Result<int>::success(9)
    });

    ClassDetailsPage page(&services, false, nullptr, nullptr, &readPort);
    page.loadClass(Classroom(QStringLiteral("Selected Fallback"), 73));
    auto* details = page.findChild<ClassDetailsSection*>();
    auto* schedule = page.findChild<ClassScheduleSection*>();
    auto* header = page.findChild<PageHeader*>();
    auto* countEdit = studentCountEdit(page);
    QVERIFY(details && schedule && header && countEdit);

    QVERIFY(details->grade().isEmpty());
    QCOMPARE(details->classColor(), QStringLiteral("#FFFFFF"));
    QCOMPARE(details->fontColor(), QStringLiteral("#000000"));
    QCOMPARE(countEdit->text(), QStringLiteral("9"));
    QVERIFY(schedule->regularRows().isEmpty());
    QCOMPARE(header->subtitle(), QStringLiteral("Selected Fallback"));

    Application::ClassDetailsPageFields fields = firstFields();
    readPort.result = Application::ClassDetailsPageReadResult::success({
        classId(73),
        Domain::Result<Application::ClassDetailsPageFields>::success(
            std::move(fields)
            ),
        Domain::Result<std::string>::failure({
            .code = Domain::ErrorCode::NotFound,
            .message = "teacher unavailable",
            .recoverable = false
        }),
        Domain::Result<int>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "roster unavailable",
            .recoverable = true
        })
    });
    page.loadClass(Classroom(QStringLiteral("Selected Fallback"), 73));

    QCOMPARE(details->grade(), QStringLiteral("E4"));
    QCOMPARE(countEdit->text(), QStringLiteral("0"));
    QVERIFY(header->subtitle().contains(QStringLiteral("E4 Theseus")));
    QVERIFY(header->subtitle().contains(QStringLiteral("No Teacher")));
    QVERIFY(!page.hasUnsavedChanges());
}

void ClassDetailsPageDisplayTests::successfulSaveRefreshesTeacherAndKeepsSavedFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int databaseClassId = createClass(
        services,
        QStringLiteral("Class Details Display Save")
        );
    QVERIFY(databaseClassId > 0);

    FakeReadPort readPort;
    auto staleFields = firstFields();
    readPort.responses = {
        Application::ClassDetailsPageReadResult::success(
            makeSnapshot(databaseClassId, firstFields(), "Teacher One", 18)
            ),
        Application::ClassDetailsPageReadResult::success(
            makeSnapshot(databaseClassId, std::move(staleFields), "Teacher Two", 21)
            )
    };
    FakeSavePort savePort;
    ClassDetailsPage page(
        &services,
        false,
        nullptr,
        &savePort,
        &readPort
        );
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Display Save"), databaseClassId)
        );

    auto* details = page.findChild<ClassDetailsSection*>();
    auto* schedule = page.findChild<ClassScheduleSection*>();
    auto* header = page.findChild<PageHeader*>();
    auto* countEdit = studentCountEdit(page);
    QVERIFY(details && schedule && header && countEdit);

    details->gradeEditor()->setCurrentText(QStringLiteral("E5"));
    details->levelEditor()->setCurrentText(QStringLiteral("Artemis"));
    details->readingBookEditor()->setCurrentText(
        QStringLiteral("Reading Explorer 2")
        );
    details->essayBookEditor()->setCurrentText(QStringLiteral("5A"));
    QVERIFY(page.hasUnsavedChanges());

    QVERIFY(page.saveChanges());

    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.has_value());
    QCOMPARE(details->grade(), QStringLiteral("E5"));
    QCOMPARE(details->level(), QStringLiteral("Artemis"));
    QCOMPARE(details->readingBook(), QStringLiteral("Reading Explorer 2"));
    QCOMPARE(details->essayBook(), QStringLiteral("5A"));
    QCOMPARE(countEdit->text(), QStringLiteral("18"));
    QCOMPARE(schedule->regularRows().size(), 2);
    QCOMPARE(schedule->intensiveRows().size(), 1);
    QVERIFY(header->subtitle().contains(QStringLiteral("E5 Artemis")));
    QVERIFY(header->subtitle().contains(QStringLiteral("Teacher Two")));
    QVERIFY(!header->subtitle().contains(QStringLiteral("Teacher One")));
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(readPort.requests.size(), std::size_t(2));
}

QTEST_MAIN(ClassDetailsPageDisplayTests)

#include "class_details_page_display_tests.moc"
