#include "core/application_services.h"
#include "data/data_service.h"

DataService* ApplicationServices::dataService() const
{
    return nullptr;
}

bool DataService::isOpen() const
{
    return false;
}

QList<NativeEnglishTeacher> DataService::getNativeEnglishTeachers()
{
    return {};
}

Status DataService::saveNativeEnglishTeacherDirectory(
    const QList<NativeEnglishTeacher>&,
    const QList<int>&)
{
    return {};
}

QList<GsTeamMember> DataService::getGsTeamMembers()
{
    return {};
}

Status DataService::saveGsTeamDirectory(
    const QList<GsTeamMember>&,
    const QList<int>&)
{
    return {};
}
