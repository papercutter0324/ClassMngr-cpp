#pragma once

#include <QChar>
#include <QString>

struct HangulCompositionResult
{
    QString committed;
    QString preedit;
    bool consumed = true;
};

class HangulComposer
{
public:
    [[nodiscard]] HangulCompositionResult input(
        QChar jamo
        );

    [[nodiscard]] HangulCompositionResult backspace();
    [[nodiscard]] HangulCompositionResult commit();

    void reset();

    [[nodiscard]] bool isComposing() const;
    [[nodiscard]] QString preedit() const;

private:
    [[nodiscard]] QString composedText() const;
    void clearState();

private:
    QChar m_initial;
    QChar m_medial;
    QChar m_final;
    QString m_vowelInputs;
    QString m_finalInputs;
};
