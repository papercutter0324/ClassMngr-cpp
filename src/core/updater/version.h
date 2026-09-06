#pragma once

#include "core/result.h"

#include "classmngr/engine/semantic_version.h"

#include <QString>

class Version
{
public:
    Version() = default;
    Version(
        int majorVersion,
        int minorVersion,
        int patchVersion
        );

    [[nodiscard]] static Result<Version> parse(
        const QString& text
        );

    [[nodiscard]] QString toString() const;
    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int majorVersion() const;
    [[nodiscard]] int minorVersion() const;
    [[nodiscard]] int patchVersion() const;

    friend bool operator==(
        const Version& lhs,
        const Version& rhs
        ) = default;

    friend bool operator<(
        const Version& lhs,
        const Version& rhs
        );

private:
    classmngr::engine::SemanticVersion m_value;
};

bool operator!=(
    const Version& lhs,
    const Version& rhs
    );

bool operator>(
    const Version& lhs,
    const Version& rhs
    );

bool operator<=(
    const Version& lhs,
    const Version& rhs
    );

bool operator>=(
    const Version& lhs,
    const Version& rhs
    );
