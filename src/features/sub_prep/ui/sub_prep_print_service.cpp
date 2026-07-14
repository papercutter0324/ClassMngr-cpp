#include "sub_prep_print_service.h"

#include "core/fontmanager.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <algorithm>
#include <cmath>

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
constexpr qreal HeaderHeightInches = 0.32;
constexpr qreal FooterHeightInches = 0.24;

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
                "<div class=\"schedule-slot essay\"><span class=\"essay-label\">%1</span></div>"
                )
                .arg(htmlText(translate("Essay")));
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
    bool useBorderlessEmptyCells = false
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
                    "<td width=\"%1%\"><span class=\"field-label\">%2</span><br/>%3</td>"
                    )
                    .arg(
                        cellWidth,
                        htmlText(fact.first),
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
    const QString& note
    )
{
    if (label.trimmed().isEmpty())
    {
        return QStringLiteral("<div class=\"note\">%1</div>")
            .arg(htmlText(note));
    }

    return QStringLiteral(
        "<div class=\"note-label\">%1</div>"
        "<div class=\"note\">%2</div>"
        )
        .arg(
            htmlText(label),
            htmlText(note)
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
    const QString& contents
    )
{
    return QStringLiteral(
        "<div class=\"subsection\"><h3 class=\"subsection-heading\">%1</h3>%2</div>"
        )
        .arg(
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
                    translate("Korean Teacher Name"),
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
            4
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
                        htmlText(translate("Class Notes")),
                        htmlText(details.info.notes)
                        );
            }

            html += QStringLiteral("</li>");
        }

        html += QStringLiteral("</ul></div>");

        if (!isNotAvailable(group.teacher.notes))
        {
            html += noteHtml(
                translate("Co-Teacher Notes"),
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
    const QSet<int>& pageBreakBeforeTeacherGroups = {}
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
            htmlText(translate("Prepare substitute materials and class notes."))
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
                4
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
            noteHtml(QString(), request.classMaterials)
            )
        + subsectionHtml(
            translate("Book Report Grading"),
            noteHtml(
                translate("Grading Instructions"),
                request.gradingInstructions
                )
            + noteHtml(
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
        noteHtml(translate("Notes"), request.subNotes)
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
        "font-size:14pt; font-weight:700; margin:14px 0 9px 0; padding:7px 9px; "
        "page-break-after:avoid; }"
        ".subsection { margin:0 0 11px 0; width:100%; }"
        ".subsection-heading { background:#e9f1f5; border-left:4px solid #3f7c96; "
        "color:#23495c; font-size:12pt; font-weight:700; margin:9px 0 6px 0; padding:5px 8px; "
        "page-break-after:avoid; }"
        ".teacher-heading { background:#e9f1f5; border:1px solid #3f7c96; "
        "color:#23495c; font-size:12pt; font-weight:700; padding:7px 9px; }"
        ".facts { border-collapse:collapse; margin:0 0 8px 0; table-layout:fixed; width:100%; }"
        ".facts td { background:#ffffff; border:1px solid #bdcbd2; padding:7px 9px; vertical-align:top; }"
        ".facts td.empty-fact-cell { background:transparent; border:0; padding:0; }"
        ".field-label, .note-label { color:#486675; font-size:9pt; font-weight:700; }"
        ".note-label { margin:8px 0 3px 0; }"
        ".note { background:#f8fbfc; border:1px solid #bdcbd2; border-left:3px solid #7aa3b5; "
        "padding:8px 9px; }"
        ".teacher-card { border-collapse:collapse; margin:0; page-break-inside:avoid; width:100%; }"
        ".teacher-card-content { border:1px solid #9fb3bd; border-top:0; padding:9px; vertical-align:top; }"
        ".teacher-card-wrapper { margin:0; padding:0; width:100%; }"
        ".teacher-section-spacer { font-size:6pt; margin:0 0 8px 0; padding:0; }"
        ".teacher-class-list { border-top:1px solid #d1dce1; margin:9px 0 0 0; padding:7px 0 0 0; }"
        ".class-list-heading { color:#2c5366; font-size:11pt; font-weight:700; margin:0 0 3px 0; }"
        ".class-list { margin:0; padding:0 0 0 19px; }"
        ".class-list li { margin:4px 0; padding:0; }"
        ".class-list-item { font-weight:700; }"
        ".class-list-note { color:#52656f; font-size:9.5pt; margin:2px 0 0 0; }"
        ".class-list-note span { color:#486675; font-weight:700; }"
        ".schedule { border-collapse:collapse; font-size:8.5pt; margin:0 0 8px 0; "
        "page-break-inside:avoid; table-layout:fixed; width:100%; }"
        ".schedule th { background:#244b5f; border:1px solid #244b5f; color:#ffffff; "
        "font-weight:700; padding:7px 4px; }"
        ".schedule td { border:1px solid #9fb3bd; padding:5px 4px; text-align:center; vertical-align:middle; }"
        ".schedule-time { background:#e9f1f5; color:#263f4b; font-weight:700; width:12%; }"
        ".schedule-entry { margin:1px 0; padding:4px 3px; }"
        ".schedule-slot { font-weight:700; padding:6px 3px; }"
        ".essay { background:#ffffff; color:#000000; font-size:10pt; font-style:italic; }"
        ".essay-label { font-weight:700; white-space:nowrap; }"
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

void drawPageChrome(
    QPainter& painter,
    const QRectF& content,
    qreal headerHeight,
    qreal footerHeight,
    int pageNumber
    )
{
    QFont headingFont =
        FontManager::getUiFont(-1, QFont::DemiBold);
    headingFont.setPointSizeF(9.5);

    QFont pageFont =
        FontManager::getUiFont(-1);
    pageFont.setPointSizeF(8.5);

    painter.save();
    painter.setPen(QColor(QStringLiteral("#1f3a4d")));
    painter.setFont(headingFont);
    painter.drawText(
        QRectF(
            content.left(),
            content.top(),
            content.width() * 0.7,
            headerHeight - 12.0
            ),
        Qt::AlignLeft | Qt::AlignVCenter,
        translate("Sub Prep")
        );

    painter.setPen(QColor(QStringLiteral("#5f6f7a")));
    painter.setFont(pageFont);
    painter.drawText(
        QRectF(
            content.right() - (content.width() * 0.2),
            content.top(),
            content.width() * 0.2,
            headerHeight - 12.0
            ),
        Qt::AlignRight | Qt::AlignVCenter,
        QObject::tr("Page %1").arg(pageNumber)
        );
    painter.drawLine(
        QPointF(content.left(), content.top() + headerHeight - 8.0),
        QPointF(content.right(), content.top() + headerHeight - 8.0)
        );

    painter.setFont(pageFont);
    painter.drawText(
        QRectF(
            content.left(),
            content.bottom() - footerHeight + 8.0,
            content.width(),
            footerHeight - 8.0
            ),
        Qt::AlignRight | Qt::AlignVCenter,
        QObject::tr("ClassMngr")
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
    const qreal headerHeight =
        pixelsForInches(
            HeaderHeightInches,
            writer.resolution()
            );
    const qreal footerHeight =
        pixelsForInches(
            FooterHeightInches,
            writer.resolution()
            );
    const QRectF body(
        content.left(),
        content.top() + headerHeight,
        content.width(),
        content.height() - headerHeight - footerHeight
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

    for (int pass = 0; pass < request.classInformation.size(); ++pass)
    {
        document.setHtml(
            documentHtml(request, pageBreakBeforeTeacherGroups)
            );

        const QSet<int> spanningGroups =
            teacherSectionsThatSpanPages(document, body.height());
        const QSet<int> newPageBreaks =
            spanningGroups - pageBreakBeforeTeacherGroups;

        if (newPageBreaks.isEmpty())
        {
            break;
        }

        pageBreakBeforeTeacherGroups.unite(newPageBreaks);
    }

    document.setHtml(
        documentHtml(request, pageBreakBeforeTeacherGroups)
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
        drawPageChrome(
            painter,
            content,
            headerHeight,
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
