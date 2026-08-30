#include "speaking_eval_report_output_policy.h"

#include "classmngr/engine/speaking_evaluation_report_output_policy.h"
#include "core/utils/file_name_utils.h"

#include <QObject>
#include <QStandardPaths>

namespace
{
classmngr::engine::ClassInfo toPortableClassInfo(
    const ClassInfo& source
    )
{
    classmngr::engine::ClassInfo result;
    result.classGrade = source.classGrade.toStdString();
    result.classLevel = source.classLevel.toStdString();
    result.classTimes.reserve(source.classTimes.size());
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back({
            time.day.toStdString(),
            time.startTime.toStdString(),
            time.endTime.toStdString()
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
    return QString::fromStdString(
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::defaultDirectory(
            toPortableClassInfo(classInfo),
            evaluationName.toStdString(),
            root.toStdString(),
            QObject::tr("Speaking Evaluation").toStdString(),
            QObject::tr("Evaluation").toStdString()
            )
        );
}

QString SpeakingEvalReportOutputPolicy::batchArchivePath(
    const QString& outputDirectory
    )
{
    return QString::fromStdString(
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::batchArchivePath(
            outputDirectory.toStdString(),
            QObject::tr("Speaking Evaluation Reports").toStdString()
            )
        );
}

QString SpeakingEvalReportOutputPolicy::studentFileName(
    const QString& englishName,
    const QString& koreanName
    )
{
    const QString english = englishName.trimmed();
    const QString korean = koreanName.trimmed();
    QString baseName;
    if (!english.isEmpty() && !korean.isEmpty())
    {
        baseName = QStringLiteral("%1 (%2)").arg(english, korean);
    }
    else
    {
        baseName = !english.isEmpty() ? english : korean;
    }
    return FileNameUtils::filesystemSafeFileName(
        baseName.simplified(),
        QStringLiteral(".pdf"),
        QObject::tr("Student"),
        QChar(u'-')
        );
}
