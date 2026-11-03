#pragma once

#include "core/settingsmanager.h"
#include "next/application/splitter_state_port.h"

#include <QByteArray>
#include <QMetaType>
#include <QVariant>

#include <cstddef>
#include <string>
#include <string_view>

namespace ClassMngr::Next::Platform
{

// SettingsManager adapter for the four explicitly supported Next page-owned
// splitters. The namespace remains separate from the legacy unscoped key.
class SettingsManagerSplitterStatePort final
    : public Application::SplitterStatePort
{
public:
    [[nodiscard]] std::string read(
        const Application::SplitterStateKey key
        ) const override
    {
        const QString storageKey = settingsKey(key);
        if (storageKey.isEmpty())
        {
            return {};
        }

        const QVariant stored = SettingsManager::instance().get(storageKey);
        if (stored.metaType().id() != QMetaType::QByteArray)
        {
            return {};
        }

        const QByteArray payload = stored.toByteArray();
        if (payload.isEmpty())
        {
            return {};
        }
        return std::string(
            payload.constData(),
            static_cast<std::size_t>(payload.size())
            );
    }

    void write(
        const Application::SplitterStateKey key,
        const std::string_view statePayload
        ) const override
    {
        const QString storageKey = settingsKey(key);
        if (storageKey.isEmpty())
        {
            return;
        }

        SettingsManager& settings = SettingsManager::instance();
        settings.set(
            storageKey,
            QByteArray(
                statePayload.data(),
                static_cast<qsizetype>(statePayload.size())
                )
            );
        // PageHost saves a final active page while it is being destroyed,
        // after the bootstrap shutdown flush has already run.
        settings.sync();
    }

private:
    [[nodiscard]] static QString settingsKey(
        const Application::SplitterStateKey key
        )
    {
        switch (key)
        {
        case Application::SplitterStateKey::ClassesNavigatorContent:
            return QStringLiteral("ui/next/splitters/classesNavigatorContent");
        case Application::SplitterStateKey::TestingClassesManager:
            return QStringLiteral("ui/next/splitters/testingClassesManager");
        case Application::SplitterStateKey::SpeakingEvaluationContent:
            return QStringLiteral("ui/next/splitters/speakingEvaluationContent");
        case Application::SplitterStateKey::ClassTransferWorkflow:
            return QStringLiteral("ui/next/splitters/classTransferWorkflow");
        }

        return {};
    }
};

} // namespace ClassMngr::Next::Platform
