#!/usr/bin/env python3
"""Focused regressions for retiring a socket while parsing one input chunk."""

import argparse
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import threading
import zlib

TIMEOUT = 15
ANSI_ESCAPE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


class ScriptedServer:
    def __init__(self, payloads):
        self.payloads = payloads
        self.connections = []
        self.errors = []
        self.active = None
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(len(payloads))
        self.listener.settimeout(TIMEOUT)
        self.port = self.listener.getsockname()[1]
        self.thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, exc_type, exc, traceback):
        if exc_type is not None:
            self.cancel()
        self.thread.join(2)
        if self.thread.is_alive():
            self.cancel()
            self.thread.join(2)
        if exc_type is None:
            if self.thread.is_alive():
                raise AssertionError("loopback server did not terminate")
            if self.errors:
                raise AssertionError(f"loopback server failed: {self.errors}")
        return False

    def cancel(self):
        for sock in (self.active, self.listener):
            if sock is None:
                continue
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass

    def _serve(self):
        try:
            for payload in self.payloads:
                conn, _ = self.listener.accept()
                conn.settimeout(TIMEOUT)
                self.active = conn
                received = bytearray()
                with conn:
                    if isinstance(payload, tuple):
                        marker, payload = payload
                        while marker not in received:
                            data = conn.recv(4096)
                            if not data:
                                raise AssertionError(
                                    f"connection closed before {marker!r}"
                                )
                            received.extend(data)
                    if payload:
                        conn.sendall(payload)
                    while True:
                        try:
                            data = conn.recv(4096)
                        except ConnectionResetError:
                            break
                        if not data:
                            break
                        received.extend(data)
                self.active = None
                self.connections.append(bytes(received))
        except Exception as exc:
            self.errors.append(repr(exc))
        finally:
            self.active = None
            self.listener.close()


def _run_tf_visual(args, env):
    master, slave = os.openpty()
    output = []
    proc = subprocess.Popen(
        args, stdin=slave, stdout=slave, stderr=slave, env=env
    )
    os.close(slave)

    def read_output():
        while True:
            try:
                data = os.read(master, 4096)
            except OSError:
                break
            if not data:
                break
            output.append(data)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    timeout_error = None
    try:
        proc.wait(timeout=TIMEOUT)
    except subprocess.TimeoutExpired as exc:
        timeout_error = exc
        proc.kill()
        proc.wait()
    reader.join(2)
    os.close(master)
    reader.join(2)
    stdout = b"".join(output).decode("utf-8", errors="replace")
    if timeout_error:
        raise AssertionError(f"tf timed out\noutput:\n{stdout}") from timeout_error
    return subprocess.CompletedProcess(args, proc.returncode, stdout, "")


def run_tf(binary, script, visual=False):
    with tempfile.TemporaryDirectory(prefix="tf-socket-input-") as temp:
        root = Path(temp)
        libdir = root / "lib"
        libdir.mkdir()
        (libdir / "stdlib.tf").write_text("", encoding="utf-8")
        config = root / "test.tf"
        config.write_text(script, encoding="utf-8")
        suppressions = root / "lsan.supp"
        suppressions.write_text(
            "leak:ch_visual\nleak:hwrite\nleak:init_ssl\nleak:redraw_window\n",
            encoding="ascii",
        )
        env = os.environ.copy()
        env.update({"LC_ALL": "C.UTF-8", "TERM": "xterm"})
        options = env.get("LSAN_OPTIONS", "")
        env["LSAN_OPTIONS"] = (
            options + (":" if options else "") + f"suppressions={suppressions}"
        )
        args = [str(binary), f"-L{libdir}", f"-f{config}", "-n"]
        if visual:
            proc = _run_tf_visual(args, env)
        else:
            args.append("-v")
            try:
                proc = subprocess.run(
                    args,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    timeout=TIMEOUT,
                    encoding="utf-8",
                    errors="replace",
                    env=env,
                )
            except subprocess.TimeoutExpired as exc:
                raise AssertionError(
                    f"tf timed out\nstdout:\n{exc.stdout}\nstderr:\n{exc.stderr}"
                ) from exc
    diagnostics = proc.stdout + "\n" + proc.stderr
    if proc.returncode or "ERROR: AddressSanitizer:" in diagnostics or \
            "runtime error:" in diagnostics or "ERROR: LeakSanitizer:" in diagnostics:
        raise AssertionError(
            f"tf exited {proc.returncode}\nstdout:\n{proc.stdout}"
            f"\nstderr:\n{proc.stderr}"
        )
    return proc


def output_lines(output):
    return ANSI_ESCAPE.sub("", output).splitlines()


def assert_line(output, text):
    if output_lines(output).count(text) != 1:
        raise AssertionError(f"expected one {text!r} line:\n{output}")


def assert_no_stale_output(output, text):
    if text in output_lines(output):
        raise AssertionError(f"stale input {text!r} was processed:\n{output}")


def prompt_clears_foreground(binary):
    with ScriptedServer([b"PROMPT\xff\xf9"]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/set telopt=on
/test addworld("prompt", "", "127.0.0.1", "{server.port}")
/def -1 -hPROMPT retire = /test echo("SAW_PROMPT")%; /repeat -0 1 /quit -y%; /fg -n
/connect -b prompt
''')
    assert_line(proc.stdout, "SAW_PROMPT")


def queued_disconnect(binary):
    with ScriptedServer([b""]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/test addworld("queued", "", "127.0.0.1", "{server.port}")
/def -1 -wqueued -mglob -t"QUEUED_OLD" retire = /test echo("SAW_QUEUED_OLD")%; /repeat -0 1 /quit -y%; /dc queued
/def -wqueued -mglob -t"QUEUED_STALE" stale = /test echo("SAW_QUEUED_STALE")
/connect -b queued
/test fake_recv(strcat("QUEUED_OLD", char(10), "QUEUED_STALE", char(10)), "queued", "r")
''')
    assert_line(proc.stdout, "SAW_QUEUED_OLD")
    assert_no_stale_output(proc.stdout, "SAW_QUEUED_STALE")


def queued_before_close(binary):
    with ScriptedServer([b""]) as world_a, ScriptedServer([b""]) as world_b:
        proc = run_tf(binary, f'''/set connect=blocking
/test addworld("A", "", "127.0.0.1", "{world_a.port}")
/test addworld("B", "", "127.0.0.1", "{world_b.port}")
/def -1 -wA -mglob -t"OUTER" outer = /test echo("SAW_OUTER")%; /test fake_recv("B_VALID", "B")%; /dc B%; /repeat -1 1 /quit -y
/def -1 -wB -mglob -t"B_VALID" seen = /test echo("SAW_B_VALID")%; /quit -y
/connect -b B
/connect -f A
/test fake_recv("OUTER", "A")
''')
    assert_line(proc.stdout, "SAW_OUTER")
    assert_line(proc.stdout, "SAW_B_VALID")



def queued_prompt_before_close(binary):
    with ScriptedServer([b""]) as world_a, ScriptedServer([b""]) as world_b:
        proc = run_tf(binary, f'''/set connect=blocking
/test addworld("A", "", "127.0.0.1", "{world_a.port}")
/test addworld("B", "", "127.0.0.1", "{world_b.port}")
/def -i quit_later = /fg B%; /repeat -1 1 /quit -y
/def -1 -wA -mglob -t"OUTER" outer = /fg B%; /test prompt("B_PROMPT")%; /dc B%; /repeat -0 1 /quit_later
/connect -b B
/connect -f A
/test fake_recv("OUTER", "A")
''', visual=True)
    if proc.stdout.count("B_PROMPT") != 1:
        raise AssertionError(f"queued prompt was not displayed:\n{proc.stdout}")

def queued_reconnect(binary):
    with ScriptedServer([b"", b"QUEUED_NEW\n"]) as queued, \
            ScriptedServer([b""]) as foreground:
        proc = run_tf(binary, f'''/set connect=blocking
/test addworld("queued", "", "127.0.0.1", "{queued.port}")
/test addworld("foreground", "", "127.0.0.1", "{foreground.port}")
/def -1 -wqueued -mglob -t"QUEUED_OLD" retire = /test echo("SAW_QUEUED_OLD")%; /dc queued%; /connect -b queued
/def -wqueued -mglob -t"QUEUED_STALE" stale = /test echo("SAW_QUEUED_STALE")
/def -1 -wqueued -mglob -t"QUEUED_NEW" replacement = /test echo("SAW_QUEUED_NEW")%; /quit -y
/connect -b queued
/connect -f foreground
/test fake_recv("QUEUED_OLD", "queued")
/test fake_recv("QUEUED_STALE", "queued")
/dc queued
/test send("AFTER_DC")
''')
    assert_line(proc.stdout, "SAW_QUEUED_OLD")
    assert_line(proc.stdout, "SAW_QUEUED_NEW")
    assert_no_stale_output(proc.stdout, "SAW_QUEUED_STALE")
    if foreground.connections != [b"AFTER_DC\r\n"]:
        raise AssertionError(
            f"post-/dc send reached wrong socket: {foreground.connections!r}, "
            f"{queued.connections!r}"
        )
    if "Internal error" in proc.stdout + proc.stderr:
        raise AssertionError(f"queue accounting failed:\n{proc.stdout}{proc.stderr}")


def queued_reincarnation_before_close(binary):
    with ScriptedServer([b"", b""]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/test addworld("A", "", "127.0.0.1", "{server.port}")
/def -1 -wA -mglob -t"OLD" retire = /test echo("SAW_OLD")%; /dc A%; /connect -b A%; /test fake_recv("NEW", "A")%; /dc A%; /repeat -1 1 /quit -y
/def -1 -wA -mglob -t"NEW" seen = /test echo("SAW_NEW")%; /quit -y
/connect -b A
/test fake_recv("OLD", "A")
''')
    assert_line(proc.stdout, "SAW_OLD")
    assert_line(proc.stdout, "SAW_NEW")


def telnet_callback_disconnect(binary):
    with ScriptedServer([(b"READY", b"\xff\xf1TELNET_CALLBACK_STALE")]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/set telopt=on
/test addworld("diagnostic", "", "127.0.0.1", "{server.port}")
/connect -b diagnostic
/def -1 -hBGTEXT retire = /test echo("SAW_TELNET_CALLBACK")%; /repeat -0 1 /quit -y%; /dc diagnostic
/test send("READY", "diagnostic", "f")
''')
    assert_line(proc.stdout, "SAW_TELNET_CALLBACK")
    assert_no_stale_output(proc.stdout, "TELNET_CALLBACK_STALE")


def telnet_disconnect(binary):
    payload = (
        b"\xff\xfd\x18"
        b"DISCONNECT_OLD\n"
        b"\xff\xfa\x18\x01\xff\xf0"
        b"DISCONNECT_STALE\n"
    )
    with ScriptedServer([payload]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/set telopt=on
/test addworld("disconnect", "", "127.0.0.1", "{server.port}")
/def -1 -wdisconnect -mglob -t"DISCONNECT_OLD" retire = /test echo("SAW_DISCONNECT_OLD")%; /repeat -0 1 /quit -y%; /dc disconnect
/def -wdisconnect -mglob -t"DISCONNECT_STALE" stale = /test echo("SAW_DISCONNECT_STALE")
/connect -b disconnect
''')
    assert_line(proc.stdout, "SAW_DISCONNECT_OLD")
    assert_no_stale_output(proc.stdout, "SAW_DISCONNECT_STALE")


def telnet_reconnect(binary):
    payload = (
        b"\xff\xfd\x18"
        b"TELNET_OLD\n"
        b"\xff\xfa\x18\x01\xff\xf0"
        b"TELNET_STALE\n"
    )
    with ScriptedServer([payload, b"TELNET_NEW\n"]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/set telopt=on
/test addworld("telnet", "", "127.0.0.1", "{server.port}")
/def -i reconnect_telnet = /connect -b telnet
/def -1 -wtelnet -mglob -t"TELNET_OLD" retire = /test echo("SAW_TELNET_OLD")%; /repeat -0 1 /reconnect_telnet%; /dc telnet
/def -wtelnet -mglob -t"TELNET_STALE" stale = /test echo("SAW_TELNET_STALE")
/def -1 -wtelnet -mglob -t"TELNET_NEW" replacement = /test echo("SAW_TELNET_NEW")%; /quit -y
/connect -b telnet
''')
    assert_line(proc.stdout, "SAW_TELNET_OLD")
    assert_line(proc.stdout, "SAW_TELNET_NEW")
    assert_no_stale_output(proc.stdout, "SAW_TELNET_STALE")


def mccp_reconnect(binary):
    compressed = zlib.compress(
        b"MCCP_OLD\n\xff\xfa\x18\x01\xff\xf0MCCP_STALE\n"
    )
    payload = b"\xff\xfb\x56\xff\xfa\x56\xff\xf0" + compressed
    with ScriptedServer([payload, b"MCCP_NEW\n"]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/set telopt=on
/test addworld("mccp", "", "127.0.0.1", "{server.port}")
/def -i reconnect_mccp = /connect -b mccp
/def -1 -wmccp -mglob -t"MCCP_OLD" retire = /test echo("SAW_MCCP_OLD")%; /repeat -0 1 /reconnect_mccp%; /dc mccp
/def -wmccp -mglob -t"MCCP_STALE" stale = /test echo("SAW_MCCP_STALE")
/def -1 -wmccp -mglob -t"MCCP_NEW" replacement = /test echo("SAW_MCCP_NEW")%; /quit -y
/connect -b mccp
''')
    assert_line(proc.stdout, "SAW_MCCP_OLD")
    assert_line(proc.stdout, "SAW_MCCP_NEW")
    assert_no_stale_output(proc.stdout, "SAW_MCCP_STALE")
    if len(server.connections) != 2 or b"\xff\xfd\x56" not in \
            server.connections[0] or server.connections[1]:
        raise AssertionError(f"unexpected MCCP transport data: {server.connections!r}")


def normal_input(binary):
    with ScriptedServer([b"NORMAL_INPUT\n"]) as server:
        proc = run_tf(binary, f'''/set connect=blocking
/test addworld("normal", "", "127.0.0.1", "{server.port}")
/def -1 -wnormal -mglob -t"NORMAL_INPUT" seen = /test echo("SAW_NORMAL_INPUT")%; /quit -y
/connect -b normal
''')
    assert_line(proc.stdout, "SAW_NORMAL_INPUT")


def unrelated_socket(binary):
    retirement = (
        b"\xff\xfd\x18"
        b"RETIRE_A\n"
        b"\xff\xfa\x18\x01\xff\xf0"
        b"STALE_A\n"
    )
    with ScriptedServer([retirement]) as world_a, ScriptedServer([b""]) as world_b:
        proc = run_tf(binary, f'''/set connect=blocking
/set telopt=on
/test addworld("A", "", "127.0.0.1", "{world_a.port}")
/test addworld("B", "", "127.0.0.1", "{world_b.port}")
/connect -b B
/def -i notify_b = /test send("B_SURVIVED", "B", "f")%; /quit -y
/def -1 -wA -mglob -t"RETIRE_A" retire = /repeat -0 1 /notify_b%; /dc A
/connect -b A
''')
    if world_b.connections != [b"B_SURVIVED"]:
        raise AssertionError(
            f"unrelated socket activity was lost: {world_b.connections!r}"
        )
    assert_no_stale_output(proc.stdout, "STALE_A")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", nargs="?", default="src/tf", type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f"binary not found: {binary}")

    for name, case in (
        ("prompt clears foreground", prompt_clears_foreground),
        ("queued disconnect retirement", queued_disconnect),
        ("queued line before close", queued_before_close),
        ("queued prompt before close", queued_prompt_before_close),
        ("queued same-socket reincarnation", queued_reconnect),
        ("queued reincarnation before close", queued_reincarnation_before_close),
        ("TELNET callback retirement", telnet_callback_disconnect),
        ("disconnect during TELNET chunk", telnet_disconnect),
        ("same-socket TELNET reincarnation", telnet_reconnect),
        ("MCCP recursive reincarnation", mccp_reconnect),
        ("normal input", normal_input),
        ("unrelated socket activity", unrelated_socket),
    ):
        case(binary)
        print(f"PASS: {name}")


if __name__ == "__main__":
    main()
