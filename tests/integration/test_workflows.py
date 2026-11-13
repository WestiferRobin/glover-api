"""Real node pipelines and failures; API schema tables belong to test_http_api."""

from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import signal
import socket
import threading

from integration.support import GLOVE, HOST, MOCK_ORACLE, NetworkTest, free_port, request


def opposite(hand):
    return "left" if hand == "right" else "right"


class WorkflowTests(NetworkTest):
    def client(self, transport, mode="combined", stay=False, timeout=10000, port=None):
        if port is None:
            port = free_port(socket.SOCK_DGRAM if transport == "udp" else socket.SOCK_STREAM)
        args = [GLOVE, "client", "--transport", transport, "--bind", HOST, "--port", port]
        if transport == "http":
            args += ["--submission", mode, "--stay-open", str(stay).lower()]
        if transport != "tcp":
            args += ["--timeout-ms", timeout]
        process = self.start(*args)
        if transport == "http":
            process.ready_http(port)
        else:
            process.ready_socket(port, transport.upper())
        return process, port

    def primary(self, transport, port, hand="right", inbound=None, mode="combined", timeout=10000):
        args = [GLOVE, "primary", "--transport", transport, "--hand", hand, "--peer", HOST, "--port", port]
        if transport == "http":
            args += ["--submission", mode]
        upstream = None
        if inbound:
            upstream = free_port(socket.SOCK_DGRAM if inbound == "udp" else socket.SOCK_STREAM)
            args += ["--bind", HOST, "--listen-port", upstream]
            if transport == "http":
                args += ["--upstream-transport", inbound]
            if inbound == "udp":
                args += ["--timeout-ms", timeout]
        process = self.start(*args)
        if inbound:
            process.ready_socket(upstream, inbound.upper())
        return process, upstream

    def secondary(self, transport, port, hand="left"):
        return self.start(GLOVE, "secondary", "--transport", transport, "--hand", hand,
                          "--peer", HOST, "--port", port)

    def silent_success(self, process):
        process.wait()
        self.assertEqual(process.stdout, b"", process.diagnostic())
        self.assertEqual(process.stderr, b"", process.diagnostic())

    def canonical_output(self, process):
        expected = MOCK_ORACLE.read_bytes()
        self.assertEqual(process.stdout, expected, process.diagnostic())
        self.assertEqual(len(process.stdout.splitlines()), 5)
        self.assertEqual(len(set(process.stdout.splitlines())), 5)

    def client_success(self, process, transport, port):
        process.wait()
        self.canonical_output(process)
        if transport == "http":
            expected = f"HTTP Client listening on {HOST}:{port}".encode()
            self.assertTrue(all(line == expected for line in process.stderr.splitlines()), process.diagnostic())
        else:
            self.assertEqual(process.stderr, b"", process.diagnostic())
        self.released(port, transport)

    def released(self, port, transport):
        # Actual rebind/listen proves release; TCP TIME_WAIT is not a listener.
        kind = socket.SOCK_DGRAM if transport == "udp" else socket.SOCK_STREAM
        with socket.socket(socket.AF_INET, kind) as replacement:
            if kind == socket.SOCK_STREAM:
                replacement.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            replacement.bind((HOST, port))
            if kind == socket.SOCK_STREAM:
                replacement.listen(1)

    def workflow(self, name, hand):
        outbound, inbound, mode = {
            "tcp_two": ("tcp", None, "combined"),
            "tcp_three": ("tcp", "tcp", "combined"),
            "udp_three": ("udp", "udp", "combined"),
            "http_two": ("http", None, "combined"),
            "tcp_http": ("http", "tcp", "combined"),
            "udp_http": ("http", "udp", "combined"),
            "http_direct": ("http", None, "individual"),
        }[name]
        with self.subTest(workflow=name, primary_hand=hand, secondary_hand=opposite(hand)):
            client, port = self.client(outbound, mode)
            primary, upstream = self.primary(outbound, port, hand, inbound, mode)
            secondary = None
            if inbound:
                secondary = self.secondary(inbound, upstream, opposite(hand))
            elif mode == "individual":
                self.silent_success(primary)
                self.assertEqual(client.stdout, b"")
                self.assertEqual(request(port, path="/gloves")[0], 204)
                secondary = self.secondary("http", port, opposite(hand))
            if secondary:
                self.silent_success(secondary)
            self.silent_success(primary)
            self.client_success(client, outbound, port)
            if upstream:
                self.released(upstream, inbound)

    def test_success_tcp_two_right(self):
        self.workflow("tcp_two", "right")

    def test_success_tcp_two_left(self):
        self.workflow("tcp_two", "left")

    def test_success_tcp_three_right(self):
        self.workflow("tcp_three", "right")

    def test_success_tcp_three_left(self):
        self.workflow("tcp_three", "left")

    def test_success_udp_three_right(self):
        self.workflow("udp_three", "right")

    def test_success_udp_three_left(self):
        self.workflow("udp_three", "left")

    def test_success_http_two_right(self):
        self.workflow("http_two", "right")

    def test_success_http_two_left(self):
        self.workflow("http_two", "left")

    def test_success_tcp_http_right(self):
        self.workflow("tcp_http", "right")

    def test_success_tcp_http_left(self):
        self.workflow("tcp_http", "left")

    def test_success_udp_http_right(self):
        self.workflow("udp_http", "right")

    def test_success_udp_http_left(self):
        self.workflow("udp_http", "left")

    def test_success_http_direct_right(self):
        self.workflow("http_direct", "right")

    def test_success_http_direct_left(self):
        self.workflow("http_direct", "left")

    @staticmethod
    def hand_samples(hand):
        # Test inputs come from the one independent oracle, not another fixture definition.
        states = [json.loads(line) for line in MOCK_ORACLE.read_bytes().splitlines()]
        offset = 0 if hand == "left" else 5
        return [f"S5 {hand[0].upper()} ".encode() + bytes(48 + bit for bit in state[offset:offset+5])
                for state in states]

    @staticmethod
    def tcp_inject(port, payload):
        with socket.create_connection((HOST, port), timeout=2) as peer:
            peer.sendall(payload)
            peer.shutdown(socket.SHUT_WR)

    def tcp_failure(self, payload, diagnostic, expected_output=b"", outbound="tcp"):
        client, port = self.client(outbound, stay=outbound == "http")
        primary, upstream = self.primary(outbound, port, inbound="tcp")
        self.tcp_inject(upstream, payload)
        primary.wait(expected=1)
        self.assertEqual(primary.stdout, b"")
        self.assertIn(diagnostic, primary.stderr)
        if outbound == "http":
            client.child.terminate()
            client.wait(expected=0 if expected_output == MOCK_ORACLE.read_bytes() else 1)
        else:
            client.wait()  # TCP Client accepts clean EOF independently of sample count.
            self.assertEqual(client.stderr, b"")
        self.assertEqual(client.stdout, expected_output)
        self.released(upstream, "tcp")
        self.released(port, outbound)

    def test_tcp_malformed_hand(self):
        self.tcp_failure(b"not a hand\n", b"Invalid five-state payload")

    def test_tcp_wrong_hand(self):
        self.tcp_failure(b"S5 R 10000\n", b"opposite physical hand")

    def test_tcp_early_eof(self):
        self.tcp_failure(self.hand_samples("left")[0] + b"\n", b"before five samples",
                         MOCK_ORACLE.read_bytes().splitlines(keepends=True)[0])

    def test_tcp_extra_sample_not_hidden_by_client_success(self):
        frames = self.hand_samples("left")
        self.tcp_failure(b"\n".join(frames + [frames[0]]) + b"\n", b"after five samples", MOCK_ORACLE.read_bytes())

    def test_tcp_unterminated_frame(self):
        self.tcp_failure(b"S5 L 10000", b"missing LF")

    def test_tcp_oversized_frame(self):
        self.tcp_failure(b"x" * 4097, b"exceeds 4096")

    def test_tcp_primary_client_unavailable(self):
        with socket.socket() as reserved:
            reserved.bind((HOST, 0))
            primary, _ = self.primary("tcp", reserved.getsockname()[1])
            primary.wait(expected=1)
            self.assertIn(b"TCP connect", primary.stderr)
            self.assertEqual(primary.stdout, b"")

    def test_tcp_standalone_client_short_eof_is_success(self):
        client, port = self.client("tcp")
        line = MOCK_ORACLE.read_bytes().splitlines(keepends=True)[0]
        self.tcp_inject(port, line)
        client.wait()
        self.assertEqual(client.stdout, line)
        self.assertEqual(client.stderr, b"")
        self.released(port, "tcp")

    def test_tcp_missing_sender_supervisor_cleanup_and_rebind(self):
        client, port = self.client("tcp")
        # TCP has no application deadline. The test supplies its own bounded wait.
        with self.assertRaisesRegex(AssertionError, "Expected exit 0 within"):
            client.wait(timeout=0.15)
        client.child.terminate()
        client.wait(expected=-signal.SIGTERM)
        replacement, _ = self.client("tcp", port=port)
        primary, _ = self.primary("tcp", port)
        self.silent_success(primary)
        self.client_success(replacement, "tcp", port)

    def test_tcp_primary_missing_sender_supervisor_cleanup(self):
        client, port = self.client("tcp")
        primary, upstream = self.primary("tcp", port, inbound="tcp")
        with self.assertRaisesRegex(AssertionError, "Expected exit 0 within"):
            primary.wait(timeout=0.15)
        primary.child.terminate()
        primary.wait(expected=-signal.SIGTERM)
        client.wait()  # Closing the relay produces clean EOF, not five samples.
        self.assertEqual(primary.stdout, b"")
        self.assertEqual(client.stdout, b"")
        self.assertEqual(client.stderr, b"")
        self.released(upstream, "tcp")
        self.released(port, "tcp")

    def udp_failure(self, datagrams, diagnostic):
        # A local raw receiver keeps the downstream endpoint valid without an
        # unrelated Client timeout delaying each malformed upstream case.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as downstream:
            downstream.bind((HOST, 0))
            downstream.settimeout(2)
            primary, upstream = self.primary("udp", downstream.getsockname()[1], inbound="udp", timeout=2000)
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.settimeout(2)
                for index, payload in enumerate(datagrams):
                    sender.sendto(payload, (HOST, upstream))
                    if index + 1 < len(datagrams):
                        # A forwarded first packet proves acceptance before the duplicate.
                        downstream.recvfrom(4096)
            primary.wait(expected=1, timeout=5)
            self.assertEqual(primary.stdout, b"")
            self.assertIn(diagnostic, primary.stderr)
            self.released(upstream, "udp")

    def test_udp_invalid_envelope(self):
        self.udp_failure([b"bad envelope"], b"Invalid UDP envelope")

    def test_udp_invalid_index(self):
        self.udp_failure([b"U1 5 S5 L 10000"], b"Invalid UDP sample index")

    def test_udp_duplicate_index(self):
        self.udp_failure([b"U1 0 S5 L 10000"] * 2, b"Duplicate UDP sample index")

    def test_udp_wrong_hand(self):
        self.udp_failure([b"U1 0 S5 R 10000"], b"opposite physical hand")

    def test_udp_malformed_hand(self):
        self.udp_failure([b"U1 0 S5 L 20000"], b"binary digits")

    def test_udp_missing_sample_primary_timeout(self):
        self.udp_failure([b"U1 0 S5 L 10000"], b"missing sample indices: 1,2,3,4")

    def test_udp_missing_sender_client_timeout(self):
        client, port = self.client("udp", timeout=1500)
        client.wait(expected=1, timeout=5)
        self.assertEqual(client.stdout, b"")
        self.assertIn(b"missing sample indices: 0,1,2,3,4", client.stderr)
        self.released(port, "udp")

    def test_udp_missing_sender_primary_timeout(self):
        self.udp_failure([], b"missing sample indices: 0,1,2,3,4")

    def test_udp_client_duplicate_and_missing_samples(self):
        for duplicate in (False, True):
            with self.subTest(duplicate=duplicate):
                client, port = self.client("udp", timeout=1500)
                state = MOCK_ORACLE.read_bytes().splitlines()[0]
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    sender.settimeout(2)
                    sender.sendto(b"U1 0 " + state, (HOST, port))
                    if duplicate:
                        sender.sendto(b"U1 0 " + state, (HOST, port))
                client.wait(expected=1, timeout=5)
                self.assertEqual(client.stdout, b"")
                self.assertIn(b"Duplicate UDP sample index" if duplicate else b"before timeout", client.stderr)
                self.released(port, "udp")

    def test_udp_client_rejects_malformed_datagrams(self):
        state = MOCK_ORACLE.read_bytes().splitlines()[0]
        for payload, diagnostic in ((b"bad", b"Invalid UDP envelope"),
                                    (b"U1 5 " + state, b"Invalid UDP sample index"),
                                    (b"U1 0 [2,0,0,0,0,0,0,0,0,0]", b"Invalid state")):
            with self.subTest(payload=payload):
                client, port = self.client("udp")
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                    sender.settimeout(2)
                    sender.sendto(payload, (HOST, port))
                client.wait(expected=1)
                self.assertEqual(client.stdout, b"")
                self.assertIn(diagnostic, client.stderr)
                self.released(port, "udp")

    def test_udp_reordered_pairing_both_hands_and_transports(self):
        for outbound in ("udp", "http"):
            for hand in ("right", "left"):
                with self.subTest(outbound=outbound, primary_hand=hand):
                    client, port = self.client(outbound)
                    primary, upstream = self.primary(outbound, port, hand, "udp")
                    samples = self.hand_samples(opposite(hand))
                    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                        sender.settimeout(2)
                        for index in (4, 1, 3, 0, 2):
                            sender.sendto(f"U1 {index} ".encode() + samples[index], (HOST, upstream))
                    self.silent_success(primary)
                    self.client_success(client, outbound, port)
                    self.released(upstream, "udp")

    def test_http_relay_client_unavailable_secondary_exits_independently(self):
        with socket.socket() as reserved:
            reserved.bind((HOST, 0))
            primary, upstream = self.primary("http", reserved.getsockname()[1], inbound="udp")
            secondary = self.secondary("udp", upstream)
            self.silent_success(secondary)  # UDP send success does not acknowledge forwarding.
            primary.wait(expected=1)
            self.assertEqual(primary.stdout, b"")
            self.assertIn(b"HTTP forwarding sample 0", primary.stderr)
            self.assertIn(b"HTTP POST failed", primary.stderr)
            self.released(upstream, "udp")

    def test_http_relay_wrong_mode_rejected(self):
        client, port = self.client("http", mode="individual", stay=True)
        primary, upstream = self.primary("http", port, inbound="udp")
        secondary = self.secondary("udp", upstream)
        self.silent_success(secondary)
        primary.wait(expected=1)
        self.assertIn(b"expected HTTP 204, received HTTP 409", primary.stderr)
        self.assertEqual(primary.stdout, b"")
        self.assertEqual(request(port, path="/gloves")[0], 204)
        self.incomplete_http(client, port)
        self.released(upstream, "udp")

    @contextmanager
    def downstream_response(self, malformed=False):
        """One bounded test-owned peer response, without changing the C++ driver."""
        requests = []
        errors = []

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
                requests.append((self.path, body))
                if malformed:
                    self.wfile.write(b"invalid HTTP response\r\n\r\n")
                else:
                    self.send_response(200)  # Successful HTTP is still the wrong acknowledgment.
                    self.send_header("Content-Length", "0")
                    self.end_headers()

            def log_message(self, *_args):
                pass  # The assertions inspect captured requests and explicit errors instead.

        class Peer(HTTPServer):
            def get_request(self):
                connection, address = super().get_request()
                connection.settimeout(2)
                return connection, address

            def handle_error(self, *_args):
                import traceback
                errors.append(traceback.format_exc())

        server = Peer((HOST, 0), Handler)
        server.timeout = 5

        def serve():
            try:
                server.handle_request()
            except Exception as error:
                errors.append(repr(error))

        worker = threading.Thread(target=serve)
        worker.start()  # HTTPServer construction has already bound/listened.
        try:
            yield server.server_port, requests
        finally:
            worker.join(timeout=8)
            server.server_close()
            self.assertFalse(worker.is_alive(), "bounded downstream peer did not terminate")
            self.assertEqual(errors, [])

    def relay_bad_response(self, malformed):
        with self.downstream_response(malformed) as (port, requests):
            primary, upstream = self.primary("http", port, inbound="udp")
            secondary = self.secondary("udp", upstream)
            self.silent_success(secondary)
            primary.wait(expected=1)
            self.assertEqual(primary.stdout, b"")
            self.assertIn(b"HTTP forwarding sample 0", primary.stderr)
            self.assertIn(b"HTTP POST failed" if malformed else b"expected HTTP 204, received HTTP 200", primary.stderr)
            self.assertEqual(len(requests), 1, "rejected requests must not be retried")
            self.assertEqual(requests[0][0], "/gloves")
            self.assertEqual(json.loads(requests[0][1])["index"], 0)
            self.released(upstream, "udp")

    def test_http_relay_rejects_non_204_success(self):
        self.relay_bad_response(False)

    def test_http_relay_rejects_malformed_response(self):
        self.relay_bad_response(True)

    def test_http_relay_incomplete_tcp_upstream(self):
        self.tcp_failure(self.hand_samples("left")[0] + b"\n", b"before five samples", outbound="http")

    def test_http_relay_malformed_tcp_upstream(self):
        self.tcp_failure(b"malformed\n", b"Invalid five-state payload", outbound="http")

    def test_http_relay_extra_sample_failure_after_client_completion(self):
        frames = self.hand_samples("left")
        self.tcp_failure(b"\n".join(frames + [frames[0]]) + b"\n", b"after five samples",
                         MOCK_ORACLE.read_bytes(), outbound="http")

    def test_http_relay_missing_udp_sender_timeout(self):
        client, port = self.client("http", stay=True)
        primary, upstream = self.primary("http", port, inbound="udp", timeout=1500)
        primary.wait(expected=1, timeout=5)
        self.assertEqual(primary.stdout, b"")
        self.assertIn(b"missing sample indices: 0,1,2,3,4", primary.stderr)
        self.assertEqual(request(port, path="/gloves")[0], 204)
        self.incomplete_http(client, port)
        self.released(upstream, "udp")

    def incomplete_http(self, client, port, signum=signal.SIGTERM, missing=None):
        client.child.send_signal(signum)
        client.wait(expected=1)
        self.assertEqual(client.stdout, b"", client.diagnostic())
        self.assertIn(b"HTTP session failed or incomplete", client.stderr)
        if missing:
            self.assertIn(f"missing sample indices: {missing}".encode(), client.stderr)
        self.released(port, "http")

    def only_direct_sender(self, role):
        client, port = self.client("http", mode="individual", stay=True)
        if role == "primary":
            sender, _ = self.primary("http", port, mode="individual")
        else:
            sender = self.secondary("http", port)
        self.silent_success(sender)
        self.assertEqual(request(port, path="/gloves")[0], 204)
        status, _, body = request(port, path=f"/gloves?role={role}")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["index"], 4)
        other = "secondary" if role == "primary" else "primary"
        self.assertEqual(request(port, path=f"/gloves?role={other}")[0], 204)
        self.incomplete_http(client, port, missing="0,1,2,3,4")

    def test_direct_http_only_primary(self):
        self.only_direct_sender("primary")

    def test_direct_http_only_secondary(self):
        self.only_direct_sender("secondary")

    def test_direct_http_one_index_unpaired(self):
        client, port = self.client("http", mode="individual", stay=True)
        primary, _ = self.primary("http", port, mode="individual")
        self.silent_success(primary)
        states = [json.loads(line) for line in MOCK_ORACLE.read_bytes().splitlines()]
        for index in range(4):
            body = {"role": "secondary", "hand": "left", "index": index, "fingers": states[index][:5]}
            status, _, output = request(port, "POST", "/gloves", json.dumps(body), {"Content-Type": "application/json"})
            self.assertEqual((status, output), (204, b""))
        self.assertEqual(json.loads(request(port, path="/gloves")[2])["index"], 3)
        self.incomplete_http(client, port, missing="4")

    def test_direct_http_wrong_hand_sender_rejected(self):
        client, port = self.client("http", mode="individual", stay=True)
        primary, _ = self.primary("http", port, mode="individual")
        self.silent_success(primary)
        secondary = self.secondary("http", port, hand="right").wait(expected=1)
        self.assertEqual(secondary.stdout, b"")
        self.assertIn(b"expected HTTP 204, received HTTP 409", secondary.stderr)
        self.assertEqual(request(port, path="/gloves?role=secondary")[0], 204)
        self.incomplete_http(client, port)

    def test_direct_http_conflicting_primary_role_rejected(self):
        client, port = self.client("http", mode="individual", stay=True)
        primary, _ = self.primary("http", port, hand="right", mode="individual")
        self.silent_success(primary)
        conflict, _ = self.primary("http", port, hand="left", mode="individual")
        conflict.wait(expected=1)
        self.assertEqual(conflict.stdout, b"")
        self.assertIn(b"expected HTTP 204, received HTTP 409", conflict.stderr)
        state = json.loads(request(port, path="/gloves?role=primary")[2])
        self.assertEqual((state["hand"], state["index"]), ("right", 4))
        self.incomplete_http(client, port)

    def test_direct_http_duplicate_sender_rejected_without_overwrite(self):
        client, port = self.client("http", mode="individual", stay=True)
        primary, _ = self.primary("http", port, mode="individual")
        self.silent_success(primary)
        before = request(port, path="/gloves?role=primary")
        duplicate, _ = self.primary("http", port, mode="individual")
        duplicate.wait(expected=1)
        self.assertEqual(duplicate.stdout, b"")
        self.assertIn(b"expected HTTP 204, received HTTP 409", duplicate.stderr)
        after = request(port, path="/gloves?role=primary")
        self.assertEqual((after[0], after[2]), (before[0], before[2]))
        secondary = self.secondary("http", port)
        self.silent_success(secondary)
        client.until(lambda: client.stdout == MOCK_ORACLE.read_bytes(), "paired output after rejected duplicate")
        client.child.terminate()
        self.client_success(client, "http", port)

    def completed_http_signal(self, signum, hand):
        client, port = self.client("http", stay=True)
        primary, _ = self.primary("http", port, hand)
        self.silent_success(primary)
        client.until(lambda: client.stdout == MOCK_ORACLE.read_bytes(), "completed stay-open output")
        self.assertIsNone(client.child.poll())
        status, _, body = request(port)
        self.assertEqual((status, json.loads(body)), (200, {"status": "ok"}))
        latest = json.loads(request(port, path="/gloves")[2])
        self.assertEqual(latest, {"index": 4, "left": [1]*5, "right": [1]*5})
        for role, physical in (("primary", hand), ("secondary", opposite(hand))):
            status, _, body = request(port, path=f"/gloves?role={role}")
            self.assertEqual(status, 200)
            self.assertEqual(json.loads(body), {"role": role, "hand": physical, "index": 4, "fingers": [1]*5})
        client.child.send_signal(signum)
        self.client_success(client, "http", port)
        replacement, _ = self.client("http", port=port, stay=True)
        self.assertEqual(request(port, path="/gloves")[0], 204)
        self.incomplete_http(replacement, port, signum=signum)

    def test_http_completed_sigint_and_reuse(self):
        self.completed_http_signal(signal.SIGINT, "left")

    def test_http_completed_sigterm_and_reuse(self):
        self.completed_http_signal(signal.SIGTERM, "right")

    def test_http_incomplete_sigint(self):
        client, port = self.client("http", mode="individual", stay=True)
        primary, _ = self.primary("http", port, mode="individual")
        self.silent_success(primary)
        self.incomplete_http(client, port, signum=signal.SIGINT)

    def test_http_incomplete_sigterm(self):
        client, port = self.client("http", mode="individual", stay=True)
        secondary = self.secondary("http", port)
        self.silent_success(secondary)
        self.incomplete_http(client, port, signum=signal.SIGTERM)

    def test_http_missing_sender_timeout(self):
        client, port = self.client("http", timeout=1500)
        client.wait(expected=1, timeout=5)
        self.assertEqual(client.stdout, b"")
        self.assertIn(b"missing sample indices: 0,1,2,3,4", client.stderr)
        self.released(port, "http")
