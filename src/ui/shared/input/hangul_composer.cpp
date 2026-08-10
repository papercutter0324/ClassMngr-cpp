#include "hangul_composer.h"

#include <QStringView>

namespace
{

constexpr char16_t HangulSyllableBase = 0xAC00;
constexpr int MedialCount = 21;
constexpr int FinalCount = 28;

constexpr QStringView Initials =
    u"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ";
constexpr QStringView Medials =
    u"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
constexpr QStringView Finals =
    u" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ";

QChar combinedVowel(
    QChar current,
    QChar next
    )
{
    QString pair;
    pair.reserve(2);
    pair.append(current);
    pair.append(next);

    if (pair == QStringLiteral("ㅗㅏ")) return u'ㅘ';
    if (pair == QStringLiteral("ㅗㅐ")) return u'ㅙ';
    if (pair == QStringLiteral("ㅗㅣ")) return u'ㅚ';
    if (pair == QStringLiteral("ㅘㅐ")) return u'ㅙ';
    if (pair == QStringLiteral("ㅜㅓ")) return u'ㅝ';
    if (pair == QStringLiteral("ㅜㅔ")) return u'ㅞ';
    if (pair == QStringLiteral("ㅜㅣ")) return u'ㅟ';
    if (pair == QStringLiteral("ㅝㅔ")) return u'ㅞ';
    if (pair == QStringLiteral("ㅡㅣ")) return u'ㅢ';

    return {};
}

QChar combinedFinal(
    QChar current,
    QChar next
    )
{
    QString pair;
    pair.reserve(2);
    pair.append(current);
    pair.append(next);

    if (pair == QStringLiteral("ㄱㅅ")) return u'ㄳ';
    if (pair == QStringLiteral("ㄴㅈ")) return u'ㄵ';
    if (pair == QStringLiteral("ㄴㅎ")) return u'ㄶ';
    if (pair == QStringLiteral("ㄹㄱ")) return u'ㄺ';
    if (pair == QStringLiteral("ㄹㅁ")) return u'ㄻ';
    if (pair == QStringLiteral("ㄹㅂ")) return u'ㄼ';
    if (pair == QStringLiteral("ㄹㅅ")) return u'ㄽ';
    if (pair == QStringLiteral("ㄹㅌ")) return u'ㄾ';
    if (pair == QStringLiteral("ㄹㅍ")) return u'ㄿ';
    if (pair == QStringLiteral("ㄹㅎ")) return u'ㅀ';
    if (pair == QStringLiteral("ㅂㅅ")) return u'ㅄ';

    return {};
}

bool isConsonant(
    QChar value
    )
{
    return Initials.indexOf(value) >= 0
        || Finals.indexOf(value) > 0;
}

bool isVowel(
    QChar value
    )
{
    return Medials.indexOf(value) >= 0;
}

bool canBeInitial(
    QChar value
    )
{
    return Initials.indexOf(value) >= 0;
}

bool canBeFinal(
    QChar value
    )
{
    return Finals.indexOf(value) > 0;
}

} // namespace

HangulCompositionResult HangulComposer::input(
    QChar jamo
    )
{
    HangulCompositionResult result;

    if (!isConsonant(jamo) && !isVowel(jamo))
    {
        result.consumed = false;
        result.preedit = preedit();
        return result;
    }

    if (isConsonant(jamo))
    {
        if (m_medial.isNull())
        {
            if (m_initial.isNull())
            {
                m_initial = jamo;
            }
            else
            {
                result.committed = composedText();
                clearState();
                m_initial = jamo;
            }
        }
        else if (m_initial.isNull())
        {
            result.committed = composedText();
            clearState();
            m_initial = jamo;
        }
        else if (m_final.isNull() && canBeFinal(jamo))
        {
            m_final = jamo;
            m_finalInputs = jamo;
        }
        else if (!m_final.isNull())
        {
            const QChar compound = combinedFinal(m_final, jamo);

            if (!compound.isNull())
            {
                m_final = compound;
                m_finalInputs.append(jamo);
            }
            else
            {
                result.committed = composedText();
                clearState();
                m_initial = jamo;
            }
        }
        else
        {
            result.committed = composedText();
            clearState();
            m_initial = jamo;
        }
    }
    else
    {
        if (m_medial.isNull())
        {
            m_medial = jamo;
            m_vowelInputs = jamo;
        }
        else if (m_final.isNull())
        {
            const QChar compound = combinedVowel(m_medial, jamo);

            if (!compound.isNull())
            {
                m_medial = compound;
                m_vowelInputs.append(jamo);
            }
            else
            {
                result.committed = composedText();
                clearState();
                m_medial = jamo;
                m_vowelInputs = jamo;
            }
        }
        else
        {
            const QChar nextInitial = m_finalInputs.back();

            if (m_finalInputs.size() > 1)
            {
                m_finalInputs.chop(1);
                m_final = m_finalInputs.front();
            }
            else
            {
                m_final = {};
                m_finalInputs.clear();
            }

            result.committed = composedText();
            clearState();
            m_initial = canBeInitial(nextInitial)
                ? nextInitial
                : QChar{};
            m_medial = jamo;
            m_vowelInputs = jamo;
        }
    }

    result.preedit = preedit();
    return result;
}

HangulCompositionResult HangulComposer::backspace()
{
    HangulCompositionResult result;

    if (!m_final.isNull())
    {
        m_finalInputs.chop(1);
        m_final = m_finalInputs.isEmpty()
            ? QChar{}
            : m_finalInputs.front();
    }
    else if (!m_medial.isNull())
    {
        m_vowelInputs.chop(1);

        if (m_vowelInputs.isEmpty())
        {
            m_medial = {};
        }
        else
        {
            m_medial = m_vowelInputs.front();

            for (int index = 1; index < m_vowelInputs.size(); ++index)
            {
                m_medial = combinedVowel(
                    m_medial,
                    m_vowelInputs.at(index)
                    );
            }
        }
    }
    else if (!m_initial.isNull())
    {
        m_initial = {};
    }
    else
    {
        result.consumed = false;
    }

    result.preedit = preedit();
    return result;
}

HangulCompositionResult HangulComposer::commit()
{
    HangulCompositionResult result;
    result.committed = composedText();
    clearState();
    return result;
}

void HangulComposer::reset()
{
    clearState();
}

bool HangulComposer::isComposing() const
{
    return !m_initial.isNull()
        || !m_medial.isNull()
        || !m_final.isNull();
}

QString HangulComposer::preedit() const
{
    return composedText();
}

QString HangulComposer::composedText() const
{
    if (m_initial.isNull() || m_medial.isNull())
    {
        QString text;

        if (!m_initial.isNull())
        {
            text.append(m_initial);
        }

        if (!m_medial.isNull())
        {
            text.append(m_medial);
        }

        return text;
    }

    const qsizetype initialIndex = Initials.indexOf(m_initial);
    const qsizetype medialIndex = Medials.indexOf(m_medial);
    const qsizetype finalIndex = m_final.isNull()
        ? 0
        : Finals.indexOf(m_final);

    if (initialIndex < 0 || medialIndex < 0 || finalIndex < 0)
    {
        QString text;
        text.append(m_initial);
        text.append(m_medial);
        text.append(m_final);
        return text;
    }

    const char16_t syllable = static_cast<char16_t>(
        HangulSyllableBase
        + (
            (initialIndex * MedialCount) + medialIndex
            ) * FinalCount
        + finalIndex
        );

    return QString(1, QChar{syllable});
}

void HangulComposer::clearState()
{
    m_initial = {};
    m_medial = {};
    m_final = {};
    m_vowelInputs.clear();
    m_finalInputs.clear();
}
