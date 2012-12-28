
# libusbus

<https://bitbucket.org/liamstask/libusbus>

libusbus is a small, simple C library providing cross-platform access to vendor specific USB devices. If you're looking for direct access to your USB device's endpoints, you've come to the right place.

libusbus currently targets the following platforms:

* OS X: IOKit
* Windows: WinUSB

Would love some assistance on other *nix platforms.

# Building

I'm currently experimenting with using premake as a build tool - I've just been using it to generate Makefiles so far, but it's pretty handy. I'm using the 4.4beta-4 version found at <http://industriousone.com/premake/download>. Once you've got that you can generate the Makefiles and then build them like so:

    $ premake4 gmake
    $ make

On Windows, I've tested with MinGW but not with MSVC yet. MinGW-w64 is the only MinGW distribution I've seen that includes the winusb headers - <http://tdm-gcc.tdragon.net> makes it easy to install.

# Rationale

I was bitten one too many times by some of libusb's quirks and, after wading through the source a few times, decided I would rather start over with something much simpler. Much of the API is inspired by libusb, but I've tried to simplify where possible. A couple relevant design decisions:

* use a more permissive license - ensure that the library can easily be statically linked into applications for simplified deployment
* focus on device centric behavior - in my experience, relatively few applications require the more extensive device tree management that libusb provides (at some cost of complexity), so just leave it out
* on Windows, only support WinUSB and drop the other legacy libusb-win32 variants
* ensure hotplug (connect/disconnect) events are well supported
* no locking or other thread management - leave this to the application
* as few heap allocations as possible - keep it simple and efficient
* no isochronous transfer support (for now) - this is not supported by WinUSB yet, so leave it out until it is

# Status

The library is still new, so API changes are still possible and bugs are likely :)

Device enumeration, synchronous I/O, and async I/O are working well on IOKit and WinUSB.

The default event delivery mechanism is via callbacks - I would generally prefer to provide an event pump, but the IOKit APIs deliver events via callbacks, so it's a bit more direct to follow their lead.

Still mulling the best way to incorporate async events into an application's event loop. Right now usbusProcessEvents() must be called at regular intervals, which is not terrible but not as seamless as possible.

For IOKit, we can provide CFRunLoopSourceRefs for each event source. For WinUSB, we can provide HANDLEs to each device. Not sure yet whether this will be sufficient.

On Windows, a further complication is that connect/disconnect events are delivered via a window so they can't be integrated directly into the same event mechanism as I/O events. Still considering the best way to integrate these.
