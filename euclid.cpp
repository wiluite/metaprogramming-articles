#include <cassert>
#include <cmath>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

template <unsigned...>
struct ValueList {};

template <unsigned N, class Result = ValueList<>, bool = (N > 0)>
struct make_index_seq_t;

template <unsigned N, unsigned...Values>
struct make_index_seq_t<N, ValueList<Values...>, true> : make_index_seq_t<N - 1, ValueList<(N-1) , Values...> > {};

template <class Result>
struct make_index_seq_t<0, Result, false> {
    using type = Result;
};
template<unsigned N>
using make_index_seq = typename make_index_seq_t<N>::type;

using std::enable_if;
using std::get;
using std::tuple;
using std::tuple_size;
using std::make_tuple;
using std::decay;
using std::move;
using std::forward;

#if 0
namespace another {
    struct S4 {
        double a;
        double b;
        double c;
        double d;
    };
    auto as_tuple(S4 const& arg) -> decltype(make_tuple(arg.a, arg.b, arg.c, arg.d)) { return make_tuple(arg.a, arg.b, arg.c, arg.d); }
}
#else
namespace another {
    struct S4;
}
auto as_tuple(another::S4 const& arg) -> decltype(make_tuple(double{}, double{}, double{}, double{}));
#endif

namespace map_n_fold {

    namespace detail {
        template <class Fun, class T1, class T2, unsigned... Indices>
        auto tuple_map(Fun fun, T1 tup1, T2 tup2, ValueList<Indices...>)
            -> decltype(make_tuple(fun(get<Indices>(tup1), get<Indices>(tup2))...)) {
            return make_tuple(fun(get<Indices>(tup1), get<Indices>(tup2))...);
        }

        template <typename Fun, typename T, typename TupleT>
        auto tuple_fold(Fun, T acc, TupleT &&, ValueList<>) -> T {
            return acc;
        }

        template <typename Fun, typename T, typename TupleT, unsigned N, unsigned ...Ns>
        auto tuple_fold(Fun fun, T acc, TupleT &&tup, ValueList<N, Ns...>) -> T {
            auto acc1 = fun(move(acc), get<N>(tup));
            return tuple_fold(fun, move(acc1), forward<TupleT>(tup), ValueList<Ns...>{});
        }
    }  // namespace detail

    template <class Fun, class ... Cs1, class ... Cs2, typename enable_if<sizeof...(Cs1) == sizeof...(Cs2)>::type* = nullptr>
    auto tuple_map(Fun fun, tuple<Cs1...> tup1, tuple<Cs2...> tup2)
        ->decltype(detail::tuple_map(fun, tup1, tup2, make_index_seq<sizeof...(Cs1)>{})) {
        return detail::tuple_map(fun, tup1, tup2, make_index_seq<sizeof...(Cs1)>{});
    }

    template<class...>
    using VOID_T = void;

    template <class T, class = VOID_T<>>
    struct has_as_tuple : std::false_type{};

    template <class T>
    struct has_as_tuple<T, VOID_T<decltype(std::declval<T>().as_tuple())> > : std::true_type{};

    template <class Fun, class T, class... Types, typename std::enable_if<has_as_tuple<T>::value>::type* = nullptr >
    auto tuple_map(Fun fun, T struct_, tuple<Types...> tup) -> decltype(tuple_map(fun, struct_.as_tuple(), tup)) {
        return tuple_map(fun, struct_.as_tuple(), tup);
    }

    template <class Fun, class T, class... Types, typename std::enable_if<!has_as_tuple<T>::value>::type* = nullptr >
    auto tuple_map(Fun fun, T struct_, tuple<Types...> tup) -> decltype(tuple_map(fun, struct_.as_tuple(), tup)) {
        return tuple_map(fun, struct_.as_tuple(), tup);
    }

    template <class Fun, class... Types, class U, typename std::enable_if<has_as_tuple<U>::value>::type* = nullptr >
    auto tuple_map(Fun fun, tuple<Types...> tup, U struct_) -> decltype(tuple_map(fun, tup, struct_.as_tuple())) {
        return tuple_map(fun, tup, struct_.as_tuple());
    }

    template <class Fun, class... Types, class U, typename std::enable_if<!has_as_tuple<U>::value>::type* = nullptr >
    auto tuple_map(Fun fun, tuple<Types...> tup, U struct_) -> decltype(tuple_map(fun, tup, struct_.as_tuple())) {
        return tuple_map(fun, tup, struct_.as_tuple());
    }

    template <class Fun, class T, class U, typename std::enable_if<has_as_tuple<T>::value and has_as_tuple<U>::value>::type* = nullptr>
    auto tuple_map(Fun fun, T struct1, U struct2) -> decltype(tuple_map(fun, struct1.as_tuple(), struct2.as_tuple())) {
        return tuple_map(fun, struct1.as_tuple(), struct2.as_tuple());
    }

    template <class Fun, class T, class U, typename std::enable_if<!has_as_tuple<T>::value and !has_as_tuple<U>::value>::type* = nullptr>
    auto tuple_map(Fun fun, T struct1, U struct2) -> decltype(tuple_map(fun, as_tuple(struct1), as_tuple(struct2))) {
        return tuple_map(fun, as_tuple(struct1), as_tuple(struct2));
    }

    template <class Fun, class T, class U, typename std::enable_if<has_as_tuple<T>::value and !has_as_tuple<U>::value>::type* = nullptr>
    auto tuple_map(Fun fun, T struct1, U struct2) -> decltype(tuple_map(fun, struct1.as_tuple(), as_tuple(struct2))) {
        return tuple_map(fun, struct1.as_tuple(), as_tuple(struct2));
    }

    template <class Fun, class T, class U, typename std::enable_if<!has_as_tuple<T>::value and has_as_tuple<U>::value>::type* = nullptr>
    auto tuple_map(Fun fun, T struct1, U struct2) -> decltype(tuple_map(fun, as_tuple(struct1), struct2.as_tuple())) {
        return tuple_map(fun, as_tuple(struct1), struct2.as_tuple());
    }

    template <typename TupleT>
    using index_tuple = make_index_seq<tuple_size<typename decay<TupleT>::type>::value>;

    template<typename Fun, typename T, typename TupleT>
    auto tuple_fold(Fun fun, T value, TupleT &&tup) -> decltype(detail::tuple_fold(fun, move(value), forward<TupleT>(tup), index_tuple<TupleT>{})) {
        return detail::tuple_fold(fun, move(value), forward<TupleT>(tup), index_tuple<TupleT>{});
    }

}  // namespace map_n_fold
#define TEMPLATE_CALL_OPERATOR
#if __cplusplus < 201402L && defined(TEMPLATE_CALL_OPERATOR)
struct difference_squared {
    template <class X, class Y>
    auto operator()(X num1, Y num2) -> decltype(pow(num1 - num2, 2)) {
        return pow(num1 - num2, 2);
    }
};
struct sum_of_squared_differences {
    template <class X, class Y>
    auto operator()(X sum, Y num) -> decltype(sum) {
        sum += num; return sum;
    }
};
#endif
template <class T, class U>
auto euclid(T t, U u) -> double {
    using map_n_fold::tuple_fold;
    using map_n_fold::tuple_map;
#if __cplusplus >= 201402L
    auto difference_squared = [](auto num1, auto num2) {  // для map
        return pow(num1 - num2, 2);
    };
    auto sum_of_squared_differences = [](auto sum, auto num) {  // для fold
        sum += num; return sum;
    };
    return sqrt(tuple_fold(sum_of_squared_differences, 0, tuple_map(difference_squared, t, u)));
#else
  #if !defined(TEMPLATE_CALL_OPERATOR)
    struct sum_of_squared_differences {
        auto operator()(double sum, double num) -> double {
            sum += num; return sum;
        }
    };
    struct difference_squared {
        auto operator()(double num1, double num2) -> double {
            return pow(num1 - num2, 2);
        }
    };
  #endif
    return sqrt(tuple_fold(sum_of_squared_differences(), 0, tuple_map(difference_squared(), t, u)));
#endif
}

namespace another {
    struct S4 {
        double a;
        double b;
        double c;
        double d;
    };
}
auto as_tuple(another::S4 const& arg) -> decltype(make_tuple(arg.a, arg.b, arg.c, arg.d)) { return make_tuple(arg.a, arg.b, arg.c, arg.d); }

int main() {
    using std::tie;
    using std::cout;

    struct S2 {
        double a;
        double b;
        auto as_tuple() -> decltype(tie(a, b)) { return tie(a, b); }
    };

    S2 s1 {2.0, 0.0};
    S2 s2 {4.0, 0.0};

    assert(euclid(s1, s2) == 2);
    assert(euclid(make_tuple(2.0, 0.0), s2) == 2);


    struct S3 {
        double a;
        double b;
        double c;
        auto as_tuple() -> decltype(make_tuple(a, b, c)) { return make_tuple(a, b, c); }
    };

    cout << euclid(S3{1.0, 2.0, 3.0}, S3{0.0, 1.0, 2.0}) << '\n';

    assert(euclid(another::S4{1.0, 2.0, 3.0, 4.0}, another::S4{0.0, 1.0, 2.0, 3.0}) == 2);
}

