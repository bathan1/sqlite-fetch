# group `functions` {#group__functions}

All public functions available to `yarts.c`

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public `[`clarinet_t`](website/docs/api/undefined.md#structclarinet)` * `[`use_clarinet`](#group__functions_1ga62677a495670b7d9fea1b03830cce42d)`()`            | Allocate a clarinet handle on the heap.
`public void `[`clarinet_free`](#group__functions_1gab661ad0b208450a297765102a11aca9d)`(`[`clarinet_t`](website/docs/api/undefined.md#structclarinet)` * clare)`            | Free the clarinet buffer at CLARE.
`public char * `[`clarinet_pop`](#group__functions_1ga5f4716803d94b3599f4e7785444b63f1)`(`[`clarinet_t`](website/docs/api/undefined.md#structclarinet)` * clare)`            | Pop an object from the queue in CLARE, or NULL if it's empty.

## Members

#### `public `[`clarinet_t`](website/docs/api/undefined.md#structclarinet)` * `[`use_clarinet`](#group__functions_1ga62677a495670b7d9fea1b03830cce42d)`()` {#group__functions_1ga62677a495670b7d9fea1b03830cce42d}

Allocate a clarinet handle on the heap.

#### `public void `[`clarinet_free`](#group__functions_1gab661ad0b208450a297765102a11aca9d)`(`[`clarinet_t`](website/docs/api/undefined.md#structclarinet)` * clare)` {#group__functions_1gab661ad0b208450a297765102a11aca9d}

Free the clarinet buffer at CLARE.

#### `public char * `[`clarinet_pop`](#group__functions_1ga5f4716803d94b3599f4e7785444b63f1)`(`[`clarinet_t`](website/docs/api/undefined.md#structclarinet)` * clare)` {#group__functions_1ga5f4716803d94b3599f4e7785444b63f1}

Pop an object from the queue in CLARE, or NULL if it's empty.

