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
#include <QTemporaryDir>
#include <QTextDocument>

namespace SubPrepPrintService
{
namespace
{
constexpr QPageSize::PageSizeId SubPrepPdfPageSize = QPageSize::A4;
constexpr qreal SubPrepPdfMarginInches = 0.5;
constexpr int SubPrepPdfResolutionDpi = 300;
constexpr qreal HeaderHeight = 50.0;
constexpr qreal FooterHeight = 38.0;

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
                "<div class=\"schedule-slot essay\">%1</div>"
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
    const QList<QPair<QString, QString>>& facts
    )
{
    QString html = QStringLiteral("<table class=\"facts\">");

    for (int index = 0; index < facts.size(); index += 2)
    {
        html += QStringLiteral("<tr>");

        for (int column = 0; column < 2; ++column)
        {
            const int factIndex = index + column;

            if (factIndex >= facts.size())
            {
                html += QStringLiteral("<td></td>");
                continue;
            }

            const auto& fact = facts.at(factIndex);
            html +=
                QStringLiteral(
                    "<td><span class=\"field-label\">%1</span><br/>%2</td>"
                    )
                    .arg(
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
    const QString& contents
    )
{
    return QStringLiteral(
        "<div class=\"section\"><h2>%1</h2>%2</div>"
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

    QString html = QStringLiteral("<table class=\"schedule\"><tr><th>")
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

QString classInformationHtml(
    const QList<SubPrepClassInformation::TeacherGroup>& groups
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

    for (const SubPrepClassInformation::TeacherGroup& group : groups)
    {
        html += QStringLiteral("<div class=\"teacher-card\">");
        html += QStringLiteral("<h3>%1</h3>")
            .arg(
                htmlText(
                    QStringLiteral("%1: %2")
                        .arg(
                            displayValue(group.displayName),
                            displayValue(group.classListText)
                            )
                    )
                );

        for (const SubPrepClassInformation::ClassDetails& details : group.classes)
        {
            html += QStringLiteral("<div class=\"class-details\">");
            html += QStringLiteral("<h4>%1</h4>")
                .arg(htmlText(details.classLabel));
            html += factsHtml(
                {
                    {
                        translate("Level"),
                        details.info.classLevel
                    },
                    {
                        translate("Time"),
                        details.timeText
                    },
                    {
                        translate("# of Students"),
                        QString::number(details.studentCount)
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
                        translate("WiFi Password"),
                        group.teacher.wifiPassword
                    },
                    {
                        translate("Zoom ID"),
                        group.teacher.zoomId
                    },
                    {
                        translate("Zoom Password"),
                        group.teacher.zoomPassword
                    },
                    {
                        translate("Internet"),
                        group.teacher.internetType
                    },
                    {
                        translate("Projection"),
                        group.teacher.projectionType
                    }
                }
                );
            html += noteHtml(
                translate("Class Notes"),
                details.info.notes
                );
            html += QStringLiteral("</div>");
        }

        html += noteHtml(
            translate("Co-Teacher Notes"),
            group.teacher.notes
            );
        html += QStringLiteral("</div>");
    }

    return html;
}

QString documentHtml(
    const Request& request
    )
{
    QString html = QStringLiteral(
        "<html><body>"
        "<h1>%1</h1>"
        "<div class=\"subtitle\">%2</div>"
        )
        .arg(
            htmlText(translate("Sub Prep")),
            htmlText(translate("Prepare substitute materials and class notes."))
            );

    html += sectionHtml(
        translate("Important Information"),
        sectionHtml(
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
                }
                )
            )
        + sectionHtml(
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
        + sectionHtml(
            translate("Class Materials"),
            noteHtml(QString(), request.classMaterials)
            )
        + sectionHtml(
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
        classInformationHtml(request.classInformation)
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
        "body { color:#1f2933; font-family:'%1'; font-size:9.5pt; }"
        "h1 { color:#1f3a4d; font-size:20pt; font-weight:700; margin:0 0 4px 0; }"
        "h2 { background:#eaf0f4; border-bottom:1px solid #8fa3b0; color:#1f3a4d; "
        "font-size:13pt; font-weight:700; margin:18px 0 7px 0; padding:5px 7px; }"
        "h3 { color:#1f3a4d; font-size:11.5pt; font-weight:700; margin:0 0 7px 0; }"
        "h4 { color:#314c5d; font-size:10pt; font-weight:700; margin:8px 0 4px 0; }"
        ".subtitle { color:#5f6f7a; font-size:10pt; margin:0 0 12px 0; }"
        ".section { margin:0; }"
        ".facts { border-collapse:collapse; margin:0 0 7px 0; width:100%; }"
        ".facts td { border:1px solid #c8d2d8; padding:5px 7px; vertical-align:top; width:50%; }"
        ".field-label, .note-label { color:#425a69; font-size:8pt; font-weight:700; }"
        ".note-label { margin:7px 0 2px 0; }"
        ".note { background:#fafcfd; border:1px solid #c8d2d8; padding:6px 7px; }"
        ".teacher-card { border:1px solid #aabac4; margin:0 0 9px 0; padding:8px; }"
        ".class-details { border-top:1px solid #d6dfe4; margin:8px 0 0 0; padding:1px 0 0 0; }"
        ".schedule { border-collapse:collapse; font-size:7pt; margin:0 0 7px 0; width:100%; }"
        ".schedule th { background:#304c5e; color:#ffffff; font-weight:700; padding:5px 3px; }"
        ".schedule td { border:1px solid #aabac4; padding:3px; text-align:center; vertical-align:middle; }"
        ".schedule-time { background:#edf2f5; color:#263b49; font-weight:700; width:11%; }"
        ".schedule-entry { margin:1px 0; padding:3px 2px; }"
        ".schedule-slot { font-weight:700; padding:5px 2px; }"
        ".essay { background:#ffffff; color:#000000; font-style:italic; }"
        ".lunch { background:#dcdcdc; color:#000000; }"
        ".empty { color:#5f6f7a; font-style:italic; padding:7px; }"
        )
        .arg(family);
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

void drawPageChrome(
    QPainter& painter,
    const QRectF& content,
    int pageNumber
    )
{
    QFont headingFont =
        FontManager::getUiFont(-1, QFont::DemiBold);
    headingFont.setPixelSize(19);

    QFont pageFont =
        FontManager::getUiFont(-1);
    pageFont.setPixelSize(12);

    painter.save();
    painter.setPen(QColor(QStringLiteral("#1f3a4d")));
    painter.setFont(headingFont);
    painter.drawText(
        QRectF(
            content.left(),
            content.top(),
            content.width() * 0.7,
            HeaderHeight - 12.0
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
            HeaderHeight - 12.0
            ),
        Qt::AlignRight | Qt::AlignVCenter,
        QObject::tr("Page %1").arg(pageNumber)
        );
    painter.drawLine(
        QPointF(content.left(), content.top() + HeaderHeight - 8.0),
        QPointF(content.right(), content.top() + HeaderHeight - 8.0)
        );

    painter.setFont(pageFont);
    painter.drawText(
        QRectF(
            content.left(),
            content.bottom() - FooterHeight + 8.0,
            content.width(),
            FooterHeight - 8.0
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
    const QRectF body(
        content.left(),
        content.top() + HeaderHeight,
        content.width(),
        content.height() - HeaderHeight - FooterHeight
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
    QFont documentFont = FontManager::getUiFont(10);
    documentFont.setPointSizeF(9.5);
    document.setDefaultFont(documentFont);
    document.setDefaultStyleSheet(documentStyleSheet());
    document.setDocumentMargin(0.0);
    document.setPageSize(body.size());
    document.setHtml(documentHtml(request));

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
