#include "next/application/classes_list_read_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(const std::string& value)
{
    return *Domain::ClassId::fromString(value);
}

class RecordingClassesListReadPort final : public ClassesListReadPort
{
public:
    [[nodiscard]] ClassesListReadResult readClassesList() const override
    {
        ++callCount;
        return result;
    }

    mutable int callCount = 0;
    ClassesListReadResult result = ClassesListReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Unconfigured classes list read",
        .recoverable = false
    });
};

}

class NextApplicationClassesListReadQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void returnsOrderedTypedClassIdsAndNames();
    void preservesEmptyResultAndPortFailure();
    void rejectsInvalidOrDuplicateClassIds();
};

void NextApplicationClassesListReadQueryTests::
returnsOrderedTypedClassIdsAndNames()
{
    RecordingClassesListReadPort port;
    const ClassesListSnapshot expected{
        .classes = {
            {
                .classId = classId("42"),
                .className = u"Hercules"
            },
            {
                .classId = classId("7"),
                .className = u"\uAC00\uB098\uB2E4"
            }
        }
    };
    port.result = ClassesListReadResult::success(expected);
    const ClassesListReadQuery query(port);

    const ClassesListReadResult result = query.execute();

    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(result.value() == expected);
    QCOMPARE(result.value().classes.at(0).classId.value(), std::string("42"));
    QCOMPARE(result.value().classes.at(0).className, std::u16string(u"Hercules"));
    QCOMPARE(result.value().classes.at(1).classId.value(), std::string("7"));
    QCOMPARE(result.value().classes.at(1).className,
        std::u16string(u"\uAC00\uB098\uB2E4"));
}

void NextApplicationClassesListReadQueryTests::
preservesEmptyResultAndPortFailure()
{
    RecordingClassesListReadPort port;
    port.result = ClassesListReadResult::success({});
    const ClassesListReadQuery query(port);

    const ClassesListReadResult emptyResult = query.execute();
    QVERIFY(emptyResult);
    QVERIFY(emptyResult.value().classes.empty());
    QCOMPARE(port.callCount, 1);

    const Domain::OperationError expectedError{
        .code = Domain::ErrorCode::Technical,
        .message = "class list unavailable",
        .recoverable = false
    };
    port.result = ClassesListReadResult::failure(expectedError);
    const ClassesListReadResult failedResult = query.execute();
    QVERIFY(!failedResult);
    QVERIFY(failedResult.error() == expectedError);
    QCOMPARE(port.callCount, 2);
}

void NextApplicationClassesListReadQueryTests::
rejectsInvalidOrDuplicateClassIds()
{
    const std::vector<std::vector<std::string>> invalidRows{
        {"0"},
        {"01"},
        {"-1"},
        {"2147483648"},
        {"42", "42"}
    };

    for (const auto& ids : invalidRows)
    {
        RecordingClassesListReadPort port;
        ClassesListSnapshot snapshot;
        for (const std::string& id : ids)
        {
            snapshot.classes.push_back({
                .classId = classId(id),
                .className = u"Class"
            });
        }
        port.result = ClassesListReadResult::success(std::move(snapshot));
        const ClassesListReadQuery query(port);

        const ClassesListReadResult result = query.execute();
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
        QVERIFY(!result.error().recoverable);
        QCOMPARE(port.callCount, 1);
    }
}

QTEST_APPLESS_MAIN(NextApplicationClassesListReadQueryTests)

#include "next_application_classes_list_read_query_tests.moc"
