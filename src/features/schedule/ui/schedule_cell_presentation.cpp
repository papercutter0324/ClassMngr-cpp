#include "schedule_cell_presentation.h"
#include "schedule_cell_renderer_policy.h"
#include "core/fontmanager.h"
#include <QCoreApplication>
#include <QTextDocument>

namespace
{
constexpr int PreviewFontSizeReduction = 4;
QString translate(const char* source)
{
    return QCoreApplication::translate("ScheduleWidget", source);
}

ScheduleCellPresentation classPresentation(
    const ScheduleEntry& entry,
    const ScheduleCellPresentationOptions& options
    )
{
    ScheduleCellPresentation result;
    result.font = FontManager::getUiFont();
    result.background = QColor(entry.classColor.isEmpty() ? QStringLiteral("#FFFFFF") : entry.classColor);
    result.foreground = QColor(entry.fontColor.isEmpty() ? QStringLiteral("#000000") : entry.fontColor);
    result.marker = entry.kind == ScheduleEntryKind::TestingClass;
    result.markerColor = result.foreground;

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
        result.accessibleText = accessibleParts.join(QStringLiteral(", "));

        result.html = (
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
        return result;
    }

    result.html = (
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

    return result;
}

ScheduleCellPresentation multiClassPresentation(
    const QList<ScheduleEntry>& entries,
    const ScheduleCellPresentationOptions& options
    )
{
    ScheduleCellPresentation result;
    result.font = FontManager::getUiFont();
    if (!entries.isEmpty())
    {
        result.background = QColor(entries.first().classColor.isEmpty() ? QStringLiteral("#FFFFFF") : entries.first().classColor);
        result.foreground = QColor(entries.first().fontColor.isEmpty() ? QStringLiteral("#000000") : entries.first().fontColor);
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

    result.html = html;

    return result;
}

}

ScheduleCellPresentation ScheduleCellPresentation::fromCell(
    const ScheduleCellView& cell,
    const ScheduleCellPresentationOptions& options)
{
    ScheduleCellPresentation result;
    result.font = FontManager::getUiFont();
    if (!cell.entries.isEmpty())
    {
        result = cell.entries.size() > 1
            ? multiClassPresentation(cell.entries, options)
            : classPresentation(cell.entries.first(), options);
        if (result.accessibleText.isEmpty())
        {
            QTextDocument document;
            document.setHtml(result.html);
            result.accessibleText = document.toPlainText();
        }
        return result;
    }
    const int reduction = options.compactPreview ? PreviewFontSizeReduction : 0;
    if (cell.slotState == scheduleEssaySlotState())
    {
        result.text = translate("Essay");
        result.background = Qt::white;
        result.foreground = Qt::black;
        result.font = FontManager::getUiFont(16 - reduction, QFont::Bold, true);
    }
    else if (cell.slotState == scheduleLunchSlotState())
    {
        result.text = translate("Lunch");
        result.background = QColor("#DCDCDC");
        result.foreground = Qt::black;
        result.font = FontManager::getUiFont(16 - reduction, QFont::Black, true);
    }
    else if (cell.slotState == scheduleTestingSlotState())
    {
        const QString room = cell.testingRoom.trimmed();
        result.text = room.isEmpty() ? translate("Oral Testing") : translate("Oral Testing\nRm: %1").arg(room);
        result.accessibleText = room.isEmpty() ? translate("Oral Testing") : translate("Oral Testing, room %1").arg(room);
        result.font = FontManager::getUiFont(14 - reduction, QFont::Bold, true);
        result.background = QColor(options.dark ? "#4B3D20" : "#FFF0B8");
        result.foreground = QColor(options.dark ? "#FFF2C2" : "#4A3500");
        result.border = QColor(options.dark ? "#8D7339" : "#D39B25");
        result.marker = true;
        result.markerColor = QColor(options.dark ? "#FFD166" : "#B66A00");
    }
    if (result.accessibleText.isEmpty())
        result.accessibleText = result.text;
    return result;
}
