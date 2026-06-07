#ifndef CLASS_CONFLICT_H
#define CLASS_CONFLICT_H

struct ClassConflict
{
    int classId{-1};

    QString className;

    QString day;

    QString startTime;
    QString endTime;

    QString conflictingClassName;
};

#endif // CLASS_CONFLICT_H
