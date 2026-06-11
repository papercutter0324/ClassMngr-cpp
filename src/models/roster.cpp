#include "roster.h"

const QStringList Roster::BaseColumns{
    "English",
    "Korean",
    "Winter",
    "Speech Contest",
    "Summer",
    "Autumn",
};

void Roster::reset()
{
    columns.clear();
    columnWidths.clear();
    rows.clear();
}
