#include "core/application_services.h"
#include "data/data_service.h"

DataService* ApplicationServices::dataService() const
{
    return nullptr;
}

void DataService::updateTeacher(const Teacher&)
{
}

Teacher DataService::getTeacher(int)
{
    return {};
}
