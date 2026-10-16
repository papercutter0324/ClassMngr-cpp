#include "next/application/sub_prep_navigation_metadata_policy.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace ClassMngr::Next::Application;

namespace
{

struct Metadata final
{
    std::string label;
    int firstDayOrder;
    int firstTimeOrder;

    friend bool operator==(const Metadata&, const Metadata&) = default;
};

bool regularRequestSelectsRegularCandidateAtomically()
{
    const Metadata regular{"regular label", 1, 101};
    const Metadata intensive{"intensive label", 3, 303};
    const Metadata selected{"selected label", 5, 505};

    return selectSubPrepNavigationMetadata(
               ScheduleViewMode::Regular,
               &regular,
               &intensive,
               selected
               ) == regular;
}

bool regularRequestDoesNotSelectIntensiveOnlyCandidate()
{
    const Metadata intensive{"intensive label", 3, 303};
    const Metadata selected{"regular selected-mode label", 1, 101};

    return selectSubPrepNavigationMetadata(
               ScheduleViewMode::Regular,
               static_cast<const Metadata*>(nullptr),
               &intensive,
               selected
               ) == selected;
}

bool regularRecordPresenceWinsIntensiveRequestEvenWithUnusableMetadata()
{
    const Metadata regular{"", -1, -1};
    const Metadata intensive{"usable intensive label", 3, 303};
    const Metadata selected{"selected label", 5, 505};

    return selectSubPrepNavigationMetadata(
               ScheduleViewMode::Intensive,
               &regular,
               &intensive,
               selected
               ) == regular;
}

bool intensiveRequestUsesIntensiveCandidateWhenRegularIsAbsent()
{
    const Metadata intensive{"intensive label", 3, 303};
    const Metadata selected{"selected label", 5, 505};

    return selectSubPrepNavigationMetadata(
               ScheduleViewMode::Intensive,
               static_cast<const Metadata*>(nullptr),
               &intensive,
               selected
               ) == intensive;
}

bool selectedModeIsFallbackWhenNoApplicableAllWeekCandidateExists()
{
    const Metadata intensive{"intensive label", 3, 303};
    const Metadata selected{"selected label", 5, 505};

    return selectSubPrepNavigationMetadata(
               ScheduleViewMode::Intensive,
               static_cast<const Metadata*>(nullptr),
               static_cast<const Metadata*>(nullptr),
               selected
               ) == selected
        && selectSubPrepNavigationMetadata(
               ScheduleViewMode::Regular,
               static_cast<const Metadata*>(nullptr),
               &intensive,
               selected
               ) == selected;
}

} // namespace

int main()
{
    if (!regularRequestSelectsRegularCandidateAtomically())
    {
        std::cerr << "Regular mode should select all-week Regular metadata atomically.\n";
        return EXIT_FAILURE;
    }
    if (!regularRequestDoesNotSelectIntensiveOnlyCandidate())
    {
        std::cerr << "Regular mode should ignore an Intensive-only candidate.\n";
        return EXIT_FAILURE;
    }
    if (!regularRecordPresenceWinsIntensiveRequestEvenWithUnusableMetadata())
    {
        std::cerr << "An existing Regular record should win even with unusable metadata.\n";
        return EXIT_FAILURE;
    }
    if (!intensiveRequestUsesIntensiveCandidateWhenRegularIsAbsent())
    {
        std::cerr << "Intensive mode should use all-week Intensive metadata as fallback.\n";
        return EXIT_FAILURE;
    }
    if (!selectedModeIsFallbackWhenNoApplicableAllWeekCandidateExists())
    {
        std::cerr << "Selected-mode metadata should be the applicable fallback.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
