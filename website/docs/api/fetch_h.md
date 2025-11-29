# group `fetch_h` {#group__fetch__h}

Simplified web fetch implementation that wraps tcp socket code.

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`enum `[`fetch_frame_e`](#group__fetch__h_1gaa11b8b1be285a6ffb2b605d16d916589)            | Set how [fetch](website/docs/api/undefined.md#group__fetch__h_1gae2ade91c016425d325d49524ff3a218a) will interpret stream bytes.
`public int `[`fetch`](#group__fetch__h_1gae2ade91c016425d325d49524ff3a218a)`(const char * url,struct `[`fetch_init`](website/docs/api/fetch_h.md#structfetch__init)` init)`            | Connects to host at URL over tcp and writes optional HTTP fields in INIT to the request. It returns the socket fd *after* sending the corresponding HTTP request encoded in URL.
`public char * `[`fetch_pop`](#group__fetch__h_1ga585dd7049ee255d6147a22aee62a6b91)`(int fd,size_t * length)`            | Pop the next parser framed object from the fetch buffer at FD. If LENGTH is passed in, this sets LENGTH to the length of the bytes *not including NULL TERMINATOR*, meaning the returned char buffer has total size `LENGTH + 1`.
`public static `[`buffer`](website/docs/api/undefined.md#group__fetch__h_1gab9ae1042371fae1e305253490ac72946)` * `[`append`](#group__fetch__h_1ga6a03299a996c775f5c9a4554934e61cd)`(char ** bufptr,size_t len)`            | 
`public static `[`buffer`](website/docs/api/undefined.md#group__fetch__h_1gab9ae1042371fae1e305253490ac72946)` * `[`string`](#group__fetch__h_1gaf4ef797bf728d67eebc17f319c4eb238)`(const char * fmt,...)`            | Returns a char * buffer that is meant to hold a string of size LENGTH. Uses 64 bits (or whatever your `unsigned long` size is) to save the length.
`public static size_t `[`len`](#group__fetch__h_1ga0ec73d88c8c12ca2039c88aa8fa737ba)`(const `[`buffer`](website/docs/api/undefined.md#group__fetch__h_1gab9ae1042371fae1e305253490ac72946)` * p)`            | 
`public static char * `[`to_lower`](#group__fetch__h_1ga24e3ba2eda6635f5cbbe540848d66290)`(char * s,size_t len)`            | 
`public static void `[`trim_slice`](#group__fetch__h_1ga5134bc3712b510e03a2e3eedb0aed884)`(const char ** begin,const char ** end)`            | 
`public static char * `[`remove_all`](#group__fetch__h_1ga3028dbd767e76369d5e520215371c8e0)`(const char * str,size_t * n,char ch)`            | Returns a *new* string with CH removed from STR (of length N). Sets N to the new size if it is passed in. Otherwise, it will compute `strlen` on STR to handle the removal.
`public static char * `[`trim`](#group__fetch__h_1ga044d803ca462d6176fd363c2c2270f12)`(const char * b,const char * e)`            | 
`public static char ** `[`split`](#group__fetch__h_1gaa6bb674633761cf91cdced6c342d7dd1)`(const char * str,char delim,int * out_count,int * lens)`            | 
`struct `[`fetch_init`](#structfetch__init) | Optional fields to write in the Request.

## Members

#### `enum `[`fetch_frame_e`](#group__fetch__h_1gaa11b8b1be285a6ffb2b605d16d916589) {#group__fetch__h_1gaa11b8b1be285a6ffb2b605d16d916589}

 Values                         | Descriptions                                
--------------------------------|---------------------------------------------
FRAME_JSON_OBJ            | Queues 1 top level JSON object at a time.
FRAME_TEXT            | "Trivial" queue frame, as this just passes through the incoming bytes *as-is* to the queue.

Set how [fetch](website/docs/api/undefined.md#group__fetch__h_1gae2ade91c016425d325d49524ff3a218a) will interpret stream bytes.

#### `public int `[`fetch`](#group__fetch__h_1gae2ade91c016425d325d49524ff3a218a)`(const char * url,struct `[`fetch_init`](website/docs/api/fetch_h.md#structfetch__init)` init)` {#group__fetch__h_1gae2ade91c016425d325d49524ff3a218a}

Connects to host at URL over tcp and writes optional HTTP fields in INIT to the request. It returns the socket fd *after* sending the corresponding HTTP request encoded in URL.

#### `public char * `[`fetch_pop`](#group__fetch__h_1ga585dd7049ee255d6147a22aee62a6b91)`(int fd,size_t * length)` {#group__fetch__h_1ga585dd7049ee255d6147a22aee62a6b91}

Pop the next parser framed object from the fetch buffer at FD. If LENGTH is passed in, this sets LENGTH to the length of the bytes *not including NULL TERMINATOR*, meaning the returned char buffer has total size `LENGTH + 1`.

Returns NULL either on EOF.

#### `public static `[`buffer`](website/docs/api/undefined.md#group__fetch__h_1gab9ae1042371fae1e305253490ac72946)` * `[`append`](#group__fetch__h_1ga6a03299a996c775f5c9a4554934e61cd)`(char ** bufptr,size_t len)` {#group__fetch__h_1ga6a03299a996c775f5c9a4554934e61cd}

#### `public static `[`buffer`](website/docs/api/undefined.md#group__fetch__h_1gab9ae1042371fae1e305253490ac72946)` * `[`string`](#group__fetch__h_1gaf4ef797bf728d67eebc17f319c4eb238)`(const char * fmt,...)` {#group__fetch__h_1gaf4ef797bf728d67eebc17f319c4eb238}

Returns a char * buffer that is meant to hold a string of size LENGTH. Uses 64 bits (or whatever your `unsigned long` size is) to save the length.

#### `public static size_t `[`len`](#group__fetch__h_1ga0ec73d88c8c12ca2039c88aa8fa737ba)`(const `[`buffer`](website/docs/api/undefined.md#group__fetch__h_1gab9ae1042371fae1e305253490ac72946)` * p)` {#group__fetch__h_1ga0ec73d88c8c12ca2039c88aa8fa737ba}

#### `public static char * `[`to_lower`](#group__fetch__h_1ga24e3ba2eda6635f5cbbe540848d66290)`(char * s,size_t len)` {#group__fetch__h_1ga24e3ba2eda6635f5cbbe540848d66290}

#### `public static void `[`trim_slice`](#group__fetch__h_1ga5134bc3712b510e03a2e3eedb0aed884)`(const char ** begin,const char ** end)` {#group__fetch__h_1ga5134bc3712b510e03a2e3eedb0aed884}

#### `public static char * `[`remove_all`](#group__fetch__h_1ga3028dbd767e76369d5e520215371c8e0)`(const char * str,size_t * n,char ch)` {#group__fetch__h_1ga3028dbd767e76369d5e520215371c8e0}

Returns a *new* string with CH removed from STR (of length N). Sets N to the new size if it is passed in. Otherwise, it will compute `strlen` on STR to handle the removal.

#### `public static char * `[`trim`](#group__fetch__h_1ga044d803ca462d6176fd363c2c2270f12)`(const char * b,const char * e)` {#group__fetch__h_1ga044d803ca462d6176fd363c2c2270f12}

#### `public static char ** `[`split`](#group__fetch__h_1gaa6bb674633761cf91cdced6c342d7dd1)`(const char * str,char delim,int * out_count,int * lens)` {#group__fetch__h_1gaa6bb674633761cf91cdced6c342d7dd1}

# struct `fetch_init` {#structfetch__init}

Optional fields to write in the Request.

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public const char * `[`method`](#structfetch__init_1adf7438823facb8ff78693175108cbead) | A *static* string to declare the HTTP method. Case insensitive.
`public char * `[`headers`](#structfetch__init_1a498a85763e9ffe407cf1563056a6e94a) | Plaintext HTTP headers.
`public char * `[`body`](#structfetch__init_1a62af633619d435aed048b4b027e61261) | For POST requests.
`public int `[`frame`](#structfetch__init_1af6b0942da9052ea23d73adc1ebfe38fc) | A [fetch_frame_e](website/docs/api/undefined.md#group__fetch__h_1gaa11b8b1be285a6ffb2b605d16d916589) flag to determine byte enqueue strategy.

## Members

#### `public const char * `[`method`](#structfetch__init_1adf7438823facb8ff78693175108cbead) {#structfetch__init_1adf7438823facb8ff78693175108cbead}

A *static* string to declare the HTTP method. Case insensitive.

#### `public char * `[`headers`](#structfetch__init_1a498a85763e9ffe407cf1563056a6e94a) {#structfetch__init_1a498a85763e9ffe407cf1563056a6e94a}

Plaintext HTTP headers.

#### `public char * `[`body`](#structfetch__init_1a62af633619d435aed048b4b027e61261) {#structfetch__init_1a62af633619d435aed048b4b027e61261}

For POST requests.

#### `public int `[`frame`](#structfetch__init_1af6b0942da9052ea23d73adc1ebfe38fc) {#structfetch__init_1af6b0942da9052ea23d73adc1ebfe38fc}

A [fetch_frame_e](website/docs/api/undefined.md#group__fetch__h_1gaa11b8b1be285a6ffb2b605d16d916589) flag to determine byte enqueue strategy.

