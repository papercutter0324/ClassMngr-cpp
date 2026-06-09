#pragma once

#include <QString>
#include <QStringList>
#include <QHash>

namespace ClassInfoConfig
{

// =========================
// Static UI Lists
// =========================
extern const QStringList Grades;
extern const QStringList Days;
extern const QStringList RegularHours;
extern const QStringList IntensiveHours;
extern const QStringList StartMinutes;
extern const QStringList EndMinutes;

// =========================
// Core Lookups
// =========================
QStringList levelsForGrade(const QString& grade);

QStringList readingBooks(const QString& grade, const QString& level);

QStringList essayBooks(const QString& grade, const QString& level);

}
