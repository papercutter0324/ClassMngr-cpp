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
            entry.teacherKr.trimmed(),
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

QString factsHtml(
    const QList<QPair<QString, QString>>& facts,
    int columnCount = 2,
    bool useBorderlessEmptyCells = false,
    const QString& valueClass = QString()
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
            html +=
                QStringLiteral(
                    "<td width=\"%1%\"><span class=\"field-label\">%2</span><div class=\"fact-value%3\">%4</div></td>"
                    )
                    .arg(
                        cellWidth,
                        htmlText(fact.first),
                        valueClass,
                        htmlText(fact.second)
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
    const QString& noteClass = QString()
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

    return QStringLiteral(
        "<div class=\"note-label%1\">%2</div>"
        "<div class=\"note%3\">%4</div>"
        )
        .arg(
            labelClass,
            htmlText(label),
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
        QStringLiteral(" compact-note")
        );
}

QString sectionHtml(
    const QString& heading,
    const QString& contents,
    bool startOnNewPage = false
    )
{
    return QStringLiteral(
        "<div class=\"major-section%1\"><h2>%2</h2>%3</div>"
        )
        .arg(
            startOnNewPage
                ? QStringLiteral(" new-page")
                : QString(),
            htmlText(heading),
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
            htmlText(heading),
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
        "<table class=\"schedule\" width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">"
        "<tr><th width=\"12%\">"
        )
        + htmlText(translate("Time"))
        + QStringLiteral("</th>");

    for (const QString& day : schedule.days)
    {
        html += QStringLiteral("<th>%1</th>").arg(htmlText(day));
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
            htmlText(displayValue(group.displayName))
            );
}

QString coTeacherNotesHtml(
    const QString& note
    )
{
    return QStringLiteral(
        "<div class=\"co-teacher-notes\">"
        "<div class=\"note-label\">%1</div>"
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
                    translate("Korean Name"),
                    group.teacher.teacherKr
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
            QStringLiteral(" teacher-fact-value")
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
                    "<span>%1:</span>&nbsp;%2</div>"
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
        "<h1>%1</h1>"
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
            )
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
        "body { color:#24313a; font-family:'%1'; font-size:10.5pt; }"
        ".document-title { border-bottom:2px solid #2f657d; margin:0 0 15px 0; padding:0 0 10px 0; }"
        "h1 { color:#183746; font-size:23pt; font-weight:700; margin:0 0 3px 0; }"
        ".subtitle { color:#566a75; font-size:11pt; margin:0; }"
        ".major-section { margin:0 0 14px 0; width:100%; }"
        ".new-page { page-break-before:always; }"
        "h2 { background:#244b5f; border:1px solid #244b5f; color:#ffffff; "
        "border-top-width:2px; font-size:14pt; font-weight:700; margin:14px 0 9px 0; padding:7px 9px; text-align:center; "
        "page-break-after:avoid; }"
        ".subsection { margin:0 0 11px 0; width:100%; }"
        ".class-materials-subsection { margin-bottom:27px; }"
        ".subsection-heading { background:#e9f1f5; border-left:4px solid #3f7c96; "
        "color:#23495c; font-size:12pt; font-weight:700; margin:9px 0 6px 0; padding:5px 8px; "
        "page-break-after:avoid; }"
        ".teacher-heading { background:#e9f1f5; border:1px solid #3f7c96; "
        "color:#23495c; font-size:12pt; font-weight:700; padding:7px 9px; }"
        ".facts { border-collapse:collapse; margin:0 0 8px 0; table-layout:fixed; width:100%; }"
        ".facts td { background:#ffffff; border:1px solid #bdcbd2; padding:7px 9px; vertical-align:top; }"
        ".facts td.empty-fact-cell { background:transparent; border:0; padding:0; }"
        ".campus-fact-value, .teacher-fact-value { text-align:center; }"
        ".field-label, .note-label { color:#486675; font-size:9pt; font-weight:700; }"
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
        ".class-list-heading { color:#2c5366; font-size:11pt; font-weight:700; margin:0 0 3px 0; }"
        ".class-list { margin:0; padding:0 0 0 19px; }"
        ".class-list li { margin:4px 0; padding:0; }"
        ".class-list-item { font-weight:700; }"
        ".class-list-note { color:#52656f; font-size:9.5pt; margin:2px 0 0 4em; }"
        ".class-list-note span { color:#486675; font-weight:700; }"
        ".co-teacher-notes { margin:9px 0 0 0; }"
        ".co-teacher-notes .note-label { margin:0 0 3px 0; }"
        ".co-teacher-note { margin-left:4em; }"
        ".sub-notes-prompt { margin:0; }"
        ".schedule { border:1px solid #000000; border-collapse:collapse; font-size:8.5pt; margin:0 0 8px 0; "
        "page-break-inside:avoid; table-layout:fixed; width:100%; }"
        ".schedule th { background:#244b5f; border:1px solid #000000; color:#ffffff; "
        "font-weight:700; padding:7px 4px; }"
        ".schedule td { border:1px solid #000000; padding:5px 4px; text-align:center; vertical-align:middle; }"
        ".schedule-time { background:#e9f1f5; color:#263f4b; font-weight:700; width:12%; }"
        ".schedule-entry { margin:1px 0; padding:4px 3px; }"
        ".schedule-slot { font-weight:700; padding:6px 3px; }"
        ".essay { background:#ffffff; color:#000000; font-size:12pt; font-style:normal; font-weight:700; }"
        ".lunch { background:#dcdcdc; color:#000000; }"
        ".empty { background:#f8fbfc; border:1px solid #d1dce1; color:#5c6e78; "
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
    painter.setPen(QColor(QStringLiteral("#5f6f7a")));
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

    const QSizeF documentSize =
        document.documentLayout()->documentSize();
    const int pageCount =
        std::max(
            1,
            static_cast<int>(
                std::ceil(documentSize.height() / body.height())
                )
            );

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
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
            body.top() - (pageIndex * body.height())
            );
        document.drawContents(
            &painter,
            QRectF(
                0.0,
                pageIndex * body.height(),
                body.width(),
                body.height()
                )
            );
        painter.restore();

        if (subNotesLineStart)
        {
            drawSubNotesWritingLines(
                painter,
                body,
                *subNotesLineStart,
                subNotesLineSpacing,
                pageIndex
                );
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
