#include "roster.h"

const QStringList Roster::BaseColumns{
    "English Name",
    "Korean Name",
    "Winter",
    "Spring",
    "Summer",
    "Fall",
    "Speech",
};

void Roster::reset()
{
    columns.clear();
    rows.clear();
}