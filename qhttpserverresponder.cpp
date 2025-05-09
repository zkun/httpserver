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
#include "qhttpserverresponse.h"
#include "qhttpserverrequest.h"

#include "qhttpserverresponder_p.h"
#include "qhttpserverliterals_p.h"
#include "qhttpserverresponse_p.h"
#include "qhttpserverrequest_p.h"
#include "qhttpserverstream_p.h"

#include <QtCore/qjsondocument.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qtcpsocket.h>
#include <map>
#include <memory>

#include "http_parser.h"

QT_BEGIN_NAMESPACE

static const QLoggingCategory &lc()
{
    static const QLoggingCategory category("qt.httpserver.response");
    return category;
}

// https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
static const std::map<QHttpServerResponder::StatusCode, QByteArray> statusString {
#define XX(num, name, string) { static_cast<QHttpServerResponder::StatusCode>(num), QByteArrayLiteral(#string) },
  HTTP_STATUS_MAP(XX)
#undef XX
};

template <qint64 BUFFERSIZE = 128 * 1024>
struct IOChunkedTransfer
{
    // TODO This is not the fastest implementation, as it does read & write
    // in a sequential fashion, but these operation could potentially overlap.
    // TODO Can we implement it without the buffer? Direct write to the target buffer
    // would be great.

    const qint64 bufferSize = BUFFERSIZE;
    char buffer[BUFFERSIZE];
    qint64 beginIndex = -1;
    qint64 endIndex = -1;
    QPointer<QIODevice> source;
    const QPointer<QIODevice> sink;
    const QMetaObject::Connection bytesWrittenConnection;
    const QMetaObject::Connection readyReadConnection;
    IOChunkedTransfer(QIODevice *input, QIODevice *output) :
        source(input),
        sink(output),
        bytesWrittenConnection(QObject::connect(sink.data(), &QIODevice::bytesWritten, sink.data(), [this] () {
              writeToOutput();
        })),
        readyReadConnection(QObject::connect(source.data(), &QIODevice::readyRead, source.data(), [this] () {
            readFromInput();
        }))
    {
        Q_ASSERT(!source->atEnd());  // TODO error out
        QObject::connect(sink.data(), &QObject::destroyed, source.data(), &QObject::deleteLater);
        QObject::connect(source.data(), &QObject::destroyed, source.data(), [this] () {
            delete this;
        });
        readFromInput();
    }

    ~IOChunkedTransfer()
    {
        QObject::disconnect(bytesWrittenConnection);
        QObject::disconnect(readyReadConnection);
    }

    inline bool isBufferEmpty()
    {
        Q_ASSERT(beginIndex <= endIndex);
        return beginIndex == endIndex;
    }

    void readFromInput()
    {
        if (source.isNull() || !isBufferEmpty()) // We haven't consumed all the data yet.
            return;

        beginIndex = 0;
        endIndex = source->read(buffer, bufferSize);
        if (endIndex < 0) {
            endIndex = beginIndex; // Mark the buffer as empty
            qCWarning(lc, "Error reading chunk: %s", qPrintable(source->errorString()));
        } else if (endIndex) {
            memset(buffer + endIndex, 0, sizeof(buffer) - std::size_t(endIndex));
            writeToOutput();
        }
    }

    void writeToOutput()
    {
        if (sink.isNull() || source.isNull() || isBufferEmpty())
            return;

        const auto writtenBytes = sink->write(buffer + beginIndex, endIndex);
        if (writtenBytes < 0) {
            qCWarning(lc, "Error writing chunk: %s", qPrintable(sink->errorString()));
            return;
        }
        beginIndex += writtenBytes;
        if (isBufferEmpty()) {
            if (source->bytesAvailable())
                QTimer::singleShot(0, source.data(), [this]() { readFromInput(); });
            else if (source->atEnd()) // Finishing
                source->deleteLater();
        }
    }
};

/*!
    Constructs a QHttpServerResponder using the stream \a stream
    and the socket \a socket.
*/
QHttpServerResponder::QHttpServerResponder(QHttpServerStream *stream) :
    d_ptr(new QHttpServerResponderPrivate(stream))
{
    Q_ASSERT(stream);
    Q_ASSERT(!stream->handlingRequest);

    stream->handlingRequest = true;
}

/*!
    Move-constructs a QHttpServerResponder instance, making it point
    at the same object that \a other was pointing to.
*/
QHttpServerResponder::QHttpServerResponder(QHttpServerResponder &&other)
  : d_ptr(std::move(other.d_ptr))
{}

QHttpServerResponder::~QHttpServerResponder()
{
    Q_D(QHttpServerResponder);
    if (d) {
        Q_ASSERT(d->stream);
        d->stream->responderDestroyed();
    }
}

/*!
    Answers a request with an HTTP status code \a status and
    HTTP headers \a headers. The I/O device \a data provides the body
    of the response. If \a data is sequential, the body of the
    message is sent in chunks: otherwise, the function assumes all
    the content is available and sends it all at once but the read
    is done in chunks.

    \note This function takes the ownership of \a data.
*/
void QHttpServerResponder::write(QIODevice *data, HeaderList headers, StatusCode status)
{
    Q_D(QHttpServerResponder);
    Q_ASSERT(d->stream);
    QScopedPointer<QIODevice, QScopedPointerDeleteLater> input(data);

    input->setParent(nullptr);
    if (!input->isOpen()) {
        if (!input->open(QIODevice::ReadOnly)) {
            // TODO Add developer error handling
            qCDebug(lc, "500: Could not open device %s", qPrintable(input->errorString()));
            write(StatusCode::InternalServerError);
            return;
        }
    } else if (!(input->openMode() & QIODevice::ReadOnly)) {
        // TODO Add developer error handling
        qCDebug(lc) << "500: Device is opened in a wrong mode" << input->openMode();
        write(StatusCode::InternalServerError);
        return;
    }

    writeStatusLine(status);

    if (!input->isSequential()) { // Non-sequential QIODevice should know its data size
        writeHeader(QHttpServerLiterals::contentLengthHeader(),
                    QByteArray::number(input->size()));
    }

    for (auto &&header : headers)
        writeHeader(header.first, header.second);

    d->stream->write("\r\n");

    if (input->atEnd()) {
        qCDebug(lc, "No more data available.");
        return;
    }

    // input takes ownership of the IOChunkedTransfer pointer inside his constructor
    new IOChunkedTransfer<>(input.take(), d->stream->socket);
}

/*!
    Answers a request with an HTTP status code \a status and a
    MIME type \a mimeType. The I/O device \a data provides the body
    of the response. If \a data is sequential, the body of the
    message is sent in chunks: otherwise, the function assumes all
    the content is available and sends it all at once but the read
    is done in chunks.

    \note This function takes the ownership of \a data.
*/
void QHttpServerResponder::write(QIODevice *data, const QByteArray &mimeType, StatusCode status)
{
    write(data,
          {{ QHttpServerLiterals::contentTypeHeader(), mimeType }},
          status);
}

/*!
    Answers a request with an HTTP status code \a status, JSON
    document \a document and HTTP headers \a headers.

    Note: This function sets HTTP Content-Type header as "application/json".
*/
void QHttpServerResponder::write(const QJsonDocument &document, HeaderList headers, StatusCode status)
{
    const QByteArray &json = document.toJson();

    writeStatusLine(status);
    writeHeader(QHttpServerLiterals::contentTypeHeader(),
                QHttpServerLiterals::contentTypeJson());
    writeHeader(QHttpServerLiterals::contentLengthHeader(),
                QByteArray::number(json.size()));
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

    writeHeader(QHttpServerLiterals::contentLengthHeader(),
                QByteArray::number(data.size()));
    writeBody(data);
}

/*!
    Answers a request with an HTTP status code \a status, a
    MIME type \a mimeType and a body \a data.
*/
void QHttpServerResponder::write(const QByteArray &data, const QByteArray &mimeType, StatusCode status)
{
    write(data,
          {{ QHttpServerLiterals::contentTypeHeader(), mimeType }},
          status);
}

/*!
    Answers a request with an HTTP status code \a status.

    Note: This function sets HTTP Content-Type header as "application/x-empty".
*/
void QHttpServerResponder::write(StatusCode status)
{
    write(QByteArray(), QHttpServerLiterals::contentTypeXEmpty(), status);
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
    Q_ASSERT(d->stream);

    d->bodyStarted = false;
    d->stream->write("HTTP/1.1 ");
    d->stream->write(QByteArray::number(quint32(status)));
    d->stream->write(" ");
    d->stream->write(statusString.at(status));
    d->stream->write("\r\n");
}

void QHttpServerResponder::writeHeader(const QByteArray &header, const QByteArray &value)
{
    Q_D(const QHttpServerResponder);
    Q_ASSERT(d->stream);

    d->stream->write(header);
    d->stream->write(": ");
    d->stream->write(value);
    d->stream->write("\r\n");
}

void QHttpServerResponder::writeHeaders(HeaderList headers)
{
    for (auto &&header : headers)
        writeHeader(header.first, header.second);
}

void QHttpServerResponder::writeBody(const char *body, qint64 size)
{
    Q_D(QHttpServerResponder);
    Q_ASSERT(d->stream);

    if (!d->bodyStarted) {
        d->stream->write("\r\n");
        d->bodyStarted = true;
    }

    d->stream->write(body, size);
}

/*!
    This function writes HTTP body \a body.
*/
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

    writeHeader(QHttpServerLiterals::contentLengthHeader(),
                QByteArray::number(d->data.size()));

    writeBody(d->data);
}

QT_END_NAMESPACE
