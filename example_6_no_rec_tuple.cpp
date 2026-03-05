#include "sequence_tuple.hpp"

using namespace std;

#define BOOST_MAGIC_GET_REGISTER_TYPE(Type, Index)                               \
    constexpr size_t type_to_id(type_identity<Type>) noexcept {                  \
        return Index;                                                            \
    }                                                                            \
    constexpr Type id_to_type(integral_constant<size_t, Index > ) noexcept {     \
        return {};                                                               \
    }                                                                            \

// Register all base types here
BOOST_MAGIC_GET_REGISTER_TYPE(short                 , 7)
BOOST_MAGIC_GET_REGISTER_TYPE(int                   , 8)
BOOST_MAGIC_GET_REGISTER_TYPE(char                  , 11)
BOOST_MAGIC_GET_REGISTER_TYPE(float                 , 15)
BOOST_MAGIC_GET_REGISTER_TYPE(double                , 16)
BOOST_MAGIC_GET_REGISTER_TYPE(long double           , 17)
BOOST_MAGIC_GET_REGISTER_TYPE(bool                  , 18)
BOOST_MAGIC_GET_REGISTER_TYPE(void*                 , 19)

#undef BOOST_MAGIC_GET_REGISTER_TYPE

template <class...>
struct List {};
template <class>
struct IsList : false_type {};
template <class ... Ts>
struct IsList<List<Ts...>> : true_type {};

template <class ... Args>
constexpr auto make_sequence_tuple(Args&&... args) {
    return sequence_tuple::tuple<decay_t<Args>...>(forward<Args>(args)...);
}

template <class...Ts, size_t... I>
constexpr auto make_ids(List<Ts...> lst, index_sequence<I...> is) {
    size_t types[sizeof...(I)] = {type_to_id(type_identity<Ts>{})...};
    for (auto b = types, e = &types[sizeof...(I)] - 1; b < e; )
        swap(*b++, *e--); 

    return make_sequence_tuple(types[I]...);
}

template <class L, size_t... I>
constexpr auto reverse_impl(L lst, index_sequence<I...> is) {
    constexpr auto ids = make_ids(lst, is);
    return List<decltype(id_to_type(integral_constant<size_t, get<I>(ids)>{}))... >{};
}

template <class... Ts, template <class...> class L, class = enable_if_t<IsList<L<Ts...>>::value>  >
constexpr auto reverse(L<Ts...> tp) {
    if constexpr(!sizeof...(Ts))
        return L<>{};
    else
        return reverse_impl(tp, make_index_sequence<sizeof...(Ts)>{});
}

static_assert(is_same_v<decltype(reverse(List<>{})), List<> >);
static_assert(is_same_v<decltype(reverse(List<int>{})), List<int> >);
static_assert(is_same_v<decltype(reverse(List<int, char, double, bool, short, void*,
    int, char, double, bool, short, void*,
    int, char, double, bool, short, void*,
    int, char, double, bool, short, void*,
    int, char, double, bool, short, void*
>{})), List<void*, short, bool, double, char, int, 
    void*, short, bool, double, char, int, 
    void*, short, bool, double, char, int,
    void*, short, bool, double, char, int,
    void*, short, bool, double, char, int
> >);

int main() {}
