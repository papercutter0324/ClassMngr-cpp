#include "data/data_service.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"

namespace ScheduleWidgetTestStubs
{
QString selectedClassGrade(int classId);
QString selectedClassLevel(int classId);
bool selectedClassGradeReadWillFail();
extern int selectedClassGradeReadCount;
}

Result<ClassDetailsPageReadRecord>
ClassInfoRepository::loadClassDetailsPageRecord(const int classId)
{
    ClassDetailsPageReadRecord record;
    record.classId = classId;
    record.teacherId = classId == 43 ? 8 : 7;
    record.classGrade = ScheduleWidgetTestStubs::selectedClassGrade(classId);
    record.classLevel = ScheduleWidgetTestStubs::selectedClassLevel(classId);
    return record;
}

Result<ClassDetailsPageReadOutcome>
ClassInfoRepository::loadClassDetailsPageRecordDetailed(const int classId)
{
    const Result<ClassDetailsPageReadRecord> loaded =
        loadClassDetailsPageRecord(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }

    return ClassDetailsPageReadOutcome{
        .record = *loaded,
        .parentMissing = false
    };
}

TeacherDisplayNameLookupResult
TeacherRepository::lookupTeacherDisplayName(const qint64 teacherId)
{
    if (teacherId != 7 && teacherId != 8)
    {
        return {
            .code = teacherId <= 0
                ? TeacherDisplayNameLookupCode::InvalidInput
                : TeacherDisplayNameLookupCode::Missing,
            .diagnostic = QStringLiteral(
                "The test teacher display name is unavailable."
                )
        };
    }

    const Result<Teacher> loaded = DataService().getTeacher(
        static_cast<int>(teacherId)
        );
    if (!loaded)
    {
        return {
            .code = TeacherDisplayNameLookupCode::Failure,
            .diagnostic = loaded.error()
        };
    }

    return {
        .code = TeacherDisplayNameLookupCode::Found,
        .record = {
            .teacherKr = loaded->teacherKr,
            .teacherEn = loaded->teacherEn,
            .preferredRomanization = loaded->preferredRomanization,
            .preferredName = loaded->preferredName
        }
    };
}

Result<int> RosterRepository::getRosterStudentCount(const int classId)
{
    return DataService().getRosterStudentCount(classId);
}

Result<SelectedClassGradeReadRecord>
ClassInfoRepository::loadSelectedClassGradeRecord(const int classId)
{
    const auto loaded = loadSelectedClassGradeRecordDetailed(classId);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    return loaded->record;
}

Result<SelectedClassGradeReadOutcome>
ClassInfoRepository::loadSelectedClassGradeRecordDetailed(const int classId)
{
    ++ScheduleWidgetTestStubs::selectedClassGradeReadCount;
    if (ScheduleWidgetTestStubs::selectedClassGradeReadWillFail())
    {
        return std::unexpected(
            QStringLiteral("Injected selected class grade read failure.")
            );
    }

    SelectedClassGradeReadOutcome outcome;
    outcome.record.classId = classId;
    outcome.record.classGrade = ScheduleWidgetTestStubs::selectedClassGrade(
        classId
        );
    return outcome;
}
