A pure library named parameter proposal
=======================================

This proposal provides a way to use named function parameters in C++. It is a pure library solution which obviously makes it a bit
less elegant than a language feature has potential to be. The proposal is built in several steps, which may or may not be broken
out to separate proposal in a later stage. These initial parts revolve around predicates and new functions related to packs and
tuple-likes. Using these mechanisms a framework for named parameters is built.

This proposal to some extent relies on the existence of a fixed_string class which can be used as C++20 template parameters. I
assume that such a class will be included in the next C++ version. If not it can be included here.

During development of the reference implementation it was noted that several proposed and unproposed C++ core language features
would help in simplifying both the implementation and usage of the functionality proposed here. Presently the proposal is written
to not take advantage of any features that are not in C++20.

This repository contains both the description of the proposal and a
reference implementation. The proposal text, such as it stands, can be
found here:

(named_parameters.md)
