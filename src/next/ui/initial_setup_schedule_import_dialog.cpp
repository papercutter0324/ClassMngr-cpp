#include "next/ui/initial_setup_schedule_import_dialog.h"

#include "features/schedule/import/schedule_workbook_parser.h"
#include "next/application/schedule_import_apply_validation.h"
#include "next/application/schedule_import_review_summary_projection.h"
#include "next/application/schedule_import_schedules_cleared_projection.h"
#include "next/application/schedule_import_state_projection.h"
#include "next/ui/schedule_import_request_mapper.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString sourceText(const char* value)
{
    return QCoreApplication::translate("InitialSetupScheduleImportDialog", value);
}

QString reviewText(const char* value)
{
    return QCoreApplication::translate("InitialSetupScheduleImportReviewDialog", value);
}

QString utf8String(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString roomText(const Application::ScheduleImportReviewRoomValue& room)
{
    return utf8String(room.value());
}

QString idText(const Domain::ClassId& id)
{
    return utf8String(id.value());
}

QString idText(const Domain::TeacherId& id)
{
    return utf8String(id.value());
}

template <typename Id>
std::optional<Id> typedId(const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    return Id::fromString(std::string(
        bytes.constData(),
        static_cast<std::size_t>(bytes.size())
        ));
}

QString classLabel(
    const Application::ScheduleImportStateReadClassSnapshot& classroom
    )
{
    const QString name = QString::fromStdU16String(classroom.className);
    const QString course = QStringLiteral("%1 %2")
        .arg(QString::fromStdU16String(classroom.grade),
             QString::fromStdU16String(classroom.level));
    const QString room = QString::fromStdU16String(classroom.roomNumber);
    const QString details = room.isEmpty()
        ? course
        : reviewText("%1 — Room %2").arg(course, room);
    return name.isEmpty() ? details : QStringLiteral("%1 — %2").arg(name, details);
}

QString scheduleLines(const ScheduleImportClassCandidate& candidate)
{
    QStringList lines;
    for (const ClassTime& time : candidate.times)
    {
        lines.append(QStringLiteral("%1 %2–%3")
                         .arg(time.day, time.startTime, time.endTime));
    }
    return lines.join(QStringLiteral("; "));
}

int selectedIndex(const QComboBox* combo)
{
    return combo ? combo->currentData().toInt() : -1;
}

template <typename Id>
QString typedIdText(const std::optional<Id>& id)
{
    return id ? utf8String(id->value()) : QString{};
}

Application::ScheduleImportReviewDecisionRequest collectReviewDecisions(
    const ScheduleImportReviewMapping& mapping,
    const QList<QComboBox*>& teacherActions,
    const QList<QComboBox*>& teacherRooms,
    const QList<QComboBox*>& classActions,
    const QList<QComboBox*>& classTargets
    )
{
    Application::ScheduleImportReviewDecisionRequest result = mapping.decisions;
    for (qsizetype index = 0;
         index < result.teachers.size() && index < teacherActions.size();
         ++index)
    {
        auto& teacher = result.teachers[static_cast<std::size_t>(index)];
        teacher.action = static_cast<Application::ScheduleImportReviewTeacherAction>(
            selectedIndex(teacherActions[index])
            );
        teacher.selectedRoom = Application::ScheduleImportReviewRoomValue(
            teacherRooms[index]->currentData().toString().toUtf8().toStdString()
            );
    }
    for (qsizetype index = 0;
         index < result.classes.size() && index < classActions.size();
         ++index)
    {
        auto& classroom = result.classes[static_cast<std::size_t>(index)];
        classroom.action = static_cast<Application::ScheduleImportReviewClassAction>(
            selectedIndex(classActions[index])
            );
        classroom.targetClassId = typedId<Domain::ClassId>(
            classTargets[index]->currentData().toString()
            );
        if (classroom.action == Application::ScheduleImportReviewClassAction::CreateNew)
        {
            classroom.targetClassId.reset();
        }
    }
    return result;
}

QString readinessText(
    const Application::ScheduleImportReviewReadinessResult& readiness
    )
{
    if (!readiness.decisions.accepted())
    {
        return reviewText("Choose a valid action and target for each teacher and class.");
    }
    if (!readiness.stateError)
    {
        return {};
    }
    const auto& error = *readiness.stateError;
    if (error.code == Application::ScheduleImportStateValidationErrorCode::ProjectedScheduleOverlap)
    {
        return reviewText(
            "Projected schedules overlap: %1 conflicts with %2 at %3 %4–%5."
            ).arg(
                QString::fromUtf8(error.classLabel),
                QString::fromUtf8(error.conflictingClassLabel),
                QString::fromUtf8(error.day),
                QString::fromUtf8(error.startTime),
                QString::fromUtf8(error.endTime)
                );
    }
    if (error.code == Application::ScheduleImportStateValidationErrorCode::InvalidProjectedTime)
    {
        return reviewText("A projected time is invalid: %1 %2 %3–%4.")
            .arg(
                QString::fromUtf8(error.classLabel),
                QString::fromUtf8(error.day),
                QString::fromUtf8(error.startTime),
                QString::fromUtf8(error.endTime)
                );
    }
    return reviewText("The selected targets are no longer valid.");
}

QString applySummaryText(const Application::ScheduleImportApplySummary& summary)
{
    return reviewText(
        "Teachers created: %1; rooms updated: %2; classes created: %3; classes updated: %4; classes skipped: %5; schedules cleared: %6; workbook diagnostics: %7."
        )
        .arg(summary.teachersCreated.rawValue())
        .arg(summary.teachersUpdated.rawValue())
        .arg(summary.classesCreated.rawValue())
        .arg(summary.classesUpdated.rawValue())
        .arg(summary.classesSkipped.rawValue())
        .arg(summary.schedulesCleared.rawValue())
        .arg(summary.ignoredCells.rawValue());
}

} // namespace

struct InitialSetupScheduleImportDialog::ParseResult final
{
    quint64 requestId = 0;
    QString path;
    ScheduleImportKind kind = ScheduleImportKind::Normal;
    qint64 initialSize = -1;
    qint64 initialModified = -1;
    qint64 finalSize = -1;
    qint64 finalModified = -1;
    QString fileError;
    Result<ScheduleImportWorkbook> workbook = std::unexpected(QString{});
};

namespace
{

class ScheduleImportReviewDialog final : public QDialog
{
public:
    ScheduleImportReviewDialog(
        InitialSetupScheduleImportActions actions,
        ScheduleImportReviewMapping mapping,
        QString profileName,
        const bool saveProfileNameIfBlank,
        const bool updateProfileName,
        QWidget* parent
        )
        : QDialog(parent),
          m_actions(std::move(actions)),
          m_mapping(std::move(mapping)),
          m_profileName(std::move(profileName)),
          m_saveProfileNameIfBlank(saveProfileNameIfBlank),
          m_updateProfileName(updateProfileName),
          m_colors(m_mapping.classColors)
    {
        setObjectName(QStringLiteral("classMngrNextScheduleImportReviewDialog"));
        setWindowTitle(reviewText("Review Schedule Import"));
        setModal(true);
        resize(1120, 820);

        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(
            reviewText("Review all teacher, class, room, color, and schedule decisions before applying."),
            this
            );
        heading->setWordWrap(true);
        layout->addWidget(heading);

        auto* modeRow = new QHBoxLayout;
        modeRow->addWidget(new QLabel(reviewText("Intensive import mode:"), this));
        m_intensiveMode = new QComboBox(this);
        m_intensiveMode->setObjectName(QStringLiteral("classMngrNextScheduleImportIntensiveMode"));
        m_intensiveMode->addItem(
            reviewText("Update Existing (preserve absent classes)"),
            static_cast<int>(Application::ScheduleImportPlanIntensiveMode::UpdateExisting)
            );
        m_intensiveMode->addItem(
            reviewText("Replace With New (clear absent classes)"),
            static_cast<int>(Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew)
            );
        m_intensiveMode->setVisible(m_mapping.kind == ScheduleImportKind::Intensive);
        modeRow->addWidget(m_intensiveMode, 1);
        layout->addLayout(modeRow);

        m_diagnostics = new QTextEdit(this);
        m_diagnostics->setObjectName(QStringLiteral("classMngrNextScheduleImportDiagnostics"));
        m_diagnostics->setReadOnly(true);
        m_diagnostics->setMaximumHeight(135);
        QStringList diagnosticLines;
        for (const ScheduleImportDiagnostic& diagnostic : m_mapping.user.diagnostics)
        {
            diagnosticLines.append(QStringLiteral("%1 — %2 (%3 %4): %5")
                .arg(
                    diagnostic.sheetName,
                    diagnostic.userName,
                    diagnostic.cellReference,
                    diagnostic.value,
                    diagnostic.message
                    ));
        }
        m_diagnostics->setPlainText(diagnosticLines.isEmpty()
            ? reviewText("No workbook diagnostics were found.")
            : diagnosticLines.join(QLatin1Char('\n')));
        layout->addWidget(m_diagnostics);

        m_acknowledgeDiagnostics = new QCheckBox(
            reviewText("I reviewed and acknowledge every workbook diagnostic."),
            this
            );
        m_acknowledgeDiagnostics->setObjectName(
            QStringLiteral("classMngrNextScheduleImportAcknowledgeDiagnostics")
            );
        m_acknowledgeDiagnostics->setVisible(!m_mapping.user.diagnostics.isEmpty());
        layout->addWidget(m_acknowledgeDiagnostics);

        m_effectsLabel = new QLabel(this);
        m_effectsLabel->setObjectName(QStringLiteral("classMngrNextScheduleImportEffects"));
        m_effectsLabel->setWordWrap(true);
        layout->addWidget(m_effectsLabel);

        auto* reviewContent = new QWidget(this);
        auto* reviewLayout = new QVBoxLayout(reviewContent);
        reviewLayout->setContentsMargins(0, 0, 0, 0);
        reviewLayout->setSpacing(10);
        buildTeacherControls(reviewLayout);
        buildClassControls(reviewLayout);

        auto* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setWidget(reviewContent);
        layout->addWidget(scrollArea, 1);

        m_status = new QLabel(this);
        m_status->setObjectName(QStringLiteral("classMngrNextScheduleImportReviewStatus"));
        m_status->setWordWrap(true);
        layout->addWidget(m_status);

        auto* buttons = new QDialogButtonBox(this);
        m_applyButton = buttons->addButton(
            reviewText("Apply Schedule Import"),
            QDialogButtonBox::AcceptRole
            );
        m_applyButton->setObjectName(QStringLiteral("classMngrNextScheduleImportApply"));
        auto* cancel = buttons->addButton(
            reviewText("Back to Source"),
            QDialogButtonBox::RejectRole
            );
        cancel->setObjectName(QStringLiteral("classMngrNextScheduleImportBackToSource"));
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_applyButton, &QPushButton::clicked, this, [this] { apply(); });
        connect(m_intensiveMode,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                [this] { updateReview(); });
        connect(m_acknowledgeDiagnostics,
                &QCheckBox::toggled,
                this,
                [this] { updateReview(); });
        updateReview();
    }

    [[nodiscard]] QString successSummary() const
    {
        return m_successSummary;
    }

private:
    struct TeacherControls final
    {
        QComboBox* action = nullptr;
        QComboBox* target = nullptr;
        QComboBox* room = nullptr;
    };

    struct ClassControls final
    {
        QComboBox* action = nullptr;
        QComboBox* target = nullptr;
        QPushButton* color = nullptr;
    };

    [[nodiscard]] Application::ScheduleImportPlanIntensiveMode intensiveMode() const
    {
        return static_cast<Application::ScheduleImportPlanIntensiveMode>(
            selectedIndex(m_intensiveMode)
            );
    }

    void buildTeacherControls(QVBoxLayout* layout)
    {
        auto* group = new QGroupBox(reviewText("Korean Teacher Resolutions"), this);
        auto* groupLayout = new QVBoxLayout(group);
        m_teacherControls.reserve(
            static_cast<qsizetype>(m_mapping.matching.teachers.size())
            );
        for (qsizetype index = 0;
             index < static_cast<qsizetype>(m_mapping.matching.teachers.size());
             ++index)
        {
            const auto& teacher = m_mapping.matching.teachers[static_cast<std::size_t>(index)];
            auto* row = new QGroupBox(
                QString::fromStdU16String(teacher.teacherName),
                group
                );
            row->setObjectName(QStringLiteral("classMngrNextScheduleImportTeacherRow"));
            auto* form = new QFormLayout(row);

            auto* action = new QComboBox(row);
            action->setObjectName(QStringLiteral("classMngrNextScheduleImportTeacherAction"));
            action->addItem(reviewText("Select action..."),
                static_cast<int>(Application::ScheduleImportReviewTeacherAction::Unselected));
            action->addItem(reviewText("Reuse"),
                static_cast<int>(Application::ScheduleImportReviewTeacherAction::Reuse));
            action->addItem(reviewText("Update Room"),
                static_cast<int>(Application::ScheduleImportReviewTeacherAction::UpdateRoom));
            action->addItem(reviewText("Create"),
                static_cast<int>(Application::ScheduleImportReviewTeacherAction::Create));
            action->addItem(reviewText("Skip"),
                static_cast<int>(Application::ScheduleImportReviewTeacherAction::Skip));
            action->setCurrentIndex(action->findData(static_cast<int>(
                m_mapping.decisions.teachers[static_cast<std::size_t>(index)].action
                )));

            auto* target = new QComboBox(row);
            target->setObjectName(QStringLiteral("classMngrNextScheduleImportTeacherTarget"));
            target->addItem(reviewText("Choose existing teacher..."), QString{});
            for (const Domain::TeacherId& id : teacher.matchingTeacherIds)
            {
                const auto existing = std::find_if(
                    m_mapping.snapshot.teachers.begin(),
                    m_mapping.snapshot.teachers.end(),
                    [&id](const auto& value) { return value.id == id; }
                    );
                QString label = QString::fromStdU16String(teacher.teacherName);
                if (existing != m_mapping.snapshot.teachers.end())
                {
                    label += QStringLiteral(" — ")
                        + QString::fromStdU16String(existing->roomNumber);
                }
                target->addItem(label + QStringLiteral(" (#%1)").arg(idText(id)), idText(id));
            }
            if (m_mapping.selectedTeacherIds[static_cast<std::size_t>(index)])
            {
                target->setCurrentIndex(target->findData(idText(
                    *m_mapping.selectedTeacherIds[static_cast<std::size_t>(index)]
                    )));
            }

            auto* room = new QComboBox(row);
            room->setObjectName(QStringLiteral("classMngrNextScheduleImportTeacherRoom"));
            room->addItem(reviewText("Choose imported room..."), QString{});
            for (const std::u16string& importedRoom : teacher.importedRooms)
            {
                const QString text = QString::fromStdU16String(importedRoom);
                room->addItem(text.isEmpty() ? reviewText("No room") : text, text);
            }
            const QString selectedRoom = roomText(
                m_mapping.decisions.teachers[static_cast<std::size_t>(index)].selectedRoom
                );
            if (!selectedRoom.isEmpty())
            {
                room->setCurrentIndex(room->findData(selectedRoom));
            }

            form->addRow(reviewText("Action"), action);
            form->addRow(reviewText("Existing teacher"), target);
            form->addRow(reviewText("Imported room"), room);
            auto* details = new QLabel(
                reviewText("Matching existing classes: %1; imported rooms: %2")
                    .arg(teacher.affectedClassCount.rawValue())
                    .arg(teacher.importedRooms.size()),
                row
                );
            details->setWordWrap(true);
            form->addRow(details);
            groupLayout->addWidget(row);
            m_teacherControls.append({action, target, room});
            connect(action, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this, index] { updateTeacherControls(index); });
            connect(target, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this] { updateReview(); });
            connect(room, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this] { updateReview(); });
            updateTeacherControls(index);
        }
        layout->addWidget(group);
    }

    void updateTeacherControls(qsizetype index)
    {
        if (index < 0 || index >= m_teacherControls.size()) return;
        const auto& controls = m_teacherControls[index];
        const auto action = static_cast<Application::ScheduleImportReviewTeacherAction>(
            selectedIndex(controls.action)
            );
        const bool existingTarget = action == Application::ScheduleImportReviewTeacherAction::Reuse
            || action == Application::ScheduleImportReviewTeacherAction::UpdateRoom;
        const bool needsRoom = action == Application::ScheduleImportReviewTeacherAction::Create
            || action == Application::ScheduleImportReviewTeacherAction::UpdateRoom;
        controls.target->setEnabled(existingTarget);
        controls.room->setEnabled(needsRoom);
        if (!existingTarget)
        {
            controls.target->setCurrentIndex(0);
        }
        updateReview();
    }

    void buildClassControls(QVBoxLayout* layout)
    {
        auto* group = new QGroupBox(reviewText("Class Resolutions"), this);
        auto* groupLayout = new QVBoxLayout(group);
        m_classControls.reserve(
            static_cast<qsizetype>(m_mapping.matching.classes.size())
            );
        for (qsizetype index = 0;
             index < static_cast<qsizetype>(m_mapping.matching.classes.size());
             ++index)
        {
            const auto& projection = m_mapping.matching.classes[static_cast<std::size_t>(index)];
            const std::size_t candidateIndex = projection.candidateIndex.rawValue();
            const ScheduleImportClassCandidate& candidate =
                m_mapping.user.classes.at(static_cast<qsizetype>(candidateIndex));
            auto* row = new QGroupBox(
                QStringLiteral("%1 %2 — %3")
                    .arg(candidate.classGrade, candidate.classLevel, candidate.teacherKr),
                group
                );
            row->setObjectName(QStringLiteral("classMngrNextScheduleImportClassRow"));
            auto* form = new QFormLayout(row);

            QString detailText = scheduleLines(candidate);
            if (!candidate.rooms.isEmpty())
            {
                detailText += QStringLiteral("\n")
                    + reviewText("Imported rooms: %1").arg(candidate.rooms.join(QStringLiteral(", ")));
            }
            if (!candidate.meetingPatternError.isEmpty())
            {
                detailText += QStringLiteral("\n")
                    + reviewText("Meeting pattern: %1").arg(candidate.meetingPatternError);
            }
            if (candidate.importedColors.size() > 1)
            {
                detailText += QStringLiteral("\n")
                    + reviewText("The workbook has multiple colors for this class. Confirm a color below.");
            }
            auto* details = new QLabel(detailText, row);
            details->setWordWrap(true);

            auto* action = new QComboBox(row);
            action->setObjectName(QStringLiteral("classMngrNextScheduleImportClassAction"));
            action->addItem(reviewText("Select action..."),
                static_cast<int>(Application::ScheduleImportReviewClassAction::Unselected));
            action->addItem(reviewText("Update Existing"),
                static_cast<int>(Application::ScheduleImportReviewClassAction::UpdateExisting));
            action->addItem(reviewText("Create New"),
                static_cast<int>(Application::ScheduleImportReviewClassAction::CreateNew));
            action->addItem(reviewText("Skip"),
                static_cast<int>(Application::ScheduleImportReviewClassAction::Skip));
            action->setCurrentIndex(action->findData(static_cast<int>(
                m_mapping.decisions.classes[static_cast<std::size_t>(index)].action
                )));

            auto* target = new QComboBox(row);
            target->setObjectName(QStringLiteral("classMngrNextScheduleImportClassTarget"));
            target->addItem(reviewText("Choose existing class..."), QString{});
            for (const Domain::ClassId& id : projection.matchingClassIds)
            {
                const auto existing = std::find_if(
                    m_mapping.snapshot.classes.begin(),
                    m_mapping.snapshot.classes.end(),
                    [&id](const auto& value) { return value.id == id; }
                    );
                if (existing != m_mapping.snapshot.classes.end())
                {
                    target->addItem(classLabel(*existing), idText(id));
                }
            }
            const auto defaultTarget =
                m_mapping.decisions.classes[static_cast<std::size_t>(index)].targetClassId;
            if (defaultTarget)
            {
                target->setCurrentIndex(target->findData(idText(*defaultTarget)));
            }

            auto* color = new QPushButton(reviewText("Choose Class Color…"), row);
            color->setObjectName(QStringLiteral("classMngrNextScheduleImportClassColor"));
            const QString colorName = utf8String(m_colors.at(candidateIndex).classColor);
            color->setProperty("classImportColor", colorName);
            updateColorButton(color, colorName);

            form->addRow(details);
            form->addRow(reviewText("Action"), action);
            form->addRow(reviewText("Existing class"), target);
            form->addRow(reviewText("Class color"), color);
            groupLayout->addWidget(row);
            m_classControls.append({action, target, color});
            connect(action, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this, index] { updateClassControls(index); });
            connect(target, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this] { updateReview(); });
            connect(color, &QPushButton::clicked, this, [this, color, candidateIndex] {
                const QColor selected = QColorDialog::getColor(
                    QColor(color->property("classImportColor").toString()),
                    this,
                    reviewText("Choose Class Color")
                    );
                if (!selected.isValid()) return;
                const QString selectedName = selected.name(QColor::HexRgb).toUpper();
                color->setProperty("classImportColor", selectedName);
                updateColorButton(color, selectedName);
                setColor(candidateIndex, selectedName);
                updateReview();
            });
            updateClassControls(index);
        }
        layout->addWidget(group);
    }

    static void updateColorButton(QPushButton* button, const QString& color)
    {
        const QColor selected(color);
        const QString foreground = selected.lightnessF() >= 0.5
            ? QStringLiteral("#000000")
            : QStringLiteral("#FFFFFF");
        button->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; color: %2; padding: 5px; }"
            ).arg(color, foreground));
    }

    void setColor(const std::size_t candidateIndex, const QString& color)
    {
        const QColor selected(color);
        const std::string colorUtf8 = color.toUtf8().toStdString();
        const std::string fontUtf8 = (selected.lightnessF() >= 0.5
            ? QStringLiteral("#000000")
            : QStringLiteral("#FFFFFF")).toUtf8().toStdString();
        for (auto& item : m_colors)
        {
            if (item.candidateIndex.rawValue() == candidateIndex)
            {
                item.classColor = colorUtf8;
                item.fontColor = fontUtf8;
                return;
            }
        }
    }

    void updateClassControls(qsizetype index)
    {
        if (index < 0 || index >= m_classControls.size()) return;
        const auto& controls = m_classControls[index];
        const auto action = static_cast<Application::ScheduleImportReviewClassAction>(
            selectedIndex(controls.action)
            );
        const bool targetRequired = action == Application::ScheduleImportReviewClassAction::UpdateExisting;
        controls.target->setEnabled(targetRequired);
        if (action != Application::ScheduleImportReviewClassAction::UpdateExisting)
        {
            controls.target->setCurrentIndex(0);
        }
        updateReview();
    }

    std::vector<std::optional<Domain::TeacherId>> selectedTeacherIds() const
    {
        std::vector<std::optional<Domain::TeacherId>> result;
        result.reserve(static_cast<std::size_t>(m_teacherControls.size()));
        for (const TeacherControls& controls : m_teacherControls)
        {
            result.push_back(typedId<Domain::TeacherId>(
                controls.target->currentData().toString()
                ));
        }
        return result;
    }

    QList<QComboBox*> teacherActions() const
    {
        QList<QComboBox*> result;
        for (const auto& control : m_teacherControls) result.append(control.action);
        return result;
    }
    QList<QComboBox*> teacherRooms() const
    {
        QList<QComboBox*> result;
        for (const auto& control : m_teacherControls) result.append(control.room);
        return result;
    }
    QList<QComboBox*> classActions() const
    {
        QList<QComboBox*> result;
        for (const auto& control : m_classControls) result.append(control.action);
        return result;
    }
    QList<QComboBox*> classTargets() const
    {
        QList<QComboBox*> result;
        for (const auto& control : m_classControls) result.append(control.target);
        return result;
    }

    void updateReview()
    {
        if (!m_status || !m_effectsLabel) return;
        const auto decisions = collectReviewDecisions(
            m_mapping,
            teacherActions(),
            teacherRooms(),
            classActions(),
            classTargets()
            );
        const auto teacherIds = selectedTeacherIds();
        const auto mode = intensiveMode();
        const bool acknowledged = m_mapping.user.diagnostics.isEmpty()
            || m_acknowledgeDiagnostics->isChecked();
        const auto request = makeScheduleImportApplyRequest(
            m_mapping,
            decisions,
            teacherIds,
            m_colors,
            mode,
            acknowledged,
            m_saveProfileNameIfBlank,
            m_updateProfileName
            );
        const auto readiness = evaluateScheduleImportReview(
            m_mapping,
            decisions,
            teacherIds,
            mode
            );
        QString status = readinessText(readiness);
        const auto applyFailure = Application::validateScheduleImportApplyRequest(request);
        if (applyFailure && status.isEmpty())
        {
            if (applyFailure->policyIssue
                && applyFailure->policyIssue->code
                    == Application::ScheduleImportPlanEligibilityIssueCode::UnacknowledgedDiagnostics)
            {
                status = reviewText("Acknowledge every workbook diagnostic before applying.");
            }
            else if (applyFailure->policyIssue
                && applyFailure->policyIssue->code
                    == Application::ScheduleImportPlanEligibilityIssueCode::InvalidMeetingPattern)
            {
                status = reviewText("A class has an invalid meeting pattern. Skip it or correct the workbook.");
            }
            else if (applyFailure->policyIssue
                && applyFailure->policyIssue->code
                    == Application::ScheduleImportPlanEligibilityIssueCode::InvalidCourse)
            {
                status = reviewText("A class has an unsupported grade and level.");
            }
            else
            {
                status = reviewText("Choose every required teacher, room, and class target.");
            }
        }
        else if (status.isEmpty())
        {
            status = reviewText("All import decisions are valid and ready to apply.");
        }
        m_status->setText(status);
        m_applyButton->setEnabled(
            !applyFailure
            && readiness.decisions.accepted()
            && readiness.stateStatus == Application::ScheduleImportStateEvaluationStatus::Passed
            );

        const auto stateMode = mode == Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew
            ? Application::ScheduleImportStateIntensiveMode::ReplaceWithNew
            : Application::ScheduleImportStateIntensiveMode::UpdateExisting;
        const auto stateKind = m_mapping.kind == ScheduleImportKind::Intensive
            ? Application::ScheduleImportStateKind::Intensive
            : Application::ScheduleImportStateKind::Normal;
        const auto stateRequest = makeScheduleImportStateValidationRequest(
            m_mapping,
            decisions,
            teacherIds,
            mode
            );
        const auto projectedSchedules =
            Application::projectScheduleImportStateSchedules(stateRequest);
        const bool keepsAbsentIntensiveSchedules =
            stateKind == Application::ScheduleImportStateKind::Intensive
            && stateMode
                == Application::ScheduleImportStateIntensiveMode::UpdateExisting;
        int existingRowsReplaced = 0;
        int existingRowsPreserved = 0;
        int scheduleRowsWritten = 0;
        for (const auto& classroom : m_mapping.snapshot.classes)
        {
            const auto& existingTimes = stateKind
                    == Application::ScheduleImportStateKind::Intensive
                ? classroom.intensiveTimes
                : classroom.normalTimes;
            if (existingTimes.empty())
            {
                continue;
            }
            const auto existingSchedule = std::find_if(
                projectedSchedules.begin(),
                projectedSchedules.end(),
                [&classroom](const auto& schedule)
                {
                    const auto* classId = std::get_if<Domain::ClassId>(
                        &schedule.classReference
                        );
                    return classId && *classId == classroom.id;
                }
                );
            const bool preserved = keepsAbsentIntensiveSchedules
                && existingSchedule != projectedSchedules.end()
                && existingSchedule->persistence
                    == Application::ScheduleImportStateProjectedSchedule::Persistence::KeepExistingRows;
            if (preserved)
            {
                existingRowsPreserved += static_cast<int>(existingTimes.size());
            }
            else
            {
                existingRowsReplaced += static_cast<int>(existingTimes.size());
            }
        }
        for (const auto& schedule : projectedSchedules)
        {
            if (schedule.persistence
                == Application::ScheduleImportStateProjectedSchedule::Persistence::ReplaceRows)
            {
                scheduleRowsWritten += static_cast<int>(schedule.times.size());
            }
        }
        const auto cleared = Application::projectScheduleImportSchedulesCleared(
            m_mapping.snapshot,
            decisions,
            stateKind,
            stateMode
            );
        const auto summary = Application::projectScheduleImportReviewSummary(
            decisions,
            Application::ScheduleImportReviewCount{
                static_cast<int>(m_mapping.user.diagnostics.size())
            },
            cleared
            );
        QString effects = reviewText(
            "Teachers created: %1; teacher rooms updated: %2; teachers skipped: %3; classes created: %4; classes updated: %5; classes skipped: %6."
            )
            .arg(summary.teachersCreated.rawValue())
            .arg(summary.teacherRoomsUpdated.rawValue())
            .arg(summary.teachersSkipped.rawValue())
            .arg(summary.classesCreated.rawValue())
            .arg(summary.classesUpdated.rawValue())
            .arg(summary.classesSkipped.rawValue());
        effects += QStringLiteral("\n") + reviewText(
            "Teachers created: %1; rooms updated: %2; classes created: %3; classes updated: %4; classes skipped: %5; schedules cleared: %6; workbook diagnostics: %7."
            )
            .arg(summary.teachersCreated.rawValue())
            .arg(summary.teacherRoomsUpdated.rawValue())
            .arg(summary.classesCreated.rawValue())
            .arg(summary.classesUpdated.rawValue())
            .arg(summary.classesSkipped.rawValue())
            .arg(summary.schedulesCleared.rawValue())
            .arg(summary.ignoredCells.rawValue());
        effects += QStringLiteral("\n") + reviewText(
            "Existing schedule rows replaced: %1; preserved: %2; schedule rows written: %3."
            ).arg(existingRowsReplaced)
            .arg(existingRowsPreserved)
            .arg(scheduleRowsWritten);
        effects += QStringLiteral("\n") + reviewText(
            "Workbook diagnostics requiring acknowledgement: %1."
            ).arg(summary.ignoredCells.rawValue());
        if (m_updateProfileName)
        {
            effects += QStringLiteral("\n") + reviewText(
                "The confirmed workbook name updates My Information after a successful apply."
                );
        }
        else if (m_saveProfileNameIfBlank)
        {
            effects += QStringLiteral("\n") + reviewText(
                "The workbook name fills the blank profile name after a successful apply."
                );
        }
        else if (!m_profileName.isEmpty()
                 && normalizedScheduleImportUserName(m_mapping.user.name)
                    != normalizedScheduleImportUserName(m_profileName))
        {
            effects += QStringLiteral("\n") + reviewText("The profile name remains unchanged.");
        }
        m_effectsLabel->setText(effects);
    }

    void apply()
    {
        const auto decisions = collectReviewDecisions(
            m_mapping,
            teacherActions(),
            teacherRooms(),
            classActions(),
            classTargets()
            );
        const auto request = makeScheduleImportApplyRequest(
            m_mapping,
            decisions,
            selectedTeacherIds(),
            m_colors,
            intensiveMode(),
            m_mapping.user.diagnostics.isEmpty()
                || m_acknowledgeDiagnostics->isChecked(),
            m_saveProfileNameIfBlank,
            m_updateProfileName
            );
        if (!m_actions.apply)
        {
            m_status->setText(reviewText("Schedule import is unavailable."));
            return;
        }
        const auto applied = m_actions.apply(request);
        if (!applied)
        {
            m_status->setText(reviewText("Schedule import failed: %1")
                .arg(QString::fromStdU16String(applied.error().message)));
            return;
        }
        m_successSummary = applySummaryText(*applied);
        m_status->setText(m_successSummary);
        m_applyButton->setEnabled(false);
        accept();
    }

    InitialSetupScheduleImportActions m_actions;
    ScheduleImportReviewMapping m_mapping;
    QString m_profileName;
    bool m_saveProfileNameIfBlank = false;
    bool m_updateProfileName = false;
    std::vector<Application::ScheduleImportPlanEligibilityClassColors> m_colors;
    QComboBox* m_intensiveMode = nullptr;
    QTextEdit* m_diagnostics = nullptr;
    QCheckBox* m_acknowledgeDiagnostics = nullptr;
    QLabel* m_effectsLabel = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_applyButton = nullptr;
    QList<TeacherControls> m_teacherControls;
    QList<ClassControls> m_classControls;
    QString m_successSummary;
};

} // namespace

InitialSetupScheduleImportDialog::InitialSetupScheduleImportDialog(
    InitialSetupScheduleImportActions actions,
    QString profileName,
    QWidget* parent
    )
    : QDialog(parent),
      m_actions(std::move(actions)),
      m_profileName(std::move(profileName))
{
    setObjectName(QStringLiteral("classMngrNextScheduleImportSourceDialog"));
    setWindowTitle(sourceText("Import Schedule"));
    setModal(true);
    resize(680, 540);

    auto* layout = new QVBoxLayout(this);
    auto* fileRow = new QHBoxLayout;
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setObjectName(QStringLiteral("classMngrNextScheduleImportFilePath"));
    m_filePathEdit->setReadOnly(true);
    m_browseButton = new QPushButton(sourceText("Choose Workbook…"), this);
    m_browseButton->setObjectName(QStringLiteral("classMngrNextScheduleImportBrowse"));
    fileRow->addWidget(m_filePathEdit, 1);
    fileRow->addWidget(m_browseButton);
    layout->addWidget(new QLabel(sourceText("Choose an .xlsx schedule workbook."), this));
    layout->addLayout(fileRow);

    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(sourceText("Schedule type:"), this));
    m_kindCombo = new QComboBox(this);
    m_kindCombo->setObjectName(QStringLiteral("classMngrNextScheduleImportKind"));
    m_kindCombo->addItem(sourceText("Regular"), static_cast<int>(ScheduleImportKind::Normal));
    m_kindCombo->addItem(sourceText("Intensives"), static_cast<int>(ScheduleImportKind::Intensive));
    modeRow->addWidget(m_kindCombo, 1);
    m_loadButton = new QPushButton(sourceText("Load Workbook"), this);
    m_loadButton->setObjectName(QStringLiteral("classMngrNextScheduleImportLoad"));
    modeRow->addWidget(m_loadButton);
    layout->addLayout(modeRow);

    m_statusLabel = new QLabel(sourceText("Select a schedule workbook to begin."), this);
    m_statusLabel->setObjectName(QStringLiteral("classMngrNextScheduleImportSourceStatus"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("classMngrNextScheduleImportProgress"));
    m_progressBar->setRange(0, 0);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    auto* selectionBox = new QGroupBox(sourceText("Schedule Source"), this);
    auto* selectionForm = new QFormLayout(selectionBox);
    m_sheetCombo = new QComboBox(selectionBox);
    m_sheetCombo->setObjectName(QStringLiteral("classMngrNextScheduleImportSheet"));
    m_userCombo = new QComboBox(selectionBox);
    m_userCombo->setObjectName(QStringLiteral("classMngrNextScheduleImportUser"));
    selectionForm->addRow(sourceText("Visible worksheet"), m_sheetCombo);
    selectionForm->addRow(sourceText("Detected user block"), m_userCombo);
    m_userStatusLabel = new QLabel(selectionBox);
    m_userStatusLabel->setWordWrap(true);
    selectionForm->addRow(m_userStatusLabel);
    m_updateProfileNameCheck = new QCheckBox(selectionBox);
    m_updateProfileNameCheck->setObjectName(
        QStringLiteral("classMngrNextScheduleImportProfileNameConfirmation")
        );
    selectionForm->addRow(m_updateProfileNameCheck);
    m_diagnosticsLabel = new QLabel(selectionBox);
    m_diagnosticsLabel->setObjectName(
        QStringLiteral("classMngrNextScheduleImportDiagnosticsSummary")
        );
    m_diagnosticsLabel->setWordWrap(true);
    selectionForm->addRow(m_diagnosticsLabel);
    selectionBox->setEnabled(false);
    layout->addWidget(selectionBox, 1);

    m_buttons = new QDialogButtonBox(this);
    m_nextButton = m_buttons->addButton(sourceText("Review Import"), QDialogButtonBox::AcceptRole);
    m_nextButton->setObjectName(QStringLiteral("classMngrNextScheduleImportReview"));
    auto* cancel = m_buttons->addButton(sourceText("Cancel Setup"), QDialogButtonBox::RejectRole);
    cancel->setObjectName(QStringLiteral("classMngrNextScheduleImportCancel"));
    layout->addWidget(m_buttons);

    connect(m_browseButton, &QPushButton::clicked, this,
            &InitialSetupScheduleImportDialog::browseForWorkbook);
    connect(m_loadButton, &QPushButton::clicked, this,
            &InitialSetupScheduleImportDialog::parseSelectedWorkbook);
    connect(m_kindCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this] {
                const bool hasSelectedFile = !m_filePathEdit->text().isEmpty();
                invalidateWorkbook();
                setStatus(hasSelectedFile
                    ? sourceText("The import type changed. Load the workbook again.")
                    : sourceText("Select a schedule workbook to begin."));
            });
    connect(m_sheetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this] { populateUsers(); });
    connect(m_userCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this] { updateSelectedUser(); });
    connect(m_updateProfileNameCheck, &QCheckBox::toggled, this,
            [this] { updateSourceControls(); });
    connect(m_nextButton, &QPushButton::clicked, this,
            [this] { openReview(); });
    connect(m_buttons, &QDialogButtonBox::rejected, this,
            &InitialSetupScheduleImportDialog::reject);
    updateSourceControls();
}

QString InitialSetupScheduleImportDialog::successSummary() const
{
    return m_successSummary;
}

void InitialSetupScheduleImportDialog::reject()
{
    if (!m_cancelSetupCalled)
    {
        m_cancelSetupCalled = true;
        if (m_actions.cancelSetup)
        {
            m_actions.cancelSetup();
        }
    }
    QDialog::reject();
}

void InitialSetupScheduleImportDialog::browseForWorkbook()
{
    if (!m_actions.chooseWorkbook)
    {
        setStatus(sourceText("The schedule workbook picker is unavailable."));
        return;
    }
    const auto selected = m_actions.chooseWorkbook();
    if (!selected)
    {
        setStatus(sourceText("The schedule workbook could not be selected: %1")
            .arg(utf8String(selected.error().message)));
        return;
    }
    if (!selected.value().has_value()) return;
    const QString filePath = *selected.value();
    m_filePathEdit->setText(filePath);
    invalidateWorkbook();
    parseSelectedWorkbook();
}

void InitialSetupScheduleImportDialog::invalidateWorkbook()
{
    ++m_parseRequestId;
    m_workbook.reset();
    m_loadedSize = -1;
    m_loadedModified = -1;
    m_visibleSheetIndexes.clear();
    m_sheetCombo->clear();
    m_userCombo->clear();
    m_diagnosticsLabel->clear();
    m_userStatusLabel->clear();
    m_updateProfileNameCheck->setChecked(false);
    updateSourceControls();
}

void InitialSetupScheduleImportDialog::parseSelectedWorkbook()
{
    const QString filePath = m_filePathEdit->text();
    if (filePath.isEmpty())
    {
        setStatus(sourceText("Choose an .xlsx schedule workbook."));
        return;
    }
    if (QFileInfo(filePath).suffix().compare(QStringLiteral("xlsx"), Qt::CaseInsensitive) != 0)
    {
        setStatus(sourceText("Schedule import accepts .xlsx workbooks only."));
        return;
    }
    const QFileInfo before(filePath);
    if (!before.exists() || !before.isFile())
    {
        setStatus(sourceText("The selected workbook could not be found."));
        return;
    }

    const quint64 requestId = ++m_parseRequestId;
    m_workbook.reset();
    setStatus(sourceText("Reading and validating the workbook…"));
    setLoading(true);
    const ScheduleImportKind kind = static_cast<ScheduleImportKind>(
        m_kindCombo->currentData().toInt()
        );

    auto* watcher = new QFutureWatcher<ParseResult>(this);
    connect(watcher, &QFutureWatcher<ParseResult>::finished, this, [this, watcher] {
        ParseResult result = watcher->result();
        watcher->deleteLater();
        finishParse(std::move(result));
    });
    const auto parser = m_actions.parseWorkbook;
    watcher->setFuture(QtConcurrent::run(
        [requestId, filePath, kind, parser, initialSize = before.size(),
         initialModified = before.lastModified().toMSecsSinceEpoch()]()
        {
            ParseResult result;
            result.requestId = requestId;
            result.path = filePath;
            result.kind = kind;
            result.initialSize = initialSize;
            result.initialModified = initialModified;
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly))
            {
                result.fileError = file.errorString();
                result.workbook = std::unexpected(file.errorString());
                return result;
            }
            result.workbook = parser
                ? parser(file.readAll(), kind)
                : parseScheduleImportWorkbook(file, kind);
            const QFileInfo after(filePath);
            result.finalSize = after.size();
            result.finalModified = after.lastModified().toMSecsSinceEpoch();
            if (result.initialSize != result.finalSize
                || result.initialModified != result.finalModified)
            {
                result.fileError = QStringLiteral(
                    "The workbook changed while it was being read. Load it again."
                    );
            }
            return result;
        }
        ));

    int timeout = 20000;
    if (m_actions.validationTimeoutMilliseconds)
    {
        timeout = m_actions.validationTimeoutMilliseconds();
    }
    QTimer::singleShot(qMax(1, timeout), this, [this, requestId] {
        if (m_loading && requestId == m_parseRequestId)
        {
            ++m_parseRequestId;
            setLoading(false);
            setStatus(sourceText("Workbook parsing timed out. Retry the import or choose another file."));
        }
    });
}

void InitialSetupScheduleImportDialog::finishParse(ParseResult result)
{
    if (result.requestId != m_parseRequestId
        || result.path != m_filePathEdit->text()
        || result.kind != static_cast<ScheduleImportKind>(m_kindCombo->currentData().toInt()))
    {
        return;
    }
    setLoading(false);
    if (!result.fileError.isEmpty())
    {
        setStatus(result.fileError);
        return;
    }
    if (!result.workbook)
    {
        setStatus(sourceText("The workbook could not be read: %1")
            .arg(result.workbook.error()));
        return;
    }
    m_workbook = std::move(*result.workbook);
    m_loadedSize = result.finalSize;
    m_loadedModified = result.finalModified;
    populateVisibleSheets();
    if (m_visibleSheetIndexes.isEmpty())
    {
        m_workbook.reset();
        setStatus(sourceText("The workbook has no visible worksheets to import."));
        return;
    }
    setStatus(sourceText("Workbook loaded. Choose a visible worksheet and detected user block."));
    populateUsers();
    updateSourceControls();
}

void InitialSetupScheduleImportDialog::populateVisibleSheets()
{
    m_visibleSheetIndexes.clear();
    m_sheetCombo->clear();
    if (!m_workbook) return;
    const auto visibleCount = std::count_if(
        m_workbook->sheets.begin(),
        m_workbook->sheets.end(),
        [](const auto& sheet) { return sheet.visible; }
        );
    if (visibleCount > 1)
    {
        m_sheetCombo->addItem(sourceText("Select a worksheet..."), -1);
    }
    for (qsizetype index = 0; index < m_workbook->sheets.size(); ++index)
    {
        const auto& sheet = m_workbook->sheets[index];
        if (!sheet.visible) continue;
        m_visibleSheetIndexes.append(static_cast<int>(index));
        m_sheetCombo->addItem(sheet.name, static_cast<int>(index));
    }
    if (visibleCount == 1)
    {
        m_sheetCombo->setCurrentIndex(m_sheetCombo->findData(m_visibleSheetIndexes.front()));
    }
    else if (visibleCount > 1)
    {
        m_sheetCombo->setCurrentIndex(0);
    }
}

void InitialSetupScheduleImportDialog::populateUsers()
{
    m_userCombo->clear();
    if (!m_workbook) return;
    const int sheetIndex = selectedIndex(m_sheetCombo);
    if (sheetIndex < 0 || sheetIndex >= m_workbook->sheets.size())
    {
        updateSelectedUser();
        return;
    }
    const ScheduleImportSheet& sheet = m_workbook->sheets[sheetIndex];
    m_userCombo->addItem(sourceText("Select a detected name..."), -1);
    const QString normalizedProfile = normalizedScheduleImportUserName(m_profileName);
    int matchingIndex = -1;
    int matchingCount = 0;
    for (qsizetype index = 0; index < sheet.users.size(); ++index)
    {
        if (!normalizedProfile.isEmpty()
            && normalizedScheduleImportUserName(sheet.users[index].name) == normalizedProfile)
        {
            matchingIndex = static_cast<int>(index);
            ++matchingCount;
        }
        m_userCombo->addItem(sheet.users[index].name, static_cast<int>(index));
    }
    m_userCombo->setCurrentIndex(matchingCount == 1
        ? m_userCombo->findData(matchingIndex)
        : 0);
    updateSelectedUser();
}

void InitialSetupScheduleImportDialog::updateSelectedUser()
{
    const int sheetIndex = selectedIndex(m_sheetCombo);
    const int userIndex = selectedIndex(m_userCombo);
    const ScheduleImportSheet* sheet = nullptr;
    const ScheduleImportUserBlock* user = nullptr;
    if (m_workbook && sheetIndex >= 0 && sheetIndex < m_workbook->sheets.size())
    {
        sheet = &m_workbook->sheets[sheetIndex];
        if (userIndex >= 0 && userIndex < sheet->users.size())
        {
            user = &sheet->users[userIndex];
        }
    }
    const bool profileBlank = m_profileName.trimmed().isEmpty();
    const bool mismatch = user && !profileBlank
        && normalizedScheduleImportUserName(user->name)
            != normalizedScheduleImportUserName(m_profileName);
    if (user && profileBlank)
    {
        m_userStatusLabel->setText(sourceText(
            "My Information has no name. You may save the selected workbook name after a successful import."
            ));
    }
    else if (mismatch)
    {
        m_userStatusLabel->setText(sourceText(
            "The selected name differs from My Information (%1). You may confirm a profile name update."
            ).arg(m_profileName));
    }
    else
    {
        m_userStatusLabel->clear();
    }
    m_userStatusLabel->setVisible(user != nullptr);
    m_updateProfileNameCheck->setVisible(user && (profileBlank || mismatch));
    m_updateProfileNameCheck->setText(profileBlank
        ? sourceText("Save the selected workbook name as the profile name after import.")
        : sourceText("Confirm updating the profile name to %1 after import.")
            .arg(user ? user->name : QString{}));
    if (!user || (!profileBlank && !mismatch))
    {
        m_updateProfileNameCheck->setChecked(false);
    }
    const int diagnosticCount = user
        ? user->diagnostics.size() + (sheet ? sheet->diagnostics.size() : 0)
        : (sheet ? sheet->diagnostics.size() : 0);
    m_diagnosticsLabel->setText(diagnosticCount == 0
        ? sourceText("No workbook diagnostics for this selection.")
        : sourceText("Workbook diagnostics to review: %1. Acknowledge them before applying.")
            .arg(diagnosticCount));
    m_diagnosticsLabel->setVisible(sheet != nullptr);
    updateSourceControls();
}

void InitialSetupScheduleImportDialog::updateSourceControls()
{
    const bool loaded = m_workbook.has_value();
    m_loadButton->setEnabled(!m_loading && !m_filePathEdit->text().isEmpty());
    m_browseButton->setEnabled(!m_loading);
    m_kindCombo->setEnabled(!m_loading);
    m_sheetCombo->parentWidget()->setEnabled(loaded && !m_loading);
    m_nextButton->setEnabled(
        loaded
        && !m_loading
        && selectedIndex(m_sheetCombo) >= 0
        && selectedIndex(m_userCombo) >= 0
        );
}

void InitialSetupScheduleImportDialog::setStatus(const QString& text)
{
    m_statusLabel->setText(text);
}

void InitialSetupScheduleImportDialog::setLoading(const bool loading)
{
    m_loading = loading;
    m_progressBar->setVisible(loading);
    m_loadButton->setText(loading ? sourceText("Loading…") : sourceText("Load Workbook"));
    updateSourceControls();
}

void InitialSetupScheduleImportDialog::openReview()
{
    const QFileInfo currentFile(m_filePathEdit->text());
    if (!m_workbook
        || currentFile.size() != m_loadedSize
        || currentFile.lastModified().toMSecsSinceEpoch() != m_loadedModified)
    {
        invalidateWorkbook();
        setStatus(sourceText(
            "The workbook changed after it was loaded. Load it again before reviewing."
            ));
        return;
    }
    const int sheetIndex = selectedIndex(m_sheetCombo);
    const int userIndex = selectedIndex(m_userCombo);
    if (!m_workbook || sheetIndex < 0 || sheetIndex >= m_workbook->sheets.size())
    {
        setStatus(sourceText("Choose a visible worksheet."));
        return;
    }
    const ScheduleImportSheet& sheet = m_workbook->sheets[sheetIndex];
    if (userIndex < 0 || userIndex >= sheet.users.size())
    {
        setStatus(sourceText("Choose a detected user block."));
        return;
    }
    ScheduleImportUserBlock user = sheet.users[userIndex];
    for (const ScheduleImportDiagnostic& diagnostic : sheet.diagnostics)
    {
        user.diagnostics.append(diagnostic);
    }

    const bool profileBlank = m_profileName.trimmed().isEmpty();
    const bool mismatch = !profileBlank
        && normalizedScheduleImportUserName(user.name)
            != normalizedScheduleImportUserName(m_profileName);
    const bool confirmUpdate = m_updateProfileNameCheck->isChecked();
    if (mismatch && confirmUpdate)
    {
        const auto choice = QMessageBox::question(
            this,
            sourceText("Confirm Profile Name Update"),
            sourceText("Update My Information from %1 to %2 after the import succeeds?")
                .arg(m_profileName, user.name),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
            );
        if (choice != QMessageBox::Yes) return;
    }
    if (!m_actions.readState)
    {
        setStatus(sourceText("Schedule import state is unavailable. Retry after reopening the profile."));
        return;
    }
    const auto state = m_actions.readState();
    const auto* snapshot = std::get_if<Application::ScheduleImportStateSnapshot>(&state);
    if (!snapshot)
    {
        const auto& failure = std::get<Application::ScheduleImportStateSnapshotFailure>(state);
        setStatus(sourceText("Current profile data could not be read: %1")
            .arg(utf8String(failure.message)));
        return;
    }
    const ScheduleImportKind kind = static_cast<ScheduleImportKind>(
        m_kindCombo->currentData().toInt()
        );
    auto mapping = mapScheduleImportReview(user, kind, *snapshot);
    if (!mapping)
    {
        setStatus(mapping.error());
        return;
    }
    ScheduleImportReviewDialog review(
        m_actions,
        std::move(*mapping),
        m_profileName,
        profileBlank && confirmUpdate,
        mismatch && confirmUpdate,
        this
        );
    if (review.exec() != QDialog::Accepted)
    {
        return;
    }
    m_successSummary = review.successSummary();
    accept();
}

} // namespace ClassMngr::Next::Ui
