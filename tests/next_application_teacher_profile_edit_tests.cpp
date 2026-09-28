#include "next/application/teacher_profile_edit_use_case.h"

#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

TeacherProfileValidationIssue validationIssue(
    std::string code,
    std::string field,
    TeacherProfileValidationSeverity severity,
    std::initializer_list<TeacherProfileValidationArgument> arguments = {}
    )
{
    TeacherProfileValidationIssue issue{
        .code = std::move(code),
        .field = std::move(field),
        .severity = severity
    };
    for (const auto& argument : arguments)
    {
        require(issue.arguments.add(argument),
            "test validation argument should satisfy the bounded contract");
    }
    return issue;
}

Domain::OperationError failure(std::string message)
{
    return {.code = Domain::ErrorCode::Technical,
            .message = std::move(message), .recoverable = true};
}

Domain::TeacherProfileFields fields()
{
    return {.teacherKr = u"김 선생", .teacherEn = u"Jane Teacher",
            .preferredRomanization = u"J. Teacher", .preferredName = u"Jane",
            .roomNumber = u"Room 204", .birthday = u"03-14",
            .phoneNumber = u"010-1234-5678", .wifiName = u"Staff WiFi",
            .wifiPassword = u"wifi-secret", .internetType = u"Both",
            .zoomId = u"jane.zoom", .zoomPassword = u"zoom-secret",
            .projectionType = u"Zoom", .notes = u"Profile notes"};
}

Domain::TeacherProfile profile(
    int id,
    Domain::TeacherProfileFields value = fields()
    )
{
    const auto typedId = Domain::TeacherId::fromInt(id);
    require(typedId.has_value(), "test profile ID must be positive");
    return {.id = *typedId, .fields = std::move(value)};
}

class FakePolicy final : public TeacherProfileValidationPolicy
{
public:
    mutable std::vector<std::string>* calls = nullptr;
    mutable std::vector<Domain::TeacherProfile> inputs;
    Domain::TeacherProfileFields normalizedFields = fields();
    std::vector<TeacherProfileValidationIssue> issues;

    TeacherProfileValidationResult validate(
        const Domain::TeacherProfile& value
        ) const override
    {
        if (calls) calls->push_back("validate");
        inputs.push_back(value);
        return {
            .normalizedFields = normalizedFields,
            .issues = issues
        };
    }
};

class FakePort final : public TeacherProfileEditPersistencePort
{
public:
    mutable std::vector<std::string>* calls = nullptr;
    mutable std::vector<Domain::TeacherProfile> updates;
    mutable std::vector<Domain::TeacherId> reloads;
    Domain::TeacherProfile canonical = profile(1);
    bool updateSucceeds = true;
    bool reloadSucceeds = true;

    Domain::Result<void> update(
        const Domain::TeacherProfile& value
        ) const override
    {
        if (calls) calls->push_back("update");
        updates.push_back(value);
        if (!updateSucceeds)
            return Domain::Result<void>::failure(failure("update failed"));
        return Domain::Result<void>::success();
    }

    Domain::Result<Domain::TeacherProfile> reload(
        Domain::TeacherId id
        ) const override
    {
        if (calls) calls->push_back("reload");
        reloads.push_back(id);
        if (!reloadSucceeds)
            return Domain::Result<Domain::TeacherProfile>::failure(
                failure("reload failed"));
        return Domain::Result<Domain::TeacherProfile>::success(canonical);
    }
};

void invalidInputsDoNotWrite()
{
    FakePolicy policy;
    FakePort port;
    const std::vector<TeacherProfileValidationIssue> expectedIssues{
        validationIssue(
            "teacher.name.unusual",
            "teacherEn",
            TeacherProfileValidationSeverity::Warning,
            {{.key = "value", .value = u"Unknown name"}}
            ),
        validationIssue(
            "teacher.name.required",
            "teacherEn",
            TeacherProfileValidationSeverity::Error,
            {{.key = "value", .value = u""},
             {.key = "allowedValues", .value = u"teacherKr,teacherEn"}}
            )
    };
    policy.issues = expectedIssues;
    auto result = TeacherProfileEditUseCase::execute(
        TeacherProfileEditRequest::fromIntId(8, fields()), policy, port);
    require(!result && result.error().kind
                == TeacherProfileEditFailureKind::ValidationFailed,
        "policy rejection should be a validation failure");
    require(port.updates.empty() && port.reloads.empty(),
        "invalid profile must not write or reload");
    require(result.error().validationIssues == expectedIssues,
        "validation failure should preserve all structured issues and arguments");
    require(result.error().validationIssues[1].code == "teacher.name.required"
                && result.error().validationIssues[1].field == "teacherEn"
                && result.error().validationIssues[1].severity
                    == TeacherProfileValidationSeverity::Error,
        "validation issue code, field, and severity should remain intact");

    result = TeacherProfileEditUseCase::execute(
        TeacherProfileEditRequest::fromIntId(0, fields()), policy, port);
    require(!result && result.error().kind
                == TeacherProfileEditFailureKind::InvalidTeacherId,
        "nonpositive ID should be rejected");
    require(policy.inputs.size() == 1 && port.updates.empty()
                && port.reloads.empty(),
        "invalid ID must be rejected before policy and persistence");
}

void validSavePreservesFieldsOrderAndCanonicalReturn()
{
    std::vector<std::string> calls;
    FakePolicy policy;
    FakePort port;
    policy.calls = &calls;
    port.calls = &calls;
    const auto submitted = fields();
    auto normalized = submitted;
    normalized.teacherKr = u"Normalized KR";
    normalized.teacherEn = u"Normalized EN";
    normalized.preferredRomanization = u"Normalized Romanization";
    normalized.preferredName = u"Normalized Name";
    normalized.roomNumber = u"Room 20";
    normalized.birthday = u"04-21";
    normalized.phoneNumber = u"010-9876-5432";
    normalized.wifiName = u"Normalized WiFi";
    normalized.wifiPassword = u"normalized-wifi-secret";
    normalized.internetType = u"LAN";
    normalized.zoomId = u"normalized.zoom";
    normalized.zoomPassword = u"normalized-zoom-secret";
    normalized.projectionType = u"Any";
    normalized.notes = u"Normalized notes";
    policy.normalizedFields = normalized;
    policy.issues = {validationIssue(
        "teacher.name.normalized",
        "teacherEn",
        TeacherProfileValidationSeverity::Warning,
        {{.key = "canonicalValue", .value = u"Normalized EN"}}
        )};
    port.canonical = profile(8, {
        .teacherKr = u"Canonical KR", .teacherEn = u"Canonical EN",
        .preferredRomanization = u"Canonical Romanization",
        .preferredName = u"Canonical Name", .roomNumber = u"Room C",
        .birthday = u"04-21", .phoneNumber = u"+821012345678",
        .wifiName = u"Canonical WiFi", .wifiPassword = u"Canonical WiFi Secret",
        .internetType = u"LAN", .zoomId = u"canonical.zoom",
        .zoomPassword = u"Canonical Zoom Secret", .projectionType = u"Any",
        .notes = u"Canonical notes"});

    const auto result = TeacherProfileEditUseCase::execute(
        TeacherProfileEditRequest::fromIntId(8, submitted), policy, port);
    require(result.has_value(), "valid edit should succeed");
    require(calls == std::vector<std::string>{"validate", "update", "reload"},
        "use case call order should be validation, update, reload");
    require(policy.inputs.size() == 1
                && policy.inputs.front() == profile(8, submitted),
        "policy should receive all submitted fields before normalization");
    require(port.updates.size() == 1
                && port.updates.front() == profile(8, normalized),
        "update should receive all canonical fields returned by the policy");
    require(port.reloads.size() == 1 && port.reloads.front().value() == 8,
        "reload should use the checked typed ID");
    require(result.value() == port.canonical,
        "success should return the canonical reloaded profile");
}

void updateFailureDoesNotReload()
{
    FakePolicy policy;
    FakePort port;
    port.updateSucceeds = false;
    const auto result = TeacherProfileEditUseCase::execute(
        TeacherProfileEditRequest::fromIntId(8, fields()), policy, port);
    require(!result && result.error().kind
                == TeacherProfileEditFailureKind::UpdateFailed,
        "update failure should be distinguishable");
    require(!result.error().writeSucceeded && port.updates.size() == 1
                && port.reloads.empty(),
        "failed update should not reload or claim a successful write");
}

void reloadFailureReportsCompletedWrite()
{
    FakePolicy policy;
    FakePort port;
    port.reloadSucceeds = false;
    const auto result = TeacherProfileEditUseCase::execute(
        TeacherProfileEditRequest::fromIntId(8, fields()), policy, port);
    require(!result && result.error().kind
                == TeacherProfileEditFailureKind::ReloadFailed,
        "reload failure should be distinguishable");
    require(result.error().writeSucceeded && port.updates.size() == 1
                && port.reloads.size() == 1,
        "reload failure must report that update already succeeded");
}

void validationIssueArgumentsAreBounded()
{
    TeacherProfileValidationArguments arguments;
    for (std::size_t index = 0;
         index < TeacherProfileValidationArguments::MaximumCount;
         ++index)
    {
        require(arguments.add({.key = "value", .value = u"bounded"}),
            "arguments within the configured bound should be retained");
    }
    require(!arguments.add({.key = "overflow", .value = u"rejected"}),
        "argument count should be capped");

    TeacherProfileValidationArguments oversized;
    const std::u16string longValue(
        TeacherProfileValidationArguments::MaximumValueCodeUnits + 1,
        u'x'
        );
    require(!oversized.add({.key = "value", .value = longValue}),
        "argument UTF-16 length should be capped");
}

} // namespace

int main()
{
    try
    {
        invalidInputsDoNotWrite();
        validSavePreservesFieldsOrderAndCanonicalReturn();
        updateFailureDoesNotReload();
        reloadFailureReportsCompletedWrite();
        validationIssueArgumentsAreBounded();
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
