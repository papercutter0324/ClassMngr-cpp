#include "next/application/roster_template_print_source_read_query.h"

#include <cassert>
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

Application::RosterTemplatePrintSourceReadEntry entry(
    const int id,
    const std::u16string& grade
    )
{
    const Domain::ClassId typedId = classId(id);
    Application::RosterPrintClassInfoReadSnapshot classInfo(typedId);
    classInfo.classGrade = grade;
    classInfo.classLevel = u"Level";
    classInfo.teacherEn = u"Teacher";
    classInfo.teacherKr = u"Teacher Korean";
    classInfo.roomNumber = u"Room";
    classInfo.wifiName = u"WiFi";
    classInfo.wifiPassword = u"WiFi password";
    classInfo.zoomId = u"Zoom";
    classInfo.zoomPassword = u"Zoom password";
    classInfo.regularSchedule.push_back({u"Monday", u"9 AM", u"10 AM"});

    Application::RosterSnapshot roster;
    roster.columns = {u"English", u"Korean"};
    roster.columnWidths = {120, 140};
    roster.rows = {{u"Student", u"학생"}};
    return {
        typedId,
        std::move(classInfo),
        std::move(roster)
    };
}

class FakeRosterTemplatePrintSourceReadPort final
    : public Application::RosterTemplatePrintSourceReadPort
{
public:
    mutable int callCount = 0;
    mutable Application::RosterTemplatePrintSourceReadRequest lastQuery;
    std::optional<Domain::OperationError> failure;
    Application::RosterTemplatePrintSourceReadSnapshot response;

    [[nodiscard]] Application::RosterTemplatePrintSourceReadResult
    readRosterTemplatePrintSource(
        const Application::RosterTemplatePrintSourceReadRequest& query
        ) const override
    {
        ++callCount;
        lastQuery = query;
        if (failure)
        {
            return Application::RosterTemplatePrintSourceReadResult::failure(
                *failure
                );
        }
        return Application::RosterTemplatePrintSourceReadResult::success(
            response
            );
    }
};

void emptyAndInvalidInputsAvoidThePort()
{
    FakeRosterTemplatePrintSourceReadPort port;
    const Application::RosterTemplatePrintSourceReadQuery query(port);

    const auto empty = query.execute({});
    assert(empty);
    assert(empty.value().empty());
    assert(port.callCount == 0);

    const std::vector<Domain::ClassId> invalidIds{
        classId(1),
        *Domain::ClassId::fromString("01")
    };
    const auto invalid = query.execute(invalidIds);
    assert(!invalid);
    assert(invalid.error().code == Domain::ErrorCode::InvalidInput);
    assert(port.callCount == 0);

    const auto duplicate = query.execute({classId(1), classId(1)});
    assert(!duplicate);
    assert(duplicate.error().code == Domain::ErrorCode::InvalidInput);
    assert(port.callCount == 0);
}

void preservesRequestedOrderAndCompleteSnapshot()
{
    FakeRosterTemplatePrintSourceReadPort port;
    port.response = {entry(27, u"E7"), entry(12, u"E5")};
    const Application::RosterTemplatePrintSourceReadQuery query(port);

    const auto result = query.execute({classId(27), classId(12)});
    assert(result);
    assert(port.callCount == 1);
    assert(port.lastQuery.classIds.size() == 2);
    assert(port.lastQuery.classIds[0] == classId(27));
    assert(port.lastQuery.classIds[1] == classId(12));
    assert(result.value()[0].classInfo.classGrade == u"E7");
    assert(result.value()[1].classInfo.classGrade == u"E5");
    assert(result.value()[0].roster.columns.size() == 2);
    assert(result.value()[0].roster.columnWidths[1] == 140);
    assert(result.value()[0].roster.rows[0][0] == u"Student");
}

void propagatesFailureAndRejectsWrongCountOrIdentity()
{
    FakeRosterTemplatePrintSourceReadPort port;
    const Application::RosterTemplatePrintSourceReadQuery query(port);

    port.failure = Domain::OperationError{
        .code = Domain::ErrorCode::Technical,
        .message = "injected batch failure",
        .recoverable = false
    };
    const auto failed = query.execute({classId(5)});
    assert(!failed);
    assert(failed.error().code == Domain::ErrorCode::Technical);
    assert(failed.error().message == "injected batch failure");
    assert(port.callCount == 1);

    port.failure.reset();
    port.response = {};
    const auto wrongCount = query.execute({classId(5)});
    assert(!wrongCount);
    assert(wrongCount.error().code == Domain::ErrorCode::Validation);

    port.response = {entry(6, u"E6")};
    const auto wrongId = query.execute({classId(5)});
    assert(!wrongId);
    assert(wrongId.error().code == Domain::ErrorCode::Validation);
    assert(port.callCount == 3);
}

} // namespace

int main()
{
    emptyAndInvalidInputsAvoidThePort();
    preservesRequestedOrderAndCompleteSnapshot();
    propagatesFailureAndRejectsWrongCountOrIdentity();
}
