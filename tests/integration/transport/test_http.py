import socket

from integration.support import HOST, NetworkTest, free_port, request


class HttpTransportTests(NetworkTest):
    def server(self, port=None):
        port = free_port() if port is None else port
        server = self.driver("http-serve", port)
        server.ready_driver(port)
        return server, port

    def stop(self, server):
        server.stop_http_driver()
        self.assertEqual(server.stdout, b"READY\nSTOPPED\n")
        self.assertEqual(server.stderr, b"")

    def test_cpp_get_preserves_binary_response(self):
        server, port = self.server()
        client = self.driver("http-get", port, "/echo").wait()
        self.assertEqual(client.stdout, b"200\nhello\x00http")
        self.assertEqual(client.stderr, b"")
        self.stop(server)

    def test_cpp_post_preserves_binary_body_and_status(self):
        server, port = self.server()
        payload = bytes(range(256)) + b"\x00end"
        client = self.driver("http-post", port, "/echo", "application/octet-stream", data=payload).wait()
        self.assertEqual(client.stdout, b"201\n" + payload)
        self.assertEqual(client.stderr, b"")
        self.stop(server)

    def test_request_metadata_and_response_content_type(self):
        server, port = self.server()
        status, headers, body = request(port, "POST", "/echo", b"raw\x00bytes", {"Content-Type": "application/octet-stream"})
        self.assertEqual((status, body), (201, b"raw\x00bytes"))
        self.assertEqual(headers["content-type"], "application/octet-stream")
        status, _, body = request(port, "POST", "/request?x=%2F", b"data", {"Content-Type": "text/custom"})
        self.assertEqual((status, body), (200, b"POST\n/request\n/request?x=%2F\ntext/custom\ndata"))
        self.stop(server)

    def test_http_error_status_is_not_transport_failure(self):
        server, port = self.server()
        client = self.driver("http-get", port, "/status").wait()
        self.assertEqual(client.stdout, b"418\nteapot")
        self.assertEqual(client.stderr, b"")
        self.stop(server)

    def test_unknown_route_and_unsupported_method(self):
        server, port = self.server()
        status, _, body = request(port, path="/missing")
        self.assertEqual((status, body), (404, b"Not Found"))
        status, headers, body = request(port, "PUT", "/echo", b"")
        self.assertEqual((status, body), (405, b"Method Not Allowed"))
        self.assertEqual(headers["allow"], "GET, POST")
        status, headers, _ = request(port, "POST", "/status", b"")
        self.assertEqual(status, 405)
        self.assertEqual(headers["allow"], "GET")
        self.stop(server)

    def test_handler_exception_and_invalid_response(self):
        server, port = self.server()
        for path in ("/throw", "/invalid"):
            with self.subTest(path=path):
                status, _, body = request(port, path=path)
                self.assertEqual((status, body), (500, b"Internal Server Error"))
                self.assertNotIn(b"private", body)
        self.stop(server)

    def test_exact_route_matching(self):
        server, port = self.server()
        self.assertEqual(request(port, path="/literal.+")[0::2], (200, b"literal"))
        self.assertEqual(request(port, path="/literalXYZ")[0], 404)
        self.stop(server)

    def test_connection_refused(self):
        with socket.socket() as reserved:
            reserved.bind((HOST, 0))
            process = self.driver("http-get", reserved.getsockname()[1], "/echo").wait(expected=1)
            self.assertEqual(process.stdout, b"")
            self.assertIn(b"HTTP GET failed", process.stderr)

    def test_occupied_listener(self):
        with socket.socket() as occupied:
            occupied.bind((HOST, 0))
            occupied.listen(1)
            process = self.driver("http-serve", occupied.getsockname()[1]).wait(expected=1)
            self.assertIn(b"HTTP bind/listen failed", process.stderr)
            self.assertEqual(process.stdout, b"")

    def test_shutdown_and_port_reuse(self):
        server, port = self.server()
        self.stop(server)
        replacement, _ = self.server(port)
        self.assertEqual(request(port, path="/echo")[0], 200)
        self.stop(replacement)
