#pragma once

#include <QApplication>
#include <QClipboard>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

namespace ClassMngr::Next::Bootstrap::Tests
{

class CapturedSpeakingEvaluationAiUrlHandler final : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> urls;
    QString clipboardTextOnOpen;

public slots:
    void capture(const QUrl& url)
    {
        urls.append(url);
        clipboardTextOnOpen = QApplication::clipboard()->text();
    }
};

}  // namespace ClassMngr::Next::Bootstrap::Tests
