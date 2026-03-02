// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:significant reason:default

#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qurl.h>
#include <QtCore/qurlquery.h>
#include <QtNetwork/qhostaddress.h>
#if QT_CONFIG(ssl)
#include <QtNetwork/qsslconfiguration.h>
#endif

#include <memory>

QT_BEGIN_NAMESPACE

class QRegularExpression;
class QString;
class QHttpHeaders;

class QHttpServerRequestPrivate;
class QHttpServerRequest final
{
    friend class QHttpServerResponse;
    friend class QHttpServerStream;
    friend class QHttpServerHttp1ProtocolHandler;
    friend class QHttpServerHttp2ProtocolHandler;

    Q_GADGET

public:
    ~QHttpServerRequest();

    enum class Method
    {
        Unknown = 0x0000,
        Get     = 0x0001,
        Put     = 0x0002,
        Delete  = 0x0004,
        Post    = 0x0008,
        Head    = 0x0010,
        Options = 0x0020,
        Patch   = 0x0040,
        Connect = 0x0080,
        Trace   = 0x0100,

        AnyKnown = Get | Put | Delete | Post | Head | Options | Patch | Connect | Trace,
    };
    Q_ENUM(Method)
    Q_DECLARE_FLAGS(Methods, Method)
    Q_FLAG(Methods)

    QByteArray value(const QByteArray &key) const;
    QUrl url() const;
    QUrlQuery query() const;
    Method method() const;
    const QHttpHeaders &headers() const &;
    QHttpHeaders headers() &&;
    QByteArray body() const;
    QHostAddress remoteAddress() const;
    quint16 remotePort() const;
    QHostAddress localAddress() const;
    quint16 localPort() const;
#if QT_CONFIG(ssl)
    QSslConfiguration sslConfiguration() const;
#endif

private:
    Q_DISABLE_COPY(QHttpServerRequest)

#if !defined(QT_NO_DEBUG_STREAM)
    friend QDebug operator<<(QDebug debug, const QHttpServerRequest &request);
#endif

    explicit QHttpServerRequest(const QHostAddress &remoteAddress,
                                                    quint16 remotePort,
                                                    const QHostAddress &localAddress,
                                                    quint16 localPort);
#if QT_CONFIG(ssl)
    explicit QHttpServerRequest(const QHostAddress &remoteAddress,
                                                    quint16 remotePort,
                                                    const QHostAddress &localAddress,
                                                    quint16 localPort,
                                                    const QSslConfiguration &sslConfiguration);
#endif

    std::unique_ptr<QHttpServerRequestPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QHttpServerRequest::Methods)

QT_END_NAMESPACE
