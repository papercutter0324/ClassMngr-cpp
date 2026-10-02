#include "next/application/speaking_evaluation_private_notes_split.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using ClassMngr::Next::Application::SpeakingEvaluationPrivateNotes;
using ClassMngr::Next::Application::
    splitSpeakingEvaluationPrivateNotes;

namespace
{

bool splitsFormattedAndEmptySections()
{
    const auto populated = splitSpeakingEvaluationPrivateNotes(
        u"[Did Well]\nClear pronunciation\n"
        u"[Needs Improvement]\nAdd supporting details"
        );
    const auto empty = splitSpeakingEvaluationPrivateNotes(
        u"[Did Well]\n\n[Needs Improvement]\n"
        );

    return populated
            == SpeakingEvaluationPrivateNotes{
                u"Clear pronunciation",
                u"Add supporting details"
            }
        && empty == SpeakingEvaluationPrivateNotes{};
}

bool keepsLegacyAndMissingSeparatorInputInDidWell()
{
    const std::u16string legacy =
        u"Legacy note\n[Needs Improvement]\nStill legacy";
    const std::u16string missingSeparator =
        u"[Did Well]\nOnly the first section marker";

    return splitSpeakingEvaluationPrivateNotes(legacy)
            == SpeakingEvaluationPrivateNotes{ legacy, {} }
        && splitSpeakingEvaluationPrivateNotes(missingSeparator)
            == SpeakingEvaluationPrivateNotes{
                missingSeparator,
                {}
            };
}

bool splitsAtFirstSeparatorAndKeepsLaterSeparators()
{
    const auto sections = splitSpeakingEvaluationPrivateNotes(
        u"[Did Well]\nPositive note\n"
        u"[Needs Improvement]\nFirst concern\n"
        u"[Needs Improvement]\nSecond concern"
        );

    return sections
        == SpeakingEvaluationPrivateNotes{
            u"Positive note",
            u"First concern\n[Needs Improvement]\nSecond concern"
        };
}

bool preservesExactWhitespaceNewlinesAndUtf16Units()
{
    const std::u16string didWell =
        u"\uFEFF  \tstrength \U0001F9ED\r\n";
    const std::u16string needsImprovement =
        u"\u3000concern\u00A0\n\n\U0001F4DA\t";
    const std::u16string notes =
        u"[Did Well]\n"
        + didWell
        + u"\n[Needs Improvement]\n"
        + needsImprovement;

    return splitSpeakingEvaluationPrivateNotes(notes)
        == SpeakingEvaluationPrivateNotes{
            didWell,
            needsImprovement
        };
}

bool rejectsTextWithoutTheOpeningMarker()
{
    const std::u16string legacy =
        u"\uFEFF[Did Well]\nText\n[Needs Improvement]\nMore";
    return splitSpeakingEvaluationPrivateNotes(legacy)
        == SpeakingEvaluationPrivateNotes{ legacy, {} };
}

} // namespace

int main()
{
    if (!splitsFormattedAndEmptySections())
    {
        std::fprintf(stderr, "Formatted private notes did not split.\n");
        return EXIT_FAILURE;
    }
    if (!keepsLegacyAndMissingSeparatorInputInDidWell())
    {
        std::fprintf(stderr, "Unstructured private notes changed.\n");
        return EXIT_FAILURE;
    }
    if (!splitsAtFirstSeparatorAndKeepsLaterSeparators())
    {
        std::fprintf(stderr, "Repeated separators were not preserved.\n");
        return EXIT_FAILURE;
    }
    if (!preservesExactWhitespaceNewlinesAndUtf16Units())
    {
        std::fprintf(
            stderr,
            "Private-notes split changed exact UTF-16 section content.\n"
            );
        return EXIT_FAILURE;
    }
    if (!rejectsTextWithoutTheOpeningMarker())
    {
        std::fprintf(stderr, "Opening-marker fallback changed.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
