#include "roster.h"

#include "classmngr/engine/roster.h"

const QStringList Roster::BaseColumns = []
{
    QStringList result;
    result.reserve(
        static_cast<qsizetype>(classmngr::engine::RosterBaseColumns.size())
        );
    for (const std::string_view column
         : classmngr::engine::RosterBaseColumns)
    {
        result.append(QString::fromUtf8(
            column.data(),
            static_cast<qsizetype>(column.size())
            ));
    }
    return result;
}();

void Roster::reset()
{
    columns.clear();
    columnWidths.clear();
    rows.clear();
}
