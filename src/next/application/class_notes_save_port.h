#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <cstddef>
#include <string>
#include <system_error>

namespace ClassMngr::Next::Application
{

inline constexpr std::size_t kClassNotesSaveMaxTextCodeUnits = 10'000;

// UTF-16 text keeps the legacy QString code-unit limit explicit without
// introducing a Qt dependency into the Application contract.
struct ClassNotesSaveRequest final
{
    Domain::ClassId classId;
    std::u16string notes;
    std::u16string timeFillerActivities;

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const ClassNotesSaveRequest&,
        const ClassNotesSaveRequest&
        ) = default;
};

using ClassNotesSaveResult = Domain::Result<void>;

[[nodiscard]] inline Domain::Result<void>
validateClassNotesSaveRequest(
    const ClassNotesSaveRequest& request
    )
{
    if (request.notes.size() > kClassNotesSaveMaxTextCodeUnits
        || request.timeFillerActivities.size()
            > kClassNotesSaveMaxTextCodeUnits)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Validation,
            .message = "Class notes text exceeds the 10,000 UTF-16 code-unit limit.",
            .recoverable = false
        });
    }

    const std::string& classId = request.classId.value();
    int parsedClassId = 0;
    const auto parseResult = std::from_chars(
        classId.data(),
        classId.data() + classId.size(),
        parsedClassId
        );
    if (parseResult.ec != std::errc{}
        || parseResult.ptr != classId.data() + classId.size()
        || parsedClassId <= 0)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::InvalidInput,
            .message = "Class identifier must be a positive integer.",
            .recoverable = false
        });
    }

    return Domain::Result<void>::success();
}

inline Domain::Result<void> ClassNotesSaveRequest::validate() const
{
    return validateClassNotesSaveRequest(*this);
}

class ClassNotesSavePort
{
public:
    virtual ~ClassNotesSavePort() = default;

    [[nodiscard]] virtual ClassNotesSaveResult saveClassNotes(
        const ClassNotesSaveRequest& request
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
