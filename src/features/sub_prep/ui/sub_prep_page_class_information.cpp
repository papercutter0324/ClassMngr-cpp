#include "sub_prep_page_p.h"

#include "core/startup_profiler.h"
#include "next/application/sub_prep_class_details_query.h"
#include "next/application/sub_prep_class_information_state.h"
#include "next/application/sub_prep_schedule_summary_query.h"
#include "next/platform/application_services_sub_prep_class_details_port.h"
#include "next/platform/application_services_sub_prep_schedule_summary_port.h"
#include "features/sub_prep/ui/sub_prep_class_information_list_model.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QItemSelectionModel>
#include <QListView>
#include <QSignalBlocker>
#include <QTextEdit>

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <utility>

namespace Application = ClassMngr::Next::Application;
namespace Domain = ClassMngr::Next::Domain;

class SubPrepClassInformationDataAccess final
{
public:
    explicit SubPrepClassInformationDataAccess(
        ApplicationServices& services
        )
        : m_ownedSummaryReadPort(std::make_unique<
              ClassMngr::Next::Platform::
                  ApplicationServicesSubPrepScheduleSummaryPort
              >(services)),
          m_ownedDetailsReadPort(std::make_unique<
              ClassMngr::Next::Platform::
                  ApplicationServicesSubPrepClassDetailsPort
              >(services)),
          m_summaryQuery(*m_ownedSummaryReadPort),
          m_detailsQuery(*m_ownedDetailsReadPort)
    {
    }

    SubPrepClassInformationDataAccess(
        Application::SubPrepScheduleSummaryReadPort& summaryReadPort,
        Application::SubPrepClassDetailsReadPort& detailsReadPort
        )
        : m_summaryQuery(summaryReadPort),
          m_detailsQuery(detailsReadPort)
    {
    }

    [[nodiscard]] Application::SubPrepScheduleSummaryQueryResult
    loadSummaries(
        const Application::SubPrepScheduleScopeRequest& request
        )
    {
        return m_summaryQuery.execute(request);
    }

    [[nodiscard]] Application::SubPrepClassDetailsQueryResult loadDetails(
        const Domain::ClassId& classId
        )
    {
        return m_detailsQuery.execute(classId);
    }

private:
    std::unique_ptr<
        ClassMngr::Next::Platform::
            ApplicationServicesSubPrepScheduleSummaryPort
        > m_ownedSummaryReadPort;
    std::unique_ptr<
        ClassMngr::Next::Platform::
            ApplicationServicesSubPrepClassDetailsPort
        > m_ownedDetailsReadPort;
    Application::SubPrepScheduleSummaryQuery m_summaryQuery;
    Application::SubPrepClassDetailsQuery m_detailsQuery;
};

namespace
{
namespace Application = ClassMngr::Next::Application;
namespace Domain = ClassMngr::Next::Domain;

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::optional<Application::SubPrepWeekday> weekdayFor(
    const QString& day
    )
{
    using Application::SubPrepWeekday;
    if (day == QStringLiteral("Monday"))
    {
        return SubPrepWeekday::Monday;
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return SubPrepWeekday::Tuesday;
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return SubPrepWeekday::Wednesday;
    }
    if (day == QStringLiteral("Thursday"))
    {
        return SubPrepWeekday::Thursday;
    }
    if (day == QStringLiteral("Friday"))
    {
        return SubPrepWeekday::Friday;
    }
    if (day == QStringLiteral("Saturday"))
    {
        return SubPrepWeekday::Saturday;
    }
    if (day == QStringLiteral("Sunday"))
    {
        return SubPrepWeekday::Sunday;
    }
    return std::nullopt;
}

Application::SubPrepScheduleScopeRequest scheduleScopeRequest(
    const ScheduleViewModel& schedule,
    ScheduleDisplayMode displayMode
    )
{
    Application::SubPrepScheduleScopeRequest request;
    for (const QString& day : schedule.days)
    {
        const auto weekday = weekdayFor(day);
        if (weekday.has_value()
            && std::find(
                   request.selectedDays.cbegin(),
                   request.selectedDays.cend(),
                   *weekday
                   ) == request.selectedDays.cend())
        {
            request.selectedDays.push_back(*weekday);
        }
    }

    for (const ScheduleRowView& row : schedule.rows)
    {
        for (const ScheduleCellView& cell : row.cells)
        {
            if (!schedule.days.contains(cell.day))
            {
                continue;
            }

            for (const ScheduleEntry& entry : cell.entries)
            {
                if (entry.classId <= 0)
                {
                    continue;
                }

                auto classId = Domain::ClassId::fromString(
                    std::to_string(entry.classId)
                    );
                if (classId.has_value()
                    && std::find(
                           request.visibleClassIds.cbegin(),
                           request.visibleClassIds.cend(),
                           *classId
                           ) == request.visibleClassIds.cend())
                {
                    request.visibleClassIds.push_back(std::move(*classId));
                }
            }
        }
    }

    request.mode = displayMode == ScheduleDisplayMode::Intensive
        ? Application::ScheduleViewMode::Intensive
        : Application::ScheduleViewMode::Regular;
    return request;
}

int legacyIntegerId(
    const std::string& value
    )
{
    int parsed = -1;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
        );
    return error == std::errc{} && end == value.data() + value.size()
        ? parsed
        : -1;
}

int legacyClassId(
    const Domain::ClassId& classId
    )
{
    return legacyIntegerId(classId.value());
}

QString operationErrorMessage(
    const Domain::OperationError& error
    )
{
    return fromUtf8(error.message);
}

QString runtimeMetricsDetail(
    const SubPrepPageRuntimeMetrics& metrics
    )
{
    return QStringLiteral(
        "widgets=%1; textEdits=%2; navigationRows=%3; sourceClasses=%4; "
        "visibleClasses=%5; groups=%6; classInfoLookups=%7; "
        "teacherLookups=%8; rosterLookups=%9; "
        "queryCounts=classes:%10,classInfo:%11,teachers:%12,rosterCounts:%13; "
        "resultRows=classes:%14,classInfo:%15,classInfoSchedule:%16,teachers:%17,"
        "rosterCounts:%18; returnedStudents=%19; rebuilds=%20; selectedClassId=%21"
        )
        .arg(metrics.classInformationWidgetCount)
        .arg(metrics.classInformationTextEditCount)
        .arg(metrics.classInformationNavigationRowCount)
        .arg(metrics.classInformationSourceClassCount)
        .arg(metrics.classInformationVisibleClassCount)
        .arg(metrics.classInformationGroupCount)
        .arg(metrics.classInformationClassInfoLookupCount)
        .arg(metrics.classInformationTeacherLookupCount)
        .arg(metrics.classInformationRosterLookupCount)
        .arg(metrics.classInformationClassQueryCount)
        .arg(metrics.classInformationClassInfoQueryCount)
        .arg(metrics.classInformationTeacherQueryCount)
        .arg(metrics.classInformationRosterQueryCount)
        .arg(metrics.classInformationClassResultRowCount)
        .arg(metrics.classInformationClassInfoResultRowCount)
        .arg(metrics.classInformationClassInfoScheduleRowCount)
        .arg(metrics.classInformationTeacherResultRowCount)
        .arg(metrics.classInformationRosterResultRowCount)
        .arg(metrics.classInformationRosterStudentResultCount)
        .arg(metrics.classInformationRebuildCount)
        .arg(metrics.selectedClassId);
}
}

void SubPrepPage::initialize()
{
    Q_ASSERT(m_services);
    Q_ASSERT(m_classInformationDataAccess);
    Q_ASSERT(m_classInformationState);

    setProperty("role", UiRoles::SubPrep);
    buildUi();

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(AutosaveDelayMs);

    connect(
        m_autosaveTimer,
        &QTimer::timeout,
        this,
        &SubPrepPage::autosave
        );
}

SubPrepPage::SubPrepPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent),
      m_services(services),
      m_classInformationDataAccess(
          services
              ? std::make_unique<SubPrepClassInformationDataAccess>(*services)
              : nullptr
          ),
      m_classInformationState(
          std::make_unique<Application::SubPrepClassInformationState>()
          )
{
    initialize();
}

SubPrepPage::SubPrepPage(
    ApplicationServices* services,
    Application::SubPrepScheduleSummaryReadPort& summaryReadPort,
    Application::SubPrepClassDetailsReadPort& detailsReadPort,
    QWidget* parent
    )
    : BasePage(parent),
      m_services(services),
      m_classInformationDataAccess(
          std::make_unique<SubPrepClassInformationDataAccess>(
              summaryReadPort,
              detailsReadPort
              )
          ),
      m_classInformationState(
          std::make_unique<Application::SubPrepClassInformationState>()
          )
{
    initialize();
}

SubPrepPage::~SubPrepPage() = default;

void SubPrepPage::refreshGeneratedContent()
{
    if (m_scheduleWidget)
    {
        m_scheduleWidget->refreshSchedule();
    }

    rebuildClassInformation();
}

void SubPrepPage::rebuildClassInformation()
{
    ++m_classInformationRebuildCount;
    m_classInformationSourceClassCount = 0;
    m_classInformationVisibleClassCount = 0;
    m_classInformationGroupCount = 0;
    m_classInformationNavigationRowCount = 0;
    m_classInformationClassInfoLookupCount = 0;
    m_classInformationTeacherLookupCount = 0;
    m_classInformationRosterLookupCount = 0;
    m_classInformationClassQueryCount = 0;
    m_classInformationClassResultRowCount = 0;
    m_classInformationClassInfoQueryCount = 0;
    m_classInformationClassInfoResultRowCount = 0;
    m_classInformationClassInfoScheduleRowCount = 0;
    m_classInformationTeacherQueryCount = 0;
    m_classInformationTeacherResultRowCount = 0;
    m_classInformationRosterQueryCount = 0;
    m_classInformationRosterResultRowCount = 0;
    m_classInformationRosterStudentResultCount = 0;

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("rebuild-start"),
        runtimeMetricsDetail(runtimeMetrics())
        );

    if (
        !m_services
        || !m_services->hasOpenDatabase()
        || !m_classInformationDataAccess
        || !m_classInformationState
        || !m_scheduleWidget
        || !m_classInformationModel
        || !m_classInformationGradeTabs
        || !m_classInformationListView
        )
    {
        clearClassInformation();
        return;
    }

    const ScheduleDisplayState displayState =
        m_scheduleWidget->displayState();
    const Application::SubPrepScheduleScopeRequest request =
        scheduleScopeRequest(
            m_scheduleWidget->scheduleModel(),
            displayState.displayMode
            );

    m_classInformationSourceClassCount =
        static_cast<int>(request.visibleClassIds.size());

    auto summaryResult =
        m_classInformationDataAccess->loadSummaries(request);
    if (!summaryResult)
    {
        DialogServices::showWarning(
            this,
            tr("Load Class Information"),
            tr("Class summaries could not be loaded."),
            operationErrorMessage(summaryResult.error())
            );
        StartupProfiler::recordSubPrepClassInformationLifecycle(
            QStringLiteral("summary-read-failed"),
            runtimeMetricsDetail(runtimeMetrics())
            );
        return;
    }

    const auto refreshedState =
        m_classInformationState->refreshScope(summaryResult);
    if (!refreshedState)
    {
        DialogServices::showWarning(
            this,
            tr("Load Class Information"),
            tr("Class summaries could not be applied."),
            operationErrorMessage(refreshedState.error())
            );
        StartupProfiler::recordSubPrepClassInformationLifecycle(
            QStringLiteral("summary-state-failed"),
            runtimeMetricsDetail(runtimeMetrics())
            );
        return;
    }

    m_updatingClassInformation = true;
    *m_classInformationState = refreshedState.value();
    m_classInformationModel->setProjection(
        std::move(summaryResult.value())
        );

    m_classInformationGrades =
        m_classInformationModel->grades();
    {
        const QSignalBlocker gradeTabsBlocker(
            m_classInformationGradeTabs
            );
        m_classInformationGradeTabs->clear();
        for (const QString& grade : m_classInformationGrades)
        {
            m_classInformationGradeTabs->addTab(grade);
        }
    }

    const auto& summaries =
        m_classInformationModel->projection().classes();
    m_classInformationNavigationRowCount =
        static_cast<int>(summaries.size());
    m_classInformationVisibleClassCount =
        static_cast<int>(summaries.size());
    m_classInformationGroupCount =
        m_classInformationGrades.size();
    if (!summaries.empty())
    {
        m_classInformationClassInfoQueryCount =
            request.visibleClassIds.empty()
                || request.selectedDays.empty()
                ? 0
                : 1;
        m_classInformationClassInfoResultRowCount =
            static_cast<int>(summaries.size());
        m_classInformationTeacherResultRowCount =
            static_cast<int>(
                m_classInformationModel->projection()
                    .teacherIndex()
                    .summaries()
                    .size()
                );
        m_classInformationRosterQueryCount =
            m_classInformationClassInfoQueryCount;
        m_classInformationRosterResultRowCount =
            static_cast<int>(summaries.size());
        for (const auto& summary : summaries)
        {
            m_classInformationRosterStudentResultCount +=
                static_cast<int>(summary.studentCount);
        }
    }

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        summaries.empty()
            ? QStringLiteral("data-empty")
            : QStringLiteral("data-loaded"),
        runtimeMetricsDetail(runtimeMetrics())
        );

    const bool hasSummaries = !summaries.empty();
    std::optional<Domain::ClassId> selectedClassId =
        m_classInformationState->selectedClassId();
    if (!selectedClassId.has_value() && hasSummaries)
    {
        selectedClassId =
            m_classInformationModel->classIdAt(0);
    }

    m_classInformationListView->selectionModel()->clear();
    if (selectedClassId.has_value())
    {
        const auto selectedSummary =
            m_classInformationModel->projection().findClass(
                *selectedClassId
                );
        if (selectedSummary.has_value())
        {
            const QString selectedGrade =
                fromUtf8(selectedSummary->grade);
            const int gradeIndex =
                m_classInformationGrades.indexOf(selectedGrade);
            if (gradeIndex >= 0)
            {
                const QSignalBlocker gradeTabsBlocker(
                    m_classInformationGradeTabs
                    );
                m_classInformationGradeTabs->setCurrentIndex(gradeIndex);
            }
            m_classInformationModel->setCurrentGrade(selectedGrade);
            const int row =
                m_classInformationModel->rowForClassId(*selectedClassId);
            if (row >= 0)
            {
                const QModelIndex index =
                    m_classInformationModel->index(row, 0);
                m_classInformationListView->selectionModel()->setCurrentIndex(
                    index,
                    QItemSelectionModel::ClearAndSelect
                        | QItemSelectionModel::Rows
                    );
            }
        }
    }

    m_updatingClassInformation = false;
    if (selectedClassId.has_value())
    {
        showSelectedClassInformation(*selectedClassId);
    }
    else
    {
        m_selectedClassId = -1;
        *m_classInformationState =
            m_classInformationState->clear();
        renderSelectedClassInformation();
    }
    updateClassInformationEmptyState();

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("view-ready"),
        runtimeMetricsDetail(runtimeMetrics())
        );
}

void SubPrepPage::handleClassInformationGradeChanged(int index)
{
    if (
        m_updatingClassInformation
        || !m_classInformationGradeTabs
        || !m_classInformationModel
        || !m_classInformationListView
        || index < 0
        || index >= m_classInformationGradeTabs->count()
        )
    {
        return;
    }

    const QString grade =
        m_classInformationGradeTabs->tabText(index);
    m_updatingClassInformation = true;
    m_classInformationModel->setCurrentGrade(grade);

    int selectedRow = -1;
    if (m_classInformationState->selectedClassId().has_value())
    {
        const auto currentClass =
            m_classInformationModel->projection().findClass(
                *m_classInformationState->selectedClassId()
                );
        if (
            currentClass.has_value()
            && fromUtf8(currentClass->grade) == grade
            )
        {
            selectedRow =
                m_classInformationModel->rowForClassId(
                    currentClass->id
                    );
        }
    }
    if (selectedRow < 0 && m_classInformationModel->rowCount() > 0)
    {
        selectedRow = 0;
    }

    m_classInformationListView->selectionModel()->clear();
    std::optional<Domain::ClassId> selectedClassId;
    if (selectedRow >= 0)
    {
        const QModelIndex index =
            m_classInformationModel->index(selectedRow, 0);
        m_classInformationListView->selectionModel()->setCurrentIndex(
            index,
            QItemSelectionModel::ClearAndSelect
                | QItemSelectionModel::Rows
            );
        selectedClassId =
            m_classInformationModel->classIdAt(selectedRow);
    }
    m_updatingClassInformation = false;

    if (selectedClassId.has_value())
    {
        showSelectedClassInformation(*selectedClassId);
    }
    else
    {
        m_selectedClassId = -1;
        *m_classInformationState =
            m_classInformationState->clear();
        renderSelectedClassInformation();
    }
    updateClassInformationEmptyState();
}

void SubPrepPage::handleClassInformationSelectionChanged()
{
    if (
        m_updatingClassInformation
        || !m_classInformationListView
        || !m_classInformationModel
        )
    {
        return;
    }

    const QModelIndex currentIndex =
        m_classInformationListView->currentIndex();
    const auto classId =
        currentIndex.isValid()
        ? m_classInformationModel->classIdAt(currentIndex.row())
        : std::nullopt;
    if (classId.has_value())
    {
        showSelectedClassInformation(*classId);
    }
}

void SubPrepPage::showSelectedClassInformation(
    const Domain::ClassId& classId
    )
{
    if (
        !m_classInformationDataAccess
        || !m_classInformationState
        || !m_classInformationModel
        )
    {
        return;
    }

    const auto selectedState =
        m_classInformationState->selectVisibleClass(
            classId,
            m_classInformationModel->projection()
            );
    if (!selectedState)
    {
        return;
    }
    *m_classInformationState = selectedState.value();
    m_selectedClassId = legacyClassId(classId);
    renderSelectedClassInformation();
    updateClassInformationEmptyState();

    const auto existingDetails =
        m_classInformationState->details();
    if (
        existingDetails.has_value()
        && existingDetails->classId == classId
        )
    {
        return;
    }

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("details-read-start"),
        runtimeMetricsDetail(runtimeMetrics())
        );
    auto detailsResult =
        m_classInformationDataAccess->loadDetails(classId);
    if (!detailsResult)
    {
        DialogServices::showWarning(
            this,
            tr("Load Class Information"),
            tr("Selected class information could not be loaded."),
            operationErrorMessage(detailsResult.error())
            );
        renderSelectedClassInformation();
        StartupProfiler::recordSubPrepClassInformationLifecycle(
            QStringLiteral("details-read-failed"),
            runtimeMetricsDetail(runtimeMetrics())
            );
        return;
    }

    const auto appliedDetails =
        m_classInformationState->applyDetails(
            std::move(detailsResult.value())
            );
    if (!appliedDetails)
    {
        DialogServices::showWarning(
            this,
            tr("Load Class Information"),
            tr("Selected class information could not be applied."),
            operationErrorMessage(appliedDetails.error())
            );
        StartupProfiler::recordSubPrepClassInformationLifecycle(
            QStringLiteral("details-state-failed"),
            runtimeMetricsDetail(runtimeMetrics())
            );
        return;
    }

    *m_classInformationState = appliedDetails.value();
    renderSelectedClassInformation();
    updateClassInformationEmptyState();
    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("details-ready"),
        runtimeMetricsDetail(runtimeMetrics())
        );
}

void SubPrepPage::renderSelectedClassInformation()
{
    if (
        !m_classInformationModel
        || !m_classInformationDetailsCard
        || !m_classInformationDetails
        || !m_classInformationLevelValue
        || !m_classInformationTimeValue
        || !m_classInformationStudentCountValue
        || !m_classInformationRoomValue
        || !m_classInformationWifiNameValue
        || !m_classInformationWifiPasswordValue
        || !m_classInformationZoomIdValue
        || !m_classInformationZoomPasswordValue
        || !m_classInformationInternetValue
        || !m_classInformationProjectionValue
        || !m_classInformationClassNotes
        || !m_classInformationTeacherNotes
        )
    {
        return;
    }

    const auto& selectedId =
        m_classInformationState->selectedClassId();
    if (!selectedId.has_value())
    {
        m_selectedClassId = -1;
        m_classInformationDetailsCard->setTitle(QString());
        m_classInformationDetailsCard->setProperty("teacherId", -1);
        m_classInformationDetails->setProperty("classId", -1);
        m_classInformationDetails->setProperty("teacherId", -1);
        m_classInformationLevelValue->setText(
            tr("Level: %1").arg(valueOrNa(QString()))
            );
        m_classInformationTimeValue->setText(
            tr("Time: %1").arg(valueOrNa(QString()))
            );
        m_classInformationStudentCountValue->setText(
            tr("# of Students: %1").arg(valueOrNa(QString()))
            );
        m_classInformationRoomValue->setText(
            tr("Room: %1").arg(valueOrNa(QString()))
            );
        m_classInformationWifiNameValue->setText(
            tr("WiFi Name: %1").arg(valueOrNa(QString()))
            );
        m_classInformationWifiPasswordValue->setText(
            tr("WiFi Password: %1").arg(valueOrNa(QString()))
            );
        m_classInformationZoomIdValue->setText(
            tr("Zoom ID: %1").arg(valueOrNa(QString()))
            );
        m_classInformationZoomPasswordValue->setText(
            tr("Zoom Password: %1").arg(valueOrNa(QString()))
            );
        m_classInformationInternetValue->setText(
            tr("Internet: %1").arg(valueOrNa(QString()))
            );
        m_classInformationProjectionValue->setText(
            tr("Projection: %1").arg(valueOrNa(QString()))
            );
        m_classInformationClassNotes->setPlainText(valueOrNa(QString()));
        m_classInformationTeacherNotes->setPlainText(valueOrNa(QString()));
        m_classInformationClassNotes->setProperty("classId", -1);
        m_classInformationTeacherNotes->setProperty("teacherId", -1);
        return;
    }

    const auto selectedSummary =
        m_classInformationModel->projection().findClass(*selectedId);
    if (!selectedSummary.has_value())
    {
        return;
    }
    const auto& summary = *selectedSummary;
    const auto& details =
        m_classInformationState->details();
    const QString className =
        fromUtf8(summary.displayLabel);
    QString teacherName;
    int teacherIdValue = -1;
    if (summary.teacherId.has_value())
    {
        teacherIdValue = legacyIntegerId(summary.teacherId->value());
        const auto teacher =
            m_classInformationModel->projection().findTeacher(
                *summary.teacherId
                );
        if (teacher.has_value())
        {
            teacherName = fromUtf8(teacher->displayName);
        }
    }
    if (details.has_value() && !details->teacherDisplayName.empty())
    {
        teacherName = fromUtf8(details->teacherDisplayName);
    }

    m_classInformationDetailsCard->setTitle(
        teacherName.isEmpty()
            ? className
            : QStringLiteral("%1: %2").arg(teacherName, className)
        );
    m_classInformationDetailsCard->setProperty(
        "teacherId",
        teacherIdValue
        );
    m_classInformationDetails->setProperty(
        "classId",
        legacyClassId(*selectedId)
        );
    m_classInformationDetails->setProperty(
        "teacherId",
        teacherIdValue
        );
    m_classInformationLevelValue->setText(
        tr("Level: %1").arg(
            valueOrNa(fromUtf8(summary.level))
            )
        );
    m_classInformationTimeValue->setText(
        tr("Time: %1").arg(
            valueOrNa(fromUtf8(summary.meetingText))
            )
        );
    m_classInformationStudentCountValue->setText(
        tr("# of Students: %1").arg(
            QString::number(
                static_cast<qulonglong>(summary.studentCount)
                )
            )
        );

    const QString room = details.has_value()
        ? fromUtf8(details->teacherFacilities.room)
        : QString();
    const QString wifiName = details.has_value()
        ? fromUtf8(details->teacherFacilities.wifiName)
        : QString();
    const QString wifiPassword = details.has_value()
        ? fromUtf8(details->teacherFacilities.wifiPassword)
        : QString();
    const QString zoomId = details.has_value()
        ? fromUtf8(details->teacherFacilities.zoomId)
        : QString();
    const QString zoomPassword = details.has_value()
        ? fromUtf8(details->teacherFacilities.zoomPassword)
        : QString();
    const QString internet = details.has_value()
        ? fromUtf8(details->teacherFacilities.internetType)
        : QString();
    const QString projection = details.has_value()
        ? fromUtf8(details->teacherFacilities.projectionType)
        : QString();

    m_classInformationRoomValue->setText(
        tr("Room: %1").arg(valueOrNa(room))
        );
    m_classInformationWifiNameValue->setText(
        tr("WiFi Name: %1").arg(valueOrNa(wifiName))
        );
    m_classInformationWifiPasswordValue->setText(
        tr("WiFi Password: %1").arg(valueOrNa(wifiPassword))
        );
    m_classInformationZoomIdValue->setText(
        tr("Zoom ID: %1").arg(valueOrNa(zoomId))
        );
    m_classInformationZoomPasswordValue->setText(
        tr("Zoom Password: %1").arg(valueOrNa(zoomPassword))
        );
    m_classInformationInternetValue->setText(
        tr("Internet: %1").arg(valueOrNa(internet))
        );
    m_classInformationProjectionValue->setText(
        tr("Projection: %1").arg(valueOrNa(projection))
        );

    m_classInformationClassNotes->setPlainText(
        valueOrNa(
            details.has_value()
                ? fromUtf8(details->classNotes)
                : QString()
            )
        );
    m_classInformationTeacherNotes->setPlainText(
        valueOrNa(
            details.has_value()
                ? fromUtf8(details->teacherNotes)
                : QString()
            )
        );
    m_classInformationClassNotes->setProperty(
        "classId",
        legacyClassId(*selectedId)
        );
    m_classInformationTeacherNotes->setProperty(
        "teacherId",
        teacherIdValue
        );
}

void SubPrepPage::updateClassInformationEmptyState()
{
    if (
        !m_classInformationModel
        || !m_classInformationGradeTabs
        || !m_classInformationListView
        || !m_classInformationEmptyLabel
        || !m_classInformationDetailsCard
        )
    {
        return;
    }

    const bool hasSummaries =
        !m_classInformationModel->projection().empty();
    const bool hasSelection =
        m_classInformationState
        && m_classInformationState->selectedClassId().has_value();

    m_classInformationGradeTabs->setVisible(hasSummaries);
    m_classInformationListView->setVisible(hasSummaries);
    m_classInformationEmptyLabel->setVisible(!hasSummaries);
    m_classInformationDetailsCard->setVisible(
        hasSummaries && hasSelection
        );
}

int SubPrepPage::currentClassInformationId() const
{
    return m_selectedClassId;
}

bool SubPrepPage::selectClassForStartupDiagnostics(int classId)
{
    if (
        classId <= 0
        || !m_classInformationModel
        || !m_classInformationGradeTabs
        || !m_classInformationListView
        || !m_classInformationState
        )
    {
        return false;
    }

    const auto typedClassId =
        Domain::ClassId::fromString(std::to_string(classId));
    if (
        !typedClassId.has_value()
        || !m_classInformationModel->projection()
                .findClass(*typedClassId)
                .has_value()
        )
    {
        return false;
    }

    const auto summary =
        m_classInformationModel->projection().findClass(*typedClassId);
    if (!summary.has_value())
    {
        return false;
    }

    const QString grade = fromUtf8(summary->grade);
    const int gradeIndex =
        m_classInformationGrades.indexOf(grade);
    if (gradeIndex < 0)
    {
        return false;
    }

    m_updatingClassInformation = true;
    {
        const QSignalBlocker gradeTabsBlocker(
            m_classInformationGradeTabs
            );
        m_classInformationGradeTabs->setCurrentIndex(gradeIndex);
    }
    m_classInformationModel->setCurrentGrade(grade);
    const int row =
        m_classInformationModel->rowForClassId(*typedClassId);
    if (row < 0)
    {
        m_updatingClassInformation = false;
        return false;
    }

    const QModelIndex index =
        m_classInformationModel->index(row, 0);
    m_classInformationListView->selectionModel()->setCurrentIndex(
        index,
        QItemSelectionModel::ClearAndSelect
            | QItemSelectionModel::Rows
        );
    m_updatingClassInformation = false;
    showSelectedClassInformation(*typedClassId);
    return m_selectedClassId == classId;
}

SubPrepPageRuntimeMetrics SubPrepPage::runtimeMetrics() const
{
    SubPrepPageRuntimeMetrics metrics;
    metrics.classInformationNavigationRowCount =
        m_classInformationNavigationRowCount;
    metrics.classInformationSourceClassCount =
        m_classInformationSourceClassCount;
    metrics.classInformationVisibleClassCount =
        m_classInformationVisibleClassCount;
    metrics.classInformationGroupCount =
        m_classInformationGroupCount;
    metrics.classInformationClassInfoLookupCount =
        m_classInformationClassInfoLookupCount;
    metrics.classInformationTeacherLookupCount =
        m_classInformationTeacherLookupCount;
    metrics.classInformationRosterLookupCount =
        m_classInformationRosterLookupCount;
    metrics.classInformationClassQueryCount =
        m_classInformationClassQueryCount;
    metrics.classInformationClassResultRowCount =
        m_classInformationClassResultRowCount;
    metrics.classInformationClassInfoQueryCount =
        m_classInformationClassInfoQueryCount;
    metrics.classInformationClassInfoResultRowCount =
        m_classInformationClassInfoResultRowCount;
    metrics.classInformationClassInfoScheduleRowCount =
        m_classInformationClassInfoScheduleRowCount;
    metrics.classInformationTeacherQueryCount =
        m_classInformationTeacherQueryCount;
    metrics.classInformationTeacherResultRowCount =
        m_classInformationTeacherResultRowCount;
    metrics.classInformationRosterQueryCount =
        m_classInformationRosterQueryCount;
    metrics.classInformationRosterResultRowCount =
        m_classInformationRosterResultRowCount;
    metrics.classInformationRosterStudentResultCount =
        m_classInformationRosterStudentResultCount;
    metrics.classInformationRebuildCount =
        m_classInformationRebuildCount;
    metrics.selectedClassId =
        m_classInformationState
            && m_classInformationState->selectedClassId().has_value()
            ? legacyClassId(
                  *m_classInformationState->selectedClassId()
                  )
            : -1;

    if (m_classInformationContent)
    {
        metrics.classInformationWidgetCount =
            m_classInformationContent
                ->findChildren<QWidget*>()
                .size();
        metrics.classInformationTextEditCount =
            m_classInformationContent
                ->findChildren<QTextEdit*>()
                .size();
    }

    return metrics;
}

QList<SubPrepClassInformation::TeacherGroup>
SubPrepPage::buildClassInformation()
{
    if (!m_scheduleWidget)
    {
        return {};
    }

    return buildClassInformation(
        m_scheduleWidget->scheduleModel()
        );
}

QList<SubPrepClassInformation::TeacherGroup>
SubPrepPage::buildClassInformation(
    const ScheduleViewModel& schedule
    )
{
    auto* classService = openClassService(m_services);
    auto* teacherService = openTeacherService(m_services);
    auto* rosterService = openRosterService(m_services);

    if (
        !classService
        || !teacherService
        || !rosterService
        || !m_scheduleWidget
        )
    {
        return {};
    }

    QList<SubPrepClassInformation::SourceClass> sources;
    ++m_classInformationClassQueryCount;
    const Result<QList<Classroom>> classes = classService->classes();
    if (!classes)
    {
        DialogServices::showWarning(
            const_cast<SubPrepPage*>(this),
            tr("Load Class Information"),
            tr("Classes could not be loaded."),
            classes.error()
            );
        return {};
    }

    m_classInformationSourceClassCount = classes->size();
    m_classInformationClassResultRowCount = classes->size();

    for (const Classroom& classroom : *classes)
    {
        SubPrepClassInformation::SourceClass source;
        source.classroom = classroom;
        ++m_classInformationClassInfoLookupCount;
        ++m_classInformationClassInfoQueryCount;
        const Result<ClassInfo> classInfo =
            classService->classInfo(classroom.id);
        if (classInfo)
        {
            ++m_classInformationClassInfoResultRowCount;
            source.info = *classInfo;
            m_classInformationClassInfoScheduleRowCount +=
                classInfo->classTimes.size()
                + classInfo->intensiveTimes.size();
        }
        ++m_classInformationRosterLookupCount;
        ++m_classInformationRosterQueryCount;
        const Result<int> studentCount =
            rosterService->studentCount(classroom.id);
        if (studentCount)
        {
            ++m_classInformationRosterResultRowCount;
            source.studentCount = qMax(0, *studentCount);
            m_classInformationRosterStudentResultCount +=
                source.studentCount;
        }

        if (source.info.teacherId > 0)
        {
            ++m_classInformationTeacherLookupCount;
            ++m_classInformationTeacherQueryCount;
            const Result<Teacher> teacher =
                teacherService->teacher(source.info.teacherId);
            if (teacher)
            {
                ++m_classInformationTeacherResultRowCount;
                source.teacher = *teacher;
            }
        }

        sources.append(source);
    }

    const ScheduleDisplayState state =
        m_scheduleWidget->displayState();

    SubPrepClassInformation::BuildOptions options;
    options.visibleClassIds =
        visibleClassIds(schedule);
    options.visibleDays = schedule.days;
    options.useIntensive =
        state.displayMode
            == ScheduleDisplayMode::Intensive;

    return SubPrepClassInformation::build(
        sources,
        options
        );
}

bool SubPrepPage::restoreGradingDefaultIfNeeded()
{
    if (
        !m_gradingInstructionsEdit
        || !m_gradingInstructionsEdit
                ->toPlainText()
                .trimmed()
                .isEmpty()
        )
    {
        return false;
    }

    const QSignalBlocker blocker(m_gradingInstructionsEdit);
    m_gradingInstructionsEdit->setPlainText(
        defaultGradingInstructions()
        );

    return true;
}

QString SubPrepPage::defaultGradingInstructions() const
{
    return tr(
        "Scoring: 0 / 20 / 40 / 60 / 80 / 100\n"
        "Comments: Please leave a comment about what the student did well "
        "and what they need to work on."
        );
}

QString SubPrepPage::defaultSpecialInstructions() const
{
    return tr("N/A");
}

QLabel* SubPrepPage::createTopLevelHeading(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(text, parent);

    label->setObjectName("sectionTitle");
    label->setAlignment(Qt::AlignCenter);
    label->setFont(
        FontManager::getUiFont(
            UiConstants::Pages::SectionTitleFontSize,
            QFont::DemiBold
            )
        );

    return label;
}

QLabel* SubPrepPage::createFieldLabel(
    const QString& text,
    QWidget* parent
    ) const
{
    auto* label =
        new QLabel(text, parent);

    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    return label;
}

QTextEdit* SubPrepPage::createTextEdit(
    int minimumLines,
    bool readOnly,
    QWidget* parent
    ) const
{
    auto* edit =
        new QTextEdit(parent);

    edit->setReadOnly(readOnly);
    edit->setMinimumHeight(
        textEditHeightForLines(
            edit,
            minimumLines
            )
        );
    edit->setVerticalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    edit->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    return edit;
}

void SubPrepPage::clearClassInformation()
{
    m_updatingClassInformation = true;
    if (m_classInformationModel)
    {
        m_classInformationModel->setProjection(
            Application::ClassSummaryProjection{}
            );
    }
    if (m_classInformationGradeTabs)
    {
        const QSignalBlocker gradeTabsBlocker(
            m_classInformationGradeTabs
            );
        m_classInformationGradeTabs->clear();
    }
    if (
        m_classInformationListView
        && m_classInformationListView->selectionModel()
        )
    {
        m_classInformationListView->selectionModel()->clear();
    }
    if (m_classInformationState)
    {
        *m_classInformationState =
            m_classInformationState->clear();
    }
    m_classInformationGrades.clear();
    m_classInformationSourceClassCount = 0;
    m_classInformationVisibleClassCount = 0;
    m_classInformationGroupCount = 0;
    m_classInformationNavigationRowCount = 0;
    m_classInformationClassInfoLookupCount = 0;
    m_classInformationTeacherLookupCount = 0;
    m_classInformationRosterLookupCount = 0;
    m_classInformationClassQueryCount = 0;
    m_classInformationClassResultRowCount = 0;
    m_classInformationClassInfoQueryCount = 0;
    m_classInformationClassInfoResultRowCount = 0;
    m_classInformationClassInfoScheduleRowCount = 0;
    m_classInformationTeacherQueryCount = 0;
    m_classInformationTeacherResultRowCount = 0;
    m_classInformationRosterQueryCount = 0;
    m_classInformationRosterResultRowCount = 0;
    m_classInformationRosterStudentResultCount = 0;
    m_selectedClassId = -1;
    m_updatingClassInformation = false;

    renderSelectedClassInformation();
    updateClassInformationEmptyState();

    StartupProfiler::recordSubPrepClassInformationLifecycle(
        QStringLiteral("view-clear-requested"),
        runtimeMetricsDetail(runtimeMetrics())
        );
}

void SubPrepPage::releaseFeatureResources()
{
    clearClassInformation();
    markStale();
    BasePage::releaseFeatureResources();
}

void SubPrepPage::clearDirty()
{
    m_dirty = false;
}
