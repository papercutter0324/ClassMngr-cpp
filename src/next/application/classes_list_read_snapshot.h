#pragma once

#include "next/domain/domain_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kClassesNavigationMaximumClasses = 4'096;
inline constexpr std::size_t kClassesNavigationMaximumTextLength = 4'096;
inline constexpr std::size_t kClassesNavigationMaximumScheduleRows = 14;
inline constexpr std::size_t kClassesNavigationMaximumScheduleTokenLength = 64;

struct ClassesListEntry final
{
    Domain::ClassId classId;
    std::u16string className;

    friend bool operator==(
        const ClassesListEntry&,
        const ClassesListEntry&
        ) = default;
};

struct ClassesListSnapshot final
{
    std::vector<ClassesListEntry> classes;

    friend bool operator==(
        const ClassesListSnapshot&,
        const ClassesListSnapshot&
        ) = default;
};

} // namespace ClassMngr::Next::Application
