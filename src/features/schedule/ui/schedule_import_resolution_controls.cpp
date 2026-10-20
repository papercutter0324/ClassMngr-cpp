#include "schedule_import_resolution_controls.h"

#include "schedule_import_review_presentation.h"
#include "schedule_import_resolution_view.h"

#include "core/utils/colorutils.h"
#include "domain/models/class_info.h"
#include "domain/rules/schedule_import_rules.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QObject>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <charconv>
#include <initializer_list>
#include <string>
#include <system_error>

namespace ScheduleImportResolutionControls
{
namespace
{
using namespace ScheduleImportResolutionView;
using namespace ScheduleImportReviewPresentation;
namespace ImportState = ClassMngr::Next::Application;
namespace ImportDomain = ClassMngr::Next::Domain;

template <typename Id>
int snapshotLegacyId(const Id& id)
{
    int value = -1;
    const std::string& text = id.value();
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value
        );
    return error == std::errc{} && end == text.data() + text.size()
        ? value
        : -1;
}

template <typename Id>
qint64 snapshotTeacherId(const Id& id)
{
    qint64 value = -1;
    const std::string& text = id.value();
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value
        );
    return error == std::errc{} && end == text.data() + text.size()
        ? value
        : -1;
}

QString snapshotText(const std::u16string& value)
{
    return QString::fromStdU16String(value);
}

QList<ClassTime> snapshotTimes(
    const std::vector<ImportState::ScheduleImportStateReadTime>& times
    )
{
    QList<ClassTime> result;
    result.reserve(static_cast<qsizetype>(times.size()));
    for (const auto& time : times)
    {
        result.append({
            snapshotText(time.day),
            snapshotText(time.startTime),
            snapshotText(time.endTime)
        });
    }
    return result;
}

ClassInfo classInfoForEligibility(
    const ImportState::ScheduleImportStateReadClassSnapshot& classroom
    )
{
    ClassInfo info;
    info.classId = snapshotLegacyId(classroom.id);
    // Eligibility depends on grade, level, and schedule rows only. The
    // legacy ClassInfo field cannot represent the typed teacher ID.
    info.teacherId = -1;
    info.classGrade = snapshotText(classroom.grade);
    info.classLevel = snapshotText(classroom.level);
    info.classColor = snapshotText(classroom.classColor);
    info.roomNumber = snapshotText(classroom.roomNumber);
    info.classTimes = snapshotTimes(classroom.normalTimes);
    info.intensiveTimes = snapshotTimes(classroom.intensiveTimes);
    return info;
}

QString translate(
    const char* source
    )
{
    return QCoreApplication::translate(
        "ScheduleImportReviewDialog",
        source
        );
}

QString classIdentityKey(
    const QString& grade,
    const QString& level
    )
{
    return grade.simplified().toCaseFolded()
        + QLatin1Char('\x1f')
        + level.simplified().toCaseFolded();
}

bool shouldRealizeTeacher(
    const BuildRequest& request,
    int row
    )
{
    return !request.virtualized
        || request.realizedTeacherRows.contains(row);
}

bool shouldRealizeClass(
    const BuildRequest& request,
    int row
    )
{
    return !request.virtualized
        || request.realizedClassRows.contains(row);
}

void connectStateChanged(
    QComboBox* combo,
    const BuildRequest& request
    )
{
    QObject::connect(
        combo,
        &QComboBox::currentIndexChanged,
        request.context,
        [callback = request.stateChanged](int)
        {
            if (callback)
            {
                callback();
            }
        }
        );
}

void connectColorRequested(
    QPushButton* button,
    int candidateIndex,
    const BuildRequest& request
    )
{
    QObject::connect(
        button,
        &QPushButton::clicked,
        request.context,
        [callback = request.colorRequested, candidateIndex]()
        {
            if (callback)
            {
                callback(candidateIndex);
            }
        }
        );
}

void finalizeVirtualizedCard(
    QWidget* card,
    std::initializer_list<QWidget*> realizedWidgets
    )
{
    if (!card)
    {
        return;
    }

    card->show();
    for (QWidget* widget : realizedWidgets)
    {
        if (widget)
        {
            widget->show();
        }
    }

    if (QLayout* layout = card->layout())
    {
        layout->invalidate();
        layout->activate();
    }
}
}

Result<BuildResult> build(
    const BuildRequest& request
    )
{
    BuildResult result = request.previousState
        ? *request.previousState
        : BuildResult{};
    if (
        !request.teacherContent
        || !request.classContent
        || (!request.virtualized && (!request.teacherLayout || !request.classLayout))
        || !request.stateSnapshot
        || !request.preview
        )
    {
        return std::unexpected(
            translate("Current schedule data could not be loaded.")
            );
    }

    QHash<qint64, const ImportState::ScheduleImportStateReadTeacherSnapshot*>
        teachersById;
    QHash<int, const ImportState::ScheduleImportStateReadClassSnapshot*>
        classesById;
    teachersById.reserve(
        static_cast<qsizetype>(request.stateSnapshot->teachers.size())
        );
    for (const auto& teacher : request.stateSnapshot->teachers)
    {
        teachersById.insert(snapshotTeacherId(teacher.id), &teacher);
    }
    classesById.reserve(
        static_cast<qsizetype>(request.stateSnapshot->classes.size())
        );
    for (const auto& classroom : request.stateSnapshot->classes)
    {
        classesById.insert(snapshotLegacyId(classroom.id), &classroom);
    }

    const auto teacherSnapshot = [&teachersById](const qint64 id)
        -> const ImportState::ScheduleImportStateReadTeacherSnapshot*
    {
        return teachersById.value(id, nullptr);
    };
    const auto classSnapshot = [&classesById](const int id)
        -> const ImportState::ScheduleImportStateReadClassSnapshot*
    {
        return classesById.value(id, nullptr);
    };

    const bool restorePrevious =
        request.previousState
        && result.teacherControls.size() == request.preview->teachers.size()
        && result.classControls.size() == request.preview->classes.size();
    if (!restorePrevious)
    {
        result = BuildResult{};
        result.teacherControls.reserve(request.preview->teachers.size());
        for (int row = 0; row < request.preview->teachers.size(); ++row)
        {
            const ScheduleImportTeacherPreview& preview =
                request.preview->teachers[row];
            TeacherControl control;
            control.rowIndex = row;
            control.teacherKey = preview.teacherKey;
            control.selectedRoom = preview.importedRooms.size() == 1
                ? preview.importedRooms.first()
                : QString();

            if (preview.matchingTeacherIds.isEmpty())
            {
                control.actionValue = static_cast<int>(
                    ScheduleImportTeacherAction::Create
                    );
            }
            else if (preview.matchingTeacherIds.size() == 1)
            {
                const qint64 teacherId =
                    preview.matchingTeacherIds.first();
                const auto* existing = teacherSnapshot(teacherId);
                const bool exactRoom = existing
                    && preview.importedRooms.size() == 1
                    && snapshotText(existing->roomNumber).trimmed()
                        == preview.importedRooms.first().trimmed();
                if (exactRoom)
                {
                    control.actionValue = static_cast<int>(
                        ScheduleImportTeacherAction::Reuse
                        );
                    control.targetTeacherId = teacherId;
                }
            }
            result.teacherControls.append(control);
        }

        QList<ScheduleImportClassPreview> orderedClasses =
            request.preview->classes;
        std::stable_sort(
            orderedClasses.begin(),
            orderedClasses.end(),
            [&request](
                const ScheduleImportClassPreview& left,
                const ScheduleImportClassPreview& right
                )
            {
                return importedClassLess(
                    request.preview->user.classes[left.candidateIndex],
                    request.preview->user.classes[right.candidateIndex]
                    );
            }
            );
        result.classControls.reserve(orderedClasses.size());
        for (int row = 0; row < orderedClasses.size(); ++row)
        {
            const ScheduleImportClassPreview& preview = orderedClasses[row];
            const ScheduleImportClassCandidate& candidate =
                request.preview->user.classes[preview.candidateIndex];
            ClassControl control;
            control.rowIndex = row;
            control.candidateIndex = preview.candidateIndex;
            control.teacherKey = candidate.teacherKey;
            control.color = candidate.importedColors.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : candidate.importedColors.first().toUpper();
            if (!QColor(control.color).isValid())
            {
                control.color = QStringLiteral("#FFFFFF");
            }
            control.skipTargetId = preview.suggestedClassId;
            if (preview.suggestedClassId > 0)
            {
                control.actionValue = static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    );
                control.targetClassId = preview.suggestedClassId;
            }
            else if (preview.matchingClassIds.isEmpty())
            {
                control.actionValue = static_cast<int>(
                    ScheduleImportClassAction::CreateNew
                    );
            }
            else if (preview.exactMatch)
            {
                control.actionValue = static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    );
                control.targetClassId = preview.matchingClassIds.first();
            }
            result.classControls.append(control);
        }
    }

    if (result.classIdsByIdentity.isEmpty())
    {
        for (const auto& classroom : request.stateSnapshot->classes)
        {
            result.classIdsByIdentity[
                classIdentityKey(
                    snapshotText(classroom.grade),
                    snapshotText(classroom.level)
                    )
                ].append(snapshotLegacyId(classroom.id));
        }
    }

    // Validate all references before creating any controls. A malformed or
    // incomplete snapshot must not leave a partly populated review page.
    for (const auto& classroom : request.stateSnapshot->classes)
    {
        const qint64 teacherId = snapshotTeacherId(classroom.teacherId);
        if (teacherId > 0 && !teacherSnapshot(teacherId))
        {
            return std::unexpected(
                translate("Current schedule data could not be loaded.")
                );
        }
    }
    for (const ScheduleImportTeacherPreview& preview :
         request.preview->teachers)
    {
        for (const qint64 teacherId : preview.matchingTeacherIds)
        {
            if (!teacherSnapshot(teacherId))
            {
                return std::unexpected(
                    translate("A matching Korean teacher is no longer available.")
                    );
            }
        }
    }
    for (const ScheduleImportClassPreview& preview :
         request.preview->classes)
    {
        for (const int classId : preview.matchingClassIds)
        {
            if (!classSnapshot(classId))
            {
                return std::unexpected(
                    translate("A matching class is no longer available.")
                    );
            }
        }
        if (
            preview.suggestedClassId > 0
            && !classSnapshot(preview.suggestedClassId)
            )
        {
            return std::unexpected(
                translate("A suggested class is no longer available.")
                );
        }
    }

    for (int row = 0; row < result.teacherControls.size(); ++row)
    {
        if (!shouldRealizeTeacher(request, row))
        {
            continue;
        }
        const ScheduleImportTeacherPreview& preview =
            request.preview->teachers[row];
        TeacherControl control = result.teacherControls[row];
        control.action =
            new NoWheelComboBox(request.teacherContent);
        control.room =
            new NoWheelComboBox(request.teacherContent);
        configureCompactActionCombo(control.action);
        configureCompactActionCombo(control.room);
        control.action->setObjectName(
            QStringLiteral("scheduleImportTeacherAction_%1")
                .arg(row)
            );
        control.room->setObjectName(
            QStringLiteral("scheduleImportTeacherRoom_%1")
                .arg(row)
            );
        if (preview.importedRooms.size() > 1)
        {
            control.room->addItem(
                translate("Choose a room..."),
                QString()
                );
        }
        for (const QString& room : preview.importedRooms)
        {
            control.room->addItem(room, room);
        }

        if (control.selectedRoom.isEmpty())
        {
            control.room->setCurrentIndex(0);
        }
        else
        {
            const int roomIndex = control.room->findData(control.selectedRoom);
            if (roomIndex >= 0)
            {
                control.room->setCurrentIndex(roomIndex);
            }
        }

        if (preview.matchingTeacherIds.size() == 1)
        {
            const qint64 teacherId =
                preview.matchingTeacherIds.first();
            const auto* existing = teacherSnapshot(teacherId);
            const bool exactRoom =
                preview.importedRooms.size() == 1
                && snapshotText(existing->roomNumber).trimmed()
                    == preview.importedRooms.first().trimmed();

            if (!exactRoom)
            {
                addResolutionItem(
                    control.action,
                    translate("Choose a resolution..."),
                    -1
                    );
            }
            addResolutionItem(
                control.action,
                translate("Keep existing: %1")
                    .arg(teacherLabel(*existing)),
                static_cast<int>(
                    ScheduleImportTeacherAction::Reuse
                    ),
                teacherId
                );
            if (!exactRoom)
            {
                addResolutionItem(
                    control.action,
                    translate("Update room globally (%1 affected classes)")
                        .arg(preview.affectedClassCount),
                    static_cast<int>(
                        ScheduleImportTeacherAction::UpdateRoom
                        ),
                    teacherId
                    );
                addResolutionItem(
                    control.action,
                    translate("Skip affected classes"),
                    static_cast<int>(
                        ScheduleImportTeacherAction::Skip
                        )
                    );
            }
        }
        else
        {
            if (!preview.matchingTeacherIds.isEmpty())
            {
                addResolutionItem(
                    control.action,
                    translate("Choose a resolution..."),
                    -1
                    );
            }
            for (const qint64 teacherId : preview.matchingTeacherIds)
            {
                const auto* existing = teacherSnapshot(teacherId);
                addResolutionItem(
                    control.action,
                    translate("Use existing: %1")
                        .arg(teacherLabel(*existing)),
                    static_cast<int>(
                        ScheduleImportTeacherAction::Reuse
                        ),
                    teacherId
                    );
                addResolutionItem(
                    control.action,
                    translate("Update existing room: %1")
                        .arg(teacherLabel(*existing)),
                    static_cast<int>(
                        ScheduleImportTeacherAction::UpdateRoom
                        ),
                    teacherId
                    );
            }
            addResolutionItem(
                control.action,
                translate("Create a new Korean teacher"),
                static_cast<int>(
                    ScheduleImportTeacherAction::Create
                    )
                );
            addResolutionItem(
                control.action,
                translate("Skip affected classes"),
                static_cast<int>(
                    ScheduleImportTeacherAction::Skip
                    )
                );
        }

        auto* teacherCard =
            createReconciliationCard(
                request.teacherContent,
                QStringLiteral("scheduleImportTeacherCard_%1")
                    .arg(row)
                );
        auto* teacherCardLayout =
            new QVBoxLayout(teacherCard);
        auto* spreadsheetTeacher =
            new QLabel(
                QStringLiteral("%1 (%2)")
                    .arg(
                        preview.teacherKr,
                        preview.importedRooms.join(
                            QStringLiteral(", ")
                            )
                        ),
                teacherCard
                );
        spreadsheetTeacher->setObjectName(
            QStringLiteral("scheduleImportTeacherSource_%1")
                .arg(row)
            );
        QFont teacherFont = spreadsheetTeacher->font();
        teacherFont.setBold(true);
        spreadsheetTeacher->setFont(teacherFont);
        spreadsheetTeacher->setWordWrap(true);
        spreadsheetTeacher->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );
        teacherCardLayout->addWidget(
            spreadsheetTeacher
            );
        addLabeledControlRow(
            teacherCardLayout,
            translate("Import Action"),
            control.action,
            teacherCard
            );
        addLabeledControlRow(
            teacherCardLayout,
            translate("Imported Room"),
            control.room,
            teacherCard
            );
        const int actionIndex = findActionIndex(
            control.action,
            control.actionValue,
            control.targetTeacherId
            );
        if (actionIndex >= 0)
        {
            control.action->setCurrentIndex(actionIndex);
        }
        else
        {
            control.actionValue = control.action->currentData(ActionRole).toInt();
            control.targetTeacherId =
                control.action->currentData(TargetRole).toLongLong();
        }
        control.card = teacherCard;
        connectStateChanged(control.action, request);
        connectStateChanged(control.room, request);
        if (request.teacherLayout)
        {
            request.teacherLayout->addWidget(teacherCard);
        }
        else if (teacherCard)
        {
            teacherCard->setGeometry(
                8,
                row * request.teacherRowExtent + 6,
                std::max(0, request.teacherContent->width() - 16),
                request.teacherRowExtent - 12
                );
            finalizeVirtualizedCard(
                teacherCard,
                {
                    spreadsheetTeacher,
                    control.action,
                    control.room
                }
                );
        }
        result.teacherControls[row] = control;
    }

    QList<ScheduleImportClassPreview> orderedClasses =
        request.preview->classes;
    std::stable_sort(
        orderedClasses.begin(),
        orderedClasses.end(),
        [&request](
            const ScheduleImportClassPreview& left,
            const ScheduleImportClassPreview& right
            )
        {
            return importedClassLess(
                request.preview->user.classes[left.candidateIndex],
                request.preview->user.classes[right.candidateIndex]
                );
        }
        );

    for (int row = 0; row < orderedClasses.size(); ++row)
    {
        if (!shouldRealizeClass(request, row))
        {
            continue;
        }
        const ScheduleImportClassPreview& preview = orderedClasses[row];
        const ScheduleImportClassCandidate& candidate =
            request.preview->user.classes[
                preview.candidateIndex
                ];
        ClassControl control = result.classControls[row];
        control.action =
            new NoWheelComboBox(request.classContent);
        configureCompactActionCombo(control.action);
        control.colorButton =
            new QPushButton(request.classContent);
        control.details =
            new QLabel(request.classContent);
        control.action->setObjectName(
            QStringLiteral("scheduleImportClassAction_%1")
                .arg(preview.candidateIndex)
            );
        control.details->setObjectName(
            QStringLiteral("scheduleImportClassDifferences_%1")
                .arg(preview.candidateIndex)
            );
        control.colorButton->setObjectName(
            QStringLiteral("scheduleImportClassColor_%1")
                .arg(preview.candidateIndex)
            );
        control.details->setWordWrap(true);
        control.details->setTextFormat(Qt::RichText);
        updateClassColorButton(&control);

        if (
            !preview.exactMatch
            && !preview.matchingClassIds.isEmpty()
            )
        {
            addResolutionItem(
                control.action,
                translate("Choose Update, Create, or Skip..."),
                -1
                );
        }

        QSet<int> addedTargets;
        for (int classId : preview.matchingClassIds)
        {
            addResolutionItem(
                control.action,
                (classId == preview.suggestedClassId
                    ? translate("Update suggested: %1")
                    : translate("Update existing: %1"))
                    .arg(
                        classLabel(
                            *classSnapshot(classId),
                            teacherSnapshot(snapshotTeacherId(
                                classSnapshot(classId)->teacherId
                                )),
                            request.kind
                            )
                        ),
                static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    ),
                classId
                );
            addedTargets.insert(classId);
        }
        const QList<int> eligibleIdentityIds =
            result.classIdsByIdentity.value(
                classIdentityKey(candidate.classGrade, candidate.classLevel)
                );
        for (const int classId : eligibleIdentityIds)
        {
            if (addedTargets.contains(classId))
            {
                continue;
            }
            const auto* classroom = classSnapshot(classId);
            if (!classroom)
            {
                continue;
            }
            const ClassInfo info = classInfoForEligibility(*classroom);
            if (
                !scheduleImportClassOptionIsEligible(
                    candidate,
                    info,
                    request.kind
                    )
                )
            {
                continue;
            }
            addResolutionItem(
                control.action,
                translate("Update existing: %1")
                    .arg(
                        classLabel(
                            *classroom,
                            teacherSnapshot(snapshotTeacherId(
                                classroom->teacherId
                                )),
                            request.kind
                            )
                        ),
                static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    ),
                classId
                );
        }
        addResolutionItem(
            control.action,
            translate("Create new class"),
            static_cast<int>(
                ScheduleImportClassAction::CreateNew
                )
            );
        addResolutionItem(
            control.action,
            translate("Skip imported class"),
            static_cast<int>(
                ScheduleImportClassAction::Skip
                ),
            preview.suggestedClassId
            );

        const int selectedActionIndex = findActionIndex(
            control.action,
            control.actionValue,
            control.targetClassId
            );
        if (selectedActionIndex >= 0)
        {
            control.action->setCurrentIndex(selectedActionIndex);
        }
        else
        {
            control.actionValue = control.action->currentData(ActionRole).toInt();
            control.targetClassId = control.action->currentData(TargetRole).toInt();
        }

        const QString importedMeetings =
            compactMeetingText(candidate.times);
        auto* classCard =
            createReconciliationCard(
                request.classContent,
                QStringLiteral("scheduleImportClassCard_%1")
                    .arg(preview.candidateIndex)
                );
        auto* classCardLayout =
            new QVBoxLayout(classCard);
        auto* importedClassLabel =
            new QLabel(
                QStringLiteral("%1 %2 — %3\n(%4)")
                    .arg(
                        candidate.classGrade,
                        candidate.classLevel,
                        candidate.teacherKr,
                        importedMeetings.isEmpty()
                            ? translate("time unavailable")
                            : importedMeetings
                        ),
                classCard
                );
        importedClassLabel->setObjectName(
            QStringLiteral("scheduleImportClassCandidate_%1")
                .arg(preview.candidateIndex)
            );
        QFont classFont = importedClassLabel->font();
        classFont.setBold(true);
        importedClassLabel->setFont(classFont);
        importedClassLabel->setWordWrap(true);
        importedClassLabel->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );
        auto* classHeaderLayout = new QHBoxLayout();
        classHeaderLayout->setContentsMargins(0, 0, 0, 0);
        classHeaderLayout->setSpacing(8);
        auto* colorLayout = new QVBoxLayout();
        colorLayout->setContentsMargins(0, 0, 0, 0);
        colorLayout->setSpacing(0);
        auto* colorLabel =
            new QLabel(
                translate("Color"),
                classCard
                );
        colorLabel->setObjectName(
            QStringLiteral("scheduleImportClassColorLabel_%1")
                .arg(preview.candidateIndex)
            );
        colorLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
        colorLayout->addWidget(
            colorLabel,
            0,
            Qt::AlignRight | Qt::AlignTop
            );
        colorLayout->addWidget(
            control.colorButton,
            0,
            Qt::AlignRight | Qt::AlignTop
            );
        classHeaderLayout->addWidget(
            importedClassLabel,
            1,
            Qt::AlignLeft | Qt::AlignBottom
            );
        classHeaderLayout->addLayout(colorLayout);
        classCardLayout->addLayout(classHeaderLayout);
        auto* matchStatus =
            new QLabel(
                preview.matchExplanation,
                classCard
                );
        matchStatus->setObjectName(
            QStringLiteral("scheduleImportClassMatch_%1")
                .arg(preview.candidateIndex)
            );
        matchStatus->setWordWrap(true);
        matchStatus->setProperty(
            "matchConfidence",
            static_cast<int>(preview.matchConfidence)
            );
        classCardLayout->addWidget(matchStatus);
        addLabeledControlRow(
            classCardLayout,
            translate("Import Action"),
            control.action,
            classCard
            );
        classCardLayout->addWidget(
            control.details
            );
        connectStateChanged(control.action, request);
        connectColorRequested(
            control.colorButton,
            preview.candidateIndex,
            request
            );
        control.card = classCard;
        if (request.classLayout)
        {
            request.classLayout->addWidget(classCard);
        }
        else
        {
            classCard->setGeometry(
                8,
                row * request.classRowExtent + 6,
                std::max(0, request.classContent->width() - 16),
                request.classRowExtent - 12
                );
            finalizeVirtualizedCard(
                classCard,
                {
                    importedClassLabel,
                    colorLabel,
                    control.colorButton,
                    matchStatus,
                    control.action,
                    control.details
                }
                );
        }
        result.classControls[row] = control;
    }

    return result;
}

void updateClassColorButton(
    ClassControl* control
    )
{
    if (!control || !control->colorButton)
    {
        return;
    }

    QColor color(control->color);
    if (!color.isValid())
    {
        color = QColor(QStringLiteral("#FFFFFF"));
        control->color = QStringLiteral("#FFFFFF");
    }
    const QString description =
        QStringLiteral("%1 (%2)")
            .arg(
                translate("Select Imported Class Color"),
                control->color
                );
    control->colorButton->setText(QString());
    control->colorButton->setToolTip(description);
    control->colorButton->setAccessibleName(description);
    control->colorButton->setFixedSize(
        UiConstants::ClassInfo::Details::ColorPreviewWidth,
        UiConstants::ClassInfo::Details::ColorPreviewHeight
        );
    control->colorButton->setSizePolicy(
        QSizePolicy::Fixed,
        QSizePolicy::Fixed
        );
    control->colorButton->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "background-color:%1;"
            "border:1px solid #777;"
            "border-radius:4px;"
            "padding:0;"
            "}"
            )
            .arg(control->color)
        );
}

void captureRealizedDecisions(
    BuildResult* state
    )
{
    if (!state)
    {
        return;
    }
    for (TeacherControl& control : state->teacherControls)
    {
        if (control.action)
        {
            control.actionValue = control.action->currentData(ActionRole).toInt();
            control.targetTeacherId =
                control.action->currentData(TargetRole).toLongLong();
        }
        if (control.room)
        {
            control.selectedRoom = control.room->currentData().toString();
        }
    }
    for (ClassControl& control : state->classControls)
    {
        if (control.action)
        {
            control.actionValue = control.action->currentData(ActionRole).toInt();
            control.targetClassId = control.action->currentData(TargetRole).toInt();
        }
    }
}

void clearRealizedControls(
    BuildResult* state
    )
{
    if (!state)
    {
        return;
    }
    captureRealizedDecisions(state);
    for (TeacherControl& control : state->teacherControls)
    {
        if (control.card)
        {
            delete control.card;
        }
        control.card = nullptr;
        control.action = nullptr;
        control.room = nullptr;
    }
    for (ClassControl& control : state->classControls)
    {
        if (control.card)
        {
            delete control.card;
        }
        control.card = nullptr;
        control.action = nullptr;
        control.colorButton = nullptr;
        control.details = nullptr;
    }
}

void forceSkippedTeachers(
    BuildResult* state,
    const QSet<QString>& skippedTeacherKeys
    )
{
    if (!state || skippedTeacherKeys.isEmpty())
    {
        return;
    }
    for (ClassControl& control : state->classControls)
    {
        if (!skippedTeacherKeys.contains(control.teacherKey))
        {
            continue;
        }
        control.actionValue = static_cast<int>(
            ScheduleImportClassAction::Skip
            );
        control.targetClassId = control.skipTargetId;
        if (control.action)
        {
            const QSignalBlocker blocker(control.action);
            const int index = findActionIndex(
                control.action,
                control.actionValue,
                control.targetClassId
                );
            if (index >= 0)
            {
                control.action->setCurrentIndex(index);
            }
        }
    }
}

int realizedTeacherCount(const BuildResult& state)
{
    return static_cast<int>(std::count_if(
        state.teacherControls.cbegin(),
        state.teacherControls.cend(),
        [](const TeacherControl& control) { return control.card != nullptr; }
        ));
}

int realizedClassCount(const BuildResult& state)
{
    return static_cast<int>(std::count_if(
        state.classControls.cbegin(),
        state.classControls.cend(),
        [](const ClassControl& control) { return control.card != nullptr; }
        ));
}

int realizedClassTargetChoiceCount(const BuildResult& state)
{
    int count = 0;
    for (const ClassControl& control : state.classControls)
    {
        if (!control.action)
        {
            continue;
        }
        for (int index = 0; index < control.action->count(); ++index)
        {
            if (control.action->itemData(index, ActionRole).toInt()
                == static_cast<int>(ScheduleImportClassAction::UpdateExisting))
            {
                ++count;
            }
        }
    }
    return count;
}

QList<ScheduleImportTeacherResolution> teacherResolutions(
    const BuildResult& state
    )
{
    QList<ScheduleImportTeacherResolution> result;
    result.reserve(state.teacherControls.size());
    for (const TeacherControl& control : state.teacherControls)
    {
        result.append(
            {
                control.teacherKey,
                static_cast<ScheduleImportTeacherAction>(control.actionValue),
                control.targetTeacherId,
                control.selectedRoom
            }
            );
    }
    return result;
}

QList<ScheduleImportClassResolution> classResolutions(
    const BuildResult& state
    )
{
    QList<ScheduleImportClassResolution> result;
    result.reserve(state.classControls.size());
    for (const ClassControl& control : state.classControls)
    {
        std::optional<ImportDomain::ClassId> targetClassId;
        if (control.targetClassId > 0)
        {
            targetClassId = ImportDomain::ClassId::fromString(
                std::to_string(control.targetClassId)
                );
        }
        result.append(
            {
                control.candidateIndex,
                static_cast<ScheduleImportClassAction>(control.actionValue),
                std::move(targetClassId),
                control.color,
                ColorUtils::getContrastingFontColor(QColor(control.color))
            }
            );
    }
    return result;
}
}
