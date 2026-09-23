#include "next/application/sub_prep_class_details_query.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

ClassId classId(std::string value)
{
    return *ClassId::fromString(value);
}

TeacherId teacherId(std::string value)
{
    return *TeacherId::fromString(value);
}

SubPrepClassDetails details(
    std::string classIdentifier = "class-1",
    std::optional<TeacherId> teacher = teacherId("teacher-1"),
    std::string classNotes = "Class notes",
    std::string teacherName = "Teacher One",
    SelectedClassTeacherFacilities teacherFacilities = {
        "Room 1", {}, {}, {}, {}, {}, {}
        },
    std::string teacherNotes = "Teacher notes"
    )
{
    return SubPrepClassDetails{
        classId(std::move(classIdentifier)),
        std::move(teacher),
        std::move(classNotes),
        std::move(teacherName),
        std::move(teacherFacilities),
        std::move(teacherNotes)
    };
}

class FakeReadPort final : public SubPrepClassDetailsReadPort
{
public:
    SubPrepClassDetailsReadResult result =
        SubPrepClassDetailsReadResult::failure({
            .code = ErrorCode::Technical,
            .message = "Unconfigured fake read port",
            .recoverable = false
        });
    std::vector<ClassId> requests;

    [[nodiscard]] SubPrepClassDetailsReadResult loadDetails(
        const ClassId& requestedClassId
        ) override
    {
        requests.push_back(requestedClassId);
        return result;
    }
};

void verifyError(
    const SubPrepClassDetailsQueryResult& result,
    const ErrorCode expectedCode
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, expectedCode);
}

} // namespace

class ClassMngrNextApplicationSubPrepClassDetailsQueryTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void queryUsesTypedOwnedValues();
    void invalidRequestIdsAreRejectedBeforePortCall();
    void returnsExactRequestedClassDetails();
    void allowsMissingTeacherIdentity();
    void propagatesStructuredPortError();
    void rejectsReturnedClassIdMismatch();
    void rejectsMalformedTeacherIdAndOversizedOutputText();
    void acceptsMaximumBoundaryLengths();
    void repeatedExecutionsReadThroughPortWithoutCaching();
};

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
queryUsesTypedOwnedValues()
{
    static_assert(std::is_same_v<
        decltype(std::declval<const SubPrepClassDetailsQuery>().execute(
            std::declval<const ClassId&>()
            )),
        SubPrepClassDetailsQueryResult
        >);
    static_assert(std::is_same_v<
        decltype(std::declval<SubPrepClassDetailsReadPort&>().loadDetails(
            std::declval<const ClassId&>()
            )),
        SubPrepClassDetailsReadResult
        >);
    static_assert(std::is_same_v<decltype(SubPrepClassDetails::classId), ClassId>);
    static_assert(
        std::is_same_v<
            decltype(SubPrepClassDetails::teacherId),
            std::optional<TeacherId>
            >
        );
    static_assert(std::is_copy_constructible_v<SubPrepClassDetails>);
    static_assert(std::is_copy_assignable_v<SubPrepClassDetails>);

    QVERIFY(true);
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
invalidRequestIdsAreRejectedBeforePortCall()
{
    FakeReadPort readPort;
    const SubPrepClassDetailsQuery query(readPort);

    const auto emptyId = ClassId::fromString("");
    QVERIFY(!emptyId.has_value());

    const std::vector<ClassId> invalidIds{
        classId(" \t "),
        classId(std::string(kSummaryMaxIdentifierLength + 1, 'c'))
    };
    for (const auto& invalidId : invalidIds)
    {
        verifyError(query.execute(invalidId), ErrorCode::InvalidInput);
    }

    QVERIFY(readPort.requests.empty());
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
returnsExactRequestedClassDetails()
{
    FakeReadPort readPort;
    const auto requestedClassId = classId("class-exact-17");
    const auto expected = details(
        "class-exact-17",
        teacherId("teacher-exact-4"),
        "Exact class notes",
        "Exact teacher name",
        SelectedClassTeacherFacilities{
            "Room 4",
            "Exact WiFi name",
            "Exact WiFi password",
            "fiber",
            "Exact Zoom ID",
            "Exact Zoom password",
            "projector"
        },
        "Exact teacher notes"
        );
    readPort.result = SubPrepClassDetailsReadResult::success(expected);

    const SubPrepClassDetailsQuery query(readPort);
    const auto result = query.execute(requestedClassId);

    QVERIFY(result);
    QVERIFY(result.value() == expected);
    QCOMPARE(readPort.requests.size(), std::size_t(1));
    QVERIFY(readPort.requests.front() == requestedClassId);
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
allowsMissingTeacherIdentity()
{
    FakeReadPort readPort;
    const auto requestedClassId = classId("class-no-teacher");
    const auto expected = details(
        "class-no-teacher",
        std::nullopt,
        "Class notes without an assigned teacher",
        "",
        {},
        ""
        );
    readPort.result = SubPrepClassDetailsReadResult::success(expected);

    const SubPrepClassDetailsQuery query(readPort);
    const auto result = query.execute(requestedClassId);

    QVERIFY(result);
    QVERIFY(!result.value().teacherId.has_value());
    QVERIFY(result.value() == expected);
    QVERIFY(readPort.requests == std::vector<ClassId>{requestedClassId});
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
propagatesStructuredPortError()
{
    FakeReadPort readPort;
    const OperationError expected{
        .code = ErrorCode::Technical,
        .message = "Sub Prep class details read failed",
        .recoverable = true
    };
    readPort.result = SubPrepClassDetailsReadResult::failure(expected);

    const SubPrepClassDetailsQuery query(readPort);
    const auto result = query.execute(classId("class-1"));

    verifyError(result, expected.code);
    QCOMPARE(result.error().message, expected.message);
    QCOMPARE(result.error().recoverable, expected.recoverable);
    QCOMPARE(readPort.requests.size(), std::size_t(1));
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
rejectsReturnedClassIdMismatch()
{
    FakeReadPort readPort;
    readPort.result = SubPrepClassDetailsReadResult::success(
        details("class-someone-else")
        );

    const SubPrepClassDetailsQuery query(readPort);
    const auto result = query.execute(classId("class-requested"));

    verifyError(result, ErrorCode::Validation);
    QVERIFY(
        readPort.requests
        == std::vector<ClassId>{classId("class-requested")}
        );
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
rejectsMalformedTeacherIdAndOversizedOutputText()
{
    FakeReadPort readPort;
    const SubPrepClassDetailsQuery query(readPort);
    const auto requestedClassId = classId("class-1");

    auto malformedTeacherId = details();
    malformedTeacherId.teacherId = teacherId(" \t ");
    readPort.result = SubPrepClassDetailsReadResult::success(malformedTeacherId);
    verifyError(query.execute(requestedClassId), ErrorCode::InvalidInput);

    malformedTeacherId.teacherId = teacherId(
        std::string(kSummaryMaxIdentifierLength + 1, 't')
        );
    readPort.result = SubPrepClassDetailsReadResult::success(malformedTeacherId);
    verifyError(query.execute(requestedClassId), ErrorCode::InvalidInput);

    const std::vector<std::pair<SubPrepClassDetails, std::string>> malformedOutputs{
        {details(), "class notes"},
        {details(), "teacher display name"},
        {details(), "teacher notes"}
    };

    for (std::size_t index = 0; index < malformedOutputs.size(); ++index)
    {
        auto malformed = malformedOutputs[index].first;
        switch (index)
        {
        case 0:
            malformed.classNotes = std::string(
                kSelectedClassDetailsMaxClassNotesLength + 1,
                'n'
                );
            break;
        case 1:
            malformed.teacherDisplayName = std::string(
                kSelectedClassDetailsMaxTeacherDisplayNameLength + 1,
                'n'
                );
            break;
        case 2:
            malformed.teacherNotes = std::string(
                kSelectedClassDetailsMaxTeacherNotesLength + 1,
                'n'
                );
            break;
        }

        readPort.result = SubPrepClassDetailsReadResult::success(
            std::move(malformed)
            );
        const auto result = query.execute(requestedClassId);
        QVERIFY2(
            !result,
            malformedOutputs[index].second.c_str()
            );
        QCOMPARE(result.error().code, ErrorCode::InvalidInput);
        QVERIFY(!result.hasValue());
    }

    const std::vector<std::tuple<
        const char*,
        std::string SelectedClassTeacherFacilities::*,
        std::size_t>> oversizedFacilityFields{
        {
            "teacher room",
            &SelectedClassTeacherFacilities::room,
            kSelectedClassDetailsMaxTeacherRoomLength
        },
        {
            "teacher WiFi name",
            &SelectedClassTeacherFacilities::wifiName,
            kSelectedClassDetailsMaxTeacherWifiNameLength
        },
        {
            "teacher WiFi password",
            &SelectedClassTeacherFacilities::wifiPassword,
            kSelectedClassDetailsMaxTeacherWifiPasswordLength
        },
        {
            "teacher internet type",
            &SelectedClassTeacherFacilities::internetType,
            kSelectedClassDetailsMaxTeacherInternetTypeLength
        },
        {
            "teacher Zoom ID",
            &SelectedClassTeacherFacilities::zoomId,
            kSelectedClassDetailsMaxTeacherZoomIdLength
        },
        {
            "teacher Zoom password",
            &SelectedClassTeacherFacilities::zoomPassword,
            kSelectedClassDetailsMaxTeacherZoomPasswordLength
        },
        {
            "teacher projection type",
            &SelectedClassTeacherFacilities::projectionType,
            kSelectedClassDetailsMaxTeacherProjectionTypeLength
        }
    };
    for (const auto& [fieldName, field, maxLength] : oversizedFacilityFields)
    {
        auto malformed = details();
        malformed.teacherFacilities.*field = std::string(maxLength + 1, 'f');
        readPort.result = SubPrepClassDetailsReadResult::success(
            std::move(malformed)
            );

        const auto result = query.execute(requestedClassId);
        QVERIFY2(!result, fieldName);
        QCOMPARE(result.error().code, ErrorCode::InvalidInput);
        QVERIFY(!result.hasValue());
    }
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
acceptsMaximumBoundaryLengths()
{
    FakeReadPort readPort;
    const auto maximumClassIdentifier = std::string(
        kSummaryMaxIdentifierLength,
        'c'
        );
    const auto requestedClassId = classId(maximumClassIdentifier);
    readPort.result = SubPrepClassDetailsReadResult::success(details(
        maximumClassIdentifier,
        teacherId(std::string(kSummaryMaxIdentifierLength, 't')),
        std::string(kSelectedClassDetailsMaxClassNotesLength, 'c'),
        std::string(kSelectedClassDetailsMaxTeacherDisplayNameLength, 'n'),
        SelectedClassTeacherFacilities{
            std::string(kSelectedClassDetailsMaxTeacherRoomLength, 'r'),
            std::string(kSelectedClassDetailsMaxTeacherWifiNameLength, 'w'),
            std::string(
                kSelectedClassDetailsMaxTeacherWifiPasswordLength,
                'p'
                ),
            std::string(
                kSelectedClassDetailsMaxTeacherInternetTypeLength,
                'i'
                ),
            std::string(kSelectedClassDetailsMaxTeacherZoomIdLength, 'z'),
            std::string(
                kSelectedClassDetailsMaxTeacherZoomPasswordLength,
                'q'
                ),
            std::string(
                kSelectedClassDetailsMaxTeacherProjectionTypeLength,
                'j'
                )
        },
        std::string(kSelectedClassDetailsMaxTeacherNotesLength, 't')
        ));

    const SubPrepClassDetailsQuery query(readPort);
    const auto result = query.execute(requestedClassId);

    QVERIFY(result);
    QVERIFY(result.value().classId == requestedClassId);
    QCOMPARE(
        result.value().teacherId->value().size(),
        kSummaryMaxIdentifierLength
        );
    QCOMPARE(
        result.value().classNotes.size(),
        kSelectedClassDetailsMaxClassNotesLength
        );
    QCOMPARE(
        result.value().teacherDisplayName.size(),
        kSelectedClassDetailsMaxTeacherDisplayNameLength
        );
    QCOMPARE(result.value().teacherFacilities.room.size(),
             kSelectedClassDetailsMaxTeacherRoomLength);
    QCOMPARE(result.value().teacherFacilities.wifiName.size(),
             kSelectedClassDetailsMaxTeacherWifiNameLength);
    QCOMPARE(result.value().teacherFacilities.wifiPassword.size(),
             kSelectedClassDetailsMaxTeacherWifiPasswordLength);
    QCOMPARE(result.value().teacherFacilities.internetType.size(),
             kSelectedClassDetailsMaxTeacherInternetTypeLength);
    QCOMPARE(result.value().teacherFacilities.zoomId.size(),
             kSelectedClassDetailsMaxTeacherZoomIdLength);
    QCOMPARE(result.value().teacherFacilities.zoomPassword.size(),
             kSelectedClassDetailsMaxTeacherZoomPasswordLength);
    QCOMPARE(result.value().teacherFacilities.projectionType.size(),
             kSelectedClassDetailsMaxTeacherProjectionTypeLength);
    QCOMPARE(
        result.value().teacherNotes.size(),
        kSelectedClassDetailsMaxTeacherNotesLength
        );
    QVERIFY(readPort.requests == std::vector<ClassId>{requestedClassId});
}

void ClassMngrNextApplicationSubPrepClassDetailsQueryTests::
repeatedExecutionsReadThroughPortWithoutCaching()
{
    FakeReadPort readPort;
    const auto requestedClassId = classId("class-changing-details");
    readPort.result = SubPrepClassDetailsReadResult::success(
        details("class-changing-details", teacherId("teacher-1"), "first read")
        );
    const SubPrepClassDetailsQuery query(readPort);

    const auto firstResult = query.execute(requestedClassId);
    QVERIFY(firstResult);
    QCOMPARE(firstResult.value().classNotes, std::string("first read"));

    readPort.result = SubPrepClassDetailsReadResult::success(
        details("class-changing-details", teacherId("teacher-1"), "second read")
        );
    const auto secondResult = query.execute(requestedClassId);

    QVERIFY(secondResult);
    QCOMPARE(secondResult.value().classNotes, std::string("second read"));
    QCOMPARE(firstResult.value().classNotes, std::string("first read"));
    QCOMPARE(readPort.requests.size(), std::size_t(2));
    QCOMPARE(
        readPort.requests,
        (std::vector<ClassId>{requestedClassId, requestedClassId})
        );
}

QTEST_APPLESS_MAIN(ClassMngrNextApplicationSubPrepClassDetailsQueryTests)

#include "next_application_sub_prep_class_details_query_tests.moc"
