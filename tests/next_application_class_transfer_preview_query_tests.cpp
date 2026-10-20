#include "next/application/class_transfer_preview_query.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{
Domain::TeacherId teacherId(const char* value)
{
    return *Domain::TeacherId::fromString(value);
}

Domain::ClassId classId(const char* value)
{
    return *Domain::ClassId::fromString(value);
}

class FakeReadPort final : public Application::ClassTransferPreviewReadPort
{
public:
    mutable Application::ClassTransferPreviewRequest observedRequest;
    Application::ClassTransferPreviewReadResult response =
        Application::ClassTransferPreviewReadResult::success({});
    mutable int calls = 0;

    Application::ClassTransferPreviewReadResult readSnapshot(
        const Application::ClassTransferPreviewRequest& request
        ) const override
    {
        ++calls;
        observedRequest = request;
        return response;
    }
};

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::abort();
    }
}

void assemblesInSourceAndDestinationOrderWithTypedIds()
{
    FakeReadPort port;
    port.response = Application::ClassTransferPreviewReadResult::success({
        .teachers = {
            {teacherId("22"), {"shared teacher", "공유 교사"}},
            {teacherId("11"), {"shared teacher", "공유 교사"}},
            {teacherId("33"), {"other teacher", "다른 교사"}}
        },
        .classes = {
            {classId("90"), "e4", "perseus",
                Application::ClassTransferMatchingDestinationTeacher{
                    teacherId("22"), {"shared teacher", "공유 교사"}}},
            {classId("80"), "e4", "perseus", std::nullopt},
            {classId("85"), "e4", "perseus", std::nullopt},
            {classId("70"), "e4", "perseus",
                Application::ClassTransferMatchingDestinationTeacher{
                    teacherId("33"), {"other teacher", "다른 교사"}}}
        }
    });

    Application::ClassTransferPreviewRequest request;
    request.teachers = {
        {Application::ClassTransferTeacherKey("source-b"),
            {"shared teacher", "공유 교사"}},
        {Application::ClassTransferTeacherKey("source-b"),
            {"other teacher", "다른 교사"}},
        {Application::ClassTransferTeacherKey("source-a"),
            {"absent teacher", "없는 교사"}}
    };
    request.classes = {
        {4, Application::ClassTransferTeacherKey("source-b"), "e4", "perseus",
            Application::ClassTransferClassKey("class-b")},
        {1, Application::ClassTransferTeacherKey("missing-source-key"),
            "e4", "perseus", Application::ClassTransferClassKey("class-unknown")},
        {9, Application::ClassTransferTeacherKey("source-b"), "e4", "",
            Application::ClassTransferClassKey("class-incomplete")},
        {7, Application::ClassTransferTeacherKey{}, "e4", "perseus",
            Application::ClassTransferClassKey("class-empty-teacher")}
    };

    const auto result = Application::ClassTransferPreviewQuery(port)
        .execute(request);
    require(result.hasValue(), "expected preview query to succeed");
    require(port.calls == 1, "expected one snapshot read");
    require(port.observedRequest.teachers.size() == request.teachers.size()
            && port.observedRequest.classes.size() == request.classes.size()
            && port.observedRequest.teachers.front().sourceKey.value() == "source-b"
            && port.observedRequest.classes.front().teacherSourceKey.value() == "source-b"
            && port.observedRequest.classes.front().sourceKey.value() == "class-b"
            && port.observedRequest.classes.front().packageClassIndex == 4,
        "query must pass the request to the read port unchanged");

    const auto& preview = result.value();
    require(preview.teachers.size() == 3, "teacher source order was lost");
    require(preview.teachers[0].teacherKey.value() == "source-b",
        "first teacher source key was not retained");
    require(preview.teachers[0].matchingTeacherIds
            == std::vector<Domain::TeacherId>{teacherId("22"), teacherId("11")},
        "teacher matches must retain destination order and typed IDs");
    require(preview.teachers[1].teacherKey.value() == "source-b"
            && preview.teachers[1].matchingTeacherIds
                == std::vector<Domain::TeacherId>{teacherId("33")},
        "duplicate teacher candidate must retain its own exact-name matches");
    require(preview.teachers[2].teacherKey.value() == "source-a"
            && preview.teachers[2].matchingTeacherIds.empty(),
        "third teacher candidate was not retained");

    require(preview.classes.size() == 4, "class source order was lost");
    require(preview.classes[0].packageClassIndex.rawValue() == 4,
        "first package class index was not retained");
    require(preview.classes[0].sourceKey.value() == "class-b",
        "class source key was not retained");
    require(preview.classes[0].matchingClassIds
            == std::vector<Domain::ClassId>{classId("90")},
        "assigned teacher class match must retain its typed ID");
    require(preview.classes[1].packageClassIndex.rawValue() == 1
            && preview.classes[1].matchingClassIds
                == std::vector<Domain::ClassId>{classId("80"), classId("85")},
        "unknown source teacher key must match only unassigned classes in destination order");
    require(preview.classes[2].packageClassIndex.rawValue() == 9
            && preview.classes[2].matchingClassIds.empty(),
        "incomplete source course must have no class matches");
    require(preview.classes[3].packageClassIndex.rawValue() == 7
            && preview.classes[3].sourceKey.value() == "class-empty-teacher"
            && preview.classes[3].matchingClassIds
                == std::vector<Domain::ClassId>{classId("80"), classId("85")},
        "empty source teacher key must fall back to unassigned destinations only");
}

void returnsStructuredReadFailures()
{
    FakeReadPort port;
    port.response = Application::ClassTransferPreviewReadResult::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "preview snapshot failed",
        .recoverable = true
    });
    Application::ClassTransferPreviewRequest request;
    request.teachers.push_back({
        Application::ClassTransferTeacherKey("source"), {"shared", ""}});

    const auto result = Application::ClassTransferPreviewQuery(port)
        .execute(request);
    require(!result, "read failure must fail the query");
    require(result.error() == port.response.error(),
        "query must preserve the structured adapter error");
}

void skipsReadForRequestsThatCannotMatch()
{
    FakeReadPort port;
    Application::ClassTransferPreviewRequest request;
    request.classes.push_back({5, Application::ClassTransferTeacherKey("teacher"),
        "e4", "", Application::ClassTransferClassKey("class-5")});
    request.teachers.push_back({
        Application::ClassTransferTeacherKey("blank"), {"", ""}});

    const auto result = Application::ClassTransferPreviewQuery(port)
        .execute(request);
    require(result.hasValue(), "empty-match preview should succeed");
    require(port.calls == 0, "empty destination need must skip the read port");
    require(result.value().classes.size() == 1
            && result.value().classes.front().packageClassIndex.rawValue() == 5
            && result.value().classes.front().sourceKey.value() == "class-5"
            && result.value().classes.front().matchingClassIds.empty(),
        "empty-match class candidate should still be returned");
    require(result.value().teachers.size() == 1
            && result.value().teachers.front().teacherKey.value() == "blank"
            && result.value().teachers.front().matchingTeacherIds.empty(),
        "empty-match teacher candidate should still be returned");
}
}

int main()
{
    assemblesInSourceAndDestinationOrderWithTypedIds();
    returnsStructuredReadFailures();
    skipsReadForRequestsThatCannotMatch();
}
