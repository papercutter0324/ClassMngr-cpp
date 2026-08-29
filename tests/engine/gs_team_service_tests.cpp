#include "classmngr/engine/gs_team_service.h"
#include "classmngr/engine/open_database.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::GsTeamMember;
using classmngr::engine::GsTeamService;
using classmngr::engine::OpenDatabase;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineGsTeamServiceTests: "
              << message
              << '\n';
    return false;
}

GsTeamMember member(
    int id,
    std::string name,
    std::string koreanName,
    std::string position
    )
{
    GsTeamMember result;
    result.id = id;
    result.name = std::move(name);
    result.koreanName = std::move(koreanName);
    result.position = std::move(position);
    result.phoneNumber = " 010-3333-4444 ";
    result.birthday = " 03-07 ";
    return result;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineGsTeamServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    GsTeamService service(database);
    bool passed = true;

    const std::vector<GsTeamMember> initial{
        member(-1, " Jane   Doe ", " 제인 도 ", "Branch Manager"),
        member(-1, "Alex", "알렉스", "M3"),
        member(-1, "", " 김하늘 ", "C1")
    };
    passed &= expect(
        service.saveDirectory(initial, {}).has_value(),
        "initial GS-team directory save failed"
        );

    const auto listed = service.list();
    passed &= expect(
        listed && listed->size() == 3
            && listed->at(0).name == "Jane Doe"
            && listed->at(0).koreanName == "제인 도"
            && listed->at(0).position == "Branch Manager"
            && listed->at(1).name == "Alex"
            && listed->at(1).position == "M3"
            && listed->at(2).name.empty()
            && listed->at(2).koreanName == "김하늘",
        "GS-team listing did not preserve normalization and position order"
        );

    if (listed && listed->size() == 3)
    {
        GsTeamMember updated = listed->at(0);
        updated.name = " Jane   Cooper ";
        updated.position = "M1";
        GsTeamMember added = member(-1, "Charlie", "", "Unknown");
        passed &= expect(
            service.saveDirectory(
                {updated, listed->at(2), added},
                {listed->at(1).id, 0, -2}
                ).has_value(),
            "GS-team update/delete/add transaction failed"
            );

        const auto afterUpdate = service.list();
        passed &= expect(
            afterUpdate && afterUpdate->size() == 3
                && afterUpdate->at(0).id == updated.id
                && afterUpdate->at(0).name == "Jane Cooper"
                && afterUpdate->at(0).position == "M1"
                && afterUpdate->at(1).name.empty()
                && afterUpdate->at(1).koreanName == "김하늘"
                && afterUpdate->at(2).name == "Charlie",
            "GS-team save did not produce the expected ordered result"
            );
    }

    const std::vector<GsTeamMember> duplicateEnglish{
        member(-1, " Alex ", "가", "C1"),
        member(-1, "alex", "나", "C1")
    };
    const auto beforeRejected = service.list();
    const auto rejectedEnglish = service.saveDirectory(duplicateEnglish, {});
    passed &= expect(
        !rejectedEnglish
            && rejectedEnglish.error().code == ErrorCode::InvalidFormat,
        "duplicate GS-team English names were not rejected"
        );

    const std::vector<GsTeamMember> duplicateKorean{
        member(-1, "One", "같음", "C1"),
        member(-1, "Two", " 같음 ", "C1")
    };
    const auto rejectedKorean = service.saveDirectory(duplicateKorean, {});
    passed &= expect(
        !rejectedKorean
            && rejectedKorean.error().code == ErrorCode::InvalidFormat,
        "duplicate GS-team Korean names were not rejected"
        );

    const GsTeamMember empty = member(-1, " ", "\t", "C1");
    const auto rejectedEmpty = service.saveDirectory({empty}, {});
    passed &= expect(
        !rejectedEmpty && rejectedEmpty.error().code == ErrorCode::InvalidFormat,
        "empty GS-team names were not rejected"
        );
    const auto afterRejected = service.list();
    passed &= expect(
        beforeRejected && afterRejected
            && beforeRejected->size() == afterRejected->size(),
        "rejected GS-team save was not atomic"
        );

    return passed ? 0 : 1;
}
