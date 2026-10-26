# Official protocol sources

- [X.Org MIT-SHM](https://xorg.freedesktop.org/archive/current/doc/xextproto/shm.html): completion establishes when shared image bytes may be reused; it must be requested. Event serial describes the last request processed.
- [X.Org Xlib event predicates](https://www.x.org/releases/X11R7.5/doc/libX11/libX11.html): XCheckIfEvent removes only a matching event, preserving unmatched events. The predicate must not call Xlib or alter the queue.
- [X.Org queue ownership](https://xorg.freedesktop.org/archive/X11R7.5/doc/man/man3/XSetEventQueueOwner.3.html): Xlib owns shared-connection events by default; only the selected owner may consume them.
- [XCB checked requests](https://xcb.freedesktop.org/manual/group__XCB__Core__API.html): request_check handles checked errors and may issue an implicit sync. This does not replace SHM completion.

The implementation follows these contracts; they establish neither physical performance nor a global presentation timeout. Official GitLab/cgit raw source retrieval was unavailable, and no third-party source was used. Existing XShm declarations came from `/home/gama/sgx535-offline/glx-dependencies-20261022T023107Z/root/usr/include/X11/extensions/XShm.h`.
