#include "next/ui/font_size_presenter.h"
#include "font_family_availability_test_support.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

class FontSizePresenterTests final : public QObject
{
    Q_OBJECT
private slots:
    void allOffsetsLanguagesAndRestoration()
    {
        auto& application = *qApp;
        const QFont previous = application.font();
        struct HostCase
        {
            int applicationBasePointSize;
            int teacherProfileKoreanPointSize;
        };
        const HostCase hosts[] = {{14, 15}, {15, 16}};
        struct PreferenceCase { Application::FontSize size; int offset; };
        const PreferenceCase preferences[] = {
            {Application::FontSize::Small, -2},
            {Application::FontSize::Normal, 0},
            {Application::FontSize::Large, 2},
            {Application::FontSize::ExtraLarge, 4}
        };

        for (const HostCase& host : hosts)
        {
            Test::FixedFontFamilyAvailabilityPort fontPresentation(
                {}, {}, 14, host.applicationBasePointSize,
                host.teacherProfileKoreanPointSize);
            QCOMPARE(fontPresentation.applicationBaseFontPointSize(),
                     host.applicationBasePointSize);
            QCOMPARE(fontPresentation.teacherProfileKoreanFontPointSize(),
                     host.teacherProfileKoreanPointSize);
            {
                Ui::FontSizePresenter presenter(application, fontPresentation);
                for (const PreferenceCase& preference : preferences)
                {
                    presenter.applyStartupPreference(
                        preference.size, QStringLiteral("en_CA"),
                        QStringLiteral("Inter"), QStringLiteral("Pretendard"));
                    QCOMPARE(application.font().pointSize(),
                             host.applicationBasePointSize + preference.offset);
                    QCOMPARE(application.font().families(),
                             (QStringList{QStringLiteral("Inter"),
                                          QStringLiteral("Pretendard")}));
                    presenter.applyStartupPreference(
                        preference.size, QStringLiteral("ko_KR"),
                        QStringLiteral("Inter"), QStringLiteral("Pretendard"));
                    QCOMPARE(application.font().pointSize(),
                             host.applicationBasePointSize + preference.offset + 1);
                    QCOMPARE(application.font().families(),
                             (QStringList{QStringLiteral("Pretendard"),
                                          QStringLiteral("Inter")}));
                }
            }
        }
        QCOMPARE(application.font(), previous);
    }

    void changePreferencePersistsHostOffset()
    {
        auto& application = *qApp;
        const QFont previous = application.font();
        for (const int basePointSize : {14, 15})
        {
            Test::FixedFontFamilyAvailabilityPort fontPresentation(
                {}, {}, 14, basePointSize, basePointSize + 1);
            Test::MemoryPreferences preferences;
            {
                Ui::FontSizePresenter presenter(application, fontPresentation);
                presenter.applyStartupPreference(
                    Application::FontSize::Normal,
                    QStringLiteral("en_CA"),
                    QStringLiteral("Inter"),
                    QStringLiteral("Pretendard"));
                const auto changed = presenter.changePreference(
                    Application::FontSize::Large, preferences.model);
                QVERIFY(changed);
                QCOMPARE(application.font().pointSize(), basePointSize + 2);
                QCOMPARE(preferences.model.snapshot().fontSize(),
                         Application::FontSize::Large);
            }
        }
        QCOMPARE(application.font(), previous);
    }
};
QTEST_MAIN(FontSizePresenterTests)
#include "font_size_presenter_tests.moc"
