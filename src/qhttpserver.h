#ifndef QHTTPSERVER_H
#define QHTTPSERVER_H

#include "qabstracthttpserver.h"
#include "qhttpserverresponse.h"
#include "qhttpserverresponder.h"

class QHttpServer : public QAbstractHttpServer
{
    Q_OBJECT
public:
    using QAbstractHttpServer::QAbstractHttpServer;

    template<typename ViewHandler>
    void route(const QString &pathPattern, ViewHandler &&viewHandler)
    {
        auto handler = [viewHandler](const QHttpServerRequest &request, QHttpServerResponder &&responder) {
            constexpr size_t count = ContextTypeForFunctor<ViewHandler>::arg_count;

            if constexpr (count == 0) {
                QHttpServerResponse response = viewHandler();
                responder.sendResponse(std::move(response));
            } else if constexpr (count == 1) {
                QHttpServerResponse response = viewHandler(request);
                responder.sendResponse(std::move(response));
            } else if constexpr (count == 2) {
                viewHandler(request, std::move(responder));
            }
        };
        m_routes.insert(pathPattern, std::move(handler));
    }

    template<typename T, typename ViewHandler>
    void route(const QString &pathPattern, T *context, ViewHandler &&viewHandler)
    {
        auto handler = [context, viewHandler](const QHttpServerRequest &request, QHttpServerResponder &&responder) {
            constexpr size_t count = ContextTypeForFunctor<ViewHandler>::arg_count;

            if constexpr (count == 0) {
                QHttpServerResponse response = (context->*viewHandler)();
                responder.sendResponse(std::move(response));
            } else if constexpr (count == 1) {
                QHttpServerResponse response = (context->*viewHandler)(request);
                responder.sendResponse(std::move(response));
            } else if constexpr (count == 2) {
                (context->*viewHandler)(request, std::move(responder));
            }
        };
        m_routes.insert(pathPattern, std::move(handler));
    }

    using BeforeHandler = std::function<bool(const QHttpServerRequest &, QHttpServerResponder &)>;
    void setBeforeHandler(BeforeHandler &&handler)
    {
        m_before = std::forward<BeforeHandler>(handler);;
    }

    using Handler = std::function<void(const QHttpServerRequest &, QHttpServerResponder &&)>;
    void setMissingHandler(Handler &&handler)
    {
        m_missing = std::forward<Handler>(handler);
    }

protected:
    template<typename T>
    struct ContextTypeForFunctor {
        static constexpr size_t arg_count = 0;
    };

    template<typename ReturnT, typename... Args>
    struct ContextTypeForFunctor<ReturnT(*)(Args...)> {
        static constexpr size_t arg_count = sizeof...(Args);
    };

    template<typename ReturnT, typename ClassT, class ...Args>
    struct ContextTypeForFunctor<ReturnT (ClassT::*)(Args...)>
    {
        static constexpr size_t arg_count = sizeof...(Args);
    };

private:
    void handleRequest(const QHttpServerRequest &request, QHttpServerResponder &&responder) override;

    Handler m_missing;
    BeforeHandler m_before;
    QHash<QString, Handler> m_routes;
};

#endif // QHTTPSERVER_H
