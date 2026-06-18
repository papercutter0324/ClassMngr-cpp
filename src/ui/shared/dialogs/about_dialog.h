#pragma once

#include <QDialog>
#include <QString>

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(
        QWidget* parent = nullptr
        );

private slots:
    void showInterLicense();
    void showPretendardLicense();

private:
    void buildUi();
    void showLicense(
        const QString& title,
        const QString& relativePath
        );
};
