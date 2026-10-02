#include "next/application/student_korean_name_suffix_suggestion.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using ClassMngr::Next::Application::StudentKoreanNameSuggestionRow;
using ClassMngr::Next::Application::suggestStudentKoreanNameSuffix;

namespace
{

StudentKoreanNameSuggestionRow student(
    std::u16string englishName,
    std::u16string koreanBaseName,
    std::optional<char16_t> koreanNameSuffix = std::nullopt
    )
{
    return {
        .englishName = std::move(englishName),
        .koreanBaseName = std::move(koreanBaseName),
        .koreanNameSuffix = koreanNameSuffix
    };
}

bool invalidRowsColumnsAndIncompleteNamesReturnEmpty()
{
    const std::vector<StudentKoreanNameSuggestionRow> rows{
        student(u"Amy", u"\uAE40\uBBFC\uC9C0")
    };
    return suggestStudentKoreanNameSuffix(rows, -1, true, true).empty()
        && suggestStudentKoreanNameSuffix(rows, 1, true, true).empty()
        && suggestStudentKoreanNameSuffix(rows, 0, false, true).empty()
        && suggestStudentKoreanNameSuffix(rows, 0, true, false).empty()
        && suggestStudentKoreanNameSuffix(
               {student(u"", u"\uAE40\uBBFC\uC9C0")},
               0,
               true,
               true
               ).empty()
        && suggestStudentKoreanNameSuffix(
               {student(u"Amy", {})},
               0,
               true,
               true
               ).empty();
}

bool englishMatchingTrimsButRemainsCaseSensitive()
{
    const std::vector<StudentKoreanNameSuggestionRow> rows{
        student(u"  Amy\u00a0", u"\uAE40\uBBFC\uC9C0"),
        student(u"amy", u"\uAE40\uBBFC\uC9C0", u'A')
    };
    return suggestStudentKoreanNameSuffix(rows, 0, true, true)
        == u"\uAE40\uBBFC\uC9C0(A)";
}

bool normalizedKoreanBasesAndSuffixesUseTheFirstUnusedLetter()
{
    const std::vector<StudentKoreanNameSuggestionRow> rows{
        student(u"Alex", u"\uAE40\uBBFC\uC9C0", u'A'),
        student(u" Alex ", u"\uAE40\uBBFC\uC9C0", u'A'),
        student(u"Alex", u"\uAE40\uBBFC\uC9C0", u'C'),
        student(u"Alex", u"\uAE40\uBBFC\uC9C0(AA)"),
        student(u"Alex", u"\uAE40\uBBFC\uC9C0", u'a'),
        student(u"Other", u"\uAE40\uBBFC\uC9C0", u'B')
    };
    return suggestStudentKoreanNameSuffix(rows, 0, true, true)
        == u"\uAE40\uBBFC\uC9C0(B)";
}

bool onlyUppercaseSingleLetterSuffixesAreMarked()
{
    const std::vector<StudentKoreanNameSuggestionRow> rows{
        student(u"Alex", u"\uAE40\uBBFC\uC9C0"),
        student(u"Alex", u"\uAE40\uBBFC\uC9C0", u'a'),
        student(u"Alex", u"\uAE40\uBBFC\uC9C0", u'[')
    };
    return suggestStudentKoreanNameSuffix(rows, 0, true, true)
        == u"\uAE40\uBBFC\uC9C0(A)";
}

bool malformedKoreanTextKeepsLegacyBaseSemantics()
{
    const std::vector<StudentKoreanNameSuggestionRow> rows{
        student(u"Alex", u"invalid", u'A'),
        student(u"Alex", u"invalid", u'B'),
        student(u"Alex", u"invalid(a)")
    };
    return suggestStudentKoreanNameSuffix(rows, 0, true, true)
        == u"invalid(C)";
}

bool malformedLowercaseSuffixDoesNotChangeTheBase()
{
    const std::vector<StudentKoreanNameSuggestionRow> rows{
        student(u"Alex", u"invalid"),
        student(u"Alex", u"invalid(a)")
    };
    return suggestStudentKoreanNameSuffix(rows, 0, true, true)
        == u"invalid(A)";
}

bool exhaustedAlphabetReturnsEmpty()
{
    std::vector<StudentKoreanNameSuggestionRow> rows{
        student(u"Alex", u"\uAE40\uBBFC\uC9C0")
    };
    for (char16_t suffix = u'A'; suffix <= u'Z'; ++suffix)
    {
        rows.push_back(student(u"Alex", u"\uAE40\uBBFC\uC9C0", suffix));
    }
    return suggestStudentKoreanNameSuffix(rows, 0, true, true).empty();
}

} // namespace

int main()
{
    if (!invalidRowsColumnsAndIncompleteNamesReturnEmpty())
    {
        std::fprintf(stderr, "Invalid rows, columns, or names were not rejected.\n");
        return EXIT_FAILURE;
    }
    if (!englishMatchingTrimsButRemainsCaseSensitive())
    {
        std::fprintf(stderr, "English matching did not preserve trim/case behavior.\n");
        return EXIT_FAILURE;
    }
    if (!normalizedKoreanBasesAndSuffixesUseTheFirstUnusedLetter())
    {
        std::fprintf(stderr, "Korean base matching or suffix ordering changed.\n");
        return EXIT_FAILURE;
    }
    if (!onlyUppercaseSingleLetterSuffixesAreMarked())
    {
        std::fprintf(stderr, "A non-uppercase suffix was counted as assigned.\n");
        return EXIT_FAILURE;
    }
    if (!malformedKoreanTextKeepsLegacyBaseSemantics())
    {
        std::fprintf(stderr, "Malformed Korean text no longer follows legacy base behavior.\n");
        return EXIT_FAILURE;
    }
    if (!malformedLowercaseSuffixDoesNotChangeTheBase())
    {
        std::fprintf(stderr, "Malformed lowercase suffix changed the Korean base.\n");
        return EXIT_FAILURE;
    }
    if (!exhaustedAlphabetReturnsEmpty())
    {
        std::fprintf(stderr, "Exhausted A-Z suffixes did not return an empty result.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
