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

bool teacherDisplayLessThan(
    const Teacher& left,
    const Teacher& right
    )
{
    const std::string leftEnglish = trimAsciiWhitespace(left.teacherEn);
    const std::string rightEnglish = trimAsciiWhitespace(right.teacherEn);
    const bool leftHasEnglish = !leftEnglish.empty();
    const bool rightHasEnglish = !rightEnglish.empty();

    if (leftHasEnglish != rightHasEnglish)
    {
        return leftHasEnglish;
    }

    const auto lowerAscii = [](std::string_view value)
    {
        std::string result(value);
        for (char& character : result)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }
        return result;
    };

    const std::string leftEnglishFolded = lowerAscii(leftEnglish);
    const std::string rightEnglishFolded = lowerAscii(rightEnglish);
    if (leftEnglishFolded != rightEnglishFolded)
    {
        return leftEnglishFolded < rightEnglishFolded;
    }
    if (leftEnglish != rightEnglish)
    {
        return leftEnglish < rightEnglish;
    }

    const std::string leftKorean = trimAsciiWhitespace(left.teacherKr);
    const std::string rightKorean = trimAsciiWhitespace(right.teacherKr);
    if (leftKorean != rightKorean)
    {
        return leftKorean < rightKorean;
    }
    return left.id < right.id;
}

} // namespace classmngr::engine
