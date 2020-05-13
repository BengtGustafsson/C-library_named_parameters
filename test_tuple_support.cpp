

#include "tuple_support.h"

#include <cassert>


// Get a predicate that we can use. For mysterious reasons this is not allowed inside main.
template<typename T> using is_int = std::predicate_bind1st<std::is_same, int>::template tpl<T>;


void test_tuple_support()
{
    std::tuple<int, float> t1{ 1, 2.0f };
    std::tuple<float, int> t2{ 3.0f, 4 };
    std::array<int, 3> a1{ 5, 6, 7 };

    static_assert(std::tuple_contains<is_int, decltype(t2)>());
    static_assert(std::tuple_count_if<is_int, decltype(a1)>() == 3);
    static_assert(std::tuple_find<is_int, decltype(t1)>() == 0);
    static_assert(std::tuple_find<is_int, decltype(t2)>() == 1);
    static_assert(std::tuple_find_nth<is_int, 0, decltype(t2)>() == 1);
    static_assert(std::tuple_find_nth<is_int, 1, decltype(t2)>() == std::npos);
    static_assert(std::tuple_find_nth<is_int, 2, decltype(a1)>() == 2);

    assert(std::get_or<is_int>(1, t2) == 4);

    // Concatenate two tuples with some strings thrown in.
    auto t3 = std::tuple_concat<std::tuple>("a", t1, "b", a1, "c");
    static_assert(std::tuple_size_v<decltype(t3)> == 8);
    assert(std::get<1>(t3) == 1 && std::get<5>(t3) == 6);  // check the ints should be enough.
    
    // Concat with an empty first parameter
    auto tx = std::tuple_concat(std::tuple<>(), 3);

    // Test the arranger functions using t3.
    auto t4 = std::tuple_slice<1, 5>(t3);
    static_assert(std::tuple_size_v<decltype(t4)> == 4);
    assert(std::get<0>(t4) == 1 && std::get<3>(t4) == 5);  // check the ints should be enough.

    auto t5 = std::tuple_slice<1, std::npos>(t3); // Test npos as upper limit.
    static_assert(std::tuple_size_v<decltype(t5)> == 7);
    assert(std::get<0>(t5) == 1 && std::get<3>(t5) == 5);  // check the ints should be enough.

    auto t6 = std::tuple_filter<is_int>(t3);
    static_assert(std::tuple_size_v<decltype(t6)> == 4);
    assert(std::get<0>(t6) == 1 && std::get<3>(t6) == 7);  // check the ints should be enough.

    auto t7 = tuple_reverse(t3);
    static_assert(std::tuple_size_v<decltype(t7)> == 8);
    assert(std::get<1>(t7) == 7 && std::get<6>(t7) == 1);  // check the ints should be enough.

    // Reverse zero length.
    std::tuple<> x0;
    auto y0 = tuple_reverse(x0);
    static_assert(std::tuple_size_v<decltype(y0)> == 0);

    // Reverse 1 length
    std::tuple<int> x1{ 3 };
    auto y1 = tuple_reverse(x1);
    static_assert(std::tuple_size_v<decltype(y1)> == 1);
    assert(std::get<0>(y1) == 3);

    // Reverse 2 length
    std::tuple x2{ 2, 3 };
    auto y2 = tuple_reverse(x2);
    static_assert(std::tuple_size_v<decltype(y2)> == 2);
    assert(std::get<0>(y2) == 3 && std::get<1>(y2) == 2);

    // Reverse 3 length
    std::tuple x3{ 1, 2, 3 };
    auto y3 = tuple_reverse(x3);
    static_assert(std::tuple_size_v<decltype(y3)> == 3);
    assert(std::get<0>(y3) == 3 && std::get<1>(y3) == 2 && std::get<2>(y3) == 1);

    // Insert a scalar
    auto t8 = std::tuple_insert<1>(t2, 8);
    static_assert(std::tuple_size_v<decltype(t8)> == 3);
    assert(std::get<0>(t8) == 3.0f && std::get<1>(t8) == 8 && std::get<2>(t8) == 4);

    // Insert an array (which is flattened). Also insert last.
    auto t9 = std::tuple_insert<2, std::tuple>(t1, a1);
    static_assert(std::tuple_size_v<decltype(t9)> == 5);
    assert(std::get<0>(t9) == 1 && std::get<1>(t9) == 2.0f && std::get<2>(t9) == 5);

    // Erase one element
    auto t10 = std::tuple_erase<1>(a1);
    static_assert(std::is_same_v<decltype(t10), std::array<int, 2>>);
    assert(std::get<0>(t10) == 5 && std::get<1>(t10) == 7);
    
    // Erase a few elements
    auto t11 = std::tuple_erase<2, 4>(t3);
    static_assert(std::tuple_size_v<decltype(t11)> == 6);
    assert(std::get<1>(t11) == 1 && std::get<2>(t11) == 5 && std::get<4>(t11) == 7);
}


extern void test_named();


int main()
{
    test_named();
    test_tuple_support();
    return 0;
}
