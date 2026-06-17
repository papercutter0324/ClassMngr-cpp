#pragma once

#include "ui/shared/constants/options.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>

class LanguageService : public QObject
{
    Q_OBJECT

public:
    explicit LanguageService(
        QObject* parent = nullptr
        );

    bool setLanguage(
        Language language
        );

    [[nodiscard]] Language currentLanguage() const;
    [[nodiscard]] QString loadedLocaleName() const;

    [[nodiscard]] static Language savedLanguage();
    [[nodiscard]] static QString localeNameFor(
        Language language
        );

private:
    [[nodiscard]] QStringList candidateLocalesFor(
        Language language
        ) const;

    void removeCurrentTranslator();

private:
    QTranslator m_translator;
    Language m_language = Language::SystemDefault;
    QString m_loadedLocaleName;
    bool m_translatorInstalled = false;
};
