#pragma once

#include "next/application/splitter_state_port.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Test
{

class MemorySplitterStatePort final : public Application::SplitterStatePort
{
public:
    [[nodiscard]] std::string read(
        const Application::SplitterStateKey key
        ) const override
    {
        reads.push_back(key);
        const auto found = payloads.find(key);
        return found == payloads.end() ? std::string{} : found->second;
    }

    void write(
        const Application::SplitterStateKey key,
        const std::string_view statePayload
        ) const override
    {
        writes.push_back(key);
        payloads[key] = std::string(statePayload.data(), statePayload.size());
    }

    void seed(
        const Application::SplitterStateKey key,
        std::string statePayload
        )
    {
        payloads[key] = std::move(statePayload);
    }

    [[nodiscard]] int writeCount(
        const Application::SplitterStateKey key
        ) const
    {
        int count = 0;
        for (const Application::SplitterStateKey written : writes)
        {
            if (written == key)
            {
                ++count;
            }
        }
        return count;
    }

    mutable std::map<Application::SplitterStateKey, std::string> payloads;
    mutable std::vector<Application::SplitterStateKey> reads;
    mutable std::vector<Application::SplitterStateKey> writes;
};

} // namespace ClassMngr::Next::Test
