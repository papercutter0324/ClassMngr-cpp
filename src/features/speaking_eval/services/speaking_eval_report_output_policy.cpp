#include "speaking_eval_report_output_policy.h"

#include "classmngr/engine/speaking_evaluation_report_output_policy.h"

#include <QObject>
#include <QByteArray>
#include <QStandardPaths>

#include <string>

namespace
{
std::string toUtf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

classmngr::engine::ClassInfo toPortableClassInfo(
    const ClassInfo& source
    )
{
    classmngr::engine::ClassInfo result;
    result.classGrade = toUtf8(source.classGrade);
    result.classLevel = toUtf8(source.classLevel);
    result.classTimes.reserve(source.classTimes.size());
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back({
            toUtf8(time.day),
            toUtf8(time.startTime),
            toUtf8(time.endTime)
        });
    }
    return result;
}
}

QString SpeakingEvalReportOutputPolicy::defaultDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory
    )
{
    const QString root = documentsDirectory.trimmed().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : documentsDirectory;
    return fromUtf8(
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::defaultDirectory(
            toPortableClassInfo(classInfo),
            toUtf8(evaluationName),
            toUtf8(root),
            toUtf8(QObject::tr("Speaking Evaluation")),
            toUtf8(QObject::tr("Evaluation"))
            )
        );
}

QString SpeakingEvalReportOutputPolicy::batchArchivePath(
    const QString& outputDirectory
    )
{
    return fromUtf8(
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::batchArchivePath(
            toUtf8(outputDirectory),
            toUtf8(QObject::tr("Speaking Evaluation Reports"))
            )
        );
}

QString SpeakingEvalReportOutputPolicy::studentFileName(
    const QString& englishName,
    const QString& koreanName
    )
{
    const QString english =
        englishName.normalized(QString::NormalizationForm_C).simplified();
    const QString korean =
        koreanName.normalized(QString::NormalizationForm_C).simplified();
    const std::string englishUtf8 = toUtf8(english);
    const std::string koreanUtf8 = toUtf8(korean);
    const std::string fallbackUtf8 = toUtf8(QObject::tr("Student"));
    const std::string output =
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::studentFileName(
            englishUtf8,
            koreanUtf8,
            ".pdf",
            fallbackUtf8,
            '-'
            );
    return fromUtf8(output);
}
