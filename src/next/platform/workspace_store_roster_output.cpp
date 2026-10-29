#include "next/platform/workspace_store_roster_output.h"

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/testing_class_repository.h"

#include <QSet>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTime>
#include <algorithm>
#include <charconv>

namespace ClassMngr::Next::Platform
{
namespace
{
using namespace RosterTemplateRenderer;
using namespace Output;
const QStringList weekdays{QStringLiteral("Monday"), QStringLiteral("Tuesday"),
    QStringLiteral("Wednesday"), QStringLiteral("Thursday"), QStringLiteral("Friday")};
int idValue(const Domain::ClassId& id)
{
    int value = 0;
    const auto& text = id.value();
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
        && value > 0 && std::to_string(value) == text ? value : -1;
}
QString errorText(const RepositoryError& error) { return toLegacyDiagnostic(error); }
DocumentOutputResult failure(const QString& error) { return {DocumentOutputStatus::Failed, error}; }
QTime startTime(const QString& text)
{
    const auto first = QTime::fromString(text.trimmed(), QStringLiteral("h:mm AP"));
    return first.isValid() ? first : QTime::fromString(text.trimmed(), QStringLiteral("H:mm"));
}
struct Section { int id; QString name; QString label; RosterPrintScheduleTime time; };
struct PagePlan { QString key; QList<Section> sections; };

std::expected<QList<RosterOutputClass>, QString> classesFor(
    WorkspaceStore& store, const Domain::ClassId& currentId, bool testing)
{
    const int current = idValue(currentId);
    if (current <= 0) return std::unexpected(QObject::tr("Roster data is not available."));
    const auto isTesting = store.testingClassRepository()->isTestingClass(current);
    if (!isTesting) return std::unexpected(errorText(isTesting.error()));
    if (*isTesting != testing) return std::unexpected(QObject::tr("Roster data is not available."));
    QList<RosterOutputClass> result;
    if (testing)
    {
        const auto record = store.classRepository()->getClassById(current);
        if (!record) return std::unexpected(errorText(record.error()));
        result.append({currentId, record->name});
    }
    else
    {
        const auto records = store.classRepository()->getClasses();
        if (!records) return std::unexpected(errorText(records.error()));
        for (const auto& record : *records)
            result.append({*Domain::ClassId::fromString(std::to_string(record.id)), record.name});
        if (std::none_of(result.cbegin(), result.cend(), [&](const auto& item) { return item.classId == currentId; }))
            return std::unexpected(QObject::tr("Roster data is not available."));
    }
    return result;
}
}

Output::RosterOutputChoicesResult rosterOutputChoicesAtPath(
    const QString& path, const Domain::ClassId& current, bool testing)
{
    try
    {
        WorkspaceStore store;
        const auto opened = store.openExisting(path);
        if (!opened) return std::unexpected(errorText(opened.error()));
        auto classes = classesFor(store, current, testing);
        if (!classes) return std::unexpected(classes.error());
        RosterOutputChoices result{*classes, {}};
        // The union is intentionally unbounded and preserves class/column order.
        for (auto& item : result.classes)
        {
            const auto columns = store.rosterRepository()->loadRosterColumnNamesForClassesWithParentProvenance({idValue(item.classId)});
            if (!columns) return std::unexpected(errorText(columns.error()));
            if (columns->size() != 1 || columns->first().parentMissing) return std::unexpected(QObject::tr("Roster data is not available."));
            RosterClassData names;
            names.roster.columns = columns->first().columns;
            const auto custom = availablePerClassExtraInfoColumns({names});
            item.extraColumns = custom;
            for (const auto& name : custom)
                if (!result.extraColumns.contains(name, Qt::CaseInsensitive)) result.extraColumns.append(name);
        }
        return result;
    }
    catch (...) { return std::unexpected(QObject::tr("Roster data is not available.")); }
}

DocumentOutputResult saveRosterOutputPdfAtPath(
    const QString& path, const Output::RosterOutputRequest& request,
    const QString& outputPath, const RosterTemplateRenderer::Fonts& fonts,
    Output::RosterOutputMetrics* metrics)
{
    if (metrics) *metrics = {};
    try
    {
        if (outputPath.trimmed().isEmpty()) return failure(QObject::tr("No roster print file path was provided."));
        QTemporaryDir temporary;
        if (!temporary.isValid()) return failure(QObject::tr("Unable to create a temporary print folder."));
        const QString stagedPath = temporary.filePath(QStringLiteral("Rosters.pdf"));
        if (request.scope != RosterOutputScope::AllClasses && request.scope != RosterOutputScope::CurrentClass
            && request.scope != RosterOutputScope::SelectedClasses)
            return failure(QObject::tr("No classes were selected for printing."));
        if (request.testingClass && (request.scope != RosterOutputScope::CurrentClass
            || request.templateId != TemplateId::PerClassWithExtraInfo))
            return failure(QObject::tr("Testing classes support only the current class and Per Class with Extra Info."));
        if (request.templateId != TemplateId::ByDay && request.templateId != TemplateId::Daily
            && request.templateId != TemplateId::PerClassWithExtraInfo)
            return failure(QObject::tr("Roster data is not available."));
        if (request.templateId == TemplateId::PerClassWithExtraInfo
            && request.extraColumns.size() > perClassExtraInfoMaxExtraColumns(request.orientation))
            return failure(QObject::tr("Too many extra columns for the selected orientation."));
        WorkspaceStore store;
        const auto opened = store.openExisting(path);
        if (!opened) return failure(errorText(opened.error()));
        auto classes = classesFor(store, request.currentClassId, request.testingClass);
        if (!classes) return failure(classes.error());
        QList<RosterOutputClass> selected;
        if (request.scope == RosterOutputScope::AllClasses) selected = *classes;
        else
        {
            const auto ids = request.scope == RosterOutputScope::CurrentClass
                ? std::vector<Domain::ClassId>{request.currentClassId} : request.selectedClassIds;
            QSet<int> seen;
            for (const auto& id : ids)
            {
                auto found = std::find_if(classes->cbegin(), classes->cend(), [&](const auto& item) { return item.classId == id; });
                if (found == classes->cend()) return failure(QObject::tr("Roster data is not available."));
                if (!seen.contains(idValue(id))) { selected.append(*found); seen.insert(idValue(id)); }
            }
        }
        if (selected.isEmpty()) return failure(QObject::tr("No classes were selected for printing."));
        // Keep a read snapshot while planning and streaming pages. The only
        // operation-wide data are class identities and schedule descriptors.
        DocumentOutputResult rendered = failure(QObject::tr("Roster printing failed."));
        const auto transaction = store.withReadTransaction([&]() -> RepositoryStatus {
            QList<PagePlan> plans;
            if (request.templateId == TemplateId::PerClassWithExtraInfo)
            {
                for (const auto& item : selected)
                    plans.append({QStringLiteral("class|%1|%2").arg(plans.size()).arg(idValue(item.classId)),
                        {{idValue(item.classId), item.name, {}, {}}}});
            }
            else
            {
                QList<Section> sections;
                for (const auto& item : selected)
                {
                    const auto subtitle = store.classInfoRepository()->loadClassSubtitleRecordDetailed(idValue(item.classId), true);
                    if (!subtitle) { rendered = failure(errorText(subtitle.error())); return {}; }
                    if (subtitle->parentMissing) { rendered = failure(QObject::tr("Roster data is not available.")); return {}; }
                    const auto& info = subtitle->record;
                    QStringList parts;
                    if (!info.grade.trimmed().isEmpty()) parts.append(info.grade.trimmed());
                    if (!info.level.trimmed().isEmpty()) parts.append(info.level.trimmed());
                    const auto label = parts.isEmpty() ? item.name.trimmed() : parts.join(' ');
                    for (const auto& time : info.regularTimes)
                        if (weekdays.contains(time.day.trimmed())) sections.append({idValue(item.classId), item.name, label, {time.day.trimmed(), time.startTime, time.endTime}});
                }
                for (const auto& day : weekdays)
                {
                    QList<Section> daily;
                    for (const auto& section : sections) if (section.time.day == day) daily.append(section);
                    if (request.templateId == TemplateId::ByDay)
                    {
                        QSet<int> occupiedSlots;
                        QList<Section> matching;
                        for (const auto& section : daily)
                        {
                            const auto time = startTime(section.time.startTime);
                            const int hour = time.hour() > 12 ? time.hour() - 12 : time.hour();
                            if (!time.isValid() || hour < 4 || hour > 9) continue;
                            if (occupiedSlots.contains(hour)) { rendered = failure(QObject::tr("Multiple selected classes use the %1 %2 slot.").arg(day, section.time.startTime)); return {}; }
                            occupiedSlots.insert(hour); matching.append(section);
                        }
                        if (!matching.isEmpty()) plans.append({day, matching});
                    }
                    else
                    {
                        std::stable_sort(daily.begin(), daily.end(), [](const auto& left, const auto& right) {
                            const auto a = startTime(left.time.startTime), b = startTime(right.time.startTime);
                            if (a.isValid() != b.isValid()) return a.isValid();
                            if (a.isValid() && a != b) return a < b;
                            return left.label.localeAwareCompare(right.label) < 0;
                        });
                        for (qsizetype offset = 0; offset < daily.size(); offset += 6)
                            plans.append({offset == 0 ? day : QStringLiteral("%1|%2").arg(day).arg(offset / 6), daily.mid(offset, 6)});
                    }
                }
            }
            qsizetype pageIndex = 0;
            rendered = savePagesPdf(stagedPath, request.templateId,
                request.templateId == TemplateId::PerClassWithExtraInfo ? request.orientation : templateOrientation(request.templateId),
                [&]() -> PageResult {
                    if (pageIndex >= plans.size()) return std::optional<Page>{};
                    const auto& plan = plans.at(pageIndex++);
                    Page page{plan.key, {}};
                    QList<RosterClassData> pageClasses;
                    qsizetype pageRows = 0;
                    for (const auto& section : plan.sections)
                    {
                        RosterClassData data;
                        data.classroom = {section.id, section.name};
                        const auto info = store.classInfoRepository()->loadRosterPrintClassInfoRecordDetailed(section.id, request.templateId != TemplateId::Daily, true,
                            request.templateId == TemplateId::PerClassWithExtraInfo);
                        if (!info) return std::unexpected(errorText(info.error()));
                        if (info->parentMissing) return std::unexpected(QObject::tr("Roster data is not available."));
                        const auto& source = info->record;
                        data.classInfo = {source.classGrade, source.classLevel, source.teacherEnglishName, source.teacherKoreanName,
                            source.roomNumber, {}, {}, {}, {}, {}};
                        if (request.templateId != TemplateId::Daily) { data.classInfo.wifiName = source.wifiName; data.classInfo.wifiPassword = source.wifiPassword; }
                        { data.classInfo.zoomId = source.zoomId; data.classInfo.zoomPassword = source.zoomPassword; }
                        if (request.templateId == TemplateId::PerClassWithExtraInfo)
                            for (const auto& time : source.regularTimes) data.classInfo.regularSchedule.append({time.day, time.startTime, time.endTime});
                        else data.classInfo.regularSchedule.append(section.time);
                        QStringList columns{QStringLiteral("English"), QStringLiteral("Korean")};
                        if (request.templateId == TemplateId::PerClassWithExtraInfo) columns.append(request.extraColumns);
                        auto projection = store.rosterRepository()->loadTemplatePrintProjection(section.id, columns,
                            request.templateId == TemplateId::PerClassWithExtraInfo);
                        if (!projection) return std::unexpected(errorText(projection.error()));
                        if (metrics) metrics->scannedCells += projection->scannedCellCount;
                        pageRows += projection->rows.size();
                        data.roster = {std::move(projection->columns), std::move(projection->rows)};
                        pageClasses.append(std::move(data));
                    }
                    if (metrics) { metrics->peakProjectedClasses = std::max(metrics->peakProjectedClasses, pageClasses.size()); metrics->peakProjectedRows = std::max(metrics->peakProjectedRows, pageRows); }
                    QString error;
                    page.values = buildTemplateCellValues(request.templateId, pageClasses, request.extraColumns, request.orientation, &error);
                    if (!error.isEmpty()) return std::unexpected(error);
                    // Builders project one local page. Re-key it to the global
                    // order without retaining any previous page's values.
                    for (auto& value : page.values) value.day = page.key;
                    return std::optional<Page>(std::move(page));
                }, fonts, [&](qsizetype cells) {
                    if (metrics) { ++metrics->pagesReleased; metrics->peakPageCells = std::max(metrics->peakPageCells, cells); }
                });
            return {};
        });
        if (transaction) return failure(errorText(transaction->error));
        if (rendered.status != DocumentOutputStatus::Sent) return rendered;
        QFile source(stagedPath);
        QSaveFile destination(outputPath);
        if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
            return failure(QObject::tr("Unable to create the roster print file."));
        while (!source.atEnd())
        {
            const auto bytes = source.read(65536);
            if ((bytes.isEmpty() && source.error() != QFileDevice::NoError) || destination.write(bytes) != bytes.size())
                return failure(QObject::tr("The roster print file could not be completed."));
        }
        if (!destination.commit()) return failure(QObject::tr("The roster print file could not be completed."));
        return rendered;
    }
    catch (...) { return failure(QObject::tr("Roster data is not available.")); }
}
}
