#include "next/platform/workspace_store_speaking_evaluation_report_port.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/settings_repository.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/platform/repository_error_code.h"
#include <QByteArray>
#include <utility>

namespace ClassMngr::Next::Platform
{
WorkspaceStoreSpeakingEvaluationReportPort::WorkspaceStoreSpeakingEvaluationReportPort(QString path)
    : m_path(std::move(path)) {}

Application::SpeakingEvaluationReportContextReadResult
WorkspaceStoreSpeakingEvaluationReportPort::readReportContext(const Domain::ClassId& classId) const
{
    using Result = Application::SpeakingEvaluationReportContextReadResult;
    const auto id = Application::parseCanonicalPositiveDecimalId(classId.value());
    if (!id) return Result::failure({Domain::ErrorCode::InvalidInput, "Invalid report class ID.", false});
    WorkspaceStore store;
    const auto opened = store.openExisting(m_path);
    if (!opened) return Result::failure({repositoryDomainErrorCode(opened.error().code), opened.error().diagnostic.toStdString(), true});
    // Explicitly omit connectivity credentials from the report projection.
    const auto loaded = store.classInfoRepository()->loadRosterPrintClassInfoRecordDetailed(*id, false, false);
    if (!loaded) return Result::failure({repositoryDomainErrorCode(loaded.error().code), loaded.error().diagnostic.toStdString(), true});
    if (loaded->parentMissing) return Result::failure({Domain::ErrorCode::NotFound, "The report class is unavailable.", true});
    const auto signature = store.settingsRepository()->loadSetting(QStringLiteral("myInfo/signatureImage"));
    if (!signature) return Result::failure({repositoryDomainErrorCode(signature.error().code), signature.error().diagnostic.toStdString(), true});
    const auto& record = loaded->record;
    Application::SpeakingEvaluationReportContext result;
    result.classValues = {record.classGrade.toStdU16String(), record.classLevel.toStdU16String(),
        record.teacherEnglishName.toStdU16String(), record.teacherKoreanName.toStdU16String()};
    for (const auto& time : record.regularTimes)
        result.regularTimes.push_back({time.day.toStdU16String(), time.startTime.toStdU16String(), time.endTime.toStdU16String()});
    const QByteArray bytes = QByteArray::fromBase64(signature->toString().toLatin1());
    result.signatureImage.assign(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    return Result::success(std::move(result));
}
}
