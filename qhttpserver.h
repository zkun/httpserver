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

#include "qhttpserverrouter.h"
#include "qabstracthttpserver.h"
#include "qhttpserverresponse.h"
#include "qhttpserverviewtraits.h"
#include "qhttpserverrouterrule.h"
#include "qhttpserverrouterviewtraits.h"

#include <tuple>
#include <functional>

QT_BEGIN_NAMESPACE

class QTcpSocket;
class QHttpServerRequest;

class QHttpServerPrivate;
class QHttpServer final : public QAbstractHttpServer
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QHttpServer)

    template<int I, typename ... Ts>
    struct VariadicTypeAt { using Type = typename std::tuple_element<I, std::tuple<Ts...>>::type; };

    template<typename ... Ts>
    struct VariadicTypeLast {
        using Type = typename VariadicTypeAt<sizeof ... (Ts) - 1, Ts...>::Type;
    };

    template <typename...>
    static constexpr bool dependent_false_v = false;

    template<typename T>
    using ResponseType =
        typename std::conditional<
            std::is_base_of<QHttpServerResponse, T>::value,
            T,
            QHttpServerResponse
        >::type;

public:
    explicit QHttpServer(QObject *parent = nullptr);
    ~QHttpServer();

    QHttpServerRouter *router();

    template<typename Rule = QHttpServerRouterRule, typename ... Args>
    bool route(Args && ... args)
    {
        using ViewHandler = typename VariadicTypeLast<Args...>::Type;
        using ViewTraits = QHttpServerRouterViewTraits<ViewHandler>;
        static_assert(ViewTraits::Arguments::StaticAssert, "ViewHandler arguments are in the wrong order or not supported");
        return routeHelper<Rule, ViewHandler, ViewTraits>(
                QtPrivate::makeIndexSequence<sizeof ... (Args) - 1>{},
                std::forward<Args>(args)...);
    }

    template<typename ViewHandler>
    void afterRequest(ViewHandler &&viewHandler)
    {
        using ViewTraits = QHttpServerAfterRequestViewTraits<ViewHandler>;
        static_assert(ViewTraits::Arguments::StaticAssert, "ViewHandler arguments are in the wrong order or not supported");
        afterRequestHelper<ViewTraits, ViewHandler>(std::move(viewHandler));
    }

    using AfterRequestHandler = std::function<QHttpServerResponse(QHttpServerResponse &&response, const QHttpServerRequest &request)>;
    using MissingHandler = std::function<void(const QHttpServerRequest &request, QHttpServerResponder &&responder)>;

    void setMissingHandler(MissingHandler handler);
private:
    template<typename ViewTraits, typename ViewHandler>
    void afterRequestHelper(ViewHandler &&viewHandler) {
        auto handler = [viewHandler](QHttpServerResponse &&resp,
                                     const QHttpServerRequest &request) {
            if constexpr (ViewTraits::Arguments::Last::IsRequest::Value) {
                if constexpr (ViewTraits::Arguments::Count == 2)
                    return std::move(viewHandler(std::move(resp), request));
                else
                    static_assert(dependent_false_v<ViewTraits>);
            } else if constexpr (ViewTraits::Arguments::Last::IsResponse::Value) {
                if constexpr (ViewTraits::Arguments::Count == 1)
                    return std::move(viewHandler(std::move(resp)));
                else if constexpr (ViewTraits::Arguments::Count == 2)
                    return std::move(viewHandler(request, std::move(resp)));
                else
                    static_assert(dependent_false_v<ViewTraits>);
            } else {
                static_assert(dependent_false_v<ViewTraits>);
            }
        };

        afterRequestImpl(std::move(handler));
    }

    void afterRequestImpl(AfterRequestHandler afterRequestHandler);

    template<typename Rule, typename ViewHandler, typename ViewTraits, int ... I, typename ... Args>
    bool routeHelper(QtPrivate::IndexesList<I...>, Args &&... args)
    {
        return routeImpl<Rule,
                         ViewHandler,
                         ViewTraits,
                         typename VariadicTypeAt<I, Args...>::Type...>(std::forward<Args>(args)...);
    }

    template<typename Rule, typename ViewHandler, typename ViewTraits, typename ... Args>
    bool routeImpl(Args &&...args, ViewHandler &&viewHandler)
    {
        auto routerHandler = [this, viewHandler = std::forward<ViewHandler>(viewHandler)] (
                    const QRegularExpressionMatch &match,
                    const QHttpServerRequest &request,
                    QTcpSocket *socket) mutable {
            auto boundViewHandler = router()->bindCaptured<ViewHandler, ViewTraits>(
                    std::move(viewHandler), match);
            responseImpl<ViewTraits>(boundViewHandler, request, socket);
        };

        auto rule = std::make_unique<Rule>(std::forward<Args>(args)..., std::move(routerHandler));
        return router()->addRule<ViewHandler, ViewTraits>(std::move(rule));
    }

    template<typename ViewTraits, typename T>
    void responseImpl(T &boundViewHandler, const QHttpServerRequest &request, QTcpSocket *socket)
    {
        if constexpr (ViewTraits::Arguments::PlaceholdersCount == 0) {
            ResponseType<typename ViewTraits::ReturnType> response(boundViewHandler());
            sendResponse(std::move(response), request, socket);
        } else if constexpr (ViewTraits::Arguments::PlaceholdersCount == 1) {
            if constexpr (ViewTraits::Arguments::Last::IsRequest::Value) {
                ResponseType<typename ViewTraits::ReturnType> response(boundViewHandler(request));
                sendResponse(std::move(response), request, socket);
            } else {
                boundViewHandler(makeResponder(request, socket));
            }
        } else if constexpr (ViewTraits::Arguments::PlaceholdersCount == 2) {
            if constexpr (ViewTraits::Arguments::Last::IsRequest::Value) {
                boundViewHandler(makeResponder(request, socket), request);
            } else {
                boundViewHandler(request, makeResponder(request, socket));
            }
        } else {
            static_assert(dependent_false_v<ViewTraits>);
        }
    }

    bool handleRequest(const QHttpServerRequest &request, QTcpSocket *socket) override final;
    void missingHandler(const QHttpServerRequest &request, QTcpSocket *socket) override final;

    void sendResponse(QHttpServerResponse &&response, const QHttpServerRequest &request, QTcpSocket *socket);
};

QT_END_NAMESPACE

#endif  // QHTTPSERVER_H
