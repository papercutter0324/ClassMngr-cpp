#include "core/language_service.h"

#include <QtTest>

class LanguageServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void choosesSupportedEnglishRegion_data();
    void choosesSupportedEnglishRegion();
    void defaultsUnsupportedLocaleToAmericanEnglish();
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

QTEST_MAIN(LanguageServiceTests)

#include "language_service_tests.moc"
