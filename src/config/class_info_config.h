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

// =========================
// Core Lookups
// =========================
QStringList levelsForGrade(const QString& grade);

QStringList readingBooks(const QString& grade, const QString& level);

QStringList essayBooks(const QString& grade, const QString& level);

}