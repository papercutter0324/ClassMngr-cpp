#pragma once

#include "next/application/my_classes_class_information_snapshot.h"
#include "next/domain/operation_result.h"

#include <optional>
#include <vector>

namespace ClassMngr::Next::Application
{

struct MyClassesClassSummaryFields final
{
    std::u16string classGrade;
    std::u16string classLevel;
    std::vector<MyClassesScheduleEntry> regularSchedule;
    std::vector<MyClassesScheduleEntry> intensiveSchedule;
    std::optional<Domain::TeacherId> teacherId;

    friend bool operator==(
        const MyClassesClassSummaryFields&,
        const MyClassesClassSummaryFields&
        ) = default;
};

struct MyClassesClassSummaryBatchReadEntry final
{
    Domain::ClassId classId;
    Domain::Result<MyClassesClassSummaryFields> summary;
};

using MyClassesClassSummaryBatchReadResult =
    Domain::Result<std::vector<MyClassesClassSummaryBatchReadEntry>>;

} // namespace ClassMngr::Next::Application
