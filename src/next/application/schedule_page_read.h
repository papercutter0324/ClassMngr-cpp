#pragma once

#include "next/application/schedule_builder_source_snapshot.h"
#include "next/application/schedule_display_mode_preferences.h"
#include "next/application/schedule_display_preferences.h"
#include "next/application/schedule_slot_state_read_query.h"
#include "next/application/schedule_testing_assignment_read_query.h"

namespace ClassMngr::Next::Application
{

struct SchedulePageReadSnapshot final
{
    ScheduleBuilderSourceSnapshot source;
    ScheduleDisplayPreferences displayPreferences;
    ScheduleDisplayMode displayMode = ScheduleDisplayMode::Regular;
    ScheduleSlotStateReadSnapshot slotStates;
    ScheduleTestingAssignmentReadSnapshot testingAssignments;

    friend bool operator==(
        const SchedulePageReadSnapshot&,
        const SchedulePageReadSnapshot&
        ) = default;
};

using SchedulePageReadResult = Domain::Result<SchedulePageReadSnapshot>;

class SchedulePageReadPort
    : public ScheduleBuilderSourceReadPort,
      public ScheduleSlotStateReadPort,
      public ScheduleTestingAssignmentReadPort
{
public:
    virtual ~SchedulePageReadPort() = default;

    [[nodiscard]] virtual ScheduleBuilderSourceResult readScheduleClasses(
        const ScheduleBuilderSourceQuery& query
        ) const override = 0;
    [[nodiscard]] virtual ScheduleDisplayPreferencesResult
    readDisplayPreferences() const = 0;
    [[nodiscard]] virtual ScheduleDisplayModePreferencesLoadResult
    readDisplayMode() const = 0;
    [[nodiscard]] virtual ScheduleSlotStateReadResult readSlotStates(
        const ScheduleSlotStateReadQuery& query
        ) const override = 0;
    [[nodiscard]] virtual ScheduleTestingAssignmentReadResult
    readTestingAssignments(
        const ScheduleTestingAssignmentReadQuery& query
        ) const override = 0;
};

class SchedulePageReadUseCase final
{
public:
    [[nodiscard]] static SchedulePageReadResult execute(
        const SchedulePageReadPort& port
        )
    {
        ScheduleBuilderSourceResult source =
            ScheduleBuilderSourceQueryHandler::execute({}, port);
        if (!source)
        {
            return SchedulePageReadResult::failure(source.error());
        }

        const ScheduleDisplayPreferencesResult preferences =
            port.readDisplayPreferences();
        if (!preferences)
        {
            return SchedulePageReadResult::failure(preferences.error());
        }

        const ScheduleDisplayModePreferencesLoadResult mode =
            port.readDisplayMode();
        if (!mode)
        {
            return SchedulePageReadResult::failure(mode.error());
        }

        const ScheduleSlotStateReadResult slotStates =
            ScheduleSlotStateReadQueryHandler::execute({}, port);
        if (!slotStates)
        {
            return SchedulePageReadResult::failure(slotStates.error());
        }

        const ScheduleTestingAssignmentReadResult assignments =
            ScheduleTestingAssignmentReadQueryHandler::execute({}, port);
        if (!assignments)
        {
            return SchedulePageReadResult::failure(assignments.error());
        }

        return SchedulePageReadResult::success({
            .source = std::move(source.value()),
            .displayPreferences = preferences.value(),
            .displayMode = mode.value(),
            .slotStates = std::move(slotStates.value()),
            .testingAssignments = std::move(assignments.value())
        });
    }
};

} // namespace ClassMngr::Next::Application
