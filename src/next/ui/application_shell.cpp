#include "next/ui/application_shell.h"

#include "next/ui/page_host.h"

#include <QCloseEvent>

#include <utility>

namespace ClassMngr::Next::Ui
{

ApplicationShell::ApplicationShell(
    Resources::ResourceLoader& resourceLoader,
    QWidget* parent
    )
    : QMainWindow(parent)
    , m_pageHost(new PageHost(resourceLoader, this))
{
    setObjectName(QStringLiteral("classMngrNextApplicationShell"));
    setWindowTitle(tr("ClassMngr Next"));
    setMinimumSize(720, 480);
    resize(1024, 720);
    setCentralWidget(m_pageHost);
}

void ApplicationShell::setCloseHandler(CloseHandler handler)
{
    m_closeHandler = std::move(handler);
}

PageHost& ApplicationShell::pageHost() noexcept
{
    return *m_pageHost;
}

const PageHost& ApplicationShell::pageHost() const noexcept
{
    return *m_pageHost;
}

void ApplicationShell::closeEvent(QCloseEvent* event)
{
    QMainWindow::closeEvent(event);
    if (event->isAccepted() && m_closeHandler)
    {
        m_closeHandler();
    }
}

}  // namespace ClassMngr::Next::Ui
