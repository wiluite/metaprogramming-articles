## Маленькие статьи по метапрограммированию  

(Маленькое примечание: если переходы по ссылкам не работают, приношу извинения за неудобства. У меня в Typora всё работает.)

*    [Эмуляция boost::fusion::remove_if и boost::fusion::transform в современном C++](#эмуляция-boost::fusion::remove_if-и-boost::fusion::transform-в-современном-C++)
*    [Удобное вычисление евклидова расстояния от векторов любой размерности на стандартных кортежах](#удобное-вычисление-евклидова-расстояния-от-векторов-любой-размерности-на-стандартных-кортежах)
*    [Простая рефлексия и интроспекция при работе с POD-структурами из встроенных типов](#простая-рефлексия-и-интроспекция-при-работе-с-POD-структурами-из-встроенных-типов)
*    [О использовании constexpr в constexpr-функциях](#о-использовании-constexpr-в-constexpr-функциях)
*    [Compile-time определитель размера массива](#compile-time-определитель-размера-массива)
*    [Нерекурсивный разворот списка. История reverse и 2-3 решения вопроса](#нерекурсивный-разворот-списка.-История-reverse-и-2-3-решения-вопроса)


### Эмуляция boost::fusion::remove_if и boost::fusion::transform в современном C++

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
получая список индексов тех типов кортежа, элементы которых должны попадать в новый кортеж. Все это метафункция remove_if_index_sequence_t.
2. Используем этот список индексов для отбора нужных значений из исходного кортежа в кортеж noflts (алгоритм select).
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

### Удобное вычисление евклидова расстояния от векторов любой размерности на стандартных кортежах

https://youtu.be/jL3CNQr-0Cg  
(момент на видео с 1:21:00)

Вспоминая доклад Michael Caisse "Решения мировых проблем с Fusion" от 2013 года, Константин Владимиров указал на удобства решения задачи 
нахождения евклидова расстояния для векторов любой размерности на библиотеке Boost.Fusion. Удобства заключались в "заточенности" кортежей
(fusion::tuple) библиотеки, еe алгоримов и различных функторов под такую задачу. А также в том, что пользователь мог легко адаптировать свои
структуры и классы под конкретную задачу при помощи препроцессорных дефиниций всё той же библиотеки...
Однако, такую задачу даже в 2011-2012 году (не говоря уже о 2013) можно было сравнительно легко решить средствами тогдашней стандартной 
библиотеки и компилятора С++11. Я покажу по шагам, как выглядит это решение без всякой сторонней магии, используя лишь сам язык и его 
разнообразные грани, в которых нет и не было ничего лишнего, которые дополняют друг друга самым естественным образом. Ну а само
метапрограммирование здесь совсем несложно. А главное, чего я не понял, когда посмотрел первую половину доклада, это того, в чем все-таки были 
претензии к кортежам, которые нельзя ни улучшить, ни ухудшить: там нечего придумать. Возможно, докладчик просто не знал, что можно пользоваться
"вариадиками" и "pack expansion"? Жду критики.

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
охватывающего пространства имен, принимающую аргумент этого класса/структуры, и возвращающую кортеж. Далее (для операции MAP, потому что она
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
Пункт 5. из наших заметок мы здесь продемонстрировали лишь отчасти: набор перегруженных функций tuple_map можно посмотреть в исходном коде.
 
### Простая рефлексия и интроспекция при работе с POD-структурами из встроенных типов

Алгоритмы magic_get или Boost.PFR для разных пользовательских задач рефлексии и интроспекции могут быть чрезвычайно сложны. Нам же нужно
понимание различных приемов, которые хочется собрать вместе, чтобы включить их в свой программистский арсенал. Поэтому, задачи будут решаться
одна за другой. И в конце, возможно, они будут объединены в одно решение о том, как делать рефлексию в C++.

#### Заполнить кортеж значениями из POD-структуры

Мы имеем известный тип кортежа. Его разнородный список типов внезапно совпадает с последовательностью типов членов-данных POD-структуры. Нужно
создать объект-кортеж, заполнив его данными из объекта-структуры (впоследствии работая и с ними и с объектом-структорой через интерфейс
кортежей), не обращаясь к членам по имени.

Например, тип кортежа и объект-структура таковы:

```cpp
using tuple_template = std::tuple<int, char, float>;

struct our_struct {
    int i = 53;
    char c = '.';
    float f = 42.1;
};
```
Нужно создать набор кода, в конечном итоге возвращающий объект:

```cpp
std::tuple<int, char, float>(53, ".", 42.1)
```

Итак, причина, по которой нельзя обращаться к членам классов по имени, состоит в том, что мы хотим работать с классами как с кортежами
(обращаться к элементам различных классов с различными именами членов по порядковому номеру). А это в метапрограммировании необходимо для задач 
автоматизации обработки данных, сериализации, динамического создания типов и высокопроизводительной обобщенной обработки, где важна
неизменяемость. Мы можем использовать это, например, для печати любой такой структуры, на разрабатывая каждый раз отдельный оператор вывода в
поток.

Поэтому, первый шаг решения заключается в извлечении значения из объекта-структуры по порядковому номеру (индексу), имея в качестве опоры или
своеобразного трафарета/каркаса (для определения того, с какого места в памяти начинается каждый член структуры) кортеж с полностью совпадающей
последовательностью типов. Как и откуда вообще такая опора-трафарет возникла - тема будущих задач, связывающих методы рефлексии в единое целое.
Пока же напишем нужный нам класс и функцию-член для получения N-го значения из объекта-структуры, придав ей тот же интерфейс шаблонной функции
std::get() при работе с кортежами.

Итак, как располагаются в памяти 3 члена-данных структуры мы знаем: по смещению 0 - член типа int, по смещению 4 - член типа char, далее идут 3
байта для выравнивания следующего члена, и по смещению 8 - член типа float. Но это-то мы знаем для конкретного примера, а не для обобщенной
структуры. Посмотрим теперь, можно ли сопоставить им расположения элементов кортежа. Для этого распечатаем адреса каждого элемента кортежа и
проанализируем:

```cpp
    tuple_template t (53, '.', 42.1);

    std::cout << std::get<0>(t) << '\n'
              << (std::size_t)std::addressof(std::get<0>(t)) << '\n'
              << std::get<1>(t) << '\n'
              << (std::size_t)std::addressof(std::get<1>(t)) << '\n'
              << std::get<2>(t) << '\n'
              << (std::size_t)std::addressof(std::get<2>(t)) << '\n';
```

> 53  
979059604668  
.  
979059604664  
42.1  
979059604660  

Ага! Стандартный кортеж, оказывается, хранит свои члены-данные в обратном порядке. Но смещения - те же(0, 4, 8), как и ожидалось. 

*(маленькое примечание: если ваш компилятор не GCC или стандартная библиотека не libstdc++, то, естественно, делаете поправочки на ветер, так
как там размещение внутри std::tuple может быть совсем иным, а то и размер кортежа не соответствовать сумме размеров его членов - тогда данное
решение не годится).*

Для нас это означает, что при обращании по индексу к объекту нашей структуры придется адресную арифметику выстраивать наоборот, но ничего
страшного тут нет. Итак, пишем класс для извлечения N-го значения из структуры:

```cpp
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
        char storage_[sizeof(TupleElementType)];
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
```

Смещения данных мы вычисляем в обратном порядке:

```cpp
    return &std::get<max_idx-idx>(layout).storage_[0] - &std::get<max_idx>(layout).storage_[0];
```
а не в прямом, если бы элементы кортежа располагались у нас по возрастанию:

```cpp
    return &std::get<idx>(layout).storage_[0] - &std::get<0>(layout).storage_[0];
```
Перед вычислением смещения, мы конструируем кортеж, расчитывая, что его элементы "лягут" сообразно тем, что в объекте-структуре (только в
обратном порядке).

Тестируем получившийся класс:

```cpp
    our_struct s1 {53, '.', 42.1f} ;

    offset_based_getter<our_struct, tuple_template> getter;

    std::cout << getter.get<0>(s1) << '\n'
              << (std::size_t)std::addressof(getter.get<0>(s1)) << '\n'
              << getter.get<1>(s1) << '\n'
              << (std::size_t)std::addressof(getter.get<1>(s1)) << '\n'
              << getter.get<2>(s1) << '\n'
              << (std::size_t)std::addressof(getter.get<2>(s1)) << '\n';
```

>53  
461511849704  
.  
461511849708  
6.46245e-27  
461511849709  

Мы видим, что первые два значения вывелись правильно (53 и '.'). Но третьим значением вывелся мусор, поскольку у него неправильный адрес
461511849709, имеющий смещение 5 от начала, а не 8, как это должно быть. То есть, когда мы конструировали расположение объектов кортежа:

```cpp
std::tuple<internal_storage<Types>...> layout{};
```
компилятор сконструировал их по месту без выравнивания, которое нужно применять специально в таких случаях. Выравнивание второго члена структуры
(тип сhar) оказалось правильным потому что его выравнивание всегда 1 (подходит любой адрес), а выравнивание первого члена работает потому что
саму структуру компилятор выравнивает по (правильному) умолчанию.

Чтобы осуществить правильное выравнивание для всех членов необходимо определить storage_ как:

```cpp
alignas(alignof(TupleElementType)) char storage_[sizeof(TupleElementType)];
```

После этих изменений класс для извлечения N-го значения из структуры будет полностью рабочим.

Ну а теперь, нужно получить ВСЕ значения из структуры при помощи этого же шаблонного члена get. Это делается буквально несколькими строками:

```cpp
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

// Вызов:
auto tup = make_tuple_from_class(s1, getter);
// Результаты:
assert(std::get<0>(tup) == 53);
assert(std::get<1>(tup) == '.');
assert(std::get<2>(tup) == 42.1f);
// меняем член структуры через кортеж! (поскольку tup это кортеж неконстантных ссылок: s1 - неконстанта)
std::get<2>(tup) = 43.0f;
assert(s1.f == 43.0f);
```
AWESOME!

В magic_get версии многолетней давности, которую я изучал, место make_tuple_from_class() занимает множество функций стартующих с тамошней
make_flat_tuple_of_references() и представляющих собой какой-то умопомрачительный код бинарного размежевания до единичного значения, а затем
сбора (мержа) этих значений в кортеж. Зачем это было сделано - версия лишь одна - для правильной обработки вложенных подкортежей, ведь сами
агрегаты могут быть весьма навороченными, а решения проблем рефлексии в magic_get всесторонние, наша же статья - всего лишь демонстрация.

#### Написать метапрограмму, вычисляющую количество членов-данных(полей) класса-агрегата

Это, изумительное по красоте решение, позаимствовано из magic_get, но немного переделано для лучшей удобоваримости.

```cpp
struct ubiq_constructor {
    size_t ignore;
    template <class T> constexpr operator T&(); 
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
```
Итак, любой агрегат можно инициализировать либо меньшим числом инициализаторов, чем имеется членов-данных у агрегата, либо равным числу
членов-данных агрегата. Но инициализировать агрегат большим числом инициализаторов, ошибка компиляции. Это свойство и используется в проверке
выражения:

```cpp
decltype(T{ ubiq_constructor{I}... })
```
на его законность в различных вышестоящих и его использующих конструкциях.

Сначала (до выражения) строится некая последовательность I (0, 1, ... , N) объектов типа size_t. Затем из каждого такого объекта строятся
объекты-агрегаты класса ubiq_constructor, каждый из которых агрегатно (внутренние фигурные скобки) инициализирует свой единственный член ignore
значением типа size_t. После того, как последовательность ubiq_constructor-объектов создана, она собою, опять-таки агрегатно (внешние фигурные
скобки), инициализирует уже наш агрегатный класс T. Шаблонный оператор пользовательского преобразования в классe ubiq_constructor гарантирует,
что каков бы ни был тип у каждого очередного члена-данного агрегата, его объект будет гарантировано сконструирован и проинициализирован.
Шаблонный оператор пользовательского преобразования, позволяющий приводить правый объект в любой объект присваивания - один из основополагающих
приемов рефлексии/интроспекции.

Таким образом, задача низлежащего алгоритма состоит в подборе значения N, означающего точное количество членов-данных агрегата. И этим
занимаются две рекурсивные ветки этого алгоритма, представляющего собой частный случай бинарного поиска: первый рекурсивный случай корректирует
N в большую сторону, а второй - в меньшую. Обе рекурсии работают с двумя числами, которые в итоге сходятся в N.

Как это реализуется в C++. В каждом рекурсивном вызове для компилятора не должно создаваться неопределенности из двух возможностей и базового
случая (должно быть однозначное разрешение перегрузки). И как всегда это гарантируют уточненные частичные специализации. А благодаря тому, что
мы додумались один рекурсивный случай совместить с основным шаблоном, а другой сделать специализацией, условие разрешения/запрета перегрузки
enable_if_constructible_t достаточно прописать только один раз в специализации. В контр-партнере будет естественно работать "инверсия" условия.
Нужно еще раз подчеркнуть, что разрешающее условие для второго случая рекурсии сработает не потому, что SFINAE при очередном рекурсивном
инстанцировании что-то "выкинет" (нельзя выкинуть основной шаблон класса с телом, никак не упоминающим наше условие разрешения/запрета
перегрузки), а потому, что специализация, представляющая второй рекурсивный случай, станет более подходящей для вызова.

Реализация в magic_get построена на шаблонах constexpr-функций, помимо условия разрешения перегрузки принимает 1 дополнительный параметр, а
также требует forward-объявления одной из двух рекурсивных функций. Наш вариант, построенный на шаблонах классов, чуть-чуть лаконичней и ясней.

Проверка:

```cpp
struct our_struct {
    int i;
    char c;
    float f;
    double function1(int) {return 42.0; }
    std::array<int, 4> arr;
    void function2(int);
};

static_assert(data_member_count<our_struct>() == 4);
```

#### Создание кортежей со списком типов POD-структуры

После освоения п.3.1 мы уже умеем работать с POD-структурами через кортежный интерфейс. Разнородный список типов кортежа должен совпадать с
последовательностью типов членов-данных POD-структуры, прежде чем будет организована работа с самими данными объекта-структуры. Приведем
описание того, как построить кортеж с такой последовательностью типов. А шагов всего 2:

1. Детектирование типов, сохранение их идентификаторов в (например) вспомогательном кортеже.
2. Вытаскивание типов по привязанным идентификаторам из вспомогательного кортежа и конструирование целевого типа кортежа.

Итак, при инициализации членов-данных агрегата мы имеем (описанный в 3.2) шаблонный оператор пользовательского преобразования, позволяющий
приводить правый объект в любой объект присваивания. Таким образом мы бесплатно получаем детектирование типов всех членов-данных нашей
структуры. Сопоставив уникальный идентификатор каждому типу, которыми собираемся пользоваться (а нас устроят фундаментальные типа C++), мы можем
как сохранять эти типы (дополнив кодом шаблонный оператор пользовательского преобразования), так и восстанавливать их для построения
разнородного списка типов кортежа.

Инфраструктура для сохранения и восстановления типов:

```cpp
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
    //  и так далее
```

Детектирование типов, сохранение их идентификаторов во вспомогательном кортеже и возврат клиенту:

```cpp

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

    template <class T, size_t... I>
    constexpr auto type_to_tuple_of_ids() noexcept {
        size_t types[sizeof(T)]{};
        
        T { ubiq_val{types + I}... };

        return make_tuple(types[I]...);
    }

```

Операция (нижняя строка) вытаскивания типов по привязанным идентификаторам из вспомогательного кортежа и конструирование целевого типа кортежа:

```cpp
    // Реализация построения целевого кортежа
    template <class T, size_t ... Indices>
    constexpr auto as_tuple_impl(index_sequence<Indices...>) {
        constexpr auto a = /* кортеж готовых идентификаторов */
        return tuple<decltype(id_to_type(size_t_<get<Indices>(a)>{}))...>();
    }

```

API и проверка:

```cpp

template <class Struct>
constexpr auto as_tuple() noexcept {
    return detail::as_tuple_impl<Struct>(make_index_sequence<detail::data_member_count<Struct>()>{});
}

struct our_struct {
    int i;
    char c;
    float f;
};

auto res = as_tuple<our_struct>();
static_assert(std::is_same<decltype(res), std::tuple<int, char, float>>::value);

constexpr auto res2 = as_tuple<our_struct>();
// если res2 - constexpr, в static_assert можно сравнивать объекты
static_assert(res2 == std::tuple<int, char, float>(0, '\0', 0.0f));
// а тип будет константный
static_assert(std::is_same<decltype(res2), std::tuple<int, char, float> const>::value);

```

### О использовании constexpr в constexpr-функциях

В начале лекции "Волшебство времени компиляции" (курс 2022-2023 гг) К. Владимиров сообщает: *"constexpr - он именно поэтому и expr, что это
expression. И как expression, это элемент синтаксиса языка. Компилятор тупо смотрит на синтаксис. По синтаксису объект init типа
initializer_list< int > это constant expression? Нет, по синтаксису это не core constant expression. Всё, оно не может быть использовано, чтобы
инициализировать core constant expression. Синтаксическая проверка не проходит, и это очень важно. Констэкспры почти всегда хотят синтаксической
проверки. Это немножко облегчает реализацию компилятора.".*

И несколько далее (20-я минута) следует коментарий на такой код (код изменен мной для примеров):

```cpp
template <typename T, size_t N> class array_result {
    T data_[N] {0, 1, 2};
public:
    constexpr T& operator[](size_t n) { return data_[n]; }
};
```
*"и вот этот оператор[](), который возвращает data_[ n ], и который, в общем случае, возвращает модифицируемую ссылку. Я, когда это думаю, мне
всегда кажется, что это ужасно. Помните, когда мы говорили о классическом  метапрограммировании, мы говорили, что программа времени компиляции,
она похожа на программу, написанную в функциональном стиле. На этапе компиляции мы ничего не можем изменить. Мы можем порождать новые типы,
которые как мы помним компилятор не забывает. Но что значит изменить? Как вообще может произойти изменение данных на этапе компиляции? <..>
Инстанцирование инстанцирует что-то 1 раз, оно не может передумать и изменить ранее инстанцированное. То есть вот эти фундаментальные процессы,
на которых строится семантика языка, они крайне плохо бьются с constexpr. Вот это введение идеи, что у нас может быть что-то модифицируемо на
этапе компиляции, оно относится к 2014 году, в 2011 такого не было. И это породило расхождение процесса вычисления constexpr с процессом
инстанцирования. Потому что constexpr-функция 2011 года это просто был сахар для инстанцирования: взяли процесс инстанцирования и засахарили
его. А вот эти - это уже не инстанцирование. И тогда вопрос: а что это? Какой это процесс? Это называется constexpr evaluation. То есть, кроме
инстанцирования, разрешения имен и вывода типов у нас добавился новый процесс. Evaluation - оценка. И вот в этом процессе оценки константные
выражения могут быть изменены".*

Да, действительно, так и было:

```cpp
// возможно в C++14, невозможно в C++11
constexpr int foo(int x) {
    return (++x, x);
}
constexpr auto foo_result = foo(41);
```

Но на мой взгяд, объяснения усложнены узкоцеховыми подробностями. А именно. Какая разница, какими средствами будет обходится constexpr-функция
в своей реализации и реализации своего вычисления (в конце концов алгоритм всегда подразумевает неизбежную изменчивость), если она для
пользователя будет чистой функцией: иметь взаимно-однозначно определенные результаты на всем множестве возможных "входных" значений? А раз так,
то и с процессом однозначного/одноразового инстанцирования всех (нужных!) сущностей компилятором противоречий не возникает. Если в процессе
"оценки" определяется фактическая чистота constexpr-функции, то все хорошо.

Сonstexpr-NESS функции заключается не в подробностях и второстепенностях: мутабельности или константности ее тела, а в constexpr-NESS
результате вызова и его (результата) взаимной, с исходными данными, однозначности для внешнего наблюдателя и оценщика. А то, что является
мутируемым до результата, может и должно мутироваться во время компиляции.

```cpp
// результат 1
static_assert(array_result<int, 10>()[1] == 1);

// результат все равно 1 (новый объект)
static_assert((array_result<int, 10>()[1] = 2, array_result<int, 10>()[1]) == 1);

// несмотря на данные "извне", operator[]() - чистая функция согласно определению!
static_assert((array_result<int, 10>()[1] = 2) == 2);
static_assert((array_result<int, 10>()[1] = 3) == 3);
```

В ходе же своего вычисления, функция может использовать как свои автоматические объекты, так и полученные аргументы. Автоматические объекты
можно определить и как constexpr (с constexpr-инициализаторами), и как не-constexpr. Но неинициалированными они быть не могут согласно одному
пункту таблицы ограничений на тело constexpr-функций (с C++14), приводимой на одном из слайдов лекции. И в этом залог фактической чистоты
функций.

А что же аргументы constexpr-функции? Они ведь те же автоматические объекты, которые лежат в стеке, только инициализированные внешним кодом,
вызывающим нашу функцию. Таким образом, для нашей функции складывается ситуация некоторой неопределенности. Она, в отсутствие специального
спецификатора constexpr для аргументов, не может считать такой аргумент constexpr-выражением, даже если вызвана с результатом вычисления другой
constexpr-функции (наверняка это - причина предложения в стандарт constexpr-аргументов). Для обхода этого положения в функции нужно создать
отдельный автоматический constexpr-объект, инициализируемый результатом вызова нужной constexpr-функции. А вот это, похоже, и есть подлинная
двусмысленность constexpr-функций ("двойная природа constexpr функций", как написано на одном слайде), которая лежит не в части спецификаторов
стековых объектов (constexpr oни или нет), а в том, что несмотря на то, что хотя функции - фактически чистые функции, к ним парадоксальным
образом нельзя применять азы функционального программирования. То есть передавать одну функцию на вход другой. Вот почему в примере (п.3.3.
выше) мы написали все вызовы так, как написали. Если мы вызовем as_tuple_impl(), передав ей type_to_tuple_of_ids() как второй аргумент:

```cpp
detail {
    template <class T, size_t ... Indices, class U>
    constexpr auto as_tuple_impl(index_sequence<Indices...>, U a) {
        return tuple<decltype(id_to_type(size_t_<get<Indices>(a)>{}))...>();
    }
}

template <class Struct>
constexpr auto as_tuple() noexcept {
    return detail::as_tuple_impl<Struct>(
       make_index_sequence<detail::data_member_count<Struct>()>{},
       detail::type_to_tuple_of_ids<Struct, 0, 1, 2>()
    );
}

```
то не сможем использовать "a" в следующем constexpr-выражении, потому что у "a" нет шансов быть constexpr. И чтобы убедиться, что это
повсеместно так, достаточно посмотреть, к примеру, код библиотеки magic_get/Boost.PFR, модуль core14_classic.hpp. Где constexpr-объекты внутри
constexpr-функций используются ну просто на все деньги.

Если вы не согласны с моими рассуждениями, пишите.

### Compile-time определитель размера массива

Конечно же, Скотт Мейерс не придумывал то красивое решение, о котором говорит докладчик в том же видео (момент времени 0:59:20). Он просто
адаптировал известное решение к естественному использованию его в constexpr-функции. Это решение описано в книге Мэтью Уилсона "Imperfect C++"
от 2005 года. Русское издание от 2006 года называется: "С++. Практический подход к решению проблем программирования", Раздел 14.3, стр. 284.
И выглядит это решение так:

Определяется шаблоная структура с целочисленным параметром, с членом-массивом с количеством элементов от значения аргумента шаблона:

```cpp
template <int N>
struct array_size_struct {
    unsigned char c[N];
};
```
Объявляется, но не определяется, шаблонная функция, принимающая ссылку на массив от числа N и типа T элементов и возвращающая структуру:

```cpp
template <class T, int N>
array_size_struct<N> array_size(T (&) [N]);
```

Это решение работает в compile-time для С++98/03. Продемонстрируем, что оно работает и по сей день:

```cpp
int keyVals[] =  {1,3,7,9,11,22,35};
int mappedVals[sizeof(array_size(keyVals).c)];
```

Для удобства (и ни для чего более) определяется макрос. И результат по мановению рук превращается в вариант Мейерса:

```cpp
#define arraySize(A) sizeof(array_size(A).c)

int mappedVals[arraySize(keyVals)];
```

Причина, по которой написана эта крошечная статья, в том, что Мэтью Уилсон незаслуженно забытый автор прекрасных двух книг по "старому" C++.


### Нерекурсивный разворот списка. История reverse и 2-3 решения вопроса

Поскольку ранее 2014 года не было constexpr-функций, были лишь прежние "квадранты" метапрограммирования на списках типов и кортежах значений. А
значит было одно лишь функциональное программирование, где без рекурсии невозможно решать задачи. Проговорим еще раз то, в чем заключается
вычислительная полнота шаблонов(классов): Переменные состояния - это параметры шаблона, циклы - это рекурсия, выбор путей выполнения - это
специализация + шаблонные условные выражения, и, наконец, 4-я ипостась вычислительной полноты - целочисленная арифметика, всегда доступная в
шаблонах.

Здесь мы решим 4 проблемы: 

* улучшение асимптотической сложности алгоритма
* уменьшение времени компиляции путем уменьшения количества инстанцирований при вычислениях
* уменьшение глубины рекурсии
* избавление от рекурсии

Канонический функциональный reverse осуществляется добавлением головы текущего списка к хвосту списка, с которым осуществляется та же операция,
но без упомянутой головы. Так как списки типов по своей природе - это однонаправленные списки (наподобии std::forward_list), то добавить элемент
в конец списка можно только за O(n) (нет - но об этом далее). И если добавление в конец требует O(n), а также сам reverse() требует n добавлений,
то всё вместе, это уже квадратичный алгоритм. Однако, многие compile-time алгоритмы могут получать выгоду от того, что некоторые их части 
используют операции раскрытия пакетов (pack expansions). И в случае reverse() добавление элемента в хвост может быть написано как операция,
использующая такое раскрытие пакета, асимптотическая сложность чего уже не линейная, а константная. Таким образом, вся операция разворота списка
должна иметь линейную асимптотическую сложность, которую превзойти уже невозможно.

Делая этот исторический экскурс, я должен был убедиться, что первый попавшийся вариант метафункции reverse() имеет линейную сложность. И если
взять, скажем, реализацию этого алгоритма из библиотеки Boost.mp11, то это именно так. Не догадавшись проверить это сразу, я написал
гарантированно линейную реализацию, опирающуюся не на классический алгоритм с вставкой в конец, а на первую пришедшую в голову ремесленную
альтернативу: с использованием двух списков и привлечением кортежей. Но приводить это "решение в лоб" я здесь не буду, хотя и оставлю в
репозитарии файл с названием example_6_O(n).cpp.

Итак, мы резюмируем, что алгоритм разворота списка - всегда линейный, его реализация такова во всех современных библиотеках, и, тем самым, мы
закрываем первую проблему.

Далее. Если взглянуть на все ту же реализацию из Boost.mp11, то автор библиотеки позаботился и об уменьшении количества инстанцирований
промежуточных типов при вычислениях, делая разворот списка "с шагом 10". То есть, в рекурсивном случае осуществляется работа не с одним 
элементом списка, а сразу с десятью. Следовательно, и базовых случаев прописывается в количестве по модулю 10.

Рекурсивный случай:

```cpp
template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class... T> 
struct mp_reverse_impl<L<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T...>> {
    using type = mp_push_back<typename mp_reverse_impl<L<T...>>::type, T10, T9, T8, T7, T6, T5, T4, T3, T2, T1>;
};
```

Базовые случаи:

```cpp
template<template<class...> class L> struct mp_reverse_impl<L<>> {
    using type = L<>;
};

template<template<class...> class L, class T1> struct mp_reverse_impl<L<T1>> {
    using type = L<T1>;
};

template<template<class...> class L, class T1, class T2> struct mp_reverse_impl<L<T1, T2>> {
    using type = L<T2, T1>;
};

//...

template<template<class...> class L, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9> 
struct mp_reverse_impl<L<T1, T2, T3, T4, T5, T6, T7, T8, T9>> {
    using type = L<T9, T8, T7, T6, T5, T4, T3, T2, T1>;
};
```

Понятно, что такое уменьшение количества инстанцирований приводит как к увеличению скорости компиляции, так и к уменьшению глубины рекурсии:
теперь пользователю не придется увеличивать значение директивы компилятору -ftemplate-depth при приближении числа типов в списке к 900 (GCC)!
Это нужно будет сделать только когда это число будет приближаться к 9000, что естественно не может не радовать, поскольку предоставляет 
возможность надолго забыть о проблемах и сосредоточиться на разработке.

Таким образом, старый квадрант метапрограммирования на классах способен решить 3 проблемы из 4-х. Но избавить нас от рекурсии и забыть о 
необходимости регулировки ее глубины могут лишь constexpr-функции. То есть наша задача - написать такой constexpr-код, который бы давал
возможность работать с огромными количествами типов в списках типов не применяя рекурсию. Отсутствие же рекурсии само по себе гарантирует
увеличение скорости компиляции не только из-за известных накладных расходов на рекурсию, но и потому, что новые типы создаются лишь по
необходимости и не являются обязательной частью каждого шага (каждой итерации) цикла, как ранее.

Наш вариант решения опирается на те знания, которые мы почерпнули из п.3.3, когда операции на типах вначале производились на их взаимно
однозначных идентификаторах, и лишь в конце обретались итоговые типы. Сейчас нам нужно написать императивную итеративную версию разворота списка,
но сделать это так, чтобы она работала внутри constexpr-функций. И вот как выглядит первая попытка (не забудем компилировать данный код и любой
следующий код уже в стандарте C++20 из-за использования constexpr std::swap()):

```cpp
template <class...Ts, size_t... I>
constexpr auto make_ids(List<Ts...> lst, index_sequence<I...> is) {
    size_t type_ids[sizeof...(I)] = {type_to_id(type_identity<Ts>{})...};
    for (auto b = type_ids, e = &type_ids[sizeof...(I)] - 1; b < e; )
        swap(*b++, *e--); 

    return make_tuple(type_ids[I]...);
}

template <class L, size_t... I>
constexpr auto reverse_impl(L lst, index_sequence<I...> is) {
    constexpr auto ids = make_ids(lst, is);
    return List<decltype(id_to_type(integral_constant<size_t, get<I>(ids)>{}))... >{};
}

template <class... Ts, template <class...> class L>
constexpr auto reverse(L<Ts...> tp) {
    if constexpr(!sizeof...(Ts))
        return L<>{};
    else
        return reverse_impl(tp, make_index_sequence<sizeof...(Ts)>{});
}

```
В верхней функции массив type_ids сначала заполняется идентификаторами всех типов из списка типов, затем происходит разворот порядка этого
набора на противоположный и, наконец, построение кортежа этих, уже развернутых, идентификаторов с целью возврата клиенту. Вторая сверху функция
принимает упомянутый кортеж идентификаторов в constexpr-переменную и производит преобразование идентификаторов в типы, строя новый список типов,
который теперь будет развернут относительно исходного. Нижняя функция это обертка над средней, учитывающая случай пустого списка типов.

Проверяем: все работает, казалось бы результат достигнут, ибо никакой рекурсии здесь нет и быть не может! Пробуем составить список из 25 типов
и свести директиву компилятору -ftemplate-depth до 20, чтобы убедиться, что эта настройка перестала играть роль. Но вместо радости, получаем
ошибку компиляции с требованием ... увеличить глубину рекурсии. Что же произошло? Наш алгоритм не избавился от рекурсии, просто она стала
невидимой: она содержится во внутреннем устройстве класса std::tuple. Создавая объект-кортеж, мы перерасходовали этот выделенный маленький стек.

Одно решение этой проблемы заключается в написании или заимствовании того варианта кортежа, который в своей внутренней структуре лишен 
рекурсивности. Мы берем класс sequence_tuple::tuple из всё того же проекта Boost.PFR(magic_get), пишем для него функцию-фабрику и заменяем нашу
реализацию:

```cpp
template <class ... Args>
constexpr auto make_sequence_tuple(Args&&... args) {
    return sequence_tuple::tuple<decay_t<Args>...>(forward<Args>(args)...);
}

template <class...Ts, size_t... I>
constexpr auto make_ids(List<Ts...> lst, index_sequence<I...> is) {
    ...
    return make_sequence_tuple(types[I]...);
}
```
На этот раз все будет работать, и настройка -ftemplate-depth действительно перестала играть какую-либо роль для наших задач (за исключением,
конечно же, некоторого минимального ee значения, необходимого для работы окружения).

На этом можно было бы и закончить, поскольку мы честно решили проблему 4. Однако, мы воспользовались сторонним классом, а также не сравнивали
скорости компиляции нескольких альтернатив. И, третье - мы можем работать только с теми типами, которые зарегистрированы в нашей системе при
помощи все того же макро BOOST_MAGIC_GET_REGISTER_TYPE. Создадим одну альтернативу нашему решению - избавимся вообще от кортежей, то есть от 
sequence_tuple::tuple:

```cpp
template <size_t N>
struct size_array {
    size_t data[N];
    constexpr size_array(initializer_list<size_t> il) {
        size_t index= 0;
        for (auto e : il)
            data[index++] = e;
    }
};

template <class...Ts, size_t... I>
constexpr auto make_ids(List<Ts...> lst, index_sequence<I...> is) {
    size_array<sizeof...(I)> arr = {type_to_id(type_identity<Ts>{})...};
    for (auto b = arr.data, e = &arr.data[sizeof...(I)] - 1; b < e; )
        swap(*b++, *e--); 
    return arr;
}
```
Мы заменили кортеж на класс с constexpr-конструктором и членом-данными - массивом. Мы заполняем массив при конструировании, и возвращаем объект
этого класса клиенту. Остальной код не претерпел изменений. Мы по-прежнему должны регистрировать любые новые типы, если намерены работать с
ними, что, конечно, совсем несложно. Время компиляции в этом варианте нашего исполнения заметно лучше, чем в случае с кортежем.

И наконец, последнее что мы могли бы сделать - это реализовать еще более универсальный алгоритм, не требующей регистрации никаких типов. Впрочем,
лично я, вероятно, не смог бы придумать такое решение. Однако, есть прекрасное решение от Олега Фатxиева, представленное на C++ Russia 2019. И
выглядит оно так:

```cpp
template <class T, class U>
constexpr bool operator==(type_identity<T>, type_identity<U>) { return false; }
template <class T>
constexpr bool operator==(type_identity<T>, type_identity<T>) { return true; }

template <class IS>
struct get_impl;

template <size_t... Is>
struct get_impl<std::index_sequence<Is...>> {
    template <class T>
    static constexpr T dummy(decltype(Is, (void*)0)..., T*, ...);
};

template <size_t I, class... Ts>
constexpr auto get(List<Ts...>) {
    return type_identity<decltype
                         (
                             get_impl<make_index_sequence<I> >::dummy(static_cast<Ts*>(nullptr)...)
                         )
    >{};
}

// Проверка get<>():
static_assert(get<2>(List<double, int, char, float>{}) == type_identity<char>{});

template <class TypePack, size_t... I>
constexpr auto reverse_impl(TypePack tp, index_sequence<I...>) {
    return List<typename decltype(get<sizeof...(I) - I - 1>(tp))::type...>{};
}

template <class... Ts>
constexpr auto reverse(List<Ts...> tp) {
    return reverse_impl(tp, make_index_sequence<sizeof...(Ts)>{});
}

// Проверка reverse():
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
```
Сначала пишется шаблонная constexpr-функция get(), отбирающая тип по порядковому номеру. А затем она используется алгоритмом разворота списка.
Реализация более короткая и более сложная. Это решение можно считать окончательным и универсальным, хотя по времени компиляции оно чуть-чуть
уступает нашему решению на массиве.
