# 01-device

First step: Creating a device.

### Initial objects

Before we can do anything interesting with Vulkan, we need to get three objects:

1. `VkInstance` which is the initial interface to the Vulkan loader library.
2. `VkPhysicalDevice` which represents a GPU.
3. `VkDevice` which is a connection to a physical device.

### Loading the loader

The initial entry point is `vkGetInstanceProcAddr`, which we need to load in some OS-dependent way.
Either we can link to the Vulkan library at build time,
or we can load it dynamically at run time with
`LoadLibrary`/`GetProcAddress` (on Windows)
or `dlopen`/`dlsym` (on Linux).
Loading it at run time is a bit less convenient, but it gives us an opportunity
to present a nice error message to the user if they don't have any Vulkan drivers
installed.

`vkGetInstanceProcAddr` can give us instance-independent function pointers to:

* `vkEnumerateInstanceExtensionProperties`
* `vkEnumerateInstanceLayerProperties`
* `vkCreateInstance`

Those functions can give us a `VkInstance`.
We can pass that `VkInstance` back into `vkGetInstanceProcAddr` to get instance-specific function pointers to:

* `vkDestroyInstance`
* `vkEnumeratePhysicalDevices`
* `vkGetPhysicalDeviceProperties`
* `vkGetPhysicalDeviceQueueFamilyProperties`
* `vkCreateDevice`

Those functions can give us a `VkDevice`.

Once we've got a `VkDevice`, we have two options:

* Continue using instance-specific functions from `vkGetInstanceProcAddr` for all the rest of the Vulkan API.
* Use device-specific functions returned by `vkGetDeviceProcAddr` instead.

These are only valid for one `VkDevice`, but they might be a *tiny* bit more efficient
since they can skip the code that decides which device to dispatch each call to.

### Layers and extensions

TODO

### vkCreateInstance

Creating an instance is mostly straightforward.
We need to give it a list of the layers and extensions we want to enable,
plus a `VkApplicationInfo`.

The `VkApplicationInfo` is optional but quite useful.
You can use it to tell the driver about your application and engine - I assume
the intention is that if you're quite popular the driver developers might add
application-/engine-specific tweaks into their drivers to fix your bugs or
improve your performance.
With OpenGL they often did this by looking at the .exe's filename to figure out
which tweaks to apply; `VkApplicationInfo` makes it a little bit less yucky.

`VkApplicationInfo` also has an `apiVersion` field:

```cpp
applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 3);
```

We use `apiVersion` to say which version of the API our application is written to.
If the driver doesn't support that version, `vkCreateInstance` will fail with `VK_ERROR_INCOMPATIBLE_DRIVER`.
For example if we set `apiVersion` to 1.1.0, but the driver only knows about 1.0.3
(and will therefore be missing features that are required by 1.1.0), it will fail.

The application should specify the minimum API version that includes the features it needs.
In particular *don't* set it to `VK_API_VERSION` (defined by vulkan.h as the latest API version),
because your application may start failing when it's rebuilt with a newer version of vulkan.h
even if it doesn't use any of the new version's features.

(Vulkan's version numbering scheme is designed so that an application written for
1.0.4 will work on drivers written for 1.0.3 or for 1.0.9 - changes in the last ('patch') digit
do not break compatibility in either direction, because they won't add or remove features.
An application written for 1.1.x will work on drivers written for 1.2.x, but *not* on drivers
written for 1.0.x - changes in the second ('minor') digit might add new features that old
drivers won't support. And 2.x.x might be completely incompatible with 1.x.x.)

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

In our example code the `DebugAllocator` class tries to implement a cross-platform
aligned allocator. It uses unaligned `malloc` but with enough padding to let it
pick a correctly aligned pointer instead the returned buffer. It also stores a
small header just before the aligned pointer, containing the unaligned pointer and
the allocation size, so that it can handle `realloc` and `free` correctly.

We could have a persistent allocator object, but it's a little bit inconvenient
to have to pass that object around, so instead we use a stateless approach.
The trick is that when we pass a `VkAllocationCallbacks*` into a Vulkan API,
the driver will either use the callbacks inside that function call
or else it will make its own copy to call later.
C++ lets us create a temporary `VkAllocationCallbacks` object that will only
exist until the end of the current statement, which is just long enough for us
to safely pass a pointer to it into a Vulkan API call.

We need to be a bit careful about `VkAllocationCallbacks::pUserData`,
which must remain valid for its whole scope. E.g. the `pUserData` passed
into `vkCreateDevice` must remain valid until `vkDestroyDevice`.
We choose to set `pUserData` to a constant string literal (which will remain valid for
the lifetime of the application) that identifies the filename and line where the
allocator was created, so that `DebugAllocator` can report the location of
every allocation.

### VkPhysicalDevice

`vkEnumeratePhysicalDevices` gives us a list of all the Vulkan-capable physical devices
in the system.

If there are none, we can report an error. If there is one, we want to use it.

If there is more than one physical device, we have a non-trivial problem.

Often a user might have a fast discrete GPU, and a slow integrated GPU.
If we're a high-performance game, we probably want the discrete GPU.
But if we don't need top performance, maybe the integrated GPU will be fast enough
and consume less power, so laptop users would appreciate us using that one.
And I have one computer with a discrete GPU that's actually slower than
the integrated GPU (it's only there to provide dual-monitor support).

And the user might only have a monitor connected to one GPU. We'd probably
prefer to use that one, else it might be impossible or at least inefficient
to copy the framebuffer across to the right GPU for display.

Or they might have monitors connected to both GPUs.
And there might be multiple discrete GPUs, or any other combination.

So, how can we best handle that?

Um... I don't know. Let's just pick the first physical device in the list.
Maybe later we'll add a command-line option so the user can choose a different one.
(We need a better solution.)

### Queue families

TODO
