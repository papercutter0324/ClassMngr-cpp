#include "features/classes/evaluation_default_selection.h"

#include "classmngr/engine/evaluation_default_selection.h"

#include <QByteArray>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace
{
using EngineSelection = classmngr::engine::EvaluationDefaultSelection;
using EngineRow = classmngr::engine::SpeakingEvaluationRow;
using EngineRows = classmngr::engine::SpeakingEvaluationRows;

std::string toUtf8(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(
        utf8.constData(),
        static_cast<std::size_t>(utf8.size())
        );
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

classmngr::engine::AcademicTerm toEngineTerm(AcademicTerm term)
{
    return static_cast<classmngr::engine::AcademicTerm>(
        static_cast<int>(term)
        );
}

EngineRows toEngineRows(const SpeakingEvalRows& rows)
{
    EngineRows result;
    result.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& row : rows)
    {
        EngineRow engineRow;
        engineRow.reserve(static_cast<std::size_t>(row.size()));
        for (const QString& value : row)
        {
            engineRow.push_back(toUtf8(value));
        }
        result.push_back(std::move(engineRow));
    }

    return result;
}

} // namespace

namespace EvaluationDefaultSelection
{

QString evaluationNameForTerm(AcademicTerm term)
{
    return fromUtf8(
        EngineSelection::evaluationNameForTerm(toEngineTerm(term))
        );
}

bool isPopulated(const SpeakingEvalRows& rows)
{
    return EngineSelection::isPopulated(toEngineRows(rows));
}

QString forTermSchedule(
    const AcademicCalendarSchedule& schedule,
    SchoolLevel schoolLevel,
    const QDate& date,
    bool currentTermEvaluationIsPopulated
    )
{
    if (!schedule.hasSavedSchedules())
    {
        return {};
    }

    const AcademicTermPosition position = schedule.termAt(schoolLevel, date);
    if (!position.valid)
    {
        return {};
    }

    const classmngr::engine::AcademicTerm selectedTerm =
        EngineSelection::termForEvaluation(
            toEngineTerm(position.term),
            currentTermEvaluationIsPopulated
            );
    return fromUtf8(
        EngineSelection::evaluationNameForTerm(selectedTerm)
        );
}

} // namespace EvaluationDefaultSelection
