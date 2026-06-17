#include "language_service.h"

#include "core/appsettings.h"
#include "core/settingsmanager.h"
#include "ui/shared/state/option_state_keys.h"

#include <QApplication>
#include <QLocale>
#include <QStringList>

LanguageService::LanguageService(
    QObject* parent
    )
    : QObject(parent)
{
}

bool LanguageService::setLanguage(
    Language language
    )
{
    removeCurrentTranslator();

    m_language =
        language;

    for (const QString& localeName : candidateLocalesFor(language))
    {
        const QString resourcePath =
            QStringLiteral(":/i18n/%1%2")
                .arg(
                    QString::fromLatin1(AppSettings::TranslationPrefix),
                    localeName
                    );

        if (!m_translator.load(resourcePath))
        {
            continue;
        }

        if (qApp)
        {
            qApp->installTranslator(
                &m_translator
                );
            m_translatorInstalled = true;
        }

        m_loadedLocaleName =
            localeName;

        return true;
    }

    m_loadedLocaleName.clear();

    return language == Language::SystemDefault;
}

Language LanguageService::currentLanguage() const
{
    return m_language;
}

QString LanguageService::loadedLocaleName() const
{
    return m_loadedLocaleName;
}

Language LanguageService::savedLanguage()
{
    const int storedValue =
        SettingsManager::instance()
            .get(
                OptionKeys::Language,
                static_cast<int>(Language::SystemDefault)
                )
            .toInt();

    switch (static_cast<Language>(storedValue))
    {
    case Language::SystemDefault:
    case Language::EnglishUS:
    case Language::EnglishGB:
    case Language::EnglishCA:
    case Language::EnglishAU:
    case Language::Korean:
        return static_cast<Language>(storedValue);
    }

    return Language::SystemDefault;
}

QString LanguageService::localeNameFor(
    Language language
    )
{
    switch (language)
    {
    case Language::EnglishUS:
        return QStringLiteral("en_US");

    case Language::EnglishGB:
        return QStringLiteral("en_GB");

    case Language::EnglishCA:
        return QStringLiteral("en_CA");

    case Language::EnglishAU:
        return QStringLiteral("en_AU");

    case Language::Korean:
        return QStringLiteral("ko_KR");

    case Language::SystemDefault:
        return QString();
    }

    return QString();
}

QStringList LanguageService::candidateLocalesFor(
    Language language
    ) const
{
    if (language != Language::SystemDefault)
    {
        const QString localeName =
            localeNameFor(language);

        return localeName.isEmpty()
            ? QStringList()
            : QStringList{localeName};
    }

    QStringList locales;

    for (const QString& locale : QLocale::system().uiLanguages())
    {
        const QString localeName =
            QLocale(locale).name();

        if (!localeName.isEmpty() && !locales.contains(localeName))
        {
            locales.append(localeName);
        }
    }

    return locales;
}

void LanguageService::removeCurrentTranslator()
{
    if (!m_translatorInstalled || !qApp)
    {
        m_translatorInstalled = false;
        return;
    }

    qApp->removeTranslator(
        &m_translator
        );

    m_translatorInstalled = false;
}
