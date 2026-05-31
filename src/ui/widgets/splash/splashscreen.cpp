#include "splashscreen.h"

#include "core/fontmanager.h"

#include <QApplication>
#include <QEasingCurve>
#include <QGuiApplication>
#include <QLabel>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QScreen>

#include <QPixmap>



// =========================================================
// Constructor
// =========================================================

SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent)
{
    setFont(
        FontManager::getUiFont(
            11,
            QFont::DemiBold
            )
        );



    // =====================================================
    // Window Flags
    // =====================================================

    setWindowFlags(
        Qt::FramelessWindowHint
        | Qt::WindowStaysOnTopHint
        );



    // =====================================================
    // Splash Image
    // =====================================================

    QPixmap pixmap(
        ":/splash/splash.png"
        );

    setFixedSize(
        pixmap.size()
        );



    // =====================================================
    // Background
    // =====================================================

    m_background =
        new QLabel(this);

    m_background->setPixmap(
        pixmap
        );

    m_background->setGeometry(
        0,
        0,
        width(),
        height()
        );



    // =====================================================
    // Layout Reference
    // =====================================================

    const int barX = 80;

    const int barWidth =
        width() - 160;

    const int barY =
        height() - 20;



    // =====================================================
    // Status Shadow
    // =====================================================

    m_statusShadow =
        new QLabel(
            tr("Starting..."),
            this
            );

    m_statusShadow->setAlignment(
        Qt::AlignCenter
        );

    m_statusShadow->setStyleSheet(
        "color: rgba(0, 0, 0, 180);"
        );

    m_statusShadow->setGeometry(
        barX + 1,
        barY - 24,
        barWidth,
        20
        );



    // =====================================================
    // Status Text
    // =====================================================

    m_status =
        new QLabel(
            tr("Starting..."),
            this
            );

    m_status->setAlignment(
        Qt::AlignCenter
        );

    m_status->setStyleSheet(
        "color: white;"
        );

    m_status->setGeometry(
        barX,
        barY - 25,
        barWidth,
        20
        );



    // =====================================================
    // Progress Bar
    // =====================================================

    m_progress =
        new QProgressBar(this);

    m_progress->setRange(
        0,
        100
        );

    m_progress->setValue(0);

    m_progress->setTextVisible(false);

    m_progress->setGeometry(
        barX,
        barY,
        barWidth,
        12
        );



    // =====================================================
    // Styling
    // =====================================================

    m_progress->setStyleSheet(
        R"(
            QProgressBar {
                background-color: rgba(0, 0, 0, 150);
                border-radius: 6px;
            }

            QProgressBar::chunk {
                background-color: #4da3ff;
                border-radius: 6px;
            }
        )"
        );
}



// =========================================================
// Status
// =========================================================

void SplashScreen::setMessage(
    const QString &text
    )
{
    m_status->setText(text);

    m_statusShadow->setText(text);
}



// =========================================================
// Progress
// =========================================================

void SplashScreen::setProgress(
    int value
    )
{
    if (m_progressAnim)
    {
        m_progressAnim->stop();

        delete m_progressAnim;
    }

    m_progressAnim =
        new QPropertyAnimation(
            m_progress,
            "value",
            this
            );

    m_progressAnim->setDuration(300);

    m_progressAnim->setStartValue(
        m_progress->value()
        );

    m_progressAnim->setEndValue(
        value
        );

    m_progressAnim->start();
}



// =========================================================
// Fade Out
// =========================================================

void SplashScreen::fadeOut(
    std::function<void()> callback
    )
{
    if (m_fadeAnim)
    {
        m_fadeAnim->stop();

        delete m_fadeAnim;
    }

    m_fadeAnim =
        new QPropertyAnimation(
            this,
            "windowOpacity",
            this
            );

    m_fadeAnim->setDuration(500);

    m_fadeAnim->setStartValue(1.0);

    m_fadeAnim->setEndValue(0.0);

    m_fadeAnim->setEasingCurve(
        QEasingCurve::InOutQuad
        );

    connect(
        m_fadeAnim,
        &QPropertyAnimation::finished,
        this,
        callback
        );

    m_fadeAnim->start();
}



// =========================================================
// Centering
// =========================================================

void SplashScreen::centerOnScreen()
{
    // adjustSize();

    QScreen *screen =
        QGuiApplication::primaryScreen();

    if (!screen)
    {
        return;
    }

    QRect geometry =
        screen->availableGeometry();

    int x =
        geometry.x()
        + (geometry.width() - width()) / 2;

    int y =
        geometry.y()
        + (geometry.height() - height()) / 2;

    move(x, y);
}