#include "next/application/roster_row_transfer_use_case.h"
#include "next/application/roster_window_read_query.h"

#include <array>
#include <cstdio>
#include <optional>
#include <string_view>
#include <type_traits>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

static_assert(std::is_constructible_v<RosterRowTransferSourceIndex, int>);
static_assert(!std::is_convertible_v<int, RosterRowTransferSourceIndex>);
static_assert(!std::is_convertible_v<RosterRowTransferSourceIndex, int>);

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

struct WindowPort final : RosterWindowReadPort
{
    RosterWindowReadSnapshot snapshot;
    mutable int callCount = 0;
    mutable std::optional<Domain::ClassId> readId;

    RosterWindowReadResult readRosterWindow(
        const RosterWindowReadQuery& query
        ) const override
    {
        ++callCount;
        readId = query.classId;
        return RosterWindowReadResult::success(snapshot);
    }
};

struct Confirmation final : RosterRowTransferConfirmationPort
{
    bool accepted = false;
    mutable int calls = 0;
    mutable std::optional<RosterSnapshot> target;
    mutable std::vector<RosterSaveIssue> issues;

    bool confirmTargetQuestionableKoreanNameLengths(
        const RosterSnapshot& preparedTarget,
        const std::vector<RosterSaveIssue>& questionableIssues) const override
    {
        ++calls;
        target = preparedTarget;
        issues = questionableIssues;
        return accepted;
    }
};

RosterRowTransferRequest request()
{
    RosterSnapshot source{.columns = base, .columnWidths = {
            RosterSnapshotColumnWidth{101}, RosterSnapshotColumnWidth{102},
            RosterSnapshotColumnWidth{103}, RosterSnapshotColumnWidth{104},
            RosterSnapshotColumnWidth{105}, RosterSnapshotColumnWidth{106}},
        .rows = {{u"John", u"\uAE40\uBBFC\uC9C0", u"", u"", u"", u"Fall value"},
                 {u"Next", u"\uBC15\uC9C0\uBBFC"}, {}}};
    return {id("1"), id("2"), source, RosterRowTransferSourceIndex{0}, base};
}

bool sourceIndexHasExplicitSignedValueSemantics()
{
    const RosterRowTransferSourceIndex defaultIndex;
    const RosterRowTransferSourceIndex negativeIndex{-3};
    return defaultIndex.rawValue() == -1
        && negativeIndex.rawValue() == -3
        && negativeIndex == RosterRowTransferSourceIndex{-3}
        && negativeIndex != defaultIndex;
}

bool successProjectsAndCompactsWithoutMutatingInputs()
{
    auto req = request();
    req.sourceRoster.columns.push_back(u" Advisor\tNotes ");
    req.sourceRoster.rows[0].push_back(u"  source\t note ");
    req.sourceRoster.rows[1][0] = u"  Next\t Name  ";
    req.sourceRoster.rows[1][1] = u" \uBC15 \uC9C0\uBBFC ";
    req.sourceRoster.columnWidths.push_back(RosterSnapshotColumnWidth{107});
    const auto sourceBefore = req.sourceRoster;
    Ports ports;
    ports.target = {.columns = {u"Advisor Notes", u"Korean", u"Autumn", u"english",
            u" Extra\t Notes ", u"advisor notes", u""},
        .columnWidths = {
            RosterSnapshotColumnWidth{210}, RosterSnapshotColumnWidth{220},
            RosterSnapshotColumnWidth{230}, RosterSnapshotColumnWidth{240},
            RosterSnapshotColumnWidth{250}, RosterSnapshotColumnWidth{260},
            RosterSnapshotColumnWidth{270}},
        .rows = {{u" keep  note ", u" \uBC15 \uC9C0\uBBFC ", u" autumn  value ",
                  u" oTHER ", u" other  note ", u"ignored", u"discarded"}}};
    const auto targetBefore = ports.target;
    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* ok = std::get_if<RosterRowTransferSuccess>(&result);
    if (!ok || ok->destinationRow.rawValue() != 1 || !ports.saved) return false;
    const auto& saved = *ports.saved;
    const auto removedSource = removeRosterRow(
        sourceBefore,
        RosterRowRemovalIndex{0}
        );
    if (!std::holds_alternative<RosterSnapshot>(removedSource)) return false;
    const auto preparedSource = prepareRosterSaveText(
        std::get<RosterSnapshot>(removedSource), true,
        RosterSaveCaseInsensitiveEquals{equals});
    auto columns = base;
    columns.push_back(u"Advisor Notes"); columns.push_back(u"Extra Notes");
    return ports.calls == std::vector<std::string>{"read", "save"}
        && ports.readId == req.targetClassId
        && saved.sourceClassId == req.sourceClassId && saved.targetClassId == req.targetClassId
        && !preparedSource.hasErrors()
        && saved.sourceRoster == preparedSource.roster
        && saved.sourceRoster.columns.back() == u"Advisor Notes"
        && saved.sourceRoster.columnWidths == sourceBefore.columnWidths
        && saved.sourceRoster.rows[0] == std::vector<std::u16string>{
            u"Next Name", u"\uBC15\uC9C0\uBBFC"}
        && saved.sourceRoster.rows.back() == std::vector<std::u16string>(7)
        && saved.targetRoster.columns == columns
        && saved.targetRoster.columnWidths == std::vector<RosterSnapshotColumnWidth>{
            RosterSnapshotColumnWidth{240}, RosterSnapshotColumnWidth{220},
            RosterSnapshotColumnWidth{0}, RosterSnapshotColumnWidth{0},
            RosterSnapshotColumnWidth{0}, RosterSnapshotColumnWidth{230},
            RosterSnapshotColumnWidth{210}, RosterSnapshotColumnWidth{0}}
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
        auto req = request(); req.sourceRow = RosterRowTransferSourceIndex{row}; Ports ports;
        const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
        const auto* error = std::get_if<RosterRowRemovalError>(&result);
        if (!error || error->code != RosterRowRemovalErrorCode::InvalidRowIndex
            || !ports.calls.empty()) return false;
    }
    auto req = request(); req.sourceRow = RosterRowTransferSourceIndex{2}; Ports ports;
    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<RosterRowRemovalError>(&result);
    return error && error->code == RosterRowRemovalErrorCode::RowHasNoData && ports.calls.empty();
}

bool invalidRemainingSourceRosterPreventsReadAndSave()
{
    auto req = request();
    req.sourceRoster.rows[1][0].clear();
    const auto sourceBefore = req.sourceRoster;
    Ports ports;
    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<Domain::OperationError>(&result);
    return error && error->code == Domain::ErrorCode::Validation
        && !error->recoverable
        && error->message.find("Source") != std::string::npos
        && ports.calls.empty() && !ports.saved
        && req.sourceRoster == sourceBefore;
}

bool overWideSelectedSourceRowPreventsReadAndSave()
{
    auto req = request();
    req.sourceRoster.rows[0].push_back(u"Unmodeled selected data");
    const auto requestBefore = req;
    Ports ports;

    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<Domain::OperationError>(&result);
    return error && error->code == Domain::ErrorCode::Validation
        && error->message == "Source roster failed validation before transfer."
        && !error->recoverable
        && ports.calls.empty() && !ports.saved
        && req.sourceClassId == requestBefore.sourceClassId
        && req.targetClassId == requestBefore.targetClassId
        && req.sourceRoster == requestBefore.sourceRoster
        && req.sourceRow == requestBefore.sourceRow
        && req.baseColumnNames == requestBefore.baseColumnNames;
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
        if (!error || error->code != Domain::ErrorCode::InvalidInput
            || error->message != "Transfer requires distinct canonical positive class IDs."
            || !error->recoverable || !ports.calls.empty()
            || ports.saved || ports.readId)
            return false;
    }
    return true;
}

bool maximumClassIdForwardsThroughReadAndSave()
{
    auto req = request();
    req.targetClassId = id("2147483647");
    Ports ports;

    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* ok = std::get_if<RosterRowTransferSuccess>(&result);
    return ok && ports.calls == std::vector<std::string>{"read", "save"}
        && ports.readId == req.targetClassId
        && ports.readId->value() == "2147483647"
        && ports.saved.has_value()
        && ports.saved->sourceClassId == req.sourceClassId
        && ports.saved->sourceClassId.value() == "1"
        && ports.saved->targetClassId == req.targetClassId
        && ports.saved->targetClassId.value() == "2147483647";
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
            req.sourceRoster.columns = base;
            req.sourceRoster.columns.push_back(u"Source only");
            req.sourceRoster.columnWidths = std::vector<RosterSnapshotColumnWidth>(
                base.size() + 1, RosterSnapshotColumnWidth{0});
            req.sourceRoster.rows = {{u"", u"", u"", u"", u"", u"", u"Unmapped data"}};
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

bool invalidTargetSnapshotDoesNotReachSave()
{
    auto req = request();
    const auto sourceBefore = req.sourceRoster;
    Ports ports;
    ports.target.columns = base;
    ports.target.columns.push_back(std::u16string(65, u'X'));
    const auto targetBefore = ports.target;

    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<Domain::OperationError>(&result);
    return error && error->code == Domain::ErrorCode::Validation
        && !error->recoverable
        && error->message.find("Target") != std::string::npos
        && ports.calls == std::vector<std::string>{"read"} && !ports.saved
        && req.sourceRoster == sourceBefore && ports.target == targetBefore;
}

bool targetWindowOverflowAllowsBoundedTransferSave()
{
    const auto req = request();
    Ports savePort;
    WindowPort windowPort;
    windowPort.snapshot.roster = {
        .columns = base,
        .columnWidths = std::vector<RosterSnapshotColumnWidth>(
            base.size(), RosterSnapshotColumnWidth{100}
            ),
        .rows = {{u"Existing", u"\uBC15\uC9C0\uBBFC"}}
    };
    windowPort.snapshot.hasRowsOutsideWindow = true;
    RosterWindowAsRosterReadPort readPort(windowPort);

    const auto result = RosterRowTransferUseCase::execute(
        req,
        readPort,
        savePort,
        equals
        );
    const auto* success = std::get_if<RosterRowTransferSuccess>(&result);
    return success && success->destinationRow.rawValue() == 1
        && windowPort.callCount == 1
        && windowPort.readId == req.targetClassId
        && savePort.calls == std::vector<std::string>{"save"}
        && savePort.saved
        && savePort.saved->targetRoster.rows.size() == RosterModeledRowCount
        && savePort.saved->targetRoster.rows[0][0] == u"Existing"
        && savePort.saved->targetRoster.rows[1][0] == u"John"
        && savePort.saved->targetRoster.rows[1][5] == u"Fall value";
}

bool emptyTargetUsesCanonicalBaseAndRowZero()
{
    auto req = request();
    const auto sourceBefore = req.sourceRoster;
    Ports ports;
    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* ok = std::get_if<RosterRowTransferSuccess>(&result);
    if (!ok || ok->destinationRow.rawValue() != 0 || !ports.saved
        || ports.calls != std::vector<std::string>{"read", "save"})
        return false;

    const auto& target = ports.saved->targetRoster;
    if (target.rows.size() != RosterModeledRowCount)
        return false;
    return target.columns == base
        && target.columnWidths == std::vector<RosterSnapshotColumnWidth>(
            base.size(), RosterSnapshotColumnWidth{0})
        && target.rows[0] == std::vector<std::u16string>{
            u"John", u"\uAE40\uBBFC\uC9C0", u"", u"", u"", u"Fall value"}
        && target.rows[1] == std::vector<std::u16string>(base.size())
        && target.rows.back() == std::vector<std::u16string>(base.size())
        && req.sourceRoster == sourceBefore;
}

bool warningOnlySnapshotPreparationStillSaves()
{
    constexpr std::array questionableNames{
        std::pair{std::u16string_view(u"\uAE40"),
                  std::string_view("student_name.korean.too_short")},
        std::pair{std::u16string_view(u"\uAE40\uBBFC\uC11C\uC9C0\uC6B0"),
                  std::string_view("student_name.korean.too_long")}
    };
    for (const auto& [name, expectedCode] : questionableNames)
    {
        auto req = request();
        req.sourceQuestionableKoreanNameLengthsConfirmed = true;
        req.sourceRoster.rows[0][1] = name;
        Ports ports;
        Confirmation confirmation;
        confirmation.accepted = true;
        const auto result = RosterRowTransferUseCase::execute(
            req, ports, ports, equals, &confirmation);
        const auto* ok = std::get_if<RosterRowTransferSuccess>(&result);
        if (!ok || !ports.saved
            || ports.calls != std::vector<std::string>{"read", "save"}
            || confirmation.calls != 1 || !confirmation.target
            || confirmation.issues.size() != 1)
            return false;
        const auto& issue = confirmation.issues.front();
        if (rosterSaveIssueCodeKey(issue.code) != expectedCode
            || issue.row.rawValue() != 0
            || issue.column.rawValue() != 1
            || issue.severity != RosterSaveIssueSeverity::Warning
            || confirmation.target->rows[0][1] != name
            || *confirmation.target != ports.saved->targetRoster)
            return false;
        if (ok->destinationRow.rawValue() >= ports.saved->targetRoster.rows.size()
            || ports.saved->targetRoster.rows[ok->destinationRow.rawValue()][1] != name)
            return false;

        const auto prepared = prepareRosterSaveText(
            ports.saved->targetRoster, true,
            RosterSaveCaseInsensitiveEquals{equals});
        if (prepared.issues.empty() || prepared.hasErrors())
            return false;
    }
    return true;
}

bool rejectedTargetQuestionableKoreanLengthCancelsBeforeSave()
{
    constexpr std::array questionableNames{
        std::pair{std::u16string_view(u"\uAE40"),
                  std::string_view("student_name.korean.too_short")},
        std::pair{std::u16string_view(u"\uAE40\uBBFC\uC11C\uC9C0\uC6B0"),
                  std::string_view("student_name.korean.too_long")}
    };
    for (const auto& [name, expectedCode] : questionableNames)
    {
        auto req = request();
        req.sourceQuestionableKoreanNameLengthsConfirmed = true;
        req.sourceRoster.rows[0][1] = name;
        Ports ports;
        Confirmation confirmation;

        const auto result = RosterRowTransferUseCase::execute(
            req, ports, ports, equals, &confirmation);
        const auto* error = std::get_if<Domain::OperationError>(&result);
        if (!error || error->code != Domain::ErrorCode::Canceled
            || !error->recoverable || confirmation.calls != 1
            || confirmation.issues.size() != 1
            || rosterSaveIssueCodeKey(confirmation.issues.front().code)
                != expectedCode
            || !confirmation.target
            || ports.calls != std::vector<std::string>{"read"}
            || ports.saved)
            return false;
    }
    return true;
}

bool missingTargetConfirmationPortCancelsBeforeSave()
{
    auto req = request();
    req.sourceQuestionableKoreanNameLengthsConfirmed = true;
    req.sourceRoster.rows[0][1] = u"\uAE40";
    Ports ports;

    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<Domain::OperationError>(&result);
    return error && error->code == Domain::ErrorCode::Canceled
        && error->recoverable
        && ports.calls == std::vector<std::string>{"read"}
        && !ports.saved;
}

bool rejectsQuestionableKoreanLengthWithoutConfirmation()
{
    auto req = request();
    req.sourceRoster.rows[1][1] = u"\uAE40";
    Ports ports;

    const auto result = RosterRowTransferUseCase::execute(req, ports, ports, equals);
    const auto* error = std::get_if<Domain::OperationError>(&result);
    return error && error->code == Domain::ErrorCode::Validation
        && !error->recoverable
        && error->message == "Source roster failed validation before transfer."
        && ports.calls.empty() && !ports.saved;
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
    if (!sourceIndexHasExplicitSignedValueSemantics())
    {
        std::fprintf(stderr, "Source transfer index lost its signed value semantics.\n");
        return EXIT_FAILURE;
    }
    const std::pair<const char*, bool(*)()> cases[]{
        {"success projection and immutable inputs",successProjectsAndCompactsWithoutMutatingInputs},
        {"invalid and empty source",invalidSourcePreventsReadAndSave},
        {"invalid remaining source roster",invalidRemainingSourceRosterPreventsReadAndSave},
        {"over-wide selected source row",overWideSelectedSourceRowPreventsReadAndSave},
        {"invalid IDs",invalidIdsPreventReadAndSave},
        {"maximum class ID forwarding",maximumClassIdForwardsThroughReadAndSave},
        {"invalid target snapshot",invalidTargetSnapshotDoesNotReachSave},
        {"target window overflow allows bounded transfer save",targetWindowOverflowAllowsBoundedTransferSave},
        {"empty target base columns and row zero",emptyTargetUsesCanonicalBaseAndRowZero},
        {"warning-only roster preparation",warningOnlySnapshotPreparationStillSaves},
        {"rejected target name lengths cancel before saving",rejectedTargetQuestionableKoreanLengthCancelsBeforeSave},
        {"missing target confirmation cancels before saving",missingTargetConfirmationPortCancelsBeforeSave},
        {"questionable Korean length requires confirmation",rejectsQuestionableKoreanLengthWithoutConfirmation},
        {"ordered preparation rejections",rejectsPreparationWithoutSave},
        {"read and save failures",propagatesPortErrorsAndKeepsSnapshots},
        {"legacy name-pair collision",legacyCollisionStillRejectsWithoutWriting}};
    for (const auto& [name, test] : cases)
        if (!test()) { std::fprintf(stderr,"FAIL: %s\n",name); return 1; }
    return 0;
}
