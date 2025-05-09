/****************************************************************************
**
** Copyright (C) 2020 Mikhail Svetkin <mikhail.svetkin@gmail.com>
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

#ifndef QHTTPSERVERVIEWTRAITS_IMPL_H
#define QHTTPSERVERVIEWTRAITS_IMPL_H

#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobjectdefs.h>

#include <functional>
#include <tuple>
#include <type_traits>

QT_BEGIN_NAMESPACE

namespace QtPrivate {

template<typename T>
struct RemoveCVRef
{
    using Type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};


template<typename ReturnT, typename ... Args>
struct FunctionTraitsHelper
{
    static constexpr const int ArgumentCount = sizeof ... (Args);
    static constexpr const int ArgumentIndexMax = ArgumentCount - 1;
    using ReturnType = ReturnT;

    template <int I>
    struct Arg {
        using Type = typename std::tuple_element<I, std::tuple<Args...>>::type;

        using CleanType = typename QtPrivate::RemoveCVRef<Type>::Type;

        static constexpr bool Defined = QMetaTypeId2<CleanType>::Defined;
    };
};

template<typename T>
struct FunctionTraitsImpl;

template<typename T>
struct FunctionTraitsImpl : public FunctionTraitsImpl<decltype(&T::operator())>
{};

template<typename ReturnT, typename ... Args>
struct FunctionTraitsImpl<ReturnT (*)(Args...)> : public FunctionTraitsHelper<ReturnT, Args...>
{
};

template<class ReturnT, class ClassT, class ...Args>
struct FunctionTraitsImpl<ReturnT (ClassT::*)(Args...) const>
    : public FunctionTraitsHelper<ReturnT, Args...>
{
};

template<typename T>
using FunctionTraits = FunctionTraitsImpl<std::decay_t<T>>;

template<typename ... T>
struct CheckAny {
    static constexpr bool Value = (T::Value || ...);
    static constexpr bool Valid = (T::Valid || ...);
    static constexpr bool StaticAssert = (T::StaticAssert || ...);
};

template<typename ViewHandler, bool DisableStaticAssert>
struct ViewTraits {
    using FTraits = FunctionTraits<ViewHandler>;
    using ArgumentIndexes = typename Indexes<FTraits::ArgumentCount>::Value;

    template<int I, typename Special>
    struct SpecialHelper {
        using Arg = typename FTraits::template Arg<I>;
        using CleanSpecialT = typename RemoveCVRef<Special>::Type;

        static constexpr bool TypeMatched = std::is_same<typename Arg::CleanType, CleanSpecialT>::value;
        static constexpr bool TypeCVRefMatched = std::is_same<typename Arg::Type, Special>::value;

        static constexpr bool ValidPosition =
            (I == FTraits::ArgumentIndexMax ||
             I == FTraits::ArgumentIndexMax - 1);
        static constexpr bool ValidAll = TypeCVRefMatched && ValidPosition;

        static constexpr bool AssertCondition =
            DisableStaticAssert || !TypeMatched || TypeCVRefMatched;

        static constexpr bool AssertConditionOrder =
            DisableStaticAssert || !TypeMatched || ValidPosition;

        static constexpr bool StaticAssert = AssertCondition && AssertConditionOrder;

        static_assert(AssertConditionOrder,
                      "ViewHandler arguments error: "
                      "QHttpServerRequest or QHttpServerResponder"
                      " can only be the last argument");
    };

    template<int I, typename T>
    struct Special {
        using Helper = SpecialHelper<I, T>;
        static constexpr bool Value = Helper::TypeMatched;
        static constexpr bool Valid = Helper::ValidAll;
        static constexpr bool StaticAssert = Helper::StaticAssert;
        static constexpr bool AssertCondition = Helper::AssertCondition;
    };
};

} // namespace QtPrivate

QT_END_NAMESPACE

#endif  // QHTTPSERVERVIEWTRAITS_IMPL_H
