# AGENTS.md

## Project Overview

`velserv` is a small C daemon that bridges a Velbus serial interface to TCP.

- Main source: `velserv.c`
- Runtime modes:
  - Combined mode (default): internal TCP server + serial client bridge
  - `--server`: TCP fan-out server only
  - `--client`: serial-to-socket bridge only
- Default transport settings:
  - Serial device: `/dev/ttyACM0`
  - Bind/connect host: `127.0.0.1` in server mode
  - TCP port: `3788`

The code is intentionally single-binary, pthread-based, and has no external dependencies beyond libc/pthreads.

## Repository Layout

- `velserv.c`: core daemon implementation, CLI parsing, serial and socket threads
- `README.md`: usage, build notes, and systemd install guidance
- `velserv.service`: sample systemd unit
- `velserv_setup.sh` and `velserv-setup.sh`: installation helper scripts
- `velserv.sh`: simple run helper
- `Dockerfile`: containerized build/runtime baseline

## Build, Run, Verify

### Build

```bash
gcc -o velserv velserv.c -lpthread
```

### Quick sanity check

```bash
./velserv -h
./velserv -V
```

### Common runtime invocations

```bash
# Combined mode (daemon by default)
./velserv -d /dev/ttyACM0 -p 6000

# Foreground + verbosity for diagnostics
./velserv -f -v -d /dev/ttyACM0 -p 6000

# Server-only mode
./velserv -s -f -p 6000

# Client-only mode
./velserv -c -f -d /dev/ttyACM0 -a 127.0.0.1 -p 6000
```

## Agent Working Agreement

If you are editing this repository as an agent:

1. Keep changes small and scoped; do not refactor unrelated code.
2. Preserve existing CLI behavior and defaults unless explicitly requested.
3. Validate all behavior changes with a local build (`gcc ... -lpthread`).
4. Prefer adding targeted checks over broad rewrites.
5. If touching runtime behavior, document the user-visible impact in `README.md`.

## Code Notes and Constraints

- The program uses global state (`fd`, `sock`, flags) and long-lived threads.
- Serial port setup mixes `stty` shelling and termios configuration.
- Daemonization path closes stdio unless `-f` or `-v` is used.
- There is minimal error recovery; most hard failures call `exit(EXIT_FAILURE)`.
- No test suite exists; build-and-run smoke checks are the practical verification path.

## High-Risk Areas (Handle Carefully)

When changing these sections, assume regression risk is high:

- Frame parsing and boundary checks in `sock_to_com`, `com_to_sock`, and `server`
- Socket fan-out loop and client lifecycle management in `server`
- CLI parsing in `parse_params` (mode toggles and defaults)
- Serial configuration sequence (termios flags, RTS/DTR handling)
- Daemonization/foreground behavior in `main`

## Recommended Validation Checklist

After any non-trivial change:

1. Build cleanly with `gcc -o velserv velserv.c -lpthread`.
2. Confirm `./velserv -h` still prints expected options.
3. Run in foreground server mode (`-s -f`) and verify socket accept path.
4. If serial path changed, validate with real Velbus USB interface access.
5. If service behavior changed, re-check `velserv.service` assumptions.

## systemd and Deployment Notes

- Sample unit uses `Type=forking` and expects daemon behavior by default.
- Unit currently points to a specific `/dev/serial/by-id/...` path and port `6000`.
- Keep service and binary invocation flags in sync when changing defaults.

## Known Repository Facts (Current State)

- Git remote: `https://github.com/fredericdepuydt/velserv.git`
- Working tree may contain local untracked files (for example `Dockerfile`).
- Existing setup scripts install under `/opt/velserv` and enable systemd service.

## What Not To Do

- Do not introduce heavy build systems (CMake/autotools) unless asked.
- Do not change default protocol framing assumptions without explicit requirement.
- Do not silently alter serial defaults or service startup mode.
- Do not remove legacy scripts unless they are replaced and documented.
