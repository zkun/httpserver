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

#include "qhttpserverrequest.h"
#include "qabstracthttpserver.h"
#include "qhttpserverresponder.h"

#include "qabstracthttpserver_p.h"
#include "qhttpserverrequest_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qmetaobject.h>
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>

#include "http_parser.h"

#include <algorithm>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcHttpServer, "qt.httpserver")

QAbstractHttpServerPrivate::QAbstractHttpServerPrivate()
{
}

void QAbstractHttpServerPrivate::handleNewConnections()
{
    Q_Q(QAbstractHttpServer);
    auto tcpServer = qobject_cast<QTcpServer *>(q->sender());
    Q_ASSERT(tcpServer);
    while (auto socket = tcpServer->nextPendingConnection()) {
        auto request = new QHttpServerRequest(socket->peerAddress());  // TODO own tcp server could pre-allocate it

        QObject::connect(socket, &QTcpSocket::readyRead, q_ptr, [this, request, socket] () {
            handleReadyRead(socket, request);
        });

        QObject::connect(socket, &QTcpSocket::disconnected, socket, [request, socket] () {
            if (!request->d->handling)
                socket->deleteLater();
        });
        QObject::connect(socket, &QObject::destroyed, socket, [request] () {
            delete request;
        });
    }
}

void QAbstractHttpServerPrivate::handleReadyRead(QTcpSocket *socket,
                                                 QHttpServerRequest *request)
{
    Q_Q(QAbstractHttpServer);
    Q_ASSERT(socket);
    Q_ASSERT(request);

    if (!socket->isTransactionStarted())
        socket->startTransaction();

    if (request->d->state == QHttpServerRequestPrivate::State::OnMessageComplete)
        request->d->clear();

    if (!request->d->parse(socket)) {
        socket->disconnect();
        return;
    }

    if (!request->d->httpParser.upgrade && request->d->state != QHttpServerRequestPrivate::State::OnMessageComplete)
        return; // Partial read

    if (request->d->httpParser.upgrade && request->d->httpParser.method != HTTP_CONNECT) { // Upgrade
        const auto &upgradeValue = request->value(QByteArrayLiteral("upgrade"));
#if defined(QT_WEBSOCKETS_LIB)
        if (upgradeValue.compare(QByteArrayLiteral("websocket"), Qt::CaseInsensitive) == 0) {
            static const auto signal = QMetaMethod::fromSignal(
                        &QAbstractHttpServer::newWebSocketConnection);
            if (q->isSignalConnected(signal)) {
                QObject::disconnect(socket, &QTcpSocket::readyRead, nullptr, nullptr);
                socket->rollbackTransaction();
                websocketServer.handleConnection(socket);
                Q_EMIT socket->readyRead();
            } else {
                qWarning(lcHttpServer, "WebSocket received but no slots connected to "
                                       "QWebSocketServer::newConnection");
                socket->disconnectFromHost();
            }
            return;
        }
#endif
    }

    socket->commitTransaction();
    request->d->handling = true;
    if (!q->handleRequest(*request, socket))
        Q_EMIT q->missingHandler(*request, socket);
    request->d->handling = false;
    if (socket->state() == QAbstractSocket::UnconnectedState)
        socket->deleteLater();
}

QAbstractHttpServer::QAbstractHttpServer(QObject *parent)
    : QAbstractHttpServer(*new QAbstractHttpServerPrivate, parent)
{}

QAbstractHttpServer::QAbstractHttpServer(QAbstractHttpServerPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
#if defined(QT_WEBSOCKETS_LIB)
    Q_D(QAbstractHttpServer);
    connect(&d->websocketServer, &QWebSocketServer::newConnection,
            this, &QAbstractHttpServer::newWebSocketConnection);
#endif
}

/*!
    Tries to bind a \c QTcpServer to \a address and \a port.

    Returns the server port upon success, -1 otherwise.
*/
quint16 QAbstractHttpServer::listen(const QHostAddress &address, quint16 port)
{
    auto tcpServer = new QTcpServer(this);
    const auto listening = tcpServer->listen(address, port);
    if (listening) {
        bind(tcpServer);
        return tcpServer->serverPort();
    } else {
        qCCritical(lcHttpServer, "listen failed: %s",
                   tcpServer->errorString().toStdString().c_str());
    }

    delete tcpServer;
    return 0;
}

/*!
    Bind the HTTP server to given TCP \a server over which
    the transmission happens. It is possible to call this function
    multiple times with different instances of TCP \a server to
    handle multiple connections and ports, for example both SSL and
    non-encrypted connections.

    After calling this function, every _new_ connection will be
    handled and forwarded by the HTTP server.

    It is the user's responsibility to call QTcpServer::listen() on
    the \a server.

    If the \a server is null, then a new, default-constructed TCP
    server will be constructed, which will be listening on a random
    port and all interfaces.

    The \a server will be parented to this HTTP server.

    \sa QTcpServer, QTcpServer::listen()
*/
void QAbstractHttpServer::bind(QTcpServer *server)
{
    Q_D(QAbstractHttpServer);
    if (!server) {
        server = new QTcpServer(this);
        if (!server->listen()) {
            qCCritical(lcHttpServer, "QTcpServer listen failed (%s)",
                       qPrintable(server->errorString()));
        }
    } else {
        if (!server->isListening())
            qCWarning(lcHttpServer) << "The TCP server" << server << "is not listening.";
        server->setParent(this);
    }
    QObjectPrivate::connect(server, &QTcpServer::newConnection,
                            d, &QAbstractHttpServerPrivate::handleNewConnections,
                            Qt::UniqueConnection);
}

/*!
    Returns list of child TCP servers of this HTTP server.
 */
QVector<QTcpServer *> QAbstractHttpServer::servers() const
{
    return findChildren<QTcpServer *>().toVector();
}

#if defined(QT_WEBSOCKETS_LIB)
/*!
    \fn QAbstractHttpServer::newConnection
    This signal is emitted every time a new WebSocket connection is
    available.

    \sa hasPendingWebSocketConnections(), nextPendingWebSocketConnection()
*/

/*!
    Returns \c true if the server has pending WebSocket connections;
    otherwise returns \c false.

    \sa newWebSocketConnection(), nextPendingWebSocketConnection()
*/
bool QAbstractHttpServer::hasPendingWebSocketConnections() const
{
    Q_D(const QAbstractHttpServer);
    return d->websocketServer.hasPendingConnections();
}

/*!
    Returns the next pending connection as a connected QWebSocket
    object. QAbstractHttpServer does not take ownership of the
    returned QWebSocket object. It is up to the caller to delete the
    object explicitly when it will no longer be used, otherwise a
    memory leak will occur. \c nullptr is returned if this function
    is called when there are no pending connections.

    \note The returned QWebSocket object cannot be used from another
    thread.

    \sa newWebSocketConnection(), hasPendingWebSocketConnections()
*/
QWebSocket *QAbstractHttpServer::nextPendingWebSocketConnection()
{
    Q_D(QAbstractHttpServer);
    return d->websocketServer.nextPendingConnection();
}
#endif

QHttpServerResponder QAbstractHttpServer::makeResponder(const QHttpServerRequest &request,
                                                        QTcpSocket *socket)
{
    return QHttpServerResponder(request, socket);
}

QT_END_NAMESPACE
