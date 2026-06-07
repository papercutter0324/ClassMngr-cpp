#pragma once

#include <QString>
#include <QStringList>

namespace ClassInfoConfig
{
extern const QStringList Grades;

extern const QStringList Days;

QStringList levelsForGrade(
    const QString& grade
    );

QStringList readingBooks(
    const QString& grade,
    const QString& level
    );

QStringList essayBooks(
    const QString& grade,
    const QString& level
    );
}