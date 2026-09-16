#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPdfDocument>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest>
#include <QUuid>
#include <zlib.h>

#include <cstdio>
#include <algorithm>
#include <utility>

#include "data/database/database_schema_manager.h"
#include "domain/models/class_transfer.h"
#include "domain/models/speaking_evaluation.h"
#include "features/classes/services/class_transfer_json_codec.h"

namespace
{
constexpr int StartupTimeoutMs = 60000;
constexpr int SubPrepOutputTimeoutMs = 180000;
constexpr int SubPrepVisualTimeoutMs = 120000;
constexpr qint64 Phase0FinalNormalWorkingSetTargetBytes =
    250LL * 1024LL * 1024LL;
constexpr qint64 Phase0TransientDiagnosticCeilingBytes =
    512LL * 1024LL * 1024LL;
constexpr int LargeClassTransferTeacherCount = 12;
constexpr int LargeClassTransferClassCount = 48;
constexpr int LargeClassTransferRosterColumnCount = 6;
constexpr int LargeClassTransferRosterRowCount = 30;
constexpr int LargeClassTransferEvaluationCount = 2;
constexpr int LargeSpeakingEvaluationClassCount = 96;
constexpr int LargeSpeakingEvaluationRowCount = SpeakingEval::RowCount;
constexpr int LargeStaffDirectoryEntryCount = 96;
constexpr int LargeStaffDirectoryNativeColumnCount = 6;
constexpr int LargeStaffDirectoryGsColumnCount = 5;

struct FixtureTableExpectation
{
    const char* name;
    int expectedRows;
};

QString processOutput(
    QProcess& process
    )
{
    return QStringLiteral("stdout:\n%1\nstderr:\n%2")
        .arg(
            QString::fromLocal8Bit(process.readAllStandardOutput()),
            QString::fromLocal8Bit(process.readAllStandardError())
            );
}

bool writeDiagnosticFile(
    const QString& path,
    const QByteArray& contents,
    QString* errorMessage
    )
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Unable to write %1: %2")
                    .arg(path, file.errorString());
        }
        return false;
    }

    if (file.write(contents) != contents.size())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Unable to finish writing %1: %2")
                    .arg(path, file.errorString());
        }
        return false;
    }

    return file.flush() && file.error() == QFile::NoError;
}

void appendLittleEndian16(
    QByteArray& data,
    quint16 value
    )
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendLittleEndian32(
    QByteArray& data,
    quint32 value
    )
{
    appendLittleEndian16(data, static_cast<quint16>(value & 0xffff));
    appendLittleEndian16(data, static_cast<quint16>((value >> 16) & 0xffff));
}

struct ScheduleImportZipEntry
{
    QByteArray name;
    QByteArray contents;
    quint32 crc = 0;
    quint32 localOffset = 0;
};

QByteArray storedZip(
    QList<ScheduleImportZipEntry> entries
    )
{
    QByteArray result;
    for (ScheduleImportZipEntry& entry : entries)
    {
        entry.localOffset = static_cast<quint32>(result.size());
        entry.crc = static_cast<quint32>(
            crc32(
                crc32(0L, Z_NULL, 0),
                reinterpret_cast<const Bytef*>(entry.contents.constData()),
                static_cast<uInt>(entry.contents.size())
                )
            );
        appendLittleEndian32(result, 0x04034b50);
        appendLittleEndian16(result, 20);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian32(result, entry.crc);
        appendLittleEndian32(result, entry.contents.size());
        appendLittleEndian32(result, entry.contents.size());
        appendLittleEndian16(result, entry.name.size());
        appendLittleEndian16(result, 0);
        result.append(entry.name);
        result.append(entry.contents);
    }

    const quint32 centralOffset = static_cast<quint32>(result.size());
    for (const ScheduleImportZipEntry& entry : entries)
    {
        appendLittleEndian32(result, 0x02014b50);
        appendLittleEndian16(result, 20);
        appendLittleEndian16(result, 20);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian32(result, entry.crc);
        appendLittleEndian32(result, entry.contents.size());
        appendLittleEndian32(result, entry.contents.size());
        appendLittleEndian16(result, entry.name.size());
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian16(result, 0);
        appendLittleEndian32(result, 0);
        appendLittleEndian32(result, entry.localOffset);
        result.append(entry.name);
    }

    const quint32 centralSize =
        static_cast<quint32>(result.size()) - centralOffset;
    appendLittleEndian32(result, 0x06054b50);
    appendLittleEndian16(result, 0);
    appendLittleEndian16(result, 0);
    appendLittleEndian16(result, entries.size());
    appendLittleEndian16(result, entries.size());
    appendLittleEndian32(result, centralSize);
    appendLittleEndian32(result, centralOffset);
    appendLittleEndian16(result, 0);
    return result;
}

QString spreadsheetColumnName(int column)
{
    QString result;
    for (int value = column; value > 0; value = (value - 1) / 26)
    {
        result.prepend(
            QChar(
                static_cast<ushort>('A' + ((value - 1) % 26))
                )
            );
    }
    return result;
}

QByteArray largeScheduleImportWorksheet(
    bool conflictFree
    )
{
    const QStringList teacherNames{
        QStringLiteral("김선생"), QStringLiteral("이선생"),
        QStringLiteral("박선생"), QStringLiteral("최선생"),
        QStringLiteral("정선생"), QStringLiteral("한선생"),
        QStringLiteral("강선생"), QStringLiteral("윤선생"),
        QStringLiteral("조선생"), QStringLiteral("임선생"),
        QStringLiteral("장선생"), QStringLiteral("오선생"),
        QStringLiteral("서선생"), QStringLiteral("신선생"),
        QStringLiteral("권선생"), QStringLiteral("황선생"),
        QStringLiteral("안선생"), QStringLiteral("송선생"),
        QStringLiteral("류선생"), QStringLiteral("전선생"),
        QStringLiteral("홍선생"), QStringLiteral("문선생"),
        QStringLiteral("양선생"), QStringLiteral("배선생")
    };
    const QStringList grades{
        QStringLiteral("E4"), QStringLiteral("E5"),
        QStringLiteral("E6"), QStringLiteral("M1")
    };
    const QStringList levels{
        QStringLiteral("Theseus"), QStringLiteral("Artemis"),
        QStringLiteral("Helios"), QStringLiteral("Elephantus")
    };
    const QStringList days{
        QStringLiteral("MON"), QStringLiteral("TUE"),
        QStringLiteral("WED"), QStringLiteral("THU"),
        QStringLiteral("FRI")
    };

    QByteArray xml = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData><row r=\"1\">"
        );

    for (int block = 0; block < 5; ++block)
    {
        const int startColumn = 1 + block * 7;
        const QString nameCell =
            spreadsheetColumnName(startColumn) + QStringLiteral("1");
        xml += QStringLiteral(
            "<c r=\"%1\" t=\"inlineStr\"><is><t>Alice</t></is></c>"
            ).arg(nameCell).toUtf8();
        for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex)
        {
            const QString cell =
                spreadsheetColumnName(startColumn + 1 + dayIndex)
                + QStringLiteral("1");
            xml += QStringLiteral(
                "<c r=\"%1\" t=\"inlineStr\"><is><t>%2</t></is></c>"
                ).arg(cell, days[dayIndex]).toUtf8();
        }
    }
    xml.append(QByteArrayLiteral("</row>"));

    for (int candidate = 0; candidate < 96; ++candidate)
    {
        const int block = candidate / 20;
        const int localRow = candidate % 20;
        const int row = localRow + 2;
        const int startColumn = 1 + block * 7;
        const int teacherIndex = candidate / 4;
        const int courseIndex = candidate % 4;
        const QString time =
            QStringLiteral("%1:00~%1:55")
                .arg(
                    3
                    + (
                        conflictFree
                            ? localRow % 7
                            : localRow % 6
                        )
                    );
        const QString timeCell =
            spreadsheetColumnName(startColumn) + QString::number(row);
        const int dayColumn =
            startColumn + 1 + (candidate % days.size());
        const QString classCell =
            spreadsheetColumnName(dayColumn) + QString::number(row);
        const QString classGrade =
            conflictFree
                ? QStringLiteral("E6")
                : grades[courseIndex];
        const QString classLevel =
            conflictFree
                ? QStringList{
                      QStringLiteral("Helios"),
                      QStringLiteral("Poseidon"),
                      QStringLiteral("Gaia"),
                      QStringLiteral("Hera")
                  }[courseIndex]
                : levels[courseIndex];
        const QString classValue =
            QStringLiteral("%1 (%2)&#10;%3-%4")
                .arg(teacherNames[teacherIndex])
                .arg(301 + teacherIndex)
                .arg(classGrade)
                .arg(classLevel);

        xml += QStringLiteral("<row r=\"%1\">").arg(row).toUtf8();
        xml += QStringLiteral(
            "<c r=\"%1\" t=\"inlineStr\"><is><t>%2</t></is></c>"
            ).arg(timeCell, time).toUtf8();
        xml += QStringLiteral(
            "<c r=\"%1\" t=\"inlineStr\"><is><t>%2</t></is></c>"
            ).arg(classCell, classValue).toUtf8();
        xml += QByteArrayLiteral("</row>");
    }

    xml += QByteArrayLiteral("</sheetData></worksheet>");
    return xml;
}

QByteArray emptyScheduleImportWorksheet()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData></sheetData></worksheet>"
        );
}

QByteArray largeScheduleImportWorkbookData(
    bool conflictFree
    )
{
    const QByteArray workbook = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\""
        " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"Large Import\" sheetId=\"1\" r:id=\"rId1\"/>"
        "<sheet name=\"Empty\" sheetId=\"2\" r:id=\"rId2\"/></sheets>"
        "</workbook>"
        );
    const QByteArray relationships = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>"
        "</Relationships>"
        );
    const QByteArray styles = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"1\"><font><color rgb=\"FF000000\"/></font></fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<cellXfs count=\"1\"><xf fontId=\"0\" fillId=\"0\"/></cellXfs>"
        "</styleSheet>"
        );
    return storedZip({
        {QByteArrayLiteral("xl/workbook.xml"), workbook},
        {QByteArrayLiteral("xl/_rels/workbook.xml.rels"), relationships},
        {QByteArrayLiteral("xl/styles.xml"), styles},
        {
            QByteArrayLiteral("xl/worksheets/sheet1.xml"),
            largeScheduleImportWorksheet(conflictFree)
        },
        {
            QByteArrayLiteral("xl/worksheets/sheet2.xml"),
            emptyScheduleImportWorksheet()
        }
    });
}

bool writeLargeScheduleImportWorkbook(
    const QString& path,
    bool conflictFree = false
    )
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    const QByteArray data = largeScheduleImportWorkbookData(conflictFree);
    return file.write(data) == data.size()
        && file.flush()
        && file.error() == QFile::NoError;
}

QByteArray calendarInlineStringCell(
    const QString& reference,
    const QString& value,
    int style = 0
    )
{
    return QStringLiteral(
        "<c r=\"%1\" s=\"%2\" t=\"inlineStr\"><is><t>%3</t></is></c>"
        )
        .arg(reference)
        .arg(style)
        .arg(value)
        .toUtf8();
}

QByteArray calendarNumericCell(
    const QString& reference,
    int value,
    int style
    )
{
    return QStringLiteral(
        "<c r=\"%1\" s=\"%2\"><v>%3</v></c>"
        )
        .arg(reference)
        .arg(style)
        .arg(value)
        .toUtf8();
}

QByteArray largeCalendarImportWorksheet()
{
    const QStringList monthNames{
        QStringLiteral("January"), QStringLiteral("February"),
        QStringLiteral("March"), QStringLiteral("April"),
        QStringLiteral("May"), QStringLiteral("June"),
        QStringLiteral("July"), QStringLiteral("August"),
        QStringLiteral("September"), QStringLiteral("October"),
        QStringLiteral("November"), QStringLiteral("December")
    };

    QByteArray xml = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData>"
        );

    for (int month = 1; month <= 12; ++month)
    {
        const int blockRow = 1 + (month - 1) * 16;
        xml += QStringLiteral("<row r=\"%1\">").arg(blockRow).toUtf8();
        xml += calendarInlineStringCell(
            QStringLiteral("A%1").arg(blockRow),
            monthNames.at(month - 1)
            );
        if (month == 1)
        {
            xml += calendarInlineStringCell(
                QStringLiteral("B%1").arg(blockRow),
                QStringLiteral("2026")
                );
        }
        xml += QByteArrayLiteral("</row>");

        const QDate firstOfMonth(2026, month, 1);
        const QDate firstGridDate = firstOfMonth.addDays(
            -(firstOfMonth.dayOfWeek() - Qt::Monday)
            );
        for (int gridRow = 0; gridRow < 6; ++gridRow)
        {
            const int row = blockRow + 2 + gridRow;
            xml += QStringLiteral("<row r=\"%1\">").arg(row).toUtf8();
            for (int column = 0; column < 7; ++column)
            {
                const QDate date = firstGridDate.addDays(
                    gridRow * 7 + column
                    );
                if (date.month() != month)
                {
                    continue;
                }

                const int style =
                    date.dayOfWeek() >= Qt::Saturday
                        ? 3
                        : date.day() % 10 == 0
                            ? 2
                            : 1;
                xml += calendarNumericCell(
                    spreadsheetColumnName(column + 1)
                        + QString::number(row),
                    date.day(),
                    style
                    );
            }
            xml += QByteArrayLiteral("</row>");
        }
    }

    xml += QByteArrayLiteral("<row r=\"20\">");
    xml += calendarInlineStringCell(
        QStringLiteral("Z20"),
        QStringLiteral("DYB Workshop"),
        1
        );
    xml += QByteArrayLiteral("</row><row r=\"21\">");
    xml += calendarInlineStringCell(
        QStringLiteral("Z21"),
        QStringLiteral("Red Day"),
        2
        );
    xml += QByteArrayLiteral("</row><row r=\"22\">");
    xml += calendarInlineStringCell(
        QStringLiteral("Z22"),
        QStringLiteral("Weekend"),
        3
        );
    xml += QByteArrayLiteral(
        "</row></sheetData>"
        "<mergeCells count=\"12\">"
        );
    for (int month = 1; month <= 12; ++month)
    {
        const int blockRow = 1 + (month - 1) * 16;
        xml += QStringLiteral(
            "<mergeCell ref=\"A%1:B%1\"/>"
            ).arg(blockRow).toUtf8();
    }
    xml += QByteArrayLiteral("</mergeCells></worksheet>");
    return xml;
}

QByteArray emptyCalendarImportWorksheet()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData><row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Archive</t></is></c></row>"
        "</sheetData></worksheet>"
        );
}

QByteArray largeCalendarImportWorkbookData()
{
    const QByteArray workbook = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\""
        " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"Calendar 2026\" sheetId=\"1\" r:id=\"rId1\"/>"
        "<sheet name=\"Archive\" sheetId=\"2\" r:id=\"rId2\"/></sheets>"
        "</workbook>"
        );
    const QByteArray relationships = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>"
        "</Relationships>"
        );
    const QByteArray styles = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"1\"><font><color rgb=\"FF000000\"/></font></fonts>"
        "<fills count=\"5\">"
        "<fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFFFF2CC\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFFFCCCC\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFD9EAD3\"/></patternFill></fill>"
        "</fills>"
        "<cellXfs count=\"4\">"
        "<xf fontId=\"0\" fillId=\"0\"/>"
        "<xf fontId=\"0\" fillId=\"2\"/>"
        "<xf fontId=\"0\" fillId=\"3\"/>"
        "<xf fontId=\"0\" fillId=\"4\"/>"
        "</cellXfs></styleSheet>"
        );

    return storedZip({
        {QByteArrayLiteral("xl/workbook.xml"), workbook},
        {QByteArrayLiteral("xl/_rels/workbook.xml.rels"), relationships},
        {QByteArrayLiteral("xl/styles.xml"), styles},
        {
            QByteArrayLiteral("xl/worksheets/sheet1.xml"),
            largeCalendarImportWorksheet()
        },
        {
            QByteArrayLiteral("xl/worksheets/sheet2.xml"),
            emptyCalendarImportWorksheet()
        }
    });
}

bool writeLargeCalendarImportWorkbook(
    const QString& path
    )
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    const QByteArray data = largeCalendarImportWorkbookData();
    return file.write(data) == data.size()
        && file.flush()
        && file.error() == QFile::NoError;
}

bool thresholdExceeded(
    const char* environmentVariable,
    double value,
    QString* message
    )
{
    bool ok = false;

    const int threshold =
        qEnvironmentVariableIntValue(
            environmentVariable,
            &ok
            );

    if (!ok || threshold <= 0 || value <= threshold)
    {
        return false;
    }

    *message =
        QStringLiteral("%1 was %2 ms, exceeding %3 ms")
            .arg(
                QString::fromLatin1(environmentVariable),
                QString::number(value, 'f', 0),
                QString::number(threshold)
                );

    return true;
}

ClassTransferPackage largeClassTransferPackage()
{
    ClassTransferPackage package;
    package.exportedAtUtc = QDateTime::fromString(
        QStringLiteral("2026-09-16T00:00:00.000Z"),
        Qt::ISODateWithMs
        );

    const QStringList teacherKoreanNames{
        QStringLiteral("김민수"),
        QStringLiteral("이서준"),
        QStringLiteral("박지훈"),
        QStringLiteral("최도윤"),
        QStringLiteral("정하준"),
        QStringLiteral("강예준"),
        QStringLiteral("윤지호"),
        QStringLiteral("장현우"),
        QStringLiteral("임서연"),
        QStringLiteral("한유진"),
        QStringLiteral("오지민"),
        QStringLiteral("서하은")
    };
    const QStringList teacherEnglishNames{
        QStringLiteral("Alex Kim"),
        QStringLiteral("Brian Lee"),
        QStringLiteral("Chris Park"),
        QStringLiteral("David Choi"),
        QStringLiteral("Evan Jung"),
        QStringLiteral("Frank Kang"),
        QStringLiteral("Grace Yoon"),
        QStringLiteral("Henry Jang"),
        QStringLiteral("Irene Lim"),
        QStringLiteral("Jason Han"),
        QStringLiteral("Kevin Oh"),
        QStringLiteral("Laura Seo")
    };
    const QStringList teacherRomanizations{
        QStringLiteral("Kim Min Su"),
        QStringLiteral("Lee Seo Jun"),
        QStringLiteral("Park Ji Hun"),
        QStringLiteral("Choi Do Yun"),
        QStringLiteral("Jung Ha Jun"),
        QStringLiteral("Kang Ye Jun"),
        QStringLiteral("Yoon Ji Ho"),
        QStringLiteral("Jang Hyun Woo"),
        QStringLiteral("Im Seo Yeon"),
        QStringLiteral("Han Yu Jin"),
        QStringLiteral("Oh Ji Min"),
        QStringLiteral("Seo Ha Eun")
    };

    for (int index = 0;
         index < LargeClassTransferTeacherCount;
         ++index)
    {
        const QString ordinal =
            QStringLiteral("%1").arg(index + 1, 2, 10, QChar('0'));
        Teacher teacher;
        teacher.teacherKr = teacherKoreanNames.at(index);
        teacher.teacherEn = teacherEnglishNames.at(index);
        teacher.preferredRomanization = teacherRomanizations.at(index);
        teacher.preferredName = teacher.teacherEn;
        teacher.roomNumber = QStringLiteral("T-%1").arg(ordinal);
        teacher.birthday = QStringLiteral("02-29");
        teacher.phoneNumber = QStringLiteral("010-7000-%1").arg(ordinal);
        teacher.wifiName = QStringLiteral("Transfer WiFi %1").arg(ordinal);
        teacher.wifiPassword = QStringLiteral("transfer-password-%1").arg(ordinal);
        teacher.internetType = QStringLiteral("Both");
        teacher.zoomId = QStringLiteral("transfer-%1").arg(ordinal);
        teacher.zoomPassword = QStringLiteral("transfer-zoom-%1").arg(ordinal);
        teacher.projectionType = QStringLiteral("Any");
        teacher.notes = QStringLiteral(
            "Deterministic transfer teacher %1 with a complete profile."
            ).arg(ordinal);
        package.teachers.append({
            QStringLiteral("transfer-teacher-%1").arg(index + 1),
            teacher
        });
    }

    const QStringList rosterColumns{
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Memo"),
        QStringLiteral("Reading"),
        QStringLiteral("Writing"),
        QStringLiteral("Attendance")
    };
    const QVector<int> rosterWidths{180, 190, 260, 160, 160, 140};
    const QStringList courseGrades{
        QStringLiteral("E4"),
        QStringLiteral("E5"),
        QStringLiteral("E6"),
        QStringLiteral("M1")
    };
    const QStringList courseLevels{
        QStringLiteral("Theseus"),
        QStringLiteral("Artemis"),
        QStringLiteral("Helios"),
        QStringLiteral("Elephantus")
    };

    for (int index = 0;
         index < LargeClassTransferClassCount;
         ++index)
    {
        const int ordinal = index + 1;
        ClassTransferClass transferClass;
        transferClass.key = QStringLiteral("transfer-class-%1").arg(ordinal);
        transferClass.name = QStringLiteral("Transferred Class %1").arg(ordinal, 2, 10, QChar('0'));
        transferClass.teacherKey = QStringLiteral(
            "transfer-teacher-%1"
            ).arg((index % LargeClassTransferTeacherCount) + 1);
        transferClass.info.classId = 1000 + ordinal;
        const int courseIndex = index % 4;
        transferClass.info.classGrade = courseGrades.at(courseIndex);
        transferClass.info.classLevel = courseLevels.at(courseIndex);
        transferClass.info.readingBook.clear();
        transferClass.info.essayBook = courseIndex == 3
            ? QStringLiteral("N/A")
            : QString();
        transferClass.info.classColor =
            index % 2 == 0
                ? QStringLiteral("#DDEBFF")
                : QStringLiteral("#E3F5E5");
        transferClass.info.fontColor = QStringLiteral("#172B4D");
        transferClass.info.notes = QStringLiteral(
            "Deterministic multi-class transfer notes for class %1."
            ).arg(ordinal);
        transferClass.info.timeFillerActivities =
            QStringLiteral("Transfer activity %1").arg(ordinal);

        const int slot = index % 24;
        const int hour = (slot % 12) + 1;
        const QString period = slot < 12
            ? QStringLiteral("AM")
            : QStringLiteral("PM");
        const QString startTime = QStringLiteral("%1:00 %2")
            .arg(hour)
            .arg(period);
        const QString endTime = QStringLiteral("%1:50 %2")
            .arg(hour)
            .arg(period);
        transferClass.info.classTimes.append({
            index < 24 ? QStringLiteral("Saturday")
                       : QStringLiteral("Sunday"),
            startTime,
            endTime
        });

        transferClass.roster.columns = rosterColumns;
        transferClass.roster.columnWidths = rosterWidths;
        for (int row = 0; row < LargeClassTransferRosterRowCount; ++row)
        {
            const QString studentOrdinal =
                QStringLiteral("%1").arg(row + 1, 2, 10, QChar('0'));
            transferClass.roster.rows.append({
                QStringLiteral("Transfer Student %1-%2")
                    .arg(ordinal, 2, 10, QChar('0'))
                    .arg(studentOrdinal),
                QStringLiteral("학생 %1-%2")
                    .arg(ordinal, 2, 10, QChar('0'))
                    .arg(studentOrdinal),
                QStringLiteral("Transfer memo %1-%2")
                    .arg(ordinal, 2, 10, QChar('0'))
                    .arg(studentOrdinal),
                QStringLiteral("Book %1").arg((row % 5) + 1),
                QStringLiteral("Essay %1").arg((row % 4) + 1),
                row % 3 == 0
                    ? QStringLiteral("Present")
                    : QStringLiteral("Recorded")
            });
        }

        for (int evaluationIndex = 0;
             evaluationIndex < LargeClassTransferEvaluationCount;
             ++evaluationIndex)
        {
            ClassTransferEvaluation evaluation;
            evaluation.name = QStringLiteral("Transfer Evaluation %1")
                .arg(evaluationIndex + 1);
            evaluation.rows = SpeakingEval::emptyRows();
            for (int row = 0; row < evaluation.rows.size(); ++row)
            {
                evaluation.rows[row][SpeakingEval::toInt(
                    SpeakingEvalColumn::EnglishName)] =
                    transferClass.roster.rows[row % transferClass.roster.rows.size()]
                        .value(0);
                evaluation.rows[row][SpeakingEval::toInt(
                    SpeakingEvalColumn::KoreanName)] =
                    transferClass.roster.rows[row % transferClass.roster.rows.size()]
                        .value(1);
                evaluation.rows[row][SpeakingEval::toInt(
                    SpeakingEvalColumn::Grammar)] =
                    row % 2 == 0
                        ? QStringLiteral("A")
                        : QStringLiteral("B+");
                evaluation.rows[row][SpeakingEval::toInt(
                    SpeakingEvalColumn::Comments)] =
                    QStringLiteral("Deterministic transfer evaluation comment %1-%2.")
                        .arg(ordinal)
                        .arg(row + 1);
            }
            transferClass.evaluations.append(evaluation);
        }

        package.classes.append(transferClass);
    }

    return package;
}

bool writeLargeClassTransferPackage(
    const QString& path
    )
{
    return ClassTransferJsonCodec::saveFile(
        path,
        largeClassTransferPackage()
        ).has_value();
}

bool writeRepresentativeStartupSettings(
    const QString& settingsRoot
    )
{
    QDir directory(settingsRoot);
    if (!directory.mkpath(QStringLiteral("PaperCloud")))
    {
        return false;
    }

    QSettings settings(
        directory.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );

    // Keep this profile explicit rather than inheriting a developer's local
    // defaults. The data fixture stores schedule settings; these are the
    // application-wide settings read before the window is constructed.
    settings.setValue(QStringLiteral("options/theme"), 1);
    settings.setValue(QStringLiteral("options/fontSize"), 2);
    settings.setValue(QStringLiteral("options/language"), 1);
    settings.setValue(QStringLiteral("options/saveMode"), 0);
    settings.setValue(QStringLiteral("options/documentPageSpacing"), 2);
    settings.setValue(QStringLiteral("options/documentViewerBackground"), 1);
    settings.setValue(QStringLiteral("options/sidebarTooltipsEnabled"), true);
    settings.setValue(QStringLiteral("options/sidebarMarqueeEnabled"), false);
    settings.setValue(
        QStringLiteral("updates/automaticChecksEnabled"),
        false
        );
    settings.sync();

    return settings.status() == QSettings::NoError;
}

bool executeStartupFixtureSql(
    QSqlDatabase& database,
    const QString& sqlPath,
    QString* errorMessage
    )
{
    QFile sqlFile(sqlPath);
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage =
            QStringLiteral("Unable to open startup fixture SQL: %1")
                .arg(sqlFile.errorString());
        return false;
    }

    QSqlQuery query(database);
    const QStringList statements =
        QString::fromUtf8(sqlFile.readAll())
            .split(QChar(';'), Qt::SkipEmptyParts);

    for (const QString& rawStatement : statements)
    {
        const QString statement = rawStatement.trimmed();
        if (!statement.isEmpty() && !query.exec(statement))
        {
            *errorMessage =
                QStringLiteral("Fixture statement failed: %1 (%2)")
                    .arg(
                        statement.simplified().left(160),
                        query.lastError().text()
                        );
            return false;
        }
    }

    return true;
}

bool createStartupFixture(
    const QString& fixtureName,
    const QString& fixturePath,
    QString* errorMessage
    )
{
    const QString sqlPath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/tests/fixtures/workspaces/%1.sql"
            ).arg(fixtureName);

    const QString connectionName =
        QStringLiteral("startup-fixture-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);

        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            const Status schemaStatus =
                DatabaseSchemaManager::ensureSchema(database);
            if (!schemaStatus)
            {
                *errorMessage = schemaStatus.error();
            }
            else
            {
                success = executeStartupFixtureSql(
                    database,
                    sqlPath,
                    errorMessage
                    );
            }
        }

        database.close();
        database = QSqlDatabase();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

bool createRepresentativeStartupFixture(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    return createStartupFixture(
        QStringLiteral("representative_startup"),
        fixturePath,
        errorMessage
        );
}

bool createLargeStartupFixture(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    return createStartupFixture(
        QStringLiteral("large_startup"),
        fixturePath,
        errorMessage
        );
}

bool prepareLargeSubPrepOutputFixture(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    const QString connectionName =
        QStringLiteral("startup-sub-prep-output-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);

        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else if (!database.transaction())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            QSqlQuery query(database);
            if (!query.exec(QStringLiteral("DELETE FROM class_times")))
            {
                *errorMessage = query.lastError().text();
                database.rollback();
            }
            else if (!query.exec(
                         QStringLiteral(
                             "UPDATE roster_columns SET name = "
                             "CASE position "
                             "WHEN 0 THEN 'English' "
                             "WHEN 1 THEN 'Korean' "
                             "ELSE 'Memo' END"
                             )
                         ))
            {
                *errorMessage = query.lastError().text();
                database.rollback();
            }
            else if (!query.prepare(
                         QStringLiteral(
                             "INSERT INTO class_times "
                             "(class_id, day, start_time, end_time) "
                             "VALUES (?, ?, ?, ?)"
                             )
                         ))
            {
                *errorMessage = query.lastError().text();
                database.rollback();
            }
            else
            {
                bool insertSucceeded = true;
                const QStringList weekdays{
                    QStringLiteral("Monday"),
                    QStringLiteral("Tuesday"),
                    QStringLiteral("Wednesday"),
                    QStringLiteral("Thursday"),
                    QStringLiteral("Friday")
                };
                for (int classId = 1; classId <= 96; ++classId)
                {
                    QString day = QStringLiteral("Saturday");
                    int slot = 0;

                    if (classId <= 30)
                    {
                        day = weekdays.at((classId - 1) % 5);
                        slot = (classId - 1) / 5;
                    }

                    const int hour = 4 + slot;
                    query.bindValue(0, classId);
                    query.bindValue(1, day);
                    query.bindValue(2, QStringLiteral("%1:00 PM").arg(hour));
                    query.bindValue(3, QStringLiteral("%1:50 PM").arg(hour));
                    if (!query.exec())
                    {
                        *errorMessage =
                            QStringLiteral("Unable to prepare class %1 schedule: %2")
                                .arg(classId)
                                .arg(query.lastError().text());
                        insertSucceeded = false;
                        break;
                    }
                }

                if (insertSucceeded && database.commit())
                {
                    success = true;
                }
                else if (insertSucceeded)
                {
                    *errorMessage = database.lastError().text();
                    database.rollback();
                }
                else
                {
                    database.rollback();
                }
            }
        }

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

bool prepareLargeSubPrepEmptyFixture(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    const QString connectionName =
        QStringLiteral("startup-sub-prep-empty-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);

        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else if (!database.transaction())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            QSqlQuery query(database);
            bool cleared = true;
            for (const QString& tableName : {
                     QStringLiteral("class_times"),
                     QStringLiteral("class_intensive_times"),
                     QStringLiteral("intensive_slot_states")
                 })
            {
                if (!query.exec(QStringLiteral("DELETE FROM %1").arg(tableName)))
                {
                    *errorMessage = query.lastError().text();
                    cleared = false;
                    break;
                }
            }

            if (cleared && database.commit())
            {
                success = true;
            }
            else
            {
                if (cleared)
                {
                    *errorMessage = database.lastError().text();
                }
                database.rollback();
            }
        }

        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

QString alphabeticFixtureToken(int value)
{
    QString token;
    do
    {
        token.prepend(
            QChar(
                static_cast<ushort>(
                    'A' + (value % 26)
                    )
                )
            );
        value = value / 26 - 1;
    }
    while (value >= 0);
    return token;
}

bool addLargeSpeakingEvaluationFixtureData(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    const QString connectionName =
        QStringLiteral("startup-large-speaking-evaluation-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);

        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            QSqlQuery pragma(database);
            if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
            {
                *errorMessage = pragma.lastError().text();
            }
            else if (!database.transaction())
            {
                *errorMessage = database.lastError().text();
            }
            else
            {
                QSqlQuery evaluation(database);
                QSqlQuery row(database);
                evaluation.prepare(QStringLiteral(
                    "INSERT INTO speaking_evaluations "
                    "(class_id, evaluation_name) VALUES (?, ?)"
                    ));
                row.prepare(QStringLiteral(
                    "INSERT INTO speaking_eval_data ("
                    "evaluation_id, row_index, col_0, col_1, col_2, "
                    "col_3, col_4, col_5, col_6, col_7, col_8, col_9, col_10"
                    ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                    ));

                bool valid = true;
                for (
                    int classId = 1;
                    classId <= LargeSpeakingEvaluationClassCount && valid;
                    ++classId
                    )
                {
                    evaluation.bindValue(0, classId);
                    evaluation.bindValue(1, QStringLiteral("Winter"));
                    if (!evaluation.exec())
                    {
                        *errorMessage = evaluation.lastError().text();
                        valid = false;
                        break;
                    }

                    bool idValid = false;
                    const int evaluationId =
                        evaluation.lastInsertId().toInt(&idValid);
                    if (!idValid || evaluationId <= 0)
                    {
                        *errorMessage =
                            QStringLiteral(
                                "SQLite did not return the speaking-evaluation id."
                                );
                        valid = false;
                        break;
                    }

                    for (
                        int rowIndex = 0;
                        rowIndex < LargeSpeakingEvaluationRowCount;
                        ++rowIndex
                        )
                    {
                        const QString classToken =
                            alphabeticFixtureToken(classId - 1);
                        const QString rowToken =
                            alphabeticFixtureToken(rowIndex);
                        const QStringList values{
                            QString(),
                            QStringLiteral("Student%1%2")
                                .arg(classToken, rowToken),
                            QString(),
                            rowIndex % 5 == 0 ? QStringLiteral("A+")
                                              : QStringLiteral("A"),
                            rowIndex % 5 == 1 ? QStringLiteral("B+")
                                              : QStringLiteral("A"),
                            rowIndex % 5 == 2 ? QStringLiteral("B")
                                              : QStringLiteral("A"),
                            QStringLiteral("A"),
                            QStringLiteral("A"),
                            rowIndex % 5 == 3 ? QStringLiteral("B+")
                                              : QStringLiteral("A"),
                            QString(),
                            QStringLiteral(
                                "[Did Well]\nclear speaking\nstrong grammar\n"
                                "[Needs Improvement]\npractice pronunciation\n"
                                "review vocabulary"
                                )
                        };

                        row.bindValue(0, evaluationId);
                        row.bindValue(1, rowIndex);
                        for (int column = 0; column < values.size(); ++column)
                        {
                            row.bindValue(column + 2, values.at(column));
                        }
                        if (!row.exec())
                        {
                            *errorMessage = row.lastError().text();
                            valid = false;
                            break;
                        }
                    }
                }

                if (valid && database.commit())
                {
                    success = true;
                }
                else if (valid)
                {
                    *errorMessage = database.lastError().text();
                }
                else
                {
                    database.rollback();
                }
            }
        }

        database.close();
        database = QSqlDatabase();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

bool addLargeStaffDirectoryFixtureData(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    const QString connectionName =
        QStringLiteral("startup-large-staff-directory-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);

        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            QSqlQuery pragma(database);
            if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
            {
                *errorMessage = pragma.lastError().text();
            }
            else if (!database.transaction())
            {
                *errorMessage = database.lastError().text();
            }
            else
            {
                QSqlQuery native(database);
                QSqlQuery gs(database);
                native.prepare(QStringLiteral(
                    "INSERT INTO native_english_teachers "
                    "(name, position, phone_number, birthday, nationality, email) "
                    "VALUES (?, ?, ?, ?, ?, ?)"
                    ));
                gs.prepare(QStringLiteral(
                    "INSERT INTO gs_team "
                    "(name, korean_name, position, phone_number, birthday) "
                    "VALUES (?, ?, ?, ?, ?)"
                    ));

                const QStringList nativePositions{
                    QStringLiteral("NET"),
                    QStringLiteral("E5 Athena"),
                    QStringLiteral("M1 Song's"),
                    QStringLiteral("Team Leader")
                };
                const QStringList gsPositions{
                    QStringLiteral("Branch Manager"),
                    QStringLiteral("M3"),
                    QStringLiteral("M2"),
                    QStringLiteral("C1")
                };

                bool valid = true;
                for (
                    int index = 0;
                    index < LargeStaffDirectoryEntryCount && valid;
                    ++index
                    )
                {
                    const QString token = alphabeticFixtureToken(index);
                    const QString birthday =
                        QStringLiteral("01-%1")
                            .arg(1 + (index % 28), 2, 10, QLatin1Char('0'));

                    native.bindValue(
                        0,
                        QStringLiteral("Native Staff %1").arg(token)
                        );
                    native.bindValue(
                        1,
                        nativePositions.at(index % nativePositions.size())
                        );
                    native.bindValue(
                        2,
                        QStringLiteral("010-7000-%1")
                            .arg(index + 1, 4, 10, QLatin1Char('0'))
                        );
                    native.bindValue(3, birthday);
                    native.bindValue(
                        4,
                        QStringLiteral("Nationality %1").arg(token)
                        );
                    native.bindValue(
                        5,
                        QStringLiteral("native.%1@example.test").arg(token)
                        );
                    if (!native.exec())
                    {
                        *errorMessage = native.lastError().text();
                        valid = false;
                        break;
                    }

                    gs.bindValue(
                        0,
                        QStringLiteral("GS Staff %1").arg(token)
                        );
                    gs.bindValue(
                        1,
                        QStringLiteral("GS Korean Staff %1").arg(token)
                        );
                    gs.bindValue(
                        2,
                        gsPositions.at(index % gsPositions.size())
                        );
                    gs.bindValue(
                        3,
                        QStringLiteral("010-8000-%1")
                            .arg(index + 1, 4, 10, QLatin1Char('0'))
                        );
                    gs.bindValue(4, birthday);
                    if (!gs.exec())
                    {
                        *errorMessage = gs.lastError().text();
                        valid = false;
                        break;
                    }
                }

                if (valid && database.commit())
                {
                    success = true;
                }
                else if (valid)
                {
                    *errorMessage = database.lastError().text();
                }
                else
                {
                    database.rollback();
                }
            }
        }

        database.close();
        database = QSqlDatabase();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

bool createLegacyStartupSource(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    const QString sqlPath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/tests/fixtures/workspaces/legacy_startup.sql"
            );

    const QString connectionName =
        QStringLiteral("startup-legacy-fixture-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);

        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else if (!executeStartupFixtureSql(database, sqlPath, errorMessage))
        {
            // The SQL fixture failed before migration began.
        }
        else
        {
            success = true;
        }

        database.close();
        database = QSqlDatabase();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

bool createLegacyStartupFixture(
    const QString& fixturePath,
    QString* errorMessage
    )
{
    if (!createLegacyStartupSource(fixturePath, errorMessage))
    {
        return false;
    }

    const QString connectionName =
        QStringLiteral("startup-legacy-migration-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(fixturePath);

        if (!database.open())
        {
            *errorMessage = database.lastError().text();
        }
        else
        {
            const Status schemaStatus =
                DatabaseSchemaManager::ensureSchema(database);
            if (!schemaStatus)
            {
                *errorMessage = schemaStatus.error();
            }
            else
            {
                success = true;
            }
        }

        database.close();
        database = QSqlDatabase();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return success;
}

void printRepresentativeCheckpoint(
    const QJsonObject& checkpoint
    )
{
    const QJsonObject metrics =
        checkpoint.value(QStringLiteral("metrics")).toObject();
    const QJsonObject memory =
        checkpoint.value(QStringLiteral("memory")).toObject();
    const QByteArray name =
        checkpoint.value(QStringLiteral("name")).toString().toLocal8Bit();
    const QByteArray platform =
        memory.value(QStringLiteral("platform")).toString().toLocal8Bit();

    std::printf(
        "Representative startup: checkpoint=%s, elapsed=%.0f ms, platform=%s, "
        "workingSet=%.0f, peakWorkingSet=%.0f, private=%.0f, widgets=%d, "
        "pages=%d/%d, schedules=%d, renders=%.0f\n",
        name.constData(),
        checkpoint.value(QStringLiteral("elapsedMs")).toDouble(),
        platform.constData(),
        memory.value(QStringLiteral("workingSetBytes")).toDouble(),
        memory.value(QStringLiteral("peakWorkingSetBytes")).toDouble(),
        memory.value(QStringLiteral("privateUsageBytes")).toDouble(),
        metrics.value(QStringLiteral("widgetCount")).toInt(),
        metrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
        metrics.value(QStringLiteral("registeredPageCount")).toInt(),
        metrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
        metrics.value(QStringLiteral("scheduleRenderCount")).toDouble()
        );
}
}

class StartupPerformanceTests : public QObject
{
    Q_OBJECT

private slots:
    void representativeStartupFixtureIsCompleteAndDeterministic();
    void largeStartupFixtureIsCompleteAndDeterministic();
    void legacyStartupFixtureMigratesAndRemainsReadable();
    void rejectsCorruptWorkspaceFile();
    void rejectsLockedLegacyWorkspaceDuringMigration();
    void reportsStartupMetricsAndHonorsThresholds();
    void runsRepresentativeWorkspaceLifecycleWorkflow();
    void capturesLargeSubPrepBoundaryWhenConfigured();
    void capturesLargeClassesBoundaryWhenConfigured();
    void capturesLargeScheduleBoundaryWhenConfigured();
    void capturesLargeScheduleImportBoundaryWhenConfigured();
    void capturesLargeScheduleImportApplyBoundaryWhenConfigured();
    void capturesLargeCalendarImportBoundaryWhenConfigured();
    void capturesLargeClassTransferBoundaryWhenConfigured();
    void capturesLargeSpeakingEvaluationBoundaryWhenConfigured();
    void capturesLargeStaffDirectoryBoundaryWhenConfigured();
    void capturesLargeSubPrepOutputBoundaryWhenConfigured();
    void capturesLargeSubPrepVisualStatesWhenConfigured();
    void capturesLargeResourceTraceWhenConfigured();
    void capturesLargePdfViewerVisualStatesWhenConfigured();
    void capturesVisualLanguageAndThemeVariants();
    void capturesRepresentativeVisualVariants();
};

void StartupPerformanceTests
    ::representativeStartupFixtureIsCompleteAndDeterministic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString configuredFixturePath =
        qEnvironmentVariable(
            "CLASSMNGR_STARTUP_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    const QString fixturePath =
        configuredFixturePath.isEmpty()
            ? directory.filePath(QStringLiteral("representative-startup.tps"))
            : configuredFixturePath;
    if (!configuredFixturePath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(fixturePath).absolutePath()),
            qPrintable(
                QStringLiteral("Could not create fixture output directory for %1")
                    .arg(fixturePath)
                )
            );
    }
    QString fixtureError;
    QVERIFY2(
        createRepresentativeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );

    const QString connectionName =
        QStringLiteral("startup-representative-fixture-validation");
    QSqlDatabase database =
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
    database.setDatabaseName(fixturePath);
    QVERIFY2(
        database.open(),
        qPrintable(database.lastError().text())
        );

    QSqlQuery query(database);
    QVERIFY2(
        query.exec(QStringLiteral("PRAGMA integrity_check")),
        qPrintable(query.lastError().text())
        );
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("ok"));

    for (const FixtureTableExpectation& expected : {
             FixtureTableExpectation{"teachers", 4},
             FixtureTableExpectation{"classes", 8},
             FixtureTableExpectation{"class_times", 10},
             FixtureTableExpectation{"class_intensive_times", 4},
             FixtureTableExpectation{"intensive_slot_states", 3},
             FixtureTableExpectation{"roster_columns", 8},
             FixtureTableExpectation{"roster_data", 24},
             FixtureTableExpectation{"app_settings", 12}
         })
    {
        QVERIFY2(
            query.exec(
                QStringLiteral("SELECT COUNT(*) FROM %1")
                    .arg(QString::fromLatin1(expected.name))
                ),
            qPrintable(query.lastError().text())
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), expected.expectedRows);
    }

    QVERIFY(query.exec(QStringLiteral(R"(
        SELECT COUNT(DISTINCT teacher_id)
        FROM class_info
        WHERE teacher_id IS NOT NULL
    )")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 4);

    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(DISTINCT class_id) FROM roster_data"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);

    query.prepare(QStringLiteral(
        "SELECT value FROM app_settings WHERE key=?"
        ));
    query.addBindValue(QStringLiteral("schedule_display_mode"));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("regular"));

    query.prepare(QStringLiteral(
        "SELECT value FROM app_settings WHERE key=?"
        ));
    query.addBindValue(QStringLiteral("schedule_show_weekends"));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("true"));

    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void StartupPerformanceTests::largeStartupFixtureIsCompleteAndDeterministic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString configuredFixturePath =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_STARTUP_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    const QString fixturePath =
        configuredFixturePath.isEmpty()
            ? directory.filePath(QStringLiteral("large-startup.tps"))
            : configuredFixturePath;
    if (!configuredFixturePath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(fixturePath).absolutePath()),
            qPrintable(
                QStringLiteral("Could not create fixture output directory for %1")
                    .arg(fixturePath)
                )
            );
    }
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    const QString connectionName =
        QStringLiteral("startup-large-fixture-validation");
    QSqlDatabase database =
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
    database.setDatabaseName(fixturePath);
    QVERIFY2(
        database.open(),
        qPrintable(database.lastError().text())
        );

    QSqlQuery query(database);
    QVERIFY2(
        query.exec(QStringLiteral("PRAGMA integrity_check")),
        qPrintable(query.lastError().text())
        );
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("ok"));

    for (const FixtureTableExpectation& expected : {
             FixtureTableExpectation{"teachers", 24},
             FixtureTableExpectation{"classes", 96},
             FixtureTableExpectation{"class_times", 768},
             FixtureTableExpectation{"class_intensive_times", 24},
             FixtureTableExpectation{"intensive_slot_states", 5},
             FixtureTableExpectation{"roster_columns", 288},
             FixtureTableExpectation{"roster_data", 7200},
             FixtureTableExpectation{"speaking_evaluations", 20},
             FixtureTableExpectation{"speaking_eval_data", 600},
             FixtureTableExpectation{"campuses", 3},
             FixtureTableExpectation{"calendar_events", 180},
             FixtureTableExpectation{"app_settings", 12}
         })
    {
        QVERIFY2(
            query.exec(
                QStringLiteral("SELECT COUNT(*) FROM %1")
                    .arg(QString::fromLatin1(expected.name))
                ),
            qPrintable(query.lastError().text())
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), expected.expectedRows);
    }

    QVERIFY(query.exec(QStringLiteral(R"(
        SELECT COUNT(DISTINCT teacher_id)
        FROM class_info
        WHERE teacher_id IS NOT NULL
    )")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 24);

    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(DISTINCT class_id) FROM roster_data"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 96);

    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void StartupPerformanceTests::legacyStartupFixtureMigratesAndRemainsReadable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString configuredFixturePath =
        qEnvironmentVariable(
            "CLASSMNGR_LEGACY_STARTUP_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    const QString fixturePath =
        configuredFixturePath.isEmpty()
            ? directory.filePath(QStringLiteral("legacy-startup.db"))
            : configuredFixturePath;
    if (!configuredFixturePath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(fixturePath).absolutePath()),
            qPrintable(
                QStringLiteral("Could not create fixture output directory for %1")
                    .arg(fixturePath)
                )
            );
    }
    QString fixtureError;
    QVERIFY2(
        createLegacyStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );

    const QString connectionName =
        QStringLiteral("startup-legacy-fixture-validation");
    QSqlDatabase database =
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
    database.setDatabaseName(fixturePath);
    QVERIFY2(
        database.open(),
        qPrintable(database.lastError().text())
        );
    QVERIFY(DatabaseSchemaManager::ensureSchema(database));

    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
    QVERIFY(query.next());
    QCOMPARE(
        query.value(0).toInt(),
        DatabaseSchemaManager::LatestSchemaVersion
        );

    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);

    QVERIFY(query.exec(QStringLiteral("PRAGMA integrity_check")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("ok"));

    QVERIFY(query.exec(QStringLiteral(
        "SELECT name FROM classes WHERE id=1"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("Legacy E4"));

    QVERIFY(query.exec(QStringLiteral(
        "SELECT teacher_id IS NULL FROM class_info WHERE class_id=1"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);

    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(*) FROM class_times WHERE class_id=1"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);

    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

    QVERIFY(QFile::exists(
        fixturePath + QStringLiteral(".pre-schema-v4-backup")
        ));
}

void StartupPerformanceTests::rejectsCorruptWorkspaceFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("corrupt-startup.db"));
    const QByteArray corruptContents =
        QByteArrayLiteral("ClassMngr corrupt workspace fixture\n");
    QFile corruptFile(fixturePath);
    QVERIFY(corruptFile.open(QIODevice::WriteOnly));
    QCOMPARE(
        corruptFile.write(corruptContents),
        static_cast<qint64>(corruptContents.size())
        );
    corruptFile.close();

    const QString connectionName =
        QStringLiteral("startup-corrupt-fixture-validation");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
        database.setDatabaseName(fixturePath);

        if (database.open())
        {
            const Status status =
                DatabaseSchemaManager::ensureSchema(database);
            QVERIFY2(
                !status,
                "A corrupt workspace must be rejected by schema validation."
                );
            database.close();
        }

        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QFile unchangedFile(fixturePath);
    QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
    QCOMPARE(unchangedFile.readAll(), corruptContents);
}

void StartupPerformanceTests::rejectsLockedLegacyWorkspaceDuringMigration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("locked-startup.db"));
    QString fixtureError;
    QVERIFY2(
        createLegacyStartupSource(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );

    const QString lockConnectionName =
        QStringLiteral("startup-locked-fixture-lock");
    QSqlDatabase lockDatabase =
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            lockConnectionName
            );
    lockDatabase.setDatabaseName(fixturePath);
    QVERIFY2(
        lockDatabase.open(),
        qPrintable(lockDatabase.lastError().text())
        );

    QSqlQuery lockQuery(lockDatabase);
    QVERIFY2(
        lockQuery.exec(QStringLiteral("BEGIN EXCLUSIVE")),
        qPrintable(lockQuery.lastError().text())
        );

    const QString migrationConnectionName =
        QStringLiteral("startup-locked-fixture-migration");
    {
        QSqlDatabase migrationDatabase =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                migrationConnectionName
                );
        migrationDatabase.setDatabaseName(fixturePath);

        if (migrationDatabase.open())
        {
            const Status status =
                DatabaseSchemaManager::ensureSchema(migrationDatabase);
            QVERIFY2(
                !status,
                "A locked workspace must reject a write migration."
                );
            migrationDatabase.close();
        }

        migrationDatabase = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(migrationConnectionName);

    QVERIFY2(
        lockQuery.exec(QStringLiteral("ROLLBACK")),
        qPrintable(lockQuery.lastError().text())
        );
    lockDatabase.close();
    lockDatabase = QSqlDatabase();
    QSqlDatabase::removeDatabase(lockConnectionName);

    const QString recoveryConnectionName =
        QStringLiteral("startup-locked-fixture-recovery");
    {
        QSqlDatabase recoveryDatabase =
            QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                recoveryConnectionName
                );
        recoveryDatabase.setDatabaseName(fixturePath);
        QVERIFY2(
            recoveryDatabase.open(),
            qPrintable(recoveryDatabase.lastError().text())
            );
        QVERIFY(DatabaseSchemaManager::ensureSchema(recoveryDatabase));

        QSqlQuery versionQuery(recoveryDatabase);
        QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(versionQuery.next());
        QCOMPARE(
            versionQuery.value(0).toInt(),
            DatabaseSchemaManager::LatestSchemaVersion
            );

        recoveryDatabase.close();
        recoveryDatabase = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(recoveryConnectionName);
}

void StartupPerformanceTests::reportsStartupMetricsAndHonorsThresholds()
{
    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;

    QVERIFY(directory.isValid());

    const QString metricsPath =
        directory.filePath(
            QStringLiteral("startup-metrics.json")
            );

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();

    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );

    if (!environment.contains(QStringLiteral("QT_QPA_PLATFORM")))
    {
        environment.insert(
            QStringLiteral("QT_QPA_PLATFORM"),
            QStringLiteral("offscreen")
            );
    }

    process.setProcessEnvironment(environment);

    QElapsedTimer processTimer;

    processTimer.start();

    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-output"),
            metricsPath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );

    if (!process.waitForFinished(StartupTimeoutMs))
    {
        process.kill();
        process.waitForFinished();

        QFAIL(
            qPrintable(
                QStringLiteral("ClassMngr startup performance run timed out.\n%1")
                    .arg(processOutput(process))
                )
            );
    }

    const qint64 processStartToExitMs =
        processTimer.elapsed();

    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        process.exitCode() == 0,
        qPrintable(
            QStringLiteral("ClassMngr exited with code %1.\n%2")
                .arg(process.exitCode())
                .arg(processOutput(process))
            )
        );

    QFile metricsFile(metricsPath);

    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly),
        qPrintable(
            QStringLiteral("Unable to open startup metrics file: %1")
                .arg(metricsFile.errorString())
            )
        );

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            metricsFile.readAll(),
            &parseError
            );

    QVERIFY2(
        parseError.error == QJsonParseError::NoError,
        qPrintable(parseError.errorString())
        );
    QVERIFY(document.isObject());

    const QJsonObject metrics =
        document.object();

    QCOMPARE(
        metrics.value(QStringLiteral("format")).toString(),
        QStringLiteral("classmngr-startup-profile-v2")
        );
    const QJsonObject scenario =
        metrics.value(QStringLiteral("scenario")).toObject();
    QCOMPARE(
        scenario.value(QStringLiteral("name")).toString(),
        QStringLiteral("minimal-startup")
        );
    QCOMPARE(
        scenario.value(QStringLiteral("actions")).toArray().size(),
        4
        );
    QCOMPARE(
        scenario.value(QStringLiteral("settleMilliseconds")).toInt(),
        0
        );
    const QJsonArray checkpoints =
        metrics.value(QStringLiteral("checkpoints")).toArray();
    QVERIFY(checkpoints.size() >= 17);

    QSet<QString> checkpointNames;
    QJsonObject startupComplete;
    int startupCompleteCount = 0;
    for (const QJsonValue& value : checkpoints)
    {
        const QJsonObject checkpoint = value.toObject();
        const QString name = checkpoint.value(QStringLiteral("name")).toString();
        checkpointNames.insert(name);
        QVERIFY(checkpoint.value(QStringLiteral("elapsedMs")).toDouble(-1.0) >= 0.0);
        QVERIFY(checkpoint.value(QStringLiteral("memory")).toObject().contains(
            QStringLiteral("workingSetBytes")
            ));
        QVERIFY(checkpoint.value(QStringLiteral("metrics")).toObject().contains(
            QStringLiteral("widgetCount")
            ));

        if (name == QStringLiteral("startup-complete"))
        {
            startupComplete = checkpoint;
            ++startupCompleteCount;
        }
    }

    const QSet<QString> requiredCheckpoints{
        QStringLiteral("process-start"),
        QStringLiteral("qapplication-created"),
        QStringLiteral("preferences-resolved"),
        QStringLiteral("locale-applied"),
        QStringLiteral("font-applied"),
        QStringLiteral("theme-applied"),
        QStringLiteral("resource-system-initialized"),
        QStringLiteral("splash-shown"),
        QStringLiteral("services-created"),
        QStringLiteral("main-window-shell-created"),
        QStringLiteral("page-manager-initialized"),
        QStringLiteral("controllers-connected"),
        QStringLiteral("database-opened"),
        QStringLiteral("navigation-data-loaded"),
        QStringLiteral("startup-page-created"),
        QStringLiteral("startup-page-loaded"),
        QStringLiteral("window-shown"),
        QStringLiteral("startup-complete")
    };
    for (const QString& requiredCheckpoint : requiredCheckpoints)
    {
        QVERIFY(checkpointNames.contains(requiredCheckpoint));
    }
    QVERIFY(!startupComplete.isEmpty());
    QCOMPARE(startupCompleteCount, 1);

    const QJsonObject startupMetrics =
        startupComplete.value(QStringLiteral("metrics")).toObject();
    QVERIFY(startupMetrics.value(QStringLiteral("widgetCount")).toInt() > 0);
    QVERIFY(startupMetrics.value(QStringLiteral("registeredPageCount")).toInt() > 0);
    QVERIFY(startupMetrics.value(QStringLiteral("instantiatedPageCount")).toInt() > 0);
    QCOMPARE(
        startupMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
        1
        );
    QCOMPARE(
        startupMetrics.value(QStringLiteral("scheduleWidgetsCreated")).toDouble(),
        1.0
        );
    QCOMPARE(
        startupMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
        0.0
        );

    const QJsonArray events = metrics.value(QStringLiteral("events")).toArray();
    QSet<QString> eventNames;
    bool startupScheduleDiagnosticPassed = false;
    const double startupCompleteElapsedMilliseconds =
        startupComplete.value(QStringLiteral("elapsedMs")).toDouble();
    for (const QJsonValue& value : events)
    {
        const QJsonObject event = value.toObject();
        const QString eventName =
            event.value(QStringLiteral("name")).toString();
        eventNames.insert(eventName);

        QVERIFY(
            eventName != QStringLiteral("page-created")
            || event.value(QStringLiteral("elapsedMs")).toDouble()
                <= startupCompleteElapsedMilliseconds
            );

        if (
            eventName == QStringLiteral("schedule-widget-startup-diagnostic")
            && event.value(QStringLiteral("detail")).toString()
                == QStringLiteral("expected=1; live=1; created=1; passed=true")
            )
        {
            startupScheduleDiagnosticPassed = true;
        }
    }
    QVERIFY(eventNames.contains(QStringLiteral("page-created")));
    QVERIFY(eventNames.contains(QStringLiteral("schedule-widget-created")));
    QVERIFY(!eventNames.contains(QStringLiteral("schedule-render-start")));
    QVERIFY(!eventNames.contains(QStringLiteral("schedule-render-end")));
    QVERIFY(startupScheduleDiagnosticPassed);

    const QString representativeDatabasePath =
        directory.filePath(QStringLiteral("representative-startup.tps"));
    QString representativeFixtureError;
    QVERIFY2(
        createRepresentativeStartupFixture(
            representativeDatabasePath,
            &representativeFixtureError
            ),
        qPrintable(representativeFixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic representative startup settings."
        );

    const QString representativeMetricsPath =
        directory.filePath(
            QStringLiteral("representative-startup-metrics.json")
            );
    const QString representativeVisualDirectory =
        directory.filePath(
            QStringLiteral("representative-startup-visual")
            );
    QProcess representativeProcess;
    representativeProcess.setProcessEnvironment(environment);
    representativeProcess.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("5000"),
            QStringLiteral("--startup-performance-output"),
            representativeMetricsPath,
            QStringLiteral("--startup-visual-capture-output"),
            representativeVisualDirectory,
            representativeDatabasePath
        }
        );
    QVERIFY2(
        representativeProcess.waitForStarted(StartupTimeoutMs),
        qPrintable(representativeProcess.errorString())
        );
    QVERIFY2(
        representativeProcess.waitForFinished(StartupTimeoutMs),
        qPrintable(processOutput(representativeProcess))
        );
    QCOMPARE(representativeProcess.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        representativeProcess.exitCode() == 0,
        qPrintable(processOutput(representativeProcess))
        );

    QFile representativeMetricsFile(representativeMetricsPath);
    QVERIFY(representativeMetricsFile.open(QIODevice::ReadOnly));
    QJsonParseError representativeParseError;
    const QJsonDocument representativeDocument =
        QJsonDocument::fromJson(
            representativeMetricsFile.readAll(),
            &representativeParseError
            );
    QVERIFY2(
        representativeParseError.error == QJsonParseError::NoError,
        qPrintable(representativeParseError.errorString())
        );
    QVERIFY(representativeDocument.isObject());

    for (const QString& captureName : {
             QStringLiteral("startup-complete.png"),
             QStringLiteral("settled-final.png")
         })
    {
        const QString capturePath =
            QDir(representativeVisualDirectory).filePath(captureName);
        const QImage image(capturePath);
        QVERIFY2(
            !image.isNull(),
            qPrintable(
                QStringLiteral("Unable to read visual capture: %1")
                    .arg(capturePath)
                )
            );
        QVERIFY(image.width() > 0);
        QVERIFY(image.height() > 0);
    }

    const QJsonObject representativeReport = representativeDocument.object();
    QCOMPARE(
        representativeReport.value(QStringLiteral("format")).toString(),
        QStringLiteral("classmngr-startup-profile-v2")
        );
    const QJsonObject representativeScenario =
        representativeReport.value(QStringLiteral("scenario")).toObject();
    QCOMPARE(
        representativeScenario.value(QStringLiteral("name")).toString(),
        QStringLiteral("representative-startup")
        );
    QCOMPARE(
        representativeScenario.value(QStringLiteral("actions")).toArray().size(),
        4
        );
    QCOMPARE(
        representativeScenario.value(QStringLiteral("settleMilliseconds")).toInt(),
        5000
        );

    QHash<QString, QJsonObject> representativeCheckpoints;
    QHash<QString, int> representativeCheckpointCounts;
    QJsonObject representativeWindowShown;
    QJsonObject representativeStartupComplete;
    QJsonObject representativeSettledOneSecond;
    QJsonObject representativeSettledFiveSeconds;
    for (const QJsonValue& value : representativeReport
             .value(QStringLiteral("checkpoints"))
             .toArray())
    {
        const QJsonObject checkpoint = value.toObject();
        const QString name = checkpoint.value(QStringLiteral("name")).toString();
        representativeCheckpoints.insert(name, checkpoint);
        ++representativeCheckpointCounts[name];

        if (name == QStringLiteral("window-shown"))
        {
            representativeWindowShown = checkpoint;
        }
        else if (name == QStringLiteral("startup-complete"))
        {
            representativeStartupComplete = checkpoint;
        }
        else if (name == QStringLiteral("settled-1s"))
        {
            representativeSettledOneSecond = checkpoint;
        }
        else if (name == QStringLiteral("settled-5s"))
        {
            representativeSettledFiveSeconds = checkpoint;
        }
    }
    QVERIFY(!representativeWindowShown.isEmpty());
    QVERIFY(!representativeStartupComplete.isEmpty());
    QCOMPARE(
        representativeCheckpointCounts.value(QStringLiteral("window-shown")),
        1
        );
    QCOMPARE(
        representativeCheckpointCounts.value(QStringLiteral("startup-complete")),
        1
        );
    QCOMPARE(
        representativeCheckpointCounts.value(QStringLiteral("settled-5s")),
        1
        );
    QVERIFY(!representativeSettledOneSecond.isEmpty());
    QVERIFY(!representativeSettledFiveSeconds.isEmpty());

    const QJsonObject representativeStartupMetrics =
        representativeStartupComplete.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        representativeStartupMetrics
            .value(QStringLiteral("scheduleRenderCount"))
            .toDouble(),
        1.0
        );

    // These three checkpoints are the permanent startup regression surface:
    // first visible frame, completed startup, and five-second steady state.
    // Widget counts may drop when the splash is released, but hidden pages or
    // schedules must never appear in any of the three snapshots.
    for (const QString& checkpointName : {
             QStringLiteral("window-shown"),
             QStringLiteral("startup-complete"),
             QStringLiteral("settled-5s")
         })
    {
        const QJsonObject checkpoint =
            representativeCheckpoints.value(checkpointName);
        const QJsonObject checkpointMetrics =
            checkpoint.value(QStringLiteral("metrics")).toObject();
        const QJsonObject checkpointMemory =
            checkpoint.value(QStringLiteral("memory")).toObject();

        QVERIFY(
            checkpoint.value(QStringLiteral("elapsedMs")).toDouble(-1.0)
                >= 0.0
            );
        QVERIFY(checkpointMemory.value(QStringLiteral("available")).toBool());
        QVERIFY(!checkpointMemory.value(QStringLiteral("platform")).toString().isEmpty());
        QVERIFY(
            checkpointMemory.value(QStringLiteral("workingSetBytes")).toDouble()
                > 0.0
            );
        QVERIFY(
            checkpointMemory.value(QStringLiteral("privateUsageBytes")).toDouble()
                > 0.0
            );
        QVERIFY(
            checkpointMemory.value(QStringLiteral("peakWorkingSetBytes")).toDouble()
                >= checkpointMemory.value(QStringLiteral("workingSetBytes")).toDouble()
            );
        QVERIFY(checkpointMetrics.value(QStringLiteral("widgetCount")).toInt() > 0);
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
            1
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("registeredPageCount")).toInt(),
            11
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
            1
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("scheduleWidgetsCreated")).toDouble(),
            1.0
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
            1.0
            );

        printRepresentativeCheckpoint(checkpoint);
    }
    std::fflush(stdout);

    const QJsonObject representativePeakMemory =
        representativeReport.value(QStringLiteral("peakMemory")).toObject();
    QVERIFY(representativePeakMemory.value(QStringLiteral("available")).toBool());
    QCOMPARE(
        representativePeakMemory
            .value(QStringLiteral("checkpointSampleCount"))
            .toInt(),
        representativeReport.value(QStringLiteral("checkpoints")).toArray().size()
        );
    QVERIFY(
        representativePeakMemory.value(QStringLiteral("workingSetBytes")).toDouble()
            >= representativeSettledFiveSeconds
                .value(QStringLiteral("memory"))
                .toObject()
                .value(QStringLiteral("workingSetBytes"))
                .toDouble()
        );
    QVERIFY(
        representativePeakMemory
            .value(QStringLiteral("peakWorkingSetBytes"))
            .toDouble()
            >= representativePeakMemory
                .value(QStringLiteral("workingSetBytes"))
                .toDouble()
        );

    for (const QJsonObject& settledCheckpoint : {
            representativeSettledOneSecond,
            representativeSettledFiveSeconds
        })
    {
        const QJsonObject settledMetrics =
            settledCheckpoint.value(QStringLiteral("metrics")).toObject();
        QCOMPARE(
            settledMetrics.value(QStringLiteral("widgetCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("widgetCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("instantiatedPageCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("registeredPageCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("registeredPageCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleRenderCount")).toDouble()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("liveScheduleWidgetCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleWidgetsCreated")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleWidgetsCreated")).toDouble()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleTableItemsCreated")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleTableItemsCreated")).toDouble()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleCellWidgetsCreated")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleCellWidgetsCreated")).toDouble()
            );
    }

    QSet<QString> representativeEventNames;
    QSet<QString> representativePageIdentifiers;
    bool representativeScheduleDiagnosticPassed = false;
    const double representativeStartupCompleteElapsedMilliseconds =
        representativeStartupComplete.value(QStringLiteral("elapsedMs")).toDouble();
    for (const QJsonValue& value : representativeReport
             .value(QStringLiteral("events"))
             .toArray())
    {
        const QJsonObject event = value.toObject();
        const QString eventName =
            event.value(QStringLiteral("name")).toString();
        representativeEventNames.insert(eventName);
        QVERIFY(
            eventName != QStringLiteral("page-created")
            || event.value(QStringLiteral("elapsedMs")).toDouble()
                <= representativeStartupCompleteElapsedMilliseconds
            );

        QVERIFY(
            eventName != QStringLiteral("schedule-render-start")
            || event.value(QStringLiteral("elapsedMs")).toDouble()
                <= representativeStartupCompleteElapsedMilliseconds
            );

        if (eventName == QStringLiteral("page-created"))
        {
            representativePageIdentifiers.insert(
                event.value(QStringLiteral("detail")).toString()
                );
        }

        if (
            eventName == QStringLiteral("schedule-widget-startup-diagnostic")
            && event.value(QStringLiteral("detail")).toString()
                == QStringLiteral("expected=1; live=1; created=1; passed=true")
            )
        {
            representativeScheduleDiagnosticPassed = true;
        }
    }
    QCOMPARE(representativePageIdentifiers.size(), 1);
    QVERIFY(representativePageIdentifiers.contains(QStringLiteral("my-workspace")));
    QVERIFY(!representativePageIdentifiers.contains(QStringLiteral("sub-prep")));
    QVERIFY(!representativePageIdentifiers.contains(QStringLiteral("pdf-viewer")));
    QVERIFY(!representativePageIdentifiers.contains(QStringLiteral("campus-dashboard")));
    QVERIFY(
        representativeEventNames.contains(
            QStringLiteral("schedule-render-start")
            )
        );
    QVERIFY(
        representativeEventNames.contains(
            QStringLiteral("schedule-render-end")
            )
        );
    QVERIFY(representativeScheduleDiagnosticPassed);

    const double startupCompleteMs =
        startupComplete.value(QStringLiteral("elapsedMs")).toDouble(-1.0);
    const double progressUpdates =
        metrics.value(QStringLiteral("progressUpdates")).toDouble(-1.0);
    const double finalProgress =
        metrics.value(QStringLiteral("finalProgress")).toDouble(-1.0);

    QVERIFY(startupCompleteMs >= 0.0);
    QVERIFY(progressUpdates > 0.0);
    QCOMPARE(finalProgress, 100.0);
    QVERIFY(processStartToExitMs >= startupCompleteMs);

#if defined(Q_OS_WIN)
    const QJsonObject startupMemory =
        startupComplete.value(QStringLiteral("memory")).toObject();
    QVERIFY(startupMemory.value(QStringLiteral("workingSetBytes")).toDouble() > 0.0);
    QVERIFY(startupMemory.value(QStringLiteral("privateUsageBytes")).toDouble() > 0.0);
#endif

    std::printf(
        "Startup performance: startupComplete=%.0f ms, process=%lld ms, widgets=%d, pages=%d/%d, schedules=%d, renders=%.0f, progressUpdates=%.0f, finalProgress=%.0f\n",
        startupCompleteMs,
        static_cast<long long>(processStartToExitMs),
        startupMetrics.value(QStringLiteral("widgetCount")).toInt(),
        startupMetrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
        startupMetrics.value(QStringLiteral("registeredPageCount")).toInt(),
        startupMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
        startupMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
        progressUpdates,
        finalProgress
        );
    std::fflush(stdout);

    QString thresholdMessage;

    if (
        thresholdExceeded(
            "CLASSMNGR_STARTUP_WINDOW_MAX_MS",
            startupCompleteMs,
            &thresholdMessage
            )
        )
    {
        const QByteArray message =
            thresholdMessage.toLocal8Bit();

        std::printf(
            "Startup performance threshold failed: %s\n",
            message.constData()
            );
        std::fflush(stdout);

        QFAIL(message.constData());
    }

    if (
        thresholdExceeded(
            "CLASSMNGR_STARTUP_READY_MAX_MS",
            startupCompleteMs,
            &thresholdMessage
            )
        )
    {
        const QByteArray message =
            thresholdMessage.toLocal8Bit();

        std::printf(
            "Startup performance threshold failed: %s\n",
            message.constData()
            );
        std::fflush(stdout);

        QFAIL(message.constData());
    }

    if (
        thresholdExceeded(
            "CLASSMNGR_STARTUP_PROCESS_MAX_MS",
            static_cast<double>(processStartToExitMs),
            &thresholdMessage
            )
        )
    {
        const QByteArray message =
            thresholdMessage.toLocal8Bit();

        std::printf(
            "Startup performance threshold failed: %s\n",
            message.constData()
            );
        std::fflush(stdout);

        QFAIL(message.constData());
    }
}

void StartupPerformanceTests::runsRepresentativeWorkspaceLifecycleWorkflow()
{
    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("representative-workspace-workflow.tps"));
    QString fixtureError;
    QVERIFY2(
        createRepresentativeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
            "Unable to write deterministic representative-workspace settings."
        );

    const QString metricsPath =
        directory.filePath(
            QStringLiteral("representative-workspace-workflow.json")
            );
    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    const QString pdfCaptureDirectory =
        directory.filePath(QStringLiteral("pdf-captures"));
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR"),
        pdfCaptureDirectory
        );
    if (!environment.contains(QStringLiteral("QT_QPA_PLATFORM")))
    {
        environment.insert(
            QStringLiteral("QT_QPA_PLATFORM"),
            QStringLiteral("offscreen")
            );
    }
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    QVERIFY2(
        process.waitForFinished(StartupTimeoutMs),
        qPrintable(processOutput(process))
        );
    QVERIFY2(
        process.exitStatus() == QProcess::NormalExit,
        qPrintable(
            QStringLiteral(
                "Representative workspace lifecycle workflow terminated abnormally.\n%1"
                )
                .arg(processOutput(process))
            )
        );
    QVERIFY2(
        process.exitCode() == 0,
        qPrintable(
            QStringLiteral(
                "Representative workspace lifecycle workflow exited with code %1.\n%2"
                )
                .arg(process.exitCode())
                .arg(processOutput(process))
            )
        );

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError,
        qPrintable(parseError.errorString())
        );
    QVERIFY(document.isObject());

    const QJsonObject report = document.object();
    const QJsonObject workflow =
        report.value(QStringLiteral("workflow")).toObject();
    QVERIFY(workflow.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(
        workflow.value(QStringLiteral("stepDelayMilliseconds")).toInt(),
        150
        );
    QCOMPARE(
        workflow.value(QStringLiteral("pages")).toArray().size(),
        12
        );

    const QList<QString> expectedPageSequence{
        QStringLiteral("my-workspace"),
        QStringLiteral("schedule"),
        QStringLiteral("classes"),
        QStringLiteral("testing-classes"),
        QStringLiteral("teacher-info"),
        QStringLiteral("native-english-teachers"),
        QStringLiteral("gs-team"),
        QStringLiteral("campus-dashboard"),
        QStringLiteral("sub-prep"),
        QStringLiteral("my-classes"),
        QStringLiteral("pdf-viewer"),
        QStringLiteral("my-workspace")
    };

    QList<QString> readyPageSequence;
    QList<QString> leftPageSequence;
    QHash<QString, QJsonObject> checkpoints;
    QSet<QString> checkpointNames;
    for (const QJsonValue& value : report
             .value(QStringLiteral("checkpoints"))
             .toArray())
    {
        const QJsonObject checkpoint = value.toObject();
        const QString name = checkpoint.value(QStringLiteral("name"))
            .toString();
        checkpointNames.insert(name);
        if (name == QStringLiteral("workflow-page-ready"))
        {
            readyPageSequence.append(
                checkpoint.value(QStringLiteral("detail")).toString()
                );
        }
        if (name == QStringLiteral("workflow-page-left"))
        {
            leftPageSequence.append(
                checkpoint.value(QStringLiteral("detail")).toString()
                );
        }
        if (
            name == QStringLiteral("workflow-complete")
            || name == QStringLiteral("startup-complete")
            || name == QStringLiteral("settled-1s")
            || name.startsWith(QStringLiteral("pdf-"))
            )
        {
            checkpoints.insert(name, checkpoint);
        }
    }

    QCOMPARE(readyPageSequence, expectedPageSequence);
    QCOMPARE(
        leftPageSequence,
        expectedPageSequence.mid(0, expectedPageSequence.size() - 1)
        );
    for (const QString& name : {
             QStringLiteral("workflow-child-ready"),
             QStringLiteral("workflow-child-released"),
             QStringLiteral("workflow-complete"),
             QStringLiteral("settled-1s"),
             QStringLiteral("pdf-catalog-ready"),
             QStringLiteral("pdf-workflow-start"),
             QStringLiteral("pdf-open-start"),
             QStringLiteral("pdf-opened"),
             QStringLiteral("pdf-rendered"),
             QStringLiteral("pdf-released"),
             QStringLiteral("pdf-closed"),
             QStringLiteral("pdf-error"),
             QStringLiteral("pdf-reopen-start"),
             QStringLiteral("pdf-reopened"),
             QStringLiteral("pdf-reopened-rendered"),
             QStringLiteral("pdf-released-after-reopen"),
             QStringLiteral("pdf-workflow-complete")
         })
    {
        QVERIFY2(
            checkpointNames.contains(name),
            qPrintable(
                QStringLiteral("Missing lifecycle checkpoint: %1").arg(name)
                )
            );
    }
    QVERIFY(!checkpointNames.contains(QStringLiteral("workflow-page-failed")));
    QVERIFY(!checkpointNames.contains(QStringLiteral("workflow-child-failed")));

    const QJsonObject startupMetrics =
        checkpoints.value(QStringLiteral("startup-complete"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject workflowMetrics =
        checkpoints.value(QStringLiteral("workflow-complete"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject settledMetrics =
        checkpoints.value(QStringLiteral("settled-1s"))
            .value(QStringLiteral("metrics"))
            .toObject();

    QCOMPARE(startupMetrics.value(QStringLiteral("instantiatedPageCount"))
                 .toInt(), 1);
    QCOMPARE(
        startupMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        0
        );
    QCOMPARE(
        startupMetrics.value(QStringLiteral("pdfDocumentsLoaded")).toDouble(),
        0.0
        );
    QCOMPARE(
        startupMetrics.value(QStringLiteral("pdfDocumentsReleased")).toDouble(),
        0.0
        );
    QCOMPARE(
        startupMetrics.value(QStringLiteral("pdfRenderCount")).toDouble(),
        0.0
        );
    QCOMPARE(workflowMetrics.value(QStringLiteral("instantiatedPageCount"))
                 .toInt(), 11);
    QCOMPARE(workflowMetrics.value(QStringLiteral("registeredPageCount"))
                 .toInt(), 11);
    QVERIFY(
        workflowMetrics.value(QStringLiteral("widgetCount")).toInt()
            > startupMetrics.value(QStringLiteral("widgetCount")).toInt()
        );
    QVERIFY(
        workflowMetrics.value(QStringLiteral("scheduleWidgetsCreated"))
            .toDouble()
            >= 3.0
        );
    QCOMPARE(
        workflowMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        0
        );
    QCOMPARE(
        workflowMetrics.value(QStringLiteral("pdfDocumentsLoaded")).toDouble(),
        2.0
        );
    QCOMPARE(
        workflowMetrics.value(QStringLiteral("pdfDocumentsReleased")).toDouble(),
        2.0
        );
    QCOMPARE(
        workflowMetrics.value(QStringLiteral("pdfRenderCount")).toDouble(),
        2.0
        );
    QCOMPARE(
        settledMetrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
        workflowMetrics.value(QStringLiteral("instantiatedPageCount")).toInt()
        );
    QCOMPARE(
        settledMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
        workflowMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt()
        );
    QCOMPARE(
        settledMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        0
        );
    QCOMPARE(
        settledMetrics.value(QStringLiteral("pdfDocumentsLoaded")).toDouble(),
        2.0
        );
    QCOMPARE(
        settledMetrics.value(QStringLiteral("pdfDocumentsReleased")).toDouble(),
        2.0
        );
    QCOMPARE(
        settledMetrics.value(QStringLiteral("pdfRenderCount")).toDouble(),
        2.0
        );

    const QJsonObject pdfOpenedMetrics =
        checkpoints.value(QStringLiteral("pdf-opened"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject pdfReleasedMetrics =
        checkpoints.value(QStringLiteral("pdf-released"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject pdfReopenedMetrics =
        checkpoints.value(QStringLiteral("pdf-reopened"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject pdfReleasedAfterReopenMetrics =
        checkpoints.value(QStringLiteral("pdf-released-after-reopen"))
            .value(QStringLiteral("metrics"))
            .toObject();

    QCOMPARE(
        pdfOpenedMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        1
        );
    QCOMPARE(
        pdfOpenedMetrics.value(QStringLiteral("pdfDocumentsLoaded")).toDouble(),
        1.0
        );
    QCOMPARE(
        pdfOpenedMetrics.value(QStringLiteral("pdfDocumentsReleased")).toDouble(),
        0.0
        );
    QCOMPARE(
        pdfOpenedMetrics.value(QStringLiteral("pdfRenderCount")).toDouble(),
        1.0
        );
    QCOMPARE(
        pdfReleasedMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        0
        );
    QCOMPARE(
        pdfReleasedMetrics.value(QStringLiteral("pdfDocumentsReleased")).toDouble(),
        1.0
        );
    QCOMPARE(
        pdfReopenedMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        1
        );
    QCOMPARE(
        pdfReopenedMetrics.value(QStringLiteral("pdfDocumentsLoaded")).toDouble(),
        2.0
        );
    QCOMPARE(
        pdfReopenedMetrics.value(QStringLiteral("pdfDocumentsReleased")).toDouble(),
        1.0
        );
    QCOMPARE(
        pdfReopenedMetrics.value(QStringLiteral("pdfRenderCount")).toDouble(),
        2.0
        );
    QCOMPARE(
        pdfReleasedAfterReopenMetrics.value(QStringLiteral("livePdfDocumentCount"))
            .toInt(),
        0
        );
    QCOMPARE(
        pdfReleasedAfterReopenMetrics.value(QStringLiteral("pdfDocumentsReleased"))
            .toDouble(),
        2.0
        );

    QSet<QString> enteredPages;
    QSet<QString> leftPages;
    int pdfDocumentsLoaded = 0;
    int pdfDocumentsReleased = 0;
    int pdfDocumentsRendered = 0;
    for (const QJsonValue& value : report
             .value(QStringLiteral("events"))
             .toArray())
    {
        const QJsonObject event = value.toObject();
        const QString eventName =
            event.value(QStringLiteral("name")).toString();
        const QString pageIdentifier =
            event.value(QStringLiteral("detail")).toString();
        if (eventName == QStringLiteral("page-enter"))
        {
            enteredPages.insert(pageIdentifier);
        }
        else if (eventName == QStringLiteral("page-leave"))
        {
            leftPages.insert(pageIdentifier);
        }
        else if (eventName == QStringLiteral("pdf-document-loaded"))
        {
            ++pdfDocumentsLoaded;
            QVERIFY(
                event.value(QStringLiteral("detail")).toString()
                    .startsWith(QStringLiteral("DYB Lesson Planning Guide.pdf; pages="))
                );
        }
        else if (eventName == QStringLiteral("pdf-document-released"))
        {
            ++pdfDocumentsReleased;
            QCOMPARE(
                event.value(QStringLiteral("detail")).toString(),
                QStringLiteral("DYB Lesson Planning Guide.pdf")
                );
        }
        else if (eventName == QStringLiteral("pdf-document-rendered"))
        {
            ++pdfDocumentsRendered;
            QVERIFY(
                event.value(QStringLiteral("detail")).toString()
                    .startsWith(QStringLiteral("DYB Lesson Planning Guide.pdf; size="))
                );
        }
    }
    for (const QString& pageIdentifier : expectedPageSequence)
    {
        QVERIFY(enteredPages.contains(pageIdentifier));
    }
    for (const QString& pageIdentifier : expectedPageSequence.mid(
             0,
             expectedPageSequence.size() - 1
             ))
    {
        QVERIFY(leftPages.contains(pageIdentifier));
    }
    QCOMPARE(pdfDocumentsLoaded, 2);
    QCOMPARE(pdfDocumentsReleased, 2);
    QCOMPARE(pdfDocumentsRendered, 2);
    for (const QString& fileName : {
             QStringLiteral("pdf-catalog.png"),
             QStringLiteral("pdf-opened.png"),
             QStringLiteral("pdf-closed.png"),
             QStringLiteral("pdf-error.png"),
             QStringLiteral("pdf-reopened.png")
         })
    {
        const QImage image(
            QDir(pdfCaptureDirectory).filePath(fileName)
            );
        QVERIFY2(
            !image.isNull() && image.width() > 0 && image.height() > 0,
            qPrintable(
                QStringLiteral("Missing PDF lifecycle capture: %1")
                    .arg(fileName)
                )
            );
    }

    const double workflowElapsed =
        checkpoints.value(QStringLiteral("workflow-complete"))
            .value(QStringLiteral("elapsedMs"))
            .toDouble();
    const double settledElapsed =
        checkpoints.value(QStringLiteral("settled-1s"))
            .value(QStringLiteral("elapsedMs"))
            .toDouble();
    QVERIFY(settledElapsed > workflowElapsed);
}

void StartupPerformanceTests
    ::capturesLargeSubPrepBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-sub-prep-workflow.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral("Unable to create large Sub Prep reference root: %1")
                .arg(outputRoot)
            )
        );
    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-sub-prep-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));

    if (QFileInfo::exists(metricsPath))
    {
        QVERIFY(QFile::remove(metricsPath));
    }
    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-sub-prep-lifecycle"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );

    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }

    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start sub-prep")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Sub Prep transition.\n"
                "stdout/stderr were retained under %1."
                )
                .arg(outputRoot)
        )
        );
    for (const QString& expectedTrace : {
             QStringLiteral("sub-prep-lifecycle-start"),
             QStringLiteral("sub-prep-refresh-1-start"),
             QStringLiteral("sub-prep-refresh-1-complete"),
             QStringLiteral("sub-prep-left-1"),
             QStringLiteral("sub-prep-reentry-1"),
             QStringLiteral("sub-prep-refresh-2-start"),
             QStringLiteral("sub-prep-refresh-2-complete"),
             QStringLiteral("sub-prep-left-2"),
             QStringLiteral("sub-prep-reentry-2"),
             QStringLiteral("sub-prep-lifecycle-complete")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Sub Prep lifecycle did not record '%1'.\n"
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QString lastTraceLine;
    QString lastSubPrepLifecycleEvent;
    for (const QString& line : traceLines)
    {
        lastTraceLine = line;
        if (line.startsWith(
                QStringLiteral("sub-prep-class-information ")
                ))
        {
            lastSubPrepLifecycleEvent = line;
        }
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("fixture"), QStringLiteral("large_startup.sql"));
    manifest.insert(
        QStringLiteral("fixtureScale"),
        QStringLiteral("large_startup_sub_prep_lifecycle")
        );
    manifest.insert(
        QStringLiteral("scenario"),
        QStringLiteral(
            "Sub Prep entry, selection, refresh, leave, repeated re-entry"
            )
        );
    manifest.insert(QStringLiteral("teacherCount"), 24);
    manifest.insert(QStringLiteral("classCount"), 96);
    manifest.insert(QStringLiteral("rosterCellCount"), 7200);
    manifest.insert(QStringLiteral("processFinished"), finished);
    manifest.insert(
        QStringLiteral("exitStatus"),
        process.exitStatus() == QProcess::NormalExit
            ? QStringLiteral("normal")
            : QStringLiteral("crash")
        );
    manifest.insert(QStringLiteral("exitCode"), process.exitCode());
    manifest.insert(QStringLiteral("timedOut"), !finished);
    manifest.insert(QStringLiteral("traceLineCount"), traceLines.size());
    manifest.insert(QStringLiteral("lastTraceLine"), lastTraceLine);
    manifest.insert(
        QStringLiteral("lastSubPrepLifecycleEvent"),
        lastSubPrepLifecycleEvent
        );
    manifest.insert(
        QStringLiteral("tracePath"),
        QStringLiteral("workflow-trace.txt")
        );
    manifest.insert(
        QStringLiteral("metricsPath"),
        QStringLiteral("large-sub-prep-workflow.json")
        );
    manifest.insert(
        QStringLiteral("stdoutPath"),
        QStringLiteral("process-stdout.txt")
        );
    manifest.insert(
        QStringLiteral("stderrPath"),
        QStringLiteral("process-stderr.txt")
        );

    QJsonObject lastCheckpoint;
    bool workflowCompleted = false;
    bool lifecycleCompleted = false;
    QString lastLifecycleEventFromMetrics;
    QJsonArray lifecycleCheckpoints;
    int subPrepRosterQueryCount = 0;
    int subPrepRosterResultRowCount = 0;
    int subPrepRosterCellCount = 0;
    int subPrepReturnedStudentCount = 0;
    const auto detailValue =
        [](const QString& detail, const QString& key)
        {
            for (const QString& field :
                 detail.split(QStringLiteral("; "), Qt::SkipEmptyParts))
            {
                const QString prefix = key + QChar('=');
                if (!field.startsWith(prefix))
                {
                    continue;
                }

                bool converted = false;
                const int value =
                    field.mid(prefix.size()).toInt(&converted);
                return converted ? value : 0;
            }
            return 0;
        };
    QFile metricsFile(metricsPath);
    if (metricsFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QJsonParseError parseError;
        const QJsonDocument metricsDocument =
            QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject())
        {
            const QJsonObject metricsReport = metricsDocument.object();
            manifest.insert(
                QStringLiteral("peakMemory"),
                metricsReport.value(QStringLiteral("peakMemory"))
                );
            const QJsonArray checkpoints =
                metricsReport.value(QStringLiteral("checkpoints"))
                    .toArray();
            if (!checkpoints.isEmpty())
            {
                lastCheckpoint = checkpoints.last().toObject();
                manifest.insert(
                    QStringLiteral("lastCheckpointName"),
                    lastCheckpoint.value(QStringLiteral("name"))
                    );
                manifest.insert(
                    QStringLiteral("lastCheckpointMetrics"),
                    lastCheckpoint.value(QStringLiteral("metrics"))
                    );
                manifest.insert(
                    QStringLiteral("lastCheckpointMemory"),
                    lastCheckpoint.value(QStringLiteral("memory"))
                    );
            }

            for (const QJsonValue& value : metricsReport
                     .value(QStringLiteral("events"))
                     .toArray())
            {
                const QJsonObject event = value.toObject();
                if (
                    event.value(QStringLiteral("name")).toString()
                        == QStringLiteral("sub-prep-roster-query")
                    )
                {
                    const QString detail =
                        event.value(QStringLiteral("detail")).toString();
                    ++subPrepRosterQueryCount;
                    subPrepRosterResultRowCount +=
                        detailValue(detail, QStringLiteral("rows"));
                    subPrepRosterCellCount +=
                        detailValue(detail, QStringLiteral("cells"));
                    subPrepReturnedStudentCount +=
                        detailValue(
                            detail,
                            QStringLiteral("returnedStudents")
                            );
                }
                if (
                    event.value(QStringLiteral("name")).toString()
                        == QStringLiteral("sub-prep-class-information")
                    )
                {
                    lastLifecycleEventFromMetrics =
                        event.value(QStringLiteral("detail")).toString();
                }
            }

            for (const QJsonValue& value : checkpoints)
            {
                const QJsonObject checkpoint = value.toObject();
                const QString checkpointName =
                    checkpoint.value(QStringLiteral("name")).toString();
                if (checkpointName.startsWith(QStringLiteral("sub-prep-")))
                {
                    lifecycleCheckpoints.append(checkpoint);
                }
                if (checkpointName == QStringLiteral("workflow-complete"))
                {
                    manifest.insert(
                        QStringLiteral("workflowCompleteElapsedMs"),
                        checkpoint.value(QStringLiteral("elapsedMs"))
                        );
                }
                else if (checkpointName == QStringLiteral("settled-1s"))
                {
                    manifest.insert(
                        QStringLiteral("settledOneSecondElapsedMs"),
                        checkpoint.value(QStringLiteral("elapsedMs"))
                        );
                }
                if (
                    checkpointName
                        == QStringLiteral("workflow-complete")
                    )
                {
                    workflowCompleted = true;
                }
                if (
                    checkpointName
                        == QStringLiteral("sub-prep-lifecycle-complete")
                    && checkpoint.value(QStringLiteral("detail"))
                           .toString()
                           .contains(QStringLiteral("passed=true"))
                    )
                {
                    lifecycleCompleted = true;
                }
            }
        }
    }

    manifest.insert(
        QStringLiteral("metricsAvailable"),
        !lastCheckpoint.isEmpty()
        );
    manifest.insert(QStringLiteral("workflowCompleted"), workflowCompleted);
    manifest.insert(QStringLiteral("lifecycleCompleted"), lifecycleCompleted);
    manifest.insert(
        QStringLiteral("subPrepRosterQueryCount"),
        subPrepRosterQueryCount
        );
    manifest.insert(
        QStringLiteral("subPrepRosterResultRowCount"),
        subPrepRosterResultRowCount
        );
    manifest.insert(
        QStringLiteral("subPrepRosterCellCount"),
        subPrepRosterCellCount
        );
    manifest.insert(
        QStringLiteral("subPrepReturnedStudentCount"),
        subPrepReturnedStudentCount
        );
    manifest.insert(
        QStringLiteral("subPrepLifecycleCheckpoints"),
        lifecycleCheckpoints
        );
    manifest.insert(
        QStringLiteral("lastSubPrepLifecycleEventFromMetrics"),
        lastLifecycleEventFromMetrics
        );
    manifest.insert(
        QStringLiteral("routeOutcome"),
        workflowCompleted
            ? QStringLiteral("completed")
            : !finished
                ? QStringLiteral("timed-out")
                : process.exitStatus() != QProcess::NormalExit
                    ? QStringLiteral("abnormal-exit")
                    : QStringLiteral("workflow-incomplete")
        );

    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
}

void StartupPerformanceTests::capturesLargeClassesBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_CLASSES_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_CLASSES_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-classes-workflow.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral("Unable to create large Classes reference root: %1")
                .arg(outputRoot)
            )
        );

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-classes-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));

    if (QFileInfo::exists(metricsPath))
    {
        QVERIFY(QFile::remove(metricsPath));
    }
    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-classes-lifecycle"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );

    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }

    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start classes")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Classes transition.\n"
                "stdout/stderr were retained under %1."
                )
                .arg(outputRoot)
            )
        );
    for (const QString& expectedTrace : {
             QStringLiteral("classes-lifecycle-start"),
             QStringLiteral("classes-refresh-1-start"),
             QStringLiteral("classes-refresh-1-complete"),
             QStringLiteral("classes-left-1"),
             QStringLiteral("classes-reentry-1"),
             QStringLiteral("classes-refresh-2-start"),
             QStringLiteral("classes-refresh-2-complete"),
             QStringLiteral("classes-left-2"),
             QStringLiteral("classes-reentry-2"),
             QStringLiteral("classes-lifecycle-complete")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Classes lifecycle did not record '%1'.\n"
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("fixture"), QStringLiteral("large_startup.sql"));
    manifest.insert(
        QStringLiteral("fixtureScale"),
        QStringLiteral("large_startup_classes_lifecycle")
        );
    manifest.insert(
        QStringLiteral("scenario"),
        QStringLiteral(
            "Classes entry, selection, refresh, leave, repeated re-entry"
            )
        );
    manifest.insert(QStringLiteral("teacherCount"), 24);
    manifest.insert(QStringLiteral("classCount"), 96);
    manifest.insert(QStringLiteral("rosterCellCount"), 7200);
    manifest.insert(QStringLiteral("processFinished"), finished);
    manifest.insert(
        QStringLiteral("exitStatus"),
        process.exitStatus() == QProcess::NormalExit
            ? QStringLiteral("normal")
            : QStringLiteral("crash")
        );
    manifest.insert(QStringLiteral("exitCode"), process.exitCode());
    manifest.insert(QStringLiteral("timedOut"), !finished);
    manifest.insert(QStringLiteral("traceLineCount"), traceLines.size());
    manifest.insert(
        QStringLiteral("tracePath"),
        QStringLiteral("workflow-trace.txt")
        );
    manifest.insert(
        QStringLiteral("metricsPath"),
        QStringLiteral("large-classes-workflow.json")
        );
    manifest.insert(
        QStringLiteral("stdoutPath"),
        QStringLiteral("process-stdout.txt")
        );
    manifest.insert(
        QStringLiteral("stderrPath"),
        QStringLiteral("process-stderr.txt")
        );

    QJsonObject lastCheckpoint;
    QJsonObject lastClassesLifecycleCheckpoint;
    bool workflowCompleted = false;
    bool lifecycleCompleted = false;
    QJsonArray lifecycleCheckpoints;
    QFile metricsFile(metricsPath);
    if (metricsFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QJsonParseError parseError;
        const QJsonDocument metricsDocument =
            QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject())
        {
            const QJsonObject metricsReport = metricsDocument.object();
            manifest.insert(
                QStringLiteral("peakMemory"),
                metricsReport.value(QStringLiteral("peakMemory"))
                );
            const QJsonArray checkpoints =
                metricsReport.value(QStringLiteral("checkpoints"))
                    .toArray();
            if (!checkpoints.isEmpty())
            {
                lastCheckpoint = checkpoints.last().toObject();
                manifest.insert(
                    QStringLiteral("lastCheckpointName"),
                    lastCheckpoint.value(QStringLiteral("name"))
                    );
                manifest.insert(
                    QStringLiteral("lastCheckpointMetrics"),
                    lastCheckpoint.value(QStringLiteral("metrics"))
                    );
                manifest.insert(
                    QStringLiteral("lastCheckpointMemory"),
                    lastCheckpoint.value(QStringLiteral("memory"))
                    );
            }

            for (const QJsonValue& value : checkpoints)
            {
                const QJsonObject checkpoint = value.toObject();
                const QString checkpointName =
                    checkpoint.value(QStringLiteral("name")).toString();
                if (checkpointName.startsWith(QStringLiteral("classes-")))
                {
                    lifecycleCheckpoints.append(checkpoint);
                    lastClassesLifecycleCheckpoint = checkpoint;
                }
                if (checkpointName == QStringLiteral("workflow-complete"))
                {
                    manifest.insert(
                        QStringLiteral("workflowCompleteElapsedMs"),
                        checkpoint.value(QStringLiteral("elapsedMs"))
                        );
                    workflowCompleted = true;
                }
                if (
                    checkpointName
                        == QStringLiteral("classes-lifecycle-complete")
                    && checkpoint.value(QStringLiteral("detail"))
                           .toString()
                           .contains(QStringLiteral("passed=true"))
                    )
                {
                    lifecycleCompleted = true;
                }
                if (checkpointName == QStringLiteral("settled-1s"))
                {
                    manifest.insert(
                        QStringLiteral("settledOneSecondElapsedMs"),
                        checkpoint.value(QStringLiteral("elapsedMs"))
                        );
                }
            }
        }
    }

    manifest.insert(
        QStringLiteral("metricsAvailable"),
        !lastCheckpoint.isEmpty()
        );
    manifest.insert(QStringLiteral("workflowCompleted"), workflowCompleted);
    manifest.insert(QStringLiteral("lifecycleCompleted"), lifecycleCompleted);
    manifest.insert(
        QStringLiteral("classesLifecycleCheckpoints"),
        lifecycleCheckpoints
        );
    if (!lastClassesLifecycleCheckpoint.isEmpty())
    {
        manifest.insert(
            QStringLiteral("classesLifecycleMetrics"),
            lastClassesLifecycleCheckpoint.value(QStringLiteral("metrics"))
            );
        manifest.insert(
            QStringLiteral("classesLifecycleMemory"),
            lastClassesLifecycleCheckpoint.value(QStringLiteral("memory"))
            );
    }
    manifest.insert(
        QStringLiteral("routeOutcome"),
        workflowCompleted
            ? QStringLiteral("completed")
            : !finished
                ? QStringLiteral("timed-out")
                : process.exitStatus() != QProcess::NormalExit
                    ? QStringLiteral("abnormal-exit")
                    : QStringLiteral("workflow-incomplete")
        );

    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );

    QVERIFY2(
        lifecycleCompleted,
        qPrintable(
            QStringLiteral(
                "The Classes lifecycle did not complete.\n"
                "stdout/stderr were retained under %1."
                )
                .arg(outputRoot)
            )
        );
    QVERIFY2(
        workflowCompleted,
        qPrintable(
            QStringLiteral(
                "The full heavy workflow did not complete.\n"
                "stdout/stderr were retained under %1."
                )
                .arg(outputRoot)
            )
        );
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QJsonObject classesMetrics =
        lastClassesLifecycleCheckpoint.value(QStringLiteral("metrics"))
            .toObject();
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesSourceClassCount")).toInt(),
        96
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesVisibleClassCount")).toInt(),
        96
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesNavigationGradeGroupCount"))
            .toInt(),
        4
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesNavigationClassTabCount"))
            .toInt(),
        192
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesClassQueryCount")).toInt(),
        3
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesClassResultRowCount"))
            .toInt(),
        288
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesClassInfoQueryCount"))
            .toInt(),
        288
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesClassInfoResultRowCount"))
            .toInt(),
        288
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesClassInfoScheduleRowCount"))
            .toInt(),
        2376
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesTeacherQueryCount")).toInt(),
        288
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesTeacherResultRowCount"))
            .toInt(),
        288
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesVisibleSectionCount")).toInt(),
        6
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesInstantiatedEditorCount"))
            .toInt(),
        1
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesLoadedEditorClassCount"))
            .toInt(),
        1
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesRebuildCount")).toInt(),
        3
        );
    QCOMPARE(
        classesMetrics.value(QStringLiteral("classesSelectedClassId")).toInt(),
        1
        );
}

void StartupPerformanceTests::capturesLargeScheduleBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_SCHEDULE_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_SCHEDULE_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-schedule-workflow.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral("Unable to create large Schedule reference root: %1")
                .arg(outputRoot)
            )
        );

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-schedule-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));

    if (QFileInfo::exists(metricsPath))
    {
        QVERIFY(QFile::remove(metricsPath));
    }
    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-schedule-lifecycle"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );

    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }

    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start schedule")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Schedule transition.\n"
                "stdout/stderr were retained under %1."
                )
                .arg(outputRoot)
            )
        );
    for (const QString& expectedTrace : {
             QStringLiteral("schedule-lifecycle-start"),
             QStringLiteral("schedule-refresh-1-start"),
             QStringLiteral("schedule-refresh-1-complete"),
             QStringLiteral("schedule-left-1"),
             QStringLiteral("schedule-reentry-1"),
             QStringLiteral("schedule-refresh-2-start"),
             QStringLiteral("schedule-refresh-2-complete"),
             QStringLiteral("schedule-left-2"),
             QStringLiteral("schedule-reentry-2"),
             QStringLiteral("schedule-lifecycle-complete")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Schedule lifecycle did not record '%1'.\n"
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("fixture"), QStringLiteral("large_startup.sql"));
    manifest.insert(
        QStringLiteral("fixtureScale"),
        QStringLiteral("large_startup_schedule_lifecycle")
        );
    manifest.insert(
        QStringLiteral("scenario"),
        QStringLiteral(
            "Schedule entry, refresh, leave, repeated re-entry"
            )
        );
    manifest.insert(QStringLiteral("teacherCount"), 24);
    manifest.insert(QStringLiteral("classCount"), 96);
    manifest.insert(QStringLiteral("scheduleFixtureSlotCount"), 8);
    manifest.insert(QStringLiteral("scheduleFixtureWeekdayCellCount"), 40);
    manifest.insert(QStringLiteral("processFinished"), finished);
    manifest.insert(
        QStringLiteral("exitStatus"),
        process.exitStatus() == QProcess::NormalExit
            ? QStringLiteral("normal")
            : QStringLiteral("crash")
        );
    manifest.insert(QStringLiteral("exitCode"), process.exitCode());
    manifest.insert(QStringLiteral("timedOut"), !finished);
    manifest.insert(QStringLiteral("traceLineCount"), traceLines.size());
    manifest.insert(
        QStringLiteral("tracePath"),
        QStringLiteral("workflow-trace.txt")
        );
    manifest.insert(
        QStringLiteral("metricsPath"),
        QStringLiteral("large-schedule-workflow.json")
        );
    manifest.insert(
        QStringLiteral("stdoutPath"),
        QStringLiteral("process-stdout.txt")
        );
    manifest.insert(
        QStringLiteral("stderrPath"),
        QStringLiteral("process-stderr.txt")
        );

    QJsonObject lastCheckpoint;
    QJsonObject lastScheduleLifecycleCheckpoint;
    bool workflowCompleted = false;
    bool lifecycleCompleted = false;
    QJsonArray lifecycleCheckpoints;
    QFile metricsFile(metricsPath);
    if (metricsFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QJsonParseError parseError;
        const QJsonDocument metricsDocument =
            QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject())
        {
            const QJsonObject metricsReport = metricsDocument.object();
            manifest.insert(
                QStringLiteral("peakMemory"),
                metricsReport.value(QStringLiteral("peakMemory"))
                );
            const QJsonArray checkpoints =
                metricsReport.value(QStringLiteral("checkpoints"))
                    .toArray();
            if (!checkpoints.isEmpty())
            {
                lastCheckpoint = checkpoints.last().toObject();
                manifest.insert(
                    QStringLiteral("lastCheckpointName"),
                    lastCheckpoint.value(QStringLiteral("name"))
                    );
                manifest.insert(
                    QStringLiteral("lastCheckpointMetrics"),
                    lastCheckpoint.value(QStringLiteral("metrics"))
                    );
                manifest.insert(
                    QStringLiteral("lastCheckpointMemory"),
                    lastCheckpoint.value(QStringLiteral("memory"))
                    );
            }

            for (const QJsonValue& value : checkpoints)
            {
                const QJsonObject checkpoint = value.toObject();
                const QString checkpointName =
                    checkpoint.value(QStringLiteral("name")).toString();
                if (checkpointName.startsWith(QStringLiteral("schedule-")))
                {
                    lifecycleCheckpoints.append(checkpoint);
                    lastScheduleLifecycleCheckpoint = checkpoint;
                }
                if (checkpointName == QStringLiteral("workflow-complete"))
                {
                    manifest.insert(
                        QStringLiteral("workflowCompleteElapsedMs"),
                        checkpoint.value(QStringLiteral("elapsedMs"))
                        );
                    workflowCompleted = true;
                }
                if (
                    checkpointName
                        == QStringLiteral("schedule-lifecycle-complete")
                    && checkpoint.value(QStringLiteral("detail"))
                           .toString()
                           .contains(QStringLiteral("passed=true"))
                    )
                {
                    lifecycleCompleted = true;
                }
                if (checkpointName == QStringLiteral("settled-1s"))
                {
                    manifest.insert(
                        QStringLiteral("settledOneSecondElapsedMs"),
                        checkpoint.value(QStringLiteral("elapsedMs"))
                        );
                }
            }
        }
    }

    manifest.insert(
        QStringLiteral("metricsAvailable"),
        !lastCheckpoint.isEmpty()
        );
    manifest.insert(QStringLiteral("workflowCompleted"), workflowCompleted);
    manifest.insert(QStringLiteral("lifecycleCompleted"), lifecycleCompleted);
    manifest.insert(
        QStringLiteral("scheduleLifecycleCheckpoints"),
        lifecycleCheckpoints
        );
    if (!lastScheduleLifecycleCheckpoint.isEmpty())
    {
        manifest.insert(
            QStringLiteral("scheduleLifecycleMetrics"),
            lastScheduleLifecycleCheckpoint.value(QStringLiteral("metrics"))
            );
        manifest.insert(
            QStringLiteral("scheduleLifecycleMemory"),
            lastScheduleLifecycleCheckpoint.value(QStringLiteral("memory"))
            );
    }
    manifest.insert(
        QStringLiteral("routeOutcome"),
        workflowCompleted
            ? QStringLiteral("completed")
            : !finished
                ? QStringLiteral("timed-out")
                : process.exitStatus() != QProcess::NormalExit
                    ? QStringLiteral("abnormal-exit")
                    : QStringLiteral("workflow-incomplete")
        );

    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );

    QVERIFY2(
        lifecycleCompleted,
        qPrintable(
            QStringLiteral(
                "The Schedule lifecycle did not complete.\n"
                "stdout/stderr were retained under %1."
                )
                .arg(outputRoot)
            )
        );
    QVERIFY2(
        workflowCompleted,
        qPrintable(
            QStringLiteral(
                "The full heavy workflow did not complete.\n"
                "stdout/stderr were retained under %1."
                )
                .arg(outputRoot)
            )
        );
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QJsonObject scheduleMetrics =
        lastScheduleLifecycleCheckpoint.value(QStringLiteral("metrics"))
            .toObject();
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleModelRowCount")).toInt(),
        7
        );
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleModelCellCount")).toInt(),
        49
        );
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleModelEntryCount")).toInt(),
        768
        );
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleTableRowCount")).toInt(),
        7
        );
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleTableColumnCount"))
            .toInt(),
        8
        );
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleTableItemCount")).toInt(),
        7
        );
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleTableCellWidgetCount"))
            .toInt(),
        49
        );
    QCOMPARE(
        scheduleMetrics.value(QStringLiteral("scheduleVisibleClassCount"))
            .toInt(),
        96
        );
}

void StartupPerformanceTests::capturesLargeScheduleImportBoundaryWhenConfigured()
{
    const QString cancelOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    const QString applyOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_SCHEDULE_IMPORT_APPLY_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    const bool applyLifecycle = !applyOutputRoot.isEmpty();
    const QString configuredOutputRoot =
        applyLifecycle
            ? applyOutputRoot
            : cancelOutputRoot;
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString workbookPath =
        directory.filePath(QStringLiteral("large-schedule-import.xlsx"));
    QVERIFY2(
        writeLargeScheduleImportWorkbook(workbookPath, applyLifecycle),
        "Unable to write the deterministic large schedule import workbook."
        );

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-schedule-import.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large Schedule Import reference root: %1"
                ).arg(outputRoot)
            )
        );
    const QString retainedWorkbookPath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-schedule-import.xlsx")
            );
    if (QFileInfo::exists(retainedWorkbookPath))
    {
        QVERIFY(QFile::remove(retainedWorkbookPath));
    }
    QVERIFY2(
        QFile::copy(
            workbookPath,
            retainedWorkbookPath
            ),
        "Unable to retain the generated large schedule import workbook."
        );

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-schedule-import-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    if (QFileInfo::exists(metricsPath))
    {
        QVERIFY(QFile::remove(metricsPath));
    }
    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(
            QIODevice::WriteOnly
            | QIODevice::Truncate
            | QIODevice::Text
            ),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_SCHEDULE_IMPORT_PATH"),
        workbookPath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_SCHEDULE_IMPORT_OUTPUT_DIR"),
        outputRoot
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    QStringList arguments{
        QStringLiteral("--startup-performance-test"),
        QStringLiteral("--startup-performance-workflow")
    };
    arguments.append(
        applyLifecycle
            ? QStringLiteral(
                "--startup-performance-schedule-import-apply-lifecycle"
                )
            : QStringLiteral(
                "--startup-performance-schedule-import-lifecycle"
                )
        );
    arguments.append({
        QStringLiteral("--startup-performance-scenario"),
        QStringLiteral("representative"),
        QStringLiteral("--startup-performance-settle-ms"),
        QStringLiteral("1000"),
        QStringLiteral("--startup-performance-output"),
        metricsPath,
        fixturePath
    });
    process.start(
        appPath,
        arguments
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }
    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start schedule")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Schedule transition. "
                "stdout/stderr were retained under %1."
                ).arg(outputRoot)
            )
        );
    const QStringList expectedLifecycleTrace =
        applyLifecycle
            ? QStringList{
                  QStringLiteral("schedule-import-dialog-opened"),
                  QStringLiteral("schedule-import-operation-start"),
                  QStringLiteral("schedule-import-workbook-loaded"),
                  QStringLiteral("schedule-import-parse-complete"),
                  QStringLiteral("schedule-import-review-start"),
                  QStringLiteral("schedule-import-review-prepared"),
                  QStringLiteral("schedule-import-review-ready"),
                  QStringLiteral("schedule-import-apply-start"),
                  QStringLiteral("schedule-import-apply-inputs"),
                  QStringLiteral("schedule-import-apply-prepared"),
                  QStringLiteral("schedule-import-operation-applied"),
                  QStringLiteral("schedule-import-apply-complete"),
                  QStringLiteral("schedule-import-review-released"),
                  QStringLiteral("schedule-import-post-review-release"),
                  QStringLiteral("schedule-import-operation-released"),
                  QStringLiteral("schedule-import-post-release"),
                  QStringLiteral("schedule-import-page-refreshed"),
                  QStringLiteral("schedule-import-operation-end")
              }
            : QStringList{
                  QStringLiteral("schedule-import-dialog-opened"),
                  QStringLiteral("schedule-import-operation-start"),
                  QStringLiteral("schedule-import-workbook-loaded"),
                  QStringLiteral("schedule-import-parse-complete"),
                  QStringLiteral("schedule-import-review-start"),
                  QStringLiteral("schedule-import-review-prepared"),
                  QStringLiteral("schedule-import-review-ready"),
                  QStringLiteral("schedule-import-cancel-start"),
                  QStringLiteral("schedule-import-operation-cancelled"),
                  QStringLiteral("schedule-import-review-released"),
                  QStringLiteral("schedule-import-post-review-release"),
                  QStringLiteral("schedule-import-operation-released"),
                  QStringLiteral("schedule-import-post-release"),
                  QStringLiteral("schedule-import-operation-end")
              };
    for (const QString& expectedTrace : expectedLifecycleTrace)
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Schedule Import lifecycle did not record '%1'. "
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QJsonObject report;
    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument metricsDocument =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject(),
        qPrintable(parseError.errorString())
        );
    report = metricsDocument.object();

    QJsonObject parseCheckpoint;
    QJsonObject reviewCheckpoint;
    QJsonObject applyCheckpoint;
    QJsonObject postReviewCheckpoint;
    QJsonObject postReleaseCheckpoint;
    QJsonObject pageRefreshCheckpoint;
    QJsonObject operationEndCheckpoint;
    bool workflowCompleted = false;
    bool lifecycleCompleted = false;
    for (const QJsonValue& value :
         report.value(QStringLiteral("checkpoints")).toArray())
    {
        const QJsonObject checkpoint = value.toObject();
        const QString name =
            checkpoint.value(QStringLiteral("name")).toString();
        if (name == QStringLiteral("schedule-import-parse-complete"))
        {
            parseCheckpoint = checkpoint;
        }
        else if (name == QStringLiteral("schedule-import-review-ready"))
        {
            reviewCheckpoint = checkpoint;
        }
        else if (name == QStringLiteral("schedule-import-apply-complete"))
        {
            applyCheckpoint = checkpoint;
        }
        else if (name == QStringLiteral("schedule-import-post-review-release"))
        {
            postReviewCheckpoint = checkpoint;
        }
        else if (name == QStringLiteral("schedule-import-post-release"))
        {
            postReleaseCheckpoint = checkpoint;
        }
        else if (name == QStringLiteral("schedule-import-page-refreshed"))
        {
            pageRefreshCheckpoint = checkpoint;
        }
        else if (name == QStringLiteral("schedule-import-operation-end"))
        {
            operationEndCheckpoint = checkpoint;
        }
        else if (name == QStringLiteral("workflow-complete"))
        {
            workflowCompleted = true;
        }
        else if (
            name == QStringLiteral("schedule-import-operation-end")
            && checkpoint.value(QStringLiteral("detail"))
                   .toString()
                   .contains(QStringLiteral("cancelled=true"))
            )
        {
            lifecycleCompleted = true;
        }
    }
    lifecycleCompleted =
        lifecycleCompleted
        || !operationEndCheckpoint.isEmpty()
            && operationEndCheckpoint.value(QStringLiteral("detail"))
                   .toString()
                   .contains(
                       applyLifecycle
                           ? QStringLiteral("committed=true")
                           : QStringLiteral("cancelled=true")
                       );

    QVERIFY(!parseCheckpoint.isEmpty());
    QVERIFY(!reviewCheckpoint.isEmpty());
    if (applyLifecycle)
    {
        QVERIFY(!applyCheckpoint.isEmpty());
        QVERIFY(!pageRefreshCheckpoint.isEmpty());
    }
    QVERIFY(!postReviewCheckpoint.isEmpty());
    QVERIFY(!postReleaseCheckpoint.isEmpty());
    QVERIFY(!operationEndCheckpoint.isEmpty());
    QVERIFY(workflowCompleted);
    QVERIFY(lifecycleCompleted);
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QJsonObject parseMetrics =
        parseCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        parseMetrics.value(
            QStringLiteral("scheduleImportWorkbookSheetCount")
            ).toInt(),
        2
        );
    QCOMPARE(
        parseMetrics.value(
            QStringLiteral("scheduleImportWorkbookUserCount")
            ).toInt(),
        5
        );
    QCOMPARE(
        parseMetrics.value(
            QStringLiteral("scheduleImportWorkbookClassCandidateCount")
            ).toInt(),
        96
        );
    QVERIFY(
        !parseMetrics.value(
            QStringLiteral("scheduleImportRawBytesRetained")
            ).toBool()
        );
    QVERIFY(
        parseMetrics.value(
            QStringLiteral("scheduleImportWorkbookRetained")
            ).toBool()
        );

    const QJsonObject reviewMetrics =
        reviewCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        reviewMetrics.value(
            QStringLiteral("scheduleImportPreviewTeacherCount")
            ).toInt() > 0
        );
    QVERIFY(
        reviewMetrics.value(
            QStringLiteral("scheduleImportPreviewClassCount")
            ).toInt() > 0
        );
    QVERIFY(
        reviewMetrics.value(
            QStringLiteral("scheduleImportReviewTeacherControlCount")
            ).toInt() > 0
        );
    QVERIFY(
        reviewMetrics.value(
            QStringLiteral("scheduleImportReviewClassControlCount")
            ).toInt() > 0
        );
    QVERIFY(
        reviewMetrics.value(
            QStringLiteral("scheduleImportReviewRetained")
            ).toBool()
        );
    QVERIFY(
        !postReviewCheckpoint
             .value(QStringLiteral("metrics"))
             .toObject()
             .value(QStringLiteral("scheduleImportReviewRetained"))
             .toBool()
        );
    if (applyLifecycle)
    {
        const QJsonObject applyMetrics =
            applyCheckpoint.value(QStringLiteral("metrics")).toObject();
        QVERIFY(
            applyMetrics.value(
                QStringLiteral("scheduleImportExistingTeacherCount")
                ).toInt() > 0
            );
        QCOMPARE(
            applyMetrics.value(
                QStringLiteral("scheduleImportExistingClassCount")
                ).toInt(),
            96
            );
        QCOMPARE(
            applyMetrics.value(
                QStringLiteral("scheduleImportExistingClassInfoCount")
                ).toInt(),
            96
            );
        QVERIFY(
            applyMetrics.value(
                QStringLiteral("scheduleImportApplyFinalClassCount")
                ).toInt() > 0
            );
        QVERIFY(
            applyMetrics.value(
                QStringLiteral("scheduleImportApplyFinalScheduleRowCount")
                ).toInt() > 0
            );
        QVERIFY(
            applyMetrics.value(
                QStringLiteral("scheduleImportOperationsApplied")
                ).toInt() >= 1
            );
        QVERIFY(
            pageRefreshCheckpoint.value(QStringLiteral("metrics"))
                .toObject()
                .value(QStringLiteral("scheduleVisibleClassCount"))
                .toInt() > 0
            );
    }
    else
    {
        QVERIFY(
            postReviewCheckpoint
                .value(QStringLiteral("metrics"))
                .toObject()
                .value(QStringLiteral("scheduleImportWorkbookRetained"))
                .toBool()
            );
    }
    const QJsonObject postReleaseMetrics =
        postReleaseCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        !postReleaseMetrics
             .value(QStringLiteral("scheduleImportWorkbookRetained"))
             .toBool()
        );
    QVERIFY(
        postReleaseMetrics
            .value(
                applyLifecycle
                    ? QStringLiteral("scheduleImportOperationsApplied")
                    : QStringLiteral("scheduleImportOperationsCancelled")
                )
            .toInt() >= 1
        );
    QVERIFY(
        postReleaseMetrics
            .value(QStringLiteral("scheduleImportOperationsReleased"))
            .toInt() >= 1
        );

    QJsonObject manifest;
    manifest.insert(QStringLiteral("fixture"), QStringLiteral("large_startup.sql"));
    manifest.insert(
        QStringLiteral("workbookFixture"),
        QStringLiteral("generated large-schedule-import.xlsx")
        );
    manifest.insert(
        QStringLiteral("fixtureScale"),
        applyLifecycle
            ? QStringLiteral("large_startup_schedule_import_apply_lifecycle")
            : QStringLiteral("large_startup_schedule_import_lifecycle")
        );
    manifest.insert(
        QStringLiteral("scenario"),
        applyLifecycle
            ? QStringLiteral(
                "96-class heavy route, workbook parse, review, conflict acknowledgement, transaction apply, cleanup, schedule refresh"
                )
            : QStringLiteral(
                "96-class heavy route, workbook parse, review, conflict acknowledgement, cancel, cleanup"
                )
        );
    manifest.insert(QStringLiteral("teacherCount"), 24);
    manifest.insert(QStringLiteral("classCount"), 96);
    manifest.insert(QStringLiteral("workbookBytes"), QFileInfo(workbookPath).size());
    manifest.insert(QStringLiteral("processFinished"), finished);
    manifest.insert(
        QStringLiteral("exitStatus"),
        process.exitStatus() == QProcess::NormalExit
            ? QStringLiteral("normal")
            : QStringLiteral("crash")
        );
    manifest.insert(QStringLiteral("exitCode"), process.exitCode());
    manifest.insert(QStringLiteral("timedOut"), !finished);
    manifest.insert(QStringLiteral("traceLineCount"), traceLines.size());
    manifest.insert(
        QStringLiteral("metricsPath"),
        QStringLiteral("large-schedule-import-workflow.json")
        );
    manifest.insert(
        QStringLiteral("tracePath"),
        QStringLiteral("workflow-trace.txt")
        );
    manifest.insert(
        QStringLiteral("stdoutPath"),
        QStringLiteral("process-stdout.txt")
        );
    manifest.insert(
        QStringLiteral("stderrPath"),
        QStringLiteral("process-stderr.txt")
        );
    manifest.insert(
        QStringLiteral("reviewCheckpointMetrics"),
        reviewCheckpoint.value(QStringLiteral("metrics"))
        );
    if (applyLifecycle)
    {
        manifest.insert(
            QStringLiteral("applyCheckpointMetrics"),
            applyCheckpoint.value(QStringLiteral("metrics"))
            );
        manifest.insert(
            QStringLiteral("pageRefreshCheckpointMetrics"),
            pageRefreshCheckpoint.value(QStringLiteral("metrics"))
            );
    }
    manifest.insert(
        QStringLiteral("postReleaseCheckpointMetrics"),
        postReleaseCheckpoint.value(QStringLiteral("metrics"))
        );

    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
    QVERIFY(
        QFileInfo::exists(
            QDir(outputRoot).filePath(
                QStringLiteral("schedule-import-source.png")
                )
            )
        );
    QVERIFY(
        QFileInfo::exists(
            QDir(outputRoot).filePath(
                QStringLiteral("schedule-import-review.png")
                )
            )
        );
}

void StartupPerformanceTests::capturesLargeScheduleImportApplyBoundaryWhenConfigured()
{
    capturesLargeScheduleImportBoundaryWhenConfigured();
}

void StartupPerformanceTests::capturesLargeClassTransferBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral(
                "ClassMngr executable does not exist: %1"
                ).arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString packagePath =
        directory.filePath(QStringLiteral("large-class-transfer.json"));
    QVERIFY2(
        writeLargeClassTransferPackage(packagePath),
        "Unable to write the deterministic large class-transfer package."
        );

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-class-transfer.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large Class Transfer reference root: %1"
                ).arg(outputRoot)
            )
        );
    const QString retainedPackagePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-class-transfer.json")
            );
    if (QFileInfo::exists(retainedPackagePath))
    {
        QVERIFY(QFile::remove(retainedPackagePath));
    }
    QVERIFY(QFile::copy(packagePath, retainedPackagePath));

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-class-transfer-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    if (QFileInfo::exists(metricsPath))
    {
        QVERIFY(QFile::remove(metricsPath));
    }
    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(
            QIODevice::WriteOnly
            | QIODevice::Truncate
            | QIODevice::Text
            ),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_CLASS_TRANSFER_PATH"),
        packagePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_CLASS_TRANSFER_OUTPUT_DIR"),
        outputRoot
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral(
                "--startup-performance-class-transfer-lifecycle"
                ),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }
    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start classes")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Classes transition. "
                "stdout/stderr were retained under %1."
                ).arg(outputRoot)
            )
        );
    for (const QString& expectedTrace : {
             QStringLiteral("class-transfer-source-opened"),
             QStringLiteral("class-transfer-operation-start"),
             QStringLiteral("class-transfer-package-loaded"),
             QStringLiteral("class-transfer-preview-prepared"),
             QStringLiteral("class-transfer-preview-ready"),
             QStringLiteral("class-transfer-dialog-opened"),
             QStringLiteral("class-transfer-apply-start"),
             QStringLiteral("class-transfer-operation-applied"),
             QStringLiteral("class-transfer-dialog-released"),
             QStringLiteral("class-transfer-operation-released"),
             QStringLiteral("class-transfer-post-release"),
             QStringLiteral("class-transfer-page-refreshed"),
             QStringLiteral("class-transfer-operation-end committed=true")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Class Transfer lifecycle did not record '%1'. "
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument metricsDocument =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject(),
        qPrintable(parseError.errorString())
        );
    const QJsonObject report = metricsDocument.object();
    const auto checkpointNamed =
        [&report](const QString& name)
        {
            for (const QJsonValue& value :
                 report.value(QStringLiteral("checkpoints")).toArray())
            {
                const QJsonObject checkpoint = value.toObject();
                if (checkpoint.value(QStringLiteral("name")).toString() == name)
                {
                    return checkpoint;
                }
            }
            return QJsonObject{};
        };

    const QJsonObject packageCheckpoint =
        checkpointNamed(QStringLiteral("class-transfer-package-loaded"));
    const QJsonObject previewCheckpoint =
        checkpointNamed(QStringLiteral("class-transfer-preview-prepared"));
    const QJsonObject dialogCheckpoint =
        checkpointNamed(QStringLiteral("class-transfer-dialog-opened"));
    const QJsonObject appliedCheckpoint =
        checkpointNamed(QStringLiteral("class-transfer-operation-applied"));
    const QJsonObject releasedCheckpoint =
        checkpointNamed(QStringLiteral("class-transfer-operation-released"));
    const QJsonObject pageCheckpoint =
        checkpointNamed(QStringLiteral("class-transfer-page-refreshed"));
    const QJsonObject workflowCheckpoint =
        checkpointNamed(QStringLiteral("workflow-complete"));

    QVERIFY(!packageCheckpoint.isEmpty());
    QVERIFY(!previewCheckpoint.isEmpty());
    QVERIFY(!dialogCheckpoint.isEmpty());
    QVERIFY(!appliedCheckpoint.isEmpty());
    QVERIFY(!releasedCheckpoint.isEmpty());
    QVERIFY(!pageCheckpoint.isEmpty());
    QVERIFY(!workflowCheckpoint.isEmpty());
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QJsonObject packageMetrics =
        packageCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        packageMetrics.value(QStringLiteral("classTransferPackageTeacherCount"))
            .toInt(),
        LargeClassTransferTeacherCount
        );
    QCOMPARE(
        packageMetrics.value(QStringLiteral("classTransferPackageClassCount"))
            .toInt(),
        LargeClassTransferClassCount
        );
    QCOMPARE(
        packageMetrics.value(QStringLiteral("classTransferPackageRosterCellCount"))
            .toInt(),
        LargeClassTransferClassCount
            * LargeClassTransferRosterColumnCount
            * LargeClassTransferRosterRowCount
        );
    QCOMPARE(
        packageMetrics.value(QStringLiteral("classTransferPackageEvaluationCount"))
            .toInt(),
        LargeClassTransferClassCount * LargeClassTransferEvaluationCount
        );
    QVERIFY(
        packageMetrics.value(QStringLiteral("classTransferPackageEvaluationCellCount"))
            .toInt() > 20000
        );
    QVERIFY(
        !packageMetrics.value(QStringLiteral("classTransferRawBytesRetained"))
            .toBool()
        );
    QVERIFY(
        packageMetrics.value(QStringLiteral("classTransferPackageRetained"))
            .toBool()
        );

    const QJsonObject previewMetrics =
        previewCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        previewMetrics.value(QStringLiteral("classTransferPreviewTeacherCount"))
            .toInt(),
        LargeClassTransferTeacherCount
        );
    QCOMPARE(
        previewMetrics.value(QStringLiteral("classTransferPreviewClassCount"))
            .toInt(),
        LargeClassTransferClassCount
        );
    QCOMPARE(
        previewMetrics.value(QStringLiteral("classTransferDestinationClassCount"))
            .toInt(),
        96
        );
    QCOMPARE(
        previewMetrics.value(QStringLiteral("classTransferMatchingClassCount"))
            .toInt(),
        0
        );
    QVERIFY(
        previewMetrics.value(QStringLiteral("classTransferPreviewRetained"))
            .toBool()
        );

    const QJsonObject dialogMetrics =
        dialogCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        dialogMetrics.value(QStringLiteral("classTransferDialogTeacherControlCount"))
            .toInt(),
        LargeClassTransferTeacherCount
        );
    QCOMPARE(
        dialogMetrics.value(QStringLiteral("classTransferDialogClassControlCount"))
            .toInt(),
        LargeClassTransferClassCount
        );
    QVERIFY(
        dialogMetrics.value(QStringLiteral("classTransferDialogRetained"))
            .toBool()
        );

    const QJsonObject appliedMetrics =
        appliedCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        appliedMetrics.value(QStringLiteral("classTransferClassesCreated"))
            .toInt(),
        LargeClassTransferClassCount
        );
    QCOMPARE(
        appliedMetrics.value(QStringLiteral("classTransferTeachersCreated"))
            .toInt(),
        LargeClassTransferTeacherCount
        );
    QCOMPARE(
        appliedMetrics.value(QStringLiteral("classTransferDestinationClassesBefore"))
            .toInt(),
        96
        );
    QCOMPARE(
        appliedMetrics.value(QStringLiteral("classTransferDestinationClassesAfter"))
            .toInt(),
        96 + LargeClassTransferClassCount
        );

    const QJsonObject releasedMetrics =
        releasedCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        !releasedMetrics.value(QStringLiteral("classTransferRawBytesRetained"))
            .toBool()
        );
    QVERIFY(
        !releasedMetrics.value(QStringLiteral("classTransferJsonDocumentRetained"))
            .toBool()
        );
    QVERIFY(
        !releasedMetrics.value(QStringLiteral("classTransferPackageRetained"))
            .toBool()
        );
    QVERIFY(
        !releasedMetrics.value(QStringLiteral("classTransferPreviewRetained"))
            .toBool()
        );
    QVERIFY(
        !releasedMetrics.value(QStringLiteral("classTransferDialogRetained"))
            .toBool()
        );
    QVERIFY(
        !releasedMetrics.value(QStringLiteral("classTransferOperationRetained"))
            .toBool()
        );

    const QJsonObject pageMetrics =
        pageCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        pageMetrics.value(QStringLiteral("classesSourceClassCount")).toInt(),
        144
        );
    QVERIFY(
        pageMetrics.value(QStringLiteral("classTransferOperationRetained"))
            .toBool() == false
        );

    QJsonObject manifest{
        {QStringLiteral("fixture"), QStringLiteral("large_startup.sql")},
        {
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_class_transfer_package")
        },
        {
            QStringLiteral("scenario"),
            QStringLiteral(
                "Classes entry, multi-class package review, transaction commit, release, and refresh"
                )
        },
        {QStringLiteral("teacherCount"), LargeClassTransferTeacherCount},
        {QStringLiteral("classCount"), LargeClassTransferClassCount},
        {
            QStringLiteral("rosterCellCount"),
            LargeClassTransferClassCount
                * LargeClassTransferRosterColumnCount
                * LargeClassTransferRosterRowCount
        },
        {
            QStringLiteral("evaluationCount"),
            LargeClassTransferClassCount * LargeClassTransferEvaluationCount
        },
        {QStringLiteral("processFinished"), finished},
        {
            QStringLiteral("exitStatus"),
            process.exitStatus() == QProcess::NormalExit
                ? QStringLiteral("normal")
                : QStringLiteral("crash")
        },
        {QStringLiteral("exitCode"), process.exitCode()},
        {QStringLiteral("timedOut"), !finished},
        {QStringLiteral("traceLineCount"), traceLines.size()},
        {QStringLiteral("tracePath"), QStringLiteral("workflow-trace.txt")},
        {
            QStringLiteral("metricsPath"),
            QStringLiteral("large-class-transfer-workflow.json")
        },
        {QStringLiteral("stdoutPath"), QStringLiteral("process-stdout.txt")},
        {QStringLiteral("stderrPath"), QStringLiteral("process-stderr.txt")},
        {
            QStringLiteral("peakMemory"),
            report.value(QStringLiteral("peakMemory"))
        },
        {
            QStringLiteral("lastCheckpoint"),
            report.value(QStringLiteral("checkpoints")).toArray().isEmpty()
                ? QJsonObject{}
                : report.value(QStringLiteral("checkpoints"))
                    .toArray().last().toObject()
        }
    };
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
    QVERIFY(
        QFileInfo::exists(
            QDir(outputRoot).filePath(
                QStringLiteral("class-transfer-review.png")
                )
            )
        );
}

void StartupPerformanceTests::capturesLargeSpeakingEvaluationBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral(
                "ClassMngr executable does not exist: %1"
                ).arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-speaking-evaluation.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        addLargeSpeakingEvaluationFixtureData(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large Speaking Evaluation reference root: %1"
                ).arg(outputRoot)
            )
        );

    const QString retainedFixturePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-speaking-evaluation.tps")
            );
    if (QFileInfo::exists(retainedFixturePath))
    {
        QVERIFY(QFile::remove(retainedFixturePath));
    }
    QVERIFY(QFile::copy(fixturePath, retainedFixturePath));

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-speaking-evaluation-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    for (const QString& fileName : {
             QStringLiteral("large-speaking-evaluation-workflow.json"),
             QStringLiteral("workflow-trace.txt"),
             QStringLiteral("manifest.json"),
             QStringLiteral("process-stdout.txt"),
             QStringLiteral("process-stderr.txt"),
             QStringLiteral("speaking-evaluation-page.png"),
             QStringLiteral("speaking-evaluation-report.png"),
             QStringLiteral("speaking-evaluation-export-dialog.png"),
             QStringLiteral("speaking-evaluation-ai-review.png")
         })
    {
        const QString path = QDir(outputRoot).filePath(fileName);
        if (QFileInfo::exists(path))
        {
            QVERIFY(QFile::remove(path));
        }
    }

    const QString reportOutputDirectory =
        QDir(outputRoot).filePath(
            QStringLiteral("speaking-evaluation-output")
            );
    QVERIFY(QDir().mkpath(reportOutputDirectory));
    const QDir reportOutput(reportOutputDirectory);
    for (const QFileInfo& fileInfo : reportOutput.entryInfoList(
             QDir::Files | QDir::NoDotAndDotDot,
             QDir::Name
             ))
    {
        QVERIFY(QFile::remove(fileInfo.absoluteFilePath()));
    }

    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(
            QIODevice::WriteOnly
            | QIODevice::Truncate
            | QIODevice::Text
            ),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_SPEAKING_EVALUATION_OUTPUT_DIR"),
        outputRoot
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral(
                "--startup-performance-speaking-evaluation-lifecycle"
                ),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }
    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start classes")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Classes transition. "
                "stdout/stderr were retained under %1."
                ).arg(outputRoot)
            )
        );
    for (const QString& expectedTrace : {
             QStringLiteral("speaking-evaluation-source-opened"),
             QStringLiteral("speaking-evaluation-operation-start"),
             QStringLiteral("speaking-evaluation-page-prepared"),
             QStringLiteral("speaking-evaluation-reports-prepared"),
             QStringLiteral("speaking-evaluation-report-dialog-prepared"),
             QStringLiteral("speaking-evaluation-report-dialog-released"),
             QStringLiteral("speaking-evaluation-export-dialog-prepared"),
             QStringLiteral("speaking-evaluation-export-dialog-released"),
             QStringLiteral("speaking-evaluation-export-start"),
             QStringLiteral("speaking-evaluation-export-complete"),
             QStringLiteral("speaking-evaluation-ai-dialog-prepared"),
             QStringLiteral("speaking-evaluation-ai-response-prepared"),
             QStringLiteral("speaking-evaluation-ai-dialog-released"),
             QStringLiteral("speaking-evaluation-comments-applied"),
             QStringLiteral("speaking-evaluation-operation-released"),
             QStringLiteral("speaking-evaluation-page-refreshed"),
             QStringLiteral("complete")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Speaking Evaluation lifecycle did not record '%1'. "
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument metricsDocument =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject(),
        qPrintable(parseError.errorString())
        );
    const QJsonObject report = metricsDocument.object();
    const auto checkpointNamed =
        [&report](const QString& name)
        {
            for (const QJsonValue& value :
                 report.value(QStringLiteral("checkpoints")).toArray())
            {
                const QJsonObject checkpoint = value.toObject();
                if (checkpoint.value(QStringLiteral("name")).toString() == name)
                {
                    return checkpoint;
                }
            }
            return QJsonObject{};
        };

    const QJsonObject pageCheckpoint =
        checkpointNamed(QStringLiteral("speaking-evaluation-page-prepared"));
    const QJsonObject reportCheckpoint =
        checkpointNamed(QStringLiteral("speaking-evaluation-reports-prepared"));
    const QJsonObject reportDialogCheckpoint =
        checkpointNamed(
            QStringLiteral("speaking-evaluation-report-dialog-prepared")
            );
    const QJsonObject exportDialogCheckpoint =
        checkpointNamed(
            QStringLiteral("speaking-evaluation-export-dialog-prepared")
            );
    const QJsonObject powerPointRendererCheckpoint =
        checkpointNamed(
            QStringLiteral("speaking-evaluation-powerpoint-renderer-ready")
            );
    const QJsonObject powerPointUnavailableCheckpoint =
        checkpointNamed(
            QStringLiteral("speaking-evaluation-powerpoint-renderer-unavailable")
            );
    const QJsonObject exportCheckpoint =
        checkpointNamed(QStringLiteral("speaking-evaluation-export-complete"));
    const QJsonObject aiCheckpoint =
        checkpointNamed(QStringLiteral("speaking-evaluation-ai-dialog-prepared"));
    const QJsonObject responseCheckpoint =
        checkpointNamed(
            QStringLiteral("speaking-evaluation-ai-response-prepared")
            );
    const QJsonObject releasedCheckpoint =
        checkpointNamed(
            QStringLiteral("speaking-evaluation-operation-released")
            );
    const QJsonObject refreshedCheckpoint =
        checkpointNamed(QStringLiteral("speaking-evaluation-page-refreshed"));
    const QJsonObject workflowCheckpoint =
        checkpointNamed(QStringLiteral("workflow-complete"));

    QVERIFY(!pageCheckpoint.isEmpty());
    QVERIFY(!reportCheckpoint.isEmpty());
    QVERIFY(!reportDialogCheckpoint.isEmpty());
    QVERIFY(!exportDialogCheckpoint.isEmpty());
    QVERIFY(
        powerPointRendererCheckpoint.isEmpty()
            != powerPointUnavailableCheckpoint.isEmpty()
        );
    QVERIFY(!exportCheckpoint.isEmpty());
    QVERIFY(!aiCheckpoint.isEmpty());
    QVERIFY(!responseCheckpoint.isEmpty());
    QVERIFY(!releasedCheckpoint.isEmpty());
    QVERIFY(!refreshedCheckpoint.isEmpty());
    QVERIFY(!workflowCheckpoint.isEmpty());
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QJsonObject pageMetrics =
        pageCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalSourceClassCount"))
            .toInt(),
        LargeSpeakingEvaluationClassCount
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalVisibleClassCount"))
            .toInt(),
        LargeSpeakingEvaluationClassCount
        );
    QVERIFY(
        pageMetrics.value(QStringLiteral("speakingEvalClassTabWidgetCount"))
            .toInt() > 0
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalClassTabCount"))
            .toInt(),
        LargeSpeakingEvaluationClassCount
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalEvaluationTabCount"))
            .toInt(),
        4
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalModelRowCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalModelColumnCount"))
            .toInt(),
        SpeakingEval::ColumnCount
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalModelCellCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount * SpeakingEval::ColumnCount
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalLoadedEvaluationCount"))
            .toInt(),
        1
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalLoadedEvaluationRowCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QCOMPARE(
        pageMetrics.value(QStringLiteral("speakingEvalLoadedEvaluationCellCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount * SpeakingEval::ColumnCount
        );

    const QJsonObject reportMetrics =
        reportCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        reportMetrics.value(QStringLiteral("speakingEvalBatchReportCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QVERIFY(
        reportMetrics.value(QStringLiteral("speakingEvalBatchReportTextBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        reportMetrics.value(QStringLiteral("speakingEvalReportListRetained"))
            .toBool()
        );

    const QJsonObject reportDialogMetrics =
        reportDialogCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        reportDialogMetrics
            .value(QStringLiteral("speakingEvalReportDialogReportCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QVERIFY(
        reportDialogMetrics
            .value(QStringLiteral("speakingEvalReportDialogRetained"))
            .toBool()
        );

    const QJsonObject exportDialogMetrics =
        exportDialogCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        exportDialogMetrics
            .value(QStringLiteral("speakingEvalExportDialogReportCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QVERIFY(
        exportDialogMetrics
            .value(QStringLiteral("speakingEvalExportDialogRetained"))
            .toBool()
        );

    const QJsonObject exportMetrics =
        exportCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        exportMetrics.value(QStringLiteral("speakingEvalExportPdfCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QVERIFY(
        exportMetrics.value(QStringLiteral("speakingEvalExportPdfBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        exportMetrics.value(QStringLiteral("speakingEvalExportArchiveBytes"))
            .toDouble() > 0
        );

    const QJsonObject aiMetrics =
        aiCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        aiMetrics.value(QStringLiteral("speakingEvalAiDialogReportCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QCOMPARE(
        aiMetrics.value(QStringLiteral("speakingEvalAiSelectionRowCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QCOMPARE(
        aiMetrics.value(QStringLiteral("speakingEvalAiSelectionColumnCount"))
            .toInt(),
        3
        );
    QCOMPARE(
        aiMetrics.value(QStringLiteral("speakingEvalAiSelectionItemCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount * 3
        );
    QVERIFY(
        aiMetrics.value(QStringLiteral("speakingEvalAiDialogRetained"))
            .toBool()
        );

    const QJsonObject responseMetrics =
        responseCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        responseMetrics.value(QStringLiteral("speakingEvalAiReviewRowCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QCOMPARE(
        responseMetrics.value(QStringLiteral("speakingEvalAiReviewColumnCount"))
            .toInt(),
        5
        );
    QCOMPARE(
        responseMetrics.value(QStringLiteral("speakingEvalAiReviewItemCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount * 5
        );
    QCOMPARE(
        responseMetrics.value(QStringLiteral("speakingEvalAiAcceptedCommentCount"))
            .toInt(),
        LargeSpeakingEvaluationRowCount
        );
    QVERIFY(
        responseMetrics.value(QStringLiteral("speakingEvalAiPromptBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        responseMetrics.value(QStringLiteral("speakingEvalAiResponseBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        responseMetrics.value(QStringLiteral("speakingEvalAiResponseRetained"))
            .toBool()
        );

    const QJsonObject releasedMetrics =
        releasedCheckpoint.value(QStringLiteral("metrics")).toObject();
    for (const QString& key : {
             QStringLiteral("speakingEvalReportListRetained"),
             QStringLiteral("speakingEvalReportDialogRetained"),
             QStringLiteral("speakingEvalExportDialogRetained"),
             QStringLiteral("speakingEvalAiDialogRetained"),
             QStringLiteral("speakingEvalAiResponseRetained"),
             QStringLiteral("speakingEvalExportOperationRetained"),
             QStringLiteral("speakingEvalOperationRetained")
         })
    {
        QVERIFY2(
            !releasedMetrics.value(key).toBool(),
            qPrintable(
                QStringLiteral(
                    "Speaking Evaluation retention flag remained set: %1"
                    ).arg(key)
                )
            );
    }
    QCOMPARE(
        releasedMetrics.value(QStringLiteral("speakingEvalOperationsReleased"))
            .toDouble(),
        1.0
        );

    QVERIFY(
        !refreshedCheckpoint.value(QStringLiteral("metrics"))
            .toObject()
            .value(QStringLiteral("speakingEvalOperationRetained"))
            .toBool()
        );

    const QStringList pdfFiles =
        reportOutput.entryList(
            QStringList{QStringLiteral("*.pdf")},
            QDir::Files,
            QDir::Name
            );
    QCOMPARE(pdfFiles.size(), LargeSpeakingEvaluationRowCount);
    const QString archivePath =
        reportOutput.filePath(
            QStringLiteral("speaking-evaluation-output.zip")
            );
    QVERIFY2(
        QFileInfo(archivePath).size() > 0,
        qPrintable(
            QStringLiteral("Expected Speaking Evaluation archive was not created: %1")
                .arg(archivePath)
            )
        );
    for (const QString& imageName : {
             QStringLiteral("speaking-evaluation-page.png"),
             QStringLiteral("speaking-evaluation-report.png"),
             QStringLiteral("speaking-evaluation-export-dialog.png"),
             QStringLiteral("speaking-evaluation-ai-review.png")
         })
    {
        QVERIFY2(
            QFileInfo(
                QDir(outputRoot).filePath(imageName)
                ).size() > 0,
            qPrintable(
                QStringLiteral("Expected Speaking Evaluation capture is missing: %1")
                    .arg(imageName)
                )
        );
    }
    if (!powerPointRendererCheckpoint.isEmpty())
    {
        QVERIFY(
            QFileInfo(
                QDir(outputRoot).filePath(
                    QStringLiteral(
                        "speaking-evaluation-powerpoint-renderer.png"
                        )
                    )
                ).size() > 0
            );
    }

    QJsonObject manifest{
        {QStringLiteral("fixture"), QStringLiteral("large_startup.sql")},
        {
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_speaking_evaluation_batch")
        },
        {
            QStringLiteral("scenario"),
            QStringLiteral(
                "96-class speaking-evaluation navigation, report review, AI batch review, PDF export, release, and refresh"
                )
        },
        {QStringLiteral("classCount"), LargeSpeakingEvaluationClassCount},
        {
            QStringLiteral("canonicalEvaluationCount"),
            LargeSpeakingEvaluationClassCount
        },
        {
            QStringLiteral("evaluationRowCount"),
            LargeSpeakingEvaluationClassCount * LargeSpeakingEvaluationRowCount
        },
        {QStringLiteral("selectedBatchReportCount"), LargeSpeakingEvaluationRowCount},
        {
            QStringLiteral("powerPointRendererVisualReference"),
            !powerPointRendererCheckpoint.isEmpty()
        },
        {QStringLiteral("powerPointAutomationExecuted"), false},
        {QStringLiteral("processFinished"), finished},
        {
            QStringLiteral("exitStatus"),
            process.exitStatus() == QProcess::NormalExit
                ? QStringLiteral("normal")
                : QStringLiteral("crash")
        },
        {QStringLiteral("exitCode"), process.exitCode()},
        {QStringLiteral("timedOut"), !finished},
        {QStringLiteral("traceLineCount"), traceLines.size()},
        {QStringLiteral("tracePath"), QStringLiteral("workflow-trace.txt")},
        {
            QStringLiteral("metricsPath"),
            QStringLiteral("large-speaking-evaluation-workflow.json")
        },
        {QStringLiteral("stdoutPath"), QStringLiteral("process-stdout.txt")},
        {QStringLiteral("stderrPath"), QStringLiteral("process-stderr.txt")},
        {
            QStringLiteral("outputDirectory"),
            QStringLiteral("speaking-evaluation-output")
        },
        {
            QStringLiteral("peakMemory"),
            report.value(QStringLiteral("peakMemory"))
        },
        {
            QStringLiteral("lastCheckpoint"),
            report.value(QStringLiteral("checkpoints")).toArray().isEmpty()
                ? QJsonObject{}
                : report.value(QStringLiteral("checkpoints"))
                    .toArray().last().toObject()
        }
    };
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
}

void StartupPerformanceTests::capturesLargeStaffDirectoryBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral(
                "ClassMngr executable does not exist: %1"
                ).arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-staff-directory.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        addLargeStaffDirectoryFixtureData(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large Staff Directory reference root: %1"
                ).arg(outputRoot)
            )
        );

    const QString retainedFixturePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-staff-directory.tps")
            );
    if (QFileInfo::exists(retainedFixturePath))
    {
        QVERIFY(QFile::remove(retainedFixturePath));
    }
    QVERIFY(QFile::copy(fixturePath, retainedFixturePath));

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-staff-directory-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    for (const QString& fileName : {
             QStringLiteral("large-staff-directory-workflow.json"),
             QStringLiteral("workflow-trace.txt"),
             QStringLiteral("manifest.json"),
             QStringLiteral("process-stdout.txt"),
             QStringLiteral("process-stderr.txt"),
             QStringLiteral("staff-directory-native-english-teachers.png"),
             QStringLiteral("staff-directory-gs-team.png")
         })
    {
        const QString path = QDir(outputRoot).filePath(fileName);
        if (QFileInfo::exists(path))
        {
            QVERIFY(QFile::remove(path));
        }
    }

    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(
            QIODevice::WriteOnly
            | QIODevice::Truncate
            | QIODevice::Text
            ),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_STAFF_DIRECTORY_OUTPUT_DIR"),
        outputRoot
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-staff-directory-lifecycle"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }
    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start native-english-teachers")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Native English Teacher transition. "
                "stdout/stderr were retained under %1."
                ).arg(outputRoot)
            )
        );
    QVERIFY2(
        traceLines.contains(QStringLiteral("start gs-team")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the GS Team transition. "
                "stdout/stderr were retained under %1."
                ).arg(outputRoot)
            )
        );
    for (const QString& expectedTrace : {
             QStringLiteral("staff-directory-native-operation-start"),
             QStringLiteral("staff-directory-native-page-prepared"),
             QStringLiteral("staff-directory-native-refresh-1"),
             QStringLiteral("staff-directory-native-refresh-2"),
             QStringLiteral("staff-directory-native-page-left"),
             QStringLiteral("staff-directory-native-page-reentered"),
             QStringLiteral("staff-directory-native-operation-released"),
             QStringLiteral("staff-directory-gs-operation-start"),
             QStringLiteral("staff-directory-gs-page-prepared"),
             QStringLiteral("staff-directory-gs-refresh-1"),
             QStringLiteral("staff-directory-gs-refresh-2"),
             QStringLiteral("staff-directory-gs-page-left"),
             QStringLiteral("staff-directory-gs-page-reentered"),
             QStringLiteral("staff-directory-gs-operation-released"),
             QStringLiteral("complete")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Staff Directory lifecycle did not record '%1'. "
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument metricsDocument =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject(),
        qPrintable(parseError.errorString())
        );
    const QJsonObject report = metricsDocument.object();
    const auto checkpointNamed =
        [&report](const QString& name)
        {
            for (const QJsonValue& value :
                 report.value(QStringLiteral("checkpoints")).toArray())
            {
                const QJsonObject checkpoint = value.toObject();
                if (checkpoint.value(QStringLiteral("name")).toString() == name)
                {
                    return checkpoint;
                }
            }
            return QJsonObject{};
        };

    const QJsonObject nativePrepared = checkpointNamed(
        QStringLiteral("staff-directory-native-page-prepared")
        );
    const QJsonObject nativeRefresh = checkpointNamed(
        QStringLiteral("staff-directory-native-refresh-2")
        );
    const QJsonObject nativeLeft = checkpointNamed(
        QStringLiteral("staff-directory-native-page-left")
        );
    const QJsonObject nativeReentered = checkpointNamed(
        QStringLiteral("staff-directory-native-page-reentered")
        );
    const QJsonObject nativeReleased = checkpointNamed(
        QStringLiteral("staff-directory-native-operation-released")
        );
    const QJsonObject gsPrepared = checkpointNamed(
        QStringLiteral("staff-directory-gs-page-prepared")
        );
    const QJsonObject gsRefresh = checkpointNamed(
        QStringLiteral("staff-directory-gs-refresh-2")
        );
    const QJsonObject gsLeft = checkpointNamed(
        QStringLiteral("staff-directory-gs-page-left")
        );
    const QJsonObject gsReentered = checkpointNamed(
        QStringLiteral("staff-directory-gs-page-reentered")
        );
    const QJsonObject gsReleased = checkpointNamed(
        QStringLiteral("staff-directory-gs-operation-released")
        );
    const QJsonObject workflowCheckpoint = checkpointNamed(
        QStringLiteral("workflow-complete")
        );

    for (const QJsonObject& checkpoint : {
             nativePrepared,
             nativeRefresh,
             nativeLeft,
             nativeReentered,
             nativeReleased,
             gsPrepared,
             gsRefresh,
             gsLeft,
             gsReentered,
             gsReleased,
             workflowCheckpoint
         })
    {
        QVERIFY(!checkpoint.isEmpty());
        QVERIFY(
            checkpoint.value(QStringLiteral("memory"))
                .toObject()
                .value(QStringLiteral("available"))
                .toBool()
            );
    }
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QJsonObject nativeMetrics =
        nativePrepared.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        nativeMetrics.value(QStringLiteral("staffDirectoryNativeRowCount"))
            .toInt(),
        LargeStaffDirectoryEntryCount
        );
    QCOMPARE(
        nativeMetrics.value(QStringLiteral("staffDirectoryNativeColumnCount"))
            .toInt(),
        LargeStaffDirectoryNativeColumnCount
        );
    QCOMPARE(
        nativeMetrics.value(QStringLiteral("staffDirectoryNativeItemCount"))
            .toInt(),
        LargeStaffDirectoryEntryCount * LargeStaffDirectoryNativeColumnCount
        );
    QVERIFY(
        nativeMetrics.value(QStringLiteral("staffDirectoryNativePageWidgetCount"))
            .toInt() > 0
        );
    QVERIFY(
        nativeMetrics.value(QStringLiteral("staffDirectoryNativeTextBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        nativeMetrics.value(QStringLiteral("staffDirectoryNativeTableRetained"))
            .toBool()
        );

    const QJsonObject gsMetrics =
        gsPrepared.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        gsMetrics.value(QStringLiteral("staffDirectoryGsRowCount"))
            .toInt(),
        LargeStaffDirectoryEntryCount
        );
    QCOMPARE(
        gsMetrics.value(QStringLiteral("staffDirectoryGsColumnCount"))
            .toInt(),
        LargeStaffDirectoryGsColumnCount
        );
    QCOMPARE(
        gsMetrics.value(QStringLiteral("staffDirectoryGsItemCount"))
            .toInt(),
        LargeStaffDirectoryEntryCount * LargeStaffDirectoryGsColumnCount
        );
    QVERIFY(
        gsMetrics.value(QStringLiteral("staffDirectoryGsPageWidgetCount"))
            .toInt() > 0
        );
    QVERIFY(
        gsMetrics.value(QStringLiteral("staffDirectoryGsTextBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        gsMetrics.value(QStringLiteral("staffDirectoryGsTableRetained"))
            .toBool()
        );

    const QJsonObject nativeRefreshMetrics =
        nativeRefresh.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        nativeRefreshMetrics.value(QStringLiteral("staffDirectoryNativeRefreshCount"))
            .toInt(),
        2
        );
    QCOMPARE(
        nativeRefreshMetrics.value(QStringLiteral("staffDirectoryNativeItemCount"))
            .toInt(),
        LargeStaffDirectoryEntryCount * LargeStaffDirectoryNativeColumnCount
        );
    const QJsonObject nativeLeftMetrics =
        nativeLeft.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        nativeLeftMetrics.value(QStringLiteral("staffDirectoryNativeTableRetained"))
            .toBool()
        );
    QVERIFY(
        nativeLeftMetrics.value(QStringLiteral("staffDirectoryNativeOperationRetained"))
            .toBool()
        );
    const QJsonObject nativeReentryMetrics =
        nativeReentered.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        nativeReentryMetrics.value(QStringLiteral("staffDirectoryNativeReentryCount"))
            .toInt(),
        1
        );
    const QJsonObject nativeReleasedMetrics =
        nativeReleased.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        !nativeReleasedMetrics
             .value(QStringLiteral("staffDirectoryNativeOperationRetained"))
             .toBool()
        );
    QVERIFY(
        nativeReleasedMetrics
            .value(QStringLiteral("staffDirectoryNativeTableRetained"))
            .toBool()
        );

    const QJsonObject gsRefreshMetrics =
        gsRefresh.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        gsRefreshMetrics.value(QStringLiteral("staffDirectoryGsRefreshCount"))
            .toInt(),
        2
        );
    const QJsonObject gsLeftMetrics =
        gsLeft.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        gsLeftMetrics.value(QStringLiteral("staffDirectoryGsTableRetained"))
            .toBool()
        );
    QVERIFY(
        gsLeftMetrics.value(QStringLiteral("staffDirectoryGsOperationRetained"))
            .toBool()
        );
    const QJsonObject gsReentryMetrics =
        gsReentered.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        gsReentryMetrics.value(QStringLiteral("staffDirectoryGsReentryCount"))
            .toInt(),
        1
        );
    const QJsonObject gsReleasedMetrics =
        gsReleased.value(QStringLiteral("metrics")).toObject();
    for (const QString& key : {
             QStringLiteral("staffDirectoryNativeOperationRetained"),
             QStringLiteral("staffDirectoryGsOperationRetained")
         })
    {
        QVERIFY2(
            !gsReleasedMetrics.value(key).toBool(),
            qPrintable(
                QStringLiteral("Staff Directory operation remained retained: %1")
                    .arg(key)
                )
            );
    }
    for (const QString& key : {
             QStringLiteral("staffDirectoryNativeTableRetained"),
             QStringLiteral("staffDirectoryGsTableRetained")
         })
    {
        QVERIFY2(
            gsReleasedMetrics.value(key).toBool(),
            qPrintable(
                QStringLiteral("Staff Directory table was unexpectedly released: %1")
                    .arg(key)
                )
            );
    }
    QCOMPARE(
        gsReleasedMetrics.value(QStringLiteral("staffDirectoryOperationsStarted"))
            .toDouble(),
        2.0
        );
    QCOMPARE(
        gsReleasedMetrics.value(QStringLiteral("staffDirectoryPagesPrepared"))
            .toDouble(),
        2.0
        );
    QCOMPARE(
        gsReleasedMetrics.value(QStringLiteral("staffDirectoryRefreshes"))
            .toDouble(),
        4.0
        );
    QCOMPARE(
        gsReleasedMetrics.value(QStringLiteral("staffDirectoryLeaves"))
            .toDouble(),
        2.0
        );
    QCOMPARE(
        gsReleasedMetrics.value(QStringLiteral("staffDirectoryReentries"))
            .toDouble(),
        2.0
        );
    QCOMPARE(
        gsReleasedMetrics.value(QStringLiteral("staffDirectoryOperationsFailed"))
            .toDouble(),
        0.0
        );
    QCOMPARE(
        gsReleasedMetrics.value(QStringLiteral("staffDirectoryOperationsReleased"))
            .toDouble(),
        2.0
        );

    for (const QString& imageName : {
             QStringLiteral("staff-directory-native-english-teachers.png"),
             QStringLiteral("staff-directory-gs-team.png")
         })
    {
        QVERIFY2(
            QFileInfo(
                QDir(outputRoot).filePath(imageName)
                ).size() > 0,
            qPrintable(
                QStringLiteral("Expected Staff Directory capture is missing: %1")
                    .arg(imageName)
                )
            );
    }

    QJsonObject manifest{
        {QStringLiteral("fixture"), QStringLiteral("large_startup.sql")},
        {
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_staff_directory")
        },
        {
            QStringLiteral("scenario"),
            QStringLiteral(
                "96-entry Native English Teacher and GS Team directory load, refresh, leave, re-entry, and retention"
                )
        },
        {QStringLiteral("classCount"), 96},
        {
            QStringLiteral("nativeEntryCount"),
            LargeStaffDirectoryEntryCount
        },
        {
            QStringLiteral("gsEntryCount"),
            LargeStaffDirectoryEntryCount
        },
        {
            QStringLiteral("nativeCellItemCount"),
            LargeStaffDirectoryEntryCount * LargeStaffDirectoryNativeColumnCount
        },
        {
            QStringLiteral("gsCellItemCount"),
            LargeStaffDirectoryEntryCount * LargeStaffDirectoryGsColumnCount
        },
        {QStringLiteral("processFinished"), finished},
        {
            QStringLiteral("exitStatus"),
            process.exitStatus() == QProcess::NormalExit
                ? QStringLiteral("normal")
                : QStringLiteral("crash")
        },
        {QStringLiteral("exitCode"), process.exitCode()},
        {QStringLiteral("timedOut"), !finished},
        {QStringLiteral("traceLineCount"), traceLines.size()},
        {QStringLiteral("tracePath"), QStringLiteral("workflow-trace.txt")},
        {
            QStringLiteral("metricsPath"),
            QStringLiteral("large-staff-directory-workflow.json")
        },
        {QStringLiteral("stdoutPath"), QStringLiteral("process-stdout.txt")},
        {QStringLiteral("stderrPath"), QStringLiteral("process-stderr.txt")},
        {
            QStringLiteral("peakMemory"),
            report.value(QStringLiteral("peakMemory"))
        },
        {
            QStringLiteral("lastCheckpoint"),
            report.value(QStringLiteral("checkpoints")).toArray().isEmpty()
                ? QJsonObject{}
                : report.value(QStringLiteral("checkpoints"))
                    .toArray().last().toObject()
        }
    };
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
}

void StartupPerformanceTests::capturesLargeSubPrepOutputBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-sub-prep-output.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        prepareLargeSubPrepOutputFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral("Unable to create large Sub Prep output reference root: %1")
                .arg(outputRoot)
            )
        );
    const QString targetRoot =
        QDir(outputRoot).filePath(QStringLiteral("output-target"));
    if (QDir(targetRoot).exists())
    {
        QVERIFY2(
            QDir(targetRoot).removeRecursively(),
            qPrintable(
                QStringLiteral("Unable to clear the prior output target: %1")
                    .arg(targetRoot)
                )
            );
    }

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-sub-prep-output-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    for (const QString& fileName : {
             QStringLiteral("manifest.json"),
             QStringLiteral("process-stdout.txt"),
             QStringLiteral("process-stderr.txt"),
             QStringLiteral("generated-large-sub-prep-output.tps")
         })
    {
        const QString path = QDir(outputRoot).filePath(fileName);
        if (QFileInfo::exists(path))
        {
            QVERIFY(QFile::remove(path));
        }
    }
    for (const QString& path : {metricsPath, tracePath})
    {
        if (QFileInfo::exists(path))
        {
            QVERIFY(QFile::remove(path));
        }
    }

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_SUB_PREP_OUTPUT_TARGET_ROOT"),
        targetRoot
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-sub-prep-output-lifecycle"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );

    const bool finished =
        process.waitForFinished(SubPrepOutputTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(SubPrepOutputTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(finished, qPrintable(processOutput(process)));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    QByteArray traceContents;
    QFile traceFile(tracePath);
    QVERIFY2(
        traceFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(traceFile.errorString())
        );
    traceContents = traceFile.readAll();
    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString& expectedTrace : {
             QStringLiteral("start sub-prep"),
             QStringLiteral("sub-prep-lifecycle-complete"),
             QStringLiteral("sub-prep-output-operation-start"),
             QStringLiteral("sub-prep-output-validation-error"),
             QStringLiteral("sub-prep-output-generated"),
             QStringLiteral("sub-prep-output-operation-released"),
             QStringLiteral("complete")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Sub Prep output route did not record '%1'."
                    )
                    .arg(expectedTrace)
                )
            );
    }

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument metricsDocument =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError,
        qPrintable(parseError.errorString())
        );
    QVERIFY(metricsDocument.isObject());
    const QJsonObject report = metricsDocument.object();

    QHash<QString, QJsonObject> checkpoints;
    for (const QJsonValue& value : report
             .value(QStringLiteral("checkpoints"))
             .toArray())
    {
        const QJsonObject checkpoint = value.toObject();
        checkpoints.insert(
            checkpoint.value(QStringLiteral("name")).toString(),
            checkpoint
            );
    }
    for (const QString& checkpointName : {
             QStringLiteral("sub-prep-output-operation-start"),
             QStringLiteral("sub-prep-output-validation-error"),
             QStringLiteral("sub-prep-output-generated"),
             QStringLiteral("sub-prep-output-operation-released"),
             QStringLiteral("workflow-complete"),
             QStringLiteral("settled-1s")
         })
    {
        QVERIFY2(
            checkpoints.contains(checkpointName),
            qPrintable(
                QStringLiteral("Missing output checkpoint: %1")
                    .arg(checkpointName)
                )
            );
        QVERIFY(
            checkpoints.value(checkpointName)
                .value(QStringLiteral("memory"))
                .toObject()
                .value(QStringLiteral("available"))
                .toBool()
            );
    }

    const QJsonObject outputStartMetrics =
        checkpoints.value(QStringLiteral("sub-prep-output-operation-start"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject validationCheckpoint =
        checkpoints.value(QStringLiteral("sub-prep-output-validation-error"));
    const QJsonObject outputGeneratedMetrics =
        checkpoints.value(QStringLiteral("sub-prep-output-generated"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject outputReleasedMetrics =
        checkpoints.value(QStringLiteral("sub-prep-output-operation-released"))
            .value(QStringLiteral("metrics"))
            .toObject();
    const QJsonObject settledMetrics =
        checkpoints.value(QStringLiteral("settled-1s"))
            .value(QStringLiteral("metrics"))
            .toObject();

    QCOMPARE(
        outputStartMetrics.value(QStringLiteral("subPrepOutputOperationsStarted"))
            .toDouble(),
        1.0
        );
    QVERIFY(
        outputStartMetrics.value(QStringLiteral("subPrepOutputOperationRetained"))
            .toBool()
        );
    QCOMPARE(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputOperationsGenerated"))
            .toDouble(),
        1.0
        );
    QVERIFY(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputOperationRetained"))
            .toBool()
        );
    QVERIFY(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputDocumentsRetained"))
            .toBool()
        );
    QVERIFY(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputPdfCount"))
            .toInt() >= 2
        );
    QVERIFY(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputPageCount"))
            .toInt() > 0
        );
    QVERIFY(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputPdfBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        outputGeneratedMetrics
            .value(QStringLiteral("subPrepOutputDecodedFirstPageBytes"))
            .toDouble() > 0
        );
    QCOMPARE(
        outputReleasedMetrics.value(QStringLiteral("subPrepOutputOperationsFailed"))
            .toDouble(),
        0.0
        );
    QCOMPARE(
        outputReleasedMetrics.value(QStringLiteral("subPrepOutputOperationsReleased"))
            .toDouble(),
        1.0
        );
    QVERIFY(
        !outputReleasedMetrics.value(QStringLiteral("subPrepOutputOperationRetained"))
            .toBool()
        );
    QVERIFY(
        !outputReleasedMetrics.value(QStringLiteral("subPrepOutputDocumentsRetained"))
            .toBool()
        );
    QCOMPARE(
        settledMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        0
        );
    QVERIFY(
        validationCheckpoint.value(QStringLiteral("detail"))
            .toString()
            .contains(QStringLiteral("okEnabled=false"))
        );
    QVERIFY(
        validationCheckpoint.value(QStringLiteral("detail"))
            .toString()
            .contains(QStringLiteral("captured=true"))
        );

    QStringList pdfPaths;
    QDirIterator pdfIterator(
        targetRoot,
        {QStringLiteral("*.pdf")},
        QDir::Files,
        QDirIterator::Subdirectories
        );
    while (pdfIterator.hasNext())
    {
        pdfPaths.append(pdfIterator.next());
    }
    std::sort(pdfPaths.begin(), pdfPaths.end());
    QVERIFY(pdfPaths.size() >= 2);

    int pageCount = 0;
    qint64 pdfBytes = 0;
    for (const QString& pdfPath : pdfPaths)
    {
        QPdfDocument document;
        QCOMPARE(document.load(pdfPath), QPdfDocument::Error::None);
        QCOMPARE(document.status(), QPdfDocument::Status::Ready);
        QVERIFY(document.pageCount() > 0);
        pageCount += document.pageCount();
        pdfBytes += QFileInfo(pdfPath).size();
    }
    QCOMPARE(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputPdfCount"))
            .toInt(),
        pdfPaths.size()
        );
    QCOMPARE(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputPageCount"))
            .toInt(),
        pageCount
        );
    QCOMPARE(
        outputGeneratedMetrics.value(QStringLiteral("subPrepOutputPdfBytes"))
            .toDouble(),
        static_cast<double>(pdfBytes)
        );

    const QString dialogCapturePath =
        QDir(targetRoot).filePath(
            QStringLiteral("sub-prep-output-dialog.png")
            );
    const QImage dialogCapture(dialogCapturePath);
    QVERIFY(!dialogCapture.isNull());
    QVERIFY(QFileInfo(dialogCapturePath).size() > 0);

    const QString validationCapturePath =
        QDir(targetRoot).filePath(
            QStringLiteral("sub-prep-output-validation-error.png")
            );
    const QImage validationCapture(validationCapturePath);
    QVERIFY(!validationCapture.isNull());
    QVERIFY(QFileInfo(validationCapturePath).size() > 0);

    int firstPageCaptureCount = 0;
    QDirIterator imageIterator(
        targetRoot,
        {QStringLiteral("generated-output-*-first-page.png")},
        QDir::Files,
        QDirIterator::Subdirectories
        );
    while (imageIterator.hasNext())
    {
        const QString imagePath = imageIterator.next();
        const QImage image(imagePath);
        QVERIFY(!image.isNull());
        QVERIFY(QFileInfo(imagePath).size() > 0);
        ++firstPageCaptureCount;
    }
    QCOMPARE(firstPageCaptureCount, pdfPaths.size());

    const QString retainedFixturePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-sub-prep-output.tps")
            );
    if (QFileInfo::exists(retainedFixturePath))
    {
        QVERIFY(QFile::remove(retainedFixturePath));
    }
    QVERIFY2(
        QFile::copy(fixturePath, retainedFixturePath),
        qPrintable(
            QStringLiteral("Unable to retain the generated fixture: %1")
                .arg(retainedFixturePath)
            )
        );

    const QJsonObject peakMemory =
        report.value(QStringLiteral("peakMemory")).toObject();
    QVERIFY(peakMemory.value(QStringLiteral("available")).toBool());
    const QJsonObject manifest{
        {QStringLiteral("fixture"), QStringLiteral("large_startup.sql")},
        {
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_sub_prep_output")
        },
        {
            QStringLiteral("scenario"),
            QStringLiteral(
                "96-class Sub Prep generation dialog, package PDFs, first-page decoding, and release"
                )
        },
        {QStringLiteral("pdfCount"), pdfPaths.size()},
        {QStringLiteral("pageCount"), pageCount},
        {QStringLiteral("pdfBytes"), static_cast<double>(pdfBytes)},
        {QStringLiteral("firstPageCaptureCount"), firstPageCaptureCount},
        {QStringLiteral("validationErrorVisualReference"), true},
        {QStringLiteral("processFinished"), finished},
        {QStringLiteral("exitStatus"), QStringLiteral("normal")},
        {QStringLiteral("exitCode"), process.exitCode()},
        {QStringLiteral("timedOut"), !finished},
        {QStringLiteral("traceLineCount"), traceLines.size()},
        {
            QStringLiteral("outputTarget"),
            QStringLiteral("output-target")
        },
        {
            QStringLiteral("workflowCompleteElapsedMs"),
            checkpoints.value(QStringLiteral("workflow-complete"))
                .value(QStringLiteral("elapsedMs"))
        },
        {
            QStringLiteral("settledElapsedMs"),
            checkpoints.value(QStringLiteral("settled-1s"))
                .value(QStringLiteral("elapsedMs"))
        },
        {QStringLiteral("peakMemory"), peakMemory},
        {
            QStringLiteral("tracePath"),
            QStringLiteral("workflow-trace.txt")
        },
        {
            QStringLiteral("metricsPath"),
            QStringLiteral("large-sub-prep-output-workflow.json")
        },
        {QStringLiteral("stdoutPath"), QStringLiteral("process-stdout.txt")},
        {QStringLiteral("stderrPath"), QStringLiteral("process-stderr.txt")}
    };
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
}

void StartupPerformanceTests::capturesLargeSubPrepVisualStatesWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_SUB_PREP_VISUAL_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_SUB_PREP_VISUAL_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-sub-prep-visual.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    const QString emptyFixturePath =
        directory.filePath(QStringLiteral("large-sub-prep-empty.tps"));
    QVERIFY2(
        createLargeStartupFixture(emptyFixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        prepareLargeSubPrepEmptyFixture(emptyFixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large Sub Prep visual reference root: %1"
                )
                .arg(outputRoot)
            )
        );

    const QString retainedFixturePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-sub-prep-visual.tps")
            );
    if (QFileInfo::exists(retainedFixturePath))
    {
        QVERIFY(QFile::remove(retainedFixturePath));
    }
    QVERIFY2(
        QFile::copy(fixturePath, retainedFixturePath),
        "Unable to retain the generated large Sub Prep visual fixture."
        );
    const QString retainedEmptyFixturePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-sub-prep-empty.tps")
            );
    if (QFileInfo::exists(retainedEmptyFixturePath))
    {
        QVERIFY(QFile::remove(retainedEmptyFixturePath));
    }
    QVERIFY2(
        QFile::copy(emptyFixturePath, retainedEmptyFixturePath),
        "Unable to retain the generated empty Sub Prep visual fixture."
        );

    const QProcessEnvironment baseEnvironment =
        [&directory]()
        {
            QProcessEnvironment environment =
                QProcessEnvironment::systemEnvironment();
            environment.insert(
                QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
                directory.filePath(QStringLiteral("settings"))
                );
            environment.insert(
                QStringLiteral("QT_QPA_PLATFORM"),
                QStringLiteral("offscreen")
                );
            return environment;
        }();

    const auto checkpointNamed =
        [](const QJsonObject& report, const QString& name)
        {
            for (const QJsonValue& value : report
                     .value(QStringLiteral("checkpoints"))
                     .toArray())
            {
                const QJsonObject checkpoint = value.toObject();
                if (
                    checkpoint.value(QStringLiteral("name"))
                        .toString()
                    == name
                    )
                {
                    return checkpoint;
                }
            }
            return QJsonObject{};
        };

    QJsonArray variantManifest;
    for (const auto& variant : {
             std::pair<const char*, const char*> {"english", "light"},
             std::pair<const char*, const char*> {"english", "dark"},
             std::pair<const char*, const char*> {"korean", "light"},
             std::pair<const char*, const char*> {"korean", "dark"}
         })
    {
        const QString variantName =
            QStringLiteral("%1-%2")
                .arg(
                    QString::fromLatin1(variant.first),
                    QString::fromLatin1(variant.second)
                    );
        const QString variantRoot =
            QDir(outputRoot).filePath(variantName);
        if (QDir(variantRoot).exists())
        {
            QVERIFY2(
                QDir(variantRoot).removeRecursively(),
                qPrintable(
                    QStringLiteral(
                        "Unable to clear prior Sub Prep visual variant: %1"
                        )
                        .arg(variantRoot)
                    )
                );
        }
        QVERIFY(QDir().mkpath(variantRoot));

        const QString metricsPath =
            QDir(variantRoot).filePath(QStringLiteral("metrics.json"));
        const QString tracePath =
            QDir(variantRoot).filePath(QStringLiteral("workflow-trace.txt"));
        QProcessEnvironment environment = baseEnvironment;
        environment.insert(
            QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
            tracePath
            );

        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(
            appPath,
            {
                QStringLiteral("--startup-performance-test"),
                QStringLiteral("--startup-performance-workflow"),
                QStringLiteral("--startup-performance-sub-prep-visual-states"),
                QStringLiteral("--startup-performance-scenario"),
                QStringLiteral("representative"),
                QStringLiteral("--startup-performance-settle-ms"),
                QStringLiteral("0"),
                QStringLiteral("--startup-performance-output"),
                metricsPath,
                QStringLiteral("--startup-visual-capture-output"),
                variantRoot,
                QStringLiteral("--startup-visual-capture-language"),
                QString::fromLatin1(variant.first),
                QStringLiteral("--startup-visual-capture-theme"),
                QString::fromLatin1(variant.second),
                fixturePath
            }
            );

        QVERIFY2(
            process.waitForStarted(SubPrepVisualTimeoutMs),
            qPrintable(process.errorString())
            );
        bool finished =
            process.waitForFinished(SubPrepVisualTimeoutMs);
        if (!finished)
        {
            process.kill();
            QVERIFY2(
                process.waitForFinished(SubPrepVisualTimeoutMs),
                qPrintable(process.errorString())
                );
        }

        const QByteArray standardOutput = process.readAllStandardOutput();
        const QByteArray standardError = process.readAllStandardError();
        QString diagnosticError;
        QVERIFY2(
            writeDiagnosticFile(
                QDir(variantRoot).filePath(QStringLiteral("process-stdout.txt")),
                standardOutput,
                &diagnosticError
                ),
            qPrintable(diagnosticError)
            );
        QVERIFY2(
            writeDiagnosticFile(
                QDir(variantRoot).filePath(QStringLiteral("process-stderr.txt")),
                standardError,
                &diagnosticError
                ),
            qPrintable(diagnosticError)
            );
        QVERIFY2(
            finished,
            qPrintable(
                QStringLiteral(
                    "Sub Prep visual variant %1 timed out. Diagnostics are under %2."
                    )
                    .arg(variantName, variantRoot)
                )
            );
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(
            process.exitCode() == 0,
            qPrintable(
                QStringLiteral(
                    "Sub Prep visual variant %1 exited with code %2.\nstdout:\n%3\nstderr:\n%4"
                    )
                    .arg(
                        variantName,
                        QString::number(process.exitCode()),
                        QString::fromLocal8Bit(standardOutput),
                        QString::fromLocal8Bit(standardError)
                        )
                )
            );

        for (const QString& captureName : {
                 QStringLiteral("startup-complete.png"),
                 QStringLiteral("sub-prep-editing-read-only.png"),
                 QStringLiteral("sub-prep-selected.png"),
                 QStringLiteral("sub-prep-changed-selection.png")
             })
        {
            const QString capturePath =
                QDir(variantRoot).filePath(captureName);
            const QImage image(capturePath);
            QVERIFY2(
                !image.isNull() && image.width() > 0 && image.height() > 0,
                qPrintable(
                    QStringLiteral(
                        "Missing Sub Prep visual capture %1 for %2."
                        )
                        .arg(capturePath, variantName)
                    )
                );
            QVERIFY(QFileInfo(capturePath).size() > 0);
        }

        QFile metricsFile(metricsPath);
        QVERIFY2(
            metricsFile.open(QIODevice::ReadOnly),
            qPrintable(metricsFile.errorString())
            );
        QJsonParseError parseError;
        const QJsonDocument metricsDocument =
            QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
        QVERIFY2(
            parseError.error == QJsonParseError::NoError
                && metricsDocument.isObject(),
            qPrintable(parseError.errorString())
            );
        const QJsonObject report = metricsDocument.object();
        const qint64 peakWorkingSetBytes = static_cast<qint64>(
            report.value(QStringLiteral("peakMemory"))
                .toObject()
                .value(QStringLiteral("peakWorkingSetBytes"))
                .toDouble()
            );
        const bool withinFinalNormalWorkingSetTarget =
            peakWorkingSetBytes < Phase0FinalNormalWorkingSetTargetBytes;
        const bool withinTransientDiagnosticCeiling =
            peakWorkingSetBytes < Phase0TransientDiagnosticCeilingBytes;
        QVERIFY2(
            withinTransientDiagnosticCeiling,
            qPrintable(
                QStringLiteral(
                    "Legacy Sub Prep visual route exceeded the Phase 0 transient diagnostic ceiling: %1 bytes."
                    )
                    .arg(peakWorkingSetBytes)
                )
            );
        QVERIFY(
            report.value(QStringLiteral("workflow"))
                .toObject()
                .value(QStringLiteral("enabled"))
                .toBool()
            );
        QVERIFY(
            checkpointNamed(
                report,
                QStringLiteral("workflow-page-failed")
                ).isEmpty()
            );

        const QJsonObject selectedCheckpoint =
            checkpointNamed(
                report,
                QStringLiteral("sub-prep-visual-selected")
                );
        const QJsonObject editingReadOnlyCheckpoint =
            checkpointNamed(
                report,
                QStringLiteral("sub-prep-visual-editing-read-only")
                );
        const QJsonObject changedCheckpoint =
            checkpointNamed(
                report,
                QStringLiteral("sub-prep-visual-changed-selection")
                );
        const QJsonObject completeCheckpoint =
            checkpointNamed(
                report,
                QStringLiteral("sub-prep-visual-states-complete")
                );
        for (const QJsonObject& checkpoint : {
                 editingReadOnlyCheckpoint,
                 selectedCheckpoint,
                 changedCheckpoint,
                 completeCheckpoint
             })
        {
            QVERIFY(!checkpoint.isEmpty());
            QVERIFY(
                checkpoint.value(QStringLiteral("memory"))
                    .toObject()
                    .value(QStringLiteral("available"))
                .toBool()
            );
        }
        QVERIFY(
            editingReadOnlyCheckpoint.value(QStringLiteral("detail"))
                .toString()
                .contains(QStringLiteral("editableTextEdits=4"))
        );
        QVERIFY(
            editingReadOnlyCheckpoint.value(QStringLiteral("detail"))
                .toString()
                .contains(QStringLiteral("readOnlyLineEdits=6"))
        );
        QVERIFY(
            editingReadOnlyCheckpoint.value(QStringLiteral("detail"))
                .toString()
                .contains(QStringLiteral("contractValid=true"))
        );
        QVERIFY(
            editingReadOnlyCheckpoint.value(QStringLiteral("detail"))
                .toString()
                .contains(QStringLiteral("captured=true"))
        );
        const QJsonObject selectedMetrics =
            selectedCheckpoint.value(QStringLiteral("metrics")).toObject();
        const QJsonObject changedMetrics =
            changedCheckpoint.value(QStringLiteral("metrics")).toObject();
        QVERIFY(
            selectedMetrics
                .value(QStringLiteral("subPrepClassInformationVisibleClassCount"))
                .toInt()
                >= 90
            );
        QVERIFY(
            selectedMetrics.value(QStringLiteral("subPrepSelectedClassId"))
                .toInt()
                > 0
            );
        QVERIFY(
            selectedMetrics.value(QStringLiteral("subPrepClassInformationWidgetCount"))
                .toInt()
                > 0
            );
        QVERIFY(
            changedMetrics
                .value(QStringLiteral("subPrepClassInformationVisibleClassCount"))
                .toInt()
                >= 90
            );
        QVERIFY(
            changedMetrics.value(QStringLiteral("subPrepSelectedClassId"))
                .toInt()
                > 0
            );
        QVERIFY(
            changedMetrics.value(QStringLiteral("subPrepSelectedClassId"))
                .toInt()
                != selectedMetrics.value(QStringLiteral("subPrepSelectedClassId"))
                    .toInt()
            );
        QVERIFY(
            completeCheckpoint.value(QStringLiteral("detail"))
                .toString()
                .contains(QStringLiteral("passed=true"))
            );

        variantManifest.append(
            QJsonObject{
                {QStringLiteral("name"), variantName},
                {QStringLiteral("language"), QString::fromLatin1(variant.first)},
                {QStringLiteral("theme"), QString::fromLatin1(variant.second)},
                {QStringLiteral("outputDirectory"), variantName},
                {QStringLiteral("metricsPath"), variantName + QStringLiteral("/metrics.json")},
                {QStringLiteral("tracePath"), variantName + QStringLiteral("/workflow-trace.txt")},
                {QStringLiteral("peakMemory"), report.value(QStringLiteral("peakMemory"))},
                {QStringLiteral("finalNormalWorkingSetTargetPass"), withinFinalNormalWorkingSetTarget},
                {QStringLiteral("transientDiagnosticCeilingPass"), withinTransientDiagnosticCeiling},
                {QStringLiteral("editingReadOnlyVisualReference"), true}
            }
            );
    }

    const QString emptyRoot = QDir(outputRoot).filePath(QStringLiteral("empty"));
    if (QDir(emptyRoot).exists())
    {
        QVERIFY(QDir(emptyRoot).removeRecursively());
    }
    QVERIFY(QDir().mkpath(emptyRoot));
    const QString emptyMetricsPath =
        QDir(emptyRoot).filePath(QStringLiteral("metrics.json"));
    const QString emptyTracePath =
        QDir(emptyRoot).filePath(QStringLiteral("workflow-trace.txt"));
    QProcessEnvironment emptyEnvironment = baseEnvironment;
    emptyEnvironment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        emptyTracePath
        );

    QProcess emptyProcess;
    emptyProcess.setProcessEnvironment(emptyEnvironment);
    emptyProcess.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-sub-prep-visual-states"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("0"),
            QStringLiteral("--startup-performance-output"),
            emptyMetricsPath,
            QStringLiteral("--startup-visual-capture-output"),
            emptyRoot,
            emptyFixturePath
        }
        );
    QVERIFY2(
        emptyProcess.waitForStarted(SubPrepVisualTimeoutMs),
        qPrintable(emptyProcess.errorString())
        );
    bool emptyFinished =
        emptyProcess.waitForFinished(SubPrepVisualTimeoutMs);
    if (!emptyFinished)
    {
        emptyProcess.kill();
        QVERIFY2(
            emptyProcess.waitForFinished(SubPrepVisualTimeoutMs),
            qPrintable(emptyProcess.errorString())
            );
    }
    const QByteArray emptyStandardOutput =
        emptyProcess.readAllStandardOutput();
    const QByteArray emptyStandardError =
        emptyProcess.readAllStandardError();
    QString emptyDiagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(emptyRoot).filePath(QStringLiteral("process-stdout.txt")),
            emptyStandardOutput,
            &emptyDiagnosticError
            ),
        qPrintable(emptyDiagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(emptyRoot).filePath(QStringLiteral("process-stderr.txt")),
            emptyStandardError,
            &emptyDiagnosticError
            ),
        qPrintable(emptyDiagnosticError)
        );
    QVERIFY2(
        emptyFinished,
        qPrintable(
            QStringLiteral(
                "Sub Prep empty visual state timed out. Diagnostics are under %1."
                )
                .arg(emptyRoot)
            )
        );
    QCOMPARE(emptyProcess.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        emptyProcess.exitCode() == 0,
        qPrintable(
            QStringLiteral(
                "Sub Prep empty visual state exited with code %1.\nstdout:\n%2\nstderr:\n%3"
                )
                .arg(
                    QString::number(emptyProcess.exitCode()),
                    QString::fromLocal8Bit(emptyStandardOutput),
                    QString::fromLocal8Bit(emptyStandardError)
                    )
            )
        );

    const QString emptyCapturePath =
        QDir(emptyRoot).filePath(QStringLiteral("sub-prep-empty.png"));
    const QImage emptyImage(emptyCapturePath);
    QVERIFY(!emptyImage.isNull());
    QVERIFY(emptyImage.width() > 0);
    QVERIFY(emptyImage.height() > 0);
    QVERIFY(QFileInfo(emptyCapturePath).size() > 0);

    QFile emptyMetricsFile(emptyMetricsPath);
    QVERIFY2(
        emptyMetricsFile.open(QIODevice::ReadOnly),
        qPrintable(emptyMetricsFile.errorString())
        );
    QJsonParseError emptyParseError;
    const QJsonDocument emptyDocument =
        QJsonDocument::fromJson(emptyMetricsFile.readAll(), &emptyParseError);
    QVERIFY2(
        emptyParseError.error == QJsonParseError::NoError
            && emptyDocument.isObject(),
        qPrintable(emptyParseError.errorString())
        );
    const QJsonObject emptyReport = emptyDocument.object();
    const qint64 emptyPeakWorkingSetBytes = static_cast<qint64>(
        emptyReport.value(QStringLiteral("peakMemory"))
            .toObject()
            .value(QStringLiteral("peakWorkingSetBytes"))
            .toDouble()
        );
    const bool emptyWithinFinalNormalWorkingSetTarget =
        emptyPeakWorkingSetBytes < Phase0FinalNormalWorkingSetTargetBytes;
    const bool emptyWithinTransientDiagnosticCeiling =
        emptyPeakWorkingSetBytes < Phase0TransientDiagnosticCeilingBytes;
    QVERIFY2(
        emptyWithinTransientDiagnosticCeiling,
        qPrintable(
            QStringLiteral(
                "Legacy Sub Prep empty visual route exceeded the Phase 0 transient diagnostic ceiling: %1 bytes."
                )
                .arg(emptyPeakWorkingSetBytes)
            )
        );
    const QJsonObject emptyCheckpoint =
        checkpointNamed(
            emptyReport,
            QStringLiteral("sub-prep-visual-empty")
            );
    const QJsonObject emptyCompleteCheckpoint =
        checkpointNamed(
            emptyReport,
            QStringLiteral("sub-prep-visual-states-complete")
            );
    QVERIFY(!emptyCheckpoint.isEmpty());
    QVERIFY(!emptyCompleteCheckpoint.isEmpty());
    const QJsonObject emptyMetrics =
        emptyCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        emptyMetrics.value(QStringLiteral("subPrepClassInformationVisibleClassCount"))
            .toInt(),
        0
        );
    QCOMPARE(
        emptyMetrics.value(QStringLiteral("subPrepSelectedClassId")).toInt(),
        -1
        );
    QVERIFY(
        emptyCompleteCheckpoint.value(QStringLiteral("detail"))
            .toString()
            .contains(QStringLiteral("passed=true"))
        );

    const QJsonObject manifest{
        {QStringLiteral("fixture"), QStringLiteral("large_startup.sql")},
        {
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_sub_prep_visual_states")
        },
        {
            QStringLiteral("scenario"),
            QStringLiteral(
                "96-class Sub Prep editing/read-only, selected, changed-selection, empty, and language/theme visual states"
                )
        },
        {QStringLiteral("classCount"), 96},
        {QStringLiteral("teacherCount"), 24},
        {QStringLiteral("rosterCellCount"), 7200},
        {
            QStringLiteral("memoryBudgets"),
            QJsonObject{
                {
                    QStringLiteral("finalNormalWorkingSetTargetBytes"),
                    static_cast<double>(Phase0FinalNormalWorkingSetTargetBytes)
                },
                {
                    QStringLiteral("transientDiagnosticCeilingBytes"),
                    static_cast<double>(Phase0TransientDiagnosticCeilingBytes)
                },
                {QStringLiteral("comparator"), QStringLiteral("strictly-less-than")},
                {
                    QStringLiteral("primaryMetric"),
                    QStringLiteral("windows-working-set")
                },
                {
                    QStringLiteral("phase0Role"),
                    QStringLiteral("baseline-and-trend")
                },
                {
                    QStringLiteral("finalTargetPhase"),
                    QStringLiteral("phase-9")
                }
            }
        },
        {QStringLiteral("variants"), variantManifest},
        {QStringLiteral("editingReadOnlyVisualReference"), true},
        {
            QStringLiteral("emptyState"),
            QJsonObject{
                {QStringLiteral("outputDirectory"), QStringLiteral("empty")},
                {QStringLiteral("metricsPath"), QStringLiteral("empty/metrics.json")},
                {QStringLiteral("tracePath"), QStringLiteral("empty/workflow-trace.txt")},
                {QStringLiteral("peakWorkingSetBytes"), static_cast<double>(emptyPeakWorkingSetBytes)},
                {QStringLiteral("finalNormalWorkingSetTargetPass"), emptyWithinFinalNormalWorkingSetTarget},
                {QStringLiteral("transientDiagnosticCeilingPass"), emptyWithinTransientDiagnosticCeiling}
            }
        },
        {QStringLiteral("fixturePath"), QStringLiteral("generated-large-sub-prep-visual.tps")},
        {QStringLiteral("emptyFixturePath"), QStringLiteral("generated-large-sub-prep-empty.tps")}
    };
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
}

void StartupPerformanceTests::capturesLargeCalendarImportBoundaryWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_CALENDAR_IMPORT_BOUNDARY_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_CALENDAR_IMPORT_BOUNDARY_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString workbookPath =
        directory.filePath(QStringLiteral("large-calendar-import.xlsx"));
    QVERIFY2(
        writeLargeCalendarImportWorkbook(workbookPath),
        "Unable to write the deterministic large Calendar import workbook."
        );

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-calendar-import.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    const QString settingsRoot =
        directory.filePath(QStringLiteral("settings"));
    QVERIFY2(
        writeRepresentativeStartupSettings(settingsRoot),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large Calendar Import reference root: %1"
                ).arg(outputRoot)
            )
        );
    const QString retainedWorkbookPath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-calendar-import.xlsx")
            );
    if (QFileInfo::exists(retainedWorkbookPath))
    {
        QVERIFY(QFile::remove(retainedWorkbookPath));
    }
    QVERIFY2(
        QFile::copy(workbookPath, retainedWorkbookPath),
        "Unable to retain the generated large Calendar import workbook."
        );

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-calendar-import-workflow.json")
            );
    const QString tracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    if (QFileInfo::exists(metricsPath))
    {
        QVERIFY(QFile::remove(metricsPath));
    }
    QFile traceOutput(tracePath);
    QVERIFY2(
        traceOutput.open(
            QIODevice::WriteOnly
            | QIODevice::Truncate
            | QIODevice::Text
            ),
        qPrintable(traceOutput.errorString())
        );
    traceOutput.close();

    const auto workbookData =
        std::make_shared<QByteArray>(largeCalendarImportWorkbookData());
    QTcpServer server;
    QVERIFY2(
        server.listen(QHostAddress::LocalHost),
        qPrintable(server.errorString())
        );
    QObject::connect(
        &server,
        &QTcpServer::newConnection,
        &server,
        [&server, workbookData]()
        {
            while (server.hasPendingConnections())
            {
                QTcpSocket* socket = server.nextPendingConnection();
                QObject::connect(
                    socket,
                    &QTcpSocket::disconnected,
                    socket,
                    &QObject::deleteLater
                    );
                const auto response =
                    std::make_shared<QByteArray>(
                    QByteArrayLiteral(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/vnd.openxmlformats-officedocument.spreadsheetml.sheet\r\n"
                        )
                    + QByteArrayLiteral("Content-Length: ")
                    + QByteArray::number(workbookData->size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
                    + *workbookData
                    );
                const auto responded = std::make_shared<bool>(false);
                QObject::connect(
                    socket,
                    &QTcpSocket::readyRead,
                    socket,
                    [socket, response, responded]()
                    {
                        socket->readAll();
                        if (*responded)
                        {
                            return;
                        }
                        *responded = true;
                        socket->write(*response);
                        socket->flush();
                        socket->disconnectFromHost();
                    }
                    );
            }
        }
        );

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        settingsRoot
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        tracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL"),
        QStringLiteral("http://127.0.0.1:%1/large-calendar.xlsx")
            .arg(server.serverPort())
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_CALENDAR_IMPORT_OUTPUT_DIR"),
        outputRoot
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral(
                "--startup-performance-calendar-import-lifecycle"
                ),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    QElapsedTimer processTimer;
    processTimer.start();
    bool finished = false;
    while (processTimer.elapsed() < StartupTimeoutMs)
    {
        if (process.waitForFinished(25))
        {
            finished = true;
            break;
        }
        QCoreApplication::processEvents(
            QEventLoop::AllEvents,
            5
            );
    }
    if (!finished && process.state() == QProcess::NotRunning)
    {
        finished = true;
    }
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }
    QCoreApplication::processEvents(
        QEventLoop::AllEvents,
        50
        );
    server.close();

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );

    QByteArray traceContents;
    QFile traceFile(tracePath);
    if (traceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        traceContents = traceFile.readAll();
    }
    const QStringList traceLines =
        QString::fromUtf8(traceContents)
            .split(QChar('\n'), Qt::SkipEmptyParts);
    QVERIFY2(
        traceLines.contains(QStringLiteral("start schedule")),
        qPrintable(
            QStringLiteral(
                "The heavy route did not reach the Schedule transition. "
                "stdout/stderr were retained under %1."
                ).arg(outputRoot)
            )
        );
    for (const QString& expectedTrace : {
             QStringLiteral("calendar-import-preferences-start"),
             QStringLiteral("calendar-import-preferences-opened"),
             QStringLiteral("calendar-import-ui-start"),
             QStringLiteral("calendar-import-operation-start"),
             QStringLiteral("calendar-import-response-received"),
             QStringLiteral("calendar-import-workbook-parsed"),
             QStringLiteral("calendar-import-events-prepared"),
             QStringLiteral("calendar-import-existing-events-loaded"),
             QStringLiteral("calendar-import-save-prepared"),
             QStringLiteral("calendar-import-operation-applied"),
             QStringLiteral("calendar-import-finished"),
             QStringLiteral("calendar-import-page-refreshed"),
             QStringLiteral("calendar-import-operation-released"),
             QStringLiteral("calendar-import-dialog-released"),
             QStringLiteral("calendar-import-page-released")
         })
    {
        bool foundTrace = false;
        for (const QString& line : traceLines)
        {
            if (line.startsWith(expectedTrace))
            {
                foundTrace = true;
                break;
            }
        }
        QVERIFY2(
            foundTrace,
            qPrintable(
                QStringLiteral(
                    "The heavy Calendar Import lifecycle did not record '%1'. "
                    "stdout/stderr were retained under %2."
                    )
                    .arg(expectedTrace, outputRoot)
                )
            );
    }

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument metricsDocument =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError
            && metricsDocument.isObject(),
        qPrintable(parseError.errorString())
        );
    const QJsonObject report = metricsDocument.object();
    const auto checkpointNamed =
        [&report](const QString& name)
        {
            for (const QJsonValue& value :
                 report.value(QStringLiteral("checkpoints")).toArray())
            {
                const QJsonObject checkpoint = value.toObject();
                if (checkpoint.value(QStringLiteral("name")).toString() == name)
                {
                    return checkpoint;
                }
            }
            return QJsonObject{};
        };

    const QJsonObject parsedCheckpoint =
        checkpointNamed(QStringLiteral("calendar-import-workbook-parsed"));
    const QJsonObject eventsCheckpoint =
        checkpointNamed(QStringLiteral("calendar-import-events-prepared"));
    const QJsonObject saveCheckpoint =
        checkpointNamed(QStringLiteral("calendar-import-save-prepared"));
    const QJsonObject appliedCheckpoint =
        checkpointNamed(QStringLiteral("calendar-import-operation-applied"));
    const QJsonObject releasedCheckpoint =
        checkpointNamed(QStringLiteral("calendar-import-operation-released"));
    const QJsonObject pageCheckpoint =
        checkpointNamed(QStringLiteral("calendar-import-page-refreshed"));
    const QJsonObject workflowCheckpoint =
        checkpointNamed(QStringLiteral("workflow-complete"));

    QVERIFY(!parsedCheckpoint.isEmpty());
    QVERIFY(!eventsCheckpoint.isEmpty());
    QVERIFY(!saveCheckpoint.isEmpty());
    QVERIFY(!appliedCheckpoint.isEmpty());
    QVERIFY(!releasedCheckpoint.isEmpty());
    QVERIFY(!pageCheckpoint.isEmpty());
    QVERIFY(!workflowCheckpoint.isEmpty());
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    const QJsonObject parsedMetrics =
        parsedCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        parsedMetrics.value(
            QStringLiteral("calendarImportWorkbookSheetCount")
            ).toInt(),
        2
        );
    QVERIFY(
        parsedMetrics.value(
            QStringLiteral("calendarImportWorkbookCellCount")
            ).toInt() > 300
        );
    QVERIFY(
        parsedMetrics.value(
            QStringLiteral("calendarImportWorkbookMergedRangeCount")
            ).toInt() >= 12
        );
    QCOMPARE(
        parsedMetrics.value(
            QStringLiteral("calendarImportWorkbookStyleCount")
            ).toInt(),
        4
        );
    QVERIFY(
        !parsedMetrics.value(
            QStringLiteral("calendarImportRawBytesRetained")
            ).toBool()
        );
    QVERIFY(
        parsedMetrics.value(
            QStringLiteral("calendarImportWorkbookRetained")
            ).toBool()
        );

    const QJsonObject eventsMetrics =
        eventsCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        eventsMetrics.value(
            QStringLiteral("calendarImportParsedEventCount")
            ).toInt() > 100
        );
    QVERIFY(
        eventsMetrics.value(
            QStringLiteral("calendarImportParsedSkippedCount")
            ).toInt() > 0
        );
    QVERIFY(
        eventsMetrics.value(
            QStringLiteral("calendarImportEventsRetained")
            ).toBool()
        );

    const QJsonObject saveMetrics =
        saveCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        saveMetrics.value(
            QStringLiteral("calendarImportExistingEventCount")
            ).toInt() >= 0
        );
    QVERIFY(
        saveMetrics.value(
            QStringLiteral("calendarImportEventsToSaveCount")
            ).toInt() > 100
        );

    const QJsonObject appliedMetrics =
        appliedCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        appliedMetrics.value(
            QStringLiteral("calendarImportSavedEventCount")
            ).toInt() > 100
        );
    QVERIFY(
        appliedMetrics.value(
            QStringLiteral("calendarImportOperationsApplied")
            ).toInt() >= 1
        );
    QVERIFY(
        appliedMetrics.value(
            QStringLiteral("calendarImportOperationRetained")
            ).toBool()
        );

    const QJsonObject releasedMetrics =
        releasedCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        !releasedMetrics.value(
            QStringLiteral("calendarImportRawBytesRetained")
            ).toBool()
        );
    QVERIFY(
        !releasedMetrics.value(
            QStringLiteral("calendarImportWorkbookRetained")
            ).toBool()
        );
    QVERIFY(
        !releasedMetrics.value(
            QStringLiteral("calendarImportEventsRetained")
            ).toBool()
        );
    QVERIFY(
        !releasedMetrics.value(
            QStringLiteral("calendarImportOperationRetained")
            ).toBool()
        );
    QVERIFY(
        releasedMetrics.value(
            QStringLiteral("calendarImportOperationsReleased")
            ).toInt() >= 1
        );

    const QJsonObject pageMetrics =
        pageCheckpoint.value(QStringLiteral("metrics")).toObject();
    QVERIFY(
        pageMetrics.value(
            QStringLiteral("calendarCacheEventCount")
            ).toInt() > 0
        );
    QVERIFY(
        pageMetrics.value(
            QStringLiteral("calendarCacheRetainedRangeCount")
            ).toInt() > 0
        );
    QVERIFY(
        !pageMetrics.value(
            QStringLiteral("calendarCacheLoading")
            ).toBool()
        );

    QJsonObject manifest;
    manifest.insert(QStringLiteral("fixture"), QStringLiteral("large_startup.sql"));
    manifest.insert(
        QStringLiteral("workbookFixture"),
        QStringLiteral("generated large-calendar-import.xlsx")
        );
    manifest.insert(
        QStringLiteral("fixtureScale"),
        QStringLiteral("large_startup_calendar_import_lifecycle")
        );
    manifest.insert(
        QStringLiteral("scenario"),
        QStringLiteral(
            "96-class heavy route, Calendar workbook response, parse, apply, Preferences close, cache refresh, cleanup"
            )
        );
    manifest.insert(QStringLiteral("teacherCount"), 24);
    manifest.insert(QStringLiteral("classCount"), 96);
    manifest.insert(QStringLiteral("workbookBytes"), QFileInfo(workbookPath).size());
    manifest.insert(QStringLiteral("processFinished"), finished);
    manifest.insert(
        QStringLiteral("exitStatus"),
        process.exitStatus() == QProcess::NormalExit
            ? QStringLiteral("normal")
            : QStringLiteral("crash")
        );
    manifest.insert(QStringLiteral("exitCode"), process.exitCode());
    manifest.insert(QStringLiteral("timedOut"), !finished);
    manifest.insert(QStringLiteral("traceLineCount"), traceLines.size());
    manifest.insert(
        QStringLiteral("metricsPath"),
        QStringLiteral("large-calendar-import-workflow.json")
        );
    manifest.insert(
        QStringLiteral("tracePath"),
        QStringLiteral("workflow-trace.txt")
        );
    manifest.insert(
        QStringLiteral("stdoutPath"),
        QStringLiteral("process-stdout.txt")
        );
    manifest.insert(
        QStringLiteral("stderrPath"),
        QStringLiteral("process-stderr.txt")
        );
    manifest.insert(
        QStringLiteral("parsedCheckpointMetrics"),
        parsedCheckpoint.value(QStringLiteral("metrics"))
        );
    manifest.insert(
        QStringLiteral("releasedCheckpointMetrics"),
        releasedCheckpoint.value(QStringLiteral("metrics"))
        );
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
    QVERIFY(manifestFile.flush());
    QVERIFY(manifestFile.error() == QFile::NoError);

    QVERIFY(
        QFileInfo::exists(
            QDir(outputRoot).filePath(
                QStringLiteral("calendar-import-preferences.png")
                )
            )
        );
    QVERIFY(
        QFileInfo::exists(
            QDir(outputRoot).filePath(QStringLiteral("calendar-page.png"))
            )
        );
}

void StartupPerformanceTests::capturesLargePdfViewerVisualStatesWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_PDF_VIEWER_VISUAL_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_PDF_VIEWER_VISUAL_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-pdf-viewer-visual.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    const QString settingsRoot =
        directory.filePath(QStringLiteral("settings"));
    QVERIFY2(
        writeRepresentativeStartupSettings(settingsRoot),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large PDF viewer visual reference root: %1"
                ).arg(outputRoot)
            )
        );
    const QString retainedFixturePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-pdf-viewer-visual.tps")
            );
    if (QFileInfo::exists(retainedFixturePath))
    {
        QVERIFY(QFile::remove(retainedFixturePath));
    }
    QVERIFY2(
        QFile::copy(fixturePath, retainedFixturePath),
        "Unable to retain the generated large PDF viewer visual fixture."
        );

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-pdf-viewer-visual-workflow.json")
            );
    const QString workflowTracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    for (const QString& fileName : {
             QStringLiteral("large-pdf-viewer-visual-workflow.json"),
             QStringLiteral("workflow-trace.txt"),
             QStringLiteral("manifest.json"),
             QStringLiteral("process-stdout.txt"),
             QStringLiteral("process-stderr.txt"),
             QStringLiteral("pdf-catalog.png"),
             QStringLiteral("pdf-opened.png"),
             QStringLiteral("pdf-closed.png"),
             QStringLiteral("pdf-error.png"),
             QStringLiteral("pdf-reopened.png")
         })
    {
        const QString path = QDir(outputRoot).filePath(fileName);
        if (QFileInfo::exists(path))
        {
            QVERIFY(QFile::remove(path));
        }
    }

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        settingsRoot
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        workflowTracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR"),
        outputRoot
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(metricsFile.readAll(), &parseError);
    QVERIFY2(
        parseError.error == QJsonParseError::NoError
            && document.isObject(),
        qPrintable(parseError.errorString())
        );
    const QJsonObject report = document.object();
    const auto checkpointNamed =
        [&report](const QString& name)
        {
            for (
                const QJsonValue& value :
                report.value(QStringLiteral("checkpoints")).toArray()
                )
            {
                const QJsonObject checkpoint = value.toObject();
                if (checkpoint.value(QStringLiteral("name")).toString() == name)
                {
                    return checkpoint;
                }
            }
            return QJsonObject{};
        };

    const QJsonObject catalogCheckpoint =
        checkpointNamed(QStringLiteral("pdf-catalog-ready"));
    const QJsonObject openedCheckpoint =
        checkpointNamed(QStringLiteral("pdf-opened"));
    const QJsonObject closedCheckpoint =
        checkpointNamed(QStringLiteral("pdf-closed"));
    const QJsonObject errorCheckpoint =
        checkpointNamed(QStringLiteral("pdf-error"));
    const QJsonObject reopenedCheckpoint =
        checkpointNamed(QStringLiteral("pdf-reopened"));
    const QJsonObject completeCheckpoint =
        checkpointNamed(QStringLiteral("pdf-workflow-complete"));
    for (const QJsonObject& checkpoint : {
             catalogCheckpoint,
             openedCheckpoint,
             closedCheckpoint,
             errorCheckpoint,
             reopenedCheckpoint,
             completeCheckpoint
         })
    {
        QVERIFY(!checkpoint.isEmpty());
    }

    const QJsonObject catalogMetrics =
        catalogCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        catalogMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        0
        );
    QCOMPARE(
        catalogMetrics.value(QStringLiteral("pdfDocumentsLoaded")).toDouble(),
        0.0
        );
    const QJsonObject openedMetrics =
        openedCheckpoint.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        openedMetrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
        1
        );
    const QJsonObject closedMetrics =
        closedCheckpoint.value(QStringLiteral("metrics")).toObject();
    const QJsonObject errorMetrics =
        errorCheckpoint.value(QStringLiteral("metrics")).toObject();
    for (const QJsonObject& metrics : {closedMetrics, errorMetrics})
    {
        QCOMPARE(
            metrics.value(QStringLiteral("livePdfDocumentCount")).toInt(),
            0
            );
    }
    QVERIFY(
        errorCheckpoint.value(QStringLiteral("detail"))
            .toString()
            .contains(QStringLiteral("activeDocumentCount=0"))
        );
    QCOMPARE(
        reopenedCheckpoint.value(QStringLiteral("metrics"))
            .toObject()
            .value(QStringLiteral("livePdfDocumentCount"))
            .toInt(),
        1
        );
    QCOMPARE(
        completeCheckpoint.value(QStringLiteral("metrics"))
            .toObject()
            .value(QStringLiteral("livePdfDocumentCount"))
            .toInt(),
        0
        );

    for (const QString& fileName : {
             QStringLiteral("pdf-catalog.png"),
             QStringLiteral("pdf-opened.png"),
             QStringLiteral("pdf-closed.png"),
             QStringLiteral("pdf-error.png"),
             QStringLiteral("pdf-reopened.png")
         })
    {
        const QImage image(QDir(outputRoot).filePath(fileName));
        QVERIFY2(
            !image.isNull() && image.width() > 0 && image.height() > 0,
            qPrintable(
                QStringLiteral("Missing large PDF viewer capture: %1")
                    .arg(fileName)
                )
            );
    }

    QJsonObject manifest{
        {QStringLiteral("fixture"), QStringLiteral("large-pdf-viewer-visual.tps")},
        {
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_pdf_viewer_visual_states")
        },
        {
            QStringLiteral("scenario"),
            QStringLiteral(
                "96-class packaged PDF viewer catalog-ready, open, render, close, error, reopen, and release states"
                )
        },
        {QStringLiteral("processFinished"), finished},
        {
            QStringLiteral("exitStatus"),
            process.exitStatus() == QProcess::NormalExit
                ? QStringLiteral("normal")
                : QStringLiteral("crash")
        },
        {QStringLiteral("exitCode"), process.exitCode()},
        {QStringLiteral("metricsPath"), QStringLiteral("large-pdf-viewer-visual-workflow.json")},
        {QStringLiteral("workflowTracePath"), QStringLiteral("workflow-trace.txt")},
        {
            QStringLiteral("captureNames"),
            QJsonArray{
                QStringLiteral("pdf-catalog.png"),
                QStringLiteral("pdf-opened.png"),
                QStringLiteral("pdf-closed.png"),
                QStringLiteral("pdf-error.png"),
                QStringLiteral("pdf-reopened.png")
            }
        },
        {QStringLiteral("peakMemory"), report.value(QStringLiteral("peakMemory"))},
        {
            QStringLiteral("workflowCompleteElapsedMs"),
            completeCheckpoint.value(QStringLiteral("elapsedMs"))
        }
    };
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
}

void StartupPerformanceTests::capturesLargeResourceTraceWhenConfigured()
{
    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_LARGE_RESOURCE_TRACE_REFERENCE_DIR"
            ).trimmed();
    if (configuredOutputRoot.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_LARGE_RESOURCE_TRACE_REFERENCE_DIR to run the heavy route."
            );
    }

    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");
    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("large-resource-trace.tps"));
    QString fixtureError;
    QVERIFY2(
        createLargeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    const QString settingsRoot =
        directory.filePath(QStringLiteral("settings"));
    QVERIFY2(
        writeRepresentativeStartupSettings(settingsRoot),
        "Unable to write deterministic large-workspace settings."
        );

    const QString outputRoot =
        QFileInfo(configuredOutputRoot).absoluteFilePath();
    QVERIFY2(
        QDir().mkpath(outputRoot),
        qPrintable(
            QStringLiteral(
                "Unable to create large resource trace reference root: %1"
                ).arg(outputRoot)
            )
        );

    const QString retainedFixturePath =
        QDir(outputRoot).filePath(
            QStringLiteral("generated-large-resource-trace.tps")
            );
    if (QFileInfo::exists(retainedFixturePath))
    {
        QVERIFY(QFile::remove(retainedFixturePath));
    }
    QVERIFY2(
        QFile::copy(fixturePath, retainedFixturePath),
        "Unable to retain the generated large resource trace fixture."
        );

    const QString metricsPath =
        QDir(outputRoot).filePath(
            QStringLiteral("large-resource-trace-workflow.json")
            );
    const QString workflowTracePath =
        QDir(outputRoot).filePath(QStringLiteral("workflow-trace.txt"));
    const QString resourceTracePath =
        QDir(outputRoot).filePath(QStringLiteral("resource-trace.json"));
    for (const QString& fileName : {
             QStringLiteral("large-resource-trace-workflow.json"),
             QStringLiteral("workflow-trace.txt"),
             QStringLiteral("resource-trace.json"),
             QStringLiteral("manifest.json"),
             QStringLiteral("process-stdout.txt"),
             QStringLiteral("process-stderr.txt")
         })
    {
        const QString path = QDir(outputRoot).filePath(fileName);
        if (QFileInfo::exists(path))
        {
            QVERIFY(QFile::remove(path));
        }
    }

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        settingsRoot
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"),
        workflowTracePath
        );
    environment.insert(
        QStringLiteral("CLASSMNGR_STARTUP_RESOURCE_TRACE_PATH"),
        resourceTracePath
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );
    process.setProcessEnvironment(environment);
    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-workflow"),
            QStringLiteral("--startup-performance-resource-trace"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("1000"),
            QStringLiteral("--startup-performance-output"),
            metricsPath,
            fixturePath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );
    const bool finished =
        process.waitForFinished(StartupTimeoutMs);
    if (!finished)
    {
        process.kill();
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
    }

    const QByteArray standardOutput = process.readAllStandardOutput();
    const QByteArray standardError = process.readAllStandardError();
    QString diagnosticError;
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stdout.txt")),
            standardOutput,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY2(
        writeDiagnosticFile(
            QDir(outputRoot).filePath(QStringLiteral("process-stderr.txt")),
            standardError,
            &diagnosticError
            ),
        qPrintable(diagnosticError)
        );
    QVERIFY(finished);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    QVERIFY(QFileInfo::exists(metricsPath));
    QVERIFY(QFileInfo::exists(resourceTracePath));

    QFile metricsFile(metricsPath);
    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly),
        qPrintable(metricsFile.errorString())
        );
    QJsonParseError metricsParseError;
    const QJsonDocument metricsDocument =
        QJsonDocument::fromJson(metricsFile.readAll(), &metricsParseError);
    QVERIFY2(
        metricsParseError.error == QJsonParseError::NoError
            && metricsDocument.isObject(),
        qPrintable(metricsParseError.errorString())
    );
    const QJsonObject metrics = metricsDocument.object();
    const auto checkpointNamed =
        [&metrics](const QString& name)
        {
            for (
                const QJsonValue& value :
                metrics.value(QStringLiteral("checkpoints")).toArray()
                )
            {
                const QJsonObject checkpoint = value.toObject();
                if (checkpoint.value(QStringLiteral("name")).toString() == name)
                {
                    return checkpoint;
                }
            }
            return QJsonObject{};
        };
    const QJsonObject resourceTraceStart =
        checkpointNamed(
            QStringLiteral("resource-trace-start")
            );
    const QJsonObject resourceTraceComplete =
        checkpointNamed(
            QStringLiteral("resource-trace-complete")
            );
    QVERIFY(!resourceTraceStart.isEmpty());
    QVERIFY(!resourceTraceComplete.isEmpty());
    QVERIFY(
        resourceTraceComplete.value(QStringLiteral("detail"))
            .toString()
            .contains(QStringLiteral("passed=true"))
        );

    QFile traceFile(resourceTracePath);
    QVERIFY2(
        traceFile.open(QIODevice::ReadOnly),
        qPrintable(traceFile.errorString())
        );
    QJsonParseError traceParseError;
    const QJsonDocument traceDocument =
        QJsonDocument::fromJson(traceFile.readAll(), &traceParseError);
    QVERIFY2(
        traceParseError.error == QJsonParseError::NoError
            && traceDocument.isObject(),
        qPrintable(traceParseError.errorString())
        );
    const QJsonObject trace = traceDocument.object();
    QCOMPARE(
        trace.value(QStringLiteral("schema")).toString(),
        QStringLiteral("classmngr-resource-trace-v1")
        );
    QCOMPARE(
        trace.value(QStringLiteral("scenario")).toString(),
        QStringLiteral("packaged-release-heavy-startup")
        );

    const QJsonObject summary =
        trace.value(QStringLiteral("summary")).toObject();
    QVERIFY(summary.value(QStringLiteral("entryCount")).toInt() > 100);
    QVERIFY(
        summary.value(QStringLiteral("totalInstalledPayloadBytes"))
            .toDouble() > 0
        );
    QVERIFY(
        summary.value(QStringLiteral("totalDecodedResidentBytes"))
            .toDouble() > 0
        );
    QVERIFY(summary.value(QStringLiteral("decodedImageCount")).toInt() > 0);
    QVERIFY(
        summary.value(QStringLiteral("startupNecessaryEntryCount"))
            .toInt() > 0
        );
    QVERIFY(
        summary.value(QStringLiteral("onDemandEntryCount")).toInt() > 0
        );
    QCOMPARE(
        summary.value(QStringLiteral("optionalUnavailablePackCount")).toInt(),
        1
        );
    QCOMPARE(summary.value(QStringLiteral("errorCount")).toInt(), 0);

    const QJsonArray packLifecycles =
        trace.value(QStringLiteral("packLifecycles")).toArray();
    QCOMPARE(packLifecycles.size(), 7);
    for (const QJsonValue& packValue : packLifecycles)
    {
        const QJsonObject pack = packValue.toObject();
        if (pack.value(QStringLiteral("packId")).toString()
            == QStringLiteral("roster-designs"))
        {
            QVERIFY(!pack.value(QStringLiteral("required")).toBool());
            QVERIFY(!pack.value(QStringLiteral("acquired")).toBool());
            QVERIFY(!pack.value(QStringLiteral("error")).toString().isEmpty());
            continue;
        }
        QVERIFY(pack.value(QStringLiteral("required")).toBool());
        QVERIFY(pack.value(QStringLiteral("acquired")).toBool());
        QCOMPARE(
            pack.value(QStringLiteral("mountedAfter")).toBool(),
            pack.value(QStringLiteral("mountedBefore")).toBool()
            );
    }

    bool sawEmbeddedEntry = false;
    bool sawImageEntry = false;
    int pdfEntryCount = 0;
    int pptxEntryCount = 0;
    const QJsonArray entries =
        trace.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& entryValue : entries)
    {
        const QJsonObject entry = entryValue.toObject();
        sawEmbeddedEntry =
            sawEmbeddedEntry
            || entry.value(QStringLiteral("resourcePack")).toString()
                == QStringLiteral("embedded");
        sawImageEntry =
            sawImageEntry
            || entry.value(QStringLiteral("decodedResidentBytes")).toDouble()
                > 0;
        const QString suffix =
            entry.value(QStringLiteral("suffix")).toString();
        if (suffix == QStringLiteral("pdf"))
        {
            ++pdfEntryCount;
            QCOMPARE(
                entry.value(QStringLiteral("decodedResidentBytes")).toDouble(),
                0.0
                );
            QCOMPARE(
                entry.value(QStringLiteral("loadPolicy")).toString(),
                QStringLiteral("on-demand")
                );
        }
        if (suffix == QStringLiteral("pptx"))
        {
            ++pptxEntryCount;
            QCOMPARE(
                entry.value(QStringLiteral("decodedResidentBytes")).toDouble(),
                0.0
                );
            QCOMPARE(
                entry.value(QStringLiteral("loadPolicy")).toString(),
                QStringLiteral("on-demand")
                );
        }
    }
    QVERIFY(sawEmbeddedEntry);
    QVERIFY(sawImageEntry);
    QVERIFY(pdfEntryCount > 0);
    QVERIFY(pptxEntryCount > 0);

    QJsonObject manifest{
        {QStringLiteral("fixture"), QStringLiteral("large-resource-trace.tps")},
        {
            QStringLiteral("fixtureScale"),
            QStringLiteral("large_packaged_resource_trace")
        },
        {
            QStringLiteral("scenario"),
            QStringLiteral(
                "large-workspace full navigation with packaged resource payload, decoded-image, and lease lifecycle trace"
                )
        },
        {QStringLiteral("processFinished"), finished},
        {
            QStringLiteral("exitStatus"),
            process.exitStatus() == QProcess::NormalExit
                ? QStringLiteral("normal")
                : QStringLiteral("crash")
        },
        {QStringLiteral("exitCode"), process.exitCode()},
        {QStringLiteral("metricsPath"), QStringLiteral("large-resource-trace-workflow.json")},
        {QStringLiteral("resourceTracePath"), QStringLiteral("resource-trace.json")},
        {QStringLiteral("workflowTracePath"), QStringLiteral("workflow-trace.txt")},
        {QStringLiteral("entryCount"), summary.value(QStringLiteral("entryCount"))},
        {
            QStringLiteral("totalInstalledPayloadBytes"),
            summary.value(QStringLiteral("totalInstalledPayloadBytes"))
        },
        {
            QStringLiteral("totalDecodedResidentBytes"),
            summary.value(QStringLiteral("totalDecodedResidentBytes"))
        },
        {QStringLiteral("pdfEntryCount"), pdfEntryCount},
        {QStringLiteral("pptxEntryCount"), pptxEntryCount},
        {QStringLiteral("trace"), trace}
    };
    QFile manifestFile(
        QDir(outputRoot).filePath(QStringLiteral("manifest.json"))
        );
    QVERIFY2(
        manifestFile.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable(manifestFile.errorString())
        );
    QVERIFY(
        manifestFile.write(
            QJsonDocument(manifest).toJson(QJsonDocument::Indented)
            ) > 0
        );
}

void StartupPerformanceTests::capturesVisualLanguageAndThemeVariants()
{
    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );

    for (const auto& variant : {
             std::pair<const char*, const char*>{"english", "light"},
             std::pair<const char*, const char*>{"english", "dark"},
             std::pair<const char*, const char*>{"korean", "light"},
             std::pair<const char*, const char*>{"korean", "dark"}
         })
    {
        const QString variantName =
            QStringLiteral("%1-%2")
                .arg(
                    QString::fromLatin1(variant.first),
                    QString::fromLatin1(variant.second)
                    );
        const QString outputDirectory =
            directory.filePath(
                QStringLiteral("visual/%1").arg(variantName)
                );

        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(
            appPath,
            {
                QStringLiteral("--startup-visual-capture-output"),
                outputDirectory,
                QStringLiteral("--startup-visual-capture-language"),
                QString::fromLatin1(variant.first),
                QStringLiteral("--startup-visual-capture-theme"),
                QString::fromLatin1(variant.second)
            }
            );

        QVERIFY2(
            process.waitForStarted(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(processOutput(process))
            );
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(
            process.exitCode() == 0,
            qPrintable(
                QStringLiteral(
                    "Visual capture variant %1 exited with code %2.\n%3"
                    )
                    .arg(
                        variantName,
                        QString::number(process.exitCode()),
                        processOutput(process)
                        )
                )
            );

        const QString capturePath =
            QDir(outputDirectory).filePath(
                QStringLiteral("startup-complete.png")
                );
        const QImage image(capturePath);
        QVERIFY2(
            !image.isNull(),
            qPrintable(
                QStringLiteral("Unable to read visual capture: %1")
                    .arg(capturePath)
                )
            );
        QVERIFY(image.width() > 0);
        QVERIFY(image.height() > 0);
    }
}

void StartupPerformanceTests::capturesRepresentativeVisualVariants()
{
    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString fixturePath =
        directory.filePath(QStringLiteral("representative-startup.tps"));
    QString fixtureError;
    QVERIFY2(
        createRepresentativeStartupFixture(fixturePath, &fixtureError),
        qPrintable(fixtureError)
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic representative startup settings."
        );

    QTemporaryDir temporaryOutputDirectory;
    QVERIFY(temporaryOutputDirectory.isValid());

    const QString configuredOutputRoot =
        qEnvironmentVariable(
            "CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR"
            ).trimmed();
    const QString outputRoot =
        configuredOutputRoot.isEmpty()
            ? temporaryOutputDirectory.path()
            : configuredOutputRoot;
    QVERIFY(QDir().mkpath(outputRoot));

    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );
    environment.insert(
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("offscreen")
        );

    for (const auto& variant : {
             std::pair<const char*, const char*>{"english", "light"},
             std::pair<const char*, const char*>{"english", "dark"},
             std::pair<const char*, const char*>{"korean", "light"},
             std::pair<const char*, const char*>{"korean", "dark"}
         })
    {
        const QString variantName =
            QStringLiteral("%1-%2")
                .arg(
                    QString::fromLatin1(variant.first),
                    QString::fromLatin1(variant.second)
                    );
        const QString outputDirectory =
            QDir(outputRoot).filePath(
                QStringLiteral("representative/%1").arg(variantName)
                );

        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(
            appPath,
            {
                QStringLiteral("--startup-visual-capture-output"),
                outputDirectory,
                QStringLiteral("--startup-visual-capture-language"),
                QString::fromLatin1(variant.first),
                QStringLiteral("--startup-visual-capture-theme"),
                QString::fromLatin1(variant.second),
                QStringLiteral("--startup-performance-scenario"),
                QStringLiteral("representative"),
                QStringLiteral("--startup-performance-settle-ms"),
                QStringLiteral("0"),
                fixturePath
            }
            );

        QVERIFY2(
            process.waitForStarted(StartupTimeoutMs),
            qPrintable(process.errorString())
            );
        QVERIFY2(
            process.waitForFinished(StartupTimeoutMs),
            qPrintable(processOutput(process))
            );
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(
            process.exitCode() == 0,
            qPrintable(
                QStringLiteral(
                    "Representative visual variant %1 exited with code %2.\n%3"
                    )
                    .arg(
                        variantName,
                        QString::number(process.exitCode()),
                        processOutput(process)
                        )
                )
            );

        const QString capturePath =
            QDir(outputDirectory).filePath(
                QStringLiteral("startup-complete.png")
                );
        const QImage image(capturePath);
        QVERIFY2(
            !image.isNull(),
            qPrintable(
                QStringLiteral("Unable to read visual capture: %1")
                    .arg(capturePath)
                )
            );
        QVERIFY(image.width() > 0);
        QVERIFY(image.height() > 0);
    }
}

QTEST_MAIN(StartupPerformanceTests)

#include "startup_performance_tests.moc"
