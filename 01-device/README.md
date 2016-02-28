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
