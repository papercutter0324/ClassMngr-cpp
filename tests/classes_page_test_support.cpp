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
    ++ScheduleWidgetTestStubs::selectedClassGradeReadCount;
    if (ScheduleWidgetTestStubs::selectedClassGradeReadWillFail())
    {
        return std::unexpected(
            QStringLiteral("Injected selected class grade read failure.")
            );
    }

    SelectedClassGradeReadRecord record;
    record.classId = classId;
    record.classGrade = ScheduleWidgetTestStubs::selectedClassGrade(classId);
    return record;
}
