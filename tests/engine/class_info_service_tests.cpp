#include "classmngr/engine/class_info_config.h"
#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_info_validator.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_time_validator.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/teacher_service.h"

#include <iostream>
#include <string>
#include <string_view>

namespace
{
namespace Engine = classmngr::engine;

using classmngr::engine::ClassInfo;
using classmngr::engine::ClassInfoService;
using classmngr::engine::ClassRepository;
using classmngr::engine::ClassTime;
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::Teacher;
using classmngr::engine::TeacherService;
using classmngr::engine::ValidationResult;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineClassInfoServiceTests: "
              << message
              << '\n';
    return false;
}

bool hasIssue(
    const ValidationResult& validation,
    std::string_view code,
    std::string_view field = {}
    )
{
    for (const auto& issue : validation.issues())
    {
        if (issue.code == code && (field.empty() || issue.field == field))
        {
            return true;
        }
    }
    return false;
}

Teacher validTeacher()
{
    Teacher teacher;
    teacher.teacherKr = "홍길동";
    teacher.teacherEn = "Jane Doe";
    teacher.preferredName = "Jane Doe";
    teacher.roomNumber = "Room 1";
    teacher.birthday = "02-29";
    teacher.phoneNumber = "010-1234-5678";
    teacher.wifiName = "Wifi";
    teacher.wifiPassword = "secret";
    teacher.internetType = "WiFi";
    teacher.zoomId = "zoom-id";
    teacher.zoomPassword = "zoom-secret";
    teacher.projectionType = "HDMI";
    return teacher;
}

ClassInfo validClassInfo(
    int classId,
    int teacherId
    )
{
    ClassInfo info;
    info.classId = classId;
    info.teacherId = teacherId;
    info.classGrade = " e4 ";
    info.classLevel = " theseus ";
    info.readingBook = " reading explorer 2 ";
    info.essayBook = " 4b ";
    info.classColor = " #aabbcc ";
    info.fontColor = " #102030 ";
    info.classTimes = {
        ClassTime{" monday ", "16:00", "16:50"},
        ClassTime{"Tuesday", "4:00 pm", "4:50 PM"}
    };
    info.intensiveTimes = {
        ClassTime{" wednesday ", "09:00", "09:50"}
    };
    info.notes = " Class notes ";
    info.timeFillerActivities = " Vocabulary review ";
    return info;
}
} // namespace

int main()
{
    bool passed = true;

    const classmngr::engine::ClassInfoConfig::StringList levels =
        classmngr::engine::ClassInfoConfig::levelsForGrade("e4");
    passed &= expect(
        levels.empty(),
        "configuration lookup unexpectedly accepted a non-canonical grade"
        );
    passed &= expect(
        classmngr::engine::ClassInfoConfig::readingBooks("M1", "Elephantus").size()
            == 4
            && classmngr::engine::ClassInfoConfig::essayBooks("M1", "Solis")
                   == std::vector<std::string>{"N/A"},
        "grade-specific book fallback rules were not preserved"
        );

    ClassInfo normalizationFixture;
    normalizationFixture.classId = 1;
    normalizationFixture.classGrade = " e4 ";
    normalizationFixture.classLevel = " theseus ";
    normalizationFixture.readingBook = " reading explorer 2 ";
    normalizationFixture.essayBook = " 4b ";
    normalizationFixture.classColor = " #aabbcc ";
    normalizationFixture.fontColor = " #102030 ";
    normalizationFixture.classTimes = {
        ClassTime{" monday ", "16:00", "16:50"}
    };
    const ClassInfo normalized = Engine::ClassInfoValidator::normalized(
        normalizationFixture
        );
    passed &= expect(
        normalized.classGrade == "E4"
            && normalized.classLevel == "Theseus"
            && normalized.readingBook == "Reading Explorer 2"
            && normalized.essayBook == "4B"
            && normalized.classColor == "#AABBCC"
            && normalized.fontColor == "#102030"
            && normalized.classTimes.front().day == "Monday"
            && normalized.classTimes.front().startTime == "4:00 PM"
            && normalized.classTimes.front().endTime == "4:50 PM",
        "class-information normalization did not produce canonical values"
        );

    ClassInfo invalid = normalized;
    invalid.classColor = "#not-a-color";
    invalid.classTimes.push_back(invalid.classTimes.front());
    const ValidationResult invalidValidation = Engine::ClassInfoValidator::validate(invalid);
    passed &= expect(
        invalidValidation.hasErrors()
            && hasIssue(invalidValidation, "color.invalid_hex", "classColor")
            && hasIssue(invalidValidation, "class_time.duplicate_slot"),
        "class-information validation did not report invalid color and duplicate time"
        );

    const ValidationResult timeValidation = Engine::ClassTimeValidator::validate(
        {ClassTime{"Monday", "4:50 PM", "4:00 PM"}},
        "classTimes"
        );
    passed &= expect(
        hasIssue(timeValidation, "schedule.time.end_not_after_start", "classTimes[0].endTime"),
        "class-time ordering validation did not reject a reversed interval"
        );

    const auto opened = OpenDatabase::execute(":memory:");
    passed &= expect(
        opened && *opened != nullptr,
        "OpenDatabase failed for class-information service test"
        );
    if (!opened || *opened == nullptr)
    {
        return 1;
    }

    auto& database = **opened;
    ClassRepository classes(database);
    TeacherService teachers(database);
    ClassInfoService service(database);

    const auto classId = classes.create("Portable class");
    const auto teacherId = teachers.create(validTeacher());
    passed &= expect(
        classId && teacherId,
        "class-information fixture creation failed"
        );
    if (!classId || !teacherId)
    {
        return 1;
    }

    const ClassInfo source = validClassInfo(*classId, *teacherId);
    passed &= expect(
        service.save(source).has_value(),
        "validated class-information save failed"
        );

    const auto loaded = service.load(*classId);
    passed &= expect(
        loaded
            && loaded->classId == *classId
            && loaded->teacherId == *teacherId
            && loaded->teacherKr == "홍길동"
            && loaded->teacherEn == "Jane Doe"
            && loaded->teacherPreferredName == "Jane Doe"
            && loaded->classGrade == "E4"
            && loaded->classLevel == "Theseus"
            && loaded->readingBook == "Reading Explorer 2"
            && loaded->essayBook == "4B"
            && loaded->classColor == "#AABBCC"
            && loaded->fontColor == "#102030"
            && loaded->notes == "Class notes"
            && loaded->timeFillerActivities == "Vocabulary review"
            && loaded->classTimes.size() == 2
            && loaded->classTimes[0].day == "Monday"
            && loaded->classTimes[0].startTime == "4:00 PM"
            && loaded->intensiveTimes.size() == 1
            && loaded->intensiveTimes[0].startTime == "9:00 AM",
        "class-information round trip did not preserve normalized data or teacher join"
        );

    ClassInfo changed = *loaded;
    changed.classTimes = {ClassTime{"Friday", "08:00", "08:50"}};
    changed.intensiveTimes.clear();
    passed &= expect(
        service.save(changed).has_value(),
        "class-information update failed"
        );
    const auto updated = service.load(*classId);
    passed &= expect(
        updated
            && updated->classTimes.size() == 1
            && updated->classTimes.front().day == "Friday"
            && updated->classTimes.front().startTime == "8:00 AM"
            && updated->intensiveTimes.empty(),
        "class-information update did not replace both time collections"
        );

    ClassInfo rejected = changed;
    rejected.classColor = "#invalid";
    rejected.classTimes = {
        ClassTime{"Friday", "08:00", "08:50"},
        ClassTime{"friday", "8:00 AM", "8:50 AM"}
    };
    const auto rejectedSave = service.save(rejected);
    passed &= expect(
        !rejectedSave && rejectedSave.error().code == ErrorCode::InvalidFormat,
        "invalid class information was not rejected with a typed format error"
        );
    const auto afterRejected = service.load(*classId);
    passed &= expect(
        afterRejected
            && afterRejected->classColor == "#AABBCC"
            && afterRejected->classTimes.size() == 1
            && afterRejected->classTimes.front().day == "Friday",
        "rejected class-information save changed persisted data"
        );

    passed &= expect(
        service.saveNotes(*classId, "Updated notes", "Updated filler").has_value(),
        "class note save failed"
        );
    const auto withNotes = service.load(*classId);
    passed &= expect(
        withNotes
            && withNotes->notes == "Updated notes"
            && withNotes->timeFillerActivities == "Updated filler",
        "class note round trip failed"
        );

    const auto longNotes = service.saveNotes(
        *classId,
        std::string(10001, 'x'),
        ""
        );
    passed &= expect(
        !longNotes && longNotes.error().code == ErrorCode::InvalidFormat,
        "overlong class notes were not rejected"
        );

    const auto emptyClassId = classes.create("Class without information");
    passed &= expect(
        emptyClassId.has_value(),
        "empty class fixture creation failed"
        );
    if (emptyClassId)
    {
        const auto emptyInfo = service.load(*emptyClassId);
        passed &= expect(
            emptyInfo
                && emptyInfo->classId == *emptyClassId
                && emptyInfo->teacherId == -1
                && emptyInfo->classColor == "#FFFFFF"
                && emptyInfo->fontColor == "#000000"
                && emptyInfo->classTimes.empty()
                && emptyInfo->intensiveTimes.empty(),
            "class-information load did not provide defaults for an empty class"
            );
    }

    const auto missing = service.load(999999);
    passed &= expect(
        !missing && missing.error().code == ErrorCode::NotFound,
        "missing class-information load did not return not-found"
        );

    return passed ? 0 : 1;
}
