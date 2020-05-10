

#include "named.h"

#include<iostream>

inline const char* first_tag = "first";
std::value_name<first_tag> first;
inline const char* second_tag = "second";
std::value_name<second_tag> second;
inline const char* third_tag = "third";
std::value_name<third_tag> third;
inline const char* fourth_tag = "fourth";
std::value_name<fourth_tag> fourth;

template<size_t IX, typename... Ps> auto test_function(Ps&&... ps)
{
    auto t = std::make_ref_tuple(std::forward<Ps>(ps)...);
    constexpr size_t N = std::tuple_find_nth<std::is_named, IX, decltype(t)>();
    if constexpr (N != std::npos) {
        std::cout << std::decay_t < std::tuple_element_t<N, decltype(t)>>::name << std::endl;
        test_function<N + 1>(std::forward<Ps>(ps)...);
    }
}

void test_named()
{
    int xval = 3;
    auto x = first = xval;
    auto y = std::move(x);
    
    std::tuple{ x, y };

    test_function<0>(first=1, second("hej"), third(2, 3), fourth);
}

