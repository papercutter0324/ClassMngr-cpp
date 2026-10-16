#pragma once

#include "ui/shared/constants/options.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>

#include <functional>
#include <memory>

class LanguageService : public QObject
{
    Q_OBJECT

public:
    struct TranslationOperations
    {
        std::function<bool(QTranslator&, const QString&)> load;
        std::function<bool(QTranslator*)> install;
    };

    class PreparedLanguage
    {
        friend class LanguageService;
        Language language = Language::SystemDefault;
        QString locale;
        std::unique_ptr<QTranslator> translator;
    };

    explicit LanguageService(
        QObject* parent = nullptr,
        TranslationOperations operations = {}
        );
    ~LanguageService() override;

    [[nodiscard]] std::unique_ptr<PreparedLanguage> prepareLanguage(Language language) const;
    [[nodiscard]] bool applyPreparedLanguage(std::unique_ptr<PreparedLanguage> prepared);

    bool setLanguage(
        Language language
        );

    [[nodiscard]] Language currentLanguage() const;
    [[nodiscard]] QString loadedLocaleName() const;

    [[nodiscard]] static QString localeNameFor(
        Language language
        );
    [[nodiscard]] static QString englishLocaleFor(
        const QStringList& uiLanguages
        );

private:
    [[nodiscard]] QStringList candidateLocalesFor(
        Language language
        ) const;

    void removeCurrentTranslator();

private:
    TranslationOperations m_operations;
    std::unique_ptr<QTranslator> m_translator;
    Language m_language = Language::SystemDefault;
    QString m_loadedLocaleName;
    bool m_translatorInstalled = false;
};
