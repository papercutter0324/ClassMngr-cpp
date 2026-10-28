#pragma once

#include "next/application/class_analytics_page_read.h"

#include <QDate>
#include <QString>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] Application::ClassAnalyticsPageReadResult
readClassAnalyticsAtPath(
    const QString& databasePath,
    const Application::ClassAnalyticsPageReadRequest& request
    );

// Deterministic boundary for tests and paired captures; production always
// uses the current local date through readClassAnalyticsAtPath().
[[nodiscard]] Application::ClassAnalyticsPageReadResult
readClassAnalyticsAtPathForDate(
    const QString& databasePath,
    const Application::ClassAnalyticsPageReadRequest& request,
    const QDate& referenceDate
    );

} // namespace ClassMngr::Next::Platform
