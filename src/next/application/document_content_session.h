#pragma once

#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Application
{

// The application layer retains only a compact token, path, or URI supplied
// by an adapter. Document bytes and viewer/platform document objects never
// cross this boundary.
inline constexpr std::size_t kDocumentContentMaxReferenceLength = 4'096;

enum class DocumentContentPhase
{
    Idle,
    Requested,
    Loading,
    Ready,
    Failed,
    Released
};

class DocumentContentReference final
{
public:
    using Text = std::string;

    DocumentContentReference() = default;

    explicit DocumentContentReference(
        Text text
        )
        : m_text(std::move(text))
    {
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_text.empty();
    }

    [[nodiscard]] const Text& value() const noexcept
    {
        return m_text;
    }

    [[nodiscard]] const Text& text() const noexcept
    {
        return m_text;
    }

    friend bool operator==(
        const DocumentContentReference&,
        const DocumentContentReference&
        ) = default;

private:
    Text m_text;
};

// Identifies one accepted document-content request. Adapters must retain the
// token alongside asynchronous events so an event from an earlier request
// cannot advance a later request on the same session owner.
class DocumentContentSessionToken final
{
public:
    friend bool operator==(
        const DocumentContentSessionToken&,
        const DocumentContentSessionToken&
        ) = default;

private:
    friend class DocumentContentSession;

    explicit constexpr DocumentContentSessionToken(
        const std::uint64_t value
        ) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept
    {
        return m_value;
    }

    std::uint64_t m_value = 0;
};

// A copyable application projection of one document-content session. The
// optional reference is metadata only: the platform/viewer adapter owns the
// active document object and all content bytes, and releases them at or before
// the owner invokes DocumentContentSession::release().
class DocumentContentSnapshot final
{
public:
    using Reference = DocumentContentReference;

    DocumentContentSnapshot() = default;

    [[nodiscard]] DocumentContentPhase phase() const noexcept
    {
        return m_phase;
    }

    [[nodiscard]] const std::optional<Reference>& reference() const noexcept
    {
        return m_reference;
    }

    [[nodiscard]] const std::optional<Reference>& contentReference() const
        noexcept
    {
        return m_reference;
    }

    [[nodiscard]] const std::optional<Domain::OperationError>& error() const
        noexcept
    {
        return m_error;
    }

    friend bool operator==(
        const DocumentContentSnapshot&,
        const DocumentContentSnapshot&
        ) = default;

private:
    friend class DocumentContentSession;

    DocumentContentPhase m_phase = DocumentContentPhase::Idle;
    std::optional<Reference> m_reference;
    std::optional<Domain::OperationError> m_error;
};

// Owns one document-content lifecycle without owning document bytes or a
// viewer object. The adapter must serialize its load/close events through
// this owner and release its actual content at or before release().
class DocumentContentSession final
{
public:
    using Reference = DocumentContentReference;
    using SessionToken = DocumentContentSessionToken;

    DocumentContentSession() = default;

    [[nodiscard]] DocumentContentSnapshot snapshot() const
    {
        return m_snapshot;
    }

    [[nodiscard]] Domain::Result<SessionToken> request(
        Reference reference
        )
    {
        if (!isValidReference(reference))
        {
            return Domain::Result<SessionToken>::failure(
                invalidInput(
                    "Document content reference must be non-blank and bounded."
                    )
                );
        }

        if (retainsActiveDocument())
        {
            return Domain::Result<SessionToken>::failure(
                conflict(
                    "Release the active document before requesting a replacement."
                    )
                );
        }

        if (m_generation == std::numeric_limits<std::uint64_t>::max())
        {
            return Domain::Result<SessionToken>::failure(
                conflict(
                    "Document content session generation is exhausted."
                    )
                );
        }

        // A new request is the reset boundary for a failed or released
        // session. It also guarantees that no stale terminal error survives.
        ++m_generation;
        m_snapshot = DocumentContentSnapshot{};
        m_snapshot.m_phase = DocumentContentPhase::Requested;
        m_snapshot.m_reference = std::move(reference);
        return Domain::Result<SessionToken>::success(
            SessionToken(m_generation)
            );
    }

    [[nodiscard]] Domain::Result<SessionToken> request(
        std::string reference
        )
    {
        return request(Reference(std::move(reference)));
    }

    [[nodiscard]] Domain::Result<void> beginLoading(
        const SessionToken token
        )
    {
        if (!isCurrentGeneration(token))
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Document content event belongs to an earlier session."
                    )
                );
        }

        if (m_snapshot.phase() != DocumentContentPhase::Requested)
        {
            return Domain::Result<void>::failure(
                conflict("Document content can only begin loading after request.")
                );
        }

        m_snapshot.m_phase = DocumentContentPhase::Loading;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> markReady(
        const SessionToken token
        )
    {
        if (!isCurrentGeneration(token))
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Document content event belongs to an earlier session."
                    )
                );
        }

        if (m_snapshot.phase() != DocumentContentPhase::Loading)
        {
            return Domain::Result<void>::failure(
                conflict("Document content can only become ready while loading.")
                );
        }

        // The ready event records state only. The adapter retains the actual
        // content object and bytes outside this application contract.
        m_snapshot.m_phase = DocumentContentPhase::Ready;
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> fail(
        const SessionToken token,
        Domain::OperationError error
        )
    {
        if (!isCurrentGeneration(token))
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Document content event belongs to an earlier session."
                    )
                );
        }

        if (m_snapshot.phase() != DocumentContentPhase::Requested
            && m_snapshot.phase() != DocumentContentPhase::Loading)
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Document content can only fail while requested or loading."
                    )
                );
        }

        m_snapshot.m_phase = DocumentContentPhase::Failed;
        m_snapshot.m_error = std::move(error);
        return Domain::Result<void>::success();
    }

    [[nodiscard]] Domain::Result<void> release()
    {
        if (m_snapshot.phase() == DocumentContentPhase::Idle
            || m_snapshot.phase() == DocumentContentPhase::Released)
        {
            return Domain::Result<void>::failure(
                conflict(
                    "Document content can only be released from an active session."
                    )
                );
        }

        // The platform/viewer adapter releases its bytes and document object
        // at or before this boundary. The application projection keeps no
        // reference or error after release.
        m_snapshot = DocumentContentSnapshot{};
        m_snapshot.m_phase = DocumentContentPhase::Released;
        return Domain::Result<void>::success();
    }

    friend bool operator==(
        const DocumentContentSession&,
        const DocumentContentSession&
        ) = default;

private:
    [[nodiscard]] bool isCurrentGeneration(
        const SessionToken token
        ) const noexcept
    {
        return token.value() == m_generation;
    }

    [[nodiscard]] static bool isValidReference(
        const Reference& reference
        ) noexcept
    {
        if (reference.value().empty()
            || reference.value().size() > kDocumentContentMaxReferenceLength)
        {
            return false;
        }

        return !std::all_of(
            reference.value().cbegin(),
            reference.value().cend(),
            [](const char character)
            {
                return std::isspace(
                    static_cast<unsigned char>(character)
                    ) != 0;
            }
            );
    }

    [[nodiscard]] bool retainsActiveDocument() const noexcept
    {
        return m_snapshot.phase() == DocumentContentPhase::Requested
            || m_snapshot.phase() == DocumentContentPhase::Loading
            || m_snapshot.phase() == DocumentContentPhase::Ready;
    }

    [[nodiscard]] static Domain::OperationError invalidInput(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::InvalidInput,
            .message = message,
            .recoverable = false
        };
    }

    [[nodiscard]] static Domain::OperationError conflict(
        const char* message
        )
    {
        return Domain::OperationError{
            .code = Domain::ErrorCode::Conflict,
            .message = message,
            .recoverable = true
        };
    }

    DocumentContentSnapshot m_snapshot;
    std::uint64_t m_generation = 0;
};

}
