/*
    SPDX-FileCopyrightText: 2026 KWin Authors

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWin
{

class Display;
class KdeToplevelHintsManagerV1Private;

class KWIN_EXPORT KdeToplevelHintsManagerV1 : public QObject
{
    Q_OBJECT
public:
    explicit KdeToplevelHintsManagerV1(Display *display, QObject *parent = nullptr);
    ~KdeToplevelHintsManagerV1() override;

private:
    std::unique_ptr<KdeToplevelHintsManagerV1Private> d;
};

} // namespace KWin
