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
#include "qhttpserverrequest_p.h"

#include <QtNetwork/qtcpsocket.h>

QT_BEGIN_NAMESPACE

static const std::array<void(*)(const QString &, QUrl *), UF_MAX> parseUrlFunctions {
    [](const QString &string, QUrl *url) { url->setScheme(string); },
    [](const QString &string, QUrl *url) { url->setHost(string); },
    [](const QString &string, QUrl *url) { url->setPort(string.toInt()); },
    [](const QString &string, QUrl *url) { url->setPath(string, QUrl::TolerantMode); },
    [](const QString &string, QUrl *url) { url->setQuery(string); },
    [](const QString &string, QUrl *url) { url->setFragment(string); },
    [](const QString &string, QUrl *url) { url->setUserInfo(string); },
};

http_parser_settings QHttpServerRequestPrivate::httpParserSettings {
    &QHttpServerRequestPrivate::onMessageBegin,
    &QHttpServerRequestPrivate::onUrl,
    &QHttpServerRequestPrivate::onStatus,
    &QHttpServerRequestPrivate::onHeaderField,
    &QHttpServerRequestPrivate::onHeaderValue,
    &QHttpServerRequestPrivate::onHeadersComplete,
    &QHttpServerRequestPrivate::onBody,
    &QHttpServerRequestPrivate::onMessageComplete,
    &QHttpServerRequestPrivate::onChunkHeader,
    &QHttpServerRequestPrivate::onChunkComplete
};

QHttpServerRequestPrivate::QHttpServerRequestPrivate()
{
    httpParser.data = this;
    http_parser_init(&httpParser, HTTP_REQUEST);
}

QByteArray QHttpServerRequestPrivate::header(const QByteArray &key) const
{
    return headers.value(headerHash(key)).second;
}

bool QHttpServerRequestPrivate::parse(const QByteArray fragment)
{
    if (fragment.size()) {
        url.setScheme(QStringLiteral("http"));

        const auto parsed = http_parser_execute(&httpParser,
                                                &httpParserSettings,
                                                fragment.constData(),
                                                size_t(fragment.size()));
        if (int(parsed) < fragment.size()) {
            qDebug("Parse error: %d", httpParser.http_errno);
            return false;
        }
    }
    return true;
}

uint QHttpServerRequestPrivate::headerHash(const QByteArray &key) const
{
    return qHash(key.toLower(), headersSeed);
}

void QHttpServerRequestPrivate::clear()
{
    lastHeader.clear();
    headers.clear();
    body.clear();
    url.clear();
}

bool QHttpServerRequestPrivate::parseUrl(const char *at, size_t length, bool connect, QUrl *url)
{
    struct http_parser_url u;
    if (http_parser_parse_url(at, length, connect ? 1 : 0, &u) != 0)
        return false;

    for (auto i = 0u; i < UF_MAX; i++) {
        if (u.field_set & (1 << i)) {
            parseUrlFunctions[i](QString::fromUtf8(at + u.field_data[i].off,
                                                   u.field_data[i].len),
                                 url);
        }
    }

    return true;
}

QHttpServerRequestPrivate *QHttpServerRequestPrivate::instance(http_parser *httpParser)
{
    return static_cast<QHttpServerRequestPrivate *>(httpParser->data);
}

int QHttpServerRequestPrivate::onMessageBegin(http_parser *httpParser)
{
    instance(httpParser)->state = State::OnMessageBegin;
    return 0;
}

int QHttpServerRequestPrivate::onUrl(http_parser *httpParser, const char *at, size_t length)
{
    auto instance = static_cast<QHttpServerRequestPrivate *>(httpParser->data);
    instance->state = State::OnUrl;
    parseUrl(at, length, false, &instance->url);
    return 0;
}

int QHttpServerRequestPrivate::onStatus(http_parser *httpParser, const char *at, size_t length)
{
    instance(httpParser)->state = State::OnStatus;
    return 0;
}

int QHttpServerRequestPrivate::onHeaderField(http_parser *httpParser, const char *at, size_t length)
{
    auto i = instance(httpParser);
    i->state = State::OnHeaders;
    const auto key = QByteArray(at, int(length));
    i->headers.insert(i->headerHash(key), qMakePair(key, QByteArray()));
    i->lastHeader = key;
    return 0;
}

int QHttpServerRequestPrivate::onHeaderValue(http_parser *httpParser, const char *at, size_t length)
{
    auto i = instance(httpParser);
    i->state = State::OnHeaders;
    Q_ASSERT(!i->lastHeader.isEmpty());
    const auto value = QByteArray(at, int(length));
    i->headers[i->headerHash(i->lastHeader)] = qMakePair(i->lastHeader, value);
    if (i->lastHeader.compare(QByteArrayLiteral("host"), Qt::CaseInsensitive) == 0)
        parseUrl(at, length, true, &i->url);
#if defined(QT_DEBUG)
    i->lastHeader.clear();
#endif
    return 0;
}

int QHttpServerRequestPrivate::onHeadersComplete(http_parser *httpParser)
{
    instance(httpParser)->state = State::OnHeadersComplete;
    return 0;
}

int QHttpServerRequestPrivate::onBody(http_parser *httpParser, const char *at, size_t length)
{
    auto i = instance(httpParser);
    i->state = State::OnBody;
    if (i->body.isEmpty()) {
        i->body.reserve(
                static_cast<int>(httpParser->content_length) +
                static_cast<int>(length));
    }

    i->body.append(at, int(length));
    return 0;
}

int QHttpServerRequestPrivate::onMessageComplete(http_parser *httpParser)
{
    instance(httpParser)->state = State::OnMessageComplete;
    return 0;
}

int QHttpServerRequestPrivate::onChunkHeader(http_parser *httpParser)
{
    instance(httpParser)->state = State::OnChunkHeader;
    return 0;
}

int QHttpServerRequestPrivate::onChunkComplete(http_parser *httpParser)
{
    instance(httpParser)->state = State::OnChunkComplete;
    return 0;
}

QHttpServerRequest::QHttpServerRequest(QObject *parent)
    : QObject(parent)
{
    d = std::make_unique<QHttpServerRequestPrivate>();
}

QHttpServerRequest::~QHttpServerRequest()
{}

QByteArray QHttpServerRequest::value(const QByteArray &key) const
{
    return d->headers.value(d->headerHash(key)).second;
}

QUrl QHttpServerRequest::url() const
{
    return d->url;
}

QUrlQuery QHttpServerRequest::query() const
{
    return QUrlQuery(d->url.query());
}

QHttpServerRequest::Method QHttpServerRequest::method() const
{
    switch (d->httpParser.method) {
    case HTTP_GET:
        return QHttpServerRequest::Method::Get;
    case HTTP_PUT:
        return QHttpServerRequest::Method::Put;
    case HTTP_DELETE:
        return QHttpServerRequest::Method::Delete;
    case HTTP_POST:
        return QHttpServerRequest::Method::Post;
    case HTTP_HEAD:
        return QHttpServerRequest::Method::Head;
    case HTTP_OPTIONS:
        return QHttpServerRequest::Method::Options;
    case HTTP_PATCH:
        return QHttpServerRequest::Method::Patch;
    case HTTP_CONNECT:
        return QHttpServerRequest::Method::Connect;
    default:
        return QHttpServerRequest::Method::Unknown;
    }
}

QList<QPair<QByteArray, QByteArray> > QHttpServerRequest::headers() const
{
    QList<QPair<QByteArray, QByteArray>> ret;
    for (auto it : d->headers)
        ret.push_back({it.first, it.second});
    return ret;
}

QByteArray QHttpServerRequest::body() const
{
    return d->body;
}

QT_END_NAMESPACE
