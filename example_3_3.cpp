#include <utility>
#include <iostream>
#include <array>
#include <tuple>
#include <string>

#if __cplusplus > 201402L // C++17

#include <string_view>

template <class T>
constexpr
std::string_view
type_name() {
    using namespace std;
    std::string_view pp = __PRETTY_FUNCTION__;
    return {pp.data() + 49, pp.find(';', 49) - 49};
}
#endif

using namespace std;

struct our_struct {
    int i;
    char c;
    float f;
};

namespace detail {

    template <size_t Index>
    using size_t_ = integral_constant<size_t, Index >;

    template <class T> struct ident{
        using type = T;
    };

    namespace typeid_conversions {
        #define BOOST_MAGIC_GET_REGISTER_TYPE(Type, Index)              \
            constexpr size_t type_to_id(ident<Type>) noexcept {         \
                return Index;                                           \
            }                                                           \
            constexpr Type id_to_type( size_t_<Index > ) noexcept {     \
                return {};                                              \
            }                                                           \

        // Register all base types here
        BOOST_MAGIC_GET_REGISTER_TYPE(unsigned char         , 1)
        BOOST_MAGIC_GET_REGISTER_TYPE(unsigned short        , 2)
        BOOST_MAGIC_GET_REGISTER_TYPE(unsigned int          , 3)
        BOOST_MAGIC_GET_REGISTER_TYPE(unsigned long         , 4)
        BOOST_MAGIC_GET_REGISTER_TYPE(unsigned long long    , 5)
        BOOST_MAGIC_GET_REGISTER_TYPE(signed char           , 6)
        BOOST_MAGIC_GET_REGISTER_TYPE(short                 , 7)
        BOOST_MAGIC_GET_REGISTER_TYPE(int                   , 8)
        BOOST_MAGIC_GET_REGISTER_TYPE(long                  , 9)
        BOOST_MAGIC_GET_REGISTER_TYPE(long long             , 10)
        BOOST_MAGIC_GET_REGISTER_TYPE(char                  , 11)
        BOOST_MAGIC_GET_REGISTER_TYPE(wchar_t               , 12)
        BOOST_MAGIC_GET_REGISTER_TYPE(char16_t              , 13)
        BOOST_MAGIC_GET_REGISTER_TYPE(char32_t              , 14)
        BOOST_MAGIC_GET_REGISTER_TYPE(float                 , 15)
        BOOST_MAGIC_GET_REGISTER_TYPE(double                , 16)
        BOOST_MAGIC_GET_REGISTER_TYPE(long double           , 17)
        BOOST_MAGIC_GET_REGISTER_TYPE(bool                  , 18)
        BOOST_MAGIC_GET_REGISTER_TYPE(void*                 , 19)
        BOOST_MAGIC_GET_REGISTER_TYPE(const void*           , 20)
        BOOST_MAGIC_GET_REGISTER_TYPE(volatile void*        , 21)
        BOOST_MAGIC_GET_REGISTER_TYPE(const volatile void*  , 22)
        BOOST_MAGIC_GET_REGISTER_TYPE(nullptr_t             , 23)

        #undef BOOST_MAGIC_GET_REGISTER_TYPE
    } /// typeid_conversions


    using namespace typeid_conversions;

    struct ubiq_val {
        size_t* ref_;

        constexpr void assign(size_t val) const noexcept {
            *ref_ = val;
        }

        template <class Type>
        constexpr operator Type() const noexcept {
            assign(type_to_id(ident<Type>{}));
            return Type();
        }
    };

    template <size_t... I>
    constexpr auto foo_tuple(size_t * data, index_sequence<I...>) {
        return make_tuple(data[I]...);
    }

    // Детектирование типов, сохранение их идентификаторов во вспомогательном кортеже
    template <class T, size_t... I>
    constexpr auto type_to_tuple_of_ids() noexcept {
        size_t types[sizeof(T)]{};
        
        T { ubiq_val{types + I}... };

        return foo_tuple(types, make_index_sequence<sizeof...(I)>());
    }

    // Проверка 
    constexpr auto foo_tup = type_to_tuple_of_ids<our_struct, 0, 1, 2>();
    static_assert(get<0>(foo_tup) == type_to_id(ident<int>{})); // == 8 запомненный идентификатор для int
    static_assert(get<1>(foo_tup) == type_to_id(ident<char>{})); // == 11 запомненный идентификатор для char
    static_assert(get<2>(foo_tup) == type_to_id(ident<float>{})); // == 15 запомненный идентификатор для float

    // Реализация построения целевого кортежа 
    template <class T, size_t ... Indices>
    constexpr auto as_tuple_impl(index_sequence<Indices...>) {
        // Вызов операции детектирования типов и сохранения их идентификаторов
        constexpr auto a = type_to_tuple_of_ids<T, Indices...>();
        // Вытаскивание типов по привязанным идентификаторам из вспомогательного кортежа и конструирование целевого типа кортежа 
        return tuple<decltype(id_to_type(size_t_<get<Indices>(a)>{}))...>();
    }

    // Набор кода из примера 3.2.
    struct ubiq_constructor {
        size_t ignore;
        template <class T> constexpr operator T&(); // Undefined, allows initialization of reference fields (T& and const T&)
    };

    template <class T, size_t... I>
    auto enable_if_constructible(index_sequence<I...>) -> decltype(T{ ubiq_constructor{I}... });

    template <class...>
    using void_t = void;

    template <class T, size_t N, class = void_t<>>
    struct enable_if_constructible_t : std::false_type {};

    template <class T, size_t N>
    struct enable_if_constructible_t<T, N, void_t<decltype(enable_if_constructible<T>(make_index_sequence<N>()))>> 
        : std::true_type {};

    template <class T, size_t Begin, size_t Middle, class = void >
    struct detect_data_member_count : detect_data_member_count<T, Middle, Middle + (Middle - Begin + 1) / 2> {};

    template <class T, size_t Begin, size_t Middle >
    struct detect_data_member_count<T, Begin, Middle, enable_if_t<!enable_if_constructible_t<T, Middle>::value>> 
        : detect_data_member_count<T, Begin, (Begin + Middle) / 2> {};

    template <class T, size_t N>
    struct detect_data_member_count<T, N, N> {
        static constexpr size_t value = N;
    };

    template <class T>
    constexpr size_t data_member_count() {
        return detect_data_member_count<T, 0, sizeof(T)>::value;
    }

} /// namespace


template <class Struct>
constexpr auto as_tuple() noexcept {
    return detail::as_tuple_impl<Struct>(make_index_sequence<detail::data_member_count<Struct>()>{});
}

// Проверка 
auto res = as_tuple<our_struct>();
static_assert(std::is_same<decltype(res), std::tuple<int, char, float>>::value);

constexpr auto res2 = as_tuple<our_struct>();
// если res2 - constexpr, в static_assert можно сравнивать объекты
static_assert(res2 == std::tuple<int, char, float>(0, '\0', 0.0f));
// а тип будет константный
static_assert(std::is_same<decltype(res2), std::tuple<int, char, float> const>::value);

int main() {
    auto res = as_tuple<our_struct>();
#if __cplusplus > 201402L // C++17
    cout << type_name<decltype(res)>() << endl;
#endif
}

