

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

struct Point { 
    Point(int x, int y) : x(x), y(y) {}
    int x, y; 
};

template<typename... Ps> auto test_function(Ps&&... ps)
{
    auto t = std::forward_as_tuple(std::forward<Ps>(ps)...);
    auto args = std::bind_parameters(t, second = "Hopp", third = Point{ 0, 0 }, first = 0);
    const char* name = std::get<0>(args).value;
    int value = std::get<first>(args);
    auto p = std::get<third>(args);
}

void test_named()
{
    constexpr bool no = &first_tag == &second_tag;
    int xval = 3;
    auto x = first = xval;
    auto y = std::move(x);
    
    test_function();
    test_function(first = 1);
    test_function(first = 1, third(3, 4));
    test_function(first = 1, second("hej"), third(3, 4));
}

