#pragma once

#include "next/application/splitter_state_port.h"

#include <QByteArray>
#include <QPointer>
#include <QSplitter>

#include <memory>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ClassMngr::Next::Ui
{

// Captures a splitter's configured defaults once, then applies a stored state
// once for this page instance. It saves only when the page is suspended.
class SplitterStatePersistence final
{
public:
    SplitterStatePersistence(
        std::shared_ptr<Application::SplitterStatePort> port,
        const Application::SplitterStateKey key
        )
        : m_port(std::move(port))
        , m_key(key)
    {
        if (!m_port)
        {
            throw std::invalid_argument(
                "Next page splitter state requires an injected persistence port."
                );
        }
    }

    void restoreAfterConfiguration(QSplitter* splitter)
    {
        if (!splitter || m_splitter)
        {
            return;
        }

        m_splitter = splitter;
        m_defaultState = splitter->saveState();

        const std::string stored = m_port->read(m_key);
        if (stored.empty())
        {
            return;
        }

        const QByteArray payload(
            stored.data(),
            static_cast<qsizetype>(stored.size())
            );
        if (!splitter->restoreState(payload))
        {
            static_cast<void>(splitter->restoreState(m_defaultState));
        }
    }

    void save() const noexcept
    {
        if (!m_splitter)
        {
            return;
        }

        try
        {
            const QByteArray payload = m_splitter->saveState();
            m_port->write(
                m_key,
                std::string_view(
                    payload.constData(),
                    static_cast<std::size_t>(payload.size())
                    )
                );
        }
        catch (...)
        {
            // PageHost invokes this from noexcept lifecycle callbacks.
        }
    }

private:
    std::shared_ptr<Application::SplitterStatePort> m_port;
    Application::SplitterStateKey m_key;
    QPointer<QSplitter> m_splitter;
    QByteArray m_defaultState;
};

} // namespace ClassMngr::Next::Ui
