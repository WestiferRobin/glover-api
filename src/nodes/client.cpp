#include <codec/glove_json.hpp>
#include <transport/http/server.hpp>
#include <mock.hpp>
#include <algorithm>
#include <climits>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <pthread.h>
#include <signal.h>
#include <system_error>
#include <cerrno>

#include <type_traits>
#include <tuple>
#include <nodes/client.hpp>
#include <iostream>
#include <transport/detail/state_codec.hpp>
#include <transport/udp/detail/udp_envelope.hpp>
#include <stdexcept>

Client::Client(const std::string& bindAddress, std::uint16_t port,
               NodeTransport transport, std::chrono::milliseconds timeout)
    : udpTimeout(timeout) {
    if (transport == NodeTransport::Tcp) {
        server.emplace(bindAddress, port);
    } else if (transport == NodeTransport::Udp) {
        udpServer.emplace(bindAddress, port);
    } else {
        throw std::invalid_argument("Invalid node transport");
    }
}

void Client::runUdp() {
    std::array<bool, UDP_SAMPLE_COUNT> seen{};
    std::array<std::array<bool, PAIR_FINGER_COUNT>, UDP_SAMPLE_COUNT> states{};
    try {
        udpServer->run(UDP_SAMPLE_COUNT, udpTimeout, [&](const std::string& datagram) {
            const auto sample = parseUdpSample(datagram);
            const auto state = parseState(sample.payload);
            if (seen[sample.index]) {
                throw std::invalid_argument(
                    "Duplicate UDP sample index: " + std::to_string(sample.index));
            }
            states[sample.index] = state;
            seen[sample.index] = true;
        });
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(error.what()) + missingUdpSamples(seen));
    }
    // Publish only a complete valid session, in fixture order regardless of arrival order.
    for (const auto& state : states) {
        std::cout << serializeState(state) << '\n';
    }
}

void Client::run() {
    if (httpPort) {
        runHttp();
        return;
    }
    if (udpServer) {
        runUdp();
        return;
    }
    if (!server) {
        std::cout << "Client node started.\n";
        return;
    }
    server->run([](const std::string& payload) {
        const auto state = parseState(payload);
        std::cout << serializeState(state) << '\n';
    });
}


namespace {
struct HttpSlot {
    std::optional<HandState> primary, secondary;
    std::optional<std::array<bool, PAIR_FINGER_COUNT>> complete;
};
struct HttpSession {
    std::mutex mutex;
    std::condition_variable changed;
    std::array<HttpSlot, std::tuple_size_v<std::remove_cvref_t<decltype(mockSamples())>>> slots{};
    std::optional<Hand> primaryHand;
    std::optional<std::size_t> latestPrimary, latestSecondary, latestComplete;
    bool complete = false, failed = false, stopped = false;
    bool serverDone = false;
    std::exception_ptr serverError;
    std::chrono::steady_clock::time_point deadline;
};
Hand opposite(Hand hand) { return hand == Hand::Left ? Hand::Right : Hand::Left; }
void advance(std::optional<std::size_t>& latest, std::size_t index) {
    if (!latest || index > *latest) latest = index;
}
}

Client::Client(const std::string& address, std::uint16_t port,
               HttpSubmissionMode submission, std::chrono::milliseconds timeout, bool stayOpen)
    : httpAddress(address), httpPort(port),
      httpSubmission(submission), httpStayOpen(stayOpen), httpTimeout(timeout) {
    if (!port || timeout.count() <= 0 || timeout.count() > INT_MAX ||
        (submission != HttpSubmissionMode::Combined && submission != HttpSubmissionMode::Individual))
        throw std::invalid_argument("Invalid HTTP Client configuration");
}

void Client::runHttp() {
    HttpServer server(httpAddress, httpPort);
    HttpSession session;
    session.deadline = std::chrono::steady_clock::now() + httpTimeout;
    server.get("/health", [](const HttpRequest& request) {
        if (request.target.find('?') != std::string::npos)
            return HttpServerResponse{400, "Health does not accept query parameters"};
        return HttpServerResponse{200, "{\"status\":\"ok\"}", "application/json"};
    });
    server.get("/gloves", [&](const HttpRequest& request) {
        std::optional<GloveRole> role;
        try { role = glove_json::roleFilter(request.target); }
        catch (const std::invalid_argument& error) { return HttpServerResponse{400, error.what()}; }
        std::optional<HandState> hand;
        std::optional<std::array<bool, PAIR_FINGER_COUNT>> state;
        std::optional<std::size_t> index;
        {
            std::lock_guard lock(session.mutex);
            index = !role ? session.latestComplete : *role == GloveRole::Primary
                ? session.latestPrimary : session.latestSecondary;
            if (index) {
                const auto& slot = session.slots[*index];
                if (!role) state = slot.complete;
                else hand = *role == GloveRole::Primary ? slot.primary : slot.secondary;
            }
        }
        if (!index) return HttpServerResponse{204, ""};
        return HttpServerResponse{200, role
            ? glove_json::individual(*role, hand->hand, *index, hand->state)
            : glove_json::combined(*index, *state), "application/json"};
    });
    server.post("/gloves", [&](const HttpRequest& request) -> HttpServerResponse {
        if (request.target.find('?') != std::string::npos)
            return {400, "POST does not accept query parameters"};
        if (!glove_json::isJsonContentType(request.contentType))
            return {415, "Expected application/json with optional UTF-8 charset"};
        try {
            const auto input = glove_json::parse(request.body);
            std::lock_guard lock(session.mutex);
            if (!session.complete && std::chrono::steady_clock::now() >= session.deadline)
                session.failed = true;
            if (session.failed || session.stopped) return {409, "Session is closed"};
            if (input.combined != (httpSubmission == HttpSubmissionMode::Combined))
                return {409, "Submission mode conflict"};
            const auto primaryHand = input.role == GloveRole::Primary ? input.hand : opposite(input.hand);
            if (session.primaryHand && *session.primaryHand != primaryHand)
                return {409, "Role-to-hand mapping conflict"};
            auto& slot = session.slots.at(input.index);
            if (input.combined ? slot.complete.has_value() :
                (input.role == GloveRole::Primary ? slot.primary.has_value() : slot.secondary.has_value()))
                return {409, "Duplicate sample"};
            session.primaryHand = primaryHand;
            if (input.combined) {
                slot.primary = HandState{primaryHand, primaryHand == Hand::Left ? input.left : input.right};
                slot.secondary = HandState{opposite(primaryHand), primaryHand == Hand::Left ? input.right : input.left};
                advance(session.latestPrimary, input.index);
                advance(session.latestSecondary, input.index);
            } else if (input.role == GloveRole::Primary) {
                slot.primary = HandState{input.hand, input.fingers};
                advance(session.latestPrimary, input.index);
            } else {
                slot.secondary = HandState{input.hand, input.fingers};
                advance(session.latestSecondary, input.index);
            }
            if (slot.primary && slot.secondary) {
                std::array<bool, PAIR_FINGER_COUNT> state{};
                const auto& left = primaryHand == Hand::Left ? slot.primary->state : slot.secondary->state;
                const auto& right = primaryHand == Hand::Right ? slot.primary->state : slot.secondary->state;
                std::copy(left.begin(), left.end(), state.begin());
                std::copy(right.begin(), right.end(), state.begin() + FINGER_COUNT);
                slot.complete = state;
                advance(session.latestComplete, input.index);
            }
            session.complete = std::all_of(session.slots.begin(), session.slots.end(),
                                          [](const auto& entry) { return entry.complete.has_value(); });
            session.changed.notify_one();
            return {204, ""};
        } catch (const std::invalid_argument& error) {
            return {400, error.what()};
        } catch (...) {
            std::lock_guard lock(session.mutex);
            session.failed = true;
            session.changed.notify_one();
            throw;
        }
    });

    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    // This CLI execution ends the process; retain the mask through cleanup so
    // repeated stop signals cannot interrupt worker joins.
    const int maskError = pthread_sigmask(SIG_BLOCK, &signals, nullptr);
    if (maskError) throw std::system_error(maskError, std::generic_category(), "HTTP signal mask");
    std::thread runner([&] {
        std::exception_ptr error;
        try { server.run(); } catch (...) { error = std::current_exception(); }
        std::lock_guard lock(session.mutex);
        session.serverError = error;
        session.serverDone = true;
        session.changed.notify_one();
    });
    try {
        bool announced = false, printed = false;
        for (;;) {
            std::unique_lock lock(session.mutex);
            if (session.serverDone) break;
            if (!announced && server.isRunning()) {
                std::cerr << "HTTP Client listening on " << httpAddress << ':' << httpPort << '\n';
                announced = true;
            }
            sigset_t pending;
            if (sigpending(&pending)) throw std::system_error(errno, std::generic_category(), "HTTP signals");
            const bool stopping = sigismember(&pending, SIGINT) || sigismember(&pending, SIGTERM);
            if (stopping) {
                int received = 0;
                const int error = sigwait(&signals, &received);
                if (error) throw std::system_error(error, std::generic_category(), "HTTP signal wait");
            }
            if (!session.complete && (stopping || std::chrono::steady_clock::now() >= session.deadline))
                session.failed = true;
            if (session.complete && !session.failed && !printed) {
                const auto slots = session.slots;
                printed = true;
                lock.unlock();
                for (const auto& slot : slots) std::cout << serializeState(*slot.complete) << '\n';
                std::cout.flush();
                lock.lock();
            }
            if (session.failed || stopping || (session.complete && !httpStayOpen)) {
                session.stopped = true;
                lock.unlock();
                server.stop();
                break;
            }
            session.changed.wait_for(lock, std::chrono::milliseconds(50));
        }
        runner.join();
    } catch (...) {
        server.stop();
        runner.join();
        throw;
    }
    if (session.serverError) std::rethrow_exception(session.serverError);
    if (session.failed || !session.complete) {
        std::string missing;
        for (std::size_t i = 0; i < session.slots.size(); ++i) {
            if (!session.slots[i].complete) missing += (missing.empty() ? "" : ",") + std::to_string(i);
        }
        throw std::runtime_error("HTTP session failed or incomplete; missing sample indices: " +
                                 (missing.empty() ? "none" : missing));
    }
}
