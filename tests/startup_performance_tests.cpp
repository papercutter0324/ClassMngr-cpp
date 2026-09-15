#include <QDir>
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
#include <utility>

#include "data/database/database_schema_manager.h"

namespace
{
constexpr int StartupTimeoutMs = 60000;

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
             QStringLiteral("pdf-workflow-start"),
             QStringLiteral("pdf-open-start"),
             QStringLiteral("pdf-opened"),
             QStringLiteral("pdf-rendered"),
             QStringLiteral("pdf-released"),
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
             QStringLiteral("pdf-opened.png"),
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
