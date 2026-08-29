#include "classmngr/engine/semantic_version.h"

#include <iostream>
#include <string_view>

namespace
{
using classmngr::engine::SemanticVersion;
using classmngr::engine::ErrorCode;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineTests: " << message << '\n';
    return false;
}
}

int main()
{
    bool passed = true;

    const auto parsed = SemanticVersion::parse(" 1.2.3 ");
    passed &= expect(parsed.has_value(), "valid version was rejected");
    if (parsed)
    {
        passed &= expect(parsed->toString() == "1.2.3", "format changed");
        passed &= expect(parsed->majorVersion() == 1, "major component changed");
        passed &= expect(parsed->minorVersion() == 2, "minor component changed");
        passed &= expect(parsed->patchVersion() == 3, "patch component changed");
    }

    const auto malformed = SemanticVersion::parse("1.2");
    passed &= expect(
        malformed.error().code == ErrorCode::InvalidFormat,
        "malformed version did not return a typed format error"
        );

    const auto overflowing = SemanticVersion::parse("2147483648.0.0");
    passed &= expect(
        overflowing.error().code == ErrorCode::NumericOverflow,
        "overflowing version did not return a typed numeric error"
        );

    passed &= expect(
        classmngr::engine::errorCodeName(ErrorCode::Database) == "database",
        "database error code name changed"
        );

    for (const std::string_view invalid : {
             "",
             "1.2",
             "1.2.3.4",
             "v1.2.3",
             "1.a.3",
             "2147483648.0.0"
         })
    {
        passed &= expect(
            !SemanticVersion::parse(invalid),
            "invalid version was accepted"
            );
    }

    const auto lower = SemanticVersion::parse("1.2.9");
    const auto higher = SemanticVersion::parse("1.10.0");
    passed &= expect(lower && higher, "comparison fixtures failed to parse");
    if (lower && higher)
    {
        passed &= expect(*lower < *higher, "version ordering changed");
        passed &= expect(*higher > *lower, "reverse ordering changed");
        passed &= expect(*lower <= *higher, "less/equal ordering changed");
        passed &= expect(*higher >= *lower, "greater/equal ordering changed");
    }

    return passed ? 0 : 1;
}
