#include "speaking_eval_report_output_policy.h"

#include "core/utils/file_name_utils.h"

#include <QDir>
#include <QObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTime>

namespace
{
QString safeFolderName(const QString& value, const QString& fallback)
{
    QString name = value.trimmed();
    name.replace(
        QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
        QStringLiteral("-")
        );
    name = name.simplified();
    return name.isEmpty() ? fallback : name;
}

QString shortDay(const QString& day)
{
    const QString normalized = day.trimmed().toLower();
    if (normalized.startsWith(QStringLiteral("mon"))) return QStringLiteral("M");
    if (normalized.startsWith(QStringLiteral("tue"))) return QStringLiteral("T");
    if (normalized.startsWith(QStringLiteral("wed"))) return QStringLiteral("W");
    if (normalized.startsWith(QStringLiteral("thu"))) return QStringLiteral("Th");
    if (normalized.startsWith(QStringLiteral("fri"))) return QStringLiteral("F");
    if (normalized.startsWith(QStringLiteral("sat"))) return QStringLiteral("Sa");
    if (normalized.startsWith(QStringLiteral("sun"))) return QStringLiteral("Su");
    return day.trimmed().left(2);
}

QString shortTime(const QString& value)
{
    const QStringList formats{
        QStringLiteral("h:mm AP"), QStringLiteral("h:mmAP"),
        QStringLiteral("hh:mm AP"), QStringLiteral("hh:mmAP"),
        QStringLiteral("H:mm"), QStringLiteral("HH:mm")
    };
    for (const QString& format : formats)
    {
        const QTime time = QTime::fromString(value.trimmed(), format);
        if (time.isValid())
        {
            return time.toString(
                time.minute() == 0
                    ? QStringLiteral("hap")
                    : QStringLiteral("h:mmap")
                ).toLower();
        }
    }
    return value.trimmed().remove(QLatin1Char(' ')).toLower();
}
}

QString SpeakingEvalReportOutputPolicy::defaultDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory
    )
{
    QString className = QStringList{
        classInfo.classGrade.trimmed(),
        classInfo.classLevel.trimmed()
    }.filter(QRegularExpression(QStringLiteral(".+"))).join(QLatin1Char(' '));
    if (className.isEmpty())
    {
        className = QObject::tr("Speaking Evaluation");
    }

    QStringList days;
    for (const ClassTime& classTime : classInfo.classTimes)
    {
        const QString day = shortDay(classTime.day);
        if (!day.isEmpty() && !days.contains(day))
        {
            days.append(day);
        }
    }
    QString schedule;
    if (!classInfo.classTimes.isEmpty())
    {
        const QString time = shortTime(classInfo.classTimes.first().startTime);
        if (!days.isEmpty() && !time.isEmpty())
        {
            schedule = QStringLiteral("%1 - %2").arg(days.join(QString()), time);
        }
        else
        {
            schedule = !days.isEmpty() ? days.join(QString()) : time;
        }
    }
    if (!schedule.trimmed().isEmpty())
    {
        className += QStringLiteral(" (%1)").arg(schedule);
    }

    const QString root = documentsDirectory.trimmed().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : documentsDirectory;
    return QDir::cleanPath(
        QDir(root).filePath(
            QStringLiteral("DYB/SpeakingEvals/%1/%2")
                .arg(
                    safeFolderName(className, QObject::tr("Speaking Evaluation")),
                    safeFolderName(evaluationName, QObject::tr("Evaluation"))
                    )
            )
        );
}

QString SpeakingEvalReportOutputPolicy::batchArchivePath(
    const QString& outputDirectory
    )
{
    const QDir directory(QDir::cleanPath(outputDirectory));
    const QString baseName = safeFolderName(
        directory.dirName(),
        QObject::tr("Speaking Evaluation Reports")
        );
    return directory.filePath(baseName + QStringLiteral(".zip"));
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
