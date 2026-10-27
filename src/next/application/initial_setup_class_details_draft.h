#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <string>

namespace ClassMngr::Next::Application
{

// The setup wizard's unpersisted class-details values. This deliberately has
// no ClassId: class creation belongs to the later Class Times step.
struct InitialSetupClassDetailsDraft final
{
    Domain::TeacherId koreanTeacherId;
    bool classColorChosen = false;
    std::u16string classColor;
    std::u16string fontColor;
    std::u16string classGrade;
    std::u16string classLevel;
    std::u16string readingBook;
    std::u16string essayBook;

    friend bool operator==(
        const InitialSetupClassDetailsDraft&,
        const InitialSetupClassDetailsDraft&
        ) = default;
};

using InitialSetupClassDetailsDraftCaptureResult = Domain::Result<void>;

} // namespace ClassMngr::Next::Application
