// C++17
#include <array>
#include <cassert>
#include <iostream>
#include <utility>

using std::index_sequence;
using std::void_t;
using std::make_index_sequence;
using std::enable_if_t;

struct ubiq_constructor {
    size_t ignore;
    template <class T> constexpr operator T&();  // Undefined, allows initialization of reference fields (T& and const T&)
};

template <class T, size_t... I>
auto enable_if_constructible(index_sequence<I...>) -> decltype(T{ ubiq_constructor{I}... });

template <class T, size_t N, class = void_t<>>
struct enable_if_constructible_t : std::false_type {};

template <class T, size_t N>
struct enable_if_constructible_t<T, N, void_t<decltype(enable_if_constructible<T>(make_index_sequence<N>()))>>
    : std::true_type {};

// Первичный шаблон, объединенный с первым рекурсивным случаем
template <class T, size_t Begin, size_t Middle, class = void >
struct detect_data_member_count : detect_data_member_count<T, Middle, Middle + (Middle - Begin + 1) / 2> {};

// Второй рекурсивный случай, исполненный уже в виде специализации
template <class T, size_t Begin, size_t Middle >
struct detect_data_member_count<T, Begin, Middle, enable_if_t<!enable_if_constructible_t<T, Middle>::value>>
    : detect_data_member_count<T, Begin, (Begin + Middle) / 2> {};

// Базовый случай
template <class T, size_t N>
struct detect_data_member_count<T, N, N> {
    static constexpr size_t value = N;
};

// API клиента
template <class T>
constexpr size_t data_member_count() {
    return detect_data_member_count<T, 0, sizeof(T)>::value;
}

// Проверка
struct our_struct {
    int i;
    char c;
    float f;
    double function1(int) {return 42.0; }
    std::array<int, 4> arr;
    void function2(int);
};

static_assert(data_member_count<our_struct>() == 4);

int main() {}

