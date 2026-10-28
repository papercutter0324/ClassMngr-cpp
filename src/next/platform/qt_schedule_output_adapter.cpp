#include "next/platform/qt_schedule_output_adapter.h"

#include "features/schedule/services/schedule_print_service.h"

#include <exception>
#include <utility>

namespace ClassMngr::Next::Platform
{

Ui::ScheduleOutputResult performScheduleOutput(
    Ui::ScheduleOutputRequest request,
    QWidget* parent
    )
{
    SchedulePrintService::Request printRequest;
    printRequest.parent = parent;
    printRequest.document = std::move(request.document);
    printRequest.style = request.style;
    printRequest.currentTheme = request.currentTheme;
    printRequest.englishFont = std::move(request.englishFont);
    printRequest.koreanFont = std::move(request.koreanFont);
    printRequest.userName = std::move(request.profileName);
    printRequest.showEnglishNames = request.showEnglishNames;
    printRequest.pageOrientation = request.orientation;

    try
    {
        const SchedulePrintService::Result result =
            request.kind == Ui::ScheduleOutputKind::Print
                ? SchedulePrintService::printSchedule(printRequest)
                : SchedulePrintService::saveSchedulePdf(
                    printRequest, request.documentPath);
        using Status = Ui::ScheduleOutputResult::Status;
        switch (result.status)
        {
        case SchedulePrintService::Status::Sent:
            return {Status::Sent, result.message};
        case SchedulePrintService::Status::Canceled:
            return {Status::Canceled, {}};
        case SchedulePrintService::Status::Failed:
        default:
            return {Status::Failed, result.message};
        }
    }
    catch (const std::exception&)
    {
        return {
            Ui::ScheduleOutputResult::Status::Failed,
            {}
        };
    }
    catch (...)
    {
        return {
            Ui::ScheduleOutputResult::Status::Failed,
            {}
        };
    }
}

} // namespace ClassMngr::Next::Platform
