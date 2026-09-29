#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Application
{

struct ScheduleEditorClassInfoSnapshot final
{
    Domain::ClassId classId;
    std::u16string classGrade;
    std::u16string classLevel;
    std::u16string readingBook;
    std::u16string essayBook;
    std::u16string classColor;
    std::u16string fontColor;
    std::u16string teacherKoreanName;
    std::u16string roomNumber;

    friend bool operator==(
        const ScheduleEditorClassInfoSnapshot&,
        const ScheduleEditorClassInfoSnapshot&
        ) = default;
};

using ScheduleEditorClassInfoReadResult =
    Domain::Result<ScheduleEditorClassInfoSnapshot>;

class ScheduleEditorClassInfoReadPort
{
public:
    virtual ~ScheduleEditorClassInfoReadPort() = default;

    [[nodiscard]] virtual ScheduleEditorClassInfoReadResult
    readScheduleEditorClassInfo(
        const Domain::ClassId& classId
        ) const = 0;
};

class ScheduleEditorClassInfoQuery final
{
public:
    [[nodiscard]] static ScheduleEditorClassInfoReadResult execute(
        const Domain::ClassId& classId,
        const ScheduleEditorClassInfoReadPort& port
        )
    {
        if (!isCanonicalPositiveClassId(classId.value()))
        {
            return ScheduleEditorClassInfoReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message =
                    "Selected class identifier must be a canonical positive integer.",
                .recoverable = true
            });
        }

        auto source = port.readScheduleEditorClassInfo(classId);
        if (!source)
        {
            return ScheduleEditorClassInfoReadResult::failure(source.error());
        }

        auto snapshot = std::move(source.value());
        if (snapshot.classId != classId)
        {
            return ScheduleEditorClassInfoReadResult::failure({
                .code = Domain::ErrorCode::Validation,
                .message =
                    "The schedule editor read returned a different class identifier.",
                .recoverable = false
            });
        }

        return ScheduleEditorClassInfoReadResult::success(
            std::move(snapshot)
            );
    }

private:
    [[nodiscard]] static bool isCanonicalPositiveClassId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return false;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        return error == std::errc{}
            && end == value.data() + value.size()
            && parsed > 0
            && std::to_string(parsed) == value;
    }
};

} // namespace ClassMngr::Next::Application
