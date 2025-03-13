#include "qhttpserver.h"
#include "qhttpserverrequest.h"

void QHttpServer::handleRequest(const QHttpServerRequest &request, QHttpServerResponder &&responder)
{
    const QString path = request.url().path();
    if (m_before && m_before(request, responder))
        return;

    if (m_routes.contains(path))
        m_routes.value(path)(request, std::move(responder));
    else if (m_missing)
        m_missing(request, std::move(responder));
    else
        responder.sendResponse(QHttpServerResponse::StatusCode::NotFound);
}
