#pragma once

#include <QHash>
#include <QList>
#include <QStringList>

namespace StudentNameUtils
{

enum class ValidationIssue
{
    EnglishTooLong,
    EnglishContainsNonAscii,
    KoreanTooShort,
    KoreanUnusualLength,
    KoreanTooLong
};

QString normalizeEnglishName(const QString& value);
QString normalizeKoreanName(const QString& value);
QString baseKoreanName(const QString& value);
QString koreanNameSuffix(const QString& value);
QString namePairKey(const QString& englishName, const QString& koreanName);
QHash<QString, QList<int>> rowsByNamePair(
    const QList<QStringList>& rows,
    int englishColumn,
    int koreanColumn
    );
QList<ValidationIssue> validateEnglishName(
    const QString& value,
    qsizetype maximumLength = 20
    );
QList<ValidationIssue> validateKoreanName(const QString& value);
QHash<QString, QList<int>> duplicateRowsByNamePair(
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
