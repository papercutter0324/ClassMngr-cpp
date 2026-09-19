#pragma once

#include "next/domain/domain_types.h"

#include <utility>
#include <variant>

namespace ClassMngr::Next::Application
{

enum class SelectionKind
{
    None = 0,
    Teacher = 1,
    Class = 2,
    Campus = 3,
    CalendarEvent = 4
};

using SelectionValue = std::variant<
    std::monostate,
    Domain::TeacherId,
    Domain::ClassId,
    Domain::CampusId,
    Domain::CalendarEventId
    >;

// A copyable value describing the one current selection, or no selection.
// Each selected identifier is owned by the snapshot and remains independent
// of the state owner's later replacements or clears.
class SelectionStateSnapshot final
{
public:
    SelectionStateSnapshot() = default;

    explicit SelectionStateSnapshot(
        SelectionValue value
        )
        : m_value(std::move(value))
    {
    }

    explicit SelectionStateSnapshot(
        Domain::TeacherId id
        )
        : m_value(std::move(id))
    {
    }

    explicit SelectionStateSnapshot(
        Domain::ClassId id
        )
        : m_value(std::move(id))
    {
    }

    explicit SelectionStateSnapshot(
        Domain::CampusId id
        )
        : m_value(std::move(id))
    {
    }

    explicit SelectionStateSnapshot(
        Domain::CalendarEventId id
        )
        : m_value(std::move(id))
    {
    }

    [[nodiscard]] SelectionKind kind() const noexcept
    {
        if (std::holds_alternative<Domain::TeacherId>(m_value))
        {
            return SelectionKind::Teacher;
        }

        if (std::holds_alternative<Domain::ClassId>(m_value))
        {
            return SelectionKind::Class;
        }

        if (std::holds_alternative<Domain::CampusId>(m_value))
        {
            return SelectionKind::Campus;
        }

        if (std::holds_alternative<Domain::CalendarEventId>(m_value))
        {
            return SelectionKind::CalendarEvent;
        }

        return SelectionKind::None;
    }

    [[nodiscard]] bool hasSelection() const noexcept
    {
        return kind() != SelectionKind::None;
    }

    [[nodiscard]] const SelectionValue& value() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] const Domain::TeacherId* teacherId() const noexcept
    {
        return std::get_if<Domain::TeacherId>(&m_value);
    }

    [[nodiscard]] const Domain::ClassId* classId() const noexcept
    {
        return std::get_if<Domain::ClassId>(&m_value);
    }

    [[nodiscard]] const Domain::CampusId* campusId() const noexcept
    {
        return std::get_if<Domain::CampusId>(&m_value);
    }

    [[nodiscard]] const Domain::CalendarEventId* calendarEventId() const noexcept
    {
        return std::get_if<Domain::CalendarEventId>(&m_value);
    }

    friend bool operator==(
        const SelectionStateSnapshot&,
        const SelectionStateSnapshot&
        ) = default;

private:
    SelectionValue m_value;
};

// Owns the application-wide current selection. A replacement is explicit and
// immediately releases the previously owned identifier; clear() restores the
// no-selection value.
class SelectionState final
{
public:
    SelectionState() = default;

    [[nodiscard]] SelectionStateSnapshot snapshot() const
    {
        return m_snapshot;
    }

    void setSelection(
        SelectionValue value
        )
    {
        m_snapshot = SelectionStateSnapshot(std::move(value));
    }

    void setSelection(
        Domain::TeacherId id
        )
    {
        m_snapshot = SelectionStateSnapshot(std::move(id));
    }

    void setSelection(
        Domain::ClassId id
        )
    {
        m_snapshot = SelectionStateSnapshot(std::move(id));
    }

    void setSelection(
        Domain::CampusId id
        )
    {
        m_snapshot = SelectionStateSnapshot(std::move(id));
    }

    void setSelection(
        Domain::CalendarEventId id
        )
    {
        m_snapshot = SelectionStateSnapshot(std::move(id));
    }

    void clear()
    {
        m_snapshot = SelectionStateSnapshot{};
    }

private:
    SelectionStateSnapshot m_snapshot;
};

using CurrentSelectionSnapshot = SelectionStateSnapshot;
using CurrentSelectionState = SelectionState;

}
