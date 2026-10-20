#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

class SelectedClassGradeValue final
{
public:
    SelectedClassGradeValue() = default;

    explicit SelectedClassGradeValue(std::string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const SelectedClassGradeValue&,
        const SelectedClassGradeValue&
        ) = default;

private:
    std::string m_value;
};

struct SelectedClassGradeReadSnapshot final
{
    Domain::ClassId classId;
    SelectedClassGradeValue classGrade;
};

} // namespace ClassMngr::Next::Application
