#pragma once

#include <QChar>
#include <QCoreApplication>
#include <QString>

namespace PdfPrintDialogPrivate
{
constexpr int OptionsPanelWidth = 320;
constexpr int DialogMinimumWidth = 1050;
constexpr int DialogMinimumHeight = 720;
constexpr int PageRangeAll = 0;
constexpr int PageRangeCustom = 1;
constexpr int ColorModeColor = 0;
constexpr int ColorModeBlackAndWhite = 1;

inline QString customPagesSample()
{
    return QCoreApplication::translate(
        "PdfPrintDialog",
        "e.g. 1-3, 6, 9-11"
        );
}

inline bool isAllowedPageRangeCharacter(
    QChar character
    )
{
    return character.isDigit()
        || character.isSpace()
        || character == QLatin1Char(',')
        || character == QLatin1Char('-');
}
}
