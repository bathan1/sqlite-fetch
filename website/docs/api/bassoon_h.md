# group `bassoon_h` {#group__bassoon__h}

A JSON deque wrapper over byte streams.

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public struct `[`bassoon`](website/docs/api/bassoon_h.md#structbassoon)` * `[`use_bass`](#group__bassoon__h_1gadcbfe052e3b367dfe37fa8f100ef562e)`()`            | Allocate a clarinet handle on the heap.
`public void `[`bass_free`](#group__bassoon__h_1ga50e4b6eeb564dba62b4191bb166e3778)`(struct `[`bassoon`](website/docs/api/bassoon_h.md#structbassoon)` * bass)`            | Free the bassoon buffer at BASS.
`public char * `[`bass_pop`](#group__bassoon__h_1ga1dc005e52d9cefdc31fcf3b8fa09e0db)`(struct `[`bassoon`](website/docs/api/bassoon_h.md#structbassoon)` * bass)`            | Pop an object from the queue in BASS, or NULL if it's empty.
`struct `[`bassoon`](#structbassoon) | A JSON object queue with a writable file descriptor. You can write whatever bytes you want to it...

## Members

#### `public struct `[`bassoon`](website/docs/api/bassoon_h.md#structbassoon)` * `[`use_bass`](#group__bassoon__h_1gadcbfe052e3b367dfe37fa8f100ef562e)`()` {#group__bassoon__h_1gadcbfe052e3b367dfe37fa8f100ef562e}

Allocate a clarinet handle on the heap.

#### `public void `[`bass_free`](#group__bassoon__h_1ga50e4b6eeb564dba62b4191bb166e3778)`(struct `[`bassoon`](website/docs/api/bassoon_h.md#structbassoon)` * bass)` {#group__bassoon__h_1ga50e4b6eeb564dba62b4191bb166e3778}

Free the bassoon buffer at BASS.

#### `public char * `[`bass_pop`](#group__bassoon__h_1ga1dc005e52d9cefdc31fcf3b8fa09e0db)`(struct `[`bassoon`](website/docs/api/bassoon_h.md#structbassoon)` * bass)` {#group__bassoon__h_1ga1dc005e52d9cefdc31fcf3b8fa09e0db}

Pop an object from the queue in BASS, or NULL if it's empty.

# struct `bassoon` {#structbassoon}

A JSON object queue with a writable file descriptor. You can write whatever bytes you want to it...

...like bytes from a TCP stream.

Reads are constant-time from both its head and its tail, so it's technically a "deque".

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public char ** `[`buffer`](#structbassoon_1ab13115333dda3d59436d9be1e9b086a5) | Underlying buffer on the HEAP.
`public unsigned long `[`cap`](#structbassoon_1acb27efcc5f4cf60633e267216f66ea4a) | [buffer](website/docs/api/bassoon_h.md#structbassoon_1ab13115333dda3d59436d9be1e9b086a5) capacity, i.e. read/write at `BUFFER[x >= CAP]` is UB.
`public unsigned long `[`hd`](#structbassoon_1aefad588360749b19b906c954d88c070a) | First in end.
`public unsigned long `[`tl`](#structbassoon_1af3ccefd93c8c0ce1ea913a905ad06b61) | Last in end.
`public unsigned long `[`count`](#structbassoon_1a30ecaa51f4d3ec25c6065b1df5ffdd1f) | Stored size. Is updated dynamically from calls to [bass_pop](website/docs/api/undefined.md#group__bassoon__h_1ga1dc005e52d9cefdc31fcf3b8fa09e0db).
`public FILE * `[`writable`](#structbassoon_1ae16b012f6f870ceebc05091a8ebba882) | Plain writable "stream" of the queue so you can just `fwrite()` on it.

## Members

#### `public char ** `[`buffer`](#structbassoon_1ab13115333dda3d59436d9be1e9b086a5) {#structbassoon_1ab13115333dda3d59436d9be1e9b086a5}

Underlying buffer on the HEAP.

#### `public unsigned long `[`cap`](#structbassoon_1acb27efcc5f4cf60633e267216f66ea4a) {#structbassoon_1acb27efcc5f4cf60633e267216f66ea4a}

[buffer](website/docs/api/bassoon_h.md#structbassoon_1ab13115333dda3d59436d9be1e9b086a5) capacity, i.e. read/write at `BUFFER[x >= CAP]` is UB.

#### `public unsigned long `[`hd`](#structbassoon_1aefad588360749b19b906c954d88c070a) {#structbassoon_1aefad588360749b19b906c954d88c070a}

First in end.

#### `public unsigned long `[`tl`](#structbassoon_1af3ccefd93c8c0ce1ea913a905ad06b61) {#structbassoon_1af3ccefd93c8c0ce1ea913a905ad06b61}

Last in end.

#### `public unsigned long `[`count`](#structbassoon_1a30ecaa51f4d3ec25c6065b1df5ffdd1f) {#structbassoon_1a30ecaa51f4d3ec25c6065b1df5ffdd1f}

Stored size. Is updated dynamically from calls to [bass_pop](website/docs/api/undefined.md#group__bassoon__h_1ga1dc005e52d9cefdc31fcf3b8fa09e0db).

#### `public FILE * `[`writable`](#structbassoon_1ae16b012f6f870ceebc05091a8ebba882) {#structbassoon_1ae16b012f6f870ceebc05091a8ebba882}

Plain writable "stream" of the queue so you can just `fwrite()` on it.

