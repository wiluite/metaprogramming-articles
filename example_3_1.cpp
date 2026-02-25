#include <tuple>
#include <type_traits>
#include <iostream>
#include <utility>
#include <cassert>

using tuple_template = std::tuple<int, char, float>;

struct our_struct {
    int i = 53;
    char c = '.';
    float f = 42.1;
};

template <typename Struct, typename Tup>
class offset_based_getter;

template <typename Struct, typename ... Types>
class offset_based_getter<Struct, std::tuple<Types...>> {

    using this_t = offset_based_getter<Struct, std::tuple<Types...>>;

    using Tup_t = std::tuple<Types...>;

    static_assert(sizeof(Struct) == sizeof(Tup_t), "Тип структуры и тип кортежа имеют разные размеры!");
    static_assert(alignof(Struct) == alignof(Tup_t), "Значения выравниваний у типов структуры и кортежа разные!");

    static_assert(!std::is_const<Struct>::value, "Для использования offset_based_getter константность должна быть снята");
    static_assert(!std::is_reference<Struct>::value, "Для использования offset_based_getter ссылочность должна быть снята");
    static_assert(!std::is_volatile<Struct>::value, "Для использования offset_based_getter волатильность должна быть снята");

    // Получить тип idx-члена
    template <std::size_t idx>
    using index_t = typename std::tuple_element<idx, Tup_t>::type;

    template<class TupleElementType>
    struct internal_storage {
        //char storage_[sizeof(TupleElementType)];
        alignas(alignof(TupleElementType)) char storage_[sizeof(TupleElementType)];
    };

    // Получить смещение члена по индексу idx
    // Идея: Размещение объекта имеет те же смещения, что и смещения Tup, таким образом, если Tup и Struct "layout compatible", тогда и любые
    // вычисления смещений - также корректны
    template <std::size_t idx>
    static std::ptrdiff_t offset() noexcept {
        std::tuple<internal_storage<Types>...> layout{};
        constexpr auto max_idx = std::tuple_size<Tup_t>::value - 1;
        return &std::get<max_idx-idx>(layout).storage_[0] - &std::get<max_idx>(layout).storage_[0];
    }

    // Инкапсулирует арифметику смещений и переинтерпретацию типов (reinterpret_cast)
    // Кастует найденное смещение на тип элемента по запрошенному индексу. Находит само смещение - член-функция выше
    template <std::size_t idx>
    static const index_t<idx> * get_pointer(const Struct * u) noexcept {
        return reinterpret_cast<const index_t<idx> *>(reinterpret_cast<const char *>(u) + this_t::offset<idx>());
    }

    // Перегрузка (для неконстантного Struct)
    template <std::size_t idx>
    static index_t<idx> * get_pointer(Struct * u) noexcept {
        return reinterpret_cast<index_t<idx> *>(reinterpret_cast<char *>(u) + this_t::offset<idx>());
    }

public:

    // Получает значение по индексу из пользовательской структуры. Тип значения index_t<idx> берется из кортежа, последовательность типов 
    // которого совпадает с последовательностью типов в структуре.
    template <std::size_t idx>
    index_t<idx> const & get(Struct const & u) const noexcept {
        return *this_t::get_pointer<idx>(std::addressof(u));
    }

    // Перегрузка (для неконстантного Struct)
    template <std::size_t idx>
    index_t<idx> & get(Struct & u) const noexcept {
        return *this_t::get_pointer<idx>(std::addressof(u));
    }

    // нужен, чтобы построить индексную последовательность при необходимости
    static constexpr std::size_t tuple_size = sizeof...(Types);
};

namespace detail {
    template <class UserType, class Getter, std::size_t... Indices>
    auto make_tuple_from_class(UserType & s, Getter & g, std::index_sequence<Indices...>) {
        return std::tie(g.template get<Indices>(s)...);
    }
}
template <class UserType, class Getter>
auto make_tuple_from_class(UserType & s, Getter & g) {
    return detail::make_tuple_from_class(s, g, std::make_index_sequence<Getter::tuple_size>{});
}

int main() {
    our_struct s1;

    offset_based_getter<our_struct, tuple_template> getter;

    auto tup = make_tuple_from_class(s1, getter);
    assert(std::get<0>(tup) == 53);
    assert(std::get<1>(tup) == '.');  
    assert(std::get<2>(tup) == 42.1f);
    std::get<2>(tup) = 43.0f;
    assert(std::get<2>(tup) == 43.0);
}
