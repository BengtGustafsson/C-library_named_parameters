A pure library named parameter proposal
=======================================

This proposal provides a way to use named function parameters in C++. It is a pure library solution which obviously makes it a bit
less elegant than a language feature has potential to be. The proposal is built in several steps, which may or may not be broken
out to separate proposal in a later stage. These initial parts revolve around predicates and new functions related to packs and
tuple-likes. Using these mechanisms a framework for named parameters is built.

This proposal to some extent relies on the existence of a fixed_string class which can be used as C++20 template parameters. I
assume that such a class will be included in the next C++ version. If not it can be included here, modeled after for instance the
boost::StaticString that came out yesterday.

During development of the reference implementation it was noted that several proposed and unproposed C++ core language features
would help in simplifying both the implementation and usage of the functionality proposed here. Presently the proposal is written
to not take advantage of any features that are not in C++20.


Type Predicates and type predicate composition
==============================================

The first building block of this proposal provides a set of class templates which can be used to compose unary _type predicates_
in the usual and/or/not ways. The goal of these class templates is to be able to compose unary type predicates into more complex
ones. Unary type preficastes are class templates that adhere to the following concept:

    template<template<typename> class PRED> concept type_predicate = requires<typename T> {
        { PRED<T>::value } -> constexpr static bool;
    };

This is the form of template that we recognize from the type testing traits in <type_traits>, such as is_reference, is_base_of
etc.

**NOTE: It is probably not possible to define a concept like this, with a template parameter list after requires. This is to mean
that if the PRED template is instantiated with any T it must contain a constexpr bool value. Maybe it can be specified using
_some_ syntax, I'm not proficient enough to see that though.

**NOTE: It is not possible to use a _template concept_ like this instead of the `template<typename> class PRED` in template
parameter lists. This seems to be an omission as it would be parallel to how type concepts are used for typename template
parameters. **

As for naming we already have two forms of these three operations, with disparate names. To recap:

    // For values.
    logical_and<T>(const T& a, const T& b) { return a && b; }
    logical_or<T>(const T&, const T&);
    logical_not<T>(const T&);

    // For templates with value members.
    template<typename... Ts> struct conjunction { constexpr static bool value = (Ts ... &&); }
    template<typename... Ts> struct discjunction;
    template<typename T> struct negation;

    // New: For type predicates.
    template<templatee<typename> class... PREDs> struct and_predicate {
        template<typename T> struct tpl {
            static constexpr bool value = (SRCs<T>::value && ...);
        };
    };
    template<templatee<typename> class... PREDs> struct or_predicate;
    template<templatee<typename> class... PREDs> struct not_predicate;


**NOTE: With class template overloading conjunction and and_predicate could share a name. As the template parameter kinds differ
there is no risk of confusion. **

**NOTE: Alternatively _universal template parameters_ and specialization can be used to achieve the same effect. **

**NOTE: The nested struct tpl inside and_predicate is necessary just as the `static bool value` was necessary before we got
vatiable templates. A template alias would be a nice complement to the variable templates and type alias templates we have for
values and types. **

Another possibility that would be very valuable on this level would be a facility to bind types to template parameters to form
unary predicates from binary predicates (among other things). With this in place we could easily form a predicate to test for a
certain class or base class by binding the first template parameter of is_same and is_base_of. However, I have not been able to
figure out how such a thing could be implemented in C++20. We can do the equivalent of bind1st and bind2nd _for type predicates_,
which in this case takes us quite far, but as the corresponding value level funcions are removed it is not where we want to be.

    template<template<typename LHS, typename RHS> class TPL, typename L> struct bind1st_predicate {
        template<typename T> struct tpl {
            constexpr static bool value = TPL<L, T>::value;
        };
    }

What we would want is a way to bind template parameters of any kind that occur at any position of the incoming type using the same
idea as for bind, with placeholders. This seems far from possible, especially if all kinds of template parameters are considered.

** NOTE: With template aliases I think we can somehow implement this, and with universal template parameters it can (probably) be
done for any template parameter types. **


Support functions for tuple-likes
=================================

Tuple-likes are class templates which adhere to the _structured binding_ protocol. This means that the templates std::tuple_size,
std::tuple_element and std::get are specialized for them. This does imply that the class template itself has the form
`template<typename...> class X'. For instance std::array adheres to the structured binding protocol, as does, through some
compiler magic, simple enough structs.

Most of these functions are similar to functions found in boost::hana. However, these functions are always concerned with type


** NOTE: It is probably so that such structs don't get the tuple_size, tuple_element and get actually implemented, instead they
are useful in structured bindings via another writing in the standard. It may be worth considering changing this to allow
functionality that builds on the structured binding protocol to work for simple enough structs and C arrays too. **

Tuple like inspection functions
-------------------------------

This proposal adds some more useful templates to work with tuples and type predicates. Note that those that return values are
formulated as consteval functions, rather than template variables or structs. This is as we want to be able to overload them with
variants that have different template parameter lists. 

    consteval size_t npos = size_t(-1);

    template<template<typename> class PRED, typename TL> consteval bool tuple_contains();
    template<template<typename> class PRED, size_t POS, typename TL> consteval bool tuple_contains();
    template<template<typename> class PRED, typename TL> consteval bool tuple_contains(TL&& t);         // t only used to deduce TL
    template<template<typename> class PRED, size_t POS, typename TL> consteval bool tuple_contains(TL&& t);

    // Same overloads for these.
    template<template<typename> class PRED, typename TL> consteval size_t tuple_count_if();
    template<template<typename> class PRED, typename TL> consteval size_t tuple_find();
    template<template<typename> class PRED, typename TL> consteval bool tuple_find_nth();  // index of nth match.

These simple functions allows finding a specific type or type with certain properties. The overloads with an additional POS
parameter provide a way to start searching from a certain element. These functions are included to mimic the behaviour of methods
such as std::string::find which provides the corresponding functionality (Ok, also as this is how it is implemented). More
overloads of these functions are introduced later.

** NOTE: If variables template were overloadable we could wrap most of these functions into template variables to achieve more
consistency with tuple_size_v. **

** NOTE: If class templates were overloadable the functions themselves could be replaced with class templates to complete the
consistency with tuple_size. **

Tuple element access functions
------------------------------

An overloaded get() is provided which takes a type predicate as template parameter instead of the index. It uses tuple_find to
figure out which element to return. If no element is found a static_assert is fired.

A get_or version is available where you provide a default value to return in case the predicate didn't select any element. This is
parallel to optional::value_or but it seems more appropriate to name it similarly to get().

    template<template<typename> class PRED, typename TL> constexpr decltype(auto) get(D&& defval, TL&& t);
    template<template<typename> class PRED, size_t POS, typename TL> constexpr decltype(auto) get(D&& defval, TL&& t);
    template<template<typename> class PRED, typename TL> constexpr decltype(auto) get_or(D&& defval, TL&& t);
    template<template<typename> class PRED, size_t POS, typename TL> constexpr decltype(auto) get_or(D&& defval, TL&& t);


Tuple manipulation functions
----------------------------

This proposal offers two basic functions to manipulate tuple likes returning other tuple likes. In this area there is some design
space to investigate both when it comes to how to select what type to return and how to control the selection process. The first
version of this proposal uses a special type of predicate class template for selection, according to the following template
concept:

    template<size_t IX, size_t SIZE, typename E> concept arranger_predicate = requires<typename T> {
        { PRED<T>::value } -> constexpr static bool;
        { PRED<T>::next } -> constexpr static size_t;
    };

The function tuple_arrange uses this template to select a subset of the elements of the incoming tuple-like in any order to return
a new tuple-like of the RESULT type.

    template<template<size_t IX, size_t SIZE, typename E> class PRED, size_t POS = 0, template<typename...> class RESULT = tuple, typename TL>
    auto tuple_arrange(TL&& t);

This function instantiates PRED<POS, tuple_size_v<TL>, tuple_element_t<POS, TL>> and if its value is true element POS is to be
included in the result. The selection of elements then continues with the element according to the predicate's next value. This
continues until the next element is out of range for the input tuple.

A drawback with this system is that as far as I can understand there is no way to for instance duplicate elements from
the input tuple as the predicate is sort of a pure function with no memory. Trying to duplicate an element will inevitable result
in an endless loop.

An alternative formulation of the arranger_predicate could be for it to contain a type alias for the predicate to use next, apart
from the value and next members.

   // Example with this formulation, that returns the incoming elements three times
   template<size_t LOOP> struct triplicate {
        template<size_t IX, size_t SIZE, typename E> struct tpl {
             constexpr static bool value = true;
             constexpr static size_t next = IX == SIZE - 1 ? (LOOP == 3 ? SIZE : 0) : IX + 1;
             using predicate = triplicate<IX == SIZE - 1 ? LOOP + 1 : LOOP>;  // This implements the loop counter memory.
        };
   };

Note that this also illustrates the need for the SIZE template parameter. An anternative that could be a bit cleaner would be to
provide the TL type instead and let the predicate do its own tuple_element in case it is needed.

A third option would be to use an extra auxiliary type STATE to store state instead of storing it in the template parameter list
of the predicate itself. This may be a bit easier to understand and could be made opt in by overloading tuple_arrange for
different forms of predicate templates, with and without state:

    template<template<size_t IX, typename TL> class PRED, size_t POS = 0, template<typename...> class RESULT = tuple, typename TL>
    auto tuple_arrange(TL&& t);
    template<template<size_t IX, typename TL, typename STATE> class PRED, size_t POS = 0, template<typename...> class RESULT = tuple, typename TL>
    auto tuple_arrange(TL&& t);

The second overload would use a type alias PRED<IX, TL>::next_state to convey state between iterations (which are actually
recursions).

To complement this basic function the proposal conatins three basic arranger_predicates to form a slice, to filter a tuple-like
based on a type predicate and to reverse a tuple's elements. These predicates are used by corresponding functions which all call
tuple_arrange appropriately:

    // arranger_predicates
    template<size_t FROM, size_t TO = npos> struct slice_arranger;
    template<template<typename> class PRED> struct filter_arranger;
    struct reverse_arranger;

    // functions
    template<size_t FROM, size_t TO, template<typename...> class RESULT = tuple, typename TL> auto tuple_slice(TL&& t);
    template<template<typename T> class PRED, template<typename...> class RESULT = tuple, typename TL> auto tuple_filter(TL&& t);
    template<template<typename...> class RESULT = tuple, typename TL> auto tuple_reverse(TL&& t);

The naming of these could be questioned. Don't they read better if the words were reversed. I think so, but I think the prefix
tuple being consistent with all the other functions (except get) outweighs this.

** NOTE: With template aliases these options would essentially be the same, as such an alias can sit in for a class template. **

** NOTE: Even with for constexpr it is not possible to implement tuple_arrange in an imperative way for lack of a way to transport
the state between loop "turns". A mutable using T = x springs to mind. I haven't looked into this but it seems reasonable with
the appropriate restrictions that it can't be assigned to in a way that is not known at compile time. It seems like a logical
complement to for constexpr though. Similarly, if we wanted to collect the indices to return in a compile time vector, can this be
done in a constexpr vector although the function is called at runtime. I don't think so, but maybe it should (with appropriate
restrictions). **

### Flatten

`tuple_flatten` takes any number of arguments and creates a tuple from them, without changing value categories. If any of the arguments
are tuple-like they are inlined in the resulting tuple-like. This does not recurse, only one level of tupleness is removed
(debatable!).

    template<typename... Ts> template<typename...> class RESULT = tuple> auto tuple_flatten(Ts&& ts);

### Concat

The final function in this category completes the functionality and is `tuple_concat(TLs... tls)` which just concatenates the
elements of each tuple in tls to a longer tuple of a settable RESULT type, using the same system as selected for tuple_arrrange.

It may be of interest to be able to wrap single values that are not tuple-like into the sequence of tlses. Or maybe not as it can
easily be done by just writing tuple(x) around each (or each consequtive set).

    template<typename... TLs,  template<typename...> class RESULT = tuple> auto tuple_concat(TLs&& tls);

The only advantage of using concat instead of flatten is from an error reporting perspective, if all arguemnts are not tuples you
get an error message. In the same vein we could provide append and prepend, and possibly insert to insert new elements at each end
or at arbitrary points in a tuple-like.


Providing a better return type default.
--------------------------------------

While tuple is of `template<typename...> class` shape there is nothing in the structured binding protocol that requires the
templtae to be formed like this. Indeed, structured binding works for std::array. It would be neat to let the returned type be an
instance of the same template as the arguments. This would for instance allow concatenation of several std::arrays using
tuple_concat. This could be implemented using a trait. But as we must support different forms of templates the trait must be
specialized using functions. Thus a declval/decltype pair must be employed:

    // This is the type generating function returned from
    struct tuple_category_trait {
        template<typename... Ts> using type = tuple<Ts...>;
    }

    // A function which given any tuple returns a tuple_category_trait. Not implemented.
    template<typename... Ts> tuple_category_trait tuple_category_helper(tuple<Ts...>);

    // Similarly for std::array
    struct array_category_trait {
        template<typename T, typename... Ts> using type = array<T, sizeof...(Ts) + 1>;          // Ignoring the empty array
    };

    template<typename T, size_t N> array_category_trait tuple_category_helper(array<T, N>);
    
    // Type alias for a type with a nested type alias that can be used to generate any corresponding type.
    template<typename T> using tuple_category = decltype(tuple_category_helper(declval<T>());

    // Function to get the common tuple_category for any number of types.
    
    // Sentinel
    template<typename T> auto common_tuple_category_helper(T tup) {
        return tuple_category_helper(tup);
    }

    template<typename T, typename... Ts> auto common_tuple_category_helper(T first, Ts... rest)
    {
        using first_type = tuple_category<T>;
        using rest_type = decltype(common_tuple_category_helper(rest...));
        static_assert(is_same_v<first_type, rest_type>, "Different tuple categories given to common_tuple_category");
        return declval<first_type>();
    }

    template<typename... Ts> using common_tuple_category = decltype(common_tuple_category_helper(declval<Ts>...)); }



A note on value categories
--------------------------

While the tuple deduction guide defaults to a by value tuple these functions don't affect the value category of the elements of
the incoming tuple likes. If the tuple contains references, rvalue references or values, so will the result. Note that if an
argument tuple is a rvalue reference its elements are assumed to be rvalues except if they are references, in which case they
retain their lvalue- or rvalueness from construction time.

It is possible that we want to provide a helper factory function to create a tuple of references from a bunch of references, as it
is hard work doing this correctly yourself. For example:

    template<typename... Ts> auto make_ref_tuple(Ts&&...) { return tuple<decltype(forward<Ts>(ts)...)>(forward<Ts>(ts)...); }


Named values and value names
============================

This section describes the final pieces required for the named parameter feature. They are named_value which connects a name to a
value and value_name which acts as a factory for such named_value objects. When you name an argument at a call site you are
actually calling the operator= of a value_name object, which returns a named_value object to be passed to the called function.


named_value class
-----------------

For the tuple support functions to be useful as a basis for a named function parameter system we need a wrapper that associates a
value with a name. This is the named_value class template:

    template<fixed_string Name, typename T> struct named_value {
        using type = T;
        static constexpr fixed_string name = Name;

        T value;
    };

named_value and has an emplacing constructor forwarding all arguments to T's constructor. For obvious reasons a
T can't be named_value<T> for the same T so there is no risk of ambiguity with the copy or move constructors.

As an alternative to making the value member public it would be possible to make it private and have an operator*() that returns the value,
which could then be immutable after construction. I don't see a particular advantage to this. The alternative to let named_value
inherit T was discarded as it prevents T from being anything else than a user defined type.

** NOTE: A proposal to be able to make template parameter names available directly exists. With an opt-in feature that you havee
to state a visbility for a template parameter to be visible this could be feasible, which would simplify this and many other
templates. There could however be other snags with this that I can't remember at the moment, but I think it was basically that it
could hide names in bases if it was automatically enabled in a new standard version, so opt-in would be ok. **

** NOTE: An operator.() of any form, for instance my proposed implicit cast operator would allow immutable and invisible access to
the underlying data without having to remember a leading * or a trailing .value. **

With named_values in a tuple and a predicate to find a value of a certain name we are starting to build a named parameter system.
A predicate like this is predefined but due to the lack of binding possibilities it gets a bit tricky. First we have to figure out
whether a type is an instance of named_value and then, if it is, we have to check if its name matches the predicate's selection.
To handle this there are three levels of predicates.

    template<typename T> constexpr bool is_named_v;                             // True for named_value instances
    template<fixed_string Name, typename T> constexpr bool has_same_name_v;     // True iff T is a named_value with the same Name.
    template<fixed_string Name> struct is_named_as {                            // Binds Name to the first parameter of has_same_name_v
        template<typename T> struct tpl {
            constexpr static bool value = has_same_name_v<Name, T>;
        };
    };

is_named_as is sort of a meta predicate which you can instantiate and then provide its tpl member as the predicate of the tuple
support functions. To simplify there are overloads of these functions directly taking the name to find.

    template<fixed_string Name, typename TL> constexpr bool tuple_contains() {
        return tuple_contains<is_named_as<Name>::template tpl, TL>();
    }
    template<fixed_string Name, typename TL> constexpr size_t tuple_count_if();
    template<fixed_string Name, typename TL> constexpr size_t tuple_find();

    template<fixed_string Name, typename TL> decltype(auto) get(TL&& t);
    template<fixed_string Name, typename R, typename TL> decltype(auto) get(TL&& t);

    template<fixed_string Name, typename D, typename TL> decltype(auto) get_or(D&& default_value, TL&& t);
    template<fixed_string Name, typename R, typename D, typename TL> decltype(auto) get_or(D&& default_value, TL&& t);
    
The get and get_or overloads perform a ::value to return the embedded T rather than the entire named_value. As you already
provided the name there is no information lost in the process. The versions with a R type parameter can be used to enforce a type
on the returned value, which can be used to control overload resolution and type checking when the returned parameter value is to
be immediately provided to a function call. It has the additional benefit that if the provided value is a tuple like an attempt is
made to construct the R using make_from_tuple, which allows call sites to be more conveniently expressed.


With these functions we have a fairly useful way of implementing the definition of a function taking named_values as parameters.
Here is an example:

    template<typename... Ps> void my_function(Ps&&... pars)
    {
        auto ps = make_ref_tuple(std::forward<Ps>(pars));               // Convert parameters to a tuple of references.

        int stack_size = get_or<"stack_size">(4096, ps);               // Extract the stack_size value or use the default of 4096.
        if constexpr (tuple_contains<"name">(ps))
            cout << "The name is: " << get<"name", std::string>(ps);   // Make sure only strings are provided as names.
    }

This we can fairly conveniently express optional parameters with a known default value and optional parameters that change the
function's behavior depending on their existence. However, we are still struggling at the call sites. The best way we can call
thsi function with the two parameters is:

    my_function(named_value<"stack_size">(2048), named_value<"name">("This is the name"));

In the next section we will provide functionality to make call sites look like named parameters were part of the language, but
first a word about error management. As all name lookup happens at compile time any misspelled names will be detected and any type
mismatches will be caught. As the get and get_or functions return exactly the type that was passed to the function there are
several options on how to react to different types provided for named parameters. The two simplest approaches are shown in the
example above. As the stack_size variable is an int there will be an error if the caller provided some type that isn't convertible
to int. When printing the name it is ok for it to have any type that can be shifted to an ostream. A few other options are
available which can be of interst sometimes:

    std::variant<A, B> var = get_or<"Theaorb">(A(3), ps);    // Allow types convertible to A or B.
    std::any avar = get_or<"anyValue">("Nothing", ps);       // Anything goes, by default you get a const char*.

There is no possiblilty to use a std::optional to check if a parameter was passed. This would have been a rather odd use of
optional as it would be known at compile time if the optional was filled or empty. As the optional would have to be tested for
emptiness before trying to use the value, and by the nature of optional this test could not be a `if constexpr` the pattern seen
above with first testing using tuple_contains seems more appropriate. With something like boost::hana::optional an if constexpr
could be used but to create such a wrapper for the object would still require a new get function for this purpose as the regular
get() has to cause a compile error if the value is not found.


Value names
-----------

To simplify calling with named_value:s as arguments the value_name class is provided. This class is templated on the name and has
templated assignment and call operators which return named_value objects.

    template<fixed_string Name> class value_name {
        template<typename T> auto operator=(T&& value);                 // Return a named_value<Name, T>
        template<typename T> auto operator()(T&& value);                // Return a named_value<Name, T>
        template<typename... Ts> auto operator()(Ts&&... values);       // Return a named_value<Name, tuple<Ts...>>
    };

This allows us to write function calls just as we would expect, using both a=b and a(b) syntax as we like. The multiple parameter
operator() which wraps the values in a tuple is combined with a feature of the get functions to provide a way to for instance give
a two dimensional size directly in the call site using () syntax.

    inline value_name color{"color"};
    inline value_name name{"name"};
    inline value_name size{"size"};

    my_function(color = Colors::Red, name("Hello"), size(40, 50));

As value_names are unaware of which types they are intended to be used for as well as which functions accept them as parameters
value_name objects are highly reusable. By virtue of using a by value template parameter of struct type (C++20) and an inline
declarator all declarations of the name are interchangeable, so it is ok to declare the names that are accepted by a function as
value_name objects anywhere.

For usages within the standard library it is proposed to create a new namespace std::names and stick all value_name:s in there
regardless of where they are used. This is very similar to how std::literals is organized, but due to interchangeability of
value_names I don't think we need any inline subnamespaces for different parts of the library (other than maybe for documentation
purposes).

As an example take the proposal to add stack_size and name to threads
(http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2019r0.pdf). This could be implemented like this:

    namespace std {
        namespace names {
            inline value_name stack_size{"stack_size"};
            inline value_name name{"name"};
        }

        class thread {
            template<typename... Ps> thread(Ps&&... ps);
        };
    }


    using namespace std::names;

    std::thread my_thread(name("My Thread"), stack_size = 10000, [] { std::cout << "Thread running"; });

This requires some additional work on the part of the thread constructor to capture the values of the unnamed parameters. How to
do this is discussed in a later chapter.

This usage of value_name objects at the call site is complemented by even more overloads of the tuple support functions to be able
to use the value_name objects instead of string literals when extracting named_values from the incoming parameter pack. I won't
list them all here but now we can write the example function a bit more succinctly:

    inline value_name name("name");
    inline value_name stack_size("stack_size");

    template<typename... Ps> void my_function(Ps&&... pars)
    {
        auto ps = make_ref_tuple(std::forward<Ps>(pars));            // Convert parameters to a tuple of references.

        int stack_size = get_or<stack_size>(4096, ps);               // Extract the stack_size value or use the default of 4096.
        if constexpr (tuple_contains<name>(ps))
            cout << "The name is: " << get<name, std::string>(ps);   // Make sure only strings are provided as names.
    }



Boolean values
--------------

Special support for providing true or false boolean named_value ojects by just typing the named_value name or its negated
form. This requires some additional machinery. Basically the predicates that select named_values also work for value_names and as
all instantiations of value_name has a `static bool value = true;` the returned value from get and get_or is always correct.
Negated values are simpler, value_name just has a operator! which returns a named_value<Name, bool> filled with a false value.
This allows us to write just the parameter name or !parameter name.

    inline value_name name("a");
    inline value_name name("b");

    template<typename... Ps> void my_function(Ps&&... pars)
    {
        auto ps = std::make_ref_tuple(pars);
        bool has_a = std::get<a>(pars);
        bool has_a = std::get<b>(pars);
    }

    my_function(a, !b);


Argument binding
----------------

One major drawback with this system as presented so far is that there is no way to check that the caller didn't provide additional
arguments that were not used by the called function. The bind_parameters function provides a way to specify what arguments are
allowed, and how to 


    
    template<typename... As, typename... Ps> auto bind_parameters(tuple<As...>, Ps... parameters);

    // Use like this:

    template<typename... Ps> my_function(Ps&&... pars)
    {
        auto ps = bind_parameters(make_ref_tuple(std::forward<Ps>(pars)...),
            stack_size = 100,                         // A stack_size parameter convertible to int is allowed.
            name.optional<std::string>(),             // A name string is allowed, no default is provided.
            other.variant<int, float>(),              // An other is allowed, with int or float type.
            something.any(),                          // A something of any type is allowed.
            flag,                                     // a bool flag may be given, defaults to true
            !noflag,                                  // a bool noflag may be given, defaults to false.
            third.mandatory<int>()                    // A third with int value must be given.
        );

        int stack_size = get<stack_size>(pars);
        auto name = get<name>(pars);                                        // This is an std::optional<std::string>.
    }


This is probably possible to implement and looks fairly attractive. The bind_parameters function captures all information about the
allowed parameters and analyzes the incoming arguments, static_asserting on any problems, then it returns a tuple
similar to the one returned by make_ref_tuple which can be queried as usual. Note however that as default values can be provided
already to bind_parameters (this also implies the type allowed) there is no need to use get_or when retrieving the value from the
returned tuple later, or to explicitly specify the type of the receiving variable.

To be able to differentiate between incoming arguments and specifiers for the allowed parameters the arguments must be wrapped in
a tuple which is passed as the first argument to bind_parameters. This has the additional advantage that the user has the choice
to use make_ref_tuple or in case that the values are to be retained in the tuple returned by bind_parameters make_tuple can be
used instead.

The ps variable is a regular tuple which can then be queried as usual. As parameter::binder::bind static_asserts against any
non-conforming arguments it will also protect against trying to retrieve any named parameters that were not specified to
check_arguments further down in the body. The check_arguments function returns a callable which is then

The value_name object gets some auxiliary methods which return marker type values used by check_arguments to check incoming
arguments for consistency.

This exact formulation has not yet been implemented.


Unnamed parameters
------------------

Unnamed parameters are mostly handled by regular parameters preceeding the variadic pack for the named parameters. However, in
some cases it is inconvenient to restrict the variadic parameters to just named parameters. There are a few typical cases:

1. The function has defaulted positional parameters. In this case we don't want to express these as usual as you would have to
spell out their defaults at each call site to be able to provide named arguments. The ideal user experience is to be able to deal
with default parameters as usual, and then provide named arguments anyway.

2. The function may want to treat a unnamed parameter of a certain type as a named parameter of a certain name. This functionality
is provided by the boost::Parameter library.

3. It may be inconvenient to force the named arguments to be the last arguments. While this is common practice in many languages
there are cases when it becomes cumbersome, especially when lambdas are involved. Case in point is the thread improvement proposal
where the named properties are placed first.

4. There are cases where two or more packs of parameters of dissimilar types is a logical API. Today we can't express this easily
(i.e. without building something like this ourselves).

The tuple support functions, with the help of the is_named predicate, especially the tuple_find_nth function are useful to program
all of these patterns. This however takes some quite serious template metaprogramming to accomplish and can be quite error prone.

To minimize the effort there are special constructs to use among the check_arguments constructor parameters to implement these
cases. These come in two forms. The simplest one is only useful for defaulted positional parameters and consists of just writing
the default values of those parameters as the first parameters to the check_arguments call. Note that the emulated type of
the defaulted parameter is the exact type of the default value, which sometimes is not correct. The second form allows you to be
explicit about this type, as well as provides ways to handle the other use cases.

To specify such parameters or parameter groups a special unnamed object is used. This keeps the syntax used similar to when
working with value_name objects to create optionals, variants and anys. The `unnamed` singleton is however of a separate type with
its own separate methods to specify the use cases detailed above, although some of them work similarly as the corresponding
functions in value_name.

    
    template<typename... As, typename... Ps> auto bind_parameters(tuple<As...>, Ps... parameters);

    // Use like this:

    template<typename... Ps> my_function(Ps&&... pars)
    {
        auto ps = bind_parameters(make_ref_tuple(std::forward<Ps>(pars)...),
            100,                                      // First value in Ps can be an int, or 100 is used. (confusing?).
            unnamed = 200,                            // Second value in Ps has the same meaning as the first. (usable?).
            unnamed.typed<float>(3),                  // Third is a float which defaults to 3.

            label.default_for<std::string>(),         // Specifying a string somewhere yields a
            named_value<label, std::string>,          // If a string is specified after the three first parameters it is a label.
            stack_size = 100,                         // A stack_size parameter convertible to int is allowed.
            first.typed<int>(),                       // A first parameter can be given if it is convertible to int.
            name.optional<std::string>(),             // A name string is allowed, no default is provided.
            other.variant<int, float>(),              // An other is allowed, with int or float type.
            something.any(),                          // A something of any type is allowed. The returned variant includes mononstate
            flag,                                     // a bool flag may be given, defaults to true
            !noflag,                                  // a bool noflag may be given, defaults to false.
            third.mandatory<int>()                    // A third with int value must be given.

            unnamed.mandatory(),                      // Counting from the end the first unnamed parameter (if any) can be of any type.
            unnamed.pack<double>()                    // After that any number of doubles are captured as a std::array
            unnamed.pack()                            // And starting with the first non-double (compatible) parameter the rest are captured as a tuple.
        );

        int stack_size = get<stack_size>(pars);
        auto name = get<name>(pars);                                        // This is an std::optional<std::string>.
    }


There are a few rules to note here. Firstly each unnamed or named parameter generates exactly one element in the returned tuple,
in the same order. To get the unnamed parameters at the end using tuple_find_nth can be used to avoid having to count how many
named parameters are specified, which would be error prone.

The parameter set is subdivided into up to three sections: First section contains zero or more unnamed defaulted parameters. For
these default values must be provided, or the optional<T>() functionaoity must be used. When matching the parameters in this
section the first argument that doesn't match its corresponding parameter ends this section. The second section contains all named
parameters and these matches all named arguments beyond the already matched defaulted parameters. This includes those specified
with default_for which can consume _any_ of the remaining unnamed arguments. Section three contans those unnamed parameters that
are specified after any named parameters. These are matched in order from the set of remaining unnamed arguments. It is unclear if
the order between unnamed arguments in section two and thee should be enforced. I think it may be a bit tricky to implement such
enforcement but it could be valuable from a debugging perspective to put some more restrictions on the order between unnamed and
named parameters at call sites, for instance that all named arguments plus and default_for matched argumets (i.e. section two
arguments) must preceed section three arguments. This seems logical and doable.

When a pack parameter is reached in section one it will consume all remaining unnamed arguments that preceed the first named
argument or until one that doesn't match the type is encountered. This can be viewed as a rule that a named argument can never
match an unnamed pack. As a side note it would actually possible to allow named packs too, in which case the first of this pack is
the one with the name and then it is followed by a list of unnamed arguments until the next named argument. This is often how
command line parameters are designed, if you remove the -- before the name and the equals and commas after. There is however
nothing that corresponds to the -- on the command line to take parsing back to positional mode, save for a mismatching parameter
type.

    template<typename... Ps> std::string concat_all_files(Ps&&... pars)
    {
        auto ps = bind_parameters(make_ref_tuple(std::forward<Ps>(pars)...),
            filename.pack<std::path>()
        );

        std::vector<std::path> filenames = get<filename>(ps);
    }

    std::string allfiles = concat_all_files(filename="first.file", "second.txt", "third.cpp");

This does not seem to offer any advantage over a regular std::vector<std::path> parameter though, named or unnamed. 

It might be an idea that interests some to implement a operator- and operator-- on value_name to be able to use the -name or
--name syntax, but I don't think it adds any value.

Thus it seems that we have even sections which have unnamed arguments, and odd sections which have named arguments. Thus it could
be possible to again switch over to named arguments. I don't think this is a good idea as it would be too hard to know which
section to write the named arguments in. We should rather view the three sections as a way to increase the flexibility, mainly
for use with lambdas, which we want to place last.

To sum up the first section consumes as much as it can of unnamed arguments. Then the named parameters are matched, and
default_for parameters are located by checking succeding unnamed arguments. If one is found it is removed from the search. Once
all named parameters have been considered an error check is done to ensure that there are no remaining named parameters or unnamed
arguments that preceed the last named argument (given that default_for matched arguments are removed). Finally the third section
starts at this point, and proceeds as the first section. Finally a check that all arguments were consumed is made.

This means that in the example above a string provided as for instance the second argument ends the first section, and is then
matched as a label. 

### Breaking the fixed type rule

The unnamed.mandatory() and unnamed.pack(), by not specifying the type, breaks the rule that the tuple returned from
bind_parameters is not affected by the argument types. This is similar to the named.optional() and named.mandatory() which also
lets the returned tuple element type reflect the actual argument type. Typed packs and mandatory parameters do not break this rule
though. Maybe a typed pack should return an std::array for additional performance, this is debatable.

### Fixing overload resolution.

If we can figure out how to make bind_parameters fail in a SFINAE friendly way on errors we could use this together with enable_if
to create substitution failures if argument lists don't allow calling the function:

    parameter_binder my_function_binder(100, size_type.optional<int>());

    template<typename... Ps, typename = decltype(my_funtion_binder(std::forward<Ps>(ps)...)> void my_function(Ps&&...)
    {
        auto pars = my_function_binder(std::forward<Ps>(ps)...);

        int ss = get<stack_size>(pars);
    }


This suggests that to avoid the repeat_everything part bind_parameters should be an object that is constructed first for the
parameters and then actuals are applied to it to check and generate the return type. This reduces the job of defining the
allowed parameters.

This type of signature removal allows limited overloading but does nothing to prevent for instance ambiguity between an overload
where the first optional parameter is a double and another where it is an int. Both are valid and when presented with either an
integer or double value it is ambiguous. It might be possible to go further by wrapping the call site into some special selector
magic similar to the one that allows variant to select the right alternative in this type of case, but that would be very
complicated and generate enormous amounts of extra template instantiations.


Type erased parameter container
-------------------------------

Note that the container returned by bind_parameters has the same type regardless of the type of the tuple used as its first
parameter. This at least holds as long as all parameters are named or typed. This fact can be used to reduce code bloat by either
smart compilers (less likely) or smart programmers. While the return type of bind_parameters is unspecified it can be forwarded to
a worker function which while it still needs to be a template will be instantiated only once.

It may be possible to type erase the returned type even more so that its name can be specified. Then it would be possible to move
the implementation of the worker function to a cpp file. This would however induce unnecessary inefficiencies for more performance
critical applications. Instead it should be possible to implement a type erased version which has a templated constructor from the
return type of bind_parameters.

    class arguments {
        template<typename T> arguments(T&& args);
    };

    template<value_name, typename T> get(arguments&& args);

To simplify migration between type erased and templated versions there should be overloads of the get function for `arguments`
too. The type will however always have to be given as there is no way it can be conjured up given that arguments is type erased.
The type must match either the type given to check_arguments. For variant, any and optional the specified type must be the
corresponding std:: type as arguments can only check using typeid matching. The application must check the existance and type of
the returned value as usual after the get.

Here is a typical example of how type erased parameter handling can be implemented.

    // Non-template helper
    void my_function_impl(arguments&& args)
    {
        auto ss = get<stack_size, int>(args);
        auto n = get<name, std::string>(args);
    }

    // Templated facade.
    template<typename... Ps> void my_function(Ps&&... ps)
    {
        my_function_impl(parameter_binder>(
                             stack_size = 1024,
                             name.optional<int>()
                         )(std::forward<Ps>(ps)...));
    }


Combining parameter_binders.
---------------------------

In some cases, notably widget hierarchies, many functions take the same set of named parameters, or subsets of those of other
types of widgets. To repeat these fairly large numbers of parameters can be very tedious. To help with this it is possible to wrap
some constructor parameters of a parameter_binder into a tuple, which can be named and reused. This is done by flattening the
parameters before storing them:

        template<typename... Ts> class parameter_binder {
            using data_type = tuple_flatten_type<Ts...>;
        public:
            parameter_binder(Ts... ts) : m_data(tuple_flatten(tuple(ts...)) {}

        private:
            data_type m_data;
        };






Pack oriented functions
=======================

It is often just a detour to wrap parameter packs into tuples before doing tuple support functions on them. Therefore there is a
parallel set of pack_find, pack_get, pack_contains etc. functions available. As these functions can't return packs anyway they
have to return tuples. This means that all we save is an initial make_ref_tuple at the start of a processing chain.