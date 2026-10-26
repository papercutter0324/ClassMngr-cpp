#pragma once

#include "next/application/font_size_preferences.h"

#include <QApplication>
#include <QFont>
#include <QString>
#include <QStringList>
#include <algorithm>

namespace ClassMngr::Next::Ui
{

// Applies startup fonts after scoped registration, before shell construction.
// This mirrors FontManager's point-size rules without borrowing its global
// registrations or mutable offset. The registered families must outlive use.
class FontSizePresenter final
{
public:
    explicit FontSizePresenter(QApplication& application)
        : m_application(application), m_previousFont(application.font()) {}
    ~FontSizePresenter() { restore(); }
    FontSizePresenter(const FontSizePresenter&) = delete;
    FontSizePresenter& operator=(const FontSizePresenter&) = delete;

    void applyStartupPreference(Application::FontSize size, const QString& locale,
                                const QString& inter, const QString& pretendard)
    {
        int offset = 0;
        switch (size)
        {
        case Application::FontSize::Small: offset = -2; break;
        case Application::FontSize::Normal: break;
        case Application::FontSize::Large: offset = 2; break;
        case Application::FontSize::ExtraLarge: offset = 4; break;
        }
        // Legacy English base: 14 points, plus one on macOS. Korean adds one.
        int points = 14;
#ifdef Q_OS_MACOS
        ++points;
#endif
        const bool korean = locale.startsWith(QStringLiteral("ko"), Qt::CaseInsensitive);
        QFont font;
        font.setFamilies(korean ? QStringList{pretendard, inter} : QStringList{inter, pretendard});
        font.setPointSize(std::max(1, points + offset) + (korean ? 1 : 0));
        font.setWeight(QFont::Normal);
        font.setItalic(false);
        font.setHintingPreference(QFont::PreferFullHinting);
        font.setStyleStrategy(QFont::PreferAntialias);
        m_application.setFont(font);
        m_applied = true;
    }

    void restore() noexcept
    {
        if (m_applied)
        {
            m_application.setFont(m_previousFont);
            m_applied = false;
        }
    }

private:
    QApplication& m_application;
    QFont m_previousFont;
    bool m_applied = false;
};

}
