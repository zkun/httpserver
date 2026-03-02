// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:significant reason:default

#pragma once

#include "qhttpserverresponder.h"
#include <QtNetwork/qhttpheaders.h>

QT_BEGIN_NAMESPACE

class QJsonObject;

class QHttpServerResponsePrivate;
class QHttpServerResponse final
{
    Q_DECLARE_PRIVATE(QHttpServerResponse)
    Q_DISABLE_COPY(QHttpServerResponse)

    friend class QHttpServerResponder;
public:
    using StatusCode = QHttpServerResponder::StatusCode;

    QHttpServerResponse(QHttpServerResponse &&other) noexcept
        : d_ptr(std::exchange(other.d_ptr, nullptr)) {}
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_MOVE_AND_SWAP(QHttpServerResponse)
    void swap(QHttpServerResponse &other) noexcept { qt_ptr_swap(d_ptr, other.d_ptr); }

    Q_IMPLICIT QHttpServerResponse(StatusCode statusCode);

    Q_IMPLICIT QHttpServerResponse(const char *data,
                                                       StatusCode status = StatusCode::Ok);

    Q_IMPLICIT QHttpServerResponse(const QString &data,
                                                       StatusCode status = StatusCode::Ok);

    Q_IMPLICIT QHttpServerResponse(const QByteArray &data,
                                                       StatusCode status = StatusCode::Ok);
    Q_IMPLICIT QHttpServerResponse(QByteArray &&data,
                                                       StatusCode status = StatusCode::Ok);

    Q_IMPLICIT QHttpServerResponse(const QJsonObject &data,
                                                       StatusCode status = StatusCode::Ok);
    Q_IMPLICIT QHttpServerResponse(const QJsonArray &data,
                                                       StatusCode status = StatusCode::Ok);

    Q_IMPLICIT QHttpServerResponse(const QByteArray &mimeType,
                                                       const QByteArray &data,
                                                       StatusCode status = StatusCode::Ok);
    Q_IMPLICIT QHttpServerResponse(const QByteArray &mimeType,
                                                       QByteArray &&data,
                                                       StatusCode status = StatusCode::Ok);

    ~QHttpServerResponse();
    static QHttpServerResponse fromFile(const QString &fileName);

    QByteArray data() const;

    QByteArray mimeType() const;

    StatusCode statusCode() const;

    QHttpHeaders headers() const;
    void setHeaders(const QHttpHeaders &newHeaders);
    void setHeaders(QHttpHeaders &&newHeaders);

private:
    QHttpServerResponsePrivate *d_ptr;
};

QT_END_NAMESPACE
