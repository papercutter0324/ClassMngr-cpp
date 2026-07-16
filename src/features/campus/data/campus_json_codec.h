#pragma once

#include "domain/models/campus_info.h"

#include <QJsonObject>

class CampusJsonCodec
{
public:
    static QJsonObject toJson(const CampusInfo& campus);
    static CampusInfo fromJson(const QJsonObject& object);
};
