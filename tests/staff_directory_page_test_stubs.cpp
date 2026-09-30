#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "data/database/database_session.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"

DatabaseSession* ApplicationServices::databaseSession() const
{
    return nullptr;
}

bool DatabaseSession::isOpen() const
{
    return false;
}

NativeEnglishTeacherRepository*
DatabaseSession::nativeEnglishTeacherRepository() const
{
    return nullptr;
}

GsTeamRepository* DatabaseSession::gsTeamRepository() const
{
    return nullptr;
}

Result<QList<NativeEnglishTeacher>>
NativeEnglishTeacherRepository::getAll() const
{
    return QList<NativeEnglishTeacher>{};
}

Status NativeEnglishTeacherRepository::saveDirectory(
    const QList<NativeEnglishTeacher>&,
    const QList<int>&
    )
{
    return {};
}

Result<QList<GsTeamMember>> GsTeamRepository::getAll() const
{
    return QList<GsTeamMember>{};
}

Status GsTeamRepository::saveDirectory(
    const QList<GsTeamMember>&,
    const QList<int>&
    )
{
    return {};
}

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
