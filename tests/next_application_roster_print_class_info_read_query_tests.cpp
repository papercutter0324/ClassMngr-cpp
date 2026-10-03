#include "next/application/roster_print_class_info_read_query.h"

#include <cassert>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

class FakeRosterPrintClassInfoReadPort final
    : public Application::RosterPrintClassInfoReadPort
{
public:
    mutable int callCount = 0;
    mutable Domain::ClassId lastClassId = *Domain::ClassId::fromString("1");
    bool fail = false;
    bool returnDifferentId = false;

    [[nodiscard]] Application::RosterPrintClassInfoReadResult
    readRosterPrintClassInfo(
        const Domain::ClassId& classId
        ) const override
    {
        ++callCount;
        lastClassId = classId;
        if (fail)
        {
            return Application::RosterPrintClassInfoReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected roster print class-info failure",
                .recoverable = false
            });
        }

        Application::RosterPrintClassInfoReadSnapshot snapshot(returnDifferentId
            ? *Domain::ClassId::fromString("18")
            : classId);
        snapshot.classGrade = u"E5";
        snapshot.classLevel = u"Odyssey";
        snapshot.teacherEn = u"Teacher En";
        snapshot.teacherKr = u"Teacher Kr";
        snapshot.roomNumber = u"506";
        snapshot.wifiName = u"WiFi";
        snapshot.wifiPassword = u"wifi-secret";
        snapshot.zoomId = u"zoom-id";
        snapshot.zoomPassword = u"zoom-secret";
        snapshot.regularSchedule.push_back({
            u"Tuesday", u"5:00 PM", u"5:50 PM"
        });
        return Application::RosterPrintClassInfoReadResult::success(
            std::move(snapshot)
            );
    }
};

void rejectsNonCanonicalIdsBeforeCallingPort()
{
    FakeRosterPrintClassInfoReadPort port;
    const Application::RosterPrintClassInfoReadQuery query(port);
    for (const std::string value : {"0", "042", "+42", "42x"})
    {
        const auto classId = Domain::ClassId::fromString(value);
        assert(classId);
        const auto result = query.execute(*classId);
        assert(!result);
        assert(result.error().code == Domain::ErrorCode::InvalidInput);
    }
    assert(port.callCount == 0);
}

void propagatesSourceFailureAndRejectsMismatchedIdentity()
{
    FakeRosterPrintClassInfoReadPort port;
    const Application::RosterPrintClassInfoReadQuery query(port);
    const auto classId = Domain::ClassId::fromString("42");
    assert(classId);

    port.fail = true;
    const auto failed = query.execute(*classId);
    assert(!failed);
    assert(failed.error().code == Domain::ErrorCode::Technical);
    assert(failed.error().message ==
           "injected roster print class-info failure");
    assert(port.callCount == 1);

    port.fail = false;
    port.returnDifferentId = true;
    const auto mismatched = query.execute(*classId);
    assert(!mismatched);
    assert(mismatched.error().code == Domain::ErrorCode::Validation);
    assert(port.callCount == 2);
}

void returnsTheTypedPrintProjection()
{
    FakeRosterPrintClassInfoReadPort port;
    const Application::RosterPrintClassInfoReadQuery query(port);
    const auto classId = Domain::ClassId::fromString("42");
    assert(classId);

    const auto result = query.execute(*classId);
    assert(result);
    assert(port.callCount == 1);
    assert(port.lastClassId == *classId);
    const auto& snapshot = result.value();
    assert(snapshot.classId == *classId);
    assert(snapshot.classGrade == u"E5");
    assert(snapshot.classLevel == u"Odyssey");
    assert(snapshot.teacherEn == u"Teacher En");
    assert(snapshot.teacherKr == u"Teacher Kr");
    assert(snapshot.roomNumber == u"506");
    assert(snapshot.wifiName == u"WiFi");
    assert(snapshot.wifiPassword == u"wifi-secret");
    assert(snapshot.zoomId == u"zoom-id");
    assert(snapshot.zoomPassword == u"zoom-secret");
    assert(snapshot.regularSchedule.size() == 1);
    assert(snapshot.regularSchedule.front().day == u"Tuesday");
    assert(snapshot.regularSchedule.front().startTime == u"5:00 PM");
    assert(snapshot.regularSchedule.front().endTime == u"5:50 PM");
}

}

int main()
{
    rejectsNonCanonicalIdsBeforeCallingPort();
    propagatesSourceFailureAndRejectsMismatchedIdentity();
    returnsTheTypedPrintProjection();
}
