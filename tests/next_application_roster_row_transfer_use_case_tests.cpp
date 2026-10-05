#include "next/application/roster_row_transfer_use_case.h"

#include <cstdio>
#include <optional>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{
bool equals(std::u16string_view a, std::u16string_view b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const auto lower = [](char16_t c) { return c >= u'A' && c <= u'Z' ? c + 32 : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

Domain::ClassId id(std::string_view value) { return *Domain::ClassId::fromString(value); }
const std::vector<std::u16string> base{
    u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"};

struct Ports final : RosterReadPort, RosterRowTransferSavePort
{
    RosterSnapshot target;
    std::optional<Domain::OperationError> readError;
    std::optional<Domain::OperationError> saveError;
    mutable std::vector<std::string> calls;
    mutable std::optional<RosterRowTransferSaveRequest> saved;
    mutable std::optional<Domain::ClassId> readId;

    RosterReadResult readRoster(const RosterReadQuery& query) const override
    {
        calls.push_back("read");
        readId = query.classId;
        return readError ? RosterReadResult::failure(*readError)
                         : RosterReadResult::success(target);
    }
    Domain::Result<void> saveTransfer(const RosterRowTransferSaveRequest& request) const override
    {
        calls.push_back("save");
        saved = request;
        return saveError ? Domain::Result<void>::failure(*saveError)
                         : Domain::Result<void>::success();
    }
};

RosterRowTransferRequest request()
{
    RosterSnapshot source{.columns = base, .columnWidths = {101,102,103,104,105,106},
        .rows = {{u"John", u"\uAE40\uBBFC\uC9C0", u"", u"", u"", u"Fall value"},
                 {u"Next", u"\uBC15\uC9C0\uBBFC"}, {}}};
    return {id("1"), id("2"), source, 0, base};
}

bool successProjectsAndCompactsWithoutMutatingInputs()
{
    auto req = request();
    req.sourceRoster.columns.push_back(u"Advisor Notes");
    req.sourceRoster.rows[0].push_back(u"  source\t note ");
    req.sourceRoster.columnWidths.push_back(107);
    const auto sourceBefore = req.sourceRoster;
    Ports ports;
    ports.target = {.columns = {u"Advisor Notes", u"Korean", u"Autumn", u"english",
            u" Extra\t Notes ", u"advisor notes", u""},
        .columnWidths = {210,220,230,240,250,260,270},
        .rows = {{u" keep  note ", u" \uBC15 \uC9C0\uBBFC ", u" autumn  value ",
                  u" oTHER ", u" other  note ", u"ignored", u"discarded"}}};
    const auto targetBefore = ports.target;
    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* ok = std::get_if<RosterRowTransferSuccess>(&result);
    if (!ok || ok->destinationRow != 1 || !ports.saved) return false;
    const auto& saved = *ports.saved;
    auto columns = base;
    columns.push_back(u"Advisor Notes"); columns.push_back(u"Extra Notes");
    return ports.calls == std::vector<std::string>{"read", "save"}
        && ports.readId == req.targetClassId
        && saved.sourceClassId == req.sourceClassId && saved.targetClassId == req.targetClassId
        && saved.sourceRoster.columns == sourceBefore.columns
        && saved.sourceRoster.columnWidths == sourceBefore.columnWidths
        && saved.sourceRoster.rows[0] == sourceBefore.rows[1]
        && saved.sourceRoster.rows.back() == std::vector<std::u16string>(7)
        && saved.targetRoster.columns == columns
        && saved.targetRoster.columnWidths == std::vector<int>{240,220,0,0,0,230,210,0}
        && saved.targetRoster.rows.size() == 25
        && saved.targetRoster.rows[0] == std::vector<std::u16string>{u"Other",u"\uBC15\uC9C0\uBBFC",
            u"",u"",u"",u"autumn value",u"keep note",u"other note"}
        && saved.targetRoster.rows[1] == std::vector<std::u16string>{u"John",u"\uAE40\uBBFC\uC9C0",
            u"",u"",u"",u"Fall value",u"source note",u""}
        && saved.targetRoster.rows[24] == std::vector<std::u16string>(8)
        && req.sourceRoster == sourceBefore && ports.target == targetBefore;
}

bool invalidSourcePreventsReadAndSave()
{
    for (int row : {-1,3})
    {
        auto req = request(); req.sourceRow = row; Ports ports;
        const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
        const auto* error = std::get_if<RosterRowRemovalError>(&result);
        if (!error || error->code != RosterRowRemovalErrorCode::InvalidRowIndex
            || !ports.calls.empty()) return false;
    }
    auto req = request(); req.sourceRow = 2; Ports ports;
    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<RosterRowRemovalError>(&result);
    return error && error->code == RosterRowRemovalErrorCode::RowHasNoData && ports.calls.empty();
}

bool invalidIdsPreventReadAndSave()
{
    for (const auto& pair : std::vector<std::pair<std::string,std::string>>{
            {"0","2"},{"01","2"},{"1","x"},{"1","2 "},{"1","1"},
            {"2147483648","2"}})
    {
        auto req = request(); req.sourceClassId = id(pair.first); req.targetClassId = id(pair.second);
        Ports ports;
        const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
        const auto* error = std::get_if<Domain::OperationError>(&result);
        if (!error || error->code != Domain::ErrorCode::InvalidInput || !ports.calls.empty()) return false;
    }
    return true;
}

bool rejectsPreparationWithoutSave()
{
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        auto req = request(); Ports ports;
        ports.target.columns = base;
        RosterRowTransferPreparationRejection expected;
        if (scenario == 0)
        {
            req.sourceRoster.columns = {u"Source only"};
            req.sourceRoster.rows = {{u"Unmapped data"}};
            expected = RosterRowTransferPreparationRejection::SourceRowHasNoData;
        }
        else if (scenario == 1)
        {
            ports.target.rows.resize(26, req.sourceRoster.rows[0]);
            expected = RosterRowTransferPreparationRejection::TargetRosterIsFull;
        }
        else
        {
            ports.target.rows = {req.sourceRoster.rows[0]};
            expected = RosterRowTransferPreparationRejection::DuplicateStudentNamePair;
        }
        const auto before = req.sourceRoster;
        const auto targetBefore = ports.target;
        const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
        const auto* error = std::get_if<RosterRowTransferPreparationError>(&result);
        if (!error || error->rejection != expected || ports.calls != std::vector<std::string>{"read"}
            || ports.saved || req.sourceRoster != before || ports.target != targetBefore) return false;
    }
    return true;
}

bool legacyCollisionStillRejectsWithoutWriting()
{
    auto req = request();
    req.sourceRoster.rows[0] = {u"A", u"B\u001fC"};
    Ports ports;
    ports.target = {.columns = base, .rows = {{u"A\u001fB", u"C"}}};
    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<RosterRowTransferPreparationError>(&result);
    return error && error->rejection == RosterRowTransferPreparationRejection::DuplicateStudentNamePair
        && ports.calls == std::vector<std::string>{"read"} && !ports.saved;
}

bool propagatesPortErrorsAndKeepsSnapshots()
{
    const Domain::OperationError expected{Domain::ErrorCode::Technical,"deliberate failure",true};
    for (bool failRead : {true,false})
    {
        auto req = request(); const auto before = req.sourceRoster;
        Ports ports;
        if (failRead) ports.readError = expected; else ports.saveError = expected;
        const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
        const auto* error = std::get_if<Domain::OperationError>(&result);
        if (!error || *error != expected || req.sourceRoster != before
            || ports.calls != (failRead ? std::vector<std::string>{"read"}
                                       : std::vector<std::string>{"read","save"})
            || ports.saved.has_value() == failRead) return false;
    }
    return true;
}
}

int main()
{
    const std::pair<const char*, bool(*)()> cases[]{
        {"success projection and immutable inputs",successProjectsAndCompactsWithoutMutatingInputs},
        {"invalid and empty source",invalidSourcePreventsReadAndSave},
        {"invalid IDs",invalidIdsPreventReadAndSave},
        {"ordered preparation rejections",rejectsPreparationWithoutSave},
        {"read and save failures",propagatesPortErrorsAndKeepsSnapshots},
        {"legacy name-pair collision",legacyCollisionStillRejectsWithoutWriting}};
    for (const auto& [name, test] : cases)
        if (!test()) { std::fprintf(stderr,"FAIL: %s\n",name); return 1; }
    return 0;
}
