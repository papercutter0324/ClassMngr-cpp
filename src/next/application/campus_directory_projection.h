#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

// A directory is deliberately bounded because it is a navigation/filtering
// projection, not a replacement for the campus information graph. An adapter
// or query owner must paginate or stage a larger result before create().
inline constexpr std::size_t kCampusDirectoryMaxEntries = 4'096;
inline constexpr std::size_t kCampusDirectoryMaxIdentifierLength = 256;
inline constexpr std::size_t kCampusDirectoryMaxKeyLength = 64;
inline constexpr std::size_t kCampusDirectoryMaxDisplayNameLength = 256;
inline constexpr std::size_t kCampusDirectoryMaxAddressLength = 512;
inline constexpr std::size_t kCampusDirectoryMaxNotesLength = 2'048;

// Descriptive aliases keep the limits discoverable at both the directory and
// summary call sites without introducing a second set of bounds.
inline constexpr std::size_t kCampusSummaryMaxEntries =
    kCampusDirectoryMaxEntries;
inline constexpr std::size_t kCampusSummaryMaxIdentifierLength =
    kCampusDirectoryMaxIdentifierLength;
inline constexpr std::size_t kCampusSummaryMaxKeyLength =
    kCampusDirectoryMaxKeyLength;
inline constexpr std::size_t kCampusSummaryMaxDisplayNameLength =
    kCampusDirectoryMaxDisplayNameLength;
inline constexpr std::size_t kCampusSummaryMaxAddressLength =
    kCampusDirectoryMaxAddressLength;
inline constexpr std::size_t kCampusSummaryMaxNotesLength =
    kCampusDirectoryMaxNotesLength;

// This is flat navigation metadata only. It intentionally does not retain
// building services, map objects, image bytes, repositories, or other rich
// campus/location records. A missing notes value is distinct from present
// notes, while order and active are explicit directory metadata.
struct CampusSummary final
{
    Domain::CampusId id;
    std::string displayName;
    std::string key;
    std::string address;
    std::optional<std::string> notes;
    std::int32_t order = 0;
    bool active = true;

    // These names make the adapter-neutral key/address terminology usable by
    // callers that refer to the legacy concepts as code/location.
    [[nodiscard]] const std::string& code() const noexcept
    {
        return key;
    }

    [[nodiscard]] const std::string& shortCode() const noexcept
    {
        return key;
    }

    [[nodiscard]] const std::string& location() const noexcept
    {
        return address;
    }

    [[nodiscard]] const std::string& locationText() const noexcept
    {
        return address;
    }

    [[nodiscard]] bool hasNotes() const noexcept
    {
        return notes.has_value();
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return active;
    }

    friend bool operator==(
        const CampusSummary&,
        const CampusSummary&
        ) = default;
};

struct CampusDirectoryProjectionInput final
{
    std::vector<CampusSummary> campuses;

    friend bool operator==(
        const CampusDirectoryProjectionInput&,
        const CampusDirectoryProjectionInput&
        ) = default;
};

using CampusDirectoryInput = CampusDirectoryProjectionInput;
using CampusSummaryInput = CampusDirectoryProjectionInput;

namespace CampusDirectoryProjectionDetail
{

[[nodiscard]] inline bool isBlank(
    const std::string_view value
    ) noexcept
{
    return value.empty()
        || std::all_of(
            value.cbegin(),
            value.cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
}

[[nodiscard]] inline bool isValidIdentifier(
    const std::string_view value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kCampusDirectoryMaxIdentifierLength;
}

[[nodiscard]] inline bool isRequiredText(
    const std::string_view value,
    const std::size_t maxLength
    ) noexcept
{
    return !isBlank(value) && value.size() <= maxLength;
}

[[nodiscard]] inline bool isOptionalNotes(
    const std::optional<std::string>& value
    ) noexcept
{
    return !value.has_value()
        || isRequiredText(*value, kCampusDirectoryMaxNotesLength);
}

[[nodiscard]] inline Domain::OperationError invalidInput(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

template <typename TypedId>
[[nodiscard]] inline bool isValidId(
    const TypedId& id
    ) noexcept
{
    return isValidIdentifier(id.value());
}

template <typename Value>
[[nodiscard]] inline bool contains(
    const std::vector<Value>& values,
    const Value& candidate
    ) noexcept
{
    return std::find(values.cbegin(), values.cend(), candidate) != values.cend();
}

[[nodiscard]] inline Domain::Result<void> validateCampus(
    const CampusSummary& campus,
    const std::vector<Domain::CampusId>& existingIds,
    const std::vector<std::string>& existingKeys
    )
{
    if (!isValidId(campus.id))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Campus identifier must be non-blank and bounded."
                )
            );
    }

    if (contains(existingIds, campus.id))
    {
        return Domain::Result<void>::failure(
            invalidInput("Campus identifiers must be unique.")
            );
    }

    if (!isRequiredText(campus.key, kCampusDirectoryMaxKeyLength))
    {
        return Domain::Result<void>::failure(
            invalidInput("Campus key must be non-blank and bounded.")
            );
    }

    if (contains(existingKeys, campus.key))
    {
        return Domain::Result<void>::failure(
            invalidInput("Campus keys must be unique.")
            );
    }

    if (!isRequiredText(
            campus.displayName,
            kCampusDirectoryMaxDisplayNameLength
            )
        || !isRequiredText(campus.address, kCampusDirectoryMaxAddressLength))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Campus display name and address must be non-blank and bounded."
                )
            );
    }

    if (!isOptionalNotes(campus.notes))
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Campus notes must be absent or non-blank and bounded."
                )
            );
    }

    if (campus.order < 0)
    {
        return Domain::Result<void>::failure(
            invalidInput("Campus order must not be negative.")
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateInput(
    const CampusDirectoryProjectionInput& input
    )
{
    if (input.campuses.size() > kCampusDirectoryMaxEntries)
    {
        return Domain::Result<void>::failure(
            invalidInput(
                "Campus directory collection exceeds its bounded limit."
                )
            );
    }

    std::vector<Domain::CampusId> existingIds;
    existingIds.reserve(input.campuses.size());
    std::vector<std::string> existingKeys;
    existingKeys.reserve(input.campuses.size());

    for (const auto& campus : input.campuses)
    {
        const auto validation = validateCampus(
            campus,
            existingIds,
            existingKeys
            );
        if (!validation)
        {
            return validation;
        }

        existingIds.push_back(campus.id);
        existingKeys.push_back(campus.key);
    }

    return Domain::Result<void>::success();
}

}

// The projection owns only copied compact summaries. The adapter/query owner
// may release rich campus records, location/service graphs, repositories, and
// source buffers after create() succeeds. UI consumers receive value copies
// from lookup methods and must release their copies when their view ends.
class CampusDirectoryProjection final
{
public:
    using Input = CampusDirectoryProjectionInput;
    using Summary = CampusSummary;
    using Campus = CampusSummary;

    CampusDirectoryProjection() = default;

    [[nodiscard]] static Domain::Result<CampusDirectoryProjection> create(
        Input input
        )
    {
        const auto validation = CampusDirectoryProjectionDetail::validateInput(
            input
            );
        if (!validation)
        {
            return Domain::Result<CampusDirectoryProjection>::failure(
                validation.error()
                );
        }

        return Domain::Result<CampusDirectoryProjection>::success(
            CampusDirectoryProjection(std::move(input.campuses))
            );
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Input& input
        )
    {
        return CampusDirectoryProjectionDetail::validateInput(input);
    }

    [[nodiscard]] static Domain::Result<void> validate(
        const Summary& campus
        )
    {
        return CampusDirectoryProjectionDetail::validateCampus(
            campus,
            {},
            {}
            );
    }

    [[nodiscard]] const std::vector<Summary>& campuses() const noexcept
    {
        return m_campuses;
    }

    [[nodiscard]] const std::vector<Summary>& summaries() const noexcept
    {
        return campuses();
    }

    [[nodiscard]] const std::vector<Summary>& entries() const noexcept
    {
        return campuses();
    }

    [[nodiscard]] std::size_t campusCount() const noexcept
    {
        return m_campuses.size();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return campusCount();
    }

    [[nodiscard]] std::optional<Summary> findCampus(
        const Domain::CampusId& id
        ) const
    {
        const auto campus = std::find_if(
            m_campuses.cbegin(),
            m_campuses.cend(),
            [&id](const Summary& candidate)
            {
                return candidate.id == id;
            }
            );
        if (campus == m_campuses.cend())
        {
            return std::nullopt;
        }

        return *campus;
    }

    [[nodiscard]] std::optional<Summary> lookupCampus(
        const Domain::CampusId& id
        ) const
    {
        return findCampus(id);
    }

    [[nodiscard]] std::optional<Summary> findCampus(
        const std::string_view key
        ) const
    {
        const auto campus = std::find_if(
            m_campuses.cbegin(),
            m_campuses.cend(),
            [key](const Summary& candidate)
            {
                return candidate.key == key;
            }
            );
        if (campus == m_campuses.cend())
        {
            return std::nullopt;
        }

        return *campus;
    }

    [[nodiscard]] std::optional<Summary> lookupCampus(
        const std::string_view key
        ) const
    {
        return findCampus(key);
    }

    [[nodiscard]] std::optional<Summary> findById(
        const Domain::CampusId& id
        ) const
    {
        return findCampus(id);
    }

    [[nodiscard]] std::optional<Summary> lookupById(
        const Domain::CampusId& id
        ) const
    {
        return findCampus(id);
    }

    [[nodiscard]] std::optional<Summary> findByKey(
        const std::string_view key
        ) const
    {
        return findCampus(key);
    }

    [[nodiscard]] std::optional<Summary> lookupByKey(
        const std::string_view key
        ) const
    {
        return findCampus(key);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_campuses.empty();
    }

    friend bool operator==(
        const CampusDirectoryProjection&,
        const CampusDirectoryProjection&
        ) = default;

private:
    explicit CampusDirectoryProjection(
        std::vector<Summary> campuses
        )
        : m_campuses(std::move(campuses))
    {
    }

    std::vector<Summary> m_campuses;
};

using CampusDirectorySnapshot = CampusDirectoryProjection;
using CampusProjection = CampusDirectoryProjection;

}
