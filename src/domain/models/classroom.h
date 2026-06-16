#pragma once

#include <QString>

class Classroom
{
public:
    Classroom(
        const QString& name = {},
        int id = -1
        );

public:
    int id{-1};

    QString name;
};