#include "sub_prep_print_service.h"

#include "core/fontmanager.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QCoreApplication>
#include <QMarginsF>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QRectF>
#include <QSet>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>

namespace SubPrepPrintService
{
namespace
{
constexpr QPageSize::PageSizeId SubPrepPdfPageSize = QPageSize::A4;
constexpr qreal SubPrepPdfMarginInches = 0.5;
constexpr int SubPrepPdfResolutionDpi = 300;
constexpr qreal FooterHeightInches = 0.24;
constexpr qreal SubNotesLineSpacingPoints = 16.0;
constexpr qreal MajorSectionSpacingPoints = 21.0;
constexpr qreal ScheduleTimeColumnWidthPoints = 64.0;
constexpr int WeekdayScheduleColumnCount = 5;
constexpr int WeekendScheduleColumnCount = 7;

Result failed(
    const QString& message
    )
{
    return {
        Status::Failed,
        message
    };
}

Result canceled()
{
    return {
        Status::Canceled,
        QString()
    };
}

Result sent()
{
    return {
        Status::Sent,
        QObject::tr("Print job sent.")
    };
}

QString translate(
    const char* source
    )
{
    return QCoreApplication::translate(
        "SubPrepPage",
        source
        );
}

QString displayValue(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? QStringLiteral("N/A")
        : trimmed;
}

bool isNotAvailable(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        || trimmed.compare(
            QStringLiteral("N/A"),
            Qt::CaseInsensitive
            ) == 0;
}

bool hasZoomInformation(
    const SubPrepPrintService::Request& request
    )
{
    return !isNotAvailable(request.zoom.loginId)
        || !isNotAvailable(request.zoom.password);
}

QString htmlText(
    const QString& value
    )
{
    QString escaped =
        displayValue(value).toHtmlEscaped();
    escaped.replace(
        QStringLiteral("\n"),
        QStringLiteral("<br/>")
        );

    return escaped;
}

QString validColor(
    const QString& value,
    const QString& fallback
    )
{
    const QColor color(value);

    return color.isValid()
        ? color.name()
        : fallback;
}

QString classLine(
    const ScheduleEntry& entry
    )
{
    QStringList parts;

    if (!entry.classGrade.trimmed().isEmpty())
    {
        parts.append(entry.classGrade.trimmed());
    }

    if (!entry.classLevel.trimmed().isEmpty())
    {
        parts.append(entry.classLevel.trimmed());
    }

    return parts.join(QStringLiteral(" - "));
}

QString teacherLine(
    const ScheduleEntry& entry
    )
{
    return QStringLiteral("%1 %2")
        .arg(
            entry.teacherEn.trimmed(),
            entry.roomNumber.trimmed()
            )
        .simplified();
}

QString scheduleCellHtml(
    const ScheduleCellView& cell
    )
{
    if (cell.entries.isEmpty())
    {
        if (cell.slotState == scheduleEssaySlotState())
        {
            return QStringLiteral(
                "<div class=\"schedule-slot essay\">ESSAY</div>"
                );
        }

        if (cell.slotState == scheduleLunchSlotState())
        {
            return QStringLiteral(
                "<div class=\"schedule-slot lunch\">%1</div>"
                )
                .arg(htmlText(translate("Lunch")));
        }

        if (cell.slotState == scheduleTestingSlotState())
        {
            const QString room =
                cell.testingRoom.trimmed();
            const QString roomLine =
                room.isEmpty()
                    ? QString()
                    : QStringLiteral("<br/><span>%1</span>")
                        .arg(
                            htmlText(
                                translate("Rm: %1")
                                    .arg(room)
                                )
                            );

            return QStringLiteral(
                "<div class=\"schedule-slot testing\">"
                "<span class=\"testing-marker\">&#9701;</span> %1%2"
                "</div>"
                )
                .arg(
                    htmlText(translate("Testing")),
                    roomLine
                    );
        }

        return QString();
    }

    QString entries;

    for (const ScheduleEntry& entry : cell.entries)
    {
        entries +=
            QStringLiteral(
                "<div class=\"schedule-entry\" style=\"background:%1;color:%2;\">"
                "<strong>%3</strong><br/><span>%4</span></div>"
                )
                .arg(
                    validColor(
                        entry.classColor,
                        QStringLiteral("#ffffff")
                        ),
                    validColor(
                        entry.fontColor,
                        QStringLiteral("#000000")
                        ),
                    htmlText(teacherLine(entry)),
                    htmlText(classLine(entry))
                    );
    }

    return entries;
}

struct ScheduleTableLayout
{
    qreal tableWidthPercentage = 0.0;
    qreal timeColumnWidthPercentage = 0.0;
    qreal dayColumnWidthPercentage = 0.0;
};

QString factsHtml(
    const QList<QPair<QString, QString>>& facts,
    int columnCount = 2,
    bool useBorderlessEmptyCells = false,
    const QString& valueClass = QString(),
    const QSet<QString>& boldValueLabels = {}
    )
{
    columnCount = std::max(1, columnCount);
    const QString cellWidth =
        QString::number(100.0 / columnCount, 'f', 2);
    QString html = QStringLiteral(
        "<table class=\"facts\" width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">"
        );

    for (int index = 0; index < facts.size(); index += columnCount)
    {
        html += QStringLiteral("<tr>");

        for (int column = 0; column < columnCount; ++column)
        {
            const int factIndex = index + column;

            if (factIndex >= facts.size())
            {
                html += useBorderlessEmptyCells
                    ? QStringLiteral(
                        "<td class=\"empty-fact-cell\" width=\"%1%\"></td>"
                        ).arg(cellWidth)
                    : QStringLiteral(
                        "<td width=\"%1%\"></td>"
                        ).arg(cellWidth);
                continue;
            }

            const auto& fact = facts.at(factIndex);
            const QString valueHtml =
                boldValueLabels.contains(fact.first)
                    ? QStringLiteral("<strong>%1</strong>").arg(
                          htmlText(fact.second)
                          )
                    : htmlText(fact.second);
            html +=
                QStringLiteral(
                    "<td width=\"%1%\"><span class=\"field-label\">%2</span><div class=\"fact-value%3\">%4</div></td>"
                    )
                    .arg(
                        cellWidth,
                        htmlText(fact.first),
                        valueClass,
                        valueHtml
                        );
        }

        html += QStringLiteral("</tr>");
    }

    html += QStringLiteral("</table>");
    return html;
}

QString noteHtml(
    const QString& label,
    const QString& note,
    const QString& labelClass = QString(),
    const QString& noteClass = QString(),
    bool boldLabel = false
    )
{
    if (label.trimmed().isEmpty())
    {
        return QStringLiteral("<div class=\"note%1\">%2</div>")
            .arg(
                noteClass,
                htmlText(note)
                );
    }

    const QString labelHtml =
        boldLabel
            ? QStringLiteral("<strong>%1</strong>").arg(htmlText(label))
            : htmlText(label);

    return QStringLiteral(
        "<div class=\"note-label%1\">%2</div>"
        "<div class=\"note%3\">%4</div>"
        )
        .arg(
            labelClass,
            labelHtml,
            noteClass,
            htmlText(note)
            );
}

QString gradingNoteHtml(
    const QString& label,
    const QString& note
    )
{
    return noteHtml(
        label,
        note,
        QStringLiteral(" grading-note-label"),
        QStringLiteral(" compact-note"),
        true
        );
}

QString boldHtmlText(
    const QString& value
    )
{
    return QStringLiteral("<strong>%1</strong>")
        .arg(htmlText(value));
}

QString sectionHtml(
    const QString& heading,
    const QString& contents,
    bool startOnNewPage = false,
    bool useStandardTopSpacing = false
    )
{
    return QStringLiteral(
        "<div class=\"major-section%1%2\"><h2>%3</h2>%4</div>"
        )
        .arg(
            startOnNewPage
                ? QStringLiteral(" new-page")
                : QString(),
            useStandardTopSpacing
                ? QStringLiteral(" first-major-section")
                : QString(),
            boldHtmlText(heading),
            contents
            );
}

QString subsectionHtml(
    const QString& heading,
    const QString& contents,
    const QString& additionalClass = QString()
    )
{
    return QStringLiteral(
        "<div class=\"subsection%1\"><h3 class=\"subsection-heading\">%2</h3>%3</div>"
        )
        .arg(
            additionalClass.isEmpty()
                ? QString()
                : QStringLiteral(" %1").arg(additionalClass),
            boldHtmlText(heading),
            contents
            );
}

QString scheduleHtml(
    const ScheduleViewModel& schedule
    )
{
    if (schedule.rows.isEmpty() || schedule.days.isEmpty())
    {
        return QStringLiteral(
            "<div class=\"empty\">%1</div>"
            )
            .arg(
                htmlText(
                    translate("No registered class meeting times available.")
                    )
                );
    }

    QString html = QStringLiteral(
        "<table class=\"schedule\" align=\"center\" cellspacing=\"0\" cellpadding=\"0\">"
        "<tr><th>%1</th>"
        )
        .arg(htmlText(translate("Time")));

    for (const QString& day : schedule.days)
    {
        html += QStringLiteral("<th>%1</th>")
            .arg(htmlText(day));
    }

    html += QStringLiteral("</tr>");

    for (const ScheduleRowView& row : schedule.rows)
    {
        html += QStringLiteral("<tr><td class=\"schedule-time\">%1</td>")
            .arg(htmlText(row.timeRangeLabel));

        for (const ScheduleCellView& cell : row.cells)
        {
            html += QStringLiteral("<td>%1</td>")
                .arg(scheduleCellHtml(cell));
        }

        html += QStringLiteral("</tr>");
    }

    html += QStringLiteral("</table>");
    return html;
}

QString teacherHeadingHtml(
    const SubPrepClassInformation::TeacherGroup& group,
    const QString& borderColor
    )
{
    QString background =
        QStringLiteral("#e9f1f5");
    QString foreground =
        QStringLiteral("#23495c");

    if (!group.classes.isEmpty())
    {
        const SubPrepClassInformation::ClassDetails& firstClass =
            group.classes.first();
        background =
            validColor(
                firstClass.info.classColor,
                background
                );
        foreground =
            validColor(
                firstClass.info.fontColor,
                foreground
                );
    }

    return QStringLiteral(
        "<tr><td class=\"teacher-heading\" "
        "style=\"background:%1;border:1px solid %2;color:%3;\">%4</td></tr>"
        )
        .arg(
            background,
            borderColor,
            foreground,
            htmlText(group.teacher.teacherEn)
            );
}

QString coTeacherNotesHtml(
    const QString& note
    )
{
    return QStringLiteral(
        "<div class=\"co-teacher-notes\">"
        "<div class=\"note-label\"><strong>%1</strong></div>"
        "<div class=\"note co-teacher-note\">%2</div>"
        "</div>"
        )
        .arg(
            htmlText(translate("Co-Teacher Notes")),
            htmlText(note)
            );
}

QString subNotesHtml()
{
    return QStringLiteral(
        "<div class=\"sub-notes-prompt\">%1</div>"
        )
        .arg(
            htmlText(
                translate(
                    "If there is anything important that you want me to know, "
                    "please leave me some notes."
                    )
                )
            );
}

QString teacherBorderColor(
    const SubPrepClassInformation::TeacherGroup& group
    )
{
    if (group.classes.isEmpty())
    {
        return QStringLiteral("#3f7c96");
    }

    return validColor(
        group.classes.first().info.classColor,
        QStringLiteral("#3f7c96")
        );
}

QString classInformationHtml(
    const QList<SubPrepClassInformation::TeacherGroup>& groups,
    const QSet<int>& pageBreakBeforeGroups = {}
    )
{
    if (groups.isEmpty())
    {
        return QStringLiteral(
            "<div class=\"empty\">%1</div>"
            )
            .arg(
                htmlText(
                    translate("No scheduled class information available.")
                    )
                );
    }

    QString html;

    for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        const SubPrepClassInformation::TeacherGroup& group =
            groups.at(groupIndex);
        const QString borderColor =
            teacherBorderColor(group);
        html += QStringLiteral(
            "<div class=\"teacher-card-wrapper%1\">"
            )
            .arg(
                pageBreakBeforeGroups.contains(groupIndex)
                    ? QStringLiteral(" new-page")
                    : QString()
                );
        html += QStringLiteral(
            "<table class=\"teacher-card\" width=\"100%\" "
            "cellspacing=\"0\" cellpadding=\"0\">"
            );
        html += teacherHeadingHtml(group, borderColor);
        html += QStringLiteral(
            "<tr><td class=\"teacher-card-content\" "
            "style=\"border:1px solid %1;border-top:0;\">"
            )
            .arg(borderColor);

        html += factsHtml(
            {
                {
                    translate("English Name"),
                    group.teacher.teacherEn
                },
                {
                    translate("Room"),
                    group.teacher.roomNumber
                },
                {
                    translate("WiFi Name"),
                    group.teacher.wifiName
                },
                {
                    translate("Zoom ID"),
                    group.teacher.zoomId
                },
                {
                    translate("Internet Type"),
                    group.teacher.internetType
                },
                {
                    translate("Projection Type"),
                    group.teacher.projectionType
                },
                {
                    translate("WiFi Password"),
                    group.teacher.wifiPassword
                },
                {
                    translate("Zoom Password"),
                    group.teacher.zoomPassword
                }
            },
            4,
            false,
            QStringLiteral(" teacher-fact-value"),
            QSet<QString>{translate("English Name")}
            );

        html += QStringLiteral(
            "<div class=\"teacher-class-list\">"
            "<div class=\"class-list-heading\">%1</div>"
            "<ul class=\"class-list\">"
            )
            .arg(htmlText(translate("Classes")));

        for (const SubPrepClassInformation::ClassDetails& details : group.classes)
        {
            const QString classLabel =
                displayValue(details.classLabel);
            html += QStringLiteral(
                "<li><span class=\"class-list-item\">%1 (%2) - %3</span>"
                )
                .arg(
                    htmlText(classLabel),
                    htmlText(displayValue(details.timeText)),
                    htmlText(
                        translate("%1 Students")
                            .arg(details.studentCount)
                        )
                    );

            if (!isNotAvailable(details.info.notes))
            {
                html += QStringLiteral(
                    "<div class=\"class-list-note\">"
                    "<span><strong>%1:</strong></span>&nbsp;%2</div>"
                    )
                    .arg(
                        htmlText(translate("Notes")),
                        htmlText(details.info.notes)
                        );
            }

            html += QStringLiteral("</li>");
        }

        html += QStringLiteral("</ul></div>");

        if (!isNotAvailable(group.teacher.notes))
        {
            html += coTeacherNotesHtml(
                group.teacher.notes
                );
        }
        html += QStringLiteral("</td></tr></table>");
        html += QStringLiteral("</div>");

        if (groupIndex + 1 < groups.size())
        {
            html += QStringLiteral(
                "<p class=\"teacher-section-spacer\">&nbsp;</p>"
                );
        }
    }

    return html;
}

QString documentHtml(
    const Request& request,
    const QSet<int>& pageBreakBeforeTeacherGroups = {},
    bool startSubNotesOnNewPage = false
    )
{
    QString html = QStringLiteral(
        "<html><body>"
        "<div class=\"document-title\">"
        "<h1><strong>%1</strong></h1>"
        "<div class=\"subtitle\">%2</div>"
        "</div>"
        )
        .arg(
            htmlText(translate("Sub Prep")),
            htmlText(translate("Thank you for subbing for me!"))
            );

    html += sectionHtml(
        translate("Important Information"),
        subsectionHtml(
            translate("Campus Information"),
            factsHtml(
                {
                    {
                        translate("Office Number"),
                        request.campus.officeNumber
                    },
                    {
                        translate("Office WiFi"),
                        request.campus.officeWifi
                    },
                    {
                        translate("WiFi Password"),
                        request.campus.officeWifiPassword
                    },
                    {
                        translate("Photocopier Code"),
                        request.campus.photocopierCode
                    }
                },
                4,
                false,
                QStringLiteral(" campus-fact-value")
                )
            )
        + (hasZoomInformation(request)
               ? subsectionHtml(
                     translate("Personal Zoom Information"),
                     factsHtml(
                         {
                             {
                                 translate("Zoom Login ID"),
                                 request.zoom.loginId
                             },
                             {
                                 translate("Zoom Password"),
                                 request.zoom.password
                             }
                         }
                         )
                     )
               : QString())
        + subsectionHtml(
            translate("Class Materials"),
            noteHtml(
                QString(),
                request.classMaterials,
                QString(),
                QStringLiteral(" compact-note")
                ),
            QStringLiteral("class-materials-subsection")
            )
        + subsectionHtml(
            translate("Book Report Grading"),
            gradingNoteHtml(
                translate("Grading Instructions"),
                request.gradingInstructions
                )
            + gradingNoteHtml(
                translate("Special Instructions"),
                request.specialInstructions
                )
            ),
        false,
        true
        );

    html += sectionHtml(
        translate("Schedule"),
        scheduleHtml(request.schedule)
        );
    html += sectionHtml(
        translate("Class Information"),
        classInformationHtml(
            request.classInformation,
            pageBreakBeforeTeacherGroups
            ),
        !request.classInformation.isEmpty()
        );
    html += sectionHtml(
        translate("Sub Notes"),
        subNotesHtml(),
        startSubNotesOnNewPage
        );

    html += QStringLiteral("</body></html>");
    return html;
}

QString documentStyleSheet()
{
    const QString family =
        FontManager::getUiFont(10).family().toHtmlEscaped();

    return QStringLiteral(
        "html, body { margin:0; padding:0; width:100%; }"
        "body { color:#000000; font-family:'%1'; font-size:10.5pt; }"
        ".document-title { border-bottom:2px solid #2f657d; margin:0 0 15px 0; padding:0 0 10px 0; text-align:center; }"
        "h1 { color:#000000; font-size:23pt; font-weight:800; margin:0 0 3px 0; }"
        ".subtitle { color:#000000; font-size:11pt; margin:0; }"
        ".major-section { margin:0 0 28px 0; width:100%; }"
        ".new-page { page-break-before:always; }"
        "h2 { background:#244b5f; border:1px solid #244b5f; color:#ffffff; "
        "border-top-width:2px; font-size:14pt; font-weight:800; margin:28px 0 9px 0; padding:7px 9px; text-align:center; "
        "page-break-after:avoid; }"
        ".first-major-section h2 { margin-top:14px; }"
        ".subsection { margin:0 0 11px 0; width:100%; }"
        ".class-materials-subsection { margin-bottom:27px; }"
        ".subsection-heading { background:#e9f1f5; border-left:4px solid #3f7c96; "
        "color:#000000; font-size:12pt; font-weight:800; margin:9px 0 6px 0; padding:5px 8px; "
        "page-break-after:avoid; }"
        ".teacher-heading { background:#e9f1f5; border:1px solid #3f7c96; "
        "color:#23495c; font-size:12pt; font-weight:700; padding:7px 9px; }"
        ".facts { border-collapse:collapse; margin:0 0 8px 0; table-layout:fixed; width:100%; }"
        ".facts td { background:#ffffff; border:1px solid #bdcbd2; padding:7px 9px; vertical-align:top; }"
        ".facts td.empty-fact-cell { background:transparent; border:0; padding:0; }"
        ".campus-fact-value, .teacher-fact-value { text-align:center; }"
        ".field-label, .note-label { color:#000000; font-size:9pt; font-weight:700; }"
        ".note-label { margin:8px 0 3px 0; }"
        ".note { background:#f8fbfc; border:1px solid #bdcbd2; border-left:3px solid #7aa3b5; "
        "padding:8px 9px; }"
        ".compact-note { font-size:9pt; }"
        ".grading-note-label { font-weight:700; text-decoration:underline; }"
        ".teacher-card { border-collapse:collapse; margin:0; page-break-inside:avoid; width:100%; }"
        ".teacher-card-content { border:1px solid #9fb3bd; border-top:0; padding:9px; vertical-align:top; }"
        ".teacher-card-wrapper { margin:0; padding:0; width:100%; }"
        ".teacher-section-spacer { font-size:6pt; margin:0 0 8px 0; padding:0; }"
        ".teacher-class-list { border-top:1px solid #d1dce1; margin:9px 0 0 0; padding:7px 0 0 0; }"
        ".class-list-heading { color:#000000; font-size:11pt; font-weight:700; margin:0 0 3px 0; }"
        ".class-list { margin:0; padding:0 0 0 19px; }"
        ".class-list li { margin:4px 0; padding:0; }"
        ".class-list-item { font-weight:700; }"
        ".class-list-note { color:#000000; font-size:9.5pt; margin:2px 0 0 4em; }"
        ".class-list-note span { color:#000000; font-weight:700; }"
        ".co-teacher-notes { margin:9px 0 0 0; }"
        ".co-teacher-notes .note-label { margin:0 0 3px 0; }"
        ".co-teacher-note { margin-left:4em; }"
        ".sub-notes-prompt { margin:0; }"
        ".schedule { border:1px solid #000000; border-collapse:collapse; font-size:8.5pt; margin:0 0 8px 0; "
        "page-break-inside:avoid; table-layout:fixed; }"
        ".schedule th { background:#244b5f; border:1px solid #000000; color:#ffffff; "
        "font-weight:700; padding:7px 4px; text-align:center; }"
        ".schedule td { border:1px solid #000000; padding:5px 4px; text-align:center; vertical-align:middle; }"
        ".schedule-time { background:#e9f1f5; color:#000000; font-weight:700; }"
        ".schedule-entry { margin:1px 0; padding:4px 3px; }"
        ".schedule-slot { font-weight:700; padding:6px 3px; }"
        ".essay { background:#ffffff; color:#000000; font-size:12pt; font-style:normal; font-weight:700; }"
        ".lunch { background:#dcdcdc; color:#000000; }"
        ".testing { background:#fff0b8; color:#4a3500; border:1px solid #d39b25; }"
        ".testing-marker { color:#b66a00; font-size:13pt; }"
        ".empty { background:#f8fbfc; border:1px solid #d1dce1; color:#000000; "
        "font-style:italic; padding:9px; }"
        )
        .arg(family);
}

void collectTeacherCardTables(
    QTextFrame* frame,
    QList<QTextTable*>& cards
    )
{
    for (QTextFrame* childFrame : frame->childFrames())
    {
        QTextTable* table =
            dynamic_cast<QTextTable*>(childFrame);

        if (table && table->rows() == 2 && table->columns() == 1)
        {
            cards.append(table);
        }

        collectTeacherCardTables(childFrame, cards);
    }
}

QList<QTextTable*> teacherCardTables(
    QTextDocument& document
    )
{
    QList<QTextTable*> cards;
    collectTeacherCardTables(document.rootFrame(), cards);

    std::sort(
        cards.begin(),
        cards.end(),
        [](const QTextTable* left, const QTextTable* right)
        {
            return left->firstPosition() < right->firstPosition();
        }
        );

    return cards;
}

QSet<int> teacherSectionsThatSpanPages(
    QTextDocument& document,
    qreal pageHeight
    )
{
    QSet<int> groups;

    if (pageHeight <= 0.0)
    {
        return groups;
    }

    const QList<QTextTable*> cards =
        teacherCardTables(document);

    for (int index = 0; index < cards.size(); ++index)
    {
        const QRectF bounds =
            document.documentLayout()->frameBoundingRect(cards.at(index));

        if (bounds.isEmpty() || bounds.height() >= pageHeight)
        {
            continue;
        }

        const int firstPage =
            static_cast<int>(std::floor(bounds.top() / pageHeight));
        const int lastPage =
            static_cast<int>(std::floor(
                (bounds.bottom() - 0.01) / pageHeight
                ));

        if (firstPage != lastPage)
        {
            groups.insert(index);
        }
    }

    return groups;
}

void collectTables(
    QTextFrame* frame,
    QList<QTextTable*>& tables
    )
{
    for (QTextFrame* childFrame : frame->childFrames())
    {
        if (auto* table = dynamic_cast<QTextTable*>(childFrame))
        {
            tables.append(table);
        }

        collectTables(childFrame, tables);
    }
}

void applyScheduleTableLayout(
    QTextDocument& document,
    const ScheduleViewModel& schedule,
    const ScheduleTableLayout& layout
    )
{
    if (schedule.rows.isEmpty() || schedule.days.isEmpty())
    {
        return;
    }

    QList<QTextTable*> tables;
    collectTables(document.rootFrame(), tables);

    const int expectedRowCount = schedule.rows.size() + 1;
    const int expectedColumnCount = schedule.days.size() + 1;
    const auto table = std::find_if(
        tables.cbegin(),
        tables.cend(),
        [expectedRowCount, expectedColumnCount](const QTextTable* candidate)
        {
            return candidate
                && candidate->rows() == expectedRowCount
                && candidate->columns() == expectedColumnCount;
        }
        );

    if (table == tables.cend())
    {
        return;
    }

    QList<QTextLength> columnWidths;
    columnWidths.append(
        QTextLength(
            QTextLength::PercentageLength,
            layout.timeColumnWidthPercentage
            )
        );

    for (int index = 0; index < schedule.days.size(); ++index)
    {
        columnWidths.append(
            QTextLength(
                QTextLength::PercentageLength,
                layout.dayColumnWidthPercentage
                )
            );
    }

    QTextTableFormat format = (*table)->format();
    format.setWidth(
        QTextLength(
            QTextLength::PercentageLength,
            layout.tableWidthPercentage
            )
        );
    format.setColumnWidthConstraints(columnWidths);
    format.setLeftMargin(0.0);
    format.setRightMargin(0.0);
    format.setAlignment(Qt::AlignHCenter);
    (*table)->setFormat(format);
}

std::optional<QRectF> textBlockBounds(
    QTextDocument& document,
    const QString& text
    )
{
    const QTextCursor cursor =
        document.find(text);

    if (cursor.isNull())
    {
        return std::nullopt;
    }

    const QRectF bounds =
        document.documentLayout()->blockBoundingRect(
            cursor.block()
            );

    return bounds.isEmpty()
        ? std::nullopt
        : std::optional<QRectF>(bounds);
}

std::optional<qreal> contentBottomBeforeText(
    QTextDocument& document,
    const QString& text
    )
{
    const QTextCursor cursor = document.find(text);

    if (cursor.isNull())
    {
        return std::nullopt;
    }

    qreal contentBottom = 0.0;
    bool foundContent = false;

    for (
        QTextBlock block = document.begin();
        block.isValid() && block.position() < cursor.selectionStart();
        block = block.next()
        )
    {
        const QRectF bounds =
            document.documentLayout()->blockBoundingRect(block);

        if (bounds.isEmpty())
        {
            continue;
        }

        contentBottom = std::max(contentBottom, bounds.bottom());
        foundContent = true;
    }

    return foundContent
        ? std::optional<qreal>(contentBottom)
        : std::nullopt;
}

bool shouldStartSubNotesOnNewPage(
    QTextDocument& document,
    qreal pageHeight
    )
{
    if (pageHeight <= 0.0)
    {
        return false;
    }

    const std::optional<QRectF> headingBounds =
        textBlockBounds(
            document,
            translate("Sub Notes")
            );

    if (!headingBounds)
    {
        return false;
    }

    const int pageIndex =
        std::max(
            0,
            static_cast<int>(std::floor(
                (headingBounds->bottom() - 0.01) / pageHeight
                ))
            );
    const qreal availableBelowHeading =
        ((pageIndex + 1) * pageHeight) - headingBounds->bottom();

    return availableBelowHeading < (pageHeight / 3.0);
}

QPageLayout pageLayout()
{
    return QPageLayout(
        QPageSize(SubPrepPdfPageSize),
        QPageLayout::Portrait,
        QMarginsF(),
        QPageLayout::Inch
        );
}

QRectF pageRect(
    const QPdfWriter& writer
    )
{
    const QRect rect =
        writer.pageLayout().fullRectPixels(
            std::max(1, writer.resolution())
            );

    if (rect.width() > 0 && rect.height() > 0)
    {
        return QRectF(rect);
    }

    return QRectF(
        0.0,
        0.0,
        writer.width(),
        writer.height()
        );
}

QRectF contentRect(
    const QRectF& page,
    int resolutionDpi
    )
{
    const qreal margin =
        SubPrepPdfMarginInches
        * std::max(1, resolutionDpi);

    return page.adjusted(
        margin,
        margin,
        -margin,
        -margin
        );
}

qreal pixelsForInches(
    qreal inches,
    int resolutionDpi
    )
{
    return inches * std::max(1, resolutionDpi);
}

qreal pixelsForPoints(
    qreal points,
    int resolutionDpi
    )
{
    return points * std::max(1, resolutionDpi) / 72.0;
}

ScheduleTableLayout scheduleTableLayout(
    qreal documentWidthPixels,
    int resolutionDpi,
    const ScheduleViewModel& schedule
    )
{
    const qreal documentWidth =
        documentWidthPixels * 72.0
        / std::max(1, resolutionDpi);
    const qreal availableDayWidth =
        std::max(
            0.0,
            documentWidth - ScheduleTimeColumnWidthPoints
            );
    const bool showsWeekends =
        schedule.days.contains(QStringLiteral("Saturday"))
        || schedule.days.contains(QStringLiteral("Sunday"));
    const int dayColumnCount =
        showsWeekends
            ? WeekendScheduleColumnCount
            : WeekdayScheduleColumnCount;

    const qreal dayColumnWidth =
        availableDayWidth / dayColumnCount;
    const qreal tableWidth =
        ScheduleTimeColumnWidthPoints
        + (dayColumnWidth * schedule.days.size());

    if (documentWidth <= 0.0 || tableWidth <= 0.0)
    {
        return {};
    }

    // Keep QTextDocument's table constraints unitless: the table percentage
    // is relative to the page body, while its columns are relative to it.
    return {
        (tableWidth / documentWidth) * 100.0,
        (ScheduleTimeColumnWidthPoints / tableWidth) * 100.0,
        (dayColumnWidth / tableWidth) * 100.0
    };
}

std::optional<qreal> subNotesWritingStart(
    QTextDocument& document,
    int resolutionDpi
    )
{
    const std::optional<QRectF> promptBounds =
        textBlockBounds(
            document,
            translate(
                "If there is anything important that you want me to know, "
                "please leave me some notes."
                )
            );

    if (!promptBounds)
    {
        return std::nullopt;
    }

    return promptBounds->bottom()
        + pixelsForPoints(
            SubNotesLineSpacingPoints,
            resolutionDpi
            );
}

void drawSubNotesWritingLines(
    QPainter& painter,
    const QRectF& body,
    qreal writingStart,
    qreal lineSpacing,
    int pageIndex
    )
{
    if (lineSpacing <= 0.0)
    {
        return;
    }

    const qreal documentPageTop =
        pageIndex * body.height();
    const qreal documentPageBottom =
        documentPageTop + body.height();

    if (
        writingStart < documentPageTop
        || writingStart >= documentPageBottom
        )
    {
        return;
    }

    painter.save();
    QPen pen(Qt::black);
    pen.setWidthF(1.75);
    painter.setPen(pen);

    for (
        qreal lineY = writingStart;
        lineY < documentPageBottom;
        lineY += lineSpacing
        )
    {
        const qreal pageY =
            body.top() + (lineY - documentPageTop);
        painter.drawLine(
            QPointF(body.left(), pageY),
            QPointF(body.right(), pageY)
            );
    }

    painter.restore();
}

void drawPageFooter(
    QPainter& painter,
    const QRectF& content,
    qreal footerHeight,
    int pageNumber
    )
{
    QFont pageFont =
        FontManager::getUiFont(-1);
    pageFont.setPointSizeF(8.5);

    painter.save();
    painter.setPen(Qt::black);
    painter.setFont(pageFont);
    painter.drawText(
        QRectF(
            content.left(),
            content.bottom() - footerHeight + 8.0,
            content.width(),
            footerHeight - 8.0
            ),
        Qt::AlignRight | Qt::AlignVCenter,
        QObject::tr("Page %1").arg(pageNumber)
        );
    painter.restore();
}
}

Result saveSubPrepPdf(
    const Request& request,
    const QString& documentPath
    )
{
    if (documentPath.trimmed().isEmpty())
    {
        return failed(
            QObject::tr("No sub prep print file path was provided.")
            );
    }

    QPdfWriter writer(documentPath);
    writer.setCreator(QStringLiteral("ClassMngr"));
    writer.setTitle(translate("Sub Prep"));
    writer.setResolution(SubPrepPdfResolutionDpi);

    if (!writer.setPageLayout(pageLayout()))
    {
        return failed(
            QObject::tr("Unable to configure the sub prep print file.")
            );
    }

    QPainter painter;
    if (!painter.begin(&writer))
    {
        return failed(
            QObject::tr("Unable to create the sub prep print file.")
            );
    }

    const QRectF page = pageRect(writer);
    const QRectF content = contentRect(page, writer.resolution());
    const qreal footerHeight =
        pixelsForInches(
            FooterHeightInches,
            writer.resolution()
            );
    const QRectF body(
        content.left(),
        content.top(),
        content.width(),
        content.height() - footerHeight
        );

    if (
        page.isEmpty()
        || content.isEmpty()
        || body.width() <= 0.0
        || body.height() <= 0.0
        )
    {
        painter.end();
        return failed(
            QObject::tr("Unable to determine the sub prep print area.")
            );
    }

    QTextDocument document;
    document.documentLayout()->setPaintDevice(&writer);
    QFont documentFont = FontManager::getUiFont(10);
    documentFont.setPointSizeF(10.5);
    document.setDefaultFont(documentFont);
    document.setDefaultStyleSheet(documentStyleSheet());
    document.setDocumentMargin(0.0);
    document.setPageSize(body.size());
    const ScheduleTableLayout scheduleLayout =
        scheduleTableLayout(
            body.width(),
            writer.resolution(),
            request.schedule
            );

    QSet<int> pageBreakBeforeTeacherGroups;
    bool startSubNotesOnNewPage = false;

    for (int pass = 0; pass <= request.classInformation.size(); ++pass)
    {
        document.setHtml(
            documentHtml(
                request,
                pageBreakBeforeTeacherGroups,
                startSubNotesOnNewPage
                )
            );
        applyScheduleTableLayout(
            document,
            request.schedule,
            scheduleLayout
            );

        const QSet<int> spanningGroups =
            teacherSectionsThatSpanPages(document, body.height());
        const QSet<int> newPageBreaks =
            spanningGroups - pageBreakBeforeTeacherGroups;
        const bool moveSubNotesToNewPage =
            !startSubNotesOnNewPage
            && shouldStartSubNotesOnNewPage(
                document,
                body.height()
                );

        if (newPageBreaks.isEmpty() && !moveSubNotesToNewPage)
        {
            break;
        }

        pageBreakBeforeTeacherGroups.unite(newPageBreaks);
        startSubNotesOnNewPage =
            startSubNotesOnNewPage || moveSubNotesToNewPage;
    }

    document.setHtml(
        documentHtml(
            request,
            pageBreakBeforeTeacherGroups,
            startSubNotesOnNewPage
            )
        );
    applyScheduleTableLayout(
        document,
        request.schedule,
        scheduleLayout
        );
    const std::optional<qreal> subNotesLineStart =
        subNotesWritingStart(
            document,
            writer.resolution()
            );
    const qreal subNotesLineSpacing =
        pixelsForPoints(
            SubNotesLineSpacingPoints,
            writer.resolution()
            );

    QTextDocument fallbackSubNotesDocument;
    std::optional<qreal> fallbackSubNotesLineStart;
    const bool appendFallbackSubNotesPage =
        !subNotesLineStart.has_value();

    if (appendFallbackSubNotesPage)
    {
        fallbackSubNotesDocument.documentLayout()->setPaintDevice(
            &writer
            );
        fallbackSubNotesDocument.setDefaultFont(documentFont);
        fallbackSubNotesDocument.setDefaultStyleSheet(
            documentStyleSheet()
            );
        fallbackSubNotesDocument.setDocumentMargin(0.0);
        fallbackSubNotesDocument.setPageSize(body.size());
        fallbackSubNotesDocument.setHtml(
            QStringLiteral("<html><body>")
            + sectionHtml(
                translate("Sub Notes"),
                subNotesHtml()
                )
            + QStringLiteral("</body></html>")
            );
        fallbackSubNotesLineStart =
            subNotesWritingStart(
                fallbackSubNotesDocument,
                writer.resolution()
                );
    }

    const QSizeF documentSize =
        document.documentLayout()->documentSize();
    const int subNotesPageCount =
        subNotesLineStart
            ? static_cast<int>(
                  std::floor(*subNotesLineStart / body.height())
                  ) + 1
            : 1;
    const int documentPageCount =
        std::max(
            document.pageCount(),
            std::max(
                subNotesPageCount,
                std::max(
                    1,
                    static_cast<int>(
                        std::ceil(documentSize.height() / body.height())
                        )
                    )
                )
            );
    bool placeFallbackSubNotesOnLastPage = false;
    qreal fallbackSubNotesTop = 0.0;

    if (appendFallbackSubNotesPage)
    {
        const std::optional<qreal> contentBottom =
            contentBottomBeforeText(
                document,
                translate("Sub Notes")
                );

        if (contentBottom)
        {
            const int contentPageIndex =
                std::max(
                    0,
                    static_cast<int>(std::floor(
                        (*contentBottom - 0.01) / body.height()
                        ))
                    );
            const qreal contentBottomOnPage =
                *contentBottom
                - (contentPageIndex * body.height());
            const qreal availableHeight =
                body.height() - contentBottomOnPage;

            if (
                contentPageIndex == documentPageCount - 1
                && availableHeight >= (body.height() / 2.0)
                )
            {
                placeFallbackSubNotesOnLastPage = true;
                fallbackSubNotesTop =
                    contentBottomOnPage
                    + pixelsForPoints(
                        MajorSectionSpacingPoints,
                        writer.resolution()
                        );
            }
        }
    }

    const int pageCount =
        documentPageCount
        + (
            appendFallbackSubNotesPage
            && !placeFallbackSubNotesOnLastPage
                ? 1
                : 0
            );

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const bool isFallbackSubNotesPage =
            appendFallbackSubNotesPage
            && !placeFallbackSubNotesOnLastPage
            && pageIndex == documentPageCount;
        const bool drawsFallbackSubNotesOnThisPage =
            placeFallbackSubNotesOnLastPage
            && pageIndex == documentPageCount - 1;
        QTextDocument& pageDocument =
            isFallbackSubNotesPage
                ? fallbackSubNotesDocument
                : document;
        const int documentPageIndex =
            isFallbackSubNotesPage
                ? 0
                : pageIndex;
        const std::optional<qreal>& writingLineStart =
            isFallbackSubNotesPage
                ? fallbackSubNotesLineStart
                : subNotesLineStart;

        painter.fillRect(page, Qt::white);
        drawPageFooter(
            painter,
            content,
            footerHeight,
            pageIndex + 1
            );

        painter.save();
        painter.setClipRect(body);
        painter.translate(
            body.left(),
            body.top() - (documentPageIndex * body.height())
            );
        pageDocument.drawContents(
            &painter,
            QRectF(
                0.0,
                documentPageIndex * body.height(),
                body.width(),
                body.height()
                )
            );
        painter.restore();

        if (writingLineStart)
        {
            drawSubNotesWritingLines(
                painter,
                body,
                *writingLineStart,
                subNotesLineSpacing,
                documentPageIndex
                );
        }

        if (drawsFallbackSubNotesOnThisPage)
        {
            painter.save();
            painter.setClipRect(body);
            painter.translate(
                body.left(),
                body.top() + fallbackSubNotesTop
                );
            fallbackSubNotesDocument.drawContents(
                &painter,
                QRectF(
                    0.0,
                    0.0,
                    body.width(),
                    body.height() - fallbackSubNotesTop
                    )
                );
            painter.restore();

            if (fallbackSubNotesLineStart)
            {
                drawSubNotesWritingLines(
                    painter,
                    body,
                    fallbackSubNotesTop
                        + *fallbackSubNotesLineStart,
                    subNotesLineSpacing,
                    0
                    );
            }
        }

        if (pageIndex + 1 < pageCount && !writer.newPage())
        {
            painter.end();
            return failed(
                QObject::tr("Unable to add a page to the sub prep print file.")
                );
        }
    }

    if (!painter.end())
    {
        return failed(
            QObject::tr("The sub prep print file could not be completed.")
            );
    }

    return {
        Status::Sent,
        QObject::tr("Sub prep PDF created.")
    };
}

Result printSubPrep(
    const Request& request
    )
{
    QTemporaryDir temporaryDirectory;

    if (!temporaryDirectory.isValid())
    {
        return failed(
            QObject::tr("Unable to create a temporary print file.")
            );
    }

    const QString documentPath =
        temporaryDirectory.filePath(
            QStringLiteral("Sub Prep.pdf")
            );
    const Result saveResult =
        saveSubPrepPdf(request, documentPath);

    if (saveResult.status != Status::Sent)
    {
        return saveResult;
    }

    QPdfDocument document;
    const QPdfDocument::Error loadError =
        document.load(documentPath);

    if (
        loadError != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0
        )
    {
        return failed(
            QObject::tr("Unable to load the sub prep print file.")
            );
    }

    const PdfPrintService::Result printResult =
        PdfPrintService::printPdfDocument(
            {
                request.parent,
                &document,
                documentPath,
                0,
                translate("Print Sub Prep"),
                QPageLayout::Portrait,
                false,
                SubPrepPdfPageSize,
                true
            }
            );

    switch (printResult.status)
    {
    case PdfPrintService::Status::Sent:
        return sent();

    case PdfPrintService::Status::Canceled:
        return canceled();

    case PdfPrintService::Status::Failed:
    default:
        return failed(printResult.message);
    }
}
}
