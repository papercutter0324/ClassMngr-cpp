#pragma once

#include <QPageLayout>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QRect>
#include <QString>
#include <memory>
#include <optional>

class QIODevice;

struct PdfGenerationSpec
{
    std::optional<QString> creator;
    std::optional<QString> title;
    std::optional<int> resolutionDpi;
    std::optional<QPageLayout> pageLayout;
};

// One document operation. End every painter before destroying the session;
// destroying it completes the PDF stream before the caller commits its output.
class PdfGenerationSession
{
public:
    virtual ~PdfGenerationSession() = default;
    [[nodiscard]] virtual QPaintDevice& paintDevice() = 0;
    [[nodiscard]] virtual QPageLayout pageLayout() const = 0;
    [[nodiscard]] virtual int resolution() const = 0;
    [[nodiscard]] virtual QRect pageRectPixels() const = 0;
    // Queries a separate live engine without beginning painting on this document.
    [[nodiscard]] virtual QPaintEngine::PaintEngineFeatures paintEngineFeatures() const = 0;
    virtual bool newPage() = 0;
};

class PdfGenerationPort
{
public:
    virtual ~PdfGenerationPort() = default;
    // A null session means the supplied page layout could not be configured.
    [[nodiscard]] virtual std::unique_ptr<PdfGenerationSession> create(
        const QString& outputPath, const PdfGenerationSpec& spec) = 0;
    // The open device remains caller-owned and must outlive the session.
    [[nodiscard]] virtual std::unique_ptr<PdfGenerationSession> create(
        QIODevice& output, const PdfGenerationSpec& spec) = 0;
};
