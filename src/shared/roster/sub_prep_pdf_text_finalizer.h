#pragma once

#include <QByteArray>
#include <QList>
#include <QPaintDevice>
#include <QString>
#include <expected>
#include <memory>

class PdfGenerationSession;

// Internal, operation-scoped support for the Qt-generated Sub Prep PDF path.
// This is deliberately not a general PDF repair API.
namespace SubPrepPdfText {
struct SourceJournal {
    QList<QList<QString>> pages{QList<QString>{}};
    QString error;
};

// Forwards the original paint operations/text items to the existing writer.
// Captures actual QTextItem source strings rather than extracting PDF text.
class CaptureDevice final : public QPaintDevice {
public:
    CaptureDevice(PdfGenerationSession& writer, SourceJournal& journal);
    ~CaptureDevice() override;
    QPaintEngine* paintEngine() const override;
    bool newPage();
    bool finish();
protected:
    int metric(PaintDeviceMetric metric) const override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Supports the classic-xref, unencrypted, embedded Identity-H format emitted
// by Qt 6.12. Changes only ToUnicode objects plus required xref/trailer data.
// Unsupported structures, shaping, and conflicting source aliases fail closed.
std::expected<QByteArray, QString> finalize(
    const QByteArray& generatedPdf, const SourceJournal& source);
}
