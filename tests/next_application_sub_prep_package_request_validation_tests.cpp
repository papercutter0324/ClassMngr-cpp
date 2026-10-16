#include "next/application/sub_prep_package_request_validation.h"

#include <cstdio>

using ClassMngr::Next::Application::
    SubPrepPackageRequestValidationReason;
using ClassMngr::Next::Application::validateSubPrepPackageRequest;

namespace
{

bool returnsFirstFailureInSharedPreflightOrder()
{
    const auto noAction = validateSubPrepPackageRequest({});
    if (noAction.reason != SubPrepPackageRequestValidationReason::NoOutputAction)
    {
        return false;
    }

    const auto noDate = validateSubPrepPackageRequest(
        {
            .createFolder = true,
            .hasValidSelectedDate = false,
            .hasSelectedClassIds = false
        }
        );
    if (noDate.reason
        != SubPrepPackageRequestValidationReason::NoValidSelectedDate)
    {
        return false;
    }

    const auto noClasses = validateSubPrepPackageRequest(
        {
            .createFolder = true,
            .hasValidSelectedDate = true,
            .hasSelectedClassIds = false,
            .hasNonEmptyTargetRoot = false,
            .hasNonEmptyUserName = false
        }
        );
    if (noClasses.reason
        != SubPrepPackageRequestValidationReason::NoSelectedClassIds)
    {
        return false;
    }

    const auto noRoot = validateSubPrepPackageRequest(
        {
            .createFolder = true,
            .hasValidSelectedDate = true,
            .hasSelectedClassIds = true,
            .hasNonEmptyTargetRoot = false,
            .hasNonEmptyUserName = false
        }
        );
    if (noRoot.reason
        != SubPrepPackageRequestValidationReason::MissingTargetRoot)
    {
        return false;
    }

    const auto noName = validateSubPrepPackageRequest(
        {
            .createFolder = true,
            .hasValidSelectedDate = true,
            .hasSelectedClassIds = true,
            .hasNonEmptyTargetRoot = true,
            .hasNonEmptyUserName = false
        }
        );
    return noName.reason
        == SubPrepPackageRequestValidationReason::MissingUserName;
}

bool requiresOnlyFieldsRelevantToSelectedAction()
{
    const auto printOnly = validateSubPrepPackageRequest(
        {
            .printPaperCopies = true,
            .hasValidSelectedDate = true,
            .hasSelectedClassIds = true
        }
        );
    if (!printOnly.isValid())
    {
        return false;
    }

    const auto folderOnly = validateSubPrepPackageRequest(
        {
            .createFolder = true,
            .hasValidSelectedDate = true,
            .hasSelectedClassIds = true,
            .hasNonEmptyTargetRoot = true,
            .hasNonEmptyUserName = true
        }
        );
    if (!folderOnly.isValid())
    {
        return false;
    }

    const auto bothActions = validateSubPrepPackageRequest(
        {
            .createFolder = true,
            .printPaperCopies = true,
            .hasValidSelectedDate = true,
            .hasSelectedClassIds = true,
            .hasNonEmptyTargetRoot = true,
            .hasNonEmptyUserName = true
        }
        );
    return bothActions.isValid();
}

bool rejectsEachMissingSharedInput()
{
    const auto noDate = validateSubPrepPackageRequest(
        {
            .printPaperCopies = true,
            .hasSelectedClassIds = true
        }
        );
    const auto noClasses = validateSubPrepPackageRequest(
        {
            .printPaperCopies = true,
            .hasValidSelectedDate = true
        }
        );

    return noDate.reason
            == SubPrepPackageRequestValidationReason::NoValidSelectedDate
        && noClasses.reason
            == SubPrepPackageRequestValidationReason::NoSelectedClassIds;
}

} // namespace

int main()
{
    if (!returnsFirstFailureInSharedPreflightOrder())
    {
        std::fputs("Shared Sub Prep preflight precedence failed.\n", stderr);
        return 1;
    }

    if (!requiresOnlyFieldsRelevantToSelectedAction())
    {
        std::fputs("Shared Sub Prep action-specific inputs failed.\n", stderr);
        return 1;
    }

    if (!rejectsEachMissingSharedInput())
    {
        std::fputs("Shared Sub Prep required-input checks failed.\n", stderr);
        return 1;
    }

    return 0;
}
