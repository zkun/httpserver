#ifndef QHTTPSERVERLITERALS_H
#define QHTTPSERVERLITERALS_H

#include <QByteArray>

QT_BEGIN_NAMESPACE

namespace QHttpServerLiterals
{
inline const QByteArray contentTypeHeader = QByteArrayLiteral("Content-Type");
inline const QByteArray contentLengthHeader = QByteArrayLiteral("Content-Length");

inline const QByteArray contentTypeText = QByteArrayLiteral("text/html");
inline const QByteArray contentTypeJson = QByteArrayLiteral("application/json");
inline const QByteArray contentTypeXEmpty = QByteArrayLiteral("application/x-empty");
}

QT_END_NAMESPACE

#endif // QHTTPSERVERLITERALS_H
