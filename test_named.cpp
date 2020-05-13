

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
    auto args = std::bind_parameters(t, second = "Hopp", first = 0);
    const char* name = std::get<0>(args).value;
  //  int value = std::get<first>(args);
}

void test_named()
{
    constexpr bool no = &first_tag == &second_tag;
    int xval = 3;
    auto x = first = xval;
    auto y = std::move(x);
    
    std::tuple{ x, y };

    test_function<0>(first=1, second("hej"));
}

