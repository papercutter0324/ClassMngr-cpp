#include "core/fontmanager.h"

#include <QApplication>
#include <QFont>
#include <QtTest>

class FontManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void standardSizesAreExplicit()
    {
        QCOMPARE(FontManager::stdEnglishFont, 12);
        QCOMPARE(FontManager::stdKoreanFont, 13);
    }

    void standardFontsUseExpectedFamiliesAndSizes()
    {
        const QFont english = FontManager::getUiFont();
        const QFont korean = FontManager::getKoreanFont();

        QVERIFY(english.family().contains(
            QStringLiteral("Inter"),
            Qt::CaseInsensitive
            ));
        QCOMPARE(
            english.pointSize(),
            FontManager::getPlatformFontSize()
            );

        QVERIFY(korean.family().contains(
            QStringLiteral("Pretendard"),
            Qt::CaseInsensitive
            ));
        QCOMPARE(
            korean.pointSize(),
            FontManager::stdKoreanFont
            );

        QVERIFY(english.families().size() >= 2);
        QVERIFY(korean.families().size() >= 2);
        QVERIFY(english.families().at(1).contains(
            QStringLiteral("Pretendard"),
            Qt::CaseInsensitive
            ));
        QVERIFY(korean.families().at(1).contains(
            QStringLiteral("Inter"),
            Qt::CaseInsensitive
            ));
    }

    void koreanFontPreservesRequestedStyle()
    {
        const QFont font = FontManager::getKoreanFont(
            17,
            QFont::DemiBold,
            true
            );

        QCOMPARE(font.pointSize(), 17);
        QCOMPARE(font.weight(), QFont::DemiBold);
        QVERIFY(font.italic());
        QCOMPARE(
            font.hintingPreference(),
            QFont::PreferFullHinting
            );
        QVERIFY(
            (static_cast<int>(font.styleStrategy())
             & static_cast<int>(QFont::PreferAntialias)) != 0
            );
    }

    void missingPrimaryFamilyUsesFallback()
    {
        const QString fallbackFamily =
            QApplication::font().family();

        const QFont font = FontManager::buildFont(
            QString(),
            fallbackFamily,
            FontManager::stdEnglishFont,
            QFont::Normal,
            false
            );

        QCOMPARE(font.family(), fallbackFamily);
        QCOMPARE(
            font.pointSize(),
            FontManager::stdEnglishFont
            );
    }

    void globalFontTracksLocale()
    {
        auto* app =
            qobject_cast<QApplication*>(
                QCoreApplication::instance()
                );
        QVERIFY(app);

        FontManager::applyGlobalFont(
            *app,
            QStringLiteral("ko_KR")
            );
        QVERIFY(app->font().family().contains(
            QStringLiteral("Pretendard"),
            Qt::CaseInsensitive
            ));
        QCOMPARE(
            app->font().pointSize(),
            FontManager::stdKoreanFont
            );

        FontManager::applyGlobalFont(
            *app,
            QStringLiteral("en_US")
            );
        QVERIFY(app->font().family().contains(
            QStringLiteral("Inter"),
            Qt::CaseInsensitive
            ));
        QCOMPARE(
            app->font().pointSize(),
            FontManager::getPlatformFontSize()
            );
    }
};

QTEST_MAIN(FontManagerTests)

#include "fontmanager_tests.moc"
