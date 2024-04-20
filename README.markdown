# effects: An experimental C++ runtime effect system

`effects.hpp` provides a header-only library for tracking effects in C++ to
facilitate runtime checking of how impure source code currently is.
Relying on `effects.hpp` for effect tracking helps to prevent source code
from becoming worse in the future as it changes and it encourages source code
to have more referentially transparent functions.

## What is Referential Transparency?

[Referential transparency](https://en.wikipedia.org/wiki/Referential_transparency)
is often misunderstood and considered unimportant, though
referential transparency provides a clear benefit for ensuring source code
is reliable, both in its current form and as it changes in the future.
A function is referentially transparent if a function call with
specific arguments can always be substituted with its return value.

When functions are referentially transparent, all testing can be limited to
the function inputs to ensure the function outputs are valid.
If functions are not referentially transparent, their testing will always
require modifying global state in ways that are typically undefined
and the source code will avoid being maintainable
(causing both unstable execution as source code changes and
 increased labor costs when the source code is used).
All source code generally is unable to have all functions be
referentially transparent, due to necessary effects.
The goal of `effects.hpp` is to represent all possible effects in
C++ to make their tracking simpler and easier to understand.

Often IO operations are considered the main processing that prevents a
function from being referentially transparent, but that is an
oversimplification that avoids tracking the types of effects that exist.

## What is "purity"?

When software developers use the term "purity" or "pure function"
they are describing the least effects common to their programming language.
Mathematical purity is referential transparency in all possible
execution environments (any hardware variation with any operating system).
The [Haskell](https://en.wikipedia.org/wiki/Haskell) programming language
has its purity (in source code outside of monads with lazy evaluation)
as execution that can be nonterminating, contain (asynchronous) exceptions and
have variation due to the Operating System (OS) used or the hardware.used.

A programming language may describe its purity as only relating to a
single execution (in a single execution environment) and that can be
called "operational purity" instead of mathematical purity.
Pure functions with operational purity could provide different return values
with the same function arguments if different execution environments are
compared (different Operating Systems and/or different hardware).

## How is `effects.hpp` used?

The **effect kind** is the type of effect tracked as
`effects::kind` bit field values stored with bitwise-OR:

    pure                = 0x0000, // mathematical purity
    nonterminating      = 0x0001, // execution may not terminate
    exception           = 0x0002, // throw/(signal)/exit/abort
    reference           = 0x0004, // reference to global data not owned
    write               = 0x0008, // write to global (heap) data owned
    fpe                 = 0x0010, // Floating-Point Exceptions (FPE)
    variation_os        = 0x0020, // Operating System (OS) variation
    variation_hardware  = 0x0040, // hardware variation

To track the effects inside a function or through multiple function calls,
an `effects::context` object is created to track the use of allocated data.
An example from `tests.cpp` is provided below:

    context c(kind::reference | kind::fpe, context_type::terminating);
    region<double> value_rounded = c(2.0 / 3);

The `effects::context` constructor requires the effects that are considered
valid execution and whether execution will always terminate in a finite
amount of time.  The `effects::region` data is created for manipulating the
data while the `effects::context` object tracks the related effects during
execution.

When the `effects::context` usage is complete, the valid function should be
checked with an assert function (that is not omitted with compilation options).

    assert(c.valid());

### Limitations

* The `nonterminating` effect depends only on the `effects::context`
  constructor parameter and whether the developer knows if timeouts
  are finite and infinite loops are impossible.
* The `exception` effect requires the developer identifies all usage of
  throw to throw C++ exceptions, exit/abort function call paths, and
  any way an unignored signal could be raised.  For each instance,
  the developer should use the `effects::context` `set_exception` function.
* The `effects::region<T &>` type is assumed to be a reference to global data.
* Using stdout, stderr, or other file descriptors is a `reference` effect
  that could be created as a `effects::region<T &>` type using a local variable
  but requires the developer is aware of the file descriptors being used.
* All `effects::region` non-null pointers are assumed to be allocated from
  the heap (to create a `write` effect).
* The `variation_os` and `variation_hardware` effects require that the
  developer is aware of different execution in different environments.
  For each instance, the developer should use the `effects::context`
  `set_variation_os` and `set_variation_hardware` functions (respectively).

While these limitations may look intimidating, the verification of effects
is still helpful for ensuring source code is reliable.

A programming language with a compile-time
[effect system](https://en.wikipedia.org/wiki/Effect_system)
connected to the programming language's type system would help to avoid
developer mistakes and ensure enforcement.

## Futher Information

`effects.hpp` was partly inspired by "Function Effect Tags" in the
[ATS2/Postiats programming language](https://en.wikipedia.org/wiki/ATS_(programming_language))
(described [here](https://github.com/CloudI/CloudI/blob/0845ec697b35e46a0af8426f843f8fd3c4080039/src/api/ats/v2/cloudi.sats#L98)
and [here](https://bluishcoder.co.nz/2010/06/13/functions-in-ats.html)).

## Test

    make

## Author

Michael Truog (mjtruog at protonmail dot com)

## License

MIT License

