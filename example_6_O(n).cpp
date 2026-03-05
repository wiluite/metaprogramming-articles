#include <cstdint>
#include <utility>
#include <tuple>
#include <type_traits>

template <class L>
struct IsEmptyT {
    static constexpr bool value = false;
};
template <>
struct IsEmptyT<std::index_sequence<>> {
    static constexpr bool value = true;
};

template <class L>
struct FrontT;
template <class L, class E>
struct PushFrontT;
template <class L>
struct PopFrontT;

template <class L>
using Front = typename FrontT<L>::Type;
template <class L, class E>
using PushFront = typename PushFrontT<L, E>::Type;
template <class L>
using PopFront = typename PopFrontT<L>::Type;

template <std::size_t First, std::size_t... Rest>
struct FrontT<std::index_sequence<First, Rest...>> {
    using Type = std::integral_constant<std::size_t, First>;
};
template <std::size_t... Values, std::size_t New>
struct PushFrontT<std::index_sequence<Values...>, std::integral_constant<std::size_t, New> > {
    using Type = std::index_sequence<New, Values...>;
};
template <std::size_t V, size_t ... Values>
struct PopFrontT< std::index_sequence<V, Values...> >  {
    using Type = std::index_sequence<Values...>;
};


template <class L1, class L2, bool = IsEmptyT<L1>::value >
struct ReverseCTT;
template <class L1, class L2>
using ReverseCT = typename ReverseCTT<L1,L2>::Type;

template <class L1, class L2, bool >
struct ReverseCTT : ReverseCTT<PopFront<L1>, PushFront<L2, Front<L1> > > {};

template <class L1, class L2>
struct ReverseCTT<L1, L2, true> {
    using Type = L2;
};

namespace detail {
    template <class ... Elems, size_t ... Vals>
    constexpr auto reverse_tuple(std::tuple<Elems...> const & t, std::index_sequence<Vals...>) {
        return std::make_tuple(std::get<Vals>(t)...);
    }
}
template <class ... Elems>
constexpr auto reverse_tuple(std::tuple<Elems...> const& t) {
    using ReversedIS = ReverseCT<std::make_index_sequence<sizeof...(Elems)>, std::index_sequence<> >;
    return detail::reverse_tuple(t, ReversedIS{});
}

template <class From, template<class...> class To>
struct renameT;
template <template<class...> class From, class ... Elems, template<class...> class To>
struct renameT<From<Elems...>, To> {
    using type = To<Elems...>;
};

template <class From, template<class...> class To>
using rename = typename renameT<From, To>::type;

template <class...>
struct List {};

template <class>
struct IsList : std::false_type {};

template <class ... Ts>
struct IsList<List<Ts...>> : std::true_type {};


template <class L, class = std::enable_if_t<IsList<L>::value> >
struct reverse_list_On_T {
    using type = rename<decltype(reverse_tuple(rename<L, std::tuple>{})), List>;
};

template <class L>
using reverse_list_On = typename reverse_list_On_T<L>::type;

static_assert(std::is_same_v<reverse_list_On<List<int, char, double, bool, short, void*> >, List<void*, short, bool, double, char, int> >);

int main() {
}

