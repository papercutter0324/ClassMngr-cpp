#include "next/application/speaking_evaluation_ai_comment_prompt.h"
#include "next/platform/speaking_evaluation_qt_text_match_port.h"

#include <QtTest>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using ClassMngr::Next::Application::SpeakingEvaluationTextMatchSpan;
using ClassMngr::Next::Platform::SpeakingEvaluationQtTextMatchPort;

QString fromUtf16(const std::u16string_view value)
{
    return QString::fromUtf16(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::vector<SpeakingEvaluationTextMatchSpan> qtRegexMatches(
    const std::u16string_view text,
    const std::u16string_view literal
    )
{
    std::vector<SpeakingEvaluationTextMatchSpan> result;
    if (literal.empty())
    {
        return result;
    }
    const QRegularExpression expression(
        QRegularExpression::escape(fromUtf16(literal)),
        QRegularExpression::CaseInsensitiveOption
        );
    auto matches = expression.globalMatch(fromUtf16(text));
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        result.push_back({
            static_cast<std::size_t>(match.capturedStart()),
            static_cast<std::size_t>(match.capturedLength())
        });
    }
    return result;
}

bool equalsQtCaseInsensitive(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return fromUtf16(left).compare(fromUtf16(right), Qt::CaseInsensitive) == 0;
}

} // namespace

class NextPlatformSpeakingEvaluationQtTextMatchPortTests final : public QObject
{
    Q_OBJECT

private slots:
    void literalSpansAndEqualityMatchQtCaseInsensitiveSemantics();
    void regexMetacharactersOverlapsAndNoBoundariesStayLiteral();
    void applicationPrivacyUsesPlatformMatcherAndEqualityPort();
};

void NextPlatformSpeakingEvaluationQtTextMatchPortTests::
    literalSpansAndEqualityMatchQtCaseInsensitiveSemantics()
{
    const SpeakingEvaluationQtTextMatchPort matcher;
    const std::vector<std::pair<std::u16string, std::u16string>> cases{
        {u"aLiCe ALICE", u"Alice"},                         // ASCII case
        {u"ΟΣ ος", u"σ"},                                   // Greek sigma
        {u"K k", u"k"},                                     // Kelvin sign
        {u"ſ s", u"s"},                                     // Long s
        {u"ß ss", u"ss"},                                   // No expanding fold
        {u"İ i ı I", u"i"},                                 // Dotted/dotless I
        {u"\U00010400 \U00010428", u"\U00010428"},       // Deseret pair
        {u"é e\u0301", u"é"},                              // No normalization
        {u"a.+[b] A.+[B]", u"A.+[b]"},                     // Metacharacters
        {u"aaaa", u"aa"},                                   // Non-overlapping
        {u"Malice", u"Alice"}                               // No word boundary
    };

    for (const auto& [text, literal] : cases)
    {
        const auto actualSpans = matcher.findLiteralCaseInsensitiveMatches(
            text,
            literal
            );
        QCOMPARE(actualSpans, qtRegexMatches(text, literal));
        QCOMPARE(
            matcher.equalsCaseInsensitive(text, literal),
            equalsQtCaseInsensitive(text, literal)
            );
    }
}

void NextPlatformSpeakingEvaluationQtTextMatchPortTests::
    regexMetacharactersOverlapsAndNoBoundariesStayLiteral()
{
    const SpeakingEvaluationQtTextMatchPort matcher;
    QCOMPARE(
        matcher.findLiteralCaseInsensitiveMatches(u"a.+[b] A.+[B]", u"A.+[b]"),
        (std::vector<SpeakingEvaluationTextMatchSpan>{{0, 6}, {7, 6}})
        );
    QCOMPARE(
        matcher.findLiteralCaseInsensitiveMatches(u"aaaa", u"aa"),
        (std::vector<SpeakingEvaluationTextMatchSpan>{{0, 2}, {2, 2}})
        );
    QCOMPARE(
        matcher.findLiteralCaseInsensitiveMatches(u"Malice", u"Alice"),
        (std::vector<SpeakingEvaluationTextMatchSpan>{{1, 5}})
        );
    QVERIFY(matcher.findLiteralCaseInsensitiveMatches(u"anything", {}).empty());
}

void NextPlatformSpeakingEvaluationQtTextMatchPortTests::
    applicationPrivacyUsesPlatformMatcherAndEqualityPort()
{
    using namespace ClassMngr::Next::Application;
    const SpeakingEvaluationQtTextMatchPort matcher;
    SpeakingEvaluationAiCommentPromptRequest request;
    request.grade = ClassMngr::Next::Domain::CourseGradeBand::E4;
    request.englishName = u"K";
    request.didWell = u"k made progress";
    request.needsImprovement = u"Add details";
    const auto result = buildSpeakingEvaluationAiCommentPrompt(request, matcher);

    QVERIFY(result.valid);
    QVERIFY(result.prompt.find(u"- STD_NAME made progress")
        != std::u16string::npos);
}

QTEST_APPLESS_MAIN(NextPlatformSpeakingEvaluationQtTextMatchPortTests)

#include "next_platform_speaking_evaluation_qt_text_match_port_tests.moc"
