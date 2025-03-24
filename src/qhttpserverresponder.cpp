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

#include "qhttpserverrequest_p.h"
#include "qhttpserverliterals.h"
#include "qhttpserverresponse_p.h"
#include "qhttpserverresponder_p.h"

#include <QtCore/qtimer.h>
#include <QtCore/qjsondocument.h>
#include <QtNetwork/qtcpsocket.h>

#include <map>
#include <memory>

#include "http_parser.h"

QT_BEGIN_NAMESPACE

// https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
static const std::map<QHttpServerResponder::StatusCode, QByteArray> statusString {
#define XX(num, name, string) { static_cast<QHttpServerResponder::StatusCode>(num), QByteArrayLiteral(#string) },
  HTTP_STATUS_MAP(XX)
#undef XX
};

QHttpServerResponder::QHttpServerResponder(QTcpSocket *socket) :
    d_ptr(new QHttpServerResponderPrivate(socket))
{
    Q_ASSERT(socket);
}

/*!
    Move-constructs a QHttpServerResponder instance, making it point
    at the same object that \a other was pointing to.
*/
QHttpServerResponder::QHttpServerResponder(QHttpServerResponder &&other)
    : d_ptr(std::move(other.d_ptr))
{}

/*!
    Destroys a QHttpServerResponder.
*/
QHttpServerResponder::~QHttpServerResponder()
{}

/*!
    Answers a request with an HTTP status code \a status, JSON
    document \a document and HTTP headers \a headers.

    Note: This function sets HTTP Content-Type header as "application/json".
*/
void QHttpServerResponder::write(const QJsonDocument &document, HeaderList headers, StatusCode status)
{
    const QByteArray &json = document.toJson();

    writeStatusLine(status);
    writeHeader(QHttpServerLiterals::contentTypeHeader, QHttpServerLiterals::contentTypeJson);
    writeHeader(QHttpServerLiterals::contentLengthHeader, QByteArray::number(json.size()));
    writeHeaders(std::move(headers));
    writeBody(document.toJson());
}

/*!
    Answers a request with an HTTP status code \a status, and JSON
    document \a document.

    Note: This function sets HTTP Content-Type header as "application/json".
*/
void QHttpServerResponder::write(const QJsonDocument &document, StatusCode status)
{
    write(document, {}, status);
}

/*!
    Answers a request with an HTTP status code \a status,
    HTTP Headers \a headers and a body \a data.

    Note: This function sets HTTP Content-Length header.
*/
void QHttpServerResponder::write(const QByteArray &data, HeaderList headers, StatusCode status)
{
    writeStatusLine(status);

    for (auto &&header : headers)
        writeHeader(header.first, header.second);

    writeHeader(QHttpServerLiterals::contentLengthHeader, QByteArray::number(data.size()));
    writeBody(data);
}

/*!
    Answers a request with an HTTP status code \a status, a
    MIME type \a mimeType and a body \a data.
*/
void QHttpServerResponder::write(const QByteArray &data, const QByteArray &mimeType, StatusCode status)
{
    write(data, {{ QHttpServerLiterals::contentTypeHeader, mimeType }}, status);
}

/*!
    Answers a request with an HTTP status code \a status.

    Note: This function sets HTTP Content-Type header as "application/x-empty".
*/
void QHttpServerResponder::write(StatusCode status)
{
    write(QByteArray(), QHttpServerLiterals::contentTypeXEmpty, status);
}

/*!
    Answers a request with an HTTP status code \a status and
    HTTP Headers \a headers.
*/
void QHttpServerResponder::write(HeaderList headers, StatusCode status)
{
    write(QByteArray(), std::move(headers), status);
}

/*!
    This function writes HTTP status line with an HTTP status code \a status
    and an HTTP version \a version.
*/
void QHttpServerResponder::writeStatusLine(StatusCode status)
{
    Q_D(QHttpServerResponder);
    Q_ASSERT(d->socket->isOpen());
    d->bodyStarted = false;
    d->socket->write("HTTP/1.0 ");
    d->socket->write(QByteArray::number(quint32(status)));
    d->socket->write(" ");
    d->socket->write(statusString.at(status));
    d->socket->write("\r\n");
}

/*!
    This function writes an HTTP header \a header
    with \a value.
*/
void QHttpServerResponder::writeHeader(const QByteArray &header,
                                       const QByteArray &value)
{
    Q_D(const QHttpServerResponder);
    Q_ASSERT(d->socket->isOpen());
    d->socket->write(header);
    d->socket->write(": ");
    d->socket->write(value);
    d->socket->write("\r\n");
}

/*!
    This function writes HTTP headers \a headers.
*/
void QHttpServerResponder::writeHeaders(HeaderList headers)
{
    for (auto &&header : headers)
        writeHeader(header.first, header.second);
}

/*!
    This function writes HTTP body \a body with size \a size.
*/
void QHttpServerResponder::writeBody(const char *body, qint64 size)
{
    Q_D(QHttpServerResponder);
    Q_ASSERT(d->socket->isOpen());

    if (!d->bodyStarted) {
        d->bodyStarted = true;
        d->socket->write("\r\n");
    }

    d->socket->write(body, size);
}

void QHttpServerResponder::writeBody(const char *body)
{
    writeBody(body, qstrlen(body));
}

void QHttpServerResponder::writeBody(const QByteArray &body)
{
    writeBody(body.constData(), body.size());
}

void QHttpServerResponder::sendResponse(const QHttpServerResponse &response)
{
    const auto &d = response.d_ptr;

    writeStatusLine(d->statusCode);

    for (auto &&header : d->headers)
        writeHeader(header.first, header.second);

    writeHeader(QHttpServerLiterals::contentLengthHeader, QByteArray::number(d->data.size()));

    writeBody(d->data);
}

QT_END_NAMESPACE
