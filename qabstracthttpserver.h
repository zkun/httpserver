// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:significant reason:default

#pragma once

#include <QtCore/qobject.h>

#include <QtCore/qglobal.h>
#include "qhttpserverwebsocketupgraderesponse.h"
#include "qhttpserverconfiguration.h"

#include <QtNetwork/qhostaddress.h>

#if QT_CONFIG(ssl)
#include <QtNetwork/qhttp2configuration.h>
#endif

#include "qwebsocket.h"

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE

class QHttpServerRequest;
class QHttpServerResponder;
class QLocalServer;
class QTcpServer;

class QAbstractHttpServerPrivate;
class QAbstractHttpServer : public QObject
{
    Q_OBJECT
    friend class QHttpServerHttp1ProtocolHandler;
    friend class QHttpServerHttp2ProtocolHandler;

public:
    explicit QAbstractHttpServer(QObject *parent = nullptr);
    ~QAbstractHttpServer() override;

    QList<quint16> serverPorts() const;
    bool bind(QTcpServer *server);
    QList<QTcpServer *> servers() const;

#if QT_CONFIG(localserver)
    bool bind(QLocalServer *server);
    QList<QLocalServer *> localServers() const;
#endif

#if QT_CONFIG(ssl)
    QHttp2Configuration http2Configuration() const;
    void setHttp2Configuration(const QHttp2Configuration &configuration);
#endif

    void setConfiguration(const QHttpServerConfiguration &config);
    QHttpServerConfiguration configuration() const;

Q_SIGNALS:
    void newWebSocketConnection();

private:
    using WebSocketUpgradeVerifierPrototype =
            QHttpServerWebSocketUpgradeResponse (*)(const QHttpServerRequest &request);
    template <typename T>
    using if_compatible_callable = typename std::enable_if<
            QtPrivate::AreFunctionsCompatible<WebSocketUpgradeVerifierPrototype, T>::value,
            bool>::type;

    void addWebSocketUpgradeVerifierImpl(const QObject *context,
                                         QtPrivate::QSlotObjectBase *slotObjRaw);

public:
    bool hasPendingWebSocketConnections() const;
    std::unique_ptr<QWebSocket> nextPendingWebSocketConnection();

#ifdef Q_QDOC
    template <typename Handler>
    void addWebSocketUpgradeVerifier(const QObject *context, Handler &&func)
#else
    template <typename Handler, if_compatible_callable<Handler> = true>
    void addWebSocketUpgradeVerifier(
            const typename QtPrivate::ContextTypeForFunctor<Handler>::ContextType *context,
            Handler &&func)
#endif
    {
        addWebSocketUpgradeVerifierImpl(
                context,
                QtPrivate::makeCallableObject<WebSocketUpgradeVerifierPrototype>(
                        std::forward<Handler>(func)));
    }

private:
    QHttpServerWebSocketUpgradeResponse
    verifyWebSocketUpgrade(const QHttpServerRequest &request) const;

protected:
    QAbstractHttpServer(QAbstractHttpServerPrivate &dd, QObject *parent = nullptr);

    virtual bool handleRequest(const QHttpServerRequest &request,
                               QHttpServerResponder &responder) = 0;
    virtual void missingHandler(const QHttpServerRequest &request,
                                QHttpServerResponder &responder) = 0;

private:
    Q_DECLARE_PRIVATE(QAbstractHttpServer)
};

QT_END_NAMESPACE
