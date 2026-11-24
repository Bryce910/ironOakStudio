# Jolt Physics

Valwood's custom runtime uses the CPU Jolt Physics component, pinned to
[v5.6.0](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0).
Copyright 2021 Jorrit Rouwe. Licensed under the MIT License; the complete
license is retained at `third_party/jolt/LICENSE` and included in built packages.

Only the Jolt library and build source directories were vendored. Vendor game
sample assets were not imported. The downloaded archive identity is recorded
in `third_party/jolt/VALWOOD_SOURCE.json`.

The Valwood build uses double-precision world positions, an SSE2 baseline on
x86, and CPU simulation. Vendor GPU compute, sample executables, installation,
debug rendering, and profiler builds are disabled. These settings describe the
component build; they do not certify scene integration or measured performance.
