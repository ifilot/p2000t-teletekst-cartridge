# HTTP recovery regression tests

Run from the repository root with a checked-out Pico SDK:

```sh
PICO_SDK_PATH=/path/to/pico-sdk make -C firmware/tests http-test
```

The test includes the production Pico entry point to exercise its private
fetch handlers without exposing a test API. It compiles the SDK's actual lwIP
HTTP client, ALTCP dispatch, allocation and pbuf code, and the firmware's page
decoder. Only Pico hardware, DNS delivery, TCP events and TLS setup are host
adapters. TLS tests check configuration ownership, not encryption or certificate
validation. Unused radio and mailbox code is removed by the linker.

The tests cover correcting port 8082 to 8080 after a connection timeout,
connection refusal or synchronous startup failure, and retrying after DNS
timeout or failure. Expired DNS replies are delivered while the new request is
resolving, connecting and complete. Every successful retry passes an HTTP 200
response through the real HTTP parser and page decoder. HTTP and HTTPS cases
check connection/configuration cleanup and retain the original Host header.

They also cover exclusive version-check ownership through deferred TLS cleanup,
including disconnected, allocation and synchronous startup failures; URL and
auto-start save failures, busy reads and explicit retries; and Dutch clock
transitions in leap and non-leap years using independent UTC fixtures.

AddressSanitizer and UndefinedBehaviorSanitizer detect invalid memory access.
In environments where LeakSanitizer cannot inspect the process, run with
`ASAN_OPTIONS=detect_leaks=0`; the tests also count TCP and TLS allocations.
The firmware CI job runs these tests against its pinned Pico SDK.
