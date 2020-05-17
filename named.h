


#include "tuple_support.h"

#include <memory>
#include <functional>

// For the corresponding functionality of named_value
#include <optional>
#include <variant>
#include <any>

// This library is similar in purpose to boost Parameter (https://www.boost.org/doc/libs/1_72_0/libs/parameter/doc/html/index.html).

namespace std {

// Missing trait which just converts arrays and array references to pointers without touching other types.
template<typename T> struct array_to_pointer {
    using type = T;
};
template<typename T> struct array_to_pointer<T(&)[]> {
    using type = T*;
};
template<typename T, size_t N> struct array_to_pointer<T(&)[N]> {
    using type = T*;
};

template<typename T> using array_to_pointer_t = typename array_to_pointer<T>::type;


//////////////// named_values and value_name are building blocks for named parameter handling ////////////////

// value_name_tag should be a fixed_string of fixed max length. To make this work with current compilers it is currently replaced by
// a const char* which means that it is the identity (variable name) rather than the characters in the string that we are templating
// on. This means that it is not allowed to violate the ODR rule and declare the same value_name in different headers.
using value_name_tag = const char*&;

// Forward declare the value_name class template.
template<value_name_tag Name> class value_name;


// named_value connects a value with a name and is returned from the various operators of value_name and can be retrieved from
// tuples using special get overloads.
template<value_name_tag Name, typename T> class named_value {
public:
    using type = T;
    static constexpr value_name_tag name = Name;

    // The ctor from non-const reference is needed to avoid the template ctor from taking over. And when this is defined
    // the other copy constructors must also be manually defined.
    named_value(const named_value& src) : value(src.value) {}
    named_value(named_value& src) : value(src.value) {}
    named_value(named_value&&) = default;

    // Constructor from a value_name of same name, only available if T is bool
    named_value(value_name<Name>&) {
        static_assert(is_same_v<T, bool>, "only bool named_values can be constructed from a value_name object");
        value = true;  // Initialize here to make sure the static assert is the first compile error.
    }

    // Emplacing constructor.
    template<typename... Ps> named_value(Ps&&... pars) : value{ std::forward<Ps>(pars)... } {}

    T value;
};

// Deduction guide that selects bool as the type when a value_name is the parameter.
template<value_name_tag Name> named_value(value_name<Name>&&)->named_value<Name, bool>;

// make_named passes references through if possible, i.e. if sizeof...Ps == 1 and Ps[0] is a subclass of T, else it
// Returns a named_value with a by value T.
template<value_name_tag Name, typename T, typename... Ps> auto make_named(Ps&&... ps)
{
    using TC = decay_t<T>;
    if constexpr (sizeof...(Ps) == 1) {
        if constexpr (is_base_of_v<TC, decay_t<Ps>...>)
            return named_value<Name, decltype(forward<Ps>(ps))...>(forward<Ps>(ps)...);
        else
            return named_value<Name, TC>(forward<Ps>(ps)...);
    }
    else
        return named_value<Name, TC>(forward<Ps>(ps)...);
}

// make_named_from_tuple passes references through if possible, i.e. if P is a subclass of T, else it
// Returns a named_value with a by value T, constructed from P or if P is a tuple like its elements.
template<value_name_tag Name, typename T, typename P> auto make_named_from_tuple(P&& p)
{
    using TC = decay_t<T>;
    if constexpr (is_tuple_like_v<decay_t<P>>)
        return make_from_tuple<named_value<Name, T>>(forward<P>(p));
    else
        return make_named<Name, TC>(forward<P>(p));
}


// Marker type to indicate that default_for was called.
template<value_name_tag Name, typename T> struct default_for_value : public named_value<Name, T> {
};

// A value_name object is what you use as the parameter name at a call site. It is also used to specify which named parameters a function accepts, and their types.
// Its operator= and operator() allow specifying named parameter with name = value or name(value) syntax.
template<value_name_tag Name> class value_name {
public:
    static constexpr value_name_tag name = Name;

    // no copying allowed.
    value_name() = default;
    value_name(const value_name&) = delete;
    value_name& operator=(const value_name&) = delete;

    // These operators are the main feature of a value_name, allowing it to act as a named parameter. Note that the value is only
    // stored as a reference even if it comes in as a rvalue, which means that the named_value is only valid until the function
    // being called returns.
    template<typename T> auto operator=(T&& value) const { return named_value<Name, array_to_pointer_t<decltype(std::forward<T>(value))>>(std::forward<T>(value)); }
    template<typename T> auto operator()(T&& value) const { return named_value<Name, array_to_pointer_t<decltype(std::forward<T>(value))>>(std::forward<T>(value)); }
    template<typename T, typename... Ts> auto operator()(T&& value, Ts&&... values) const {  // One regular parameter to exclude the empty parenthesis case.
        using RET = tuple<decltype(forward<T>(value)), array_to_pointer_t<decltype(std::forward<Ts>(value))>...>;
        return named_value<Name, RET>(RET{ forward<T>(value), forward<Ts>(values)... });
    }

    // As value_name:s are found by the predicates and the get overload which takes a value_name does a ::value on the found
    // element having a bool value here ensures that just mentioning a value_name works the same as value=true.
    static constexpr bool value = true;

    // Bool false values can be generated by just naming !value_name.
    named_value<Name, bool> operator!() const { return named_value<Name, bool>(false); }

    // The following methods are used in bind_parameters to indicate different requirements on named arguments matched.

    // Optional values are useful for _parameters_ to allow the function to see whether the named argument was present or not.
    template<typename T> named_value<Name, std::optional<T>> optional() const { return named_value<Name, optional<T>>(nullopt); }

    // Variant values are used when you want to accept different types for a value_name, for instance a size as a Size object or
    // two ints. The version without default parameter automatically adds a std::monostate as the first option which is maybe
    // more convenient than wrapping the std::variant in a std::optional.
    template<typename... Ts, typename T> named_value<Name, std::variant<Ts...>> variant(T&& defval) const { return named_value<Name, std::variant<Ts...>>(std::forward<T>(defval)); }
    template<typename... Ts> named_value<Name, std::variant<monostate, Ts...>> variant() const { return named_value<Name, std::variant<monostate, Ts...>>(); }

    // Any values are allowed to hold any data type in its std::any. If no default value is given it defaults to the empty state.
    template<typename T> named_value<Name, std::any> any(T&& value) const { return named_value<Name, std::any>(std::forward<T>(value)); }
    named_value<Name, std::any> any() const { return named_value<Name, std::any>(std::any()); }

    // default_for values are used as parameters to indicate that if a actual of type T is present (after any defaulted unnamed
    // parameters have been consumed) a named_value for this name should be produced. Note that is_construtcible is used but no
    // complete overload resolution so don't use too many of these in one function.
    template<typename T> default_for_value<Name, T> default_for(T&& defval) { return default_for_value<Name, T>(std::forward<T>(defval)); }
    template<typename T> default_for_value<Name, std::optional<T>> default_for() { return default_for_value<Name, std::optional<T>>(std::optional<T>()); }
};


//////////////// Specialized tuple support functions for named values ////////////////

// Predicate to check if a type is a named_value instance. struct and variable versions both public as we still need the struct
// version when used as PRED in tuple support functions.
template<typename T> struct is_named : public false_type {};
template<value_name_tag Name, typename D> struct is_named<named_value<Name, D>> : public true_type {};
template<typename T> constexpr bool is_named_v = is_named<remove_reference_t<T>>::value;

namespace detail {
    // Helper to adjust value_name<Name> to named_value<Name, bool> used to implement the "name is bool true" feature.
    template<typename T> struct named_type_for {
        using type = T;
    };

    template<value_name_tag Name> struct named_type_for<value_name<Name>> {
        using type = named_value<Name, bool>;
    };

    template<typename T> using named_type_for_t = typename named_type_for<decay_t<T>>::type;


// 
//     // predicate to test whether T can be constructed from P, or if P is a tuple from the elements of P. This could be extended to
//     // any tuple_like but that is quite a lot more tricky, and at this point we only need it for tuple.
//     template<typename T, typename P> struct is_constructible_from_elements {
//         static constexpr bool value = is_constructible_v<T, P>;
//     };
//     template<typename T, typename... Ps> struct is_constructible_from_elements<T, tuple<Ps...>> {
//         static constexpr bool value = is_constructible_v<T, Ps...>;
//     };
// 
//     template<value_name_tag Name, typename U, typename T> constexpr bool has_same_name_and_can_construct_helper()
//     {
//         using NT = named_type_for_t<T>;
//         if constexpr (is_named_v<NT>) {
//             if constexpr (NT::name == Name && is_constructible_from_elements<U, typename NT::type>::value)
//                 return true;
//         }
// 
//         return false;
//     }
// 

    // constexpr function to see if T is constructible from Ps elements. Must use the structured binding protocol as P may not be
    // of the form T<X, Y, Z> but could be std::array or something else.
    template<size_t IX, typename T, typename P, typename... Types> constexpr bool check_tuple_construction()
    {
        if constexpr (!is_tuple_like_v<P>)
            return is_constructible_v<T, P>;
        else if constexpr (IX == tuple_size_v<P>)
            return is_constructible_v<T, Types...>;  // Error here for the empty tuple.
        else
            return check_tuple_construction<IX + 1, T, P, Types..., tuple_element<IX, P>>();
    }

    // Nested template to be able to bind T first and then provide P later.
    template<typename T, typename P> struct is_constructible_from_elements {
        constexpr static bool value = check_tuple_construction<0, T, P>();
    };
        
            
    // Helper to actually construct a T from P, whether it is a tuple or not.
    template<typename T, typename P> decltype(auto) construct_from_elements(P&& par) {
        if constexpr (is_same_v<remove_reference<T>, remove_reference<P>>)
            return forward<P>(par); // Return the incoming reference if the decayed type exactly matches.
        else if constexpr (is_tuple_like_v<P>)
            return make_from_tuple<decay_t<T>>(std::forward<P>(par));       // Return an object constructed from the par tuple
        else
            return decay_t<T>(std::forward<P>(par));                        // Return an object constructed by conversion.
    }
 }

// value_name:s return true as they are converted to named_value<Name, bool> later.
template<value_name_tag Name, typename T> struct has_same_name : false_type {};
template<value_name_tag Name, value_name_tag Name2, typename D> struct has_same_name<Name, named_value<Name2, D>> : std::integral_constant<bool, &Name == &Name2> {};
template<value_name_tag Name, value_name_tag Name2> struct has_same_name<Name, value_name<Name2>> : std::integral_constant<bool, &Name == &Name2> {};

// Predicate that can be used with the tuple support functions to find a matching named_value or value_name.
template<value_name_tag N> struct is_named_as {
    template<typename T> using tpl = has_same_name<N, decay_t<T>>;
};


// 
// // Predicate for finding a certain name. Useful to see if an element of a certain name is in a pack or tuple, and if it can be used
// // to construct a U, where if the T::type is a tuple this would first be exploded.
// template<value_name_tag Name, typename U, typename T> struct has_same_name_and_can_construct {
//     static constexpr bool value = detail::has_same_name_and_can_construct_helper<Name, U, decay_t<T>>();
// };
// 
// 
// // Predicate to test name and constructibility from Ps...
// template<value_name_tag Name, typename U> struct is_called_and_can_construct {
//     template<typename T> using tpl = has_same_name_and_can_construct<Name, U, T>;
// };
// 

// Overloads of the new tuple based functions with direct Name (and U) template parameter/s for convenience: 
template<value_name_tag Name, typename TL> constexpr size_t tuple_count_if() { return tuple_count_if<is_named_as<Name>::template tpl, TL>(); }
template<value_name_tag Name, typename TL> constexpr bool tuple_contains() { return tuple_contains<is_named_as<Name>::template tpl, TL>(); }

template<value_name_tag Name, typename TL> constexpr size_t tuple_find() { return tuple_find<is_named_as<Name>::template tpl, TL>(); }
template<value_name_tag Name, size_t POS, typename TL> constexpr size_t tuple_find() { return tuple_find<is_named_as<Name>::template tpl, POS, TL>(); }

template<value_name_tag Name, size_t N, typename TL> constexpr size_t tuple_find_nth() { return tuple_find_nth<is_named_as<Name>::template tpl, N, TL>(); }

// Note: This assumes a named_value or value_name of the correct name (as specified by the incoming ValueName) can be found.
template<auto&& ValueName, typename TL> decltype(auto) get(TL&& t)
{
    constexpr size_t IX = tuple_find<decay_t<decltype(ValueName)>::name, TL>();
    static_assert(IX != npos, "Name not found");
    return forward<typename decay_t<tuple_element_t<IX, decay_t<TL>>>::type>(get<IX>(forward<TL>(t)).value);
}


// Note: This always returns a D even if an existing value requires conversion. Emplacement construction occurs if the value found
// is a tuple-like.
template<value_name_tag Name, typename D, typename TL> decltype(auto) get_or(D&& defval, TL&& t)
{
    constexpr size_t IX = tuple_find<Name, TL>();
    if constexpr (IX == npos)
        return forward<D>(defval);
    else
        return detail::construct_from_elements<D>(get<IX>(forward<TL>(t)).value);
}


// Further overload of get_or which takes a named_value as the default value, thus not having to specify the name tag explictily:.
// Instead you can use get_or(my_name(defVal), t) to extract the value of the my_name value from t if any, or otherwise return
// defVal (with its value category preserved regardless of the fact that the named_value itself is typically a rvalue.
// %% Maybe only the rvalue overload should exist not to encourage creating named_value objects containing constants and _then_
// passing them to get_or, as those constants would then be expired.
template<value_name_tag Name, typename D, typename TL> decltype(auto) get_or(const named_value<Name, D>& defval, TL&& t)
{
    return get_or<Name, D>(std::forward<D>(defval.value), std::forward<TL>(t));
}
template<value_name_tag Name, typename D, typename TL> decltype(auto) get_or(named_value<Name, D>&& defval, TL&& t)
{
    return get_or<Name, D>(forward<D>(defval.value), forward<TL>(t));
}



namespace detail {
// Sentinel used when all the Ps are handled, just check that args is empty and then return res
template<typename A, typename R> auto bind_named_parameters(A&& args, R res) {
        static_assert(tuple_size_v<decay_t<A>> == 0, "Some named arguments were not accepted. See signature of the failing instance to see which");
        return res;     // This is always a return by value.
}

// Match any element in the tuple-like args by name to p and append the result (hit or miss) to res, while removing the
// matching element from args, if found.
template<typename A, typename R, typename P, typename... Ps> auto bind_named_parameters(A&& args, R&& res, P&& p, Ps&&... ps) {
    using PC = decay_t<P>;
    constexpr size_t IX = tuple_find<PC::name, A>();
    if constexpr (IX != npos) {  // found a value of the correct name. Try to construct the correct P::type from it, preserving the named_value wrapper.
        auto v = make_named_from_tuple<PC::name, decay_t<typename PC::type>>(forward<typename decay_t<tuple_element_t<IX, decay_t<A>>>::type>(get<IX>(args).value));
        return bind_named_parameters(tuple_erase<IX>(forward<A>(args)), tuple_concat(res, move(v)), forward<Ps>(ps)...);
    }
    else {
        // If P is a default_for_value we also check for a constructible from condition from any of the unnamed values in args.
        // Note that this is a special constructible from which takes into account tuples in args where the type in P is
        // constructible from the tuple elements.
        //using T = decay_t<typename PC::type>;
        //size_t DIX = tuple_find<predicate_and<predicate_not<is_named>::template tpl, predicate_bind1st<detail::is_constructible_from_elements, T>::template tpl>::template tpl, A>();
        //if (DIX != npos) {
        //    auto v = make_named<PC::name, T>(forward<tuple_element_t<IX, A>>(get<IX>(args)));
        //    return bind_named_parameters(forward<A>(args), tuple_concat(res, move(v)), forward<Ps>(ps)...);
        //}
        //else  // Not found, append default value to res and continue.
            return bind_named_parameters(forward<A>(args), tuple_concat(res, forward<P>(p)), forward<Ps>(ps)...);
    }
}
}

// bind_parameters takes actuals as a tuple as the first parameter and then a list of allowed parameters as the rest of the
// parameters. It matches these together and returns a tuple ordered according to the parameters, with any matching values replaced.
template<typename A, typename... Ps> auto bind_parameters(A&& args, Ps&&... ps)
{
        return detail::bind_named_parameters(forward<A>(args), tuple<>(), forward<Ps>(ps)...);
}


}   // namespace std
