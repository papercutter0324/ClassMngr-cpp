#include "app/services/feature_services.h"

FeatureService::FeatureService(DataService*)
{
}

bool FeatureService::isAvailable() const
{
    return true;
}

QList<Classroom> ClassService::classes() const
{
    return {};
}

ClassInfo ClassService::classInfo(
    int
    ) const
{
    return {};
}
