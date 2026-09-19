#pragma once

#include "next/domain/domain_types.h"
#include "next/domain/operation_result.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

using ImportSourceKey = std::string;

// These limits keep one review projection deliberately compact. The import
// adapter owns larger inputs and must paginate or stage them before creating a
// session; it must also summarize any larger source text.
inline constexpr std::size_t kImportReviewMaxSourceKeyLength = 256;
inline constexpr std::size_t kImportReviewMaxFieldLength = 128;
inline constexpr std::size_t kImportReviewMaxMessageLength = 512;
inline constexpr std::size_t kImportReviewMaxIndexEntries = 4'096;
inline constexpr std::size_t kImportReviewMaxDecisionEntries = 4'096;
inline constexpr std::size_t kImportReviewMaxDiagnosticEntries = 4'096;
inline constexpr std::size_t kImportReviewMaxCandidatesPerIndex = 256;
inline constexpr std::size_t kImportReviewMaxIdentifierLength = 256;

enum class ImportReviewCategory
{
    Teacher,
    Class
};

enum class ImportDecisionAction
{
    Unresolved,
    Create,
    UpdateOrReplace,
    Skip
};

using ImportReviewAction = ImportDecisionAction;

enum class ImportWarningKind
{
    General,
    NormalizedValue,
    DuplicateMatch
};

enum class ImportUnmatchedValueKind
{
    Missing,
    Unknown,
    Ambiguous
};

enum class ImportConflictKind
{
    DuplicateSource,
    AmbiguousMatch,
    ExistingRecord,
    InvalidValue
};

struct TeacherMatchIndex final
{
    ImportSourceKey sourceKey;
    std::vector<Domain::TeacherId> candidates;

    friend bool operator==(
        const TeacherMatchIndex&,
        const TeacherMatchIndex&
        ) = default;
};

struct ClassMatchIndex final
{
    ImportSourceKey sourceKey;
    std::vector<Domain::ClassId> candidates;

    friend bool operator==(
        const ClassMatchIndex&,
        const ClassMatchIndex&
        ) = default;
};

struct ImportWarning final
{
    ImportReviewCategory category = ImportReviewCategory::Teacher;
    ImportWarningKind kind = ImportWarningKind::General;
    ImportSourceKey sourceKey;
    std::string field;
    std::string message;

    friend bool operator==(
        const ImportWarning&,
        const ImportWarning&
        ) = default;
};

struct ImportUnmatchedValue final
{
    ImportReviewCategory category = ImportReviewCategory::Teacher;
    ImportUnmatchedValueKind kind = ImportUnmatchedValueKind::Unknown;
    ImportSourceKey sourceKey;
    std::string field;
    std::string message;

    friend bool operator==(
        const ImportUnmatchedValue&,
        const ImportUnmatchedValue&
        ) = default;
};

struct ImportConflict final
{
    ImportReviewCategory category = ImportReviewCategory::Teacher;
    ImportConflictKind kind = ImportConflictKind::InvalidValue;
    ImportSourceKey sourceKey;
    std::string field;
    std::string message;

    friend bool operator==(
        const ImportConflict&,
        const ImportConflict&
        ) = default;
};

using Warning = ImportWarning;
using UnmatchedValue = ImportUnmatchedValue;
using Conflict = ImportConflict;

class TeacherImportDecision final
{
public:
    static Domain::Result<TeacherImportDecision> create(
        ImportSourceKey sourceKey,
        ImportDecisionAction action,
        std::optional<Domain::TeacherId> targetId = std::nullopt
        );

    [[nodiscard]] const ImportSourceKey& sourceKey() const noexcept
    {
        return m_sourceKey;
    }

    [[nodiscard]] ImportDecisionAction action() const noexcept
    {
        return m_action;
    }

    [[nodiscard]] const std::optional<Domain::TeacherId>& targetId() const
        noexcept
    {
        return m_targetId;
    }

    [[nodiscard]] bool isResolved() const noexcept
    {
        return m_action != ImportDecisionAction::Unresolved;
    }

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const TeacherImportDecision&,
        const TeacherImportDecision&
        ) = default;

private:
    TeacherImportDecision(
        ImportSourceKey sourceKey,
        ImportDecisionAction action,
        std::optional<Domain::TeacherId> targetId
        )
        : m_sourceKey(std::move(sourceKey)),
          m_action(action),
          m_targetId(std::move(targetId))
    {
    }

    ImportSourceKey m_sourceKey;
    ImportDecisionAction m_action = ImportDecisionAction::Unresolved;
    std::optional<Domain::TeacherId> m_targetId;
};

class ClassImportDecision final
{
public:
    static Domain::Result<ClassImportDecision> create(
        ImportSourceKey sourceKey,
        ImportDecisionAction action,
        std::optional<Domain::ClassId> targetId = std::nullopt
        );

    [[nodiscard]] const ImportSourceKey& sourceKey() const noexcept
    {
        return m_sourceKey;
    }

    [[nodiscard]] ImportDecisionAction action() const noexcept
    {
        return m_action;
    }

    [[nodiscard]] const std::optional<Domain::ClassId>& targetId() const
        noexcept
    {
        return m_targetId;
    }

    [[nodiscard]] bool isResolved() const noexcept
    {
        return m_action != ImportDecisionAction::Unresolved;
    }

    [[nodiscard]] Domain::Result<void> validate() const;

    friend bool operator==(
        const ClassImportDecision&,
        const ClassImportDecision&
        ) = default;

private:
    ClassImportDecision(
        ImportSourceKey sourceKey,
        ImportDecisionAction action,
        std::optional<Domain::ClassId> targetId
        )
        : m_sourceKey(std::move(sourceKey)),
          m_action(action),
          m_targetId(std::move(targetId))
    {
    }

    ImportSourceKey m_sourceKey;
    ImportDecisionAction m_action = ImportDecisionAction::Unresolved;
    std::optional<Domain::ClassId> m_targetId;
};

using TeacherImportDecisions = std::vector<TeacherImportDecision>;
using ClassImportDecisions = std::vector<ClassImportDecision>;

// The input is a compact transfer value assembled by the import adapter. It
// must contain only the bounded indexes, decisions, and diagnostics below;
// workbook, decoded-cell, preview-model, and UI ownership stays outside this
// contract.
struct ImportReviewSessionInput final
{
    std::vector<TeacherMatchIndex> teacherMatchIndexes;
    std::vector<ClassMatchIndex> classMatchIndexes;
    TeacherImportDecisions teacherDecisions;
    ClassImportDecisions classDecisions;
    std::vector<ImportWarning> warnings;
    std::vector<ImportUnmatchedValue> unmatchedValues;
    std::vector<ImportConflict> conflicts;

    friend bool operator==(
        const ImportReviewSessionInput&,
        const ImportReviewSessionInput&
        ) = default;
};

class ImportReviewSession final
{
public:
    // The caller owns this operation-scoped snapshot. The parser may release
    // raw file bytes, decoded cells, and workbook buffers before this value
    // becomes authoritative. UI code materializes bounded rows from the
    // snapshot and releases its copy after apply or cancel.
    ImportReviewSession() = default;

    static Domain::Result<ImportReviewSession> create(
        ImportReviewSessionInput input
        );

    [[nodiscard]] const std::vector<TeacherMatchIndex>& teacherMatchIndexes()
        const noexcept
    {
        return m_teacherMatchIndexes;
    }

    [[nodiscard]] const std::vector<ClassMatchIndex>& classMatchIndexes() const
        noexcept
    {
        return m_classMatchIndexes;
    }

    [[nodiscard]] const TeacherImportDecisions& teacherDecisions() const
        noexcept
    {
        return m_teacherDecisions;
    }

    [[nodiscard]] const ClassImportDecisions& classDecisions() const noexcept
    {
        return m_classDecisions;
    }

    [[nodiscard]] const std::vector<ImportWarning>& warnings() const noexcept
    {
        return m_warnings;
    }

    [[nodiscard]] const std::vector<ImportUnmatchedValue>& unmatchedValues()
        const noexcept
    {
        return m_unmatchedValues;
    }

    [[nodiscard]] const std::vector<ImportConflict>& conflicts() const noexcept
    {
        return m_conflicts;
    }

    // These aliases keep the matching collections named naturally at the UI
    // boundary without adding another stored representation.
    [[nodiscard]] const std::vector<TeacherMatchIndex>& teacherMatches() const
        noexcept
    {
        return teacherMatchIndexes();
    }

    [[nodiscard]] const std::vector<ClassMatchIndex>& classMatches() const
        noexcept
    {
        return classMatchIndexes();
    }

    [[nodiscard]] bool hasConflicts() const noexcept
    {
        return !m_conflicts.empty();
    }

    [[nodiscard]] bool hasUnresolvedDecisions() const noexcept
    {
        return std::any_of(
            m_teacherDecisions.cbegin(),
            m_teacherDecisions.cend(),
            [](const TeacherImportDecision& decision)
            {
                return !decision.isResolved();
            }
            )
            || std::any_of(
                m_classDecisions.cbegin(),
                m_classDecisions.cend(),
                [](const ClassImportDecision& decision)
                {
                    return !decision.isResolved();
                }
                );
    }

    [[nodiscard]] bool readyToApply() const noexcept
    {
        return !hasConflicts() && !hasUnresolvedDecisions();
    }

    friend bool operator==(
        const ImportReviewSession&,
        const ImportReviewSession&
        ) = default;

private:
    explicit ImportReviewSession(
        ImportReviewSessionInput input
        )
        : m_teacherMatchIndexes(std::move(input.teacherMatchIndexes)),
          m_classMatchIndexes(std::move(input.classMatchIndexes)),
          m_teacherDecisions(std::move(input.teacherDecisions)),
          m_classDecisions(std::move(input.classDecisions)),
          m_warnings(std::move(input.warnings)),
          m_unmatchedValues(std::move(input.unmatchedValues)),
          m_conflicts(std::move(input.conflicts))
    {
    }

    std::vector<TeacherMatchIndex> m_teacherMatchIndexes;
    std::vector<ClassMatchIndex> m_classMatchIndexes;
    TeacherImportDecisions m_teacherDecisions;
    ClassImportDecisions m_classDecisions;
    std::vector<ImportWarning> m_warnings;
    std::vector<ImportUnmatchedValue> m_unmatchedValues;
    std::vector<ImportConflict> m_conflicts;
};

using ImportReviewSessionSnapshot = ImportReviewSession;

namespace Detail
{

[[nodiscard]] inline Domain::OperationError invalidImportReviewInput(
    const char* message
    )
{
    return Domain::OperationError{
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    };
}

[[nodiscard]] inline bool isBlank(
    const std::string& value
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

[[nodiscard]] inline bool isValidSourceKey(
    const std::string& value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kImportReviewMaxSourceKeyLength;
}

template <typename TypedId>
[[nodiscard]] inline bool isValidTypedIdentifier(
    const TypedId& identifier
    ) noexcept
{
    return !identifier.value().empty()
        && identifier.value().size() <= kImportReviewMaxIdentifierLength;
}

template <typename TypedId>
[[nodiscard]] inline bool areValidCandidates(
    const std::vector<TypedId>& candidates
    ) noexcept
{
    return candidates.size() <= kImportReviewMaxCandidatesPerIndex
        && std::all_of(
            candidates.cbegin(),
            candidates.cend(),
            [](const TypedId& candidate)
            {
                return isValidTypedIdentifier(candidate);
            }
            );
}

[[nodiscard]] inline bool isValidField(
    const std::string& value
    ) noexcept
{
    return value.size() <= kImportReviewMaxFieldLength;
}

[[nodiscard]] inline bool isValidMessage(
    const std::string& value
    ) noexcept
{
    return !isBlank(value)
        && value.size() <= kImportReviewMaxMessageLength;
}

[[nodiscard]] inline bool isValidCategory(
    const ImportReviewCategory category
    ) noexcept
{
    switch (category)
    {
    case ImportReviewCategory::Teacher:
    case ImportReviewCategory::Class:
        return true;
    }

    return false;
}

[[nodiscard]] inline bool isValidWarningKind(
    const ImportWarningKind kind
    ) noexcept
{
    switch (kind)
    {
    case ImportWarningKind::General:
    case ImportWarningKind::NormalizedValue:
    case ImportWarningKind::DuplicateMatch:
        return true;
    }

    return false;
}

[[nodiscard]] inline bool isValidUnmatchedValueKind(
    const ImportUnmatchedValueKind kind
    ) noexcept
{
    switch (kind)
    {
    case ImportUnmatchedValueKind::Missing:
    case ImportUnmatchedValueKind::Unknown:
    case ImportUnmatchedValueKind::Ambiguous:
        return true;
    }

    return false;
}

[[nodiscard]] inline bool isValidConflictKind(
    const ImportConflictKind kind
    ) noexcept
{
    switch (kind)
    {
    case ImportConflictKind::DuplicateSource:
    case ImportConflictKind::AmbiguousMatch:
    case ImportConflictKind::ExistingRecord:
    case ImportConflictKind::InvalidValue:
        return true;
    }

    return false;
}

[[nodiscard]] inline bool isValidDecisionAction(
    const ImportDecisionAction action
    ) noexcept
{
    switch (action)
    {
    case ImportDecisionAction::Unresolved:
    case ImportDecisionAction::Create:
    case ImportDecisionAction::UpdateOrReplace:
    case ImportDecisionAction::Skip:
        return true;
    }

    return false;
}

[[nodiscard]] inline bool hasValidTarget(
    const ImportDecisionAction action,
    const bool hasTarget
    ) noexcept
{
    switch (action)
    {
    case ImportDecisionAction::Unresolved:
    case ImportDecisionAction::Create:
    case ImportDecisionAction::Skip:
        return !hasTarget;
    case ImportDecisionAction::UpdateOrReplace:
        return hasTarget;
    }

    return false;
}

template <typename Decision>
[[nodiscard]] inline Domain::Result<void> validateDecisionText(
    const Decision& decision,
    const bool hasTarget
    )
{
    if (!isValidSourceKey(decision.sourceKey()))
    {
        return Domain::Result<void>::failure(
            invalidImportReviewInput(
                "Import decision source key must be non-empty and bounded."
                )
            );
    }

    if (!isValidDecisionAction(decision.action()))
    {
        return Domain::Result<void>::failure(
            invalidImportReviewInput("Import decision action is invalid.")
            );
    }

    if (!hasValidTarget(decision.action(), hasTarget))
    {
        return Domain::Result<void>::failure(
            invalidImportReviewInput(
                "Import decision target does not match its action."
                )
            );
    }

    if (decision.targetId().has_value()
        && !isValidTypedIdentifier(*decision.targetId()))
    {
        return Domain::Result<void>::failure(
            invalidImportReviewInput(
                "Import decision target identifier must be non-empty and bounded."
                )
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateDiagnosticText(
    const ImportReviewCategory category,
    const std::string& sourceKey,
    const std::string& field,
    const std::string& message
    )
{
    if (!isValidCategory(category)
        || !isValidSourceKey(sourceKey)
        || !isValidField(field)
        || !isValidMessage(message))
    {
        return Domain::Result<void>::failure(
            invalidImportReviewInput(
                "Import review diagnostic text must be bounded and valid."
                )
            );
    }

    return Domain::Result<void>::success();
}

[[nodiscard]] inline Domain::Result<void> validateSessionInput(
    const ImportReviewSessionInput& input
    )
{
    if (input.teacherMatchIndexes.size() > kImportReviewMaxIndexEntries
        || input.classMatchIndexes.size() > kImportReviewMaxIndexEntries
        || input.teacherDecisions.size() > kImportReviewMaxDecisionEntries
        || input.classDecisions.size() > kImportReviewMaxDecisionEntries
        || input.warnings.size() > kImportReviewMaxDiagnosticEntries
        || input.unmatchedValues.size() > kImportReviewMaxDiagnosticEntries
        || input.conflicts.size() > kImportReviewMaxDiagnosticEntries)
    {
        return Domain::Result<void>::failure(
            invalidImportReviewInput(
                "Import review collection exceeds its bounded entry limit."
                )
            );
    }

    for (const auto& index : input.teacherMatchIndexes)
    {
        if (!isValidSourceKey(index.sourceKey)
            || !areValidCandidates(index.candidates))
        {
            return Domain::Result<void>::failure(
                invalidImportReviewInput(
                    "Teacher match key or candidates exceed a bounded limit."
                    )
                );
        }
    }

    for (const auto& index : input.classMatchIndexes)
    {
        if (!isValidSourceKey(index.sourceKey)
            || !areValidCandidates(index.candidates))
        {
            return Domain::Result<void>::failure(
                invalidImportReviewInput(
                    "Class match key or candidates exceed a bounded limit."
                    )
                );
        }
    }

    for (const auto& decision : input.teacherDecisions)
    {
        const auto result = decision.validate();
        if (!result)
        {
            return result;
        }
    }

    for (const auto& decision : input.classDecisions)
    {
        const auto result = decision.validate();
        if (!result)
        {
            return result;
        }
    }

    for (const auto& warning : input.warnings)
    {
        if (!isValidWarningKind(warning.kind))
        {
            return Domain::Result<void>::failure(
                invalidImportReviewInput("Import warning kind is invalid.")
                );
        }

        const auto result = validateDiagnosticText(
            warning.category,
            warning.sourceKey,
            warning.field,
            warning.message
            );
        if (!result)
        {
            return result;
        }
    }

    for (const auto& unmatched : input.unmatchedValues)
    {
        if (!isValidUnmatchedValueKind(unmatched.kind))
        {
            return Domain::Result<void>::failure(
                invalidImportReviewInput(
                    "Import unmatched-value kind is invalid."
                    )
                );
        }

        const auto result = validateDiagnosticText(
            unmatched.category,
            unmatched.sourceKey,
            unmatched.field,
            unmatched.message
            );
        if (!result)
        {
            return result;
        }
    }

    for (const auto& conflict : input.conflicts)
    {
        if (!isValidConflictKind(conflict.kind))
        {
            return Domain::Result<void>::failure(
                invalidImportReviewInput("Import conflict kind is invalid.")
                );
        }

        const auto result = validateDiagnosticText(
            conflict.category,
            conflict.sourceKey,
            conflict.field,
            conflict.message
            );
        if (!result)
        {
            return result;
        }
    }

    return Domain::Result<void>::success();
}

}

inline auto TeacherImportDecision::create(
    ImportSourceKey sourceKey,
    const ImportDecisionAction action,
    std::optional<Domain::TeacherId> targetId
    ) -> Domain::Result<TeacherImportDecision>
{
    TeacherImportDecision decision(
        std::move(sourceKey),
        action,
        std::move(targetId)
        );
    const auto validation = decision.validate();
    if (!validation)
    {
        return Domain::Result<TeacherImportDecision>::failure(
            validation.error()
            );
    }

    return Domain::Result<TeacherImportDecision>::success(std::move(decision));
}

inline Domain::Result<void> TeacherImportDecision::validate() const
{
    return Detail::validateDecisionText(*this, m_targetId.has_value());
}

inline auto ClassImportDecision::create(
    ImportSourceKey sourceKey,
    const ImportDecisionAction action,
    std::optional<Domain::ClassId> targetId
    ) -> Domain::Result<ClassImportDecision>
{
    ClassImportDecision decision(
        std::move(sourceKey),
        action,
        std::move(targetId)
        );
    const auto validation = decision.validate();
    if (!validation)
    {
        return Domain::Result<ClassImportDecision>::failure(
            validation.error()
            );
    }

    return Domain::Result<ClassImportDecision>::success(std::move(decision));
}

inline Domain::Result<void> ClassImportDecision::validate() const
{
    return Detail::validateDecisionText(*this, m_targetId.has_value());
}

inline auto ImportReviewSession::create(
    ImportReviewSessionInput input
    ) -> Domain::Result<ImportReviewSession>
{
    const auto validation = Detail::validateSessionInput(input);
    if (!validation)
    {
        return Domain::Result<ImportReviewSession>::failure(
            validation.error()
            );
    }

    return Domain::Result<ImportReviewSession>::success(
        ImportReviewSession(std::move(input))
        );
}

}
