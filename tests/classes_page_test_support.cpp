#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/settings_repository.h"

#include <QSqlDatabase>

namespace ScheduleWidgetTestStubs
{
void saveSessionSetting(const QString& key, const QVariant& value);
QVariant loadSessionSetting(const QString& key);
QString selectedClassGrade(int classId);
QString selectedClassLevel(int classId);
bool selectedClassGradeReadWillFail();
extern int selectedClassGradeReadCount;
}

SettingsRepository* DatabaseSession::settingsRepository() const
{
    if (!isOpen())
    {
        return nullptr;
    }

    static QSqlDatabase database;
    static SettingsRepository repository(database);
    return &repository;
}

Status SettingsRepository::saveSetting(
    const QString& key,
    const QVariant& value
    )
{
    ScheduleWidgetTestStubs::saveSessionSetting(key, value);
    return {};
}

Status SettingsRepository::saveSettings(
    const QVariantMap& values
    )
{
    for (auto setting = values.cbegin(); setting != values.cend(); ++setting)
    {
        ScheduleWidgetTestStubs::saveSessionSetting(
            setting.key(),
            setting.value()
            );
    }
    return {};
}

Result<QVariant> SettingsRepository::loadSetting(const QString& key)
{
    return ScheduleWidgetTestStubs::loadSessionSetting(key);
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
