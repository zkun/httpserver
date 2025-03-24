/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtHttpServer module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qhttpserverresponder.h"

#include "qhttpserverrequest_p.h"
#include "qabstracthttpserver_p.h"

#include <QTcpSocket>
#include <QMetaMethod>
#include <QtCore/qloggingcategory.h>
#include <QtNetwork/qtcpserver.h>

#include <algorithm>
#include <functional>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcHttpServer, "qt.httpserver")

QAbstractHttpServerPrivate::QAbstractHttpServerPrivate()
{
}

QAbstractHttpServer::QAbstractHttpServer(QObject *parent)
    : QAbstractHttpServer(*new QAbstractHttpServerPrivate, parent)
{}

QAbstractHttpServer::~QAbstractHttpServer()
    = default;

QAbstractHttpServer::QAbstractHttpServer(QAbstractHttpServerPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
#if defined(QT_WEBSOCKETS_LIB)
    Q_D(QAbstractHttpServer);
    connect(&d->websocketServer, &QWebSocketServer::newConnection, this, &QAbstractHttpServer::newWebSocketConnection);
#endif
}

void QAbstractHttpServer::handleNewConnections()
{
    auto server = qobject_cast<QTcpServer *>(sender());
    if (server == nullptr)
        return;

    while (auto socket = server->nextPendingConnection()) {
        auto request = new QHttpServerRequest(socket);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, std::bind(&QAbstractHttpServer::handleReadyRead, this, socket, request));
    }
}
void QAbstractHttpServer::handleReadyRead(QTcpSocket *socket, QHttpServerRequest *request)
{
    Q_D(QAbstractHttpServer);
    Q_ASSERT(socket);
    Q_ASSERT(request);

    if (!socket->isTransactionStarted())
        socket->startTransaction();

    if (!request->d->parse(socket->readAll())) {
        socket->disconnectFromHost();
        return;
    }

    if (request->d->state != QHttpServerRequestPrivate::State::OnMessageComplete)
        return; // Partial read

#if defined(QT_WEBSOCKETS_LIB)
    if (request->d->httpParser.upgrade) { // Upgrade
        const auto &upgradeValue = request->value(QByteArrayLiteral("upgrade"));
        if (upgradeValue.compare(QByteArrayLiteral("websocket"), Qt::CaseInsensitive) == 0) {
            static const auto signal = QMetaMethod::fromSignal(&QAbstractHttpServer::newWebSocketConnection);
            if (isSignalConnected(signal)) {
                // Socket will now be managed by websocketServer
                socket->disconnect();
                socket->rollbackTransaction();
                // And we can delete the request, as it's no longer used
                request->deleteLater();
                d->websocketServer.handleConnection(socket);
                Q_EMIT socket->readyRead();
            } else {
                socket->disconnectFromHost();
            }
            return;
        }
    }
#endif
    auto responder = std::make_unique<QHttpServerResponder>(socket);

    socket->commitTransaction();
    request->d->handling = true;
    handleRequest(*request, std::move(responder));
    request->d->handling = false;

    if (socket->bytesAvailable() > 0)
        QMetaObject::invokeMethod(socket, &QAbstractSocket::readyRead, Qt::QueuedConnection);
}

/*!
    Tries to bind a \c QTcpServer to \a address and \a port.

    Returns the server port upon success, -1 otherwise.
*/
quint16 QAbstractHttpServer::listen(const QHostAddress &address, quint16 port)
{
    auto server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, &QAbstractHttpServer::handleNewConnections, Qt::UniqueConnection);

    if (server->listen(address, port))
        return server->serverPort();

    qCritical() << "listen failed: " << server->errorString();
    server->deleteLater();
    return 0;
}

QList<quint16> QAbstractHttpServer::serverPorts() const
{
    QList<quint16> ports;
    auto children = findChildren<QTcpServer *>();
    ports.reserve(children.count());
    std::transform(children.cbegin(), children.cend(), std::back_inserter(ports),
                   [](const QTcpServer *server) { return server->serverPort(); });
    return ports;
}


#if defined(QT_WEBSOCKETS_LIB)
bool QAbstractHttpServer::hasPendingWebSocketConnections() const
{
    Q_D(const QAbstractHttpServer);
    return d->websocketServer.hasPendingConnections();
}

std::unique_ptr<QWebSocket> QAbstractHttpServer::nextPendingWebSocketConnection()
{
    Q_D(QAbstractHttpServer);
    return std::unique_ptr<QWebSocket>(d->websocketServer.nextPendingConnection());
}
#endif

QT_END_NAMESPACE
