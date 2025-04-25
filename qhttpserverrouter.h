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

#ifndef QHTTPSERVERROUTER_H
#define QHTTPSERVERROUTER_H

#include "qhttpserverrouterviewtraits.h"

#include <QtCore/qscopedpointer.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qregularexpression.h>

#include <memory>
#include <functional>
#include <initializer_list>

QT_BEGIN_NAMESPACE

class QTcpSocket;
class QHttpServerRequest;
class QHttpServerRouterRule;

class QHttpServerRouterPrivate;
class QHttpServerRouter
{
    Q_DECLARE_PRIVATE(QHttpServerRouter)
    Q_DISABLE_COPY_MOVE(QHttpServerRouter)

public:
    QHttpServerRouter();
    ~QHttpServerRouter();

    template<typename Type>
    bool addConverter(const QLatin1String &regexp) {
        static_assert(QMetaTypeId2<Type>::Defined,
                      "Type is not registered with Qt's meta-object system: "
                      "please apply Q_DECLARE_METATYPE() to it");

        if (!QMetaType::registerConverter<QString, Type>())
            return false;

        addConverter(qMetaTypeId<Type>(), regexp);
        return true;
    }

    void addConverter(const int type, const QLatin1String &regexp);
    void removeConverter(const int);
    void clearConverters();
    const QMap<int, QLatin1String> &converters() const;

    template<typename ViewHandler, typename ViewTraits = QHttpServerRouterViewTraits<ViewHandler>>
    bool addRule(std::unique_ptr<QHttpServerRouterRule> rule)
    {
        return addRuleHelper<ViewTraits>(std::move(rule), typename ViewTraits::Arguments::Indexes{});
    }

    template<typename ViewHandler, typename ViewTraits = QHttpServerRouterViewTraits<ViewHandler>>
    typename ViewTraits::BindableType bindCaptured(ViewHandler &&handler,
                      const QRegularExpressionMatch &match) const
    {
        return bindCapturedImpl<ViewHandler, ViewTraits>(
                    std::forward<ViewHandler>(handler), match,
                    typename ViewTraits::Arguments::CapturableIndexes{});
    }

    bool handleRequest(const QHttpServerRequest &request,
                       QTcpSocket *socket) const;

private:
    template<typename ViewTraits, int ... Idx>
    bool addRuleHelper(std::unique_ptr<QHttpServerRouterRule> rule, QtPrivate::IndexesList<Idx...>)
    {
        return addRuleImpl(std::move(rule), {ViewTraits::Arguments::template metaTypeId<Idx>()...});
    }

    bool addRuleImpl(std::unique_ptr<QHttpServerRouterRule> rule, std::initializer_list<int> metaTypes);

    template<typename F, typename... Args>
    auto bind_front(F &&f, Args &&...args) const
    {
        return [f = std::forward<F>(f),
                args = std::make_tuple(std::forward<Args>(args)...)](auto &&...callArgs) {
            return std::apply(f,
                              std::tuple_cat(args,
                                             std::forward_as_tuple(std::forward<decltype(callArgs)>(
                                                     callArgs)...)));
        };
    }
    template<typename ViewHandler, typename ViewTraits, int... Cx>
    typename ViewTraits::BindableType bindCapturedImpl(ViewHandler &&handler,
                                                       const QRegularExpressionMatch &match,
                                                       QtPrivate::IndexesList<Cx...>) const
    {
        return bind_front(
                std::forward<ViewHandler>(handler),
                QVariant(match.captured(Cx + 1))
                        .value<typename ViewTraits::Arguments::template Arg<Cx>::CleanType>()...);
    }

    std::unique_ptr<QHttpServerRouterPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QHTTPSERVERROUTER_H
