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

#include "qabstracthttpserver.h"
#include <QtCore/qglobal.h>
#include "qhttpserverconfiguration.h"
#include "qhttpserverrequestfilter_p.h"

#include <private/qobject_p.h>

#include <QtCore/qcoreapplication.h>

#include <vector>

#include "qwebsocketserver.h"

#if QT_CONFIG(ssl)
#include <QtNetwork/qhttp2configuration.h>
#endif

QT_BEGIN_NAMESPACE

class QHttpServerRequest;

class QAbstractHttpServerPrivate: public QObjectPrivate
{
public:
    Q_DECLARE_PUBLIC(QAbstractHttpServer)

    QAbstractHttpServerPrivate();

    QWebSocketServer websocketServer {
        QCoreApplication::applicationName() + QLatin1Char('/') + QCoreApplication::applicationVersion(),
        QWebSocketServer::NonSecureMode
    };

    void handleNewConnections();
    bool verifyThreadAffinity(const QObject *contextObject) const;

#if QT_CONFIG(localserver)
    void handleNewLocalConnections();
#endif

    mutable bool handlingWebSocketUpgrade = false;
    struct WebSocketUpgradeVerifier
    {
        QPointer<const QObject> context;
        QtPrivate::SlotObjUniquePtr slotObject;
    };
    std::vector<WebSocketUpgradeVerifier> webSocketUpgradeVerifiers;
#if QT_CONFIG(ssl)
    QHttp2Configuration h2Configuration;
#endif
    QHttpServerConfiguration configuration;
    QHttpServerRequestFilter requestFilter;
};

QT_END_NAMESPACE
