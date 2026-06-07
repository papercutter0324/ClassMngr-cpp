#pragma once

#include <QList>
#include <QString>

struct ClassTime
{
    QString day{"Monday"};

    QString startTime{"4:00 PM"};
    QString endTime{"4:50 PM"};
};

struct ClassInfo
{
    int classId{-1};

    int teacherId{-1};

    QString classGrade;
    QString classLevel;

    QString readingBook;
    QString essayBook;

    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};

    QList<ClassTime> classTimes;
    QList<ClassTime> intensiveTimes;

    QString notes;
};