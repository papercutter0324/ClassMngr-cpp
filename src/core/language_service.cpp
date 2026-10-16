#include "language_service.h"

#include "core/appsettings.h"

#include <QApplication>
#include <QLocale>
#include <QStringList>

LanguageService::LanguageService(
    QObject* parent,
    TranslationOperations operations
    )
    : QObject(parent)
    , m_operations(std::move(operations))
{
}

LanguageService::~LanguageService()
{
    removeCurrentTranslator();
}

std::unique_ptr<LanguageService::PreparedLanguage> LanguageService::prepareLanguage(
    const Language language
    ) const
{
    if (language != Language::SystemDefault && language != Language::English
        && language != Language::Korean)
    {
        return {};
    }

    auto prepared = std::make_unique<PreparedLanguage>();
    prepared->language = language;
    prepared->translator = std::make_unique<QTranslator>();

    for (const QString& localeName : candidateLocalesFor(language))
    {
        const QString resourcePath =
            QStringLiteral(":/i18n/%1%2")
                .arg(
                    QString::fromLatin1(AppSettings::TranslationPrefix),
                    localeName
                    );

        const bool loaded = m_operations.load
            ? m_operations.load(*prepared->translator, resourcePath)
            : prepared->translator->load(resourcePath);
        if (!loaded)
        {
            continue;
        }

        prepared->locale = localeName;
        return prepared;
    }

    if (language != Language::SystemDefault)
    {
        return {};
    }
    prepared->translator.reset();
    return prepared;
}

bool LanguageService::applyPreparedLanguage(
    std::unique_ptr<PreparedLanguage> prepared
    )
{
    if (!prepared)
    {
        return false;
    }

    bool installed = false;
    if (prepared->translator && qApp)
    {
        installed = m_operations.install
            ? m_operations.install(prepared->translator.get())
            : qApp->installTranslator(prepared->translator.get());
        if (!installed)
        {
            return false;
        }
    }

    // Keep the old translator alive and installed until its replacement is
    // installed successfully. A failed request cannot disturb the live locale.
    removeCurrentTranslator();
    m_translator = std::move(prepared->translator);
    m_translatorInstalled = installed;
    m_language = prepared->language;
    m_loadedLocaleName = std::move(prepared->locale);
    return true;
}

bool LanguageService::setLanguage(const Language language)
{
    return applyPreparedLanguage(prepareLanguage(language));
}

Language LanguageService::currentLanguage() const
{
    return m_language;
}

QString LanguageService::loadedLocaleName() const
{
    return m_loadedLocaleName;
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
        m_translator.get()
        );

    m_translatorInstalled = false;
}
