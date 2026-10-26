# Primary-source protocol basis

- [XCB Core API](https://xcb.freedesktop.org/manual/group__XCB__Core__API.html): xcb_request_check consumes the checked request's error or waits until subsequent sequence progress rules it out; it can issue a sync when necessary. Inference: waiting for a final checked FreeGC before consuming preceding image cookies can share their barrier without discardingerrors.
- [XCB Protocol Stub API](https://xcb.freedesktop.org/ProtocolStubApi/): checked void-request errors remain for request_check, while reply/error storage must be consumed/freed. This candidate continues using checked requests and drainsbothmandatoryqueryreplies onfailure.
- [X.Org communication guide](https://www.x.org/guide/communication/): each client's requests are processed in order received. Inference: FreeGC queued after PutImage requests cannot precede their use ofthatGC; no persistentGC is introduced.

Sources consulted2026-10-26. No quotes or third-party performance claims used. Protocol reasoning doesnot establish physical Atom throughput; tests and later Mini measurement remain necessary.
