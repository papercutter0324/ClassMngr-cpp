#pragma once

#include "next/application/speaking_evaluation_text_match_port.h"

#include <QRegularExpression>
#include <QString>
#include <Qt>

#include <cstddef>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Platform
{

class SpeakingEvaluationQtTextMatchPort final
    : public Application::SpeakingEvaluationTextMatchPort
{
public:
    [[nodiscard]] std::vector<Application::SpeakingEvaluationTextMatchSpan>
    findLiteralCaseInsensitiveMatches(
        const std::u16string_view text,
        const std::u16string_view literal
        ) const override
    {
        std::vector<Application::SpeakingEvaluationTextMatchSpan> result;
        if (literal.empty())
        {
            return result;
        }

        const QRegularExpression expression(
            QRegularExpression::escape(toQString(literal)),
            QRegularExpression::CaseInsensitiveOption
            );
        auto matches = expression.globalMatch(toQString(text));
        while (matches.hasNext())
        {
            const QRegularExpressionMatch match = matches.next();
            result.push_back({
                Application::SpeakingEvaluationTextMatchStart{
                    static_cast<std::size_t>(match.capturedStart())
                },
                Application::SpeakingEvaluationTextMatchLength{
                    static_cast<std::size_t>(match.capturedLength())
                }
            });
        }
        return result;
    }

    [[nodiscard]] bool equalsCaseInsensitive(
        const std::u16string_view left,
        const std::u16string_view right
        ) const override
    {
        return toQString(left).compare(toQString(right), Qt::CaseInsensitive)
            == 0;
    }

private:
    [[nodiscard]] static QString toQString(const std::u16string_view value)
    {
        return QString::fromUtf16(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }
};

} // namespace ClassMngr::Next::Platform
