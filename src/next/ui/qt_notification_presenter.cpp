#include "next/ui/qt_notification_presenter.h"

#include <QApplication>
#include <QCoreApplication>
#include <QObject>
#include <QStatusBar>
#include <QString>
#include <QThread>
#include <QVariant>

#include <limits>
#include <string>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr auto SeverityProperty = "classMngrNextNotificationSeverity";

QString qString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

bool validSeverity(const Application::NotificationSeverity severity)
{
    return severity == Application::NotificationSeverity::Information
        || severity == Application::NotificationSeverity::Warning
        || severity == Application::NotificationSeverity::Error;
}

bool validTimeout(const std::chrono::milliseconds timeout)
{
    return timeout.count() > 0
        && timeout.count() <= std::numeric_limits<int>::max();
}

Domain::Result<void> invalidRequest(const std::string& message)
{
    return Domain::Result<void>::failure({
        .code = Domain::ErrorCode::InvalidInput,
        .message = message,
        .recoverable = false
    });
}

Domain::Result<void> presenterUnavailable()
{
    return Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "Qt notifications require the presenter's GUI thread and a live status bar.",
        .recoverable = false
    });
}

bool canPresent(const QStatusBar* statusBar)
{
    auto* application = qobject_cast<QApplication*>(QCoreApplication::instance());
    return application
        && application->thread() == QThread::currentThread()
        && statusBar
        && statusBar->thread() == QThread::currentThread();
}

QString messageFor(const Application::NotificationRequest& request)
{
    switch (request.severity)
    {
        case Application::NotificationSeverity::Information:
            return qString(request.message);
        case Application::NotificationSeverity::Warning:
            return QCoreApplication::translate(
                "QtNotificationPresenter",
                "Warning: "
                ) + qString(request.message);
        case Application::NotificationSeverity::Error:
            return QCoreApplication::translate(
                "QtNotificationPresenter",
                "Error: "
                ) + qString(request.message);
    }
    return {};
}

} // namespace

QtNotificationPresenter::QtNotificationPresenter(
    QStatusBar& statusBar,
    const std::chrono::milliseconds timeout
    )
    : m_statusBar(&statusBar)
    , m_timeout(timeout)
{
    statusBar.hide();
    QObject::connect(
        &statusBar,
        &QStatusBar::messageChanged,
        &statusBar,
        [&statusBar](const QString& message) {
            if (message.isEmpty())
            {
                statusBar.setProperty(SeverityProperty, QVariant{});
                statusBar.hide();
            }
        }
        );
}

QtNotificationPresenter::~QtNotificationPresenter()
{
    if (m_statusBar && m_statusBar->thread() == QThread::currentThread())
    {
        m_statusBar->clearMessage();
        m_statusBar->hide();
        m_statusBar->setProperty(SeverityProperty, QVariant{});
    }
}

Domain::Result<void> QtNotificationPresenter::show(
    const Application::NotificationRequest& request
    )
{
    if (request.message.empty()
        || qString(request.message).trimmed().isEmpty()
        || !validSeverity(request.severity))
    {
        return invalidRequest("A notification requires non-empty text and a supported severity.");
    }
    if (!validTimeout(m_timeout))
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "The Qt notification timeout must be positive and finite.",
            .recoverable = false
        });
    }
    if (!canPresent(m_statusBar))
    {
        return presenterUnavailable();
    }

    m_statusBar->setProperty(
        SeverityProperty,
        static_cast<int>(request.severity)
        );
    m_statusBar->showMessage(
        messageFor(request),
        static_cast<int>(m_timeout.count())
        );
    m_statusBar->show();
    return Domain::Result<void>::success();
}

Domain::Result<void> QtNotificationPresenter::clear()
{
    if (!canPresent(m_statusBar))
    {
        return presenterUnavailable();
    }

    m_statusBar->clearMessage();
    m_statusBar->hide();
    m_statusBar->setProperty(SeverityProperty, QVariant{});
    return Domain::Result<void>::success();
}

} // namespace ClassMngr::Next::Ui
