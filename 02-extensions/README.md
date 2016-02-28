# 02-extensions

Extensions, layers, validation.

TODO: Explain stuff.

### Layers and extensions

TODO

### VkAllocationCallbacks

This is optional and usually unnecessary, but might be helpful in more complex applications.

`VkAllocationCallbacks` lets you intercept most of the memory allocations performed by the driver.
This is mainly useful for debugging and profiling: your application can keep track of exactly
where, when, and how much has been allocated.
It's not really intended for plugging in a higher-performance memory allocator;
instead it's intended for discovering where you've got too many allocations happening
so that you can eliminate them entirely (by precalculating and caching more,
moving work onto non-time-critical threads, etc).

The interface looks a bit like the standard `malloc`/`realloc`/`free`, except that
allocations can be required to have specific alignments.
Windows has `_aligned_malloc`/`_aligned_realloc`/`_aligned_free` which work okay for this,
but if you want to support Linux or Android then it becomes much trickier.

`aligned_alloc` and `posix_memalign` can return aligned memory,
but there's no equivalent aligned realloc function.
The standard `realloc` only guarantees alignment of maybe 8 bytes
(specifically the largest alignment needed for any standard C type).

In our example code the `AllocationCallbacksBase` class tries to implement a cross-platform
aligned allocator. It uses unaligned `malloc` but with enough padding to let it
pick a correctly aligned pointer instead the returned buffer. It also stores a
small header just before the aligned pointer, containing the unaligned pointer and
the allocation size, so that it can handle `realloc` and `free` correctly.

We could have a persistent allocator object, but it's a little bit inconvenient
to have to pass that object around, so instead we use a stateless approach.
The trick is that when we pass a `VkAllocationCallbacks*` into a Vulkan API,
the driver will either use the callbacks from inside that function call
or make its own copy to call later.
C++ lets us create a temporary `VkAllocationCallbacks` object that will only
exist until the end of the current statement, which is just long enough for us
to safely pass a pointer to it into a Vulkan API call.

We need to be a bit careful about `VkAllocationCallbacks::pUserData`:
it must remain valid for its whole scope, e.g. the `pUserData` passed
into `vkCreateDevice` must remain valid until `vkDestroyDevice`.
We choose to set `pUserData` to a constant string literal (which will remain valid for
the lifetime of the application) that identifies the filename and line where the
allocator was created, so that `DebugAllocationCallbacks` can report the source location
associated with every allocation.
