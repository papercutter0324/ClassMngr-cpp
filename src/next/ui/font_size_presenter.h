#pragma once

#include "next/application/font_size_preferences.h"
#include "next/application/preferences_model.h"

#include <QApplication>
#include <QFont>
#include <QString>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <optional>

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
        m_locale = locale;
        m_interFamily = inter;
        m_pretendardFamily = pretendard;
        m_activeSize = size;
        m_application.setFont(fontFor(size, locale, inter, pretendard));
        m_applied = true;
    }

    [[nodiscard]] Domain::Result<void> changePreference(
        const Application::FontSize size,
        Application::PreferencesModel& preferences
        )
    {
        Application::UserPreferencesState candidate = preferences.state();
        const auto validated = candidate.setFontSize(size);
        if (!validated)
        {
            return validated;
        }

        const QFont previousFont = m_application.font();
        m_application.setFont(fontFor(
            size, m_locale, m_interFamily, m_pretendardFamily));
        try
        {
            const auto saved = preferences.applyAndPersist(std::move(candidate));
            if (!saved)
            {
                m_application.setFont(previousFont);
                return Domain::Result<void>::failure(saved.error());
            }
        }
        catch (...)
        {
            // Observer failures are delivered after a successful model commit.
            if (!preferences.isDirty()
                && preferences.snapshot().fontSize() == size)
            {
                m_activeSize = size;
                m_applied = true;
            }
            else
            {
                m_application.setFont(previousFont);
            }
            throw;
        }
        m_activeSize = size;
        m_applied = true;
        return Domain::Result<void>::success();
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
    [[nodiscard]] static QFont fontFor(
        const Application::FontSize size,
        const QString& locale,
        const QString& inter,
        const QString& pretendard
        )
    {
        int offset = 0;
        switch (size)
        {
        case Application::FontSize::Small: offset = -2; break;
        case Application::FontSize::Normal: break;
        case Application::FontSize::Large: offset = 2; break;
        case Application::FontSize::ExtraLarge: offset = 4; break;
        }
        int points = 14;
#ifdef Q_OS_MACOS
        ++points;
#endif
        const bool korean = locale.startsWith(QStringLiteral("ko"), Qt::CaseInsensitive);
        QFont font;
        QStringList families;
        if (korean)
        {
            families = QStringList{pretendard, inter};
        }
        else
        {
            families = QStringList{inter, pretendard};
        }
        families.removeAll(QString{});
        if (!families.isEmpty())
        {
            font.setFamilies(families);
        }
        font.setPointSize(std::max(1, points + offset) + (korean ? 1 : 0));
        font.setWeight(QFont::Normal);
        font.setItalic(false);
        font.setHintingPreference(QFont::PreferFullHinting);
        font.setStyleStrategy(QFont::PreferAntialias);
        return font;
    }

    QApplication& m_application;
    QFont m_previousFont;
    QString m_locale;
    QString m_interFamily;
    QString m_pretendardFamily;
    std::optional<Application::FontSize> m_activeSize;
    bool m_applied = false;
};

}
