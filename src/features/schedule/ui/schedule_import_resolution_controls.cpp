#include "schedule_import_resolution_controls.h"

#include "schedule_import_review_presentation.h"
#include "schedule_import_resolution_view.h"

#include "app/services/feature_services.h"
#include "domain/models/classroom.h"
#include "domain/rules/schedule_import_rules.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QSet>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>

namespace ScheduleImportResolutionControls
{
namespace
{
using namespace ScheduleImportResolutionView;
using namespace ScheduleImportReviewPresentation;

QString translate(
    const char* source
    )
{
    return QCoreApplication::translate(
        "ScheduleImportReviewDialog",
        source
        );
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
}

Result<BuildResult> build(
    const BuildRequest& request
    )
{
    BuildResult result;
    if (
        !request.teacherContent
        || !request.classContent
        || !request.teacherLayout
        || !request.classLayout
        || !request.classService
        || !request.teacherService
        || !request.preview
        )
    {
        return result;
    }

    ClassService* classService = request.classService;
    TeacherService* teacherService = request.teacherService;

    for (int row = 0;
         row < request.preview->teachers.size();
         ++row)
    {
        const ScheduleImportTeacherPreview& preview =
            request.preview->teachers[row];
        TeacherControl control;
        control.teacherKey = preview.teacherKey;
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

        if (preview.matchingTeacherIds.size() == 1)
        {
            const int teacherId =
                preview.matchingTeacherIds.first();
            const Result<Teacher> existing =
                teacherService->teacher(teacherId);
            if (!existing)
            {
                return std::unexpected(existing.error());
            }
            const bool exactRoom =
                preview.importedRooms.size() == 1
                && existing->roomNumber.trimmed()
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
            for (int teacherId : preview.matchingTeacherIds)
            {
                const Result<Teacher> existing =
                    teacherService->teacher(teacherId);
                if (!existing)
                {
                    return std::unexpected(existing.error());
                }
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
        connectStateChanged(control.action, request);
        connectStateChanged(control.room, request);
        request.teacherLayout->addWidget(teacherCard);
        result.teacherControls.append(control);
    }
    request.teacherLayout->addStretch();

    const Result<QList<Classroom>> allClasses =
        classService->classes();
    if (!allClasses)
    {
        return std::unexpected(allClasses.error());
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

    for (const ScheduleImportClassPreview& preview :
         orderedClasses)
    {
        const ScheduleImportClassCandidate& candidate =
            request.preview->user.classes[
                preview.candidateIndex
                ];
        ClassControl control;
        control.candidateIndex =
            preview.candidateIndex;
        control.teacherKey =
            candidate.teacherKey;
        control.action =
            new NoWheelComboBox(request.classContent);
        configureCompactActionCombo(control.action);
        control.colorButton =
            new QPushButton(request.classContent);
        control.color =
            candidate.importedColors.isEmpty()
                ? QStringLiteral("#FFFFFF")
                : candidate.importedColors.first().toUpper();
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
                            classService,
                            teacherService,
                            classId,
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
        for (const Classroom& classroom : *allClasses)
        {
            if (addedTargets.contains(classroom.id))
            {
                continue;
            }
            const ClassInfo info =
                classService->classInfo(classroom.id);
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
                            classService,
                            teacherService,
                            classroom.id,
                            request.kind
                            )
                        ),
                static_cast<int>(
                    ScheduleImportClassAction::UpdateExisting
                    ),
                classroom.id
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

        if (preview.suggestedClassId > 0)
        {
            control.action->setCurrentIndex(
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::UpdateExisting
                        ),
                    preview.suggestedClassId
                    )
                );
        }
        else if (preview.matchingClassIds.isEmpty())
        {
            control.action->setCurrentIndex(
                findActionIndex(
                    control.action,
                    static_cast<int>(
                        ScheduleImportClassAction::CreateNew
                        )
                    )
                );
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
        request.classLayout->addWidget(classCard);
        result.classControls.append(control);
    }
    request.classLayout->addStretch();

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
}
