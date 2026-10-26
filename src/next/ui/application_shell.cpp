#include "next/ui/application_shell.h"

#include "next/ui/initial_page.h"

#include <QCloseEvent>

#include <utility>

namespace ClassMngr::Next::Ui
{

ApplicationShell::ApplicationShell(QWidget* parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("classMngrNextApplicationShell"));
    setWindowTitle(tr("ClassMngr Next"));
    setMinimumSize(720, 480);
    resize(1024, 720);
    setCentralWidget(new InitialPage);
}

void ApplicationShell::setCloseHandler(CloseHandler handler)
{
    m_closeHandler = std::move(handler);
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
