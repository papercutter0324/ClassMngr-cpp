#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QString>

class QLabel;
class QProgressBar;
class QPropertyAnimation;



// =========================================================
// Splash Screen
// =========================================================

class SplashScreen : public QWidget
{
    Q_OBJECT

public:

    explicit SplashScreen(
        QWidget *parent = nullptr
        );

    SplashScreen(
        const QString& imagePath,
        QWidget* parent = nullptr
        );



    // =====================================================
    // Updates
    // =====================================================

    void setMessage(
        const QString &text
        );

    void setProgress(
        int value
        );



    // =====================================================
    // Animations
    // =====================================================

    void fadeOut(
        std::function<void()> callback
        );



    // =====================================================
    // Positioning
    // =====================================================

    void centerOnScreen();



private:

    // =====================================================
    // Widgets
    // =====================================================

    QLabel *m_background = nullptr;

    QLabel *m_statusShadow = nullptr;

    QLabel *m_status = nullptr;

    QProgressBar *m_progress = nullptr;



    // =====================================================
    // Animations
    // =====================================================

    QPropertyAnimation *m_progressAnim = nullptr;

    QPropertyAnimation *m_fadeAnim = nullptr;
};



#endif // SPLASHSCREEN_H
