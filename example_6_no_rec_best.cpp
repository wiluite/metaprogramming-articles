#include <utility>

using namespace std;

template <class...>
struct List {};

template <class T, class U>
constexpr bool operator==(type_identity<T>, type_identity<U>) { return false; }
template <class T>
constexpr bool operator==(type_identity<T>, type_identity<T>) { return true; }

template <class IS>
struct get_impl;

template <size_t... Is>
struct get_impl<std::index_sequence<Is...>> {
    template <class T>
    static constexpr T dummy(decltype(Is, static_cast<void*>(0))..., T*, ...);  // <2> = Is:0,1 = T dummy(void*, void*, char*,...) = (void*, void*, T*,...)
};

template <size_t I, class... Ts>
constexpr auto get(List<Ts...>) {
    return type_identity<decltype
                         (
                             get_impl<make_index_sequence<I> >::dummy(static_cast<Ts*>(nullptr)...)
                         )
    >{};
}

static_assert(get<2>(List<double, int, char, float>{}) == type_identity<char>{});

template <class TypePack, size_t... I>
constexpr auto reverse_impl(TypePack tp, index_sequence<I...>) {
    return List<typename decltype(get<sizeof...(I) - I - 1>(tp))::type...>{};
}

template <class... Ts>
constexpr auto reverse(List<Ts...> tp) {
    return reverse_impl(tp, make_index_sequence<sizeof...(Ts)>{});
}
static_assert(is_same_v<decltype(reverse(List<int, char, double, bool, short, void*,
    int, char, double, bool, short, void*,
    int, char, double, bool, short, void*,
    int, char, double, bool, short, void*,
    int, char, double, bool, short, void*
>{})),
decltype(List<void*, short, bool, double, char, int,
    void*, short, bool, double, char, int,
    void*, short, bool, double, char, int,
    void*, short, bool, double, char, int,
    void*, short, bool, double, char, int
>{})>);

int main() {}
