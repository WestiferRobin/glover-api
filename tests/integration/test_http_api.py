import concurrent.futures
import copy
import json
import signal
import threading

from integration.support import GLOVE, HOST, MOCK_ORACLE, NetworkTest, free_port, request


class HttpApiTests(NetworkTest):
    def server(self, mode="combined", stay_open=True, timeout=10000):
        port = free_port()
        process = self.start(
            GLOVE, "client", "--transport", "http", "--bind", HOST, "--port", port,
            "--submission", mode, "--stay-open", str(stay_open).lower(), "--timeout-ms", timeout,
        )
        process.ready_http(port)
        return process, port

    @staticmethod
    def combined(index, hand="right", left=None, right=None):
        return {"role": "primary", "hand": hand, "index": index,
                "left": [1, 0, 1, 0, 0] if left is None else left,
                "right": [0, 1, 0, 0, 1] if right is None else right}

    @staticmethod
    def individual(role, hand, index, fingers):
        return {"role": role, "hand": hand, "index": index, "fingers": fingers}

    def post(self, port, value, expected=204, path="/gloves", content_type="application/json"):
        body = value if isinstance(value, bytes) else json.dumps(value).encode()
        status, headers, output = request(port, "POST", path, body, {"Content-Type": content_type})
        self.assertEqual(status, expected, f"POST {path} body={body!r}; response={output!r}")
        if expected == 204:
            self.assertEqual(output, b"")
        else:
            self.assertTrue(output, "rejection includes a diagnostic")
        return status, headers, output

    def get(self, port, path="/gloves"):
        status, headers, body = request(port, path=path)
        self.assertEqual(status, 200, (path, body))
        self.assertEqual(headers["content-type"], "application/json")
        return json.loads(body)

    def empty(self, port, path="/gloves"):
        status, _, body = request(port, path=path)
        self.assertEqual((status, body), (204, b""))

    def stop_incomplete(self, process, missing=None):
        process.child.send_signal(signal.SIGTERM)
        process.wait(expected=1)
        self.assertEqual(process.stdout, b"")
        self.assertIn(b"HTTP session failed or incomplete", process.stderr)
        if missing is not None:
            self.assertIn(f"missing sample indices: {missing}".encode(), process.stderr)

    def complete(self, process, port, expected, stay_open=True):
        process.until(lambda: process.stdout == expected, "exact completed session output")
        if stay_open:
            self.assertIsNone(process.child.poll())
            self.assertEqual(request(port)[0], 200)
            process.child.send_signal(signal.SIGTERM)
        process.wait()
        self.assertEqual(process.stdout, expected)  # Also detects a second print during shutdown.
        self.assertNotIn(b"Error:", process.stderr)
        lines = process.stderr.splitlines()
        # Announcement is optional if a fast completion wins the readiness log race.
        self.assertTrue(all(line == f"HTTP Client listening on {HOST}:{port}".encode() for line in lines), process.stderr)

    @staticmethod
    def canonical():
        oracle = MOCK_ORACLE.read_bytes()
        return [json.loads(line) for line in oracle.splitlines()], oracle

    @staticmethod
    def race(*operations):
        barrier = threading.Barrier(len(operations), timeout=3)

        def invoke(operation):
            barrier.wait()
            return operation()

        with concurrent.futures.ThreadPoolExecutor(max_workers=len(operations)) as pool:
            futures = [pool.submit(invoke, operation) for operation in operations]
            return [future.result(timeout=5) for future in futures]

    def test_health_and_empty_state(self):
        process, port = self.server()
        status, headers, body = request(port)
        self.assertEqual((status, json.loads(body)), (200, {"status": "ok"}))
        self.assertEqual(headers["content-type"], "application/json")
        for path in ("/gloves", "/gloves?role=primary", "/gloves?role=secondary"):
            self.empty(port, path)
        self.stop_incomplete(process, "0,1,2,3,4")

    def test_combined_both_hand_assignments(self):
        for hand in ("left", "right"):
            with self.subTest(primary_hand=hand):
                process, port = self.server()
                submitted = self.combined(3, hand)
                self.post(port, submitted)
                self.assertEqual(self.get(port), {key: submitted[key] for key in ("index", "left", "right")})
                for role, physical in (("primary", hand), ("secondary", "right" if hand == "left" else "left")):
                    self.assertEqual(self.get(port, f"/gloves?role={role}"),
                                     self.individual(role, physical, 3, submitted[physical]))
                self.stop_incomplete(process, "0,1,2,4")

    def test_individual_pairing_both_hands_and_arrival_orders(self):
        for hand in ("left", "right"):
            for first_role in ("primary", "secondary"):
                with self.subTest(primary_hand=hand, first_role=first_role):
                    process, port = self.server(mode="individual")
                    left, right = [1, 0, 0, 1, 0], [0, 1, 1, 0, 1]
                    roles = {"primary": hand, "secondary": "right" if hand == "left" else "left"}
                    states = {"left": left, "right": right}
                    second = "secondary" if first_role == "primary" else "primary"
                    first = self.individual(first_role, roles[first_role], 2, states[roles[first_role]])
                    self.post(port, first)
                    self.empty(port)
                    self.empty(port, f"/gloves?role={second}")
                    self.assertEqual(self.get(port, f"/gloves?role={first_role}"), first)
                    self.post(port, self.individual(second, roles[second], 2, states[roles[second]]))
                    self.assertEqual(self.get(port), {"index": 2, "left": left, "right": right})
                    self.stop_incomplete(process, "0,1,3,4")

    def test_highest_complete_and_independent_role_indices(self):
        process, port = self.server(mode="individual")
        left, right = [1, 0, 1, 0, 0], [0, 1, 0, 0, 1]
        self.post(port, self.individual("primary", "left", 4, left))
        self.post(port, self.individual("secondary", "right", 1, right))
        self.empty(port)  # Different indices must not pair.
        self.assertEqual(self.get(port, "/gloves?role=primary")["index"], 4)
        self.assertEqual(self.get(port, "/gloves?role=secondary")["index"], 1)
        self.post(port, self.individual("secondary", "right", 4, right))
        latest = {"index": 4, "left": left, "right": right}
        self.assertEqual(self.get(port), latest)
        self.post(port, self.individual("primary", "left", 1, [0, 0, 0, 0, 0]))
        self.assertEqual(self.get(port), latest)
        self.assertEqual(self.get(port, "/gloves?role=primary")["index"], 4)
        self.assertEqual(self.get(port, "/gloves?role=secondary")["index"], 4)
        self.stop_incomplete(process, "0,2,3")

    def test_rejected_submissions_preserve_state_and_remaining_slots(self):
        process, port = self.server()
        states, oracle = self.canonical()
        seed = self.combined(2, "right", states[2][:5], states[2][5:])
        self.post(port, seed)
        paths = ("/gloves", "/gloves?role=primary", "/gloves?role=secondary")
        before = [self.get(port, path) for path in paths]
        valid = self.combined(3)
        invalid = []
        for field, value in (("role", "other"), ("hand", "up"), ("index", 5),
                             ("left", [2, 0, 0, 0, 0]), ("right", [0, 0])):
            payload = copy.deepcopy(valid)
            payload[field] = value
            invalid.append((payload, 400))
        missing = copy.deepcopy(valid)
        del missing["left"]
        unknown = dict(valid, extra=1)
        invalid += [(b"{", 400), (missing, 400), (unknown, 400),
                    (b'{"role":"primary","role":"primary","hand":"right","index":3,"left":[1,0,0,0,0],"right":[0,0,0,0,1]}', 400),
                    (self.combined(2, left=[0]*5, right=[0]*5), 409),
                    (self.combined(3, "left"), 409),
                    (self.individual("primary", "right", 4, [0]*5), 409)]
        for payload, status in invalid:
            with self.subTest(payload=payload):
                self.post(port, payload, expected=status)
                self.assertEqual([self.get(port, path) for path in paths], before)
                self.assertEqual(process.stdout, b"")
                self.assertIsNone(process.child.poll())
        self.post(port, valid, expected=415, content_type="text/plain")
        self.post(port, valid, expected=400, path="/gloves?role=primary")
        self.assertEqual([self.get(port, path) for path in paths], before)
        for index in (4, 0, 3, 1):
            self.post(port, self.combined(index, "right", states[index][:5], states[index][5:]))
        self.complete(process, port, oracle)

    def test_individual_rejections_do_not_establish_or_change_mapping(self):
        process, port = self.server(mode="individual")
        self.post(port, self.combined(0, "left"), expected=409)
        self.empty(port, "/gloves?role=primary")
        accepted = self.individual("secondary", "left", 0, [1, 0, 0, 1, 0])
        self.post(port, accepted)
        self.post(port, self.individual("secondary", "left", 0, [0]*5), expected=409)
        self.post(port, self.individual("primary", "left", 0, [0]*5), expected=409)
        self.assertEqual(self.get(port, "/gloves?role=secondary"), accepted)
        self.empty(port)
        self.empty(port, "/gloves?role=primary")
        self.post(port, self.individual("primary", "right", 0, [0, 1, 0, 0, 1]))
        self.assertEqual(self.get(port), {"index": 0, "left": accepted["fingers"], "right": [0, 1, 0, 0, 1]})
        self.stop_incomplete(process, "1,2,3,4")

    def test_routes_methods_and_query_validation(self):
        process, port = self.server()
        for path in ("/test", "/unknown"):
            self.assertEqual(request(port, path=path)[0], 404)
        status, headers, _ = request(port, "PUT", "/gloves", b"")
        self.assertEqual(status, 405)
        self.assertEqual(headers["allow"], "GET, POST")
        status, headers, _ = request(port, "POST", "/health", b"")
        self.assertEqual(status, 405)
        self.assertEqual(headers["allow"], "GET")
        for path in ("/health?x=1", "/gloves?", "/gloves?role=left", "/gloves?other=primary",
                     "/gloves?role=primary&role=secondary"):
            with self.subTest(path=path):
                self.assertEqual(request(port, path=path)[0], 400)
        self.empty(port)
        self.stop_incomplete(process)

    def test_combined_completion_both_hands_stays_open(self):
        states, oracle = self.canonical()
        for hand in ("left", "right"):
            with self.subTest(primary_hand=hand):
                process, port = self.server()
                for index in (4, 1, 3, 0, 2):
                    self.post(port, self.combined(index, hand, states[index][:5], states[index][5:]))
                    self.assertEqual(self.get(port)["index"], 4)
                self.post(port, self.combined(4, hand), expected=409)
                self.complete(process, port, oracle)

    def test_individual_completion_both_hands_automatic_shutdown(self):
        states, oracle = self.canonical()
        for hand in ("left", "right"):
            with self.subTest(primary_hand=hand):
                process, port = self.server(mode="individual", stay_open=False)
                opposite = "right" if hand == "left" else "left"
                for index in (4, 2, 0, 3, 1):
                    state = states[index]
                    primary = state[:5] if hand == "left" else state[5:]
                    secondary = state[5:] if hand == "left" else state[:5]
                    self.post(port, self.individual("secondary", opposite, index, secondary))
                    self.post(port, self.individual("primary", hand, index, primary))
                self.complete(process, port, oracle, stay_open=False)

    def test_partial_shutdown_has_no_success_output(self):
        process, port = self.server()
        self.post(port, self.combined(1))
        self.stop_incomplete(process, "0,2,3,4")
        # A fresh Client session can bind the same port after shutdown.
        replacement = self.start(GLOVE, "client", "--transport", "http", "--bind", HOST,
                                 "--port", port, "--timeout-ms", 10000)
        replacement.ready_http(port)
        self.empty(port)
        self.stop_incomplete(replacement, "0,1,2,3,4")

    def test_incomplete_session_timeout(self):
        process, port = self.server(timeout=1200)
        self.post(port, self.combined(0))
        process.wait(expected=1, timeout=5)
        self.assertEqual(process.stdout, b"")
        self.assertIn(b"missing sample indices: 1,2,3,4", process.stderr)

    def test_concurrent_opposite_roles_and_get_complete_once(self):
        process, port = self.server(mode="individual")
        states, oracle = self.canonical()
        for index in range(5):
            state = states[index]
            primary = self.individual("primary", "right", index, state[5:])
            secondary = self.individual("secondary", "left", index, state[:5])
            results = self.race(
                lambda: request(port, "POST", "/gloves", json.dumps(primary), {"Content-Type": "application/json"}),
                lambda: request(port, "POST", "/gloves", json.dumps(secondary), {"Content-Type": "application/json"}),
                lambda: request(port, path="/gloves"),
            )
            self.assertEqual([response[0] for response in results[:2]], [204, 204])
            self.assertEqual([response[2] for response in results[:2]], [b"", b""])
            status, _, body = results[2]
            if status == 204:
                self.assertEqual(index, 0)
                self.assertEqual(body, b"")
            else:
                self.assertEqual(status, 200)
                snapshot = json.loads(body)
                self.assertIn(snapshot["index"], {index-1, index})
                expected = states[snapshot["index"]]
                self.assertEqual(snapshot, {"index": snapshot["index"], "left": expected[:5], "right": expected[5:]})
        self.complete(process, port, oracle)

    def test_concurrent_duplicate_has_one_acceptance(self):
        process, port = self.server(mode="individual")
        sample = self.individual("primary", "left", 0, [1, 0, 0, 1, 0])
        operation = lambda: request(port, "POST", "/gloves", json.dumps(sample), {"Content-Type": "application/json"})
        results = self.race(operation, operation)
        self.assertEqual(sorted(response[0] for response in results), [204, 409])
        self.assertEqual(self.get(port, "/gloves?role=primary"), sample)
        self.empty(port)
        self.stop_incomplete(process)

    def test_concurrent_conflicting_initial_mapping(self):
        process, port = self.server(mode="individual")
        samples = [self.individual("primary", hand, 0, [1, 0, 1, 0, 0]) for hand in ("left", "right")]
        operations = [lambda sample=sample: request(port, "POST", "/gloves", json.dumps(sample),
                                                     {"Content-Type": "application/json"}) for sample in samples]
        results = self.race(*operations)
        self.assertEqual(sorted(response[0] for response in results), [204, 409])
        winner = samples[next(i for i, response in enumerate(results) if response[0] == 204)]
        self.assertEqual(self.get(port, "/gloves?role=primary"), winner)
        opposite = "right" if winner["hand"] == "left" else "left"
        self.post(port, self.individual("secondary", opposite, 0, [0, 1, 0, 0, 1]))
        expected = {winner["hand"]: winner["fingers"], opposite: [0, 1, 0, 0, 1], "index": 0}
        self.assertEqual(self.get(port), expected)
        self.stop_incomplete(process, "1,2,3,4")
