#include "classmngr/engine/campus_record_service.h"
#include "classmngr/engine/open_database.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using classmngr::engine::CampusRecord;
using classmngr::engine::CampusRecordService;
using classmngr::engine::ErrorCode;
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

    std::cerr << "ClassMngrEngineCampusRecordServiceTests: "
              << message
              << '\n';
    return false;
}

CampusRecord campusWithFields(
    std::string name
    )
{
    CampusRecord campus;
    campus.name = std::move(name);
    campus.buildingName = "본관 🏫";
    campus.address = "서울특별시 강남구";
    campus.phoneNumber = "+82-2-1234-5678";
    campus.officeNumber = "사무실 101호";
    campus.transitSteps = "2호선에서 하차 🚇";
    campus.arrivalInfo = "도착하면 안내 데스크로 오세요";
    campus.imagePath = "assets/캠퍼스.png";
    campus.officeWifi = "TeacherNet-안내";
    campus.officeWifiPassword = "비밀번호-🍜";
    campus.printerName = "Printer-プリンター";
    campus.printerSteps = "1. 용지 넣기\n2. 인쇄하기";
    campus.photocopierCode = "복사기-42";
    campus.housingLocations = "강남, 서초, 송파";
    return campus;
}

bool sameCampus(
    const CampusRecord& lhs,
    const CampusRecord& rhs
    )
{
    return lhs.id == rhs.id
        && lhs.name == rhs.name
        && lhs.buildingName == rhs.buildingName
        && lhs.address == rhs.address
        && lhs.phoneNumber == rhs.phoneNumber
        && lhs.officeNumber == rhs.officeNumber
        && lhs.transitSteps == rhs.transitSteps
        && lhs.arrivalInfo == rhs.arrivalInfo
        && lhs.imagePath == rhs.imagePath
        && lhs.officeWifi == rhs.officeWifi
        && lhs.officeWifiPassword == rhs.officeWifiPassword
        && lhs.printerName == rhs.printerName
        && lhs.printerSteps == rhs.printerSteps
        && lhs.photocopierCode == rhs.photocopierCode
        && lhs.housingLocations == rhs.housingLocations;
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEngineCampusRecordServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    auto& database = **opened;
    CampusRecordService service(database);
    bool passed = true;

    CampusRecord zulu = campusWithFields("Zulu 캠퍼스");
    const auto created = service.create(zulu);
    passed &= expect(
        created && *created > 0,
        "campus creation did not return a valid id"
        );

    CampusRecord alpha = campusWithFields("Alpha Campus");
    const auto savedNew = service.save(alpha);
    passed &= expect(
        savedNew && *savedNew > 0 && (!created || *savedNew != *created),
        "saving a new campus did not insert a separate record"
        );

    if (created)
    {
        zulu.id = *created;
        const auto loaded = service.get(*created);
        passed &= expect(
            loaded && sameCampus(*loaded, zulu),
            "campus get did not preserve every UTF-8 field"
            );
    }

    if (savedNew)
    {
        alpha.id = *savedNew;
    }

    const auto listed = service.list();
    passed &= expect(
        listed
            && listed->size() == 2
            && listed->at(0).name == "Alpha Campus"
            && listed->at(1).name == "Zulu 캠퍼스",
        "campus list did not preserve name ordering"
        );

    if (created)
    {
        CampusRecord updated = campusWithFields("Updated 캠퍼스");
        updated.id = *created;
        passed &= expect(
            service.update(updated).has_value(),
            "campus update failed"
            );
        const auto reloaded = service.get(*created);
        passed &= expect(
            reloaded && sameCampus(*reloaded, updated),
            "campus update did not map every field"
            );
    }

    if (savedNew)
    {
        CampusRecord savedUpdate = campusWithFields("Saved update");
        savedUpdate.id = *savedNew;
        const auto saved = service.save(savedUpdate);
        const auto reloaded = service.get(*savedNew);
        passed &= expect(
            saved && *saved == *savedNew
                && reloaded && sameCampus(*reloaded, savedUpdate),
            "campus save did not use the update path"
            );
    }

    const auto invalidGet = service.get(0);
    passed &= expect(
        !invalidGet && invalidGet.error().code == ErrorCode::InvalidArgument,
        "invalid campus get id was not rejected"
    );
    CampusRecord invalidUpdate;
    const auto invalidUpdateResult = service.update(invalidUpdate);
    passed &= expect(
        !invalidUpdateResult
            && invalidUpdateResult.error().code == ErrorCode::InvalidArgument,
        "invalid campus update id was not rejected"
        );
    passed &= expect(
        !service.remove(0)
            && service.remove(0).error().code == ErrorCode::InvalidArgument,
        "invalid campus delete id was not rejected"
        );

    constexpr int MissingCampusId = 99999;
    const auto missing = service.get(MissingCampusId);
    passed &= expect(
        !missing && missing.error().code == ErrorCode::NotFound,
        "missing campus get did not return not-found"
        );
    CampusRecord missingUpdate = campusWithFields("Missing campus");
    missingUpdate.id = MissingCampusId;
    const auto missingUpdated = service.update(missingUpdate);
    passed &= expect(
        !missingUpdated
            && missingUpdated.error().code == ErrorCode::NotFound,
        "missing campus update did not return not-found"
        );
    const auto missingSaved = service.save(missingUpdate);
    passed &= expect(
        !missingSaved && missingSaved.error().code == ErrorCode::NotFound,
        "missing campus save update did not return not-found"
        );

    if (created)
    {
        passed &= expect(
            service.remove(*created).has_value()
                && !service.get(*created)
                && service.get(*created).error().code == ErrorCode::NotFound,
            "campus delete did not remove the record"
            );
    }
    passed &= expect(
        service.remove(MissingCampusId).has_value(),
        "deleting a missing campus changed the old successful semantics"
        );

    passed &= expect(
        database.execute("DROP TABLE campuses").has_value()
            && database.execute(
                "CREATE TABLE campuses ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "name INTEGER, building_name TEXT, address TEXT, "
                "phone_number TEXT, office_number TEXT, transit_steps TEXT, "
                "arrival_info TEXT, image_path TEXT, office_wifi TEXT, "
                "office_wifi_password TEXT, printer_name TEXT, "
                "printer_steps TEXT, photocopier_code TEXT, "
                "housing_locations TEXT)"
                ).has_value()
            && database.execute(
                "INSERT INTO campuses (id, name) VALUES (1, 42)"
                ).has_value(),
        "malformed campus schema fixture could not be created"
        );
    const auto schemaFailure = service.get(1);
    passed &= expect(
        !schemaFailure && schemaFailure.error().code == ErrorCode::Schema,
        "malformed campus text type did not return a typed schema error"
        );

    return passed ? 0 : 1;
}
