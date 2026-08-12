#pragma once

#include <QHash>
#include <QList>
#include <QStringList>

namespace StudentNameUtils
{

QString normalizeKoreanName(const QString& value);
QString baseKoreanName(const QString& value);
QString koreanNameSuffix(const QString& value);
QString namePairKey(const QString& englishName, const QString& koreanName);
QHash<QString, QList<int>> rowsByNamePair(
    const QList<QStringList>& rows,
    int englishColumn,
    int koreanColumn
    );
QList<int> duplicateNameRows(
    const QList<QStringList>& rows,
    int row,
    int englishColumn,
    int koreanColumn
    );
QString suggestedKoreanNameWithSuffix(
    const QList<QStringList>& rows,
    int row,
    int englishColumn,
    int koreanColumn
    );

} // namespace StudentNameUtils
