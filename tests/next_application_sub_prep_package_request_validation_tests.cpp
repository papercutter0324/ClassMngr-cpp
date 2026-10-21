#include "next/application/sub_prep_package_request_validation.h"

#include <cstdio>
#include <string>
#include <vector>

using ClassMngr::Next::Application::SubPrepGenerationPlanFailureReason;
using ClassMngr::Next::Application::SubPrepGenerationPlanRequest;
using ClassMngr::Next::Application::planSubPrepGeneration;
using ClassMngr::Next::Application::
    SubPrepPackageRequestValidationReason;
using ClassMngr::Next::Application::validateSubPrepPackageRequest;
using ClassMngr::Next::Application::ScheduleViewMode;
using ClassMngr::Next::Application::SubPrepWeekday;
using ClassMngr::Next::Domain::ClassId;
using ClassMngr::Next::Domain::GregorianDate;

using PlanReason = SubPrepGenerationPlanFailureReason;

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

std::optional<GregorianDate> date(
    const int year,
    const int month,
    const int day
    )
{
    return GregorianDate::fromComponents(year, month, day);
}

std::optional<ClassId> classId(const char* value)
{
    return ClassId::fromString(value);
}

SubPrepGenerationPlanRequest validPlanRequest()
{
    const auto firstClass = classId("42");
    const auto secondClass = classId("17");
    const auto july20 = date(2026, 7, 20);
    const auto july21 = date(2026, 7, 21);
    const auto july22 = date(2026, 7, 22);
    const auto july27 = date(2026, 7, 27);

    SubPrepGenerationPlanRequest request;
    if (firstClass && secondClass)
    {
        request.selectedClassIds = {*firstClass, *secondClass};
    }
    if (july20 && july21 && july22 && july27)
    {
        request.selectedDates = {
            *july22,
            *july20,
            *july21,
            *july27,
            *july20
        };
    }
    request.selectedScheduleWeekdays = {
        SubPrepWeekday::Thursday,
        SubPrepWeekday::Monday,
        SubPrepWeekday::Thursday
    };
    request.mode = ScheduleViewMode::Intensive;
    request.createFolder = true;
    request.printPaperCopies = true;
    request.openFolderAfterGeneration = true;
    request.requestedRosterColumns = {"Allergies", "Notes"};
    request.folderLabel = "Substitute Teacher";
    request.hasNonEmptyFolderLabel = true;
    request.targetRootReady = true;
    request.replaceExisting = true;
    return request;
}

bool createsDeterministicPlanWithSeparateVacationAndScheduleDays()
{
    const auto planned = planSubPrepGeneration(validPlanRequest());
    if (!planned.isValid() || !planned.plan)
    {
        return false;
    }

    const auto& plan = *planned.plan;
    const auto july20 = date(2026, 7, 20);
    const auto july21 = date(2026, 7, 21);
    const auto july22 = date(2026, 7, 22);
    const auto july27 = date(2026, 7, 27);
    if (!july20 || !july21 || !july22 || !july27)
    {
        return false;
    }

    if (plan.normalizedDates
        != std::vector<GregorianDate>{*july20, *july21, *july22, *july27})
    {
        return false;
    }
    if (plan.rosterWeekdays
        != std::vector<SubPrepWeekday>{
            SubPrepWeekday::Monday,
            SubPrepWeekday::Tuesday,
            SubPrepWeekday::Wednesday
        })
    {
        return false;
    }
    if (plan.selectedScheduleWeekdays
        != std::vector<SubPrepWeekday>{
            SubPrepWeekday::Thursday,
            SubPrepWeekday::Monday
        })
    {
        return false;
    }

    const auto class42 = classId("42");
    const auto class17 = classId("17");
    if (!class42 || !class17
        || plan.selectedClassIds != std::vector<ClassId>{*class42, *class17})
    {
        return false;
    }
    if (plan.mode != ScheduleViewMode::Intensive
        || plan.requestedRosterColumns
            != std::vector<std::string>{"Allergies", "Notes"})
    {
        return false;
    }
    if (!plan.output.createFolder || !plan.output.printPaperCopies
        || !plan.output.folder)
    {
        return false;
    }

    return *plan.output.folder
        == ClassMngr::Next::Application::SubPrepFolderOutputPlan{
            .folderLabel = "Substitute Teacher",
            .targetRootReady = true,
            .replaceExisting = true,
            .openAfterGeneration = true
        };
}

bool preservesActionSpecificValidationAndPrecedence()
{
    SubPrepGenerationPlanRequest noAction;
    noAction.selectedDates = validPlanRequest().selectedDates;
    noAction.selectedClassIds = validPlanRequest().selectedClassIds;
    const auto noOutput = planSubPrepGeneration(noAction);
    if (noOutput.reason != PlanReason::NoOutputAction || noOutput.plan)
    {
        return false;
    }

    auto noDate = validPlanRequest();
    noDate.selectedDates.clear();
    noDate.selectedClassIds.clear();
    noDate.targetRootReady = false;
    noDate.hasNonEmptyFolderLabel = false;
    if (planSubPrepGeneration(noDate).reason
        != PlanReason::NoValidSelectedDate)
    {
        return false;
    }

    auto noClasses = validPlanRequest();
    noClasses.selectedClassIds.clear();
    if (planSubPrepGeneration(noClasses).reason
        != PlanReason::NoSelectedClassIds)
    {
        return false;
    }

    auto noTarget = validPlanRequest();
    noTarget.printPaperCopies = false;
    noTarget.targetRootReady = false;
    if (planSubPrepGeneration(noTarget).reason
        != PlanReason::MissingTargetRoot)
    {
        return false;
    }

    auto noName = validPlanRequest();
    noName.printPaperCopies = false;
    noName.hasNonEmptyFolderLabel = false;
    if (planSubPrepGeneration(noName).reason
        != PlanReason::MissingUserName)
    {
        return false;
    }

    auto invalidScheduleDay = validPlanRequest();
    invalidScheduleDay.selectedScheduleWeekdays = {
        static_cast<SubPrepWeekday>(0)
    };
    if (planSubPrepGeneration(invalidScheduleDay).reason
        != PlanReason::InvalidSelectedScheduleWeekday)
    {
        return false;
    }

    auto invalidMode = validPlanRequest();
    invalidMode.mode = static_cast<ScheduleViewMode>(99);
    return planSubPrepGeneration(invalidMode).reason
        == PlanReason::InvalidScheduleMode;
}

bool supportsPrintOnlyWithoutFolderInputs()
{
    auto request = validPlanRequest();
    request.createFolder = false;
    request.printPaperCopies = true;
    request.openFolderAfterGeneration = false;
    request.folderLabel.clear();
    request.hasNonEmptyFolderLabel = false;
    request.targetRootReady = false;
    request.replaceExisting = false;

    const auto planned = planSubPrepGeneration(request);
    return planned.isValid()
        && planned.plan
        && !planned.plan->output.createFolder
        && planned.plan->output.printPaperCopies
        && !planned.plan->output.folder;
}

bool supportsFolderOnlyOutput()
{
    auto request = validPlanRequest();
    request.createFolder = true;
    request.printPaperCopies = false;

    const auto planned = planSubPrepGeneration(request);
    return planned.isValid()
        && planned.plan
        && planned.plan->output.createFolder
        && !planned.plan->output.printPaperCopies
        && planned.plan->output.folder
        && planned.plan->output.folder->targetRootReady
        && planned.plan->output.folder->replaceExisting;
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

    if (!createsDeterministicPlanWithSeparateVacationAndScheduleDays())
    {
        std::fputs("Sub Prep generation planning did not preserve its typed selections.\n", stderr);
        return 1;
    }

    if (!preservesActionSpecificValidationAndPrecedence())
    {
        std::fputs("Sub Prep generation-plan validation failed.\n", stderr);
        return 1;
    }

    if (!supportsPrintOnlyWithoutFolderInputs())
    {
        std::fputs("Sub Prep print-only planning required folder inputs.\n", stderr);
        return 1;
    }

    if (!supportsFolderOnlyOutput())
    {
        std::fputs("Sub Prep folder-only planning failed.\n", stderr);
        return 1;
    }

    return 0;
}
