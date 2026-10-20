#pragma once

#include "next/application/positive_decimal_id_validation.h"
#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

class ScheduleBuilderSourceScheduleText final
{
public:
    ScheduleBuilderSourceScheduleText() = default;

    explicit ScheduleBuilderSourceScheduleText(std::u16string value)
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] const std::u16string& value() const noexcept
    {
        return m_value;
    }

    friend bool operator==(
        const ScheduleBuilderSourceScheduleText&,
        const ScheduleBuilderSourceScheduleText&
        ) = default;

private:
    std::u16string m_value;
};

struct ScheduleBuilderSourceScheduleRow final
{
    ScheduleBuilderSourceScheduleText day;
    ScheduleBuilderSourceScheduleText startTime;
    ScheduleBuilderSourceScheduleText endTime;

    friend bool operator==(
        const ScheduleBuilderSourceScheduleRow&,
        const ScheduleBuilderSourceScheduleRow&
        ) = default;
};

struct ScheduleBuilderSourceClass final
{
    Domain::ClassId classId;
    std::u16string teacherKoreanName;
    std::u16string teacherEnglishName;
    std::u16string teacherPreferredName;
    std::u16string roomNumber;
    std::u16string grade;
    std::u16string level;
    std::u16string classColor;
    std::u16string fontColor;
    std::vector<ScheduleBuilderSourceScheduleRow> regularSchedule;
    std::vector<ScheduleBuilderSourceScheduleRow> intensiveSchedule;

    friend bool operator==(
        const ScheduleBuilderSourceClass&,
        const ScheduleBuilderSourceClass&
        ) = default;
};

struct ScheduleBuilderSourceSnapshot final
{
    std::vector<ScheduleBuilderSourceClass> classes;

    friend bool operator==(
        const ScheduleBuilderSourceSnapshot&,
        const ScheduleBuilderSourceSnapshot&
        ) = default;
};

struct ScheduleBuilderSourceQuery final
{
    friend bool operator==(
        const ScheduleBuilderSourceQuery&,
        const ScheduleBuilderSourceQuery&
        ) = default;
};

using ScheduleBuilderSourceResult =
    Domain::Result<ScheduleBuilderSourceSnapshot>;

class ScheduleBuilderSourceReadPort
{
public:
    virtual ~ScheduleBuilderSourceReadPort() = default;

    [[nodiscard]] virtual ScheduleBuilderSourceResult readScheduleClasses(
        const ScheduleBuilderSourceQuery& query
        ) const = 0;
};

class ScheduleBuilderSourceQueryHandler final
{
public:
    [[nodiscard]] static ScheduleBuilderSourceResult execute(
        const ScheduleBuilderSourceQuery& query,
        const ScheduleBuilderSourceReadPort& port
        )
    {
        ScheduleBuilderSourceResult source = port.readScheduleClasses(query);
        if (!source)
        {
            return ScheduleBuilderSourceResult::failure(source.error());
        }

        std::set<std::string> classIds;
        for (const ScheduleBuilderSourceClass& entry : source.value().classes)
        {
            if (!isCanonicalPositiveDecimalId(entry.classId.value()))
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "A schedule source returned an invalid class ID."
                    );
            }
            if (!classIds.insert(entry.classId.value()).second)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "A schedule source returned duplicate class IDs."
                    );
            }
        }

        return source;
    }

private:
    [[nodiscard]] static ScheduleBuilderSourceResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return ScheduleBuilderSourceResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

};

} // namespace ClassMngr::Next::Application
