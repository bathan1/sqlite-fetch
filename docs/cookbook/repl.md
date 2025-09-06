<script lang="ts" setup>
import REPL from "@/components/REPL.vue";
</script>

# REPL
Until the native build is (re)made, the only way to
have a REPL shell is to build one yourself. Thanks to
`async`/`await` ergonomics of modern JavaScript, we can handle
the "Read" and the "Evaluate" parts with just a single callback
function:

<<< ../components/REPL.vue#handleQuery {ts}

You just need to attach it to some input element, most likely
a `<button>`:

<<< ../components/REPL.vue#attach {vue}

Rendering the data values is just a loop over the columns
and rows...

<<< ../components/REPL.vue#render {vue}

And that's it! You can see the final result [below](#final-result),
along with the full source code of this page [here](../components/REPL.vue).

::: info
I barely know how to use Vue, so this should go to show that
setting up a *functional* user-driven query loop isn't too bad
if you're willing to write some boilerplate (something
AI chatbots are quite good at ;)).

Making it look nice... that's a completely separate matter.
:::

::: tip
Even if you *aren't* providing a REPL directly to your users,
I've always found it helpful to have one up even if it's just up
in local development, as it acts as a great sanity check.
:::

## Final Result
Here's the final result. You can edit the text to whatever you'd like.
The initial query value here is an example of extracting JSON data
from a sandbox clinical data server:

<REPL />
