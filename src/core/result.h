#pragma once

#include <QString>

#include <expected>

using Status = std::expected<void, QString>;

template<class T>
using Result = std::expected<T, QString>;
