# Boson Scripts Directory

This directory contains automation, testing, and continuous integration scripts for the Boson Chess Engine.

## Scope & Purpose

- **Build Automation**: Multi-platform shell and PowerShell helper scripts to configure and compile Boson across MSVC, GCC, and Clang toolchains.
- **Verification & Testing**: Automated Perft run scripts, tactical benchmark batch runners, and regression detection suites.
- **Benchmarking**: Automated telemetry gathering scripts that run depth-bound searches across standard EPD suites and parse performance metrics (NPS, node counts, time-to-depth).
- **CI / CD Pipeline**: Continuous integration workflow hooks enforcing zero-warning compilation (`/W4 /WX`, `-Wall -Wextra -Werror`) and automated regression testing.
