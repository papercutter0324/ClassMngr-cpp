#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/personal_details_service.h"
#include "classmngr/engine/sqlite_database.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace
{
using classmngr::engine::ApplicationSettingsService;
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::PersonalDetails;
using classmngr::engine::PersonalDetailsService;
using classmngr::engine::SettingValue;
using classmngr::engine::SignatureMode;
using classmngr::engine::SqliteDatabase;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEnginePersonalDetailsServiceTests: "
              << message
              << '\n';
    return false;
}

bool sameDetails(
    const PersonalDetails& lhs,
    const PersonalDetails& rhs
    )
{
    return lhs.name == rhs.name
        && lhs.campus == rhs.campus
        && lhs.zoomLoginId == rhs.zoomLoginId
        && lhs.zoomPassword == rhs.zoomPassword
        && lhs.zoomNotAvailable == rhs.zoomNotAvailable
        && lhs.signatureImageBase64 == rhs.signatureImageBase64
        && lhs.signatureMode == rhs.signatureMode
        && lhs.typedSignatureText == rhs.typedSignatureText
        && lhs.typedSignatureFont == rhs.typedSignatureFont;
}

bool isTextValue(
    const classmngr::engine::Result<SettingValue>& result,
    std::string_view expected
    )
{
    return result
        && std::get_if<std::string>(&*result) != nullptr
        && *std::get_if<std::string>(&*result) == expected;
}

bool clearSettings(
    SqliteDatabase& database
    )
{
    return database.execute("DELETE FROM app_settings").has_value();
}
} // namespace

int main()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        std::cerr << "ClassMngrEnginePersonalDetailsServiceTests: "
                  << "OpenDatabase failed\n";
        return 1;
    }

    SqliteDatabase& database = **opened;
    ApplicationSettingsService settings(database);
    PersonalDetailsService service(settings);
    bool passed = true;

    const auto defaults = service.load();
    PersonalDetails expectedDefaults;
    expectedDefaults.zoomLoginId = "N/A";
    expectedDefaults.zoomPassword = "N/A";
    passed &= expect(
        defaults && sameDetails(*defaults, expectedDefaults),
        "missing settings did not produce the documented defaults"
        );

    PersonalDetails expected;
    expected.name = "홍길동 🧑‍🏫";
    expected.campus = "서울 캠퍼스 東京";
    expected.zoomLoginId = "teacher-사용자@example.test";
    expected.zoomPassword = "비밀번호-密碼";
    expected.zoomNotAvailable = false;
    expected.signatureImageBase64 =
        "data:image/png;base64,iVBORw0KGgo= opaque-한글";
    expected.signatureMode = SignatureMode::Type;
    expected.typedSignatureText = "서명 이름 ✍";
    expected.typedSignatureFont = 3;

    passed &= expect(
        service.save(expected).has_value(),
        "full personal-details save failed"
        );
    const auto roundTrip = service.load();
    passed &= expect(
        roundTrip && sameDetails(*roundTrip, expected),
        "full UTF-8 personal-details round trip failed"
        );
    passed &= expect(
        isTextValue(
            settings.load("myInfo/zoomNotAvailable"),
            "0"
            )
            && isTextValue(
                settings.load("myInfo/typedSignatureFont"),
                "3"
                )
            && isTextValue(
                settings.load("myInfo/signatureImage"),
                expected.signatureImageBase64
                ),
        "typed settings or opaque signature text were not persisted"
        );

    passed &= expect(
        clearSettings(database),
        "could not clear settings before legacy fallback test"
        );
    passed &= expect(
        settings.save(
            "subPrep/personalZoomEmail",
            SettingValue{std::string("legacy-login")}
            ).has_value()
            && settings.save(
                "subPrep/personalZoomPassword",
                SettingValue{std::string("legacy-password")}
                ).has_value()
            && settings.save(
                "subPrep/personalZoomNotAvailable",
                SettingValue{std::int64_t{0}}
                ).has_value(),
        "legacy personal-details fixture could not be saved"
        );
    const auto legacy = service.load();
    passed &= expect(
        legacy
            && legacy->zoomLoginId == "legacy-login"
            && legacy->zoomPassword == "legacy-password"
            && !legacy->zoomNotAvailable,
        "legacy Zoom settings were not used as fallbacks"
        );
    passed &= expect(
        isTextValue(
            settings.load("myInfo/zoomLoginId"),
            "legacy-login"
            )
            && isTextValue(
                settings.load("myInfo/zoomPassword"),
                "legacy-password"
                )
            && isTextValue(
                settings.load("myInfo/zoomNotAvailable"),
                "0"
                ),
        "legacy Zoom settings were not promoted to primary keys"
        );

    passed &= expect(
        clearSettings(database),
        "could not clear settings before empty-value test"
        );
    passed &= expect(
        settings.save(
            "myInfo/zoomLoginId",
            SettingValue{std::string()}
            ).has_value()
            && settings.save(
                "subPrep/personalZoomEmail",
                SettingValue{std::string("must-not-fallback")}
                ).has_value(),
        "empty-versus-missing fixture could not be saved"
        );
    const auto emptyPrimary = service.load();
    passed &= expect(
        emptyPrimary && emptyPrimary->zoomLoginId.empty(),
        "an explicitly empty primary Zoom id incorrectly used legacy data"
        );
    passed &= expect(
        isTextValue(settings.load("myInfo/zoomLoginId"), ""),
        "the explicitly empty primary Zoom id was not preserved"
        );

    passed &= expect(
        clearSettings(database),
        "could not clear settings before signature-mode test"
        );
    passed &= expect(
        settings.save(
            "myInfo/signatureMode",
            SettingValue{std::int64_t{17}}
            ).has_value(),
        "invalid signature-mode fixture could not be saved"
        );
    const auto invalidMode = service.load();
    passed &= expect(
        invalidMode && invalidMode->signatureMode == SignatureMode::Image,
        "an unsupported signature mode was not normalized to Image"
        );

    passed &= expect(
        service.saveCampus("부산 캠퍼스 🏫").has_value(),
        "saveCampus failed"
        );
    const auto savedCampus = service.load();
    passed &= expect(
        savedCampus && savedCampus->campus == "부산 캠퍼스 🏫",
        "saveCampus did not persist the UTF-8 campus"
        );

    passed &= expect(
        clearSettings(database),
        "could not clear settings before malformed-setting test"
        );
    passed &= expect(
        settings.save(
            "myInfo/typedSignatureFont",
            SettingValue{std::string("not-an-integer")}
            ).has_value(),
        "malformed setting fixture could not be saved"
        );
    const auto malformed = service.load();
    passed &= expect(
        !malformed && malformed.error().code == ErrorCode::Schema,
        "malformed typed-signature font did not return a schema error"
        );

    PersonalDetails transactionalDetails;
    transactionalDetails.name = "transactional name";
    passed &= expect(
        clearSettings(database)
            && database.execute(
                "CREATE TRIGGER reject_personal_details_setting "
                "BEFORE INSERT ON app_settings "
                "WHEN NEW.key = 'myInfo/typedSignatureFont' "
                "BEGIN "
                "SELECT RAISE(ABORT, 'injected personal-details failure'); "
                "END"
                ).has_value(),
        "transactional failure fixture could not be created"
        );
    const auto failedSave = service.save(transactionalDetails);
    passed &= expect(
        !failedSave && failedSave.error().code == ErrorCode::Database,
        "personal-details save did not propagate a database failure"
        );
    passed &= expect(
        std::holds_alternative<std::monostate>(
            *settings.load("myInfo/name")
            ),
        "failed personal-details batch was not rolled back"
        );

    SqliteDatabase closedDatabase;
    ApplicationSettingsService closedSettings(closedDatabase);
    PersonalDetailsService closedService(closedSettings);
    const auto unavailable = closedService.load();
    passed &= expect(
        !unavailable
            && unavailable.error().code == ErrorCode::Database,
        "closed settings database failure was not propagated"
        );

    return passed ? 0 : 1;
}
