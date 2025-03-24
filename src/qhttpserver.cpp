#include "qhttpserver.h"
#include "qhttpserverrequest.h"

void QHttpServer::handleRequest(const QHttpServerRequest &request, std::unique_ptr<QHttpServerResponder> responder)
{
    const QString path = request.url().path();

    if (m_routes.contains(path))
        m_routes.value(path)(request, std::move(responder));
    else if (m_missing)
        m_missing(request, std::move(responder));
    else
        responder->sendResponse(QHttpServerResponse::StatusCode::NotFound);
}
