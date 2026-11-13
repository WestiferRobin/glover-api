import signal
import socket

from integration.support import HOST, NetworkTest, free_port


class TcpTests(NetworkTest):
    def receiver(self, port=None):
        port = free_port() if port is None else port
        server = self.driver("tcp-receive", port)
        server.ready_socket(port, "TCP")
        return server, port

    def deliver(self, payload):
        server, port = self.receiver()
        client = self.driver("tcp-send", port, data=payload).wait()
        self.assertEqual(client.stdout, b"")
        self.assertEqual(client.stderr, b"")
        return server

    def test_single_frame_and_clean_eof(self):
        server = self.deliver(b"hello\n").wait()
        self.assertEqual(server.stdout, b"68656c6c6f\n")
        self.assertEqual(server.stderr, b"")

    def test_multiple_frames_order_and_empty_frame(self):
        server = self.deliver(b"first\n\nlast\n").wait()
        self.assertEqual(server.stdout, b"6669727374\n\n6c617374\n")
        self.assertEqual(server.stderr, b"")

    def test_empty_stream_clean_eof(self):
        server = self.deliver(b"").wait()
        self.assertEqual(server.stdout, b"")
        self.assertEqual(server.stderr, b"")

    def test_binary_payload_and_maximum_frame(self):
        payload = b"\x00\xff\r" + b"x" * 4093
        server = self.deliver(payload + b"\n").wait()
        self.assertEqual(server.stdout, payload.hex().encode() + b"\n")
        self.assertEqual(server.stderr, b"")

    def test_unterminated_frame_rejected(self):
        server = self.deliver(b"complete\npartial").wait(expected=1)
        self.assertEqual(server.stdout, b"636f6d706c657465\n")
        self.assertIn(b"missing LF", server.stderr)

    def test_oversized_frame_rejected(self):
        server, port = self.receiver()
        with socket.create_connection((HOST, port), timeout=2) as peer:
            peer.sendall(b"x" * 4097)
            peer.shutdown(socket.SHUT_WR)
        server.wait(expected=1)
        self.assertEqual(server.stdout, b"")
        self.assertIn(b"exceeds 4096", server.stderr)

    def test_connection_refused(self):
        # Hold the port without listening: it cannot be stolen between selection and connect.
        with socket.socket() as reserved:
            reserved.bind((HOST, 0))
            client = self.driver("tcp-send", reserved.getsockname()[1], data=b"x\n").wait(expected=1)
            self.assertEqual(client.stdout, b"")
            self.assertIn(b"TCP connect", client.stderr)

    def test_occupied_listener(self):
        with socket.socket() as occupied:
            occupied.bind((HOST, 0))
            occupied.listen(1)
            process = self.driver("tcp-receive", occupied.getsockname()[1]).wait(expected=1)
            self.assertIn(b"TCP bind", process.stderr)
            self.assertEqual(process.stdout, b"")

    def test_termination_and_port_reuse(self):
        server, port = self.receiver()
        server.child.terminate()
        server.wait(expected=-signal.SIGTERM)
        replacement, _ = self.receiver(port)
        self.driver("tcp-send", port, data=b"reused\n").wait()
        replacement.wait()
        self.assertEqual(replacement.stdout, b"726575736564\n")
