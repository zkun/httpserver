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

#ifndef QABSTRACTHTTPSERVER_H
#define QABSTRACTHTTPSERVER_H

#include <QtCore/qobject.h>

#include <QtNetwork/qhostaddress.h>

#if defined(QT_WEBSOCKETS_LIB)
#include <QtWebSockets/qwebsocket.h>
#endif // defined(QT_WEBSOCKETS_LIB)

#include <memory>

QT_BEGIN_NAMESPACE

class QTcpSocket;
class QTcpServer;
class QHttpServerRequest;
class QHttpServerResponder;

class QAbstractHttpServerPrivate;
class QAbstractHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit QAbstractHttpServer(QObject *parent = nullptr);
    ~QAbstractHttpServer() override;

    quint16 listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 0);
    QList<quint16> serverPorts() const;

#if defined(QT_WEBSOCKETS_LIB)
    bool hasPendingWebSocketConnections() const;
    std::unique_ptr<QWebSocket> nextPendingWebSocketConnection();

Q_SIGNALS:
    void newWebSocketConnection();
#endif // defined(QT_WEBSOCKETS_LIB)

protected:
    QAbstractHttpServer(QAbstractHttpServerPrivate &dd, QObject *parent = nullptr);

    virtual void handleRequest(const QHttpServerRequest &request, std::unique_ptr<QHttpServerResponder> responder) = 0;

private slots:
    void handleNewConnections();
    void handleReadyRead(QTcpSocket *socket, QHttpServerRequest *request);

private:
    Q_DECLARE_PRIVATE(QAbstractHttpServer)
};

QT_END_NAMESPACE

#endif // QABSTRACTHTTPSERVER_H
