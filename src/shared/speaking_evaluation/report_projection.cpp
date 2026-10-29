#include "shared/speaking_evaluation/report_projection.h"
#include "next/application/speaking_evaluation_report_content.h"
#include "next/domain/course.h"
#include <QDate>
#include <QObject>
#include <vector>
namespace {
namespace Application = ClassMngr::Next::Application;
QString fromApplicationString(const std::u16string& value) { return QString::fromStdU16String(value); }
std::u16string toApplicationString(const QString& value) { return value.toStdU16String(); }
int toLegacySpeakingEvaluationGrade(ClassMngr::Next::Domain::CourseGradeBand grade) {
 using G = ClassMngr::Next::Domain::CourseGradeBand;
 return grade == G::E4 ? 4 : grade == G::E5 ? 5 : grade == G::E6 ? 6 : 0;
}
}
QString speakingEvalReportDate(
    const QDate& date,
    SpeakingEvalReportTemplate reportTemplate
    )
{
    if (reportTemplate == SpeakingEvalReportTemplate::Advanced)
    {
        return date.toString(QStringLiteral("MMM. yyyy"));
    }

    return date.toString(
        date.month() >= 5 && date.month() <= 7
            ? QStringLiteral("MMMM yyyy")
            : QStringLiteral("MMM. yyyy")
        );
}

QList<SpeakingEvalBatchReportService::StudentReport>
buildSpeakingEvalStudentReports(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    const QByteArray& signatureImage
    )
{
    Application::SpeakingEvaluationReportContentRequest request;
    request.rows.reserve(static_cast<std::size_t>(rows.size()));
    for (const QStringList& row : rows)
    {
        std::vector<std::u16string> values;
        values.reserve(static_cast<std::size_t>(row.size()));
        for (const QString& value : row)
        {
            values.push_back(toApplicationString(value));
        }
        request.rows.push_back(std::move(values));
    }
    request.classValues = {
        .grade = toApplicationString(classInfo.classGrade),
        .level = toApplicationString(classInfo.classLevel),
        .nativeTeacher = toApplicationString(classInfo.teacherEn),
        .koreanTeacher = toApplicationString(classInfo.teacherKr)
    };

    const Application::SpeakingEvaluationReportContent projected =
        Application::projectSpeakingEvaluationReportContent(
            std::move(request)
            );

    QList<SpeakingEvalBatchReportService::StudentReport> reports;
    for (const Application::SpeakingEvaluationReportStudentContent& student :
         projected.students)
    {
        const QString englishName =
            fromApplicationString(student.displayEnglishName);
        const QString koreanName =
            fromApplicationString(student.displayKoreanName);
        QString displayName;
        if (englishName.isEmpty())
        {
            displayName = koreanName;
        }
        else if (koreanName.isEmpty())
        {
            displayName = englishName;
        }
        else
        {
            displayName = QObject::tr("%1 (%2)").arg(englishName, koreanName);
        }

        SpeakingEvalReportData report;
        report.englishName = fromApplicationString(student.englishName);
        report.koreanName = fromApplicationString(student.koreanName);
        report.classLabel = fromApplicationString(projected.classLabel);
        report.nativeTeacher = fromApplicationString(projected.nativeTeacher);
        report.koreanTeacher = fromApplicationString(projected.koreanTeacher);
        report.comments = fromApplicationString(student.comments);
        report.notes = fromApplicationString(student.notes);
        report.grade = toLegacySpeakingEvaluationGrade(projected.grade);
        report.reportTemplate =
            projected.reportTemplate
                    == Application::SpeakingEvaluationReportTemplate::Advanced
                ? SpeakingEvalReportTemplate::Advanced
                : SpeakingEvalReportTemplate::Standard;
        report.date = speakingEvalReportDate(
            QDate::currentDate(),
            report.reportTemplate
            );
        for (std::size_t scoreIndex = 0;
             scoreIndex < student.scores.size();
             ++scoreIndex)
        {
            report.scores[scoreIndex] = fromApplicationString(
                student.scores[scoreIndex]
                );
        }
        report.signatureImage = signatureImage;
        reports.append({
            displayName,
            report,
            static_cast<int>(student.sourceRow.rawValue())
        });
    }
    return reports;
}

int speakingEvalReportIndexForSourceRow(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    int sourceRow
    )
{
    for (int index = 0; index < reports.size(); ++index)
    {
        if (reports.at(index).sourceRow == sourceRow)
        {
            return index;
        }
    }

    return -1;
}
