# Contributing to SQLite Fetch
`sqlite-fetch` is a C based project that maps a subset of the Web APIs to the 
[SQLite](https://www.sqlite.org/) C API. It is distributed as a single dynamic 
library file.

`sqlite-fetch` is not a fork of SQLite, and is not a strict implementation of the [WHATWG](https://whatwg.org/) Fetch API. 
It also does not implement a formalized mapping between C and JS.

Despite the lack of a spec to adhere to (the *what*) and lack of a formal way to map
objects in C to and from JS (the *how*), `sqlite-fetch` prioritizes an intuitive and practical 
API design for calling Web API functions.

Any Web API outlined in the [mdn docs](https://developer.mozilla.org/en-US/docs/Web/API) is
in scope for this project to implement. The only true requirements for a Web API implementation 
to be included are that:

1. SQLite can [link to it dynamically](https://www.sqlite.org/loadext.html).
2. For each relevant object in the Web API, there also exists an object in SQLite that can *reasonably* represent it.

## Design Requirements (the *what*)
#### 1) Dynamically Linkable
Extension code shouldn't mess with the core API the SQLite amalgamation provides. In other words,
common SQLite bindings to the core API (like the SQLite shell) should be able to load the extension
at runtime.

#### 2) SQL(ite) Familiar
For each way to invoke Web API(s) in SQLite, usage should be as close as possible to standard SQL(ite).

## Implementation Guidelines (the *how*)
Extension logic and Web API implementations are encoded in 
the c files. Javascript is used to facilitate easier testing 
and to provide a convenient way to load the extension from a JS runtime.

### C

### JS

### Documentation

## Integration + Release