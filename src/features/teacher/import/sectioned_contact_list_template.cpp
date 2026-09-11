#include "sectioned_contact_list_template.h"

#include "teacher_import_name_utils.h"

#include <QObject>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace
{
constexpr int CellPositionStride = 1'000;

const QStringList LevelOrder{
    QStringLiteral("Elem Only"),
    QStringLiteral("M1"),
    QStringLiteral("M2"),
    QStringLiteral("M3"),
    QStringLiteral("H1"),
    QStringLiteral("H2")
};

const QStringList AllMarkers{
    QStringLiteral("Elem Only"),
    QStringLiteral("M1"),
    QStringLiteral("M2"),
    QStringLiteral("M3"),
    QStringLiteral("H1"),
    QStringLiteral("H2"),
    QStringLiteral("Ntr"),
    QStringLiteral("CS"),
    QStringLiteral("GS")
};

QString canonicalMarker(const QString& value)
{
    const QString simplified = value.simplified();
    for (const QString& marker : AllMarkers)
    {
        if (simplified.compare(marker, Qt::CaseInsensitive) == 0)
        {
            return marker;
        }
    }
    return {};
}

struct Section
{
    int row = 0;
    QString name;
};

QHash<int, const CalendarImport::Cell*> cellsByPosition(
    const CalendarImport::Worksheet& worksheet
    )
{
    QHash<int, const CalendarImport::Cell*> result;
    for (const CalendarImport::Cell& cell : worksheet.cells)
    {
        result.insert(cell.row * CellPositionStride + cell.column, &cell);
    }
    return result;
}

const CalendarImport::Cell* cellAt(
    const QHash<int, const CalendarImport::Cell*>& cells,
    int row,
    int column
    )
{
    return cells.value(row * CellPositionStride + column, nullptr);
}

QString cellText(
    const QHash<int, const CalendarImport::Cell*>& cells,
    int row,
    int column
    )
{
    const auto* cell = cellAt(cells, row, column);
    return cell ? cell->value.trimmed() : QString();
}

QList<Section> sectionsIn(
    const CalendarImport::Worksheet& worksheet
    )
{
    QList<Section> result;
    for (const CalendarImport::Cell& cell : worksheet.cells)
    {
        if (cell.column != 1)
        {
            continue;
        }
        const QString marker = canonicalMarker(cell.value);
        if (!marker.isEmpty())
        {
            result.append({cell.row, marker});
        }
    }
    std::sort(result.begin(), result.end(), [](const Section& left, const Section& right) {
        return left.row < right.row;
    });
    return result;
}

Result<QDate> sourceDate(const QString& value)
{
    static const QRegularExpression expression(
        QStringLiteral(R"(^\s*(\d{2}|\d{4})[.\-/](\d{1,2})[.\-/](\d{1,2})\s*ver\s*$)"),
        QRegularExpression::CaseInsensitiveOption
        );
    const QRegularExpressionMatch match = expression.match(value);
    if (!match.hasMatch())
    {
        return std::unexpected(
            QObject::tr("Cell A1 must contain a version date such as 26.07.09ver.")
            );
    }

    bool yearOk = false;
    bool monthOk = false;
    bool dayOk = false;
    int year = match.captured(1).toInt(&yearOk);
    const int month = match.captured(2).toInt(&monthOk);
    const int day = match.captured(3).toInt(&dayOk);
    if (match.captured(1).size() == 2)
    {
        year += 2000;
    }
    const QDate result(year, month, day);
    if (!yearOk || !monthOk || !dayOk || !result.isValid())
    {
        return std::unexpected(QObject::tr("The version date in cell A1 is invalid."));
    }
    return result;
}

Result<QString> normalizedBirthday(const QString& value, int row)
{
    if (value.trimmed().isEmpty())
    {
        return QString();
    }

    static const QRegularExpression expression(
        QStringLiteral(R"(^\s*(\d{1,2})[\-/\.](\d{1,2})\s*$)")
        );
    const QRegularExpressionMatch match = expression.match(value);
    if (!match.hasMatch())
    {
        return std::unexpected(
            QObject::tr("Birthday in row %1 must use month/day format.").arg(row)
            );
    }

    const int month = match.captured(1).toInt();
    const int day = match.captured(2).toInt();
    const QDate reference(2000, month, day);
    if (!reference.isValid())
    {
        return std::unexpected(QObject::tr("Birthday in row %1 is invalid.").arg(row));
    }
    return reference.toString(QStringLiteral("MM-dd"));
}

QPair<QString, bool> cleanedKoreanName(const QString& value)
{
    static const QRegularExpression expression(
        QStringLiteral(R"(\s*[_\-(]?\s*(E[456](?:\s*/\s*(?:E?[456]|[456]))*)\s*\)?\s*$)"),
        QRegularExpression::CaseInsensitiveOption
        );
    const QRegularExpressionMatch match = expression.match(value);
    if (!match.hasMatch())
    {
        return {TeacherImportNameUtils::hangulOnly(value), false};
    }
    const QString name =
        TeacherImportNameUtils::hangulOnly(value.left(match.capturedStart()));
    return {name, true};
}

QPair<QString, QString> cleanedStaffName(const QString& value)
{
    static const QRegularExpression expression(
        QStringLiteral(R"((C[1-3]|M[1-3])\s*$)"),
        QRegularExpression::CaseInsensitiveOption
        );
    const QRegularExpressionMatch match = expression.match(value);
    if (!match.hasMatch())
    {
        return {value.simplified(), QString()};
    }

    QString name = value.left(match.capturedStart()).trimmed();
    while (name.endsWith(QLatin1Char('_')) || name.endsWith(QLatin1Char('-')))
    {
        name.chop(1);
        name = name.trimmed();
    }
    return {name, match.captured(1).toUpper()};
}

bool containsHangul(const QString& value)
{
    for (const QChar character : value)
    {
        if (TeacherImportNameUtils::isHangul(character))
        {
            return true;
        }
    }
    return false;
}

bool highlightedName(
    const CalendarImport::Workbook& workbook,
    const CalendarImport::Cell* cell
    )
{
    if (!cell || cell->style < 0 || cell->style >= workbook.styles.size())
    {
        return false;
    }
    const CalendarImport::Style& style = workbook.styles.at(cell->style);
    return style.bold || style.filled;
}

bool filledCell(
    const CalendarImport::Workbook& workbook,
    const CalendarImport::Cell* cell
    )
{
    if (!cell || cell->style < 0 || cell->style >= workbook.styles.size())
    {
        return false;
    }
    return workbook.styles.at(cell->style).filled;
}

}

QString SectionedContactListTemplate::id() const
{
    return QStringLiteral("sectioned-contact-list-v1");
}

QString SectionedContactListTemplate::displayName() const
{
    return QObject::tr("Sectioned Teacher Contact List");
}

bool SectionedContactListTemplate::recognizes(
    const CalendarImport::Workbook& workbook
    ) const
{
    if (workbook.worksheets.isEmpty())
    {
        return false;
    }
    const CalendarImport::Worksheet& worksheet = workbook.worksheets.first();
    const auto cells = cellsByPosition(worksheet);
    return !cellText(cells, 1, 1).isEmpty()
        && !sectionsIn(worksheet).isEmpty();
}

QStringList SectionedContactListTemplate::discoveredSections(
    const CalendarImport::Workbook& workbook
    ) const
{
    QStringList result;
    if (workbook.worksheets.isEmpty())
    {
        return result;
    }
    for (const Section& section : sectionsIn(workbook.worksheets.first()))
    {
        if (!result.contains(section.name))
        {
            result.append(section.name);
        }
    }
    return result;
}

Result<TeacherImportPreview> SectionedContactListTemplate::parse(
    const CalendarImport::Workbook& workbook
    ) const
{
    if (workbook.worksheets.isEmpty())
    {
        return std::unexpected(QObject::tr("The workbook does not contain a worksheet."));
    }

    const CalendarImport::Worksheet& worksheet = workbook.worksheets.first();
    const auto cells = cellsByPosition(worksheet);
    const QList<Section> sections = sectionsIn(worksheet);
    if (sections.isEmpty())
    {
        return std::unexpected(QObject::tr("No supported teacher sections were found."));
    }

    const Result<QDate> parsedDate = sourceDate(cellText(cells, 1, 1));
    if (!parsedDate)
    {
        return std::unexpected(parsedDate.error());
    }

    TeacherImportPreview preview;
    preview.templateId = id();
    preview.templateName = displayName();
    preview.sourceDate = *parsedDate;
    QHash<QString, QList<KoreanTeacherImportCandidate>> koreanByLevel;

    int lastWorksheetRow = 0;
    for (const CalendarImport::Cell& cell : worksheet.cells)
    {
        lastWorksheetRow = std::max(lastWorksheetRow, cell.row);
    }

    for (int sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
    {
        const Section& section = sections.at(sectionIndex);
        const int endRow = sectionIndex + 1 < sections.size()
            ? sections.at(sectionIndex + 1).row - 1
            : lastWorksheetRow;

        for (int row = section.row + 1; row <= endRow; ++row)
        {
            const QString rawName = cellText(cells, row, 3);
            if (rawName.isEmpty())
            {
                continue;
            }

            const Result<QString> birthday =
                normalizedBirthday(cellText(cells, row, 5), row);
            if (!birthday)
            {
                return std::unexpected(birthday.error());
            }

            if (LevelOrder.contains(section.name))
            {
                const auto [name, selectedByDefault] = cleanedKoreanName(rawName);

                Teacher teacher;
                teacher.teacherKr = name;
                teacher.roomNumber = cellText(cells, row, 2);
                teacher.phoneNumber = cellText(cells, row, 4);
                teacher.birthday = *birthday;
                koreanByLevel[section.name].append({teacher, selectedByDefault});
            }
            else if (section.name == QStringLiteral("Ntr"))
            {
                const QString name = rawName.simplified();
                QString position = cellText(cells, row, 4);
                if (position.isEmpty())
                {
                    position = highlightedName(workbook, cellAt(cells, row, 3))
                        ? QStringLiteral("Team Leader")
                        : QStringLiteral("NET");
                }
                preview.nativeEnglishTeachers.append(
                    {-1, name, position, QString(), *birthday, QString()}
                    );
            }
            else if (section.name == QStringLiteral("CS")
                     || section.name == QStringLiteral("GS"))
            {
                const auto [name, position] = cleanedStaffName(rawName);
                const bool korean = containsHangul(name);

                GsTeamMember member;
                if (korean) member.koreanName = name;
                else member.name = name;
                member.position = position == QStringLiteral("M3")
                    && filledCell(workbook, cellAt(cells, row, 3))
                    ? QStringLiteral("Branch Manager")
                    : position;
                member.phoneNumber = cellText(cells, row, 4);
                member.birthday = *birthday;
                preview.gsTeamMembers.append(member);
            }
        }
    }

    for (const QString& level : LevelOrder)
    {
        const auto candidates = koreanByLevel.value(level);
        if (!candidates.isEmpty())
        {
            preview.koreanGroups.append({level, candidates});
        }
    }

    int total = preview.nativeEnglishTeachers.size() + preview.gsTeamMembers.size();
    for (const KoreanTeacherImportGroup& group : preview.koreanGroups)
    {
        total += group.candidates.size();
    }
    if (total == 0)
    {
        return std::unexpected(QObject::tr("The recognized template contains no importable people."));
    }

    return preview;
}
