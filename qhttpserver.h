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

#ifndef QHTTPSERVER_H
#define QHTTPSERVER_H

#include "qabstracthttpserver.h"
#include "qhttpserverrouter.h"
#include "qhttpserverresponse.h"
#include "qhttpserverrouterrule.h"
#include "qhttpserverviewtraits.h"
#include "qhttpserverrouterviewtraits.h"

#include <tuple>
#include <functional>

QT_BEGIN_NAMESPACE

class QHttpServerRequest;
class QHttpServerPrivate;
class QHttpServer final : public QAbstractHttpServer
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QHttpServer)

public:
    explicit QHttpServer(QObject *parent = nullptr);
    ~QHttpServer() override;

    QHttpServerRouter *router();

    template<typename Rule = QHttpServerRouterRule, typename ViewHandler>
    bool route(const QString &pathPattern, QHttpServerRequest::Methods method, const typename QtPrivate::ContextTypeForFunctor<ViewHandler>::ContextType *context,
                ViewHandler &&viewHandler)
    {
        using ViewTraits = QHttpServerRouterViewTraits<ViewHandler>;
        static_assert(ViewTraits::Arguments::StaticAssert,
                      "ViewHandler arguments are in the wrong order or not supported");
        return routeImpl<Rule, ViewHandler, ViewTraits>(pathPattern, method, context,
                                                        std::forward<ViewHandler>(viewHandler));
    }
    template<typename Rule = QHttpServerRouterRule, typename ViewHandler>
    bool route(const QString &pathPattern, const typename QtPrivate::ContextTypeForFunctor<ViewHandler>::ContextType *context,
                ViewHandler &&viewHandler)
    {
        return route<Rule>(pathPattern, QHttpServerRequest::Method::All,
                           context, std::forward<ViewHandler>(viewHandler));
    }

    template<typename Rule = QHttpServerRouterRule, typename ViewHandler>
    bool route(const QString &pathPattern, QHttpServerRequest::Methods method,
                ViewHandler &&viewHandler)
    {
        return route<Rule>(pathPattern, method, this, std::forward<ViewHandler>(viewHandler));
    }

    template<typename Rule = QHttpServerRouterRule, typename ViewHandler>
    bool route(const QString &pathPattern, ViewHandler &&viewHandler)
    {
        return route<Rule>(pathPattern, QHttpServerRequest::Method::All,
                           this, std::forward<ViewHandler>(viewHandler));
    }

    using MissingHandler = std::function<void(const QHttpServerRequest &request,
                                              QHttpServerResponder &&responder)>;

    void setMissingHandler(MissingHandler handler);

    using AfterRequestHandler = std::function<void(const QHttpServerRequest &request,
                                        QHttpServerResponse &response)>;

    void addAfterRequestHandler(AfterRequestHandler afterRequestHandler);
private:

    template<typename ViewHandler, typename ViewTraits>
    auto createRouteHandler(const typename QtPrivate::ContextTypeForFunctor<ViewHandler>::ContextType *context,
                            ViewHandler &&viewHandler)
    {
        return [this, context, viewHandler](
                       const QRegularExpressionMatch &match,
                       const QHttpServerRequest &request,
                       QHttpServerResponder &&responder) mutable {
                auto boundViewHandler = QHttpServerRouterRule::bindCaptured<ViewHandler, ViewTraits>(context,
                                                               std::forward<ViewHandler>(viewHandler), match);
                responseImpl<ViewTraits>(boundViewHandler, request, std::move(responder));
        };
    }

    template<typename Rule, typename ViewHandler, typename ViewTraits>
    bool routeImpl(const QString &pathPattern, QHttpServerRequest::Methods method,
                    const typename QtPrivate::ContextTypeForFunctor<ViewHandler>::ContextType *context,
                    ViewHandler &&viewHandler)
    {
        auto routerHandler = createRouteHandler<ViewHandler, ViewTraits>(context, std::forward<ViewHandler>(viewHandler));

        auto rule = std::make_unique<Rule>(pathPattern, method, std::move(routerHandler));
        return router()->addRule<ViewHandler, ViewTraits>(std::move(rule));
    }

    template<typename ViewTraits, typename T>
    void responseImpl(T &boundViewHandler, const QHttpServerRequest &request, QHttpServerResponder &&responder)
    {
        if constexpr (ViewTraits::Arguments::PlaceholdersCount == 0) {
            QHttpServerResponse response(boundViewHandler());
            sendResponse(std::move(response), request, std::move(responder));
        } else if constexpr (ViewTraits::Arguments::PlaceholdersCount == 1) {
            if constexpr (ViewTraits::Arguments::Last::IsRequest::Value) {
                QHttpServerResponse response(boundViewHandler(request));
                sendResponse(std::move(response), request, std::move(responder));
            } else {
                static_assert(std::is_same_v<typename ViewTraits::ReturnType, void>,
                    "Handlers with responder argument must have void return type.");
                boundViewHandler(std::move(responder));
            }
        } else if constexpr (ViewTraits::Arguments::PlaceholdersCount == 2) {
            static_assert(std::is_same_v<typename ViewTraits::ReturnType, void>,
                "Handlers with responder argument must have void return type.");
            if constexpr (ViewTraits::Arguments::Last::IsRequest::Value) {
                boundViewHandler(std::move(responder), request);
            } else {
                boundViewHandler(request, std::move(responder));
            }
        }
    }

    bool handleRequest(const QHttpServerRequest &request, QHttpServerResponder &responder) override;
    void missingHandler(const QHttpServerRequest &request, QHttpServerResponder &&responder) override;

    void sendResponse(QHttpServerResponse &&response, const QHttpServerRequest &request,
                      QHttpServerResponder &&responder);
};

QT_END_NAMESPACE

#endif  // QHTTPSERVER_H
