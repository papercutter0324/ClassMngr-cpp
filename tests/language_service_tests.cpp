#include "core/language_service.h"

#include <QtTest>

class LanguageServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void choosesSupportedEnglishRegion_data();
    void choosesSupportedEnglishRegion();
    void defaultsUnsupportedLocaleToAmericanEnglish();
    void stagesAndPreservesLiveLanguageOnLoadAndInstallFailure();
};

void LanguageServiceTests::choosesSupportedEnglishRegion_data()
{
    QTest::addColumn<QString>("uiLanguage");
    QTest::addColumn<QString>("expectedLocale");

    QTest::newRow("American") << QStringLiteral("en-US") << QStringLiteral("en_US");
    QTest::newRow("Canadian") << QStringLiteral("en-CA") << QStringLiteral("en_CA");
    QTest::newRow("British") << QStringLiteral("en-GB") << QStringLiteral("en_GB");
    QTest::newRow("Australian") << QStringLiteral("en-AU") << QStringLiteral("en_AU");
}

void LanguageServiceTests::choosesSupportedEnglishRegion()
{
    QFETCH(QString, uiLanguage);
    QFETCH(QString, expectedLocale);

    QCOMPARE(
        LanguageService::englishLocaleFor({uiLanguage}),
        expectedLocale
        );
}

void LanguageServiceTests::defaultsUnsupportedLocaleToAmericanEnglish()
{
    QCOMPARE(
        LanguageService::englishLocaleFor({QStringLiteral("en-NZ")}),
        QStringLiteral("en_US")
        );
    QCOMPARE(
        LanguageService::englishLocaleFor({QStringLiteral("ko-KR")}),
        QStringLiteral("en_US")
        );
}

void LanguageServiceTests::stagesAndPreservesLiveLanguageOnLoadAndInstallFailure()
{
    bool failLoad = false;
    bool failInstall = false;
    int installs = 0;
    QTranslator* installedTranslator = nullptr;
    LanguageService service(nullptr, {
        .load = [&](QTranslator&, const QString&) { return !failLoad; },
        .install = [&](QTranslator* translator)
        {
            ++installs;
            if (failInstall)
                return false;
            installedTranslator = translator;
            return true;
        }
    });
    QVERIFY(service.setLanguage(Language::English));
    const auto previousLocale = service.loadedLocaleName();
    QTranslator* const previousTranslator = installedTranslator;

    failLoad = true;
    QVERIFY(!service.setLanguage(Language::Korean));
    QCOMPARE(installs, 1);
    QCOMPARE(service.currentLanguage(), Language::English);
    QCOMPARE(service.loadedLocaleName(), previousLocale);
    QCOMPARE(installedTranslator, previousTranslator);

    failLoad = false;
    auto prepared = service.prepareLanguage(Language::Korean);
    QVERIFY(prepared);
    QCOMPARE(installs, 1);
    QCOMPARE(service.currentLanguage(), Language::English);
    QCOMPARE(service.loadedLocaleName(), previousLocale);

    failInstall = true;
    QVERIFY(!service.applyPreparedLanguage(std::move(prepared)));
    QCOMPARE(installs, 2);
    QCOMPARE(service.currentLanguage(), Language::English);
    QCOMPARE(service.loadedLocaleName(), previousLocale);
    QCOMPARE(installedTranslator, previousTranslator);

    failInstall = false;
    QVERIFY(service.setLanguage(Language::Korean));
    QCOMPARE(installs, 3);
    QCOMPARE(service.currentLanguage(), Language::Korean);
    QCOMPARE(service.loadedLocaleName(), QStringLiteral("ko_KR"));
    QVERIFY(installedTranslator != previousTranslator);
    QVERIFY(!service.setLanguage(static_cast<Language>(99)));
    QCOMPARE(installs, 3);
    QCOMPARE(service.currentLanguage(), Language::Korean);
}

QTEST_MAIN(LanguageServiceTests)

#include "language_service_tests.moc"
