#include "core/application_services.h"
#include "app/services/feature_services.h"

TeacherService* ApplicationServices::teacherService() const
{
    return nullptr;
}

bool FeatureService::isAvailable() const
{
    return false;
}

Result<QList<NativeEnglishTeacher>> TeacherService::nativeEnglishTeachers() const
{
    return {};
}

Status TeacherService::saveNativeEnglishTeacherDirectory(
    const QList<NativeEnglishTeacher>&,
    const QList<int>&) const
{
    return {};
}

Result<QList<GsTeamMember>> TeacherService::gsTeamMembers() const
{
    return {};
}

Status TeacherService::saveGsTeamDirectory(
    const QList<GsTeamMember>&,
    const QList<int>&) const
{
    return {};
}
