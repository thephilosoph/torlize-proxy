# torlize

A small, educational SOCKS4 / SOCKS4a client written in C.  
It lets you open a TCP connection to any host:port **through Tor** (or any other SOCKS4/4a proxy).

## Features

- **SOCKS4a** support → hostnames work (no need to resolve them yourself)
- Classic SOCKS4 when you pass a literal IPv4 address
- Bidirectional stdin ↔ remote relay (netcat-style)
- Optional one-shot HTTP HEAD test mode (`-t`)
- Configurable proxy host/port and SOCKS userid
- Clear error messages for the common SOCKS reply codes
- Cross-platform socket handling (Linux primary, Windows via `#ifdef`)

## Requirements

- A running SOCKS4/4a proxy. Most common setups:
  - **Tor Browser** → `127.0.0.1:9150` (default)
  - System `tor` daemon → `127.0.0.1:9050`
- C compiler (gcc/clang) and `make`

## Build

```bash
make
```

This produces a `torlize` binary.

```bash
make debug    # debug symbols, no optimisation
make clean
```

### Windows (MinGW / MSYS2)

Just run:

```bash
mingw32-make
```

It will automatically produce `torlize.exe` and link with `-lws2_32`.

If that still fails, try:

```bash
mingw32-make CC=gcc
```
or open the "MinGW-w64" / "MSYS2 MinGW" terminal (so `gcc` is in PATH).

## Usage

```
torlize [options] <host> <port>
```

### Options

| Flag | Meaning |
|------|---------|
| `-H, --proxy-host HOST` | Proxy address (default `127.0.0.1`) |
| `-P, --proxy-port PORT` | Proxy port (default `9150`) |
| `-u, --user USERID`     | SOCKS userid (default `torlize`) |
| `-t, --test`            | After connect, send `HEAD / HTTP/1.0` and print the response |
| `-v, --verbose`         | Extra diagnostic output |
| `-h, --help`            | Show help |

### Default mode – interactive / pipeable tunnel

```bash
# Interactive
./torlize check.torproject.org 80
# then type:
# GET / HTTP/1.0
# Host: check.torproject.org
# <blank line>

# Or pipe
echo -e "GET / HTTP/1.0\r\nHost: check.torproject.org\r\n\r\n" \
  | ./torlize check.torproject.org 80
```

### Test mode

```bash
./torlize -t -v example.com 80
```

### Using the system Tor daemon (port 9050)

```bash
./torlize -P 9050 some.onion 80
```

## Example output (verbose + test)

```
[*] target      : check.torproject.org:80
[*] proxy       : 127.0.0.1:9150
[*] userid      : torlize
[*] mode        : test (HTTP HEAD)
[*] connecting to proxy...
[*] connected to proxy, performing SOCKS handshake...
[*] SOCKS request size: 42 bytes (SOCKS4a)
[*] SOCKS reply: VN=0 CD=90 (request granted)
[+] successfully connected through proxy to check.torproject.org:80
[*] sending test HTTP HEAD request...
HTTP/1.1 200 OK
...
```

## Notes / Limitations

- Only IPv4 is supported for the *proxy* address itself.
- Bidirectional relay is fully implemented on Unix-like systems. On Windows it currently just keeps the connection open.
- No SOCKS5 support yet (planned).
- The program does **not** start Tor for you – you must have a proxy already listening.

## License

MIT License – see the [LICENSE](LICENSE) file for details.
