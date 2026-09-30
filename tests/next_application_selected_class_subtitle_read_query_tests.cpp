#include "next/application/selected_class_subtitle_read_query.h"

#include <QtTest/QtTest>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(std::string value)
{
    return *Domain::ClassId::fromString(value);
}

class FakeReadPort final : public SelectedClassSubtitleReadPort
{
public:
    SelectedClassSubtitleReadResult result =
        SelectedClassSubtitleReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured fake read port",
            .recoverable = false
        });
    mutable std::vector<Domain::ClassId> requests;

    [[nodiscard]] SelectedClassSubtitleReadResult
    readSelectedClassSubtitle(
        const Domain::ClassId& id
        ) const override
    {
        requests.push_back(id);
        return result;
    }
};

SelectedClassSubtitleReadSnapshot snapshot(
    const Domain::ClassId& id,
    Domain::Result<SelectedClassSubtitleFields> classFields,
    Domain::Result<std::optional<SelectedClassSubtitleTeacherFields>> teacher
    )
{
    return {id, std::move(classFields), std::move(teacher)};
}

}

class NextApplicationSelectedClassSubtitleReadQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void contractContainsOnlyQtFreeSubtitleInputs();
    void rejectsInvalidTypedIdsBeforeCallingPort();
    void returnsClassAndAssignedTeacherOutcomesIndependently();
    void propagatesPortFailureAndRejectsSnapshotMismatch();
};

void NextApplicationSelectedClassSubtitleReadQueryTests::
contractContainsOnlyQtFreeSubtitleInputs()
{
    static_assert(std::is_same_v<
        decltype(SelectedClassSubtitleFields::classGrade),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(SelectedClassSubtitleScheduleRow::day),
        std::u16string
        >);
    static_assert(std::is_same_v<
        decltype(SelectedClassSubtitleReadSnapshot::assignedTeacher),
        Domain::Result<std::optional<
            SelectedClassSubtitleTeacherFields
            >>
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<const SelectedClassSubtitleReadPort&>()
            .readSelectedClassSubtitle(std::declval<const Domain::ClassId&>())),
        SelectedClassSubtitleReadResult
        >);

    const SelectedClassSubtitleFields fields{
        u" E4 ",
        u"Theseus",
        {{u"Monday", u"9:00 AM"}}
    };
    QCOMPARE(fields.classGrade, std::u16string(u" E4 "));
    QCOMPARE(fields.regularSchedule.size(), std::size_t(1));
}

void NextApplicationSelectedClassSubtitleReadQueryTests::
rejectsInvalidTypedIdsBeforeCallingPort()
{
    FakeReadPort port;
    const SelectedClassSubtitleReadQuery query(port);

    for (const std::string value : {"0", "042", "-42", " 42", "42 "})
    {
        const auto result = query.execute(classId(value));
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QVERIFY(!result.error().recoverable);
    }
    QVERIFY(port.requests.empty());
}

void NextApplicationSelectedClassSubtitleReadQueryTests::
returnsClassAndAssignedTeacherOutcomesIndependently()
{
    FakeReadPort port;
    const Domain::ClassId id = classId("42");
    SelectedClassSubtitleFields fields{
        u"E4",
        u"Theseus",
        {{u"Legacy Friday", u"raw time"}, {u"Monday", u"9:00 AM"}}
    };
    const SelectedClassSubtitleTeacherFields teacher{
        u"김 선생님",
        u"English Name",
        u"Preferred Romanization",
        u"Preferred Name"
    };
    port.result = SelectedClassSubtitleReadResult::success(snapshot(
        id,
        Domain::Result<SelectedClassSubtitleFields>::success(fields),
        Domain::Result<std::optional<SelectedClassSubtitleTeacherFields>>::success(
            teacher
            )
        ));

    const SelectedClassSubtitleReadQuery query(port);
    const auto result = query.execute(id);
    QVERIFY(result);
    QVERIFY(result.value().classId == id);
    QVERIFY(result.value().classFields);
    QVERIFY(result.value().assignedTeacher);
    QCOMPARE(result.value().classFields.value(), fields);
    QVERIFY(result.value().assignedTeacher.value().has_value());
    QCOMPARE(result.value().assignedTeacher.value().value(), teacher);
    QVERIFY(port.requests == std::vector<Domain::ClassId>{id});

    const Domain::OperationError classError{
        .code = Domain::ErrorCode::Technical,
        .message = "class fields unavailable",
        .recoverable = true
    };
    port.result = SelectedClassSubtitleReadResult::success(snapshot(
        id,
        Domain::Result<SelectedClassSubtitleFields>::failure(classError),
        Domain::Result<std::optional<
            SelectedClassSubtitleTeacherFields
            >>::success(std::nullopt)
        ));
    const auto classFailure = query.execute(id);
    QVERIFY(classFailure);
    QVERIFY(!classFailure.value().classFields);
    QVERIFY(classFailure.value().classFields.error() == classError);
    QVERIFY(classFailure.value().assignedTeacher);
    QVERIFY(!classFailure.value().assignedTeacher.value().has_value());

    const Domain::OperationError teacherError{
        .code = Domain::ErrorCode::NotFound,
        .message = "teacher display fields unavailable",
        .recoverable = false
    };
    port.result = SelectedClassSubtitleReadResult::success(snapshot(
        id,
        Domain::Result<SelectedClassSubtitleFields>::success(fields),
        Domain::Result<std::optional<
            SelectedClassSubtitleTeacherFields
            >>::failure(teacherError)
        ));
    const auto teacherFailure = query.execute(id);
    QVERIFY(teacherFailure);
    QVERIFY(teacherFailure.value().classFields);
    QCOMPARE(teacherFailure.value().classFields.value(), fields);
    QVERIFY(!teacherFailure.value().assignedTeacher);
    QVERIFY(teacherFailure.value().assignedTeacher.error() == teacherError);
}

void NextApplicationSelectedClassSubtitleReadQueryTests::
propagatesPortFailureAndRejectsSnapshotMismatch()
{
    FakeReadPort port;
    const SelectedClassSubtitleReadQuery query(port);
    const Domain::OperationError expected{
        .code = Domain::ErrorCode::NotFound,
        .message = "No active database session",
        .recoverable = true
    };
    port.result = SelectedClassSubtitleReadResult::failure(expected);

    const Domain::ClassId id = classId("42");
    const auto unavailable = query.execute(id);
    QVERIFY(!unavailable);
    QVERIFY(unavailable.error() == expected);

    port.result = SelectedClassSubtitleReadResult::success(snapshot(
        classId("43"),
        Domain::Result<SelectedClassSubtitleFields>::success({}),
        Domain::Result<std::optional<
            SelectedClassSubtitleTeacherFields
            >>::success(std::nullopt)
        ));
    const auto mismatched = query.execute(id);
    QVERIFY(!mismatched);
    QCOMPARE(mismatched.error().code, Domain::ErrorCode::Validation);
}

QTEST_APPLESS_MAIN(NextApplicationSelectedClassSubtitleReadQueryTests)

#include "next_application_selected_class_subtitle_read_query_tests.moc"
