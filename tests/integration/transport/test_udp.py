import socket

from integration.support import HOST, NetworkTest, free_port


class UdpTests(NetworkTest):
    def receiver(self, count=1, timeout=4000, port=None):
        port = free_port(socket.SOCK_DGRAM) if port is None else port
        process = self.driver("udp-receive", port, count, timeout)
        process.ready_socket(port, "UDP")
        return process, port

    def test_binary_payload_preserved(self):
        receiver, port = self.receiver()
        sender = self.driver("udp-send", port, data=b"a\x00\xff\nb").wait()
        receiver.wait()
        self.assertEqual(sender.stdout, b"")
        self.assertEqual(sender.stderr, b"")
        self.assertEqual(receiver.stdout, b"6100ff0a62\n")
        self.assertEqual(receiver.stderr, b"")

    def test_multiple_datagrams_boundaries_and_empty(self):
        receiver, port = self.receiver(count=3)
        expected = b""
        for payload in (b"first", b"", b"last\n"):
            self.driver("udp-send", port, data=payload).wait()
            expected += payload.hex().encode() + b"\n"
            receiver.until(lambda: receiver.stdout == expected, "next complete datagram")
        receiver.wait()
        self.assertEqual(receiver.stdout, b"6669727374\n\n6c6173740a\n")
        self.assertEqual(receiver.stderr, b"")

    def test_maximum_size(self):
        receiver, port = self.receiver()
        payload = bytes(range(256)) * 16
        self.driver("udp-send", port, data=payload).wait()
        receiver.wait()
        self.assertEqual(receiver.stdout, payload.hex().encode() + b"\n")

    def test_sender_rejects_oversize(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
            peer.bind((HOST, 0))
            peer.settimeout(0.2)
            sender = self.driver("udp-send", peer.getsockname()[1], data=b"x" * 4097).wait(expected=1)
            self.assertIn(b"exceeds 4096", sender.stderr)
            with self.assertRaises(socket.timeout):
                peer.recvfrom(8192)

    def test_receiver_rejects_truncation(self):
        receiver, port = self.receiver()
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
            peer.settimeout(2)
            peer.sendto(b"x" * 4097, (HOST, port))
        receiver.wait(expected=1)
        self.assertIn(b"truncated", receiver.stderr)
        self.assertEqual(receiver.stdout, b"")

    def test_invalid_receiver_configuration(self):
        for address, port, count, timeout, diagnostic in (
            ("localhost", 1, 1, 1000, b"numeric IPv4"),
            (HOST, 0, 1, 1000, b"port"),
            (HOST, 65536, 1, 1000, b"port"),
            (HOST, 1, 0, 1000, b"positive count"),
            (HOST, 1, 1, 0, b"timeout"),
        ):
            with self.subTest(address=address, port=port, count=count, timeout=timeout):
                process = self.driver("udp-receive", port, count, timeout, address=address).wait(expected=1)
                self.assertIn(diagnostic, process.stderr)
                self.assertEqual(process.stdout, b"")

    def test_invalid_sender_configuration(self):
        for address, port in (("localhost", 1), (HOST, 0), (HOST, 65536)):
            with self.subTest(address=address, port=port):
                process = self.driver("udp-send", port, data=b"x", address=address).wait(expected=1)
                self.assertIn(b"UDP", process.stderr)
                self.assertEqual(process.stdout, b"")

    def test_receiver_timeout(self):
        receiver, _ = self.receiver(timeout=1200)
        receiver.wait(expected=1, timeout=5)
        self.assertEqual(receiver.stdout, b"")
        self.assertIn(b"received 0 of 1 datagrams before timeout", receiver.stderr)

    def test_occupied_port(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
            occupied.bind((HOST, 0))
            process = self.driver("udp-receive", occupied.getsockname()[1], 1, 1000).wait(expected=1)
            self.assertIn(b"UDP bind", process.stderr)

    def test_port_reuse_after_completion(self):
        receiver, port = self.receiver()
        self.driver("udp-send", port, data=b"one").wait()
        receiver.wait()
        replacement, _ = self.receiver(port=port)
        self.driver("udp-send", port, data=b"two").wait()
        replacement.wait()
        self.assertEqual(receiver.stdout, b"6f6e65\n")
        self.assertEqual(replacement.stdout, b"74776f\n")
