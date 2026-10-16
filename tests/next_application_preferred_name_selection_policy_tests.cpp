#include "next/application/preferred_name_selection_policy.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace ClassMngr::Next::Application;

namespace
{

using Choices = std::vector<std::u16string>;
using Index = std::optional<std::size_t>;

bool loadPrefersExactStoredChoice()
{
    const Choices choices{u"Alex Kim", u"Gim Allekseu"};
    return selectPreferredNameChoiceIndex(
        choices,
        u"Gim Allekseu",
        u"Alex Kim",
        PreferredNameSelectionContext::Load
        ) == Index{1};
}

bool loadFallsBackToFirstChoiceWhenStoredChoiceIsMissing()
{
    const Choices choices{u"Alex Kim", u"Gim Allekseu"};
    return selectPreferredNameChoiceIndex(
        choices,
        u"Unknown",
        u"Gim Allekseu",
        PreferredNameSelectionContext::Load
        ) == Index{0};
}

bool editPrefersExactCurrentSelectionOverStoredChoice()
{
    const Choices choices{u"Alex Kim", u"Gim Allekseu"};
    return selectPreferredNameChoiceIndex(
        choices,
        u"Gim Allekseu",
        u"Alex Kim",
        PreferredNameSelectionContext::Edit
        ) == Index{0};
}

bool editFallsBackToStoredChoiceWhenCurrentSelectionIsMissing()
{
    const Choices choices{u"Alex Kim", u"Gim Allekseu"};
    return selectPreferredNameChoiceIndex(
        choices,
        u"Gim Allekseu",
        u"Unknown",
        PreferredNameSelectionContext::Edit
        ) == Index{1};
}

bool editFallsBackToFirstChoiceWhenBothPreferencesAreMissing()
{
    const Choices choices{u"Alex Kim", u"Gim Allekseu"};
    return selectPreferredNameChoiceIndex(
        choices,
        u"Unknown stored name",
        u"Unknown current name",
        PreferredNameSelectionContext::Edit
        ) == Index{0};
}

bool matchingIsExactAndCaseSensitive()
{
    const Choices choices{u"alex kim", u"Alex Kim"};
    return selectPreferredNameChoiceIndex(
        choices,
        u"Alex Kim",
        u"ALEX KIM",
        PreferredNameSelectionContext::Edit
        ) == Index{1};
}

bool emptyChoicesHaveNoSelection()
{
    const Choices choices;
    return !selectPreferredNameChoiceIndex(
        choices,
        u"Alex Kim",
        u"Alex Kim",
        PreferredNameSelectionContext::Load
        ).has_value()
        && !selectPreferredNameChoiceIndex(
            choices,
            u"Alex Kim",
            u"Alex Kim",
            PreferredNameSelectionContext::Edit
            ).has_value();
}

} // namespace

int main()
{
    if (!loadPrefersExactStoredChoice())
    {
        std::cerr << "Load should prefer the exact stored choice.\n";
        return EXIT_FAILURE;
    }
    if (!loadFallsBackToFirstChoiceWhenStoredChoiceIsMissing())
    {
        std::cerr << "Load should fall back to the first choice.\n";
        return EXIT_FAILURE;
    }
    if (!editPrefersExactCurrentSelectionOverStoredChoice())
    {
        std::cerr << "Edit should prefer the exact current selection.\n";
        return EXIT_FAILURE;
    }
    if (!editFallsBackToStoredChoiceWhenCurrentSelectionIsMissing())
    {
        std::cerr << "Edit should use the stored choice as its fallback.\n";
        return EXIT_FAILURE;
    }
    if (!editFallsBackToFirstChoiceWhenBothPreferencesAreMissing())
    {
        std::cerr << "Edit should use the first choice as its final fallback.\n";
        return EXIT_FAILURE;
    }
    if (!matchingIsExactAndCaseSensitive())
    {
        std::cerr << "Preferred-name matching should be case-sensitive.\n";
        return EXIT_FAILURE;
    }
    if (!emptyChoicesHaveNoSelection())
    {
        std::cerr << "Empty choice lists should have no selection.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
