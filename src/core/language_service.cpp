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

    switch (storedValue)
    {
    case static_cast<int>(Language::SystemDefault):
        return Language::SystemDefault;

    case 1:
    case 2:
    case 3:
    case 4:
        SettingsManager::instance().set(
            OptionKeys::Language,
            static_cast<int>(Language::English)
            );
        return Language::English;

    case static_cast<int>(Language::Korean):
        return Language::Korean;
    }

    return Language::SystemDefault;
}

QString LanguageService::localeNameFor(
    Language language
    )
{
    switch (language)
    {
    case Language::English:
        return englishLocaleFor(QLocale::system().uiLanguages());

    case Language::Korean:
        return QStringLiteral("ko_KR");

    case Language::SystemDefault:
        return QString();
    }

    return QString();
}

QString LanguageService::englishLocaleFor(
    const QStringList& uiLanguages
    )
{
    for (const QString& uiLanguage : uiLanguages)
    {
        const QLocale locale(uiLanguage);

        if (locale.language() != QLocale::English)
        {
            continue;
        }

        switch (locale.territory())
        {
        case QLocale::UnitedStates:
            return QStringLiteral("en_US");

        case QLocale::Canada:
            return QStringLiteral("en_CA");

        case QLocale::UnitedKingdom:
            return QStringLiteral("en_GB");

        case QLocale::Australia:
            return QStringLiteral("en_AU");

        default:
            break;
        }
    }

    return QStringLiteral("en_US");
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
