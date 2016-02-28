A woefully incomplete attempt to explore and/or explain the Vulkan API step by step.

The aim is to be careful(/pedantic) about correctness, so the code should work
robustly on all conformant GPU drivers.

Currently that aim is probably not achieved.
I'm certainly not an expert and I'm learning as I go along,
so I might do things completely wrong.
Suggestions for corrections are welcome.

### Steps

Each directory has some code and documentation.
The vague plan is that each step will use and explain some new feature in a
very explicit way, and then the next step will try to abstract away the
details from the previous step and move on to some other new feature.

* 01-step: Creating a device.
* 02-???: TODO.

### Building

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 14 2015"
```

Currently only tested in VS2015, though it'd probably be worth supporting VS2013 too.
Intended to work on Linux, but not tested there yet.

### License

All code and documentation can be used under the MIT License.
