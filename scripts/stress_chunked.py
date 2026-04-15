#!/usr/bin/env python3
import argparse
import asyncio
import random
import statistics
import subprocess
import time
from collections import Counter


def build_chunked_body(payload: bytes) -> bytes:
    # One chunk + terminating chunk.
    # Chunk size is hex length of payload.
    return b"%X\r\n" % len(payload) + payload + b"\r\n0\r\n\r\n"


def build_request(host: str, port: int, path: str, payload: bytes) -> bytes:
    chunked = build_chunked_body(payload)
    headers = [
        f"POST {path} HTTP/1.1",
        f"Host: {host}:{port}",
        "User-Agent: stress_chunked.py",
        "Transfer-Encoding: chunked",
        "Connection: close",
        "\r\n",
    ]
    return "\r\n".join(headers).encode("ascii") + chunked


def detect_listen_pid(port: int) -> int | None:
    """Best-effort: find PID of the process listening on 127.0.0.1:<port> via `ss`."""
    try:
        out = subprocess.check_output(["ss", "-ltnp"], stderr=subprocess.DEVNULL).decode(
            "utf-8", errors="replace"
        )
    except Exception:
        return None

    for line in out.splitlines():
        if f":{port} " not in line and not line.rstrip().endswith(f":{port}"):
            continue
        if "users:(" not in line:
            continue
        # Example: users:(("webserv",pid=838440,fd=3))
        if "pid=" in line:
            try:
                pid_part = line.split("pid=", 1)[1]
                pid_prefix = pid_part.split(",", 1)[0]
                pid_str = "".join(ch for ch in pid_prefix if ch.isdigit())
                return int(pid_str) if pid_str else None
            except Exception:
                return None
    return None


def count_open_fds(pid: int) -> int | None:
    try:
        import os

        return len(os.listdir(f"/proc/{pid}/fd"))
    except Exception:
        return None


def count_tcp_state(port: int, state: str) -> int | None:
    """Count TCP sockets where local sport is `port` in a given ss state (established, syn-recv, time-wait, ...)."""
    try:
        out = subprocess.check_output(
            ["ss", "-tan", "state", state, f"( sport = :{port} )"],
            stderr=subprocess.DEVNULL,
        ).decode("utf-8", errors="replace")
    except Exception:
        return None

    # First line is header: State Recv-Q ...
    lines = [ln for ln in out.splitlines() if ln.strip()]
    if not lines:
        return 0
    return max(0, len(lines) - 1)


def _parse_headers(header_lines: list[bytes]) -> dict[str, str]:
    headers: dict[str, str] = {}
    for raw in header_lines:
        try:
            line = raw.decode("iso-8859-1", errors="replace").rstrip("\r\n")
        except Exception:
            continue
        if not line or ":" not in line:
            continue
        k, v = line.split(":", 1)
        headers[k.strip().lower()] = v.strip()
    return headers


async def _drain_http_body(
    reader: asyncio.StreamReader,
    headers: dict[str, str],
    status_code: int | None,
    io_timeout_s: float,
) -> None:
    # Drain body so the next response starts on a status line (important for keep-alive).
    if status_code is not None and (
        100 <= status_code < 200 or status_code in (204, 304)
    ):
        return

    te = headers.get("transfer-encoding", "").lower()
    if "chunked" in te:
        # Basic chunked decoder (discarding data)
        while True:
            line = await asyncio.wait_for(reader.readline(), timeout=io_timeout_s)
            if not line:
                return
            # Ignore chunk extensions after ';'
            size_str = line.split(b";", 1)[0].strip()
            try:
                size = int(size_str, 16)
            except Exception:
                return
            if size == 0:
                # Consume trailing CRLF after 0-size chunk and optional trailers.
                # There is a CRLF after the 0 line already; next comes trailers then blank line.
                while True:
                    trailer = await asyncio.wait_for(reader.readline(), timeout=io_timeout_s)
                    if not trailer or trailer in (b"\r\n", b"\n"):
                        return
                return
            # Read exactly chunk bytes + trailing CRLF
            remaining = size
            while remaining > 0:
                data = await asyncio.wait_for(reader.read(min(65536, remaining)), timeout=io_timeout_s)
                if not data:
                    return
                remaining -= len(data)
            # trailing CRLF
            await asyncio.wait_for(reader.readexactly(2), timeout=io_timeout_s)
        return

    cl = headers.get("content-length")
    if cl is not None:
        try:
            remaining = int(cl)
        except Exception:
            return
        while remaining > 0:
            data = await asyncio.wait_for(reader.read(min(65536, remaining)), timeout=io_timeout_s)
            if not data:
                return
            remaining -= len(data)
        return

    # If the server says it's closing, we can drain until EOF; otherwise we can't safely know body length.
    if headers.get("connection", "").lower() == "close":
        try:
            while True:
                data = await asyncio.wait_for(reader.read(65536), timeout=io_timeout_s)
                if not data:
                    return
        except Exception:
            return


async def one_request(
    req_id: int,
    host: str,
    port: int,
    path: str,
    payload: bytes,
    timeout_s: float,
    jitter_s: float,
    connect_only: bool,
    hold_s: float,
    sem: asyncio.Semaphore,
) -> tuple[str, float] | tuple[str, float]:
    await sem.acquire()
    try:
        if jitter_s:
            await asyncio.sleep(random.random() * jitter_s)

        start = time.perf_counter()
        try:
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(host, port), timeout=timeout_s
            )
        except Exception:
            return "CONNECT_FAIL", time.perf_counter() - start

        try:
            if connect_only:
                if hold_s > 0:
                    try:
                        await asyncio.wait_for(asyncio.sleep(hold_s), timeout=timeout_s + hold_s)
                    except Exception:
                        pass
                return "CONNECTED", time.perf_counter() - start

            request_bytes = build_request(host, port, path, payload)
            writer.write(request_bytes)
            await asyncio.wait_for(writer.drain(), timeout=timeout_s)

            # Read status line
            try:
                status_line = await asyncio.wait_for(reader.readline(), timeout=timeout_s)
            except Exception:
                return "NO_STATUS", time.perf_counter() - start

            # Example: HTTP/1.1 200 OK
            parts = status_line.split()
            code = "BAD_STATUS"
            if len(parts) >= 2:
                try:
                    code = parts[1].decode("ascii", errors="replace")
                except Exception:
                    code = "BAD_STATUS"

            # Drain rest quickly (don’t keep in memory)
            try:
                while True:
                    line = await asyncio.wait_for(reader.readline(), timeout=timeout_s)
                    if not line:
                        break
            except Exception:
                # ignore body drain timeouts
                pass

            return code, time.perf_counter() - start
        finally:
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass
    finally:
        sem.release()


async def slow_get_conn(
    conn_id: int,
    host: str,
    port: int,
    path: str,
    bytes_per_write: int,
    delay_s: float,
    requests_per_conn: int,
    read_response: bool,
    io_timeout_s: float,
) -> tuple[list[str], list[float]]:
    """Open one TCP connection and send one or more GET requests, trickling the header."""
    codes: list[str] = []
    sent_times: list[float] = []

    t0 = time.perf_counter()
    try:
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(host, port), timeout=io_timeout_s
        )
    except Exception:
        print(f"slow conn={conn_id} CONNECT_FAIL", flush=True)
        return ["CONNECT_FAIL"], []

    try:
        for req_i in range(requests_per_conn):
            req = (
                f"GET {path} HTTP/1.1\r\n"
                f"Host: {host}:{port}\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
            ).encode("ascii")

            for off in range(0, len(req), max(1, bytes_per_write)):
                chunk = req[off : off + max(1, bytes_per_write)]
                writer.write(chunk)
                try:
                    await asyncio.wait_for(writer.drain(), timeout=io_timeout_s)
                except Exception:
                    print(f"slow conn={conn_id} req={req_i} DRAIN_FAIL", flush=True)
                    codes.append("DRAIN_FAIL")
                    return codes, sent_times
                if delay_s > 0:
                    await asyncio.sleep(delay_s)

            dt = time.perf_counter() - t0
            sent_times.append(dt)
            print(f"slow conn={conn_id} req={req_i} header_sent t={dt:.3f}s", flush=True)

            if read_response:
                try:
                    status_line = await asyncio.wait_for(
                        reader.readline(), timeout=io_timeout_s
                    )
                    parts = status_line.split()
                    code = "NO_STATUS"
                    code_int: int | None = None
                    if len(parts) >= 2:
                        code = parts[1].decode("ascii", errors="replace")
                        try:
                            code_int = int(code)
                        except Exception:
                            code_int = None
                    codes.append(code)

                    # Drain headers
                    raw_headers: list[bytes] = []
                    while True:
                        line = await asyncio.wait_for(
                            reader.readline(), timeout=io_timeout_s
                        )
                        if not line or line in (b"\r\n", b"\n"):
                            break
                        raw_headers.append(line)

                    headers = _parse_headers(raw_headers)
                    await _drain_http_body(reader, headers, code_int, io_timeout_s)
                except Exception:
                    codes.append("READ_FAIL")
            else:
                codes.append("SENT")

        return codes, sent_times
    finally:
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass


async def main_async(args: argparse.Namespace) -> int:
    if args.slow_get:
        t0 = time.perf_counter()
        tasks = [
            asyncio.create_task(
                slow_get_conn(
                    i,
                    args.host,
                    args.port,
                    args.probe_path if args.path == "/Upload" else args.path,
                    args.bytes_per_write,
                    args.slow_delay,
                    args.requests_per_conn,
                    not args.no_read,
                    args.timeout,
                )
            )
            for i in range(args.connections)
        ]

        results = await asyncio.gather(*tasks)
        wall = time.perf_counter() - t0

        all_codes: list[str] = []
        all_sent: list[float] = []
        for codes, sent_times in results:
            all_codes.extend(codes)
            all_sent.extend(sent_times)

        codes_ctr = Counter(all_codes)
        print(f"target=http://{args.host}:{args.port}{args.path}")
        print(
            f"mode=slow-get connections={args.connections} requests_per_conn={args.requests_per_conn} bytes_per_write={args.bytes_per_write} delay={args.slow_delay}s"
        )
        print(f"wall_time={wall:.3f}s")
        print("codes:")
        for code, count in sorted(codes_ctr.items(), key=lambda kv: (-kv[1], kv[0])):
            print(f"  {code}: {count}")

        if all_sent:
            print(
                f"header_sent_timing_seconds: min={min(all_sent):.4f} p50={sorted(all_sent)[len(all_sent)//2]:.4f} max={max(all_sent):.4f}"
            )
        return 0

    payload = args.payload.encode("utf-8")

    sem = asyncio.Semaphore(args.concurrency)

    stop_sampler = asyncio.Event()

    async def sampler() -> None:
        pid = detect_listen_pid(args.port)
        t0 = time.perf_counter()
        if pid is None:
            print("sampler: pid=UNKNOWN")
        else:
            print(f"sampler: pid={pid}")

        while not stop_sampler.is_set():
            await asyncio.sleep(args.sample)
            dt = time.perf_counter() - t0
            if pid is not None:
                fd_count = count_open_fds(pid)
            else:
                fd_count = None
            established = count_tcp_state(args.port, "established")
            synrecv = count_tcp_state(args.port, "syn-recv")
            timewait = count_tcp_state(args.port, "time-wait")

            fd_str = str(fd_count) if fd_count is not None else "?"
            est_str = str(established) if established is not None else "?"
            syn_str = str(synrecv) if synrecv is not None else "?"
            tw_str = str(timewait) if timewait is not None else "?"
            print(
                f"sample t={dt:6.2f}s server_fd_count={fd_str} established={est_str} synrecv={syn_str} timewait={tw_str}",
                flush=True,
            )

    async def prober() -> None:
        t0 = time.perf_counter()
        while not stop_sampler.is_set():
            await asyncio.sleep(args.probe_interval)
            dt = time.perf_counter() - t0
            try:
                reader, writer = await asyncio.wait_for(
                    asyncio.open_connection(args.host, args.port), timeout=args.timeout
                )
            except Exception:
                print(f"probe t={dt:6.2f}s CONNECT_FAIL", flush=True)
                continue

            try:
                req = (
                    f"GET {args.probe_path} HTTP/1.1\r\n"
                    f"Host: {args.host}:{args.port}\r\n"
                    "Connection: close\r\n\r\n"
                ).encode("ascii")
                writer.write(req)
                await asyncio.wait_for(writer.drain(), timeout=args.timeout)
                status_line = await asyncio.wait_for(reader.readline(), timeout=args.timeout)
                parts = status_line.split()
                code = "BAD_STATUS"
                if len(parts) >= 2:
                    try:
                        code = parts[1].decode("ascii", errors="replace")
                    except Exception:
                        code = "BAD_STATUS"
                print(f"probe t={dt:6.2f}s http_code={code}", flush=True)
            except Exception:
                print(f"probe t={dt:6.2f}s IO_FAIL", flush=True)
            finally:
                try:
                    writer.close()
                    await writer.wait_closed()
                except Exception:
                    pass
    tasks = [
        asyncio.create_task(
            one_request(
                i,
                args.host,
                args.port,
                args.path,
                payload,
                args.timeout,
                args.jitter,
                args.connect_only,
                args.hold,
                sem,
            )
        )
        for i in range(args.requests)
    ]

    sampler_task: asyncio.Task[None] | None = None
    prober_task: asyncio.Task[None] | None = None
    if args.sample > 0 and args.connect_only:
        sampler_task = asyncio.create_task(sampler())
    if args.connect_only and args.probe:
        prober_task = asyncio.create_task(prober())

    t0 = time.perf_counter()
    results = await asyncio.gather(*tasks)
    wall = time.perf_counter() - t0

    stop_sampler.set()
    if sampler_task is not None:
        try:
            await asyncio.wait_for(sampler_task, timeout=1.0)
        except Exception:
            sampler_task.cancel()
    if prober_task is not None:
        try:
            await asyncio.wait_for(prober_task, timeout=1.0)
        except Exception:
            prober_task.cancel()

    codes = Counter(code for code, _ in results)
    times = [t for _, t in results]

    ok = sum(codes[c] for c in codes if c.isdigit() and c.startswith("2"))
    total = len(results)

    def pct(p: float) -> float:
        if not times:
            return 0.0
        idx = int(round((p / 100.0) * (len(times) - 1)))
        return sorted(times)[idx]

    print(f"target=http://{args.host}:{args.port}{args.path}")
    mode = "connect-only" if args.connect_only else "chunked-post"
    extra = f" hold={args.hold}s" if args.connect_only and args.hold else ""
    print(f"mode={mode}{extra}")
    print(f"requests={args.requests} concurrency={args.concurrency} timeout={args.timeout}s jitter={args.jitter}s")
    print(f"wall_time={wall:.3f}s rps={(total / wall) if wall > 0 else 0:.1f}")
    print(f"2xx={ok}/{total}")

    print("status_codes:")
    for code, count in sorted(codes.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"  {code}: {count}")

    if times:
        print("latency_seconds:")
        print(f"  min={min(times):.4f} avg={statistics.fmean(times):.4f} p50={pct(50):.4f} p90={pct(90):.4f} p99={pct(99):.4f} max={max(times):.4f}")

    return 0


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Concurrent chunked POST stress tester for 42 webserv"
    )
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8080)
    p.add_argument("--path", default="/Upload")
    p.add_argument("-n", "--requests", type=int, default=1025, help="Total requests")
    p.add_argument(
        "-c", "--concurrency", type=int, default=1025, help="Concurrent connections"
    )
    p.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="Per-request timeout (connect + IO)",
    )
    p.add_argument(
        "--jitter",
        type=float,
        default=0.0,
        help="Random delay (0..jitter) seconds before each request to de-sync start",
    )
    p.add_argument(
        "--payload",
        default="HelloWorld!",
        help="Payload for the single chunk (chunk size computed automatically)",
    )

    p.add_argument(
        "--slow-get",
        action="store_true",
        help="Open multiple connections and send GET request headers 1 byte at a time (slowloris-style)",
    )
    p.add_argument(
        "--connections",
        type=int,
        default=10,
        help="Number of simultaneous TCP connections (slow-get mode)",
    )
    p.add_argument(
        "--requests-per-conn",
        type=int,
        default=1,
        help="How many GET requests to send per connection (slow-get mode)",
    )
    p.add_argument(
        "--bytes-per-write",
        type=int,
        default=1,
        help="Bytes written per socket write() call (slow-get mode; default 1)",
    )
    p.add_argument(
        "--slow-delay",
        type=float,
        default=0.05,
        help="Delay between small writes in seconds (slow-get mode)",
    )
    p.add_argument(
        "--no-read",
        action="store_true",
        help="Do not read the HTTP response (slow-get mode)",
    )
    p.add_argument(
        "--connect-only",
        action="store_true",
        help="Only open TCP connections (no HTTP); useful to test select()/FD_SETSIZE limits",
    )
    p.add_argument(
        "--hold",
        type=float,
        default=0.0,
        help="Seconds to hold the TCP connection open (only with --connect-only)",
    )
    p.add_argument(
        "--sample",
        type=float,
        default=0.0,
        help="If >0 and using --connect-only, print periodic server FD/TCP-state samples every N seconds",
    )
    p.add_argument(
        "--probe",
        action="store_true",
        help="If set with --connect-only, periodically send a GET request during the hold period to see if the server stays responsive",
    )
    p.add_argument(
        "--probe-path",
        default="/",
        help="Path used for the responsiveness probe GET (default: /)",
    )
    p.add_argument(
        "--probe-interval",
        type=float,
        default=1.0,
        help="Seconds between probe GETs (default: 1.0)",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()
    try:
        return asyncio.run(main_async(args))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
