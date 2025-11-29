# Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`struct `[`ccookie_t`](#structccookie__t) | 
`struct `[`clarinet`](#structclarinet) | A JSON object queue with a writable file descriptor. You can write whatever bytes you want to it...
`struct `[`clarinet_state`](#structclarinet__state) | 
`struct `[`column_def`](#structcolumn__def) | 
`struct `[`Fetch`](#structFetch) | The SQLite virtual table
`struct `[`fetch_cursor`](#structfetch__cursor) | Cursor.
`struct `[`fetch_init`](#structfetch__init) | 
`struct `[`fetch_state`](#structfetch__state) | 
`struct `[`status_entry_t`](#structstatus__entry__t) | 
`struct `[`url`](#structurl) | Taken from Web API URL, word 4 word, bar 4 bar.

# struct `ccookie_t` 

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public yajl_handle `[`parser`](#structccookie__t_1a75df225ee3d7807f7d27e33922226af2) | 
`public struct `[`clarinet_state`](#structclarinet__state)` * `[`state`](#structccookie__t_1a5fc7e21974758603417daa269d084aeb) | 

## Members

#### `public yajl_handle `[`parser`](#structccookie__t_1a75df225ee3d7807f7d27e33922226af2) 

#### `public struct `[`clarinet_state`](#structclarinet__state)` * `[`state`](#structccookie__t_1a5fc7e21974758603417daa269d084aeb) 

# struct `clarinet` 

A JSON object queue with a writable file descriptor. You can write whatever bytes you want to it...

...like bytes from a TCP stream.

Reads are constant-time from both its head and its tail, so it's technically a "deque".

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public char ** `[`buffer`](#structclarinet_1a76e6eefd69948c25d34fab86c6a23c51) | Underlying buffer that [clarinet](#structclarinet) will free via [clarinet_free](#clarinet_8h_1ad548d76cca4928882344c65e33644636).
`public unsigned long `[`cap`](#structclarinet_1a7c5bdd62b0640ecccf0b1c0b82acd0d6) | [buffer](#structclarinet_1a76e6eefd69948c25d34fab86c6a23c51) capacity, i.e. read/write at `BUFFER[x >= CAP]` is UB.
`public unsigned long `[`hd`](#structclarinet_1a2de8627d1bb8fbd05717330f4c84d524) | First in end.
`public unsigned long `[`tl`](#structclarinet_1a526c4d082ac6946df97fde764918a9eb) | Last in end.
`public unsigned long `[`count`](#structclarinet_1a7608dc0b4a5465db85cc6dce537fcb9d) | Stored size.
`public FILE * `[`writable`](#structclarinet_1a00d1f3552a2da9fe67c611952bca2957) | Plain writable "stream" of the queue so you can just #fwrite bytes to the queue.

## Members

#### `public char ** `[`buffer`](#structclarinet_1a76e6eefd69948c25d34fab86c6a23c51) 

Underlying buffer that [clarinet](#structclarinet) will free via [clarinet_free](#clarinet_8h_1ad548d76cca4928882344c65e33644636).

#### `public unsigned long `[`cap`](#structclarinet_1a7c5bdd62b0640ecccf0b1c0b82acd0d6) 

[buffer](#structclarinet_1a76e6eefd69948c25d34fab86c6a23c51) capacity, i.e. read/write at `BUFFER[x >= CAP]` is UB.

#### `public unsigned long `[`hd`](#structclarinet_1a2de8627d1bb8fbd05717330f4c84d524) 

First in end.

#### `public unsigned long `[`tl`](#structclarinet_1a526c4d082ac6946df97fde764918a9eb) 

Last in end.

#### `public unsigned long `[`count`](#structclarinet_1a7608dc0b4a5465db85cc6dce537fcb9d) 

Stored size.

#### `public FILE * `[`writable`](#structclarinet_1a00d1f3552a2da9fe67c611952bca2957) 

Plain writable "stream" of the queue so you can just #fwrite bytes to the queue.

# struct `clarinet_state` 

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public unsigned int `[`current_depth`](#structclarinet__state_1ad96f0c3c98d4b9b9fd2a666009fa8897) | 
`public unsigned int `[`depth`](#structclarinet__state_1a7ab05e2c694d7427b7d20c74959960ba) | 
`public struct `[`clarinet`](#structclarinet)` * `[`queue`](#structclarinet__state_1a432b056463e122921a196d3cdb76bb11) | 
`public char ** `[`keys`](#structclarinet__state_1ae70463c6e9d077069d1493a22c6b99ab) | 
`public size_t `[`keys_size`](#structclarinet__state_1a6077c0cd7c4789e4079358adc5cd8d9e) | 
`public size_t `[`keys_cap`](#structclarinet__state_1a92237d7b7f9a520d465635b31e59d073) | 
`public char * `[`key`](#structclarinet__state_1a8d9d5d60ef955b57a1cf959687a798f9) | 
`public yyjson_mut_doc * `[`doc_root`](#structclarinet__state_1a91aaa67cb3d12e9e1f0aa258a9d24b89) | 
`public yyjson_mut_val * `[`object`](#structclarinet__state_1aeab57e81f3db73d65a55be813f0f6011) | 
`public unsigned int `[`pp_flags`](#structclarinet__state_1a95c2364cac26d43f9a12e8228a07056f) | 

## Members

#### `public unsigned int `[`current_depth`](#structclarinet__state_1ad96f0c3c98d4b9b9fd2a666009fa8897) 

#### `public unsigned int `[`depth`](#structclarinet__state_1a7ab05e2c694d7427b7d20c74959960ba) 

#### `public struct `[`clarinet`](#structclarinet)` * `[`queue`](#structclarinet__state_1a432b056463e122921a196d3cdb76bb11) 

#### `public char ** `[`keys`](#structclarinet__state_1ae70463c6e9d077069d1493a22c6b99ab) 

#### `public size_t `[`keys_size`](#structclarinet__state_1a6077c0cd7c4789e4079358adc5cd8d9e) 

#### `public size_t `[`keys_cap`](#structclarinet__state_1a92237d7b7f9a520d465635b31e59d073) 

#### `public char * `[`key`](#structclarinet__state_1a8d9d5d60ef955b57a1cf959687a798f9) 

#### `public yyjson_mut_doc * `[`doc_root`](#structclarinet__state_1a91aaa67cb3d12e9e1f0aa258a9d24b89) 

#### `public yyjson_mut_val * `[`object`](#structclarinet__state_1aeab57e81f3db73d65a55be813f0f6011) 

#### `public unsigned int `[`pp_flags`](#structclarinet__state_1a95c2364cac26d43f9a12e8228a07056f) 

# struct `column_def` 

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public bool `[`is_hidden`](#structcolumn__def_1a90ea5c279470f93fd737a38950d18f34) | 
`public size_t `[`generated_always_as_size`](#structcolumn__def_1addf675aacfbfceeb0db19ec0f35d215d) | 
`public size_t * `[`generated_always_as_len`](#structcolumn__def_1ab1911858c9db62723b54da5663db3677) | 
`public char ** `[`generated_always_as`](#structcolumn__def_1ad2b18c82d13d6e8cbeff0c21d75373ac) | 
`public size_t `[`name_len`](#structcolumn__def_1a8b8a925c6315c63d2b66eabce7afd095) | 
`public char * `[`name`](#structcolumn__def_1aa7cbcc73c608674829da1002e3dfb4ee) | 
`public size_t `[`typename_len`](#structcolumn__def_1aabb6568ec5226094508e73d3c68bdf38) | 
`public char * `[`typename`](#structcolumn__def_1aca990c9adf35918d46b942ed012cee9e) | 

## Members

#### `public bool `[`is_hidden`](#structcolumn__def_1a90ea5c279470f93fd737a38950d18f34) 

#### `public size_t `[`generated_always_as_size`](#structcolumn__def_1addf675aacfbfceeb0db19ec0f35d215d) 

#### `public size_t * `[`generated_always_as_len`](#structcolumn__def_1ab1911858c9db62723b54da5663db3677) 

#### `public char ** `[`generated_always_as`](#structcolumn__def_1ad2b18c82d13d6e8cbeff0c21d75373ac) 

#### `public size_t `[`name_len`](#structcolumn__def_1a8b8a925c6315c63d2b66eabce7afd095) 

#### `public char * `[`name`](#structcolumn__def_1aa7cbcc73c608674829da1002e3dfb4ee) 

#### `public size_t `[`typename_len`](#structcolumn__def_1aabb6568ec5226094508e73d3c68bdf38) 

#### `public char * `[`typename`](#structcolumn__def_1aca990c9adf35918d46b942ed012cee9e) 

# struct `Fetch` 

The SQLite virtual table

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public sqlite3_vtab `[`base`](#structFetch_1a759c45b1ca23fb8a81dc4796515ff6f7) | For SQLite to fill in
`public `[`column_def`](#structcolumn__def)` ** `[`columns`](#structFetch_1a282e52231b4cf40a5fdc26b57080410c) | The columns for this row
`public unsigned long `[`columns_len`](#structFetch_1aca82d22e5e4f43e2b352aee0807e8f55) | Number of COLUMNS entries
`public size_t `[`default_url_len`](#structFetch_1a300bb1f81fdf7dd1af93c890b87b99cb) | 
`public char * `[`default_url`](#structFetch_1a41609e0f56080157d3f9008dd17ee0b5) | A default url if one was set
`public char * `[`schema`](#structFetch_1a6bc7f6c1092892c602b2c696c916d2bc) | Resolved schema string.

## Members

#### `public sqlite3_vtab `[`base`](#structFetch_1a759c45b1ca23fb8a81dc4796515ff6f7) 

For SQLite to fill in

#### `public `[`column_def`](#structcolumn__def)` ** `[`columns`](#structFetch_1a282e52231b4cf40a5fdc26b57080410c) 

The columns for this row

#### `public unsigned long `[`columns_len`](#structFetch_1aca82d22e5e4f43e2b352aee0807e8f55) 

Number of COLUMNS entries

#### `public size_t `[`default_url_len`](#structFetch_1a300bb1f81fdf7dd1af93c890b87b99cb) 

#### `public char * `[`default_url`](#structFetch_1a41609e0f56080157d3f9008dd17ee0b5) 

A default url if one was set

#### `public char * `[`schema`](#structFetch_1a6bc7f6c1092892c602b2c696c916d2bc) 

Resolved schema string.

# struct `fetch_cursor` 

Cursor.

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public sqlite3_vtab_cursor `[`base`](#structfetch__cursor_1aeb0d0d83a89b505da983e30d6c9bcaa7) | 
`public unsigned int `[`count`](#structfetch__cursor_1a7fcf3a2003b9c9c33ef4a7f057f273c7) | 
`public int `[`sockfd`](#structfetch__cursor_1a72eaa4da5c7cab18ded000d985d5be60) | 
`public int `[`eof`](#structfetch__cursor_1aa7e6b8ff64fe69418ee2632678353c38) | 
`public yyjson_doc * `[`next_doc`](#structfetch__cursor_1a894afb6a00cab3405d0b43ff70472f76) | 

## Members

#### `public sqlite3_vtab_cursor `[`base`](#structfetch__cursor_1aeb0d0d83a89b505da983e30d6c9bcaa7) 

#### `public unsigned int `[`count`](#structfetch__cursor_1a7fcf3a2003b9c9c33ef4a7f057f273c7) 

#### `public int `[`sockfd`](#structfetch__cursor_1a72eaa4da5c7cab18ded000d985d5be60) 

#### `public int `[`eof`](#structfetch__cursor_1aa7e6b8ff64fe69418ee2632678353c38) 

#### `public yyjson_doc * `[`next_doc`](#structfetch__cursor_1a894afb6a00cab3405d0b43ff70472f76) 

# struct `fetch_init` 

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public char * `[`headers`](#structfetch__init_1a498a85763e9ffe407cf1563056a6e94a) | 
`public char * `[`body`](#structfetch__init_1a62af633619d435aed048b4b027e61261) | 
`public int `[`stream_type`](#structfetch__init_1a41361d0e02fc28a6e56d82f5538b4264) | 

## Members

#### `public char * `[`headers`](#structfetch__init_1a498a85763e9ffe407cf1563056a6e94a) 

#### `public char * `[`body`](#structfetch__init_1a62af633619d435aed048b4b027e61261) 

#### `public int `[`stream_type`](#structfetch__init_1a41361d0e02fc28a6e56d82f5538b4264) 

# struct `fetch_state` 

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public int `[`netfd`](#structfetch__state_1a83e783aad903a02cdfa5821ef6f65a66) | 
`public int `[`outfd`](#structfetch__state_1aa98f64b1fc0afee9d8d849c843ad1933) | 
`public int `[`ep`](#structfetch__state_1a899f88580e1b56ad60998fb3e9a5f6cd) | 
`public char * `[`hostname`](#structfetch__state_1a40d70d16af2906ea064cedd5a622ae34) | 
`public bool `[`is_tls`](#structfetch__state_1aa90f80eace781757ac43a3377b2171d0) | 
`public SSL_CTX * `[`ssl_ctx`](#structfetch__state_1a66c045b9392139ff336f30cf0dc0279e) | 
`public SSL * `[`ssl`](#structfetch__state_1a1694080f733e79a0e3738716fd7d30ab) | 
`public bool `[`headers_done`](#structfetch__state_1ad53b18ed357181a01f823a57e32f316c) | 
`public char `[`header_buf`](#structfetch__state_1a3cf270b9a7e3f7d94376638cf4e7c2dd) | 
`public size_t `[`header_len`](#structfetch__state_1ab0661cc8ac1e18f2309f8745d42b0595) | 
`public bool `[`chunked_mode`](#structfetch__state_1a01a0b52cbc9dbb045df1f79a33147feb) | 
`public size_t `[`content_length`](#structfetch__state_1a1a64fb4efb03a9d0923068add83e0a10) | 
`public bool `[`reading_chunk_size`](#structfetch__state_1a142c75a498e442f993f69acd9163d03e) | 
`public char `[`chunk_line`](#structfetch__state_1a08fdf5c47aaf5f3c79424a1079e5d500) | 
`public size_t `[`chunk_line_len`](#structfetch__state_1a101c01eebb74988a374cd1f1d32b4d00) | 
`public size_t `[`current_chunk_size`](#structfetch__state_1a5fa41a601c7d05330b0a70159b2c4def) | 
`public int `[`expecting_crlf`](#structfetch__state_1a21ea78bd4b7ed4ca2b3efb4ef08567f2) | 
`public `[`clarinet_t`](#clarinet_8h_1a12c41bd5a4bab4e991105887145cc38a)` * `[`clare`](#structfetch__state_1ac9d8e5c6bbe8a00d986e48a625676f0a) | 
`public char * `[`pending_send`](#structfetch__state_1a303d0c37458f66fdad0eca600221930f) | 
`public size_t `[`pending_len`](#structfetch__state_1ab57b34644408172d1b89eeb55c8f2973) | 
`public size_t `[`pending_off`](#structfetch__state_1ab3851147d7716cf61200a8bd5bfe875c) | 
`public bool `[`http_done`](#structfetch__state_1a3e654b3b89e03a2c78d704562a49d224) | 
`public bool `[`closed_outfd`](#structfetch__state_1aad8ceb082129df0b2dd36a4f2695b475) | 

## Members

#### `public int `[`netfd`](#structfetch__state_1a83e783aad903a02cdfa5821ef6f65a66) 

#### `public int `[`outfd`](#structfetch__state_1aa98f64b1fc0afee9d8d849c843ad1933) 

#### `public int `[`ep`](#structfetch__state_1a899f88580e1b56ad60998fb3e9a5f6cd) 

#### `public char * `[`hostname`](#structfetch__state_1a40d70d16af2906ea064cedd5a622ae34) 

#### `public bool `[`is_tls`](#structfetch__state_1aa90f80eace781757ac43a3377b2171d0) 

#### `public SSL_CTX * `[`ssl_ctx`](#structfetch__state_1a66c045b9392139ff336f30cf0dc0279e) 

#### `public SSL * `[`ssl`](#structfetch__state_1a1694080f733e79a0e3738716fd7d30ab) 

#### `public bool `[`headers_done`](#structfetch__state_1ad53b18ed357181a01f823a57e32f316c) 

#### `public char `[`header_buf`](#structfetch__state_1a3cf270b9a7e3f7d94376638cf4e7c2dd) 

#### `public size_t `[`header_len`](#structfetch__state_1ab0661cc8ac1e18f2309f8745d42b0595) 

#### `public bool `[`chunked_mode`](#structfetch__state_1a01a0b52cbc9dbb045df1f79a33147feb) 

#### `public size_t `[`content_length`](#structfetch__state_1a1a64fb4efb03a9d0923068add83e0a10) 

#### `public bool `[`reading_chunk_size`](#structfetch__state_1a142c75a498e442f993f69acd9163d03e) 

#### `public char `[`chunk_line`](#structfetch__state_1a08fdf5c47aaf5f3c79424a1079e5d500) 

#### `public size_t `[`chunk_line_len`](#structfetch__state_1a101c01eebb74988a374cd1f1d32b4d00) 

#### `public size_t `[`current_chunk_size`](#structfetch__state_1a5fa41a601c7d05330b0a70159b2c4def) 

#### `public int `[`expecting_crlf`](#structfetch__state_1a21ea78bd4b7ed4ca2b3efb4ef08567f2) 

#### `public `[`clarinet_t`](#clarinet_8h_1a12c41bd5a4bab4e991105887145cc38a)` * `[`clare`](#structfetch__state_1ac9d8e5c6bbe8a00d986e48a625676f0a) 

#### `public char * `[`pending_send`](#structfetch__state_1a303d0c37458f66fdad0eca600221930f) 

#### `public size_t `[`pending_len`](#structfetch__state_1ab57b34644408172d1b89eeb55c8f2973) 

#### `public size_t `[`pending_off`](#structfetch__state_1ab3851147d7716cf61200a8bd5bfe875c) 

#### `public bool `[`http_done`](#structfetch__state_1a3e654b3b89e03a2c78d704562a49d224) 

#### `public bool `[`closed_outfd`](#structfetch__state_1aad8ceb082129df0b2dd36a4f2695b475) 

# struct `status_entry_t` 

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public int `[`status`](#structstatus__entry__t_1a48fe5c18682770e4a7c07d3062174524) | 
`public const char * `[`text`](#structstatus__entry__t_1ae645dcf5b9fc662846229d7b7c72d5c9) | 

## Members

#### `public int `[`status`](#structstatus__entry__t_1a48fe5c18682770e4a7c07d3062174524) 

#### `public const char * `[`text`](#structstatus__entry__t_1ae645dcf5b9fc662846229d7b7c72d5c9) 

# struct `url` 

Taken from Web API URL, word 4 word, bar 4 bar.

## Summary

 Members                        | Descriptions                                
--------------------------------|---------------------------------------------
`public buffer * `[`hostname`](#structurl_1a674ad2043889271cb7162ceabd59ffd7) | A string containing the domain of the URL.
`public buffer * `[`pathname`](#structurl_1a45930aefb1512c502f140005196eb87b) | A string containing an initial '/' followed by the path of the URL, not including the query string or fragment.
`public buffer * `[`port`](#structurl_1acea12e67b2115f7f3798a361482acd8f) | A string containing the port number of the URL.

## Members

#### `public buffer * `[`hostname`](#structurl_1a674ad2043889271cb7162ceabd59ffd7) 

A string containing the domain of the URL.

[https://developer.mozilla.org/en-US/docs/Web/API/URL](#)

#### `public buffer * `[`pathname`](#structurl_1a45930aefb1512c502f140005196eb87b) 

A string containing an initial '/' followed by the path of the URL, not including the query string or fragment.

[https://developer.mozilla.org/en-US/docs/Web/API/URL/pathname](#)

#### `public buffer * `[`port`](#structurl_1acea12e67b2115f7f3798a361482acd8f) 

A string containing the port number of the URL.

[https://developer.mozilla.org/en-US/docs/Web/API/URL/port](#)

Generated by [Moxygen](https://sourcey.com/moxygen)