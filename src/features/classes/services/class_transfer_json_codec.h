#pragma once

#include "core/result.h"
#include "domain/models/class_transfer.h"

#include <QJsonObject>
#include <QString>

class ClassTransferJsonCodec
{
public:
    [[nodiscard]] static QJsonObject toJson(
        const ClassTransferPackage& package
        );

    [[nodiscard]] static Result<ClassTransferPackage> fromJson(
        const QJsonObject& object
        );

    [[nodiscard]] static Status saveFile(
        const QString& filePath,
        const ClassTransferPackage& package
        );

    [[nodiscard]] static Result<ClassTransferPackage> loadFile(
        const QString& filePath
        );
};
