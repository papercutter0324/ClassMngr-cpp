#pragma once

#include "next/domain/domain_types.h"

#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Application
{

struct SpeakingEvaluationRosterNamePairSnapshot final
{
    std::u16string englishName;
    std::u16string koreanName;
};

struct SpeakingEvaluationRosterNamesReadSnapshot final
{
    explicit SpeakingEvaluationRosterNamesReadSnapshot(
        Domain::ClassId value
        )
        : classId(std::move(value))
    {
    }

    Domain::ClassId classId;
    bool hasEnglishColumn = false;
    bool hasKoreanColumn = false;
    std::vector<SpeakingEvaluationRosterNamePairSnapshot> rows;
};

} // namespace ClassMngr::Next::Application
