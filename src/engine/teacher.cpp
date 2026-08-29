#include "classmngr/engine/teacher.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace classmngr::engine
{
namespace
{
std::string trimAsciiWhitespace(
    std::string_view value
    )
{
    std::size_t first = 0;
    while (first < value.size()
           && std::isspace(
               static_cast<unsigned char>(value[first])) != 0)
    {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first
           && std::isspace(
               static_cast<unsigned char>(value[last - 1])) != 0)
    {
        --last;
    }

    return std::string(value.substr(first, last - first));
}
} // namespace

std::vector<std::string> Teacher::preferredNameChoices() const
{
    std::vector<std::string> choices;
    for (const std::string_view choice : {
             std::string_view(teacherEn),
             std::string_view(preferredRomanization)
         })
    {
        const std::string trimmed = trimAsciiWhitespace(choice);
        if (!trimmed.empty()
            && std::find(choices.begin(), choices.end(), trimmed)
                == choices.end())
        {
            choices.push_back(trimmed);
        }
    }

    return choices;
}

std::string Teacher::preferredDisplayName() const
{
    const std::string selected = trimAsciiWhitespace(preferredName);
    if (!selected.empty())
    {
        return selected;
    }

    const std::vector<std::string> choices = preferredNameChoices();
    if (!choices.empty())
    {
        return choices.front();
    }

    return trimAsciiWhitespace(teacherKr);
}

} // namespace classmngr::engine
