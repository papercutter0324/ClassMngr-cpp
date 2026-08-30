#include "features/classes/services/speaking_analytics.h"

#include "classmngr/engine/speaking_analytics.h"

#include <QByteArray>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SpeakingAnalytics
{
namespace
{
using PortableCriterionSlice =
    classmngr::engine::SpeakingAnalyticsCriterionSlice;
using PortableRoster = classmngr::engine::SpeakingAnalyticsRoster;
using PortableRow = classmngr::engine::SpeakingAnalyticsRow;
using PortableRows = classmngr::engine::SpeakingAnalyticsRows;
using PortableService = classmngr::engine::SpeakingAnalyticsService;
using PortableSnapshot = classmngr::engine::SpeakingAnalyticsSnapshot;
using PortableStudentRank =
    classmngr::engine::SpeakingAnalyticsStudentRank;
using PortableYearToDatePoint =
    classmngr::engine::SpeakingAnalyticsYearToDatePoint;

std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

PortableRow toPortable(const QStringList& row)
{
    PortableRow result;
    result.reserve(static_cast<std::size_t>(row.size()));
    for (const QString& value : row)
    {
        result.push_back(toUtf8(value));
    }
    return result;
}

PortableRows toPortable(const SpeakingEvalRows& rows)
{
    PortableRows result;
    result.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& row : rows)
    {
        result.push_back(toPortable(row));
    }
    return result;
}

std::vector<PortableRows> toPortable(
    const QList<SpeakingEvalRows>& matrices
    )
{
    std::vector<PortableRows> result;
    result.reserve(static_cast<std::size_t>(matrices.size()));
    for (const SpeakingEvalRows& matrix : matrices)
    {
        result.push_back(toPortable(matrix));
    }
    return result;
}

SpeakingEvalRows fromPortable(const PortableRows& rows)
{
    SpeakingEvalRows result;
    result.reserve(static_cast<qsizetype>(rows.size()));
    for (const PortableRow& row : rows)
    {
        QStringList converted;
        converted.reserve(static_cast<qsizetype>(row.size()));
        for (const std::string& value : row)
        {
            converted.append(fromUtf8(value));
        }
        result.append(std::move(converted));
    }
    return result;
}

PortableRoster toPortable(const Roster& roster)
{
    PortableRoster result;
    result.columns.reserve(static_cast<std::size_t>(roster.columns.size()));
    for (const QString& column : roster.columns)
    {
        result.columns.push_back(toUtf8(column));
    }
    result.rows.reserve(static_cast<std::size_t>(roster.rows.size()));
    for (const QStringList& row : roster.rows)
    {
        result.rows.push_back(toPortable(row));
    }
    return result;
}

CriterionSlice fromPortable(const PortableCriterionSlice& source)
{
    CriterionSlice result;
    result.order = source.order;
    result.name = fromUtf8(source.name);
    result.students = source.students;
    for (const auto& [grade, count] : source.distribution)
    {
        result.distribution.insert(fromUtf8(grade), count);
    }
    result.average3 = source.average3;
    result.hasData = source.hasData;
    return result;
}

StudentRank fromPortable(const PortableStudentRank& source)
{
    StudentRank result;
    result.englishName = fromUtf8(source.englishName);
    result.koreanName = fromUtf8(source.koreanName);
    result.overall3 = source.overall3;
    result.overallLetter = fromUtf8(source.overallLetter);
    result.criterionLetters.reserve(
        static_cast<qsizetype>(source.criterionLetters.size())
        );
    for (const std::string& letter : source.criterionLetters)
    {
        result.criterionLetters.append(fromUtf8(letter));
    }
    result.fullyScored = source.fullyScored;
    return result;
}

Snapshot fromPortable(const PortableSnapshot& source)
{
    Snapshot result;
    result.hasData = source.hasData;
    result.classAverage3 = source.classAverage3;
    result.classAverageLetter = fromUtf8(source.classAverageLetter);
    result.rosterStudentCount = source.rosterStudentCount;
    result.fullyScoredCount = source.fullyScoredCount;

    result.criteria.reserve(static_cast<qsizetype>(source.criteria.size()));
    for (const PortableCriterionSlice& slice : source.criteria)
    {
        result.criteria.append(fromPortable(slice));
    }

    const auto appendStrings = [](const std::vector<std::string>& source,
                                  QStringList* destination)
    {
        destination->reserve(static_cast<qsizetype>(source.size()));
        for (const std::string& value : source)
        {
            destination->append(fromUtf8(value));
        }
    };
    appendStrings(source.strongestNames, &result.strongestNames);
    appendStrings(source.focusNames, &result.focusNames);
    appendStrings(source.strongestLabels, &result.strongestLabels);
    appendStrings(source.focusLabels, &result.focusLabels);
    appendStrings(source.overallLetters, &result.overallLetters);

    result.rankings.reserve(static_cast<qsizetype>(source.rankings.size()));
    for (const PortableStudentRank& rank : source.rankings)
    {
        result.rankings.append(fromPortable(rank));
    }
    return result;
}

YearToDatePoint fromPortable(const PortableYearToDatePoint& source)
{
    return {
        fromUtf8(source.evaluationName),
        source.classAverage3,
        fromUtf8(source.classAverageLetter)
    };
}

QList<int> fromPortable(const std::vector<int>& values)
{
    QList<int> result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const int value : values)
    {
        result.append(value);
    }
    return result;
}
} // namespace

QStringList evaluationNames()
{
    const std::vector<std::string> names = PortableService::evaluationNames();
    QStringList result;
    result.reserve(static_cast<qsizetype>(names.size()));
    for (const std::string& name : names)
    {
        result.append(fromUtf8(name));
    }
    return result;
}

double roundTo3(double value)
{
    return PortableService::roundTo3(value);
}

QString formatAverage(double average)
{
    return fromUtf8(PortableService::formatAverage(average));
}

int gradeToNumber(const QString& grade)
{
    return PortableService::gradeToNumber(toUtf8(grade));
}

QString numberToGrade(int number)
{
    return fromUtf8(PortableService::numberToGrade(number));
}

int roundAverageToGrade(double average)
{
    return PortableService::roundAverageToGrade(average);
}

QList<int> strongestIndices(const QList<double>& averages3)
{
    std::vector<double> portable;
    portable.reserve(static_cast<std::size_t>(averages3.size()));
    for (const double average : averages3)
    {
        portable.push_back(average);
    }
    return fromPortable(PortableService::strongestIndices(portable));
}

QList<int> focusIndices(const QList<double>& averages3)
{
    std::vector<double> portable;
    portable.reserve(static_cast<std::size_t>(averages3.size()));
    for (const double average : averages3)
    {
        portable.push_back(average);
    }
    return fromPortable(PortableService::focusIndices(portable));
}

Snapshot compute(
    const QList<SpeakingEvalRows>& matrices,
    int rosterStudentCount
    )
{
    return fromPortable(PortableService::compute(
        toPortable(matrices),
        rosterStudentCount
        ));
}

std::optional<YearToDatePoint> yearToDatePoint(
    const QString& evaluationName,
    const Snapshot& snapshot
    )
{
    // Reuse the same conversion path as compute so the retained namespace
    // remains a presentation adapter rather than a second implementation.
    PortableSnapshot portableSnapshot;
    portableSnapshot.hasData = snapshot.hasData;
    portableSnapshot.classAverage3 = snapshot.classAverage3;
    portableSnapshot.classAverageLetter = toUtf8(snapshot.classAverageLetter);
    portableSnapshot.rosterStudentCount = snapshot.rosterStudentCount;
    portableSnapshot.fullyScoredCount = snapshot.fullyScoredCount;

    portableSnapshot.rankings.reserve(
        static_cast<std::size_t>(snapshot.rankings.size())
        );
    for (const StudentRank& rank : snapshot.rankings)
    {
        PortableStudentRank portableRank;
        portableRank.englishName = toUtf8(rank.englishName);
        portableRank.koreanName = toUtf8(rank.koreanName);
        portableRank.overall3 = rank.overall3;
        portableRank.overallLetter = toUtf8(rank.overallLetter);
        portableRank.fullyScored = rank.fullyScored;
        portableRank.criterionLetters.reserve(
            static_cast<std::size_t>(rank.criterionLetters.size())
            );
        for (const QString& letter : rank.criterionLetters)
        {
            portableRank.criterionLetters.push_back(toUtf8(letter));
        }
        portableSnapshot.rankings.push_back(std::move(portableRank));
    }

    const auto point = PortableService::yearToDatePoint(
        toUtf8(evaluationName),
        portableSnapshot
        );
    return point.has_value()
        ? std::optional<YearToDatePoint>(fromPortable(*point))
        : std::nullopt;
}

SpeakingEvalRows filterMatrixByRoster(
    const SpeakingEvalRows& matrix,
    const Roster& roster
    )
{
    return fromPortable(PortableService::filterMatrixByRoster(
        toPortable(matrix),
        toPortable(roster)
        ));
}

} // namespace SpeakingAnalytics
