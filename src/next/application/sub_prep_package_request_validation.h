#pragma once

namespace ClassMngr::Next::Application
{

enum class SubPrepPackageRequestValidationReason
{
    None,
    NoOutputAction,
    NoValidSelectedDate,
    NoSelectedClassIds,
    MissingTargetRoot,
    MissingUserName
};

struct SubPrepPackageRequestValidationRequest
{
    bool createFolder = false;
    bool printPaperCopies = false;
    bool hasValidSelectedDate = false;
    bool hasSelectedClassIds = false;
    bool hasNonEmptyTargetRoot = false;
    bool hasNonEmptyUserName = false;
};

struct SubPrepPackageRequestValidationResult
{
    SubPrepPackageRequestValidationReason reason =
        SubPrepPackageRequestValidationReason::None;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return reason == SubPrepPackageRequestValidationReason::None;
    }
};

[[nodiscard]] constexpr SubPrepPackageRequestValidationResult
validateSubPrepPackageRequest(
    const SubPrepPackageRequestValidationRequest& request
    ) noexcept
{
    using Reason = SubPrepPackageRequestValidationReason;

    if (!request.createFolder && !request.printPaperCopies)
    {
        return {Reason::NoOutputAction};
    }

    if (!request.hasValidSelectedDate)
    {
        return {Reason::NoValidSelectedDate};
    }

    if (!request.hasSelectedClassIds)
    {
        return {Reason::NoSelectedClassIds};
    }

    if (request.createFolder && !request.hasNonEmptyTargetRoot)
    {
        return {Reason::MissingTargetRoot};
    }

    if (request.createFolder && !request.hasNonEmptyUserName)
    {
        return {Reason::MissingUserName};
    }

    return {Reason::None};
}

} // namespace ClassMngr::Next::Application
