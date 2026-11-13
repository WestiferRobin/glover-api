"""Small, Glover-specific process supervision; no application assertions here."""

import http.client
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
GLOVE = ROOT / "build/glove"
DRIVER = ROOT / "build/test-transport"
MOCK_ORACLE = ROOT / "tests/fixtures/expected_mock.txt"
HOST = "127.0.0.1"
START_TIMEOUT = 6.0
PROCESS_TIMEOUT = 10.0


def free_port(kind=socket.SOCK_STREAM):
    # Discovery, not reservation: startup can still lose a bind race. Do not
    # retry correctness assertions; any collision is reported with child output.
    with socket.socket(socket.AF_INET, kind) as probe:
        probe.bind((HOST, 0))
        return probe.getsockname()[1]


def request(port, method="GET", path="/health", body=None, headers=None, timeout=2.0):
    deadline = time.monotonic() + timeout

    def remaining():
        budget = deadline - time.monotonic()
        if budget <= 0:
            raise TimeoutError(f"HTTP {method} {HOST}:{port}{path} exceeded {timeout}s")
        return budget

    connection = http.client.HTTPConnection(HOST, port, timeout=remaining())
    try:
        connection.connect()
        wire = connection.sock
        wire.settimeout(remaining())
        connection.request(method, path, body=body, headers=headers or {})
        wire.settimeout(remaining())
        response = connection.getresponse()
        chunks = []
        while not response.isclosed():
            wire.settimeout(remaining())
            chunk = response.read1(65536)
            if not chunk:
                break
            chunks.append(chunk)
        remaining()
        return response.status, {k.lower(): v for k, v in response.getheaders()}, b"".join(chunks)
    finally:
        connection.close()


def owned_socket(pid, port, transport):
    tool = shutil.which("lsof")
    if tool is None:
        raise AssertionError("TCP/UDP integration readiness requires lsof on this platform")
    args = [tool, "-nP", "-a", "-p", str(pid), f"-i{transport}:{port}", "-Fn"]
    if transport == "TCP":
        args += ["-sTCP:LISTEN"]
    result = subprocess.run(args, capture_output=True, timeout=2)
    if result.returncode not in (0, 1) or result.stderr:
        raise AssertionError(f"Socket inspection failed: {args!r}: {result.stderr!r}")
    return result.returncode == 0 and f"p{pid}\n".encode() in result.stdout


class Process:
    def __init__(self, command, data=None):
        self.command = [str(arg) for arg in command]
        self.output = tempfile.TemporaryFile()
        self.errors = tempfile.TemporaryFile()
        self.input = None
        self.sockets = []
        try:
            if data is not None:
                self.input = tempfile.TemporaryFile()
                self.input.write(data)
                self.input.seek(0)
            self.child = subprocess.Popen(
                self.command, cwd=ROOT, stdin=self.input or subprocess.PIPE,
                stdout=self.output, stderr=self.errors,
            )
        except BaseException:
            self.output.close()
            self.errors.close()
            if self.input:
                self.input.close()
            raise

    @staticmethod
    def read(stream):
        # pread does not disturb the file offset shared with the child writer.
        return os.pread(stream.fileno(), os.fstat(stream.fileno()).st_size, 0)

    @property
    def stdout(self):
        return self.read(self.output)

    @property
    def stderr(self):
        return self.read(self.errors)

    def diagnostic(self):
        return (f"command={self.command!r}\npid={self.child.pid} exit={self.child.poll()}\n"
                f"stdout={self.stdout!r}\nstderr={self.stderr!r}")

    def tcp_timeout_target(self):
        if self.command[:2] == [str(DRIVER), "tcp-send"]:
            return True
        if self.command[:2] != [str(GLOVE), "primary"]:
            return False
        arguments = self.command[2:]
        if len(arguments) % 2:
            return False
        options = dict(zip(arguments[::2], arguments[1::2]))
        return (options.get("--transport") == "tcp"
                and "--bind" not in options and "--listen-port" not in options)

    def tcp_timeout_diagnostics(self):
        pid = str(self.child.pid)
        sections = ["=== GLOVER TCP TIMEOUT DIAGNOSTICS ==="]
        commands = (
            ("Process State (ps)", ["ps", "-p", pid, "-o", "pid,ppid,stat,etime,command"]),
            ("TCP Sockets (lsof)", ["lsof", "-nP", "-a", "-p", pid, "-iTCP"]),
            ("Process Sample (sample)", ["sample", pid, "1", "1", "-file", "/dev/stdout"]),
        )
        for title, command in commands:
            sections.append(f"--- {title} ---\nCommand: {command!r}")
            try:
                # Do not inspect a PID after our owned child has been reaped.
                state = self.child.poll()
                if state is not None:
                    sections.append(f"Not run: child exited during collection (exit={state})")
                    continue
                tool = shutil.which(command[0])
                if tool is None:
                    sections.append(f"Not run: tool unavailable: {command[0]}")
                    continue
                command = [tool, *command[1:]]
                sections.append(f"Resolved command: {command!r}")
                result = subprocess.run(command, capture_output=True, timeout=3)
                sections.append(f"Exit status: {result.returncode}\n"
                                f"stdout:\n{result.stdout.decode(errors='replace')}\n"
                                f"stderr:\n{result.stderr.decode(errors='replace')}")
            except subprocess.TimeoutExpired as error:
                # run() kills and reaps its diagnostic child on timeout.
                sections.append(f"Diagnostic timed out after {error.timeout}s; exit status unavailable\n"
                                f"Partial stdout:\n{(error.stdout or b'').decode(errors='replace')}\n"
                                f"Partial stderr:\n{(error.stderr or b'').decode(errors='replace')}")
            except Exception as error:
                sections.append(f"Diagnostic execution error: {type(error).__name__}: {error}; "
                                "exit status unavailable")
        sections.append("=== END DIAGNOSTICS ===")
        return "\n".join(sections)

    def wait(self, expected=0, timeout=PROCESS_TIMEOUT):
        try:
            actual = self.child.wait(timeout=timeout)
        except subprocess.TimeoutExpired as error:
            message = f"Expected exit {expected} within {timeout}s\n{self.diagnostic()}"
            if expected == 1 and self.tcp_timeout_target():
                # Capture before unwinding the test's destination reservation.
                # The deadline already failed, even if the child exits meanwhile.
                try:
                    message += "\n" + self.tcp_timeout_diagnostics()
                except Exception as diagnostic_error:
                    message += (f"\nTCP diagnostic collection failed: "
                                f"{type(diagnostic_error).__name__}: {diagnostic_error}")
            raise AssertionError(message) from error
        if actual != expected:
            raise AssertionError(f"Expected exit {expected}, actual {actual}\n{self.diagnostic()}")
        return self

    def until(self, predicate, description, timeout=START_TIMEOUT):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            if self.child.poll() is not None:
                raise AssertionError(f"Process exited before {description}\n{self.diagnostic()}")
            time.sleep(0.01)  # Bounded polling backoff, not a readiness assumption.
        raise AssertionError(f"Deadline waiting for {description}\n{self.diagnostic()}")

    def ready_socket(self, port, transport):
        self.sockets.append((port, transport))
        self.until(lambda: owned_socket(self.child.pid, port, transport), f"{transport} bind on {port}")

    def ready_http(self, port):
        self.sockets.append((port, "TCP"))

        def ready():
            try:
                status, _, body = request(port, timeout=0.3)
            except (OSError, http.client.HTTPException):
                return False
            if (status, body) != (200, b'{"status":"ok"}'):
                raise AssertionError(f"Unexpected health response {(status, body)!r}\n{self.diagnostic()}")
            return True

        self.until(ready, f"HTTP health on {port}")

    def ready_driver(self, port):
        self.sockets.append((port, "TCP"))
        self.until(lambda: self.stdout == b"READY\n", f"driver isRunning notification on {port}")

    def stop_http_driver(self):
        self.child.stdin.write(b"stop\n")
        self.child.stdin.flush()
        return self.wait()

    def close(self):
        problem = None
        try:
            if self.child.poll() is None:
                self.child.terminate()
                try:
                    self.child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.child.kill()
                    self.child.wait(timeout=3)
                    problem = "Graceful cleanup timed out; owned child required kill"
            # wait reaps even an already exited child; never signal by a reused PID.
            self.child.wait(timeout=3)
            for port, transport in self.sockets:
                if owned_socket(self.child.pid, port, transport):
                    problem = f"Owned {transport} socket on {port} remains after exit"
            if problem:
                raise AssertionError(f"{problem}\n{self.diagnostic()}")
        finally:
            if self.child.stdin:
                self.child.stdin.close()
            if self.input:
                self.input.close()
            self.output.close()
            self.errors.close()


class NetworkTest(unittest.TestCase):
    """Only common process ownership; scenarios stay in individual modules."""

    def setUp(self):
        self.processes = []
        self.addCleanup(self.cleanup_processes)

    def start(self, *command, data=None):
        process = Process(command, data)
        self.processes.append(process)
        return process

    def driver(self, mode, port, *args, data=None, address=HOST):
        return self.start(DRIVER, mode, address, port, *args, data=data)

    def cleanup_processes(self):
        # Keep all child output available in the failure report before closing files.
        result = self._outcome.result
        failed = any(case is self or getattr(case, "test_case", None) is self
                     for case, _ in result.failures + result.errors)
        if failed:
            for process in self.processes:
                print(f"\n{self.id()}\n{process.diagnostic()}", file=sys.stderr)
        errors = []
        for process in reversed(self.processes):
            try:
                process.close()
            except Exception as error:
                errors.append(str(error))
        if errors:
            raise AssertionError("Cleanup failures:\n" + "\n".join(errors))
