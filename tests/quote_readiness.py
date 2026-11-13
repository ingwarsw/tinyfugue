#!/usr/bin/env python3
"""Real-process regressions for consumed /quote readiness (POSIX, stdlib only)."""

import argparse
import os
import re
import select
import shlex
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TF = ROOT / "src" / "tf"
TIMEOUT = 5
ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
WRITER = """import pathlib, sys, time
root = pathlib.Path(sys.argv[1])
(root / 'first.ready').touch()
deadline = time.monotonic() + 20
while not (root / 'first.go').exists():
    if time.monotonic() > deadline:
        raise SystemExit('writer gate timed out')
    time.sleep(.01)
print('/consume', flush=True)
(root / 'first.written').touch()
time.sleep(20)
"""


class QuoteReadiness(unittest.TestCase):
    def wait_for(self, predicate, description):
        deadline = time.monotonic() + TIMEOUT
        while time.monotonic() < deadline:
            self.drain(0.02)
            if predicate():
                return
            if self.tf.poll() is not None:
                break
        self.fail(f"timed out waiting for {description}\n{self.text()}")

    def drain(self, timeout=0):
        if select.select([self.tf.stdout], [], [], timeout)[0]:
            chunk = os.read(self.tf.stdout.fileno(), 65536)
            self.output += chunk

    def text(self):
        text = ANSI.sub(b"", self.output).decode(errors="replace")
        return "\n".join(line.strip() for line in text.splitlines())

    def marker(self, value):
        self.wait_for(lambda: value in self.text().splitlines(), value)

    def command(self, value):
        self.tf.stdin.write((value + "\n").encode())
        self.tf.stdin.flush()

    def stop(self):
        os.kill(self.tf.pid, signal.SIGSTOP)
        deadline = time.monotonic() + TIMEOUT
        while time.monotonic() < deadline:
            pid, status = os.waitpid(self.tf.pid, os.WUNTRACED | os.WNOHANG)
            if pid:
                self.assertTrue(os.WIFSTOPPED(status), "owned TF exited before stop")
                return
            time.sleep(0.01)
        self.fail("owned TF did not stop")

    def test_persistent_quote_and_ga(self):
        with tempfile.TemporaryDirectory(prefix="tf-quote-readiness-") as directory:
            self.root = Path(directory)
            (self.root / "home").mkdir()
            (self.root / "empty.tf").write_text("")
            (self.root / "writer.py").write_text(WRITER)
            writer = shlex.join(
                [sys.executable, str(self.root / "writer.py"), str(self.root)]
            )
            startup = [
                "/set visual=off",
                "/set interactive=off",
                "/set calls=0",
                '/def consume = /set calls=$[calls + 1] %; /test echo(strcat("DISPATCH_", calls))',
                f"/quote -0 -dexec !{writer}",
                '/test echo(strcat("LPQUOTE=", lpquote))',
            ]
            (self.root / "startup.tf").write_text("\n".join(startup) + "\n")
            # No inherited TF variables, shell startup files, real worlds or stdlib.
            env = {
                "HOME": str(self.root / "home"),
                "PATH": os.defpath,
                "TERM": "dumb",
                "LANG": "C",
                "SHELL": "/bin/sh",
                "TFLIBRARY": str(self.root / "empty.tf"),
            }
            if "LD_LIBRARY_PATH" in os.environ:
                env["LD_LIBRARY_PATH"] = os.environ["LD_LIBRARY_PATH"]
            self.output = b""
            with socket.socket() as listener:
                listener.bind(("127.0.0.1", 0))
                listener.listen(1)
                listener.settimeout(TIMEOUT)
                self.tf = subprocess.Popen(
                    [
                        str(TF),
                        "-v",
                        "-n",
                        "-l",
                        f"-L{self.root}",
                        f"-f{self.root / 'startup.tf'}",
                        "127.0.0.1",
                        str(listener.getsockname()[1]),
                    ],
                    cwd=self.root,
                    env=env,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
                try:
                    with listener.accept()[0] as connection:
                        self.marker("LPQUOTE=off")
                        self.wait_for(
                            lambda: (self.root / "first.ready").exists(), "first writer"
                        )
                        # Let the zero-delay quote enter the select reader set.
                        time.sleep(0.1)
                        self.stop()
                        try:
                            (self.root / "first.go").touch()
                            self.wait_for(
                                lambda: (self.root / "first.written").exists(),
                                "first pipe write",
                            )
                            connection.sendall(b"synthetic prompt>\xff\xf9")
                            time.sleep(0.1)
                        finally:
                            os.kill(self.tf.pid, signal.SIGCONT)
                        self.marker("DISPATCH_1")
                        self.command('/test echo("HEARTBEAT_1")')
                        self.marker("HEARTBEAT_1")
                        self.assertEqual(
                            [
                                line
                                for line in self.text().splitlines()
                                if line.startswith("DISPATCH_")
                            ],
                            ["DISPATCH_1"],
                        )
                finally:
                    # All shell writers share this private session; never signal other TFs.
                    try:
                        os.killpg(self.tf.pid, signal.SIGCONT)
                        os.killpg(self.tf.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    try:
                        self.tf.wait(timeout=1)
                    except subprocess.TimeoutExpired:
                        pass
                    try:
                        os.killpg(self.tf.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    self.tf.wait(timeout=TIMEOUT)
                    self.tf.stdin.close()
                    self.tf.stdout.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tf", type=Path, default=TF, help="TF binary to exercise")
    args, remaining = parser.parse_known_args()
    TF = args.tf.resolve()
    if not TF.is_file():
        parser.error(f"build TF first or supply --tf: {TF}")
    unittest.main(argv=[sys.argv[0], *remaining])
