#include "next/application/class_co_teacher_teacher_choices_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Domain::TeacherId teacherId(const std::string& value)
{
    return *Domain::TeacherId::fromString(value);
}

class RecordingTeacherChoicesPort final
    : public Application::ClassCoTeacherTeacherChoicesReadPort
{
public:
    [[nodiscard]] Application::ClassCoTeacherTeacherChoicesResult
    readClassCoTeacherTeacherChoices() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    Application::ClassCoTeacherTeacherChoicesResult result =
        Application::ClassCoTeacherTeacherChoicesResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured teacher choices read",
            .recoverable = false
        });
};

}

class NextApplicationClassCoTeacherTeacherChoicesReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void callsPortOnceAndPreservesTypedTeacherFields();
    void propagatesPortFailure();
    void rejectsInvalidTeacherIdentities();
};

void NextApplicationClassCoTeacherTeacherChoicesReadQueryTests::
callsPortOnceAndPreservesTypedTeacherFields()
{
    RecordingTeacherChoicesPort port;
    Application::ClassCoTeacherTeacherChoicesSnapshot expected;
    expected.teachers.push_back({
        .teacherId = teacherId("42"),
        .teacherKr = u"\uAE40\uBBFC\uC900",
        .teacherEn = u"Minjun Kim",
        .roomNumber = u"Room 7",
        .internetType = u"Ethernet",
        .wifiName = u"Classroom WiFi",
        .wifiPassword = u"secret",
        .projectionType = u"HDMI",
        .zoomId = u"zoom-room",
        .zoomPassword = u"zoom-secret"
    });
    port.result = Application::ClassCoTeacherTeacherChoicesResult::success(
        expected
        );
    const Application::ClassCoTeacherTeacherChoicesReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == expected);
}

void NextApplicationClassCoTeacherTeacherChoicesReadQueryTests::
propagatesPortFailure()
{
    RecordingTeacherChoicesPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "teacher catalogue failed",
        .recoverable = false
    };
    port.result = Application::ClassCoTeacherTeacherChoicesResult::failure(
        expected
        );
    const Application::ClassCoTeacherTeacherChoicesReadQuery query(port);

    const auto result = query.execute();

    QVERIFY(!result);
    QVERIFY(result.error() == expected);
    QCOMPARE(port.callCount, 1);
}

void NextApplicationClassCoTeacherTeacherChoicesReadQueryTests::
rejectsInvalidTeacherIdentities()
{
    const std::vector<std::string> invalidIdentities{
        "0",
        "-1",
        "01",
        "2147483648",
        "teacher"
    };
    for (const std::string& invalidIdentity : invalidIdentities)
    {
        RecordingTeacherChoicesPort port;
        Application::ClassCoTeacherTeacherChoicesSnapshot snapshot;
        snapshot.teachers.push_back({
            .teacherId = teacherId(invalidIdentity),
            .teacherKr = {},
            .teacherEn = {},
            .roomNumber = {},
            .internetType = {},
            .wifiName = {},
            .wifiPassword = {},
            .projectionType = {},
            .zoomId = {},
            .zoomPassword = {}
        });
        port.result =
            Application::ClassCoTeacherTeacherChoicesResult::success(
                std::move(snapshot)
                );
        const Application::ClassCoTeacherTeacherChoicesReadQuery query(port);

        const auto result = query.execute();

        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QVERIFY(!result.error().recoverable);
        QCOMPARE(port.callCount, 1);
    }
}

QTEST_APPLESS_MAIN(NextApplicationClassCoTeacherTeacherChoicesReadQueryTests)

#include "next_application_class_co_teacher_teacher_choices_read_query_tests.moc"
