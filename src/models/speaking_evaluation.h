#pragma once

#include <QList>
#include <QStringList>

enum class Semester
{
    Winter,
    Spring,
    Summer,
    Fall
};

struct SpeakingEvaluations
{
    QList<QStringList> winter;
    QList<QStringList> spring;
    QList<QStringList> summer;
    QList<QStringList> fall;
};
