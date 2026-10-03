#include "next/application/my_classes_class_information_read_query.h"

#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

Domain::TeacherId teacherId(const std::string& value)
{
    return *Domain::TeacherId::fromString(value);
}

class FakeMyClassesClassInformationReadPort final
    : public Application::MyClassesClassInformationReadPort
{
public:
    Application::MyClassesClassInformationReadResult result =
        Application::MyClassesClassInformationReadResult::success({
            .classId = classId("7"),
            .fields = {}
        });
    mutable int calls = 0;
    mutable std::string requestedClassId;

    [[nodiscard]] Application::MyClassesClassInformationReadResult
    readMyClassesClassInformation(
        const Domain::ClassId& id
        ) const override
    {
        ++calls;
        requestedClassId = id.value();
        return result;
    }
};

}

class NextApplicationMyClassesClassInformationReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void preservesScheduleOrderAndTypedTeacherAssociation();
    void rejectsInvalidClassIdentifierBeforeRead();
    void rejectsSnapshotForDifferentClass();
    void rejectsInvalidTeacherIdentifier();
    void propagatesPortFailure();
};

void NextApplicationMyClassesClassInformationReadQueryTests::
preservesScheduleOrderAndTypedTeacherAssociation()
{
    FakeMyClassesClassInformationReadPort port;
    port.result = Application::MyClassesClassInformationReadResult::success({
        .classId = classId("7"),
        .fields = {
            .classGrade = u"E4 \U0001F9ED",
            .classLevel = u"Theseus",
            .regularSchedule = {
                {u"Wednesday", u"04:00 pm", u"04:50 pm"},
                {u"Monday", u"09:15 AM", u"10:05 AM"}
            },
            .intensiveSchedule = {
                {u"Friday", u"02:00 PM", u"04:00 PM"}
            },
            .notes = u"Class notes\n\U0001F9ED",
            .timeFillerActivities = u"Filler \U0001F4DA",
            .teacherId = teacherId("19")
        }
    });

    const Application::MyClassesClassInformationReadQuery query(port);
    const auto loaded = query.execute(classId("7"));

    QVERIFY(loaded);
    QCOMPARE(port.calls, 1);
    QCOMPARE(port.requestedClassId, std::string("7"));
    QCOMPARE(loaded.value().classId, classId("7"));
    const Application::MyClassesClassInformationFields expected{
        .classGrade = u"E4 \U0001F9ED",
        .classLevel = u"Theseus",
        .regularSchedule = {
            {u"Wednesday", u"04:00 pm", u"04:50 pm"},
            {u"Monday", u"09:15 AM", u"10:05 AM"}
        },
        .intensiveSchedule = {
            {u"Friday", u"02:00 PM", u"04:00 PM"}
        },
        .notes = u"Class notes\n\U0001F9ED",
        .timeFillerActivities = u"Filler \U0001F4DA",
        .teacherId = teacherId("19")
    };
    QVERIFY(loaded.value().fields == expected);
}

void NextApplicationMyClassesClassInformationReadQueryTests::
rejectsInvalidClassIdentifierBeforeRead()
{
    FakeMyClassesClassInformationReadPort port;
    const Application::MyClassesClassInformationReadQuery query(port);

    for (const std::string& value : {"0", "07", " 7", "7 "})
    {
        const auto loaded = query.execute(classId(value));
        QVERIFY(!loaded);
        QCOMPARE(loaded.error().code, Domain::ErrorCode::InvalidInput);
    }

    QCOMPARE(port.calls, 0);
}

void NextApplicationMyClassesClassInformationReadQueryTests::
rejectsSnapshotForDifferentClass()
{
    FakeMyClassesClassInformationReadPort port;
    port.result = Application::MyClassesClassInformationReadResult::success({
        .classId = classId("8"),
        .fields = {}
    });
    const Application::MyClassesClassInformationReadQuery query(port);

    const auto loaded = query.execute(classId("7"));

    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(port.calls, 1);
}

void NextApplicationMyClassesClassInformationReadQueryTests::
rejectsInvalidTeacherIdentifier()
{
    FakeMyClassesClassInformationReadPort port;
    port.result = Application::MyClassesClassInformationReadResult::success({
        .classId = classId("7"),
        .fields = {
            .teacherId = teacherId("019")
        }
    });
    const Application::MyClassesClassInformationReadQuery query(port);

    const auto loaded = query.execute(classId("7"));

    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::Validation);
}

void NextApplicationMyClassesClassInformationReadQueryTests::
propagatesPortFailure()
{
    FakeMyClassesClassInformationReadPort port;
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::Technical,
        .message = "read failed",
        .recoverable = true
    };
    port.result =
        Application::MyClassesClassInformationReadResult::failure(expected);
    const Application::MyClassesClassInformationReadQuery query(port);

    const auto loaded = query.execute(classId("7"));

    QVERIFY(!loaded);
    QCOMPARE(loaded.error(), expected);
    QCOMPARE(port.calls, 1);
}

QTEST_APPLESS_MAIN(NextApplicationMyClassesClassInformationReadQueryTests)

#include "next_application_my_classes_class_information_read_query_tests.moc"
