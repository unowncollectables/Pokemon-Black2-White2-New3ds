# Wi-Fi Stage 0 link inventory

The Wi-Fi handoff tip `82f9898fd` is an ancestor of the frozen base
`84f67acf15`. The first slice replaces all seven public DWC download
entry points together with the fail-closed signature stub. Keep the
eight callable stubs in the game until the real link can replace them
as a set.

## Signature dependency

The real NitroDWC verifier builds for ARM11 using portable versions of
its six word-arithmetic helpers. The DS uses the original assembly.
`Crypt_VerifySignature` calls `MATH_CalcSHA1`, which reached an empty
`sha1_block` on the 3DS. The host now supplies the compression step.
An ARM11 probe using an independently generated RSA/SHA-1 fixture
accepted the original multi-block message and rejected both a modified
message and a modified signature.

When the verifier is selected, `Crypt_SetAllocator` must come from the
same object. Replace the generic allocator stub with that real symbol
at the same time. Keep the SDK SHA-1 wrapper and the host compression
step in the closure.

## Download closure

Both `nd_api.c` and `nd.c` compile for ARM11. Their dependencies expose
the next link and runtime seams:

- Allocator and session state. The two objects reference DWC alloc/free,
  Base64, error state, AP information, and SVL token acquisition and polling.
  The two SVL entry points in the current image are generic placeholders.
- Threads, queues and formatting. The download client starts an SVL
  worker, joins it, and sends messages. `OS_CreateThread` in the port
  currently records bookkeeping without starting a native worker.
- NitroHTTP. Download code needs body and header access, progress,
  cancellation, user parameters, certificate verification settings
  and the numeric conversion helpers. Some of the names already
  present in the ELF are generic placeholders. A successful static
  link alone cannot establish asynchronous download behavior.

The existing production build remains fail-closed. This branch prepares
the signature side and records the blockers to retiring the callable
stubs. Network transport and Mystery Gift delivery need subsequent tests.
