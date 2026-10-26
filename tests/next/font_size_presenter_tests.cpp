#include "next/ui/font_size_presenter.h"

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
        int base = 14;
#ifdef Q_OS_MACOS
        ++base;
#endif
        {
            Ui::FontSizePresenter presenter(application);
            struct Case { Application::FontSize size; int offset; };
            const Case cases[] = {
                {Application::FontSize::Small, -2}, {Application::FontSize::Normal, 0},
                {Application::FontSize::Large, 2}, {Application::FontSize::ExtraLarge, 4}
            };
            for (const auto& testCase : cases)
            {
                presenter.applyStartupPreference(testCase.size, QStringLiteral("en_CA"),
                    QStringLiteral("Inter"), QStringLiteral("Pretendard"));
                QCOMPARE(application.font().pointSize(), base + testCase.offset);
                QCOMPARE(application.font().families(), (QStringList{QStringLiteral("Inter"), QStringLiteral("Pretendard")}));
                presenter.applyStartupPreference(testCase.size, QStringLiteral("ko_KR"),
                    QStringLiteral("Inter"), QStringLiteral("Pretendard"));
                QCOMPARE(application.font().pointSize(), base + testCase.offset + 1);
                QCOMPARE(application.font().families(), (QStringList{QStringLiteral("Pretendard"), QStringLiteral("Inter")}));
                presenter.applyStartupPreference(testCase.size, QStringLiteral("ko_KR"),
                    QStringLiteral("Inter"), QStringLiteral("Pretendard"));
                QCOMPARE(application.font().pointSize(), base + testCase.offset + 1);
            }
        }
        QCOMPARE(application.font(), previous);
    }
};
QTEST_MAIN(FontSizePresenterTests)
#include "font_size_presenter_tests.moc"
