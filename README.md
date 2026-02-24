## Маленькие статьи по метапрограммированию  

### 1. Эмуляция boost::fusion::remove_if и boost::fusion::transform в современном C++.
Константин Владимиров в 2020 году  
https://www.youtube.com/watch?v=UJqW_eEBA6I  
(момент на видео примерно с 1:21:30)  

привел следующий код: 


```cpp
auto to_string = [] (auto t) { std::stringstream ss; ss << t; return ss.str(); };

fusion::vector<int, std::string, float> seq {1, "abc", 3.4f};
auto noflts = fusion::remove_if<std::is_floating_point<mpl::_>>(seq);
auto strs = fusion::transform(noflts, to_string);
            // -> fusion::vector<string, string>
```
прокомментировав, что это был по-сути std::tuple - подобный кортеж за 3 года до стандартизации, который (кортеж) был гораздо круче, чем 
стандартный кортеж сейчас. И что библиотека Boost.Fusion полна подобных compile-time алгоритмов, а также, что сделать подобное в рамках работы
со стандартными кортежами довольно сложно.  

Мне лично кажется, что в стандартную библиотеку не нужно загонять подобные вещи, поскольку программист сам может написать для себя необходимое
подмножество нужных ему в проектах алгоритмов. На стандартных кортежах это не должно вызывать особых трудностей. И вот как это делается (начиная
с C++14, не считая, правда, того, что в итоговом примере для ограничения типов типами кортежей используются C++20 концепты):

```cpp
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
    struct is_empty<const tuple<>> {
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
    struct front_t<const tuple<Head, Rest...>> {
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
    struct pop_front_t<const tuple<Head, Rest...>> {
        using type = const tuple<Rest...>;
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
```
1. Мы берем список типов нужного кортежа и прогоняем его через предикат (в нашем случае прогоняем через предикат std::is_floating_point), 
получая список индексов тех типов кортежа, элементы которых должны попадать в новый кортеж: метафункция remove_if_index_sequence_t.
2. Далее просто используем этот список индексов для отбора нужных значений из исходного кортежа в кортеж noflts (алгоритм select).
3. А для получения типа с двумя стрингами (tuple < string, string >) используем операцию "MAP" последовательности элементов кортежа на нашу
лямбду. В нашем случае такая функция называется tuple_map() (и должна содержаться в любой маломальски современной библиотеке для функционального
программирования на C++), которая, применяя лямбду, автоматически выводит все нужные типы для результирующего кортежа. Это составляет само
существо операции transform() из исходного примера. 

И на этом всё. Пишем "обертки", воспроизводящие "интерфейс" fusion (как если бы мы работали с настоящими boost::fusion::remove_if и 
boost::fusion::transform):

```cpp
namespace fusion {
    template <typename T>
    concept IsTuple = requires(T t) {
        []<typename... Args>(const std::tuple<Args...>&) {}(t);
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
```

Получившийся результат воспроизводит начальный пример: 

```cpp
int main() {
    auto to_string = [] (auto t) { std::stringstream ss; ss << t; return ss.str(); };

    auto noflts = fusion::remove_if<std::is_floating_point>(make_tuple(1, "abc", 4.3f));
    auto strs = fusion::transform(noflts, to_string);

    assert(get<0>(strs) == "1");
    assert(get<1>(strs) == "abc");
    std::cout << get<0>(strs) << ' ' << get<1>(strs) << '\n';

    // В compile-time убеждаемся что все выведенные типы - те, что ожидаемы.
    static_assert(is_same<decltype(noflts), tuple<int, char const*> >::value);
    static_assert(is_same<decltype(strs), tuple<string, string> >::value);
}
```

P.S.  
Если бы вам по каким-либо причинам захотелось продолжать работать со значениями "1" и "abc" в compile-time, то вам просто пришлось бы придумать
constexpr-лямбду (CPP17), работающую с std::string_view. Сам же наш функционал является compile-time автоматически, как это и было, вероятно, и
в boost::fusion. И да - все вышестоящие различные специализации для const-типов сделаны для поддержки этой возможности:

```cpp
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
```

### 2. Удобное вычисление евклидова расстояния от векторов любой размерности на стандартных кортежах.

https://youtu.be/jL3CNQr-0Cg  
(момент на видео с 1:21:00)

Вспоминая доклад Michael Caisse "Решения мировых проблем с Fusion" от 2013 года, Константин Владимиров указал на удобства решения задачи 
нахождения евклидова расстояния для векторов любой размерности на библиотеке Boost.Fusion. Удобства заключались в "заточенности" кортежей
(fusion::tuple) библиотеки, еe алгоримов и различных функторов под такую задачу. А также в том, что пользователь мог легко адаптировать свои
структуры и классы под конкретную задачу при помощи препроцессорных дефиниций всё той же библиотеки...
Однако, такую задачу даже в 2011-2012 году (не говоря уже о 2013) можно было сравнительно легко решить средствами тогдашней стандартной 
библиотеки и компилятора С++11. Я покажу по шагам, как выглядит это решение без всякой сторонней магии, используя лишь сам язык и его 
разнообразные грани, в которых нет и не было ничего лишнего, которые дополняют друг друга самым естественным образом. Ну а само
метапрограммирование здесь совсем несложно. А главное, чего я не понял, когда посмотрел первую половину доклада, это того, в чем все-таки
были претензии к кортежам, которые нельзя ни улучшить, ни ухудшить: там нечего придумать. Возможно, докладчик просто не знал, что можно
пользоваться "вариадиками" и "pack expansion"? Жду критики.

Итак, что мы имеем в ограниченном C++11:

1. При метапрограммировании на стандартных кортежах (std::tuple) нужна std::make_index_sequence. Ее еще нет, она появляется в C++14. Напишем
сами подобную метафункцию.
2. У нас нет автоматического вывода типов (auto) для аргументов и возвращаемых значений функций. Для аргументов вывод через auto нам сроду не
сдался, а для возвращаемых значений мы будем явно определять trailing return type - только и всего.
3. Нам нужны идеоматические(канонические) операции отображения и свертки (MAP и FOLD) из функционального программирования, которые бы работали
на значениях стандартных кортежей (проще говоря, на кортежах). Их можно взять из библиотеки Cat (автор Nicola Bonelli) или написать самим.
4. В качестве предикатов и функторов мы не можем использовать ни локальные лямбды, ни локальные классы с шаблонными операторами вызова. Но,
поскольку мы знаем, что наши значения в векторах всегда double, в этом нет необходимости(и тогда возможны функторы любой локальности). Если же
нам нужны когда-то и шаблонные варианты оператора вызова, то мы просто вынуждены размещать такие структуры на уровне пространств имен. А как
спасает Fusion?
5. Когда наша структура или класс должны использоваться как векторы для вычисления евклидова расстояния, мы ничего не адаптируем при помощи 
препроцессора: мы пишем в этой структоре или этом классе член-функцию со строго определенным именем и имплементируем там возвращаемый кортеж, 
который и будет принимать участие в вычислениях. Если же мы поборники чистоты и не хотим нарушать OCP, то пишем функцию уровня того же или
охватывающего пространства имен, принимающую аргумент этого класса/структуры, и возващающего кортеж. Далее (для операции MAP, потому что она
выполяется сначала) пишется набор перегруженных функций, принимающих различные сочетания всех возможных аргументов и работающих через
статический полиморфизм, передающий созданные нами кортежи на вычисления.

Вот и все, что нам нужно.

Замена std::make_index_sequence:

```cpp
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
```

Далее. Сначала к всем парным элементам двух кортежей применяется операция MAP как функция квадрата разности двух значений. MAP формирует новый
кортеж из того же количества элементов, являющихся результатом операции всех пар. И вот как она выглядит:

```cpp
namespace map_n_fold {

    namespace detail {
        template <class Fun, class T1, class T2, unsigned... Indices>
        auto tuple_map(Fun fun, T1 tup1, T2 tup2, ValueList<Indices...>) 
            -> decltype(make_tuple(fun(get<Indices>(tup1), get<Indices>(tup2))...)) {
            return make_tuple(fun(get<Indices>(tup1), get<Indices>(tup2))...);
        }
        ......  
    }
    template <class Fun, class ... Cs1, class ... Cs2, typename enable_if<sizeof...(Cs1) == sizeof...(Cs2), void*>::type = nullptr>
    auto tuple_map(Fun fun, tuple<Cs1...> tup1, tuple<Cs2...> tup2)
        ->decltype(detail::tuple_map(fun, tup1, tup2, make_index_seq<sizeof...(Cs1)>{})) {
        return detail::tuple_map(fun, tup1, tup2, make_index_seq<sizeof...(Cs1)>{});
    }
    ......

}
```
detail::tuple_map применяет функцию к каждой паре значений из кортежей, но ко всему кортежу сразу! А сам функтор, принимаемый на вход,
тривиален:

```cpp
    struct difference_squared {
        auto operator()(double num1, double num2) -> double {
            return pow(num1 - num2, 2);
        }
    };
```
	
Операция свертки (FOLD), принимающая на вход кортеж (после MAP), выглядит следующим образом:

```cpp
namespace map_n_fold {

    namespace detail {
        template <typename Fun, typename T, typename TupleT>
        auto tuple_fold(Fun, T acc, TupleT &&, ValueList<>) -> T {
            return acc;
        }

        template <typename Fun, typename T, typename TupleT, unsigned N, unsigned ...Ns>
        auto tuple_fold(Fun fun, T acc, TupleT &&tup, ValueList<N, Ns...>) -> T {
            auto acc1 = fun(std::move(acc), get<N>(tup));
            return tuple_fold(fun, std::move(acc1), std::forward<TupleT>(tup), ValueList<Ns...>{});
        }
    }

    template<typename Fun, typename T, typename TupleT>
    auto tuple_fold(Fun fun, T value, TupleT &&tup) -> decltype(detail::tuple_fold(fun, std::move(value), std::forward<TupleT>(tup), index_tuple<TupleT>{})) {
        return detail::tuple_fold(fun, std::move(value), std::forward<TupleT>(tup), index_tuple<TupleT>{});
    }

}
```
А функтор для FOLD таков:

```cpp
    struct sum_of_squared_differences {
        auto operator()(double sum, double num) -> double {
            sum += num; return sum;
        }
    };
```

Сама универсальная функция подсчета евклидова расстояния для любой размерности удивительно лаконична (как и все остальное):

```cpp
template <class T, class U>
auto euclid(T t, U u) -> double {
    using namespace map_n_fold;
    return sqrt(tuple_fold(sum_of_squared_differences(), 0, tuple_map(difference_squared(), t, u)));
}
```

Клиенты euclid() могут быть написаны весьма разнообразно, и это завершает нашу статью. Заметим, что если as_tuple() не инжектится в наш класс,
то к сожалению, объявления и класса и as_tuple() должны быть доступны семейству перегруженных функций tuple_map. Но это скромная плата за
красоту общего решения и ухода от перегруженного Fusion-подхода:

```cpp
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
```
#### 3. Как делается простая рефлексия и интроспекция при работе с POD-структурами встроенных типов. По мотивам библиотеки tuple_get.

Алгоритмы tuple_get или Boost.PFR для разных пользовательских задач рефлексии и интроспекции могут быть чрезвычайно сложны. Нам же нужно
понимание различных приемов, которые нужно собрать вместе, чтобы включить их в свой программистский арсенал. Поэтому, будут ставиться и решаться
задачи одна за другой. И в конце, возможно, они будет объединены в одно решение о том, как делать рефлексию в C++.

