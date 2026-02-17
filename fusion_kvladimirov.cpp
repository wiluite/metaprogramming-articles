#include <forward_list>
#include <iostream>
#include <tuple>
#include <utility> // index_sequence
#include <type_traits>
#include <sstream>
#include <cassert>

using std::size_t;
using std::conditional_t;
using std::string;
using std::enable_if_t;
using std::tuple;
using std::index_sequence;
using std::get;
using std::make_tuple;
using std::forward;
using std::make_index_sequence;
using std::tuple_size;
using std::is_same;

namespace impl {
    // Примитивы для работы алгоритма remove_if_index_sequence_t
    template <class L>
    struct is_empty {
        static constexpr bool value = false;
    };
    template <>
    struct is_empty<tuple<>> {
        static constexpr bool value = true;
    };
    template <>
    struct is_empty<tuple<> const> {
        static constexpr bool value = true;
    };

    template <class L>
    struct front_t;
    template <class L>
    using front = typename front_t<L>::type;
    template <class Head, class... Rest>
    struct front_t<tuple<Head, Rest...>> {
        using type = Head;
    };
    template <class Head, class... Rest>
    struct front_t<tuple<Head, Rest...> const> {
        using type = Head;
    };

    template <class L>
    struct pop_front_t;
    template <class L>
    using pop_front = typename pop_front_t<L>::type;
    template <class Head, class... Rest>
    struct pop_front_t<tuple<Head, Rest...>> {
        using type = tuple<Rest...>;
    };
    template <class Head, class... Rest>
    struct pop_front_t<tuple<Head, Rest...> const> {
        using type = tuple<Rest...> const;
    };

    // Первичный шаблон и псевдоним алгоритма remove_if_index_sequence_t
    template <template <class...> class Compare, class TTypeList, size_t idx = 0, class Result = index_sequence<>, 
        bool = is_empty<TTypeList>::value >
    struct remove_if_index_sequence_t;
    template <template <class...> class Compare, class TTypeList>
    using remove_if_index_sequence = typename remove_if_index_sequence_t<Compare, TTypeList>::type;

    // Рекурсивный случай алгоритма 
    template <template <class...> class Compare, class TTypeList, size_t idx, size_t ... values>
    struct remove_if_index_sequence_t<Compare, TTypeList, idx, index_sequence<values...>, false> 
        : remove_if_index_sequence_t <
              Compare,
              pop_front<TTypeList>,
              idx + 1,
              conditional_t<
                  !Compare<front<TTypeList> >::value,
                  index_sequence<values..., idx>,
                  index_sequence<values...>  
              >
          >
    { };
    // Базовый случай
    template <template <class...> class Compare, class TTypeList, size_t idx, class Result>
    struct remove_if_index_sequence_t<Compare, TTypeList, idx, Result, true> {
        using type = Result;
    }; 

    // Алгоритм отбора нужных значений из кортежа (для алгоритма fusion::remove_if)
    template <class... Types, size_t ... Indices>
    constexpr auto select(tuple<Types...> tup, index_sequence<Indices...>) {
        return make_tuple(get<Indices>(tup)...);
    }

    // Реализация алгоритма fusion::transform 
    namespace details {
        template <typename Fun, typename TupleT, size_t ...N>
        constexpr auto tuple_map(Fun fun, TupleT &&tup, index_sequence<N...>) {
            return make_tuple(fun(get<N>(forward<TupleT>(tup)))...);
        }
    }

    // Хелпер
    template <typename TupleT>
    using index_tuple = make_index_sequence<tuple_size<std::decay_t<TupleT>>::value>;

    // Фасад реализации алгоритма fusion::transform
    template <typename Fun, typename TupleT> 
    constexpr auto tuple_map(Fun fun, TupleT && tup) {
        return details::tuple_map(fun, std::forward<TupleT>(tup), index_tuple<TupleT>());
    };
}

// Обертки, воспроизводящие "интерфейс" fusion
// (как если бы мы работали с настоящими boost::fusion::remove_if и boost::fusion::transform)
namespace fusion {
    template <typename T>
    concept IsTuple = requires(T t) {
        []<typename... Args>(std::tuple<Args...> const &) {}(t);
    };
    template <class T, class F>
    requires IsTuple<T>
    constexpr auto transform(T && t, F f) {
        return impl::tuple_map(f, std::forward<T>(t) );
    } 

    template <template <class...> class Predicate, class T>
    requires IsTuple<T>
    constexpr auto remove_if(T && tup) {
        using namespace impl;
        return select(std::forward<T>(tup), remove_if_index_sequence<Predicate, std::decay_t<decltype(tup)> >{});
    }
}
// Получившийся результат воспроизводит начальный пример: 
int main() {
    auto to_string = [] (auto t) { std::stringstream ss; ss << t; return ss.str(); };

    auto noflts = fusion::remove_if<std::is_floating_point>(make_tuple(1, "abc", 4.3f));
    static_assert(is_same<decltype(noflts), tuple<int, char const*> >::value);

    auto strs = fusion::transform(noflts, to_string);
    static_assert(is_same<decltype(strs), tuple<string, string> >::value);

    assert(get<0>(strs) == "1");
    assert(get<1>(strs) == "abc");
    std::cout << get<0>(strs) << ' ' << get<1>(strs) << '\n';
}


constexpr auto noflts_ct = fusion::remove_if<std::is_floating_point>(make_tuple(1, "abc", 4.3f, 42.0));
static_assert(is_same<decltype(noflts_ct), tuple<int, char const*> const >::value);

// if cpp17 (string_view + constexpr-лямбды)
#include <string_view>
using std::string_view;
constexpr auto another_lambda = [] (auto t) {
    if constexpr (std::is_same_v<decltype(t), int>) 
        return string_view("1");
    else
        return string_view(t);                 
};
constexpr auto const strs_ct = fusion::transform(noflts_ct, another_lambda);
static_assert(is_same<decltype(strs_ct), tuple<string_view, string_view> const >::value);
static_assert(strs_ct == make_tuple(string_view("1"), string_view("abc")));
static_assert(get<0>(strs_ct) == "1");
static_assert(get<1>(strs_ct) == "abc");
// 
