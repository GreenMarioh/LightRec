# Coding Rules

Language:

- C++20

Architecture:

- RAII everywhere
- Lock-free queues
- Multithreaded
- Zero-copy GPU paths
- Asynchronous encoding

Avoid:

- Frame copies
- CPU scaling
- Blocking operations
- Large allocations during gameplay

UI:

- Native WinUI 3 or Dear ImGui
- No browser UI

Performance:

- Idle RAM < 50 MB
- Idle CPU < 2%
