#pragma once

#include <QString>
#include <QStringList>

struct TestingClass
{
    int classId{-1};
    QString name;
    QString grade;
    QString level;
    QString room;
    int teacherId{-1};
    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};
    QString notes;
};

inline QStringList testingClassMixedLevels()
{
    return {
        QStringLiteral("Mixed (All)"),
        QStringLiteral("Mixed (High)"),
        QStringLiteral("Mixed (Low)")
    };
}

inline QStringList testingClassGrades()
{
    return {
        QStringLiteral("M1"),
        QStringLiteral("M2"),
        QStringLiteral("Mixed")
    };
}

inline QStringList testingClassLevelsForGrade(
    const QString& grade
    )
{
    QStringList levels =
        testingClassMixedLevels();

    if (grade == QStringLiteral("M1"))
    {
        levels.append({
            QStringLiteral("Song's"),
            QStringLiteral("Major"),
            QStringLiteral("Solis"),
            QStringLiteral("Galaxia"),
            QStringLiteral("Elephantus")
        });
    }
    else if (grade == QStringLiteral("M2"))
    {
        levels.append({
            QStringLiteral("Song's"),
            QStringLiteral("Major"),
            QStringLiteral("Tigris"),
            QStringLiteral("Leo"),
            QStringLiteral("Ursa")
        });
    }

    return levels;
}
