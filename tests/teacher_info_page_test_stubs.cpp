#include "core/application_services.h"
#include "data/data_service.h"

DataService* ApplicationServices::dataService() const
{
    return nullptr;
}

Status DataService::updateTeacher(const Teacher&)
{
    return {};
}

Teacher DataService::getTeacher(int)
{
    return {};
}
