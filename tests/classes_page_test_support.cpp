#include "data/repositories/class_info_repository.h"

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
