#pragma once

#include <compare>
#include <expected>
#include <string>
#include <string_view>

namespace classmngr::engine
{

class SemanticVersion
{
public:
    SemanticVersion() = default;
    SemanticVersion(
        int majorVersion,
        int minorVersion,
        int patchVersion
        );

    [[nodiscard]] static std::expected<SemanticVersion, std::string> parse(
        std::string_view text
        );

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int majorVersion() const;
    [[nodiscard]] int minorVersion() const;
    [[nodiscard]] int patchVersion() const;

    friend bool operator==(
        const SemanticVersion& lhs,
        const SemanticVersion& rhs
        ) = default;

    friend bool operator<(
        const SemanticVersion& lhs,
        const SemanticVersion& rhs
        );

private:
    int m_majorVersion = -1;
    int m_minorVersion = -1;
    int m_patchVersion = -1;
};

bool operator!=(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    );

bool operator>(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    );

bool operator<=(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    );

bool operator>=(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    );

} // namespace classmngr::engine
