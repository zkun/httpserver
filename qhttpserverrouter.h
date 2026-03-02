// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:significant reason:default

#pragma once

#include <QtCore/qglobal.h>
#include "qhttpserverrouterviewtraits.h"

#include <QtCore/qscopedpointer.h>
#include <QtCore/qmetatype.h>

#include <functional>
#include <initializer_list>
#include <memory>

QT_BEGIN_NAMESPACE

class QAbstractHttpServer;
class QHttpServerResponder;
class QHttpServerRequest;
class QHttpServerRouterRule;

class QHttpServerRouterPrivate;
class QHttpServerRouter
{
    Q_DECLARE_PRIVATE(QHttpServerRouter)
    Q_DISABLE_COPY_MOVE(QHttpServerRouter)

public:
    QHttpServerRouter(QAbstractHttpServer *server);
    ~QHttpServerRouter();

    template<typename Type>
    bool addConverter(QAnyStringView regexp) {
        // The QMetaType converter registry is shared by all parts of Qt which uses it.
        // Only register a converter if it is not already registered. If registering fails,
        // check that it has been registered by a different thread between the two calls
        // or return false. The registerConverter function will output an warning if a
        // converter is already registered.
        if (!QMetaType::hasRegisteredConverterFunction<QString, Type>()
            && !QMetaType::registerConverter<QString, Type>()
            && !QMetaType::hasRegisteredConverterFunction<QString, Type>())
            return false;

        addConverter(QMetaType::fromType<Type>(), regexp);
        return true;
    }

    void addConverter(QMetaType metaType, QAnyStringView regexp);
    void removeConverter(QMetaType metaType);
    void clearConverters();
    const QHash<QMetaType, QString> &converters() const &;
    QHash<QMetaType, QString> converters() &&;

    template<typename ViewHandler, typename ViewTraits = QHttpServerRouterViewTraits<ViewHandler>>
    QHttpServerRouterRule *addRule(std::unique_ptr<QHttpServerRouterRule> rule)
    {
        return addRuleHelper<ViewTraits>(
                std::move(rule),
                typename ViewTraits::Arguments::Indexes{});
    }

    bool handleRequest(const QHttpServerRequest &request,
                                           QHttpServerResponder &responder) const;

private:
    template<typename ViewTraits, size_t ... Idx>
    QHttpServerRouterRule *addRuleHelper(std::unique_ptr<QHttpServerRouterRule> rule,
                       std::index_sequence<Idx...>)
    {
        return addRuleImpl(std::move(rule), {ViewTraits::Arguments::template metaType<Idx>()...});
    }

    QHttpServerRouterRule *addRuleImpl(std::unique_ptr<QHttpServerRouterRule> rule,
                                         std::initializer_list<QMetaType> metaTypes);

    std::unique_ptr<QHttpServerRouterPrivate> d_ptr;
};

QT_END_NAMESPACE
