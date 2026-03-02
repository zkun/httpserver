// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:significant reason:default

#pragma once

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of QHttpServer. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include "qabstracthttpserver_p.h"

#include "qhttpserver.h"
#include "qhttpserverresponse.h"
#include "qhttpserverrequest.h"
#include "qhttpserverrouter.h"

#include <QtCore/qglobal.h>

#include <vector>

QT_BEGIN_NAMESPACE

class QHttpServerPrivate: public QAbstractHttpServerPrivate
{
    Q_DECLARE_PUBLIC(QHttpServer)

public:
    QHttpServerPrivate(QHttpServer *p);

    QHttpServerRouter router;
    struct AfterRequestHandler
    {
        QPointer<const QObject> context;
        QtPrivate::SlotObjUniquePtr slotObject;
    };
    std::vector<AfterRequestHandler> afterRequestHandlers;
    struct MissingHandler
    {
        QPointer<const QObject> context = nullptr;
        QtPrivate::SlotObjUniquePtr slotObject;
    } missingHandler;

    void callMissingHandler(const QHttpServerRequest &request, QHttpServerResponder &responder);
};

QT_END_NAMESPACE
