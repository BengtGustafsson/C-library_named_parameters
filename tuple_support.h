
// These includes are needed as these headers presently don't include their associated tuple_traits specializations.
#include <type_traits>
#include <tuple>
#include <array>

// Convenience functions for tuple-likes

namespace std {



// make_ref_tuple ensures that no unnecessary copying takes place. Beware of dangling references if using it.
template<template<typename...> class RESULT = tuple, typename... Ts> auto make_ref_tuple(Ts&&... ts) 
{
    return RESULT<decltype(forward<Ts>(ts))...>(forward<Ts>(ts)...);
}

// namespace level npos needed as we don't have any class to put it in. The standard should guarantee that all other npos:es are
// equal to this one.
constexpr size_t npos = static_cast<size_t>(-1);


//////////////// New trait to be able to instantiate the same template for another set of template parameters ////////////////


// trait with information about a tuple-like. Currently the information is a specialized indicator and
// if it is true a variadic type alias template factory which instantiates the template of the specialization.

template<typename T> struct tuple_traits {
    constexpr static bool specialized = false;
};

struct tuple_factory {
    template<typename... Ts> using type = tuple<Ts...>;
};

template<typename... Ts> struct tuple_traits<tuple<Ts...>> {
    constexpr static bool specialized = true;
    using factory = tuple_factory;
};

struct array_factory {
    template<typename... Ts> using type = array<std::common_type_t<Ts...>, sizeof...(Ts)>;          // Ignoring the empty array for now.
};

template<typename E, size_t N> struct tuple_traits<array<E, N>> {
    constexpr static bool specialized = true;
    using factory = array_factory;
};


// Check if a type is a tuple like in the sense that it has a factory type available, i.e. a specialization of tuple_traits.
template<typename T> constexpr bool is_tuple_like_v = tuple_traits<T>::specialized;


// Function to get the common tuple_category for any number of types, if it exists.
namespace detail {
    template<typename T, typename... Ts> auto common_tuple_traits_helper()
    {
        if constexpr (sizeof...(Ts) == 0)
            return declval<tuple_traits<T>>();
        else if constexpr (is_tuple_like_v<T>) {
            using rest_type = decltype(common_tuple_traits_helper<Ts...>());
            if constexpr (!rest_type::specialized)
                return declval<tuple_traits<T>>();
            else {
                static_assert(is_same_v<typename tuple_traits<T>::factory, typename rest_type::factory>, "Different tuple likes given to common_tuple_traits");
                return declval<tuple_traits<T>>();
            }
        }
        else
            return common_tuple_traits_helper<Ts...>();
    }
    template<typename... Ts> auto common_tuple_traits()
    {
        using ret = decltype(common_tuple_traits_helper<Ts...>());
        static_assert(ret::specialized, "No tuple-like to derive return type template from");
        return declval<ret>();
    }
}

// What is the common factory type for any number of tuple-likes. Error if they don't agree. Non-tuple-likes in their midst
// are ignored (unless there are no tuple-likes).
template<typename... TLs> using common_tuple_traits = decltype(detail::common_tuple_traits<TLs...>());

// Possible helper to directly create an instance of the tuple-like that T is an onstance of for ELs.
template<typename T, typename... ELs> using tuple_like_as = typename tuple_traits<T>::factory::template type<ELs...>;


//////////////// Type predicate composition ////////////////

#if 0

// This alternate formulation is also a possibility. Maybe something corresponding to std::bind is easier to accomplish with this.<q

// type predicates for the tuple support functions are classes with an embedded
// template variable of one typename parameter. This tempalte variable is to be true if the predicate is fulfilled.
// In the next version there will be a concept for this, something like this:

// This concept compiles under MSVC 16.5 and gcc trunk.
template<typename T, typename U = int> concept tuple_predicate = requires {
    // This  is to say that a T which is a tuple_predicate must have a template variable which when given an arbitrary typename U
    // results in a constexpr bool value.
    { T::template value<U> } -> std::same_as<const bool&>;
};

// Unfortunately this type of conceptualizable predicate formulation is incompatible to type_traits type of simple predicates which
// are themselves templates on the type under test and contain a plain value variable.

// An adaptor is easily defined but tedious to use all the time.
template<template<typename T> class PRED> struct predicate_adaptor {
    template<typename T> using tpl = PRED<T>;
};

#endif

/* **** With overloadable class templates these could share the names conjunction, disjunction and negation. Or why not unify
   all three levels to logical_and, logical_or, logical_not (or whatever name set feels best). **** */

template<template<typename> class SRC> struct predicate_not {
    template<typename T> struct tpl {
        static constexpr bool value = !SRC<T>::value;
    };
};

template<template<typename> class... SRCs> struct predicate_and {
    template<typename T> struct tpl {
        static constexpr bool value = (SRCs<T>::value && ...);
    };
};

template<template<typename> class... SRCs> struct predicate_or {
    template<typename T> struct tpl {
        static constexpr bool value = (SRCs<T>::value || ...);
    };
};

template<template<typename LHS, typename RHS> class PRED, typename L> struct predicate_bind1st {
    template<typename T> struct tpl {
        constexpr static bool value = PRED<L, T>::value;
    };
};

template<template<typename LHS, typename RHS> class PRED, typename R> struct predicate_bind2nd {
    template<typename T> struct tpl {
        constexpr static bool value = PRED<T, R>::value;
    };
};


//////////////// New tuple functions involving a type predicate ////////////////

// Count how many element types E satisfy PRED<E>::value at or after POS
template<template<typename> class PRED, size_t POS, typename TL> constexpr size_t tuple_count_if()
{
    if constexpr (POS == tuple_size_v<TL>)
        return 0;
    else if constexpr (PRED<decay_t<tuple_element_t<POS, TL>>>::value)
        return tuple_count_if<PRED, POS + 1, TL>() + 1;
    else
        return tuple_count_if<PRED, POS + 1, TL>();
}
template<template<typename> class PRED, typename TL> constexpr size_t tuple_count_if() { return tuple_count_if<PRED, 0, TL>(); }


// Check if any element types E satisfies PRED<E>::value at or after POS
template<template<typename> class PRED, size_t POS, typename TL> constexpr bool tuple_contains() { return tuple_count_if<PRED, TL>() > 0; }
template<template<typename> class PRED, typename TL> constexpr bool tuple_contains() { return tuple_contains<PRED, 0, TL>(); }


// Return the index of the first matching element at or after POS
template<template<typename> class PRED, size_t POS, typename TL> constexpr size_t tuple_find()
{
    if constexpr (POS == tuple_size_v<decay_t<TL>>) {
        return npos;
    }
    else if constexpr (PRED<decay_t<tuple_element_t<POS, decay_t<TL>>>>::value)
        return POS;
    else
        return tuple_find<PRED, POS + 1, TL>();
}
template<template<typename> class PRED, typename TL> constexpr size_t tuple_find() { return tuple_find<PRED, 0, TL>(); }


// Find nth matching element at or after pos.
template<template<typename> class PRED, size_t N, size_t POS, typename TL> constexpr size_t tuple_find_nth()
{
    constexpr size_t NEXT = tuple_find<PRED, POS, TL>();
    if constexpr (NEXT == npos)
        return npos;
    else if (N == 0)
        return NEXT;
    else
        return tuple_find_nth<PRED, N - 1, NEXT + 1, TL>();
}
template<template<typename> class PRED, size_t N, typename TL> constexpr size_t tuple_find_nth() { return tuple_find_nth<PRED, N, 0, TL>(); }


// Tuple get with predicate and default value. There is no corresponding function without default value, instead tuple_find
// must be called separately to make error handling more obvious.
// Note that get_or may return totally different types depending on whether the find was succesful or not.
template<template<typename> class PRED, typename D, typename TL> decltype(auto) get_or(D&& defval, TL&& t)
{
    constexpr size_t IX = tuple_find<PRED, decay_t<TL>>();
    if constexpr (IX == tuple_size_v<decay_t<TL>>)
        return defval;
    else
        return get<IX>(std::forward<TL>(t));
}


// tuple_arrange returns a new tuple rearranged according to an arrange predicate. The predicate is a class template which takes
// the current index and element type and the tuple size as template parameters and has a bool value which is the regular
// include/exclude and next which is a size_t denoting the next index to test, or npos if the processing is finished. With such
// different kinds of filtering, slicing and reordering of tuple elements can be done quite easily. Processing starts at the POS
// given.

// Note that while this function takes any tuple like as input for its TL parameter it will return an instantiation of the variadic
// template type given as the third template parameter.

// The returned object is of the same *tuple_category* as the parameter.

template<template<size_t IX, size_t SIZE, typename E> class PRED, size_t POS = 0, template<typename...> class RESULT, typename TL, typename... Vs>
auto tuple_arrange(TL&& t, Vs&&... vs)
{
    using TLC = decay_t<TL>;
    constexpr size_t SIZE = tuple_size_v<TLC>;

    if constexpr (POS >= SIZE)
        return RESULT<Vs...>{forward<Vs>(vs)...};
    else {
        using P = PRED<POS, SIZE, decay_t<tuple_element_t<POS, TLC>>>;

        if constexpr (P::next >= SIZE) { // Note: This includes after decrement to -1 thanks to size_t being unsigned.
            if constexpr (P::value)
                return RESULT<Vs..., tuple_element_t<POS, TLC>>{forward<Vs>(vs)..., get<POS>(t)};
            else
                return RESULT<Vs...>{forward<Vs>(vs)...};
        }
        else {
            if constexpr (P::value)
                return tuple_arrange<PRED, P::next, RESULT>(forward<TL>(t), forward<Vs>(vs)..., get<POS>(t));
            else
                return tuple_arrange<PRED, P::next, RESULT>(forward<TL>(t), forward<Vs>(vs)...);
        }
    }
}
// Unfortunately the defaulted RESULT version must be implemented as a forwareding function as the type of the tuple is needed and
// we don't want to explicitly have to name the type of our tuple when calling _with_ a RESULT template.
template<template<size_t IX, size_t SIZE, typename E> class PRED, size_t POS = 0, typename TL>
auto tuple_arrange(TL&& t)
{
    return tuple_arrange<PRED, POS, tuple_traits<TL>::template factory>(forward<TL>(t));
}
// To avoid having to give POS as 0 just to be able to give a RESULT we need another overload
template<template<size_t IX, size_t SIZE, typename E> class PRED, template<typename...> class RESULT, typename TL>
    auto tuple_arrange(TL&& t)
{
    return tuple_arrange<PRED, 0, RESULT>(forward<TL>(t));
}


// Predefined arrange predicates to slice a tuple between two indices and filter out the elements fulfilling a regular type
// predicate, and finally to reverse order of a tuple. Could maybe be in detail?

// Select a range
template<size_t FROM, size_t TO = npos> struct slice_arranger {
    template<size_t IX, size_t SIZE, typename E> struct tpl {
        constexpr static bool value = IX >= FROM && IX < TO;
        constexpr static size_t next = IX + 1;
    };
};

// Filter by type
template<template<typename T> class PRED> struct filter_arranger {
    template<size_t IX, size_t SIZE, typename E> struct tpl {
        constexpr static bool value = PRED<E>::value;
        constexpr static size_t next = IX + 1;
    };
};

// Reverse
template<size_t IX, size_t SIZE, typename E> struct reverse_arranger {
    constexpr static bool value = SIZE != 0;   // Must not try to copy first element if reversing a empty tuple.
    constexpr static size_t next = IX - 1;
};


// Return a tuple like consisting of a subset of the incoming tuple's elements. TO can be out of range, but FROM can't.
template<size_t FROM, size_t TO, template<typename...> class RESULT, typename TL> auto tuple_slice(TL&& t)
{
    return tuple_arrange<slice_arranger<FROM, TO>::template tpl, RESULT>(t);
}
template<size_t FROM, size_t TO, typename TL> auto tuple_slice(TL&& t)
{
    return tuple_slice<FROM, TO, tuple_traits<decay_t<TL>>::factory::template type>(forward<TL>(t));
}

// Return a tuple like filtered by a predicate.
template<template<typename T> class PRED, template<typename...> class RESULT, typename TL> auto tuple_filter(TL&& t)
{
    return tuple_arrange<filter_arranger<PRED>::template tpl, RESULT>(t);
}
template<template<typename T> class PRED, typename TL> auto tuple_filter(TL&& t)
{
    return tuple_filter<PRED, tuple_traits<decay_t<TL>>::factory::template type>(forward<TL>(t));
}

// Return a tuple with elements in reverse order.
template<template<typename...> class RESULT, typename TL> auto tuple_reverse(TL&& t)
{
    return tuple_arrange<reverse_arranger, tuple_size_v<decay_t<TL>> - 1, RESULT>(t);
}
template<typename TL> auto tuple_reverse(TL&& t)
{
    return tuple_reverse<tuple_traits<decay_t<TL>>::factory::template type>(forward<TL>(t));
}


namespace detail {
    // TLVs is a combination of remaining tuples followed by flattened elements accrued so far. The first NTIX elements are incoming tuples (or non-tuples).
    template<template<typename...> class RESULT, size_t NTIX, size_t EIX, typename TL, typename... TLVs> auto tuple_concat_helper(TL&& first, TLVs&&... rest)
    {
        if constexpr (NTIX == 0)
            return RESULT<TL, TLVs...>{forward<TL>(first), forward<TLVs>(rest)...}; // Now even TL is a V
        else if constexpr (is_tuple_like_v<decay_t<TL>>) {
            if constexpr (EIX == tuple_size_v<decay_t<TL>>)        // Currently processed TL ends. Don't forward it anymore. Note that this handles empty tuple likes among TLs
                return tuple_concat_helper<RESULT, NTIX - 1, 0>(forward<TLVs>(rest)...);
            else
                return tuple_concat_helper<RESULT, NTIX, EIX + 1>(forward<TL>(first), forward<TLVs>(rest)..., get<EIX>(forward<TL>(first)));
        }
        else  // Process a non-tuple, i.e. just move it last.
            return tuple_concat_helper<RESULT, NTIX - 1, 0>(forward<TLVs>(rest)..., forward<TL>(first));
    }

    // Overload just to handle the zero argument case. Needed as the main overload requires at least one TL.
    template<template<typename...> class RESULT, size_t NTIX, size_t EIX> auto tuple_concat_helper() {
        static_assert(NTIX == 0);
        return RESULT<>();
    }
}


// tuple_concat concatenates all tuple likes to a long RESULT. If there are non-tuple-likes in tls they are just inserted. This way
// prepend, append and flatten are implemented in the same function. It does hurt error checking though, which may be a reason to
// reinstate the different names.
template<template<typename...> class RESULT, typename... TLs> auto tuple_concat(TLs&&... tls)
{
    return detail::tuple_concat_helper<RESULT, sizeof...(TLs), 0>(forward<TLs>(tls)...);
}
template<typename... TLs> auto tuple_concat(TLs&&... tls)
{
    return tuple_concat<common_tuple_traits<decay_t<TLs>...>::factory::template type>(forward<TLs>(tls)...);
}

// tuple_insert allows inserting an element or a tuple-like into another tuple-like at a certain position. If the inserted thing is
// a tuple it will be flattened, which is most likely the intent. If not you can quote the tuple inside another 1 element tuple when
// calling this function.
template<size_t POS, template<typename...> class RESULT, typename TL, typename EL> auto tuple_insert(TL&& t, EL&& el)
{
    return tuple_concat<RESULT>(tuple_slice<0, POS>(forward<TL>(t)), forward<EL>(el), tuple_slice<POS, npos>(forward<TL>(t)));
}
template<size_t POS, typename TL, typename EL> auto tuple_insert(TL&& t, EL&& el)
{
    return tuple_insert<POS, common_tuple_traits<decay_t<TL>, decay_t<EL>>::factory::template type, TL, EL>(forward<TL>(t), forward<EL>(el));
}

// tuple_erase allows erasing some elements in the middle of a tuple.
template<size_t FROM, size_t TO, template<typename...> class RESULT, typename TL> auto tuple_erase(TL&& t)
{
    return tuple_concat<RESULT>(tuple_slice<0, FROM>(forward<TL>(t)), tuple_slice<TO, npos>(forward<TL>(t)));
}
template<size_t FROM, size_t TO, typename TL> auto tuple_erase(TL&& t)
{
    return tuple_erase<FROM, TO, tuple_traits<decay_t<TL>>::factory::template type>(forward<TL>(t));
}

// Overload to erase one element.
template<size_t IX, template<typename...> class RESULT, typename TL> auto tuple_erase(TL&& t)
{
    return tuple_erase<IX, IX + 1, RESULT>(forward<TL>(t));
}
template<size_t IX, typename TL> auto tuple_erase(TL&& t)
{
    return tuple_erase<IX, tuple_traits<decay_t<TL>>::factory::template type>(forward<TL>(t));
}



}   // namespace std
