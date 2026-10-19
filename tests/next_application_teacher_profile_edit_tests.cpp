#include "next/application/teacher_profile_edit_use_case.h"

#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

static_assert(!std::is_same_v<
    TeacherProfileValidationCode,
    TeacherProfileValidationField>);
static_assert(!std::is_same_v<
    TeacherProfileValidationCode,
    TeacherProfileValidationArgumentKey>);
static_assert(!std::is_same_v<
    TeacherProfileValidationField,
    TeacherProfileValidationArgumentKey>);
static_assert(!std::is_convertible_v<std::string, TeacherProfileValidationCode>);
static_assert(!std::is_convertible_v<std::string, TeacherProfileValidationField>);
static_assert(!std::is_convertible_v<
    std::string,
    TeacherProfileValidationArgumentKey>);
static_assert(std::is_same_v<
    decltype(TeacherProfileValidationArgument::value),
    std::u16string>);

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

TeacherProfileValidationArgument validationArgument(
    std::string key,
    std::u16string value
    )
{
    return {
        .key = TeacherProfileValidationArgumentKey(std::move(key)),
        .value = std::move(value)
    };
}

TeacherProfileValidationIssue validationIssue(
    std::string code,
    std::string field,
    TeacherProfileValidationSeverity severity,
    std::initializer_list<TeacherProfileValidationArgument> arguments = {}
    )
{
    TeacherProfileValidationIssue issue{
        .code = TeacherProfileValidationCode(std::move(code)),
        .field = TeacherProfileValidationField(std::move(field)),
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
    const auto typedId = Domain::TeacherId::fromString(std::to_string(id));
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
    std::vector<std::string> calls;
    FakePolicy policy;
    FakePort port;
    policy.calls = &calls;
    port.calls = &calls;
    const std::vector<TeacherProfileValidationIssue> expectedIssues{
        validationIssue(
            "teacher.name.unusual",
            "teacherEn",
            TeacherProfileValidationSeverity::Warning,
            {validationArgument("value", u"Unknown name")}
            ),
        validationIssue(
            "teacher.name.required",
            "teacherEn",
            TeacherProfileValidationSeverity::Error,
            {validationArgument("value", u""),
             validationArgument("allowedValues", u"teacherKr,teacherEn")}
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
    require(result.error().validationIssues[1].code.value() == "teacher.name.required"
                && result.error().validationIssues[1].field.value() == "teacherEn"
                && result.error().validationIssues[1].severity
                    == TeacherProfileValidationSeverity::Error,
        "validation issue code, field, and severity should remain intact");

    const auto& returnedIssues = result.error().validationIssues;
    const auto firstArguments = returnedIssues[0].arguments.values();
    const auto secondArguments = returnedIssues[1].arguments.values();
    require(returnedIssues[0].code.value() == "teacher.name.unusual"
                && returnedIssues[0].field.value() == "teacherEn"
                && returnedIssues[0].severity
                    == TeacherProfileValidationSeverity::Warning
                && firstArguments.size() == 1
                && firstArguments[0].key.value() == "value"
                && firstArguments[0].value == u"Unknown name",
            "first validation issue code, field, and argument payload should remain ordered");
    require(returnedIssues[1].code.value() == "teacher.name.required"
                && returnedIssues[1].field.value() == "teacherEn"
                && returnedIssues[1].severity
                    == TeacherProfileValidationSeverity::Error
                && secondArguments.size() == 2
                && secondArguments[0].key.value() == "value"
                && secondArguments[0].value.empty()
                && secondArguments[1].key.value() == "allowedValues"
                && secondArguments[1].value == u"teacherKr,teacherEn",
            "second validation issue should preserve complete ordered arguments");

    require(calls == std::vector<std::string>{"validate"},
        "validation rejection should call the policy without persistence");

    const auto requireInvalidId = [&](TeacherProfileEditRequest request)
    {
        const auto callsBefore = calls;
        const auto policyInputsBefore = policy.inputs.size();
        const auto updatesBefore = port.updates.size();
        const auto reloadsBefore = port.reloads.size();
        const auto invalidResult = TeacherProfileEditUseCase::execute(
            request, policy, port);
        require(!invalidResult && invalidResult.error().kind
                    == TeacherProfileEditFailureKind::InvalidTeacherId,
            "invalid teacher ID should be rejected");
        require(invalidResult.error().cause.code == Domain::ErrorCode::InvalidInput
                    && invalidResult.error().cause.message
                        == "Teacher ID must be positive."
                    && invalidResult.error().cause.recoverable,
            "invalid teacher ID should preserve its exact recoverable cause");
        require(!invalidResult.error().writeSucceeded,
            "invalid teacher ID must not report a completed write");
        require(calls == callsBefore
                    && policy.inputs.size() == policyInputsBefore,
            "invalid teacher ID must not call the validation policy");
        require(port.updates.size() == updatesBefore
                    && port.reloads.size() == reloadsBefore,
            "invalid teacher ID must not update or reload persistence");
    };

    requireInvalidId(TeacherProfileEditRequest::fromIntId(0, fields()));

    for (const auto* value : {"0", "08", "not-an-id", "-1", "+8", " 8", "8 ", "9223372036854775808"})
    {
        auto typedId = Domain::TeacherId::fromString(value);
        require(typedId.has_value(),
            "test malformed teacher ID should construct a present typed ID");
        requireInvalidId({.id = std::move(typedId), .fields = fields()});
    }
}

void unknownValidationSeverityBlocksPersistence()
{
    std::vector<std::string> calls;
    FakePolicy policy;
    FakePort port;
    policy.calls = &calls;
    port.calls = &calls;
    const std::vector<TeacherProfileValidationIssue> expectedIssues{
        validationIssue(
            "teacher.profile.unknown-severity",
            "teacherEn",
            static_cast<TeacherProfileValidationSeverity>(99),
            {validationArgument("submittedValue", u"Unclassified"),
             validationArgument("policy", u"legacy")}
            )
    };
    policy.issues = expectedIssues;

    const auto result = TeacherProfileEditUseCase::execute(
        TeacherProfileEditRequest::fromIntId(8, fields()), policy, port);

    require(!result && result.error().kind
                == TeacherProfileEditFailureKind::ValidationFailed,
        "unknown validation severity should fail validation");
    require(result.error().cause.code == Domain::ErrorCode::Validation
                && result.error().cause.message
                    == "Teacher profile validation failed."
                && result.error().cause.recoverable,
        "unknown severity should preserve the existing validation failure cause");
    require(result.error().validationIssues == expectedIssues,
        "unknown severity failure should preserve the complete issue payload");
    require(port.updates.empty() && port.reloads.empty(),
        "unknown validation severity must not update or reload persistence");
    require(calls == std::vector<std::string>{"validate"},
        "unknown severity should call only the validation policy");
}

void wideIdentitySurvivesPolicyUpdateAndReload()
{
    for (const std::string value : {"1", "2147483647", "2147483648", "9223372036854775807"})
    {
        FakePolicy policy;
        FakePort port;
        const auto id = Domain::TeacherId::fromString(value);
        require(id.has_value(), "boundary ID must construct");
        port.canonical = {.id = *id, .fields = fields()};
        const auto result = TeacherProfileEditUseCase::execute(
            {.id = *id, .fields = fields()}, policy, port);
        require(result.has_value(), "canonical signed 64-bit ID must save");
        require(result->id == *id && policy.inputs.front().id == *id
            && port.updates.front().id == *id && port.reloads.front() == *id,
            "edit must preserve exact identity across every boundary");
    }
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
        {validationArgument("canonicalValue", u"Normalized EN")}
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
    require(port.reloads.size() == 1 && port.reloads.front().value() == "8",
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

void reloadIdentifierMismatchReportsCompletedWrite()
{
    std::vector<std::string> calls;
    FakePolicy policy;
    FakePort port;
    policy.calls = &calls;
    port.calls = &calls;
    port.canonical = profile(9);

    const auto result = TeacherProfileEditUseCase::execute(
        TeacherProfileEditRequest::fromIntId(8, fields()), policy, port);

    require(!result && result.error().kind
                == TeacherProfileEditFailureKind::ReloadFailed,
        "mismatched reload identity should be a reload failure");
    require(result.error().cause.code == Domain::ErrorCode::Validation,
        "mismatched reload identity should report a validation error");
    require(result.error().cause.message
                == "Teacher profile edit reload returned a different teacher identifier.",
        "mismatched reload identity should preserve its exact message");
    require(!result.error().cause.recoverable,
        "mismatched reload identity should be nonrecoverable");
    require(result.error().writeSucceeded,
        "mismatched reload identity should report the completed write");
    require(calls == std::vector<std::string>{"validate", "update", "reload"},
        "mismatched reload should preserve validation, update, reload ordering");
    require(policy.inputs.size() == 1
                && policy.inputs.front() == profile(8, fields()),
        "validation should receive the requested teacher ID");
    require(port.updates.size() == 1
                && port.updates.front().id.value() == "8",
        "exactly one update should receive the requested teacher ID");
    require(port.reloads.size() == 1 && port.reloads.front().value() == "8",
        "exactly one reload should request the requested teacher ID");
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
    const std::vector<std::string> expectedKeys{
        "", "duplicate", "middle", "duplicate", "tail", "fifth", "sixth", "seventh"
    };
    const std::vector<std::u16string> expectedValues{
        u"", u"first", u"second", u"third", u"fourth", u"fifth", u"sixth", u"seventh"
    };
    for (std::size_t index = 0;
         index < TeacherProfileValidationArguments::MaximumCount;
         ++index)
    {
        require(arguments.add(validationArgument(
                    expectedKeys[index], expectedValues[index])),
            "arguments within the configured bound should be retained");
    }
    const auto orderedValues = arguments.values();
    require(orderedValues.size() == TeacherProfileValidationArguments::MaximumCount,
        "the maximum argument count should be retained");
    for (std::size_t index = 0; index < expectedKeys.size(); ++index)
    {
        require(orderedValues[index].key.value() == expectedKeys[index]
                    && orderedValues[index].value == expectedValues[index],
            "argument keys and UTF-16 values should retain vector order");
    }
    require(!arguments.add(validationArgument("overflow", u"rejected")),
        "argument count should be capped");

    std::string exactKey;
    for (int index = 0; index < 16; ++index)
    {
        exactKey.append("\xF0\x9F\x8C\x8D", 4);
    }
    require(exactKey.size() == 64,
        "multibyte validation argument key should total 64 bytes");
    TeacherProfileValidationArguments exactKeyArguments;
    require(exactKeyArguments.add(validationArgument(exactKey, u"accepted")),
        "64-byte UTF-8 argument key should be accepted");
    require(exactKeyArguments.values().front().key.value() == exactKey,
        "multibyte argument key should retain exact bytes");

    TeacherProfileValidationArguments oversizedKeyArguments;
    require(!oversizedKeyArguments.add(
                validationArgument(exactKey + "x", u"rejected")),
        "65-byte UTF-8 argument key should be rejected");

    const std::u16string exactValue(
        TeacherProfileValidationArguments::MaximumValueCodeUnits,
        u'x');
    TeacherProfileValidationArguments exactValueArguments;
    require(exactValueArguments.add(validationArgument("value", exactValue)),
        "256 UTF-16 code units should be accepted");
    require(exactValueArguments.values().front().value == exactValue,
        "UTF-16 argument value should be retained exactly");

    TeacherProfileValidationArguments oversized;
    const std::u16string longValue(
        TeacherProfileValidationArguments::MaximumValueCodeUnits + 1,
        u'x'
        );
    require(!oversized.add(validationArgument("value", longValue)),
        "argument UTF-16 length should be capped");
}

void validationMetadataWrappersPreserveOpaqueValues()
{
    const TeacherProfileValidationCode emptyCode;
    const TeacherProfileValidationField emptyField;
    const TeacherProfileValidationArgumentKey emptyKey;
    require(emptyCode.value().empty()
                && emptyField.value().empty()
                && emptyKey.value().empty(),
        "validation metadata wrappers should permit empty values");

    const std::string arbitraryBytes("\xFF\0\xC3", 3);
    const TeacherProfileValidationCode arbitraryCode(arbitraryBytes);
    const TeacherProfileValidationField arbitraryField(arbitraryBytes);
    const TeacherProfileValidationArgumentKey arbitraryKey(arbitraryBytes);
    require(arbitraryCode.value() == arbitraryBytes
                && arbitraryField.value() == arbitraryBytes
                && arbitraryKey.value() == arbitraryBytes,
        "validation metadata wrappers should preserve arbitrary bytes exactly");

    const TeacherProfileValidationCode unknownCode("future.validation.rule");
    const TeacherProfileValidationField unknownField("futureProfileField");
    const TeacherProfileValidationArgumentKey unknownKey("futureArgument");
    require(unknownCode.value() == "future.validation.rule"
                && unknownField.value() == "futureProfileField"
                && unknownKey.value() == "futureArgument",
        "validation metadata wrappers should preserve unknown values without filtering");
}

} // namespace

int main()
{
    try
    {
        invalidInputsDoNotWrite();
        unknownValidationSeverityBlocksPersistence();
        wideIdentitySurvivesPolicyUpdateAndReload();
        validSavePreservesFieldsOrderAndCanonicalReturn();
        updateFailureDoesNotReload();
        reloadIdentifierMismatchReportsCompletedWrite();
        reloadFailureReportsCompletedWrite();
        validationMetadataWrappersPreserveOpaqueValues();
        validationIssueArgumentsAreBounded();
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
