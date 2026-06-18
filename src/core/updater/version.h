#pragma once

#include "core/result.h"

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
    int m_majorVersion = -1;
    int m_minorVersion = -1;
    int m_patchVersion = -1;
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
