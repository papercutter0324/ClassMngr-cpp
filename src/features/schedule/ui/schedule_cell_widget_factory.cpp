#include "schedule_cell_widget_factory.h"

#include "schedule_cell_renderer_policy.h"
#include "schedule_widget_delegates.h"

#include "core/fontmanager.h"
#include "ui/shared/styles/roles.h"

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QWidget>

namespace
{
using ScheduleWidgetDelegates::CornerMarkerLabel;

constexpr int PreviewFontSizeReduction = 4;

QString translate(
    const char* source
    )
{
    return QCoreApplication::translate(
        "ScheduleWidget",
        source
        );
}

QWidget* createScheduleLabel(
    const ScheduleEntry& entry,
    const ScheduleCellWidgetOptions& options
    )
{
    QLabel* label =
        entry.kind == ScheduleEntryKind::TestingClass
            ? static_cast<QLabel*>(
                new CornerMarkerLabel(options.parent)
                )
            : new QLabel(options.parent);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setProperty("role", UiRoles::ScheduleCell);
    label->setProperty("class_id", entry.classId);
    label->setProperty(
        "testing_class_assignment",
        entry.kind == ScheduleEntryKind::TestingClass
        );
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setStyleSheet(
        ScheduleCellRendererPolicy::classStyle(
            entry.classColor,
            entry.fontColor,
            options.compactPreview ? 2.4 : 3.2,
            options.compactPreview ? 3 : 4,
            options.compactPreview ? 5 : 6
            )
        );

    const QString teacherLine =
        scheduleTeacherRoomLine(
            entry,
            options.showKoreanTeacherEnglishNames
            );
    const QString englishLine =
        ScheduleCellRendererPolicy::englishLine(entry);

    if (entry.kind == ScheduleEntryKind::TestingClass)
    {
        const QString className =
            entry.className.trimmed();
        const QString roomLine =
            entry.roomNumber.trimmed();
        const QString markerColor =
            entry.fontColor.isEmpty()
                ? QStringLiteral("#000000")
                : entry.fontColor;
        QPalette markerPalette =
            label->palette();
        markerPalette.setColor(
            QPalette::Highlight,
            QColor(markerColor)
            );
        label->setPalette(markerPalette);

        QStringList accessibleParts;
        for (
            const QString& part :
            {className, englishLine, roomLine}
            )
        {
            if (!part.trimmed().isEmpty())
            {
                accessibleParts.append(part.trimmed());
            }
        }
        label->setAccessibleName(
            accessibleParts.join(QStringLiteral(", "))
            );

        FontManager::setManagedRichText(
            label,
            QStringLiteral(
                "<div style=\"text-align:center; line-height:0.92;\">"
                "<div style=\"color:%1; font-size:%2pt; font-weight:700;\">%3</div>"
                "<div style=\"color:%1; font-size:%4pt; font-weight:500;\">%5</div>"
                "<div style=\"color:%1; font-family:'%6'; font-size:%7pt; font-weight:400;\">%8</div>"
                "</div>"
                )
                .arg(
                    entry.fontColor.isEmpty()
                        ? QStringLiteral("#000000")
                        : entry.fontColor
                    )
                .arg(
                    FontManager::adjustedPointSize(
                        13
                        - (options.compactPreview
                               ? PreviewFontSizeReduction
                               : 0)
                        )
                    )
                .arg(ScheduleCellRendererPolicy::escaped(className))
                .arg(
                    FontManager::adjustedPointSize(
                        11
                        - (options.compactPreview
                               ? PreviewFontSizeReduction
                               : 0)
                        )
                    )
                .arg(ScheduleCellRendererPolicy::escaped(englishLine))
                .arg(
                    FontManager::getKoreanFont()
                        .family()
                        .toHtmlEscaped()
                    )
                .arg(
                    FontManager::adjustedPointSize(
                        10
                        - (options.compactPreview
                               ? PreviewFontSizeReduction
                               : 0)
                        )
                    )
                .arg(ScheduleCellRendererPolicy::escaped(roomLine))
            );
        return label;
    }

    FontManager::setManagedRichText(
        label,
        QStringLiteral(
            "<div style=\"text-align:center; line-height:1.00;\">"
            "<div style=\"color:%1; font-family:'%2'; font-size:%3pt; font-weight:600;\">%4</div>"
            "<div style=\"color:%1; font-size:%5pt; font-weight:400;\">%6</div>"
            "</div>"
            )
            .arg(
                entry.fontColor.isEmpty()
                    ? QStringLiteral("#000000")
                    : entry.fontColor
                )
            .arg(
                FontManager::getKoreanFont()
                    .family()
                    .toHtmlEscaped()
                )
            .arg(
                FontManager::getKoreanFont(
                    FontManager::stdKoreanFont
                    - (options.compactPreview
                           ? PreviewFontSizeReduction
                           : 0)
                    ).pointSize()
                )
            .arg(ScheduleCellRendererPolicy::escaped(teacherLine))
            .arg(
                FontManager::adjustedPointSize(
                    14
                    - (options.compactPreview
                           ? PreviewFontSizeReduction
                           : 0)
                    )
                )
            .arg(ScheduleCellRendererPolicy::escaped(englishLine))
        );

    return label;
}

QWidget* createMultiScheduleLabel(
    const QList<ScheduleEntry>& entries,
    const ScheduleCellWidgetOptions& options
    )
{
    auto* label =
        new QLabel(options.parent);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setProperty("role", UiRoles::ScheduleMulti);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (!entries.isEmpty())
    {
        label->setProperty(
            "class_id",
            entries.first().classId
            );
        label->setStyleSheet(
            ScheduleCellRendererPolicy::classStyle(
                entries.first().classColor,
                entries.first().fontColor,
                options.compactPreview ? 2.4 : 3.2,
                options.compactPreview ? 3 : 4,
                options.compactPreview ? 5 : 6
                )
            );
    }

    QString html;

    for (const ScheduleEntry& entry : entries)
    {
        const QString teacherLine =
            scheduleTeacherRoomLine(
                entry,
                options.showKoreanTeacherEnglishNames
                );
        const QString englishLine =
            ScheduleCellRendererPolicy::englishLine(entry);
        const QString fontColor =
            entry.fontColor.isEmpty()
                ? QStringLiteral("#000000")
                : entry.fontColor;

        html +=
            QStringLiteral(
                "<div style=\"margin-bottom:%7px; text-align:center; line-height:0.96;\">"
                "<div style=\"color:%1; font-family:'%2'; font-size:%3pt; font-weight:600;\">%4</div>"
                "<div style=\"color:%1; font-size:%5pt; font-weight:400;\">%6</div>"
                "</div>"
                )
                .arg(fontColor)
                .arg(
                    FontManager::getKoreanFont()
                        .family()
                        .toHtmlEscaped()
                    )
                .arg(
                    FontManager::getKoreanFont(
                        FontManager::stdKoreanFont
                        - (options.compactPreview
                               ? PreviewFontSizeReduction
                               : 0)
                        ).pointSize()
                    )
                .arg(ScheduleCellRendererPolicy::escaped(teacherLine))
                .arg(
                    FontManager::adjustedPointSize(
                        14
                        - (options.compactPreview
                               ? PreviewFontSizeReduction
                               : 0)
                        )
                    )
                .arg(ScheduleCellRendererPolicy::escaped(englishLine))
                .arg(options.compactPreview ? 4.8 : 6.4);
    }

    FontManager::setManagedRichText(
        label,
        html
        );

    return label;
}

QWidget* createSlotLabel(
    const ScheduleCellView& cell,
    const ScheduleCellWidgetOptions& options
    )
{
    QLabel* label =
        cell.slotState == scheduleTestingSlotState()
            ? static_cast<QLabel*>(
                new CornerMarkerLabel(options.parent)
                )
            : new QLabel(options.parent);

    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setProperty("role", UiRoles::ScheduleEmpty);
    label->setProperty("is_slot_cell", true);
    label->setProperty("day", cell.day);
    label->setProperty("time_label", cell.timeLabel);
    label->setProperty("default_slot_state", cell.defaultSlotState);
    label->setProperty("slot_state", cell.slotState);
    label->setProperty("slot_toggling_enabled", cell.slotTogglingEnabled);
    label->setProperty(
        "testing_block_creation_enabled",
        cell.testingBlockCreationEnabled
        );
    label->setProperty("testing_room", cell.testingRoom);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (cell.slotState == scheduleEssaySlotState())
    {
        label->setText(
            translate("Essay")
            );
        label->setFont(
            FontManager::getUiFont(
                16
                    - (options.compactPreview
                           ? PreviewFontSizeReduction
                           : 0),
                QFont::Bold,
                true
                )
            );
        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:white;"
                "color:black;"
                "border-radius:%1px;"
                "padding:%2px %3px;"
                "}"
                )
                .arg(options.compactPreview ? 5 : 6)
                .arg(options.compactPreview ? 3.2 : 4.8)
                .arg(options.compactPreview ? 4 : 6)
            );
    }
    else if (cell.slotState == scheduleTestingSlotState())
    {
        const QString room =
            cell.testingRoom.trimmed();
        label->setText(
            room.isEmpty()
                ? translate("Oral Testing")
                : translate("Oral Testing\nRm: %1").arg(room)
            );
        label->setAccessibleName(
            room.isEmpty()
                ? translate("Oral Testing")
                : translate("Oral Testing, room %1").arg(room)
            );
        label->setFont(
            FontManager::getUiFont(
                14
                    - (options.compactPreview
                           ? PreviewFontSizeReduction
                           : 0),
                QFont::Bold,
                true
                )
            );

        const QPalette sourcePalette =
            options.parent
                ? options.parent->palette()
                : label->palette();
        const bool dark =
            sourcePalette
                .color(QPalette::Window)
                .lightness() < 128;
        QPalette testingPalette =
            label->palette();
        testingPalette.setColor(
            QPalette::Highlight,
            dark
                ? QColor(QStringLiteral("#FFD166"))
                : QColor(QStringLiteral("#B66A00"))
            );
        label->setPalette(testingPalette);
        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:%1;"
                "color:%2;"
                "border:1px solid %3;"
                "border-radius:%4px;"
                "padding:%5px %6px;"
                "}"
                )
                .arg(
                    dark
                        ? QStringLiteral("#4B3D20")
                        : QStringLiteral("#FFF0B8"),
                    dark
                        ? QStringLiteral("#FFF2C2")
                        : QStringLiteral("#4A3500"),
                    dark
                        ? QStringLiteral("#8D7339")
                        : QStringLiteral("#D39B25")
                    )
                .arg(options.compactPreview ? 5 : 6)
                .arg(options.compactPreview ? 3.2 : 4.8)
                .arg(options.compactPreview ? 4 : 6)
            );
    }
    else if (cell.slotState == scheduleLunchSlotState())
    {
        label->setText(
            translate("Lunch")
            );
        label->setFont(
            FontManager::getUiFont(
                16
                    - (options.compactPreview
                           ? PreviewFontSizeReduction
                           : 0),
                QFont::Black,
                true
                )
            );
        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:#DCDCDC;"
                "color:black;"
                "border-radius:%1px;"
                "padding:%2px %3px;"
                "}"
                )
                .arg(options.compactPreview ? 5 : 6)
                .arg(options.compactPreview ? 3.2 : 4.8)
                .arg(options.compactPreview ? 4 : 6)
            );
    }
    else
    {
        label->clear();
        label->setStyleSheet(
            QStringLiteral(
                "QLabel {"
                "background:transparent;"
                "border:none;"
                "padding:0px;"
                "}"
                )
            );
    }

    return label;
}
}

QWidget* ScheduleCellWidgetFactory::create(
    const ScheduleCellView& cell,
    const ScheduleCellWidgetOptions& options
    )
{
    if (cell.entries.isEmpty())
    {
        return createSlotLabel(cell, options);
    }

    if (cell.entries.size() > 1)
    {
        return createMultiScheduleLabel(cell.entries, options);
    }

    QWidget* label =
        createScheduleLabel(cell.entries.first(), options);
    label->setProperty("day", cell.day);
    label->setProperty("time_label", cell.timeLabel);
    return label;
}
