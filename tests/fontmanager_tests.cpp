#include "core/fontmanager.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QtTest>

class FontManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        FontManager::setSizeOffset(0);
    }

    void cleanup()
    {
        FontManager::setSizeOffset(0);

        auto* app =
            qobject_cast<QApplication*>(
                QCoreApplication::instance()
                );

        if (app)
        {
            FontManager::applyGlobalFont(
                *app,
                QStringLiteral("en_US")
                );
        }
    }

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

    void configuredOffsetAppliesToAllManagedSizes()
    {
        const QList<int> offsets =
            {
                -2,
                0,
                2,
                4
            };

        for (const int offset : offsets)
        {
            FontManager::setSizeOffset(offset);

            QCOMPARE(
                FontManager::sizeOffset(),
                offset
                );
            QCOMPARE(
                FontManager::getUiFont(10).pointSize(),
                10 + offset
                );
            QCOMPARE(
                FontManager::getKoreanFont(13).pointSize(),
                13 + offset
                );
            QCOMPARE(
                FontManager::getUiFont().pointSize(),
                FontManager::getPlatformFontSize() + offset
                );
        }
    }

    void storedFontSizeValuesAreValidated()
    {
        QCOMPARE(
            fontSizeFromStoredValue(-2),
            FontSize::Small
            );
        QCOMPARE(
            fontSizeFromStoredValue(0),
            FontSize::Normal
            );
        QCOMPARE(
            fontSizeFromStoredValue(2),
            FontSize::Large
            );
        QCOMPARE(
            fontSizeFromStoredValue(4),
            FontSize::ExtraLarge
            );
        QCOMPARE(
            fontSizeFromStoredValue(99),
            FontSize::Normal
            );
    }

    void liveFontSizeChangesDoNotAccumulate()
    {
        auto* app =
            qobject_cast<QApplication*>(
                QCoreApplication::instance()
                );
        QVERIFY(app);

        FontManager::applyGlobalFont(
            *app,
            QStringLiteral("en_US")
            );

        QWidget container;
        QLabel inheritedLabel(&container);
        QLabel explicitLabel(&container);
        explicitLabel.setFont(
            FontManager::getUiFont(10)
            );

        QTableWidget table(1, 1, &container);
        auto* item =
            new QTableWidgetItem(QStringLiteral("Item"));
        item->setFont(
            FontManager::getUiFont(11)
            );
        table.setItem(0, 0, item);

        QLabel richTextLabel(&container);
        FontManager::setManagedRichText(
            &richTextLabel,
            QStringLiteral(
                "<span style=\"font-size:13pt\">한국어</span>"
                "<span style=\"font-size:14pt\">English</span>"
                )
            );

        FontManager::applyFontSize(
            *app,
            QStringLiteral("en_US"),
            4
            );

        QCOMPARE(explicitLabel.font().pointSize(), 14);
        QCOMPARE(item->font().pointSize(), 15);
        QCOMPARE(
            inheritedLabel.font().pointSize(),
            FontManager::getPlatformFontSize() + 4
            );
        QVERIFY(richTextLabel.text().contains(
            QStringLiteral("font-size:17pt")
            ));
        QVERIFY(richTextLabel.text().contains(
            QStringLiteral("font-size:18pt")
            ));

        FontManager::applyGlobalFont(
            *app,
            QStringLiteral("ko_KR")
            );
        QVERIFY(inheritedLabel.font().family().contains(
            QStringLiteral("Pretendard"),
            Qt::CaseInsensitive
            ));
        QCOMPARE(
            inheritedLabel.font().pointSize(),
            FontManager::stdKoreanFont + 4
            );
        QVERIFY(explicitLabel.font().family().contains(
            QStringLiteral("Inter"),
            Qt::CaseInsensitive
            ));

        FontManager::applyGlobalFont(
            *app,
            QStringLiteral("en_US")
            );

        FontManager::applyFontSize(
            *app,
            QStringLiteral("en_US"),
            -2
            );

        QCOMPARE(explicitLabel.font().pointSize(), 8);
        QCOMPARE(item->font().pointSize(), 9);
        QVERIFY(richTextLabel.text().contains(
            QStringLiteral("font-size:11pt")
            ));
        QVERIFY(richTextLabel.text().contains(
            QStringLiteral("font-size:12pt")
            ));

        FontManager::applyFontSize(
            *app,
            QStringLiteral("en_US"),
            0
            );

        QCOMPARE(explicitLabel.font().pointSize(), 10);
        QCOMPARE(item->font().pointSize(), 11);
        QCOMPARE(
            inheritedLabel.font().pointSize(),
            FontManager::getPlatformFontSize()
            );
        QCOMPARE(
            richTextLabel.text(),
            QStringLiteral(
                "<span style=\"font-size:13pt\">한국어</span>"
                "<span style=\"font-size:14pt\">English</span>"
                )
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

    void buttonWidthHintsTrackTextAndFontSize()
    {
        TextFitPushButton button(
            QStringLiteral("Save Changes")
            );

        const int normalWidth =
            button.minimumSizeHint().width();
        const int normalSizeHintWidth =
            button.sizeHint().width();

        button.setText(
            QStringLiteral("Save All Changes to This Class")
            );

        const int longTextWidth =
            button.minimumSizeHint().width();
        const int longTextSizeHintWidth =
            button.sizeHint().width();

        QVERIFY(longTextWidth > normalWidth);
        QVERIFY(longTextSizeHintWidth > normalSizeHintWidth);
        QVERIFY(
            longTextWidth
            >= QFontMetrics(button.font())
                .horizontalAdvance(button.text()) + 48
            );

        QFont largerFont = button.font();
        largerFont.setPointSize(
            largerFont.pointSize() + 4
            );
        button.setFont(largerFont);

        QVERIFY(
            button.minimumSizeHint().width()
            > longTextWidth
            );
        QVERIFY(
            button.sizeHint().width()
            > longTextSizeHintWidth
            );
    }
};

QTEST_MAIN(FontManagerTests)

#include "fontmanager_tests.moc"
