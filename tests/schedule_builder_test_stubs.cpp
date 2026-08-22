#include "app/services/feature_services.h"

FeatureService::FeatureService(DataService*)
{
}

bool FeatureService::isAvailable() const
{
    return true;
}

Result<QList<Classroom>> ClassService::classes() const
{
    return {};
}

Result<ClassInfo> ClassService::classInfo(
    int
    ) const
{
    return {};
}
