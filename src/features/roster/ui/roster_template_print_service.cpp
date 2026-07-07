#include "features/roster/ui/roster_template_print_service.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPdfDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTime>
#include <QXmlStreamReader>

#include <zlib.h>

namespace RosterTemplatePrintService
{
namespace
{
constexpr int FirstStudentRow = 7;
constexpr int LastStudentRow = 29;
constexpr int MaxStudentsPerClass = LastStudentRow - FirstStudentRow + 1;

constexpr auto XlsxTemplateResource =
    ":/assets/files/rosters/Roster Template 1 - By Day.xlsx";
constexpr auto OdsTemplateResource =
    ":/assets/files/rosters/Roster Template 1 - By Day.ods";

const QStringList DaySheets{
    QStringLiteral("Monday"),
    QStringLiteral("Tuesday"),
    QStringLiteral("Wednesday"),
    QStringLiteral("Thursday"),
    QStringLiteral("Friday")
};

const QStringList ClearSheets{
    QStringLiteral("Monday"),
    QStringLiteral("Tuesday"),
    QStringLiteral("Wednesday"),
    QStringLiteral("Thursday"),
    QStringLiteral("Friday"),
    QStringLiteral("(Alt 1)"),
    QStringLiteral("(Alt 2)")
};

const QList<int> SlotColumns{2, 4, 6, 8, 10, 12};

struct ZipEntry
{
    QString name;
    quint16 method = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localHeaderOffset = 0;
};

quint16 readLe16(
    const QByteArray& data,
    qsizetype offset
    )
{
    if (offset < 0 || offset + 2 > data.size())
    {
        return 0;
    }

    const auto* bytes =
        reinterpret_cast<const uchar*>(data.constData() + offset);

    return static_cast<quint16>(
        bytes[0]
        | (bytes[1] << 8)
        );
}

quint32 readLe32(
    const QByteArray& data,
    qsizetype offset
    )
{
    if (offset < 0 || offset + 4 > data.size())
    {
        return 0;
    }

    const auto* bytes =
        reinterpret_cast<const uchar*>(data.constData() + offset);

    return static_cast<quint32>(
        bytes[0]
        | (bytes[1] << 8)
        | (bytes[2] << 16)
        | (bytes[3] << 24)
        );
}

QByteArray inflateRawDeflate(
    const QByteArray& compressed,
    quint32 uncompressedSize
    )
{
    QByteArray output;
    output.resize(static_cast<qsizetype>(uncompressedSize));

    if (uncompressedSize == 0)
    {
        return output;
    }

    z_stream stream{};
    stream.next_in =
        reinterpret_cast<Bytef*>(
            const_cast<char*>(compressed.constData())
            );
    stream.avail_in =
        static_cast<uInt>(compressed.size());
    stream.next_out =
        reinterpret_cast<Bytef*>(output.data());
    stream.avail_out =
        static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
    {
        return {};
    }

    const int result =
        inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (result != Z_STREAM_END)
    {
        return {};
    }

    output.truncate(static_cast<qsizetype>(stream.total_out));
    return output;
}

QHash<QString, ZipEntry> zipEntries(
    const QByteArray& zipData
    )
{
    constexpr int MinZipEocdSize = 22;
    constexpr int MaxZipCommentSize = 0xffff;
    constexpr quint32 ZipEocdSignature = 0x06054b50;
    constexpr quint32 ZipCentralFileSignature = 0x02014b50;

    QHash<QString, ZipEntry> entries;

    const qsizetype searchStart =
        qMax<qsizetype>(
            0,
            zipData.size() - MinZipEocdSize - MaxZipCommentSize
            );

    qsizetype eocdOffset = -1;
    for (qsizetype offset = zipData.size() - MinZipEocdSize;
         offset >= searchStart;
         --offset)
    {
        if (readLe32(zipData, offset) == ZipEocdSignature)
        {
            eocdOffset = offset;
            break;
        }
    }

    if (eocdOffset < 0)
    {
        return entries;
    }

    const quint16 entryCount =
        readLe16(zipData, eocdOffset + 10);
    const quint32 centralDirectoryOffset =
        readLe32(zipData, eocdOffset + 16);

    qsizetype offset =
        static_cast<qsizetype>(centralDirectoryOffset);

    for (int index = 0; index < entryCount; ++index)
    {
        if (
            offset + 46 > zipData.size()
            || readLe32(zipData, offset) != ZipCentralFileSignature
            )
        {
            return {};
        }

        const quint16 method =
            readLe16(zipData, offset + 10);
        const quint32 compressedSize =
            readLe32(zipData, offset + 20);
        const quint32 uncompressedSize =
            readLe32(zipData, offset + 24);
        const quint16 fileNameLength =
            readLe16(zipData, offset + 28);
        const quint16 extraLength =
            readLe16(zipData, offset + 30);
        const quint16 commentLength =
            readLe16(zipData, offset + 32);
        const quint32 localHeaderOffset =
            readLe32(zipData, offset + 42);

        if (offset + 46 + fileNameLength > zipData.size())
        {
            return {};
        }

        ZipEntry entry;
        entry.name =
            QString::fromUtf8(
                zipData.mid(offset + 46, fileNameLength)
                );
        entry.method = method;
        entry.compressedSize = compressedSize;
        entry.uncompressedSize = uncompressedSize;
        entry.localHeaderOffset = localHeaderOffset;

        entries.insert(entry.name, entry);

        offset += 46 + fileNameLength + extraLength + commentLength;
    }

    return entries;
}

QByteArray zipFileData(
    const QByteArray& zipData,
    const ZipEntry& entry
    )
{
    constexpr quint32 ZipLocalFileSignature = 0x04034b50;

    const qsizetype localOffset =
        static_cast<qsizetype>(entry.localHeaderOffset);

    if (
        localOffset + 30 > zipData.size()
        || readLe32(zipData, localOffset) != ZipLocalFileSignature
        )
    {
        return {};
    }

    const quint16 fileNameLength =
        readLe16(zipData, localOffset + 26);
    const quint16 extraLength =
        readLe16(zipData, localOffset + 28);

    const qsizetype dataOffset =
        localOffset + 30 + fileNameLength + extraLength;

    if (
        dataOffset < 0
        || dataOffset + entry.compressedSize > zipData.size()
        )
    {
        return {};
    }

    const QByteArray compressed =
        zipData.mid(
            dataOffset,
            static_cast<qsizetype>(entry.compressedSize)
            );

    if (entry.method == 0)
    {
        return compressed;
    }

    if (entry.method == 8)
    {
        return inflateRawDeflate(
            compressed,
            entry.uncompressedSize
            );
    }

    return {};
}

QString workbookTextEntry(
    const QString& path,
    const QString& entryName
    )
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    const QByteArray data =
        file.readAll();
    const QHash<QString, ZipEntry> entries =
        zipEntries(data);

    if (!entries.contains(entryName))
    {
        return {};
    }

    return QString::fromUtf8(
        zipFileData(
            data,
            entries.value(entryName)
            )
        );
}

QString columnName(
    int column
    )
{
    QString name;

    while (column > 0)
    {
        --column;
        name.prepend(
            QChar(
                QLatin1Char('A').unicode()
                + (column % 26)
                )
            );
        column /= 26;
    }

    return name;
}

int columnForStartTime(
    const QString& startTime
    )
{
    const QString trimmed =
        startTime.trimmed();

    QTime time =
        QTime::fromString(trimmed, QStringLiteral("h:mm AP"));

    if (!time.isValid())
    {
        time =
            QTime::fromString(trimmed, QStringLiteral("h:mm ap"));
    }

    if (!time.isValid())
    {
        time =
            QTime::fromString(trimmed, QStringLiteral("H:mm"));
    }

    if (!time.isValid())
    {
        return -1;
    }

    int hour =
        time.hour();

    if (hour > 12)
    {
        hour -= 12;
    }

    switch (hour)
    {
    case 4:
        return 2;
    case 5:
        return 4;
    case 6:
        return 6;
    case 7:
        return 8;
    case 8:
        return 10;
    case 9:
        return 12;
    default:
        return -1;
    }
}

int rosterColumnIndex(
    const Roster& roster,
    const QString& name
    )
{
    for (int index = 0; index < roster.columns.size(); ++index)
    {
        if (roster.columns.at(index).compare(name, Qt::CaseInsensitive) == 0)
        {
            return index;
        }
    }

    return -1;
}

QString rosterCell(
    const QStringList& row,
    int column
    )
{
    if (column < 0 || column >= row.size())
    {
        return {};
    }

    return row.at(column).trimmed();
}

QString classLabel(
    const RosterClassData& data
    )
{
    QStringList parts;

    if (!data.info.classGrade.trimmed().isEmpty())
    {
        parts.append(data.info.classGrade.trimmed());
    }

    if (!data.info.classLevel.trimmed().isEmpty())
    {
        parts.append(data.info.classLevel.trimmed());
    }

    const QString label =
        parts.join(QLatin1Char(' ')).trimmed();

    return label.isEmpty()
        ? data.classroom.name.trimmed()
        : label;
}

QString teacherLabel(
    const ClassInfo& info
    )
{
    QStringList parts;

    if (!info.teacherEn.trimmed().isEmpty())
    {
        parts.append(info.teacherEn.trimmed());
    }

    if (!info.teacherKr.trimmed().isEmpty())
    {
        parts.append(info.teacherKr.trimmed());
    }

    return parts.join(QStringLiteral(" / "));
}

void appendOperation(
    QList<FillOperation>& operations,
    const QString& sheet,
    int column,
    int row,
    const QString& value
    )
{
    operations.append(
        {
            sheet,
            columnName(column) + QString::number(row),
            value
        }
        );
}

QJsonDocument operationsDocument(
    const QList<FillOperation>& operations
    )
{
    QJsonArray array;

    for (const FillOperation& operation : operations)
    {
        QJsonObject object;
        object.insert(QStringLiteral("sheet"), operation.sheet);
        object.insert(QStringLiteral("cell"), operation.cell);
        object.insert(QStringLiteral("value"), operation.value);
        array.append(object);
    }

    return QJsonDocument(array);
}

bool writeTextFile(
    const QString& path,
    const QString& contents,
    QString* errorMessage
    )
{
    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("Unable to create %1.").arg(path);
        }
        return false;
    }

    QTextStream stream(&file);
    stream << contents;

    if (!file.commit())
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("Unable to save %1.").arg(path);
        }
        return false;
    }

    return true;
}

QString jsString(
    const QString& value
    )
{
    return QString::fromUtf8(
        QJsonDocument(
            QJsonArray{value}
            ).toJson(QJsonDocument::Compact)
        ).mid(1).chopped(1);
}

QString powershellScript(
    const QString& workbookPath,
    const QString& operationsPath,
    const QString& pdfPath
    )
{
    return QStringLiteral(R"ps1(
$ErrorActionPreference = "Stop"
$excel = New-Object -ComObject Excel.Application
$excel.Visible = $false
$excel.DisplayAlerts = $false
$workbook = $null
try {
    $workbook = $excel.Workbooks.Open(%1)
    $ops = Get-Content -Raw -LiteralPath %2 | ConvertFrom-Json
    foreach ($op in $ops) {
        try {
            $sheet = $workbook.Worksheets.Item($op.sheet)
            $sheet.Range($op.cell).Value2 = [string]$op.value
        } catch {
            throw "Unable to fill $($op.sheet)!$($op.cell): $($_.Exception.Message)"
        }
    }
    $workbook.ExportAsFixedFormat(0, %3)
} finally {
    if ($workbook -ne $null) {
        $workbook.Close($false)
    }
    $excel.Quit()
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($excel) | Out-Null
}
)ps1")
        .arg(
            jsString(workbookPath),
            jsString(operationsPath),
            jsString(pdfPath)
            );
}

QString linuxUnoScript(
    const QString& workbookPath,
    const QString& operationsPath,
    const QString& pdfPath,
    const QString& profilePath
    )
{
    return QStringLiteral(R"py(
import json
import os
import subprocess
import sys
import time

try:
    import uno
    from com.sun.star.beans import PropertyValue
except Exception as exc:
    raise SystemExit("LibreOffice UNO Python support is not available: %s" % exc)

workbook_path = %1
operations_path = %2
pdf_path = %3
profile_path = %4

def prop(name, value):
    item = PropertyValue()
    item.Name = name
    item.Value = value
    return item

soffice = os.environ.get("CLASSMNGR_SOFFICE", "soffice")
accept = "socket,host=127.0.0.1,port=2002;urp;StarOffice.ComponentContext"
process = subprocess.Popen([
    soffice,
    "-env:UserInstallation=file://" + profile_path,
    "--headless",
    "--nologo",
    "--nofirststartwizard",
    "--norestore",
    "--accept=" + accept,
])

doc = None
try:
    local_context = uno.getComponentContext()
    resolver = local_context.ServiceManager.createInstanceWithContext(
        "com.sun.star.bridge.UnoUrlResolver",
        local_context,
    )

    context = None
    last_error = None
    for _ in range(80):
        try:
            context = resolver.resolve("uno:" + accept)
            break
        except Exception as exc:
            last_error = exc
            time.sleep(0.25)

    if context is None:
        raise RuntimeError("Unable to start LibreOffice: %s" % last_error)

    desktop = context.ServiceManager.createInstanceWithContext(
        "com.sun.star.frame.Desktop",
        context,
    )

    workbook_url = uno.systemPathToFileUrl(workbook_path)
    pdf_url = uno.systemPathToFileUrl(pdf_path)
    doc = desktop.loadComponentFromURL(
        workbook_url,
        "_blank",
        0,
        (prop("Hidden", True), prop("ReadOnly", False)),
    )

    if doc is None:
        raise RuntimeError("LibreOffice could not open the roster template.")

    with open(operations_path, "r", encoding="utf-8") as handle:
        operations = json.load(handle)

    sheets = doc.getSheets()
    for operation in operations:
        sheet = sheets.getByName(operation["sheet"])
        sheet.getCellRangeByName(operation["cell"]).String = operation["value"]

    doc.storeToURL(
        pdf_url,
        (prop("FilterName", "calc_pdf_Export"), prop("Overwrite", True)),
    )
finally:
    if doc is not None:
        doc.close(True)
    process.terminate()
    try:
        process.wait(timeout=10)
    except Exception:
        process.kill()
)py")
        .arg(
            jsString(workbookPath),
            jsString(operationsPath),
            jsString(pdfPath),
            jsString(profilePath)
            );
}

QString macExcelScript(
    const QString& workbookPath,
    const QString& operationsPath,
    const QString& pdfPath
    )
{
    Q_UNUSED(operationsPath);

    QString script;
    QTextStream stream(&script);

    stream
        << "tell application \"Microsoft Excel\"\n"
        << "set display alerts to false\n"
        << "open workbook workbook file name "
        << jsString(workbookPath)
        << "\n"
        << "set wb to active workbook\n";

    QFile operationsFile(operationsPath);
    if (operationsFile.open(QIODevice::ReadOnly))
    {
        const QJsonDocument document =
            QJsonDocument::fromJson(operationsFile.readAll());

        for (const QJsonValue& value : document.array())
        {
            const QJsonObject object =
                value.toObject();
            stream
                << "set value of range "
                << jsString(object.value(QStringLiteral("cell")).toString())
                << " of worksheet "
                << jsString(object.value(QStringLiteral("sheet")).toString())
                << " of wb to "
                << jsString(object.value(QStringLiteral("value")).toString())
                << "\n";
        }
    }

    stream
        << "save workbook as wb filename "
        << jsString(pdfPath)
        << " file format PDF file format\n"
        << "close wb saving no\n"
        << "end tell\n";

    return script;
}

QString macNumbersScript(
    const QString& workbookPath,
    const QString& operationsPath,
    const QString& pdfPath
    )
{
    Q_UNUSED(operationsPath);

    return QStringLiteral(R"applescript(
tell application "Numbers"
    open %1
    set theDocument to front document
    export theDocument to %2 as PDF
    close theDocument saving no
end tell
)applescript")
        .arg(
            jsString(workbookPath),
            jsString(pdfPath)
            );
}

bool runProcess(
    const QString& program,
    const QStringList& arguments,
    QString* errorMessage
    )
{
    QProcess process;
    process.start(program, arguments);

    if (!process.waitForStarted(15000))
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("Unable to start %1.").arg(program);
        }
        return false;
    }

    if (!process.waitForFinished(120000))
    {
        process.kill();
        process.waitForFinished();

        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("%1 did not finish exporting the roster PDF.")
                    .arg(program);
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        if (errorMessage)
        {
            const QString output =
                QString::fromUtf8(process.readAllStandardError()).trimmed();

            *errorMessage =
                output.isEmpty()
                    ? QObject::tr("%1 could not export the roster PDF.")
                        .arg(program)
                    : output;
        }
        return false;
    }

    return true;
}

bool renderPdf(
    const QString& workbookPath,
    const QList<FillOperation>& operations,
    const QString& pdfPath,
    const QString& workingDirectory,
    QString* errorMessage
    )
{
    const QString operationsPath =
        QDir(workingDirectory).filePath(QStringLiteral("roster-ops.json"));
    const QString scriptPath =
        QDir(workingDirectory).filePath(QStringLiteral("render-rosters"));

    if (
        !writeTextFile(
            operationsPath,
            QString::fromUtf8(
                operationsDocument(operations).toJson(QJsonDocument::Indented)
                ),
            errorMessage
            )
        )
    {
        return false;
    }

    if (isWindows())
    {
        const QString powerShellPath =
            scriptPath + QStringLiteral(".ps1");

        if (
            !writeTextFile(
                powerShellPath,
                powershellScript(workbookPath, operationsPath, pdfPath),
                errorMessage
                )
            )
        {
            return false;
        }

        return runProcess(
            QStringLiteral("powershell"),
            {
                QStringLiteral("-NoProfile"),
                QStringLiteral("-ExecutionPolicy"),
                QStringLiteral("Bypass"),
                QStringLiteral("-File"),
                powerShellPath
            },
            errorMessage
            );
    }

    if (isMac())
    {
        const QString appleScriptPath =
            scriptPath + QStringLiteral(".applescript");
        const bool hasExcel =
            QFileInfo::exists(QStringLiteral("/Applications/Microsoft Excel.app"));

        const QString contents =
            hasExcel
                ? macExcelScript(workbookPath, operationsPath, pdfPath)
                : macNumbersScript(workbookPath, operationsPath, pdfPath);

        if (!writeTextFile(appleScriptPath, contents, errorMessage))
        {
            return false;
        }

        return runProcess(
            QStringLiteral("osascript"),
            {appleScriptPath},
            errorMessage
            );
    }

    const QString pythonPath =
        scriptPath + QStringLiteral(".py");
    const QString profilePath =
        QDir(workingDirectory).filePath(QStringLiteral("libreoffice-profile"));

    QDir().mkpath(profilePath);

    if (
        !writeTextFile(
            pythonPath,
            linuxUnoScript(workbookPath, operationsPath, pdfPath, profilePath),
            errorMessage
            )
        )
    {
        return false;
    }

    return runProcess(
        QStringLiteral("python3"),
        {pythonPath},
        errorMessage
        );
}

QString failedMessage(
    const QString& message
    )
{
    return message.trimmed().isEmpty()
        ? QObject::tr("Roster printing failed.")
        : message;
}

Result failed(
    const QString& message
    )
{
    return {
        Status::Failed,
        failedMessage(message)
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
        QObject::tr("Roster print job sent.")
    };
}

QString selectedTemplatePath(
    const QString& requestedPath
    )
{
    if (!requestedPath.trimmed().isEmpty())
    {
        return requestedPath;
    }

    return preferredBundledTemplatePath(getPlatform());
}

QString tempWorkbookFileName(
    const QString& templatePath
    )
{
    const QString suffix =
        QFileInfo(templatePath).suffix().toLower();

    return QStringLiteral("Roster Template.")
        + (suffix.isEmpty() ? QStringLiteral("xlsx") : suffix);
}

bool copyTemplateToTemp(
    const QString& templatePath,
    const QString& destinationPath,
    QString* errorMessage
    )
{
    QFile::remove(destinationPath);

    if (QFile::copy(templatePath, destinationPath))
    {
        return true;
    }

    if (errorMessage)
    {
        *errorMessage =
            QObject::tr("Unable to copy the roster template for printing.");
    }

    return false;
}

QList<RosterClassData> loadRosterClassData(
    DataService* dataService,
    const QList<int>& classIds
    )
{
    QList<RosterClassData> result;

    if (!dataService)
    {
        return result;
    }

    for (int classId : classIds)
    {
        RosterClassData data;
        data.classroom =
            dataService->getClassById(classId);
        data.info =
            dataService->loadClassInfo(classId);
        data.roster =
            dataService->loadRoster(classId);

        if (data.classroom.id > 0)
        {
            result.append(data);
        }
    }

    return result;
}
} // namespace

QStringList preferredTemplateSuffixes(
    Platform platform
    )
{
    switch (platform)
    {
    case Platform::LINUX:
        return {
            QStringLiteral("ods"),
            QStringLiteral("xlsx")
        };

    case Platform::WINDOWS:
    case Platform::MAC:
    default:
        return {
            QStringLiteral("xlsx"),
            QStringLiteral("ods")
        };
    }
}

QString preferredBundledTemplatePath(
    Platform platform
    )
{
    const QStringList suffixes =
        preferredTemplateSuffixes(platform);

    for (const QString& suffix : suffixes)
    {
        const QString path =
            suffix == QStringLiteral("ods")
                ? QString::fromLatin1(OdsTemplateResource)
                : QString::fromLatin1(XlsxTemplateResource);

        if (QFile::exists(path))
        {
            return path;
        }
    }

    return {};
}

QList<int> resolveClassIds(
    Scope scope,
    int currentClassId,
    const QList<int>& selectedClassIds,
    const QList<Classroom>& classes
    )
{
    QList<int> ids;

    switch (scope)
    {
    case Scope::CurrentClass:
        if (currentClassId > 0)
        {
            ids.append(currentClassId);
        }
        break;

    case Scope::SelectedClasses:
        for (int classId : selectedClassIds)
        {
            if (classId > 0 && !ids.contains(classId))
            {
                ids.append(classId);
            }
        }
        break;

    case Scope::AllClasses:
    default:
        for (const Classroom& classroom : classes)
        {
            if (classroom.id > 0 && !ids.contains(classroom.id))
            {
                ids.append(classroom.id);
            }
        }
        break;
    }

    return ids;
}

QList<FillOperation> buildByDayFillOperations(
    const QList<RosterClassData>& classes,
    QString* errorMessage
    )
{
    QList<FillOperation> operations;

    for (const QString& sheet : ClearSheets)
    {
        for (int column : SlotColumns)
        {
            appendOperation(operations, sheet, column, 3, QString());
            appendOperation(operations, sheet, column, 4, QString());
            appendOperation(operations, sheet, column, 5, QString());
            appendOperation(operations, sheet, column, 30, QString());
            appendOperation(operations, sheet, column, 31, QString());
            appendOperation(operations, sheet, column, 32, QString());
            appendOperation(operations, sheet, column, 33, QString());

            for (int row = FirstStudentRow; row <= LastStudentRow; ++row)
            {
                appendOperation(operations, sheet, column, row, QString());
                appendOperation(operations, sheet, column + 1, row, QString());
            }
        }
    }

    QSet<QString> occupiedSlots;

    for (const RosterClassData& data : classes)
    {
        const int englishColumn =
            rosterColumnIndex(data.roster, QStringLiteral("English"));
        const int koreanColumn =
            rosterColumnIndex(data.roster, QStringLiteral("Korean"));

        for (const ClassTime& time : data.info.classTimes)
        {
            const QString sheet =
                time.day.trimmed();

            if (!DaySheets.contains(sheet))
            {
                continue;
            }

            const int column =
                columnForStartTime(time.startTime);

            if (column < 0)
            {
                continue;
            }

            const QString slotKey =
                sheet + QLatin1Char('|') + QString::number(column);

            if (occupiedSlots.contains(slotKey))
            {
                if (errorMessage)
                {
                    *errorMessage =
                        QObject::tr(
                            "Multiple selected classes use the %1 %2 slot."
                            )
                            .arg(sheet, time.startTime);
                }
                return {};
            }

            occupiedSlots.insert(slotKey);

            appendOperation(operations, sheet, column, 3, classLabel(data));
            appendOperation(operations, sheet, column, 4, teacherLabel(data.info));
            appendOperation(operations, sheet, column, 5, data.info.roomNumber.trimmed());
            appendOperation(operations, sheet, column, 30, data.info.wifiName.trimmed());
            appendOperation(operations, sheet, column, 31, data.info.wifiPassword.trimmed());
            appendOperation(operations, sheet, column, 32, data.info.zoomId.trimmed());
            appendOperation(operations, sheet, column, 33, data.info.zoomPassword.trimmed());

            int writtenStudentCount = 0;
            for (const QStringList& row : data.roster.rows)
            {
                if (writtenStudentCount >= MaxStudentsPerClass)
                {
                    break;
                }

                const QString english =
                    rosterCell(row, englishColumn);
                const QString korean =
                    rosterCell(row, koreanColumn);

                if (english.isEmpty() && korean.isEmpty())
                {
                    continue;
                }

                const int outputRow =
                    FirstStudentRow + writtenStudentCount;

                appendOperation(operations, sheet, column, outputRow, english);
                appendOperation(operations, sheet, column + 1, outputRow, korean);
                ++writtenStudentCount;
            }
        }
    }

    return operations;
}

bool isSupportedByDayTemplate(
    const QString& templatePath,
    QString* errorMessage
    )
{
    const QString suffix =
        QFileInfo(templatePath).suffix().toLower();

    if (suffix != QStringLiteral("xlsx") && suffix != QStringLiteral("ods"))
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("Roster templates must be .xlsx or .ods files.");
        }
        return false;
    }

    if (!QFile::exists(templatePath))
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("The roster template could not be found.");
        }
        return false;
    }

    const QString workbookXml =
        suffix == QStringLiteral("xlsx")
            ? workbookTextEntry(templatePath, QStringLiteral("xl/workbook.xml"))
            : workbookTextEntry(templatePath, QStringLiteral("content.xml"));

    if (workbookXml.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage =
                QObject::tr("The roster template could not be read.");
        }
        return false;
    }

    for (const QString& day : DaySheets)
    {
        if (!workbookXml.contains(day))
        {
            if (errorMessage)
            {
                *errorMessage =
                    QObject::tr("The roster template is missing the %1 sheet.")
                        .arg(day);
            }
            return false;
        }
    }

    return true;
}

Result printRosters(
    const Request& request
    )
{
    if (!request.services || !request.services->hasOpenDatabase())
    {
        return failed(QObject::tr("No database is open."));
    }

    DataService* dataService =
        request.services->dataService();

    if (!dataService)
    {
        return failed(QObject::tr("Roster data is not available."));
    }

    const QString templatePath =
        selectedTemplatePath(request.templatePath);

    QString errorMessage;
    if (!isSupportedByDayTemplate(templatePath, &errorMessage))
    {
        return failed(errorMessage);
    }

    const QList<Classroom> classes =
        dataService->getClasses();
    const QList<int> classIds =
        resolveClassIds(
            request.scope,
            request.currentClassId,
            request.selectedClassIds,
            classes
            );

    if (classIds.isEmpty())
    {
        return failed(QObject::tr("No classes were selected for printing."));
    }

    const QList<RosterClassData> rosterClasses =
        loadRosterClassData(dataService, classIds);
    const QList<FillOperation> operations =
        buildByDayFillOperations(rosterClasses, &errorMessage);

    if (operations.isEmpty() && !errorMessage.isEmpty())
    {
        return failed(errorMessage);
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return failed(QObject::tr("Unable to create a temporary print folder."));
    }

    const QString workbookPath =
        temporaryDirectory.filePath(tempWorkbookFileName(templatePath));
    const QString pdfPath =
        temporaryDirectory.filePath(QStringLiteral("Rosters.pdf"));

    if (!copyTemplateToTemp(templatePath, workbookPath, &errorMessage))
    {
        return failed(errorMessage);
    }

    if (
        !renderPdf(
            workbookPath,
            operations,
            pdfPath,
            temporaryDirectory.path(),
            &errorMessage
            )
        )
    {
        return failed(errorMessage);
    }

    QPdfDocument document;
    const QPdfDocument::Error loadError =
        document.load(pdfPath);

    if (
        loadError != QPdfDocument::Error::None
        || document.status() != QPdfDocument::Status::Ready
        || document.pageCount() <= 0
        )
    {
        return failed(QObject::tr("Unable to load the generated roster PDF."));
    }

    const PdfPrintService::Result printResult =
        PdfPrintService::printPdfDocument(
            {
                request.parent,
                &document,
                pdfPath,
                0,
                QObject::tr("Print Rosters"),
                QPageLayout::Landscape,
                false,
                QPageSize::Letter,
                false
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

} // namespace RosterTemplatePrintService
