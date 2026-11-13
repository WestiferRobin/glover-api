"""Executable dispatch, output streams and exits; parser tables stay in C++."""

import socket

from integration.support import GLOVE, HOST, MOCK_ORACLE, NetworkTest, free_port


class CliTests(NetworkTest):
    def invoke(self, *args, expected=0):
        return self.start(GLOVE, *args).wait(expected=expected)

    def test_default_mock_and_local_primary(self):
        expected = MOCK_ORACLE.read_bytes()
        for args in ((), ("mock",), ("primary",)):
            with self.subTest(command=args):
                process = self.invoke(*args)
                self.assertEqual(process.stdout, expected)
                self.assertEqual(process.stderr, b"")

    def test_help_aliases(self):
        long_help = self.invoke("--help")
        short_help = self.invoke("-h")
        self.assertEqual(long_help.stdout, short_help.stdout)
        self.assertTrue(long_help.stdout.startswith(b"Usage: glove "))
        for text in (b"mock", b"primary", b"secondary", b"client", b"http get", b"http post"):
            self.assertIn(text, long_help.stdout)
        self.assertEqual(long_help.stderr, b"")
        self.assertEqual(short_help.stderr, b"")

    def test_bare_secondary_and_client(self):
        for role in ("secondary", "client"):
            with self.subTest(role=role):
                process = self.invoke(role)
                self.assertEqual(process.stdout, f"{role.title()} node started.\n".encode())
                self.assertEqual(process.stderr, b"")

    def test_unknown_command_has_help_on_stderr(self):
        process = self.invoke("unknown-command", expected=1)
        self.assertEqual(process.stdout, b"")
        self.assertTrue(process.stderr.startswith(b"Unknown command: unknown-command\nUsage: glove "))

    def test_representative_invalid_arguments(self):
        primary = ["primary", "--transport", "http", "--peer", HOST, "--port", "12345"]
        client = ["client", "--transport", "http", "--bind", HOST, "--port", "12345"]
        cases = [
            (["primary", "--transport", "tcp"], b"Network mode requires"),
            (["primary", "--transport", "ble", "--peer", HOST, "--port", "12345"], b"Transport must"),
            (primary + ["--hand", "up"], b"Hand must"),
            (client + ["--submission", "both"], b"Submission must"),
            (["client", "--transport", "tcp", "--bind", HOST, "--port", "0"], b"Port must"),
            (client + ["--timeout-ms", "0"], b"Timeout must"),
            (primary + ["--port", "12346"], b"Duplicate option"),
            (["client", "--transport", "tcp", "--bind", HOST, "--port", "12345", "--stay-open", "true"], b"require HTTP"),
            (["http", "demo-server"], b"Unknown HTTP action"),
        ]
        for args, diagnostic in cases:
            with self.subTest(arguments=args):
                process = self.invoke(*args, expected=1)
                self.assertEqual(process.stdout, b"")
                self.assertTrue(process.stderr.startswith(b"Error: "))
                self.assertIn(diagnostic, process.stderr)

    def http_server(self):
        port = free_port()
        server = self.driver("http-serve", port)
        server.ready_driver(port)
        return server, port

    def http(self, action, port, path, *args, expected=0):
        return self.invoke("http", action, "--host", HOST, "--port", port, "--path", path,
                           *args, expected=expected)

    def test_generic_http_get_and_error_status_exit_zero(self):
        server, port = self.http_server()
        for path, status, body in (("/echo", 200, b"hello\0http"),
                                   ("/missing", 404, b"Not Found"), ("/status", 418, b"teapot")):
            with self.subTest(path=path):
                process = self.http("get", port, path)
                self.assertEqual(process.stdout, body)
                self.assertEqual(process.stderr, f"HTTP {status}\n".encode())
        server.stop_http_driver()
        self.assertEqual(server.stderr, b"")

    def test_generic_http_post_body_and_content_type(self):
        server, port = self.http_server()
        for content_type in (None, "application/json"):
            with self.subTest(content_type=content_type):
                options = ["--body", '{"value":1}']
                if content_type:
                    options += ["--content-type", content_type]
                process = self.http("post", port, "/request", *options)
                expected_type = content_type or "text/plain"
                self.assertEqual(process.stdout, f'POST\n/request\n/request\n{expected_type}\n{{"value":1}}'.encode())
                self.assertEqual(process.stderr, b"HTTP 200\n")
        server.stop_http_driver()
        self.assertEqual(server.stderr, b"")

    def test_generic_http_application_400_still_exits_zero(self):
        port = free_port()
        server = self.start(GLOVE, "client", "--transport", "http", "--bind", HOST,
                            "--port", port, "--timeout-ms", "10000")
        server.ready_http(port)
        process = self.http("post", port, "/gloves", "--body", "{", "--content-type", "application/json")
        self.assertEqual(process.stdout, b"Invalid JSON submission")
        self.assertEqual(process.stderr, b"HTTP 400\n")
        server.child.terminate()
        server.wait(expected=1)
        self.assertEqual(server.stdout, b"")
        self.assertIn(b"missing sample indices: 0,1,2,3,4", server.stderr)

    def test_generic_http_connection_failure_exits_one(self):
        with socket.socket() as reserved:
            reserved.bind((HOST, 0))
            process = self.http("get", reserved.getsockname()[1], "/health", expected=1)
            self.assertEqual(process.stdout, b"")
            self.assertTrue(process.stderr.startswith(b"Error: HTTP GET failed:"))
