// Private orchestration and preview helpers.


QStringList daysWithValues(
    TemplateId templateId,
    const QList<RosterCellValue>& values
    )
{
    if (templateId == TemplateId::PerClassWithExtraInfo)
    {
        QStringList pages;

        for (const RosterCellValue& value : values)
        {
            if (!pages.contains(value.day))
            {
                pages.append(value.day);
            }
        }

        return pages;
    }

    QList<QString> included;

    for (const RosterCellValue& value : values)
    {
        if (
            DaySheets.contains(baseDayName(value.day))
            && !included.contains(value.day)
            )
        {
            included.append(value.day);
        }
    }

    QStringList days;
    for (const QString& day : DaySheets)
    {
        QList<QString> matchingPages;
        for (const QString& includedDay : std::as_const(included))
        {
            if (baseDayName(includedDay) == day)
            {
                matchingPages.append(includedDay);
            }
        }

        std::sort(
            matchingPages.begin(),
            matchingPages.end(),
            [](const QString& left, const QString& right)
            {
                return dailyPageIndex(left) < dailyPageIndex(right);
            }
            );

        days.append(matchingPages);
    }

    return days;
}

::Result<QList<RosterClassData>> loadRosterClassData(
    ClassService* classService,
    const ClassMngr::Next::Application::RosterReadPort* rosterReadPort,
    const QList<int>& classIds
    )
{
    QList<RosterClassData> result;

    if (!classService || !rosterReadPort)
    {
        return result;
    }

    for (int classId : classIds)
    {
        RosterClassData data;
        const ::Result<Classroom> classroom =
            classService->classroom(classId);
        if (!classroom)
        {
            return std::unexpected(classroom.error());
        }
        data.classroom = *classroom;
        const ::Result<ClassInfo> info =
            classService->classInfo(classId);
        if (!info)
        {
            return std::unexpected(info.error());
        }

        const auto typedClassId =
            ClassMngr::Next::Domain::ClassId::fromString(
                std::to_string(classId)
                );
        if (!typedClassId)
        {
            return std::unexpected(
                QObject::tr("Roster data is not available.")
                );
        }

        const ClassMngr::Next::Application::RosterReadResult roster =
            ClassMngr::Next::Application::RosterReadUseCase::execute(
                {
                    .classId = *typedClassId
                },
                *rosterReadPort
                );
        if (!roster)
        {
            const std::string& message = roster.error().message;
            return std::unexpected(
                message.empty()
                    ? QObject::tr("Roster data is not available.")
                    : QString::fromUtf8(
                        message.data(),
                        static_cast<qsizetype>(message.size())
                        )
                );
        }

        data.info = *info;
        const ClassMngr::Next::Application::RosterSnapshot& snapshot =
            roster.value();
        data.roster.columns.reserve(
            static_cast<qsizetype>(snapshot.columns.size())
            );
        for (const std::u16string& column : snapshot.columns)
        {
            data.roster.columns.append(
                QString::fromStdU16String(column)
                );
        }

        data.roster.columnWidths.reserve(
            static_cast<qsizetype>(snapshot.columnWidths.size())
            );
        for (const int width : snapshot.columnWidths)
        {
            data.roster.columnWidths.append(width);
        }

        data.roster.rows.reserve(
            static_cast<qsizetype>(snapshot.rows.size())
            );
        for (const std::vector<std::u16string>& snapshotRow : snapshot.rows)
        {
            QStringList row;
            row.reserve(static_cast<qsizetype>(snapshotRow.size()));
            for (const std::u16string& cell : snapshotRow)
            {
                row.append(QString::fromStdU16String(cell));
            }
            data.roster.rows.append(std::move(row));
        }

        if (data.classroom.id > 0)
        {
            result.append(data);
        }
    }

    return result;
}

Result loadRosterClassesForRequest(
    const Request& request,
    QList<RosterClassData>* rosterClasses
    )
{
    if (!request.services || !request.services->hasOpenDatabase())
    {
        return failed(QObject::tr("No Teacher Profile is open."));
    }

    ClassService* classService =
        request.services->classService();
    if (!classService || !request.rosterReadPort)
    {
        return failed(QObject::tr("Roster data is not available."));
    }

    if (!request.classesListReadQuery)
    {
        return failed(QObject::tr("Classes could not be loaded."));
    }

    const ClassMngr::Next::Application::ClassesListReadResult classes =
        request.classesListReadQuery->execute();
    if (!classes)
    {
        const std::string& message = classes.error().message;
        return failed(
            message.empty()
                ? QObject::tr("Classes could not be loaded.")
                : QString::fromUtf8(
                    message.data(),
                    static_cast<qsizetype>(message.size())
                    )
            );
    }

    QList<Classroom> classrooms;
    classrooms.reserve(
        static_cast<qsizetype>(classes.value().classes.size())
        );
    for (const auto& entry : classes.value().classes)
    {
        int classId = 0;
        const std::string& classIdValue = entry.classId.value();
        const auto [end, error] = std::from_chars(
            classIdValue.data(),
            classIdValue.data() + classIdValue.size(),
            classId
            );
        if (
            error != std::errc{}
            || end != classIdValue.data() + classIdValue.size()
            || classId <= 0
            )
        {
            return failed(QObject::tr("Classes could not be loaded."));
        }

        Classroom classroom;
        classroom.id = classId;
        classroom.name = QString::fromStdU16String(entry.className);
        classrooms.append(std::move(classroom));
    }

    const QList<int> classIds =
        resolveClassIds(
            request.scope,
            request.currentClassId,
            request.selectedClassIds,
            classrooms
            );

    if (classIds.isEmpty())
    {
        return failed(QObject::tr("No classes were selected for printing."));
    }

    if (rosterClasses)
    {
        const ::Result<QList<RosterClassData>> loadedRosterClasses =
            loadRosterClassData(
                classService,
                request.rosterReadPort,
                classIds
                );
        if (!loadedRosterClasses)
        {
            return failed(loadedRosterClasses.error());
        }
        *rosterClasses = *loadedRosterClasses;
    }

    return sent();
}

QList<RosterCellValue> buildTemplateCellValues(
    TemplateId templateId,
    const QList<RosterClassData>& classes,
    const QStringList& selectedExtraColumns,
    QPageLayout::Orientation perClassExtraInfoOrientation,
    QString* errorMessage
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        return buildPerClassExtraInfoCellValues(
            classes,
            selectedExtraColumns,
            perClassExtraInfoOrientation,
            errorMessage
            );

    case TemplateId::Daily:
        return buildDailyCellValues(
            classes,
            errorMessage
            );

    case TemplateId::ByDay:
    default:
        return buildByDayCellValues(
            classes,
            errorMessage
            );
    }
}

void paintTemplateDay(
    TemplateId templateId,
    QPainter& painter,
    const QString& day,
    const QHash<QString, QString>& values,
    const QRectF& pageRect,
    const QRectF& contentRect,
    int resolutionDpi
    )
{
    switch (templateId)
    {
    case TemplateId::PerClassWithExtraInfo:
        Q_UNUSED(day);
        paintPerClassWithExtraInfoPage(
            painter,
            values,
            pageRect,
            contentRect,
            resolutionDpi
            );
        break;

    case TemplateId::Daily:
        paintDailyDay(
            painter,
            day,
            values,
            pageRect,
            contentRect,
            resolutionDpi
            );
        break;

    case TemplateId::ByDay:
    default:
        paintRosterDay(
            painter,
            day,
            values,
            pageRect,
            contentRect,
            resolutionDpi
            );
        break;
    }
}

QSize previewPagePixelSize(
    TemplateId templateId,
    const QSize& requestedSize,
    QPageLayout::Orientation orientation
    )
{
    const QSize fallbackSize(
        420,
        260
        );
    const QSize boundedSize =
        requestedSize.isValid()
            ? requestedSize
            : fallbackSize;

    const bool landscape =
        orientation == QPageLayout::Landscape;
    const qreal pageWidth =
        landscape
            ? A4HeightInches
            : A4WidthInches;
    const qreal pageHeight =
        landscape
            ? A4WidthInches
            : A4HeightInches;
    const qreal scale =
        std::min(
            boundedSize.width() / pageWidth,
            boundedSize.height() / pageHeight
            );

    return QSize(
        std::max(
            1,
            qRound(pageWidth * scale)
            ),
        std::max(
            1,
            qRound(pageHeight * scale)
            )
        );
}

QImage renderPreviewImage(
    TemplateId templateId,
    const QSize& requestedSize,
    const QString& day,
    const QHash<QString, QString>& values,
    QPageLayout::Orientation orientation
    )
{
    const QSize pageSize =
        previewPagePixelSize(
            templateId,
            requestedSize,
            orientation
            );
    QImage image(
        pageSize,
        QImage::Format_ARGB32_Premultiplied
        );
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(
        QPainter::Antialiasing,
        false
        );
    painter.setRenderHint(
        QPainter::TextAntialiasing,
        true
        );

    const bool landscape =
        orientation == QPageLayout::Landscape;
    const qreal pageWidthInches =
        landscape
            ? A4HeightInches
            : A4WidthInches;
    const int resolutionDpi =
        std::max(
            1,
            qRound(image.width() / pageWidthInches)
            );
    const QRectF pageRect(
        0.0,
        0.0,
        image.width(),
        image.height()
        );
    const QRectF contentRect =
        templateContentRect(
            templateId,
            pageRect,
            resolutionDpi
            );

    paintTemplateDay(
        templateId,
        painter,
        day,
        values,
        pageRect,
        contentRect,
        resolutionDpi
        );

    painter.end();
    return image;
}
