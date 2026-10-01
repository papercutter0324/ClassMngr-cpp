#include "next/application/class_day_filter_policy.h"

#include <cstdlib>
#include <string>
#include <vector>

using ClassMngr::Next::Application::ClassDayFilterPolicy;
using ClassMngr::Next::Application::ClassDayScheduleDays;
using ClassMngr::Next::Application::ClassDayScheduleSource;
using ClassMngr::Next::Application::ClassDayVisibilityScope;
using ClassMngr::Next::Application::classDayFilterMatches;

int main()
{
    const ClassDayScheduleDays emptySchedule;
    const ClassDayFilterPolicy allClasses;
    if (!classDayFilterMatches(allClasses, emptySchedule))
    {
        return EXIT_FAILURE;
    }

    ClassDayFilterPolicy activeSchedule = allClasses;
    activeSchedule.visibilityScope = ClassDayVisibilityScope::ActiveSchedule;
    if (classDayFilterMatches(activeSchedule, emptySchedule))
    {
        return EXIT_FAILURE;
    }

    const ClassDayScheduleDays regularMonday{
        .regularDayKeys = {"monday"},
        .intensiveDayKeys = {}
    };
    if (!classDayFilterMatches(activeSchedule, regularMonday))
    {
        return EXIT_FAILURE;
    }

    ClassDayFilterPolicy normalizedMonday = allClasses;
    // The feature edge converts raw values such as " Monday " to this key;
    // whitespace-only input becomes an empty key and is ignored.
    normalizedMonday.selectedDayKeys = {"", "monday"};
    if (!classDayFilterMatches(normalizedMonday, regularMonday))
    {
        return EXIT_FAILURE;
    }
    if (classDayFilterMatches(
            normalizedMonday,
            ClassDayScheduleDays{
                .regularDayKeys = {"tuesday"},
                .intensiveDayKeys = {}
            }
            ))
    {
        return EXIT_FAILURE;
    }

    ClassDayFilterPolicy orFilter = allClasses;
    orFilter.selectedDayKeys = {"monday", "wednesday"};
    if (!classDayFilterMatches(
            orFilter,
            ClassDayScheduleDays{
                .regularDayKeys = {"friday", "wednesday"},
                .intensiveDayKeys = {}
            }
            )
        || classDayFilterMatches(
            orFilter,
            ClassDayScheduleDays{
                .regularDayKeys = {"friday"},
                .intensiveDayKeys = {}
            }
            ))
    {
        return EXIT_FAILURE;
    }

    for (const std::string alias : {"weekend", "wkend"})
    {
        ClassDayFilterPolicy weekendFilter = allClasses;
        weekendFilter.selectedDayKeys = {alias};
        if (!classDayFilterMatches(
                weekendFilter,
                ClassDayScheduleDays{
                    .regularDayKeys = {"saturday"},
                    .intensiveDayKeys = {}
                }
                )
            || !classDayFilterMatches(
                weekendFilter,
                ClassDayScheduleDays{
                    .regularDayKeys = {"sunday"},
                    .intensiveDayKeys = {}
                }
                )
            || classDayFilterMatches(
                   weekendFilter,
                   ClassDayScheduleDays{
                       .regularDayKeys = {"friday"},
                       .intensiveDayKeys = {}
                   }
                   ))
        {
            return EXIT_FAILURE;
        }
    }

    ClassDayFilterPolicy sourceFilter = allClasses;
    sourceFilter.selectedDayKeys = {"saturday"};
    const ClassDayScheduleDays splitSchedule{
        .regularDayKeys = {"monday"},
        .intensiveDayKeys = {"saturday"}
    };
    if (classDayFilterMatches(sourceFilter, splitSchedule))
    {
        return EXIT_FAILURE;
    }
    sourceFilter.scheduleSource = ClassDayScheduleSource::Intensive;
    if (!classDayFilterMatches(sourceFilter, splitSchedule))
    {
        return EXIT_FAILURE;
    }

    ClassDayFilterPolicy unknownDay = allClasses;
    unknownDay.selectedDayKeys = {"holiday"};
    if (classDayFilterMatches(unknownDay, regularMonday)
        || !classDayFilterMatches(
            unknownDay,
            ClassDayScheduleDays{
                .regularDayKeys = {"holiday"},
                .intensiveDayKeys = {}
            }
            ))
    {
        return EXIT_FAILURE;
    }

    ClassDayFilterPolicy activeIntensive = activeSchedule;
    activeIntensive.scheduleSource = ClassDayScheduleSource::Intensive;
    if (classDayFilterMatches(activeIntensive, regularMonday)
        || !classDayFilterMatches(
            activeIntensive,
            ClassDayScheduleDays{
                .regularDayKeys = {"monday"},
                .intensiveDayKeys = {""}
            }
            ))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
