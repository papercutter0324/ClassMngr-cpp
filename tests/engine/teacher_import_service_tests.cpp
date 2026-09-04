#include "classmngr/engine/open_database.h"
#include "classmngr/engine/teacher_import_service.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::GsTeamMember;
using classmngr::engine::NativeEnglishTeacher;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SqliteDatabase;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;
using classmngr::engine::Teacher;
using classmngr::engine::TeacherImportPlan;
using classmngr::engine::TeacherImportService;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineTeacherImportServiceTests: "
              << message
              << '\n';
    return false;
}

std::optional<std::string> textAt(
    SqliteDatabase& database,
    std::string_view sql,
    const SqliteParameters& parameters = {}
    )
{
    const auto result = database.query(sql, parameters);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return std::nullopt;
    }

    const auto* text = std::get_if<std::string>(
        &result->rows.front().values.front()
        );
    if (text == nullptr)
    {
        return std::nullopt;
    }
    return *text;
}

std::optional<std::int64_t> integerAt(
    SqliteDatabase& database,
    std::string_view sql,
    const SqliteParameters& parameters = {}
    )
{
    const auto result = database.query(sql, parameters);
    if (!result || result->rows.size() != 1
        || result->rows.front().values.size() != 1)
    {
        return std::nullopt;
    }

    const auto* value = std::get_if<std::int64_t>(
        &result->rows.front().values.front()
        );
    return value == nullptr
        ? std::nullopt
        : std::optional<std::int64_t>(*value);
}

Teacher koreanTeacher(
    std::string teacherKr,
    std::string roomNumber = {},
    std::string birthday = {},
    std::string phoneNumber = {}
    )
{
    Teacher teacher;
    teacher.teacherKr = std::move(teacherKr);
    teacher.teacherEn = "Imported English";
    teacher.preferredRomanization = "Imported Romanization";
    teacher.preferredName = "Imported Preferred";
    teacher.roomNumber = std::move(roomNumber);
    teacher.birthday = std::move(birthday);
    teacher.phoneNumber = std::move(phoneNumber);
    teacher.wifiName = "Imported WiFi";
    teacher.wifiPassword = "Imported WiFi Password";
    teacher.internetType = "WiFi";
    teacher.zoomId = "imported-zoom";
    teacher.zoomPassword = "Imported Zoom Password";
    teacher.projectionType = "HDMI";
    teacher.notes = "Imported notes";
    return teacher;
}

NativeEnglishTeacher nativeTeacher(
    std::string name,
    std::string position = {},
    std::string phoneNumber = {},
    std::string birthday = {},
    std::string nationality = {},
    std::string email = {}
    )
{
    NativeEnglishTeacher teacher;
    teacher.name = std::move(name);
    teacher.position = std::move(position);
    teacher.phoneNumber = std::move(phoneNumber);
    teacher.birthday = std::move(birthday);
    teacher.nationality = std::move(nationality);
    teacher.email = std::move(email);
    return teacher;
}

GsTeamMember gsMember(
    std::string name,
    std::string koreanName,
    std::string position = {},
    std::string phoneNumber = {},
    std::string birthday = {}
    )
{
    GsTeamMember member;
    member.name = std::move(name);
    member.koreanName = std::move(koreanName);
    member.position = std::move(position);
    member.phoneNumber = std::move(phoneNumber);
    member.birthday = std::move(birthday);
    return member;
}

bool importSuccessPreservesManualFields()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return expect(false, "OpenDatabase failed for success test");
    }

    auto& database = **opened;
    bool passed = true;
    passed &= expect(
        database.execute(
            "INSERT INTO teachers "
            "(teacher_kr, teacher_en, room_number, birthday, phone_number, "
            "wifi_name, notes) VALUES (?, ?, ?, ?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::string("홍길동D")},
                SqliteValue{std::string("Manual English")},
                SqliteValue{std::string("Old Room")},
                SqliteValue{std::string("01-02")},
                SqliteValue{std::string("010-old")},
                SqliteValue{std::string("Manual WiFi")},
                SqliteValue{std::string("Manual notes")}
            }
            ).has_value(),
        "could not seed the stored Korean teacher"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO native_english_teachers "
            "(name, position, phone_number, birthday, nationality, email) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::string("Alex")},
                SqliteValue{std::string("NET")},
                SqliteValue{std::string("010-native-manual")},
                SqliteValue{std::string("03-07")},
                SqliteValue{std::string("Canadian")},
                SqliteValue{std::string("alex@example.com")}
            }
            ).has_value(),
        "could not seed the stored Native English Teacher"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO gs_team "
            "(name, korean_name, position, phone_number, birthday) "
            "VALUES (?, ?, ?, ?, ?)",
            SqliteParameters{
                SqliteValue{std::string("Manual GS name")},
                SqliteValue{std::string("김하늘")},
                SqliteValue{std::string("Old position")},
                SqliteValue{std::string("010-gs-manual")},
                SqliteValue{std::string("05-09")}
            }
            ).has_value(),
        "could not seed the stored GS Team member"
        );

    TeacherImportPlan plan;
    plan.templateId = "sectioned-contact-list-v1";
    plan.sourceDate = "2026-07-09";
    plan.koreanTeachers = {
        koreanTeacher(" 홍길동 ", "413", {}, " 010-new "),
        koreanTeacher(" 김새봄D ", " 414 ", " 02-03 ", "010-new-teacher")
    };
    plan.nativeEnglishTeachers = {
        nativeTeacher("  alex ", "Team Leader", {}, " 04-08 "),
        nativeTeacher(
            "  New   Native ",
            "NET",
            "010-new-native",
            "05-06",
            "Australian",
            "new@example.com"
            )
    };
    plan.gsTeamMembers = {
        gsMember("", " 김하늘 ", "New position", {}, " 06-10 "),
        gsMember(" New   GS ", "", "Branch Manager", "010-new-gs", "07-11")
    };

    TeacherImportService service(database);
    const auto imported = service.importTeachers(plan);
    passed &= expect(
        imported
            && imported->koreanTeachers.created == 1
            && imported->koreanTeachers.updated == 1
            && imported->nativeEnglishTeachers.created == 1
            && imported->nativeEnglishTeachers.updated == 1
            && imported->gsTeamMembers.created == 1
            && imported->gsTeamMembers.updated == 1,
        "valid teacher import did not report the expected changes"
        );

    const auto korean = database.query(
        "SELECT teacher_kr, room_number, birthday, phone_number, "
        "teacher_en, wifi_name, notes FROM teachers "
        "WHERE teacher_kr=?",
        SqliteParameters{SqliteValue{std::string("홍길동")}}
        );
    passed &= expect(
        korean && korean->rows.size() == 1
            && korean->rows.front().values.size() == 7
            && std::get_if<std::string>(&korean->rows.front().values[0]) != nullptr
            && std::get_if<std::string>(&korean->rows.front().values[0])
                != nullptr
            && *std::get_if<std::string>(&korean->rows.front().values[0])
                == "홍길동"
            && *std::get_if<std::string>(&korean->rows.front().values[1])
                == "413"
            && *std::get_if<std::string>(&korean->rows.front().values[2])
                == "01-02"
            && *std::get_if<std::string>(&korean->rows.front().values[3])
                == "010-new"
            && *std::get_if<std::string>(&korean->rows.front().values[4])
                == "Manual English"
            && *std::get_if<std::string>(&korean->rows.front().values[5])
                == "Manual WiFi"
            && *std::get_if<std::string>(&korean->rows.front().values[6])
                == "Manual notes",
        "Korean import did not preserve manual fields or match the suffix"
        );

    const auto newKorean = database.query(
        "SELECT teacher_kr, teacher_en, room_number, birthday "
        "FROM teachers WHERE teacher_kr=?",
        SqliteParameters{SqliteValue{std::string("김새봄")}}
        );
    passed &= expect(
        newKorean && newKorean->rows.size() == 1
            && *std::get_if<std::string>(&newKorean->rows.front().values[0])
                == "김새봄"
            && *std::get_if<std::string>(&newKorean->rows.front().values[1])
                == "Imported English"
            && *std::get_if<std::string>(&newKorean->rows.front().values[2])
                == "414"
            && *std::get_if<std::string>(&newKorean->rows.front().values[3])
                == "02-03",
        "new Korean teacher did not store the imported fields"
        );

    const auto native = database.query(
        "SELECT name, position, phone_number, birthday, nationality, email "
        "FROM native_english_teachers WHERE name=?",
        SqliteParameters{SqliteValue{std::string("alex")}}
        );
    passed &= expect(
        native && native->rows.size() == 1
            && *std::get_if<std::string>(&native->rows.front().values[0])
                == "alex"
            && *std::get_if<std::string>(&native->rows.front().values[1])
                == "Team Leader"
            && *std::get_if<std::string>(&native->rows.front().values[2])
                == "010-native-manual"
            && *std::get_if<std::string>(&native->rows.front().values[3])
                == "04-08"
            && *std::get_if<std::string>(&native->rows.front().values[4])
                == "Canadian"
            && *std::get_if<std::string>(&native->rows.front().values[5])
                == "alex@example.com",
        "Native English import did not preserve blank-source fields"
        );

    const auto newNative = database.query(
        "SELECT name, position, phone_number, birthday, nationality, email "
        "FROM native_english_teachers WHERE name=?",
        SqliteParameters{SqliteValue{std::string("New Native")}}
        );
    passed &= expect(
        newNative && newNative->rows.size() == 1
            && *std::get_if<std::string>(&newNative->rows.front().values[1])
                == "NET"
            && *std::get_if<std::string>(&newNative->rows.front().values[5])
                == "new@example.com",
        "new Native English Teacher did not store all fields"
        );

    const auto gs = database.query(
        "SELECT name, korean_name, position, phone_number, birthday "
        "FROM gs_team WHERE korean_name=?",
        SqliteParameters{SqliteValue{std::string("김하늘")}}
        );
    passed &= expect(
        gs && gs->rows.size() == 1
            && *std::get_if<std::string>(&gs->rows.front().values[0])
                == "Manual GS name"
            && *std::get_if<std::string>(&gs->rows.front().values[1])
                == "김하늘"
            && *std::get_if<std::string>(&gs->rows.front().values[2])
                == "New position"
            && *std::get_if<std::string>(&gs->rows.front().values[3])
                == "010-gs-manual"
            && *std::get_if<std::string>(&gs->rows.front().values[4])
                == "06-10",
        "GS import did not preserve blank-source fields"
        );

    const auto newGs = database.query(
        "SELECT name, position, phone_number, birthday FROM gs_team "
        "WHERE name=?",
        SqliteParameters{SqliteValue{std::string("New GS")}}
        );
    passed &= expect(
        newGs && newGs->rows.size() == 1
            && *std::get_if<std::string>(&newGs->rows.front().values[1])
                == "Branch Manager"
            && *std::get_if<std::string>(&newGs->rows.front().values[2])
                == "010-new-gs"
            && *std::get_if<std::string>(&newGs->rows.front().values[3])
                == "07-11",
        "new GS Team member did not store all fields"
        );

    passed &= expect(
        textAt(
            database,
            "SELECT value FROM app_settings WHERE key=?",
            SqliteParameters{
                SqliteValue{
                    std::string(TeacherImportService::LatestSourceDateSetting)
                }
            }
            ) == std::optional<std::string>("2026-07-09"),
        "latest source date was not saved in ISO format"
        );
    return passed;
}

bool duplicateSourceCandidatesAreRejected()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return expect(false, "OpenDatabase failed for duplicate candidate test");
    }

    auto& database = **opened;
    TeacherImportService service(database);
    bool passed = true;

    TeacherImportPlan korean;
    korean.sourceDate = "2026-08-01";
    korean.koreanTeachers.push_back(
        koreanTeacher("\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99")
        );
    korean.koreanTeachers.push_back(
        koreanTeacher(" \xED\x99\x8D \xEA\xB8\xB8 \xEB\x8F\x99 ")
        );
    const auto koreanResult = service.importTeachers(korean);
    passed &= expect(
        !koreanResult
            && koreanResult.error().code == ErrorCode::InvalidFormat,
        "normalized duplicate Korean candidates were not rejected"
        );

    TeacherImportPlan native;
    native.sourceDate = "2026-08-01";
    native.nativeEnglishTeachers.push_back(nativeTeacher("Duplicate"));
    native.nativeEnglishTeachers.push_back(nativeTeacher(" duplicate "));
    const auto nativeResult = service.importTeachers(native);
    passed &= expect(
        !nativeResult
            && nativeResult.error().code == ErrorCode::InvalidFormat
            && nativeResult.error().message.find("duplicate")
                != std::string::npos,
        "normalized duplicate Native English candidates were not rejected"
        );

    TeacherImportPlan gs;
    gs.sourceDate = "2026-08-01";
    gs.gsTeamMembers.push_back(gsMember("New GS", ""));
    gs.gsTeamMembers.push_back(gsMember(" new   gs ", ""));
    const auto gsResult = service.importTeachers(gs);
    passed &= expect(
        !gsResult
            && gsResult.error().code == ErrorCode::InvalidFormat
            && gsResult.error().message.find("new   gs")
                != std::string::npos,
        "normalized duplicate GS candidates were not rejected"
        );

    return passed;
}

bool normalizedNamesMatchStoredRecords()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return expect(false, "OpenDatabase failed for normalized matching test");
    }

    auto& database = **opened;
    bool passed = true;
    passed &= expect(
        database.execute(
            "INSERT INTO native_english_teachers (name, position) "
            "VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::string("  aLeX  ")},
                SqliteValue{std::string("NET")}
            }
            ).has_value(),
        "could not seed the normalized Native English Teacher"
        );
    passed &= expect(
        database.execute(
            "INSERT INTO gs_team (name, position) VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::string("  New   GS  ")},
                SqliteValue{std::string("NET")}
            }
            ).has_value(),
        "could not seed the normalized GS Team member"
        );

    TeacherImportPlan plan;
    plan.sourceDate = "2026-08-01";
    plan.nativeEnglishTeachers.push_back(
        nativeTeacher(" ALEX ", "Team Leader", {}, " 02-09 ")
        );
    plan.gsTeamMembers.push_back(
        gsMember("new gs", "", "Branch Manager", {}, " 03-10 ")
        );

    TeacherImportService service(database);
    const auto imported = service.importTeachers(plan);
    passed &= expect(
        imported
            && imported->nativeEnglishTeachers.updated == 1
            && imported->gsTeamMembers.updated == 1,
        "normalized source names did not match stored records"
        );

    const auto nativeBirthday = textAt(
        database,
        "SELECT birthday FROM native_english_teachers WHERE name=?",
        SqliteParameters{SqliteValue{std::string("ALEX")}}
        );
    passed &= expect(
        nativeBirthday == std::optional<std::string>("02-09"),
        "normalized Native English birthday was not preserved"
        );
    const auto gsBirthday = textAt(
        database,
        "SELECT birthday FROM gs_team WHERE name=?",
        SqliteParameters{SqliteValue{std::string("new gs")}}
        );
    passed &= expect(
        gsBirthday == std::optional<std::string>("03-10"),
        "normalized GS birthday was not preserved"
        );
    return passed;
}

bool duplicateAndAmbiguousImportsRollBack()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return expect(false, "OpenDatabase failed for rollback test");
    }

    auto& database = **opened;
    TeacherImportService service(database);
    bool passed = true;

    TeacherImportPlan invalidDate;
    invalidDate.sourceDate = "not-a-date";
    invalidDate.koreanTeachers.push_back(koreanTeacher("날짜오류"));
    const auto invalidDateResult = service.importTeachers(invalidDate);
    passed &= expect(
        !invalidDateResult
            && invalidDateResult.error().code == ErrorCode::InvalidFormat,
        "invalid source date was not rejected by the engine"
        );

    TeacherImportPlan empty;
    empty.sourceDate = "2026-08-01";
    const auto emptyResult = service.importTeachers(empty);
    passed &= expect(
        !emptyResult
            && emptyResult.error().code == ErrorCode::InvalidFormat,
        "empty teacher import plan was not rejected by the engine"
        );

    TeacherImportPlan duplicate;
    duplicate.sourceDate = "2026-08-01";
    duplicate.koreanTeachers.push_back(koreanTeacher("롤백교사"));
    duplicate.nativeEnglishTeachers.push_back(nativeTeacher("Duplicate"));
    duplicate.nativeEnglishTeachers.push_back(nativeTeacher(" duplicate "));
    const auto duplicateResult = service.importTeachers(duplicate);
    passed &= expect(
        !duplicateResult
            && duplicateResult.error().code == ErrorCode::InvalidFormat,
        "normalized duplicate import was not rejected as invalid format"
        );
    passed &= expect(
        integerAt(
            database,
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr=?",
            SqliteParameters{SqliteValue{std::string("롤백교사")}}
            ) == std::optional<std::int64_t>(0),
        "duplicate import wrote Korean rows before failing"
        );

    passed &= expect(
        database.execute(
            "INSERT INTO native_english_teachers "
            "(name, position) VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::string("Jamie")},
                SqliteValue{std::string("NET")}
            }
            ).has_value()
            && database.execute(
                "INSERT INTO native_english_teachers "
                "(name, position) VALUES (?, ?)",
                SqliteParameters{
                    SqliteValue{std::string(" jamie ")},
                    SqliteValue{std::string("NET")}
                }
                ).has_value(),
        "could not seed ambiguous Native English Teachers"
        );

    TeacherImportPlan ambiguous;
    ambiguous.sourceDate = "2026-08-02";
    ambiguous.koreanTeachers.push_back(koreanTeacher("원자성교사"));
    ambiguous.nativeEnglishTeachers.push_back(
        nativeTeacher("JAMIE", "Team Leader")
        );
    const auto ambiguousResult = service.importTeachers(ambiguous);
    passed &= expect(
        !ambiguousResult
            && ambiguousResult.error().code == ErrorCode::Constraint,
        "ambiguous stored match was not reported as a constraint"
        );
    passed &= expect(
        integerAt(
            database,
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr=?",
            SqliteParameters{SqliteValue{std::string("원자성교사")}}
            ) == std::optional<std::int64_t>(0),
        "ambiguous import did not roll back earlier Korean writes"
        );
    passed &= expect(
        !textAt(
            database,
            "SELECT value FROM app_settings WHERE key=?",
            SqliteParameters{
                SqliteValue{
                    std::string(TeacherImportService::LatestSourceDateSetting)
                }
            }
            ).has_value(),
        "failed import changed the latest source date"
        );
    return passed;
}

bool ambiguousStoredMatchesAreRejectedForEachDirectory()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return expect(false, "OpenDatabase failed for directory ambiguity test");
    }

    auto& database = **opened;
    TeacherImportService service(database);
    bool passed = true;

    passed &= expect(
        database.execute(
            "INSERT INTO teachers (teacher_kr) VALUES (?)",
            SqliteParameters{
                SqliteValue{std::string("\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99")}
            }
            ).has_value()
            && database.execute(
                "INSERT INTO teachers (teacher_kr) VALUES (?)",
                SqliteParameters{
                    SqliteValue{
                        std::string(" \xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99 ")
                    }
                }
                ).has_value(),
        "could not seed ambiguous Korean teachers"
        );
    TeacherImportPlan korean;
    korean.sourceDate = "2026-08-02";
    korean.koreanTeachers.push_back(
        koreanTeacher("\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99")
        );
    const auto koreanResult = service.importTeachers(korean);
    passed &= expect(
        !koreanResult && koreanResult.error().code == ErrorCode::Constraint,
        "ambiguous stored Korean teachers were not rejected"
        );

    passed &= expect(
        database.execute(
            "INSERT INTO gs_team (name, position) VALUES (?, ?)",
            SqliteParameters{
                SqliteValue{std::string("  New   GS  ")},
                SqliteValue{std::string("NET")}
            }
            ).has_value()
            && database.execute(
                "INSERT INTO gs_team (name, position) VALUES (?, ?)",
                SqliteParameters{
                    SqliteValue{std::string("new gs")},
                    SqliteValue{std::string("NET")}
                }
                ).has_value(),
        "could not seed ambiguous GS Team members"
        );
    TeacherImportPlan gs;
    gs.sourceDate = "2026-08-02";
    gs.gsTeamMembers.push_back(gsMember("NEW GS", ""));
    const auto gsResult = service.importTeachers(gs);
    passed &= expect(
        !gsResult && gsResult.error().code == ErrorCode::Constraint,
        "ambiguous stored GS Team members were not rejected"
        );

    return passed;
}

bool olderDateDoesNotReplaceNewerDate()
{
    const auto opened = OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return expect(false, "OpenDatabase failed for date test");
    }

    auto& database = **opened;
    TeacherImportService service(database);
    bool passed = true;

    TeacherImportPlan newer;
    newer.sourceDate = "2026-07-09";
    newer.koreanTeachers.push_back(koreanTeacher("홍길동"));
    passed &= expect(
        service.importTeachers(newer).has_value(),
        "newer import failed in date monotonicity test"
        );

    TeacherImportPlan older;
    older.sourceDate = "2026-01-01";
    older.koreanTeachers.push_back(koreanTeacher("홍길동"));
    older.nativeEnglishTeachers.push_back(nativeTeacher("Imported English"));
    passed &= expect(
        service.importTeachers(older).has_value(),
        "older import failed instead of preserving the newer date"
        );
    passed &= expect(
        textAt(
            database,
            "SELECT value FROM app_settings WHERE key=?",
            SqliteParameters{
                SqliteValue{
                    std::string(TeacherImportService::LatestSourceDateSetting)
                }
            }
            ) == std::optional<std::string>("2026-07-09"),
        "older source date replaced the newer stored date"
        );
    return passed;
}
} // namespace

int main()
{
    const bool passed = importSuccessPreservesManualFields()
        && duplicateSourceCandidatesAreRejected()
        && normalizedNamesMatchStoredRecords()
        && duplicateAndAmbiguousImportsRollBack()
        && ambiguousStoredMatchesAreRejectedForEachDirectory()
        && olderDateDoesNotReplaceNewerDate();
    return passed ? 0 : 1;
}
