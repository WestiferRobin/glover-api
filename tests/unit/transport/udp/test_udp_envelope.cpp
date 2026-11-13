#include "test_support.hpp"
#include <transport/udp/detail/udp_envelope.hpp>
#include <limits>
#include <vector>

void test::udpEnvelope(Runner& runner) {
    for (std::size_t index=0; index<5; ++index) {
        runner.run("UdpEnvelope_ExactIndex_" + show(index), [=] {
            const std::string expected = "U1 " + std::to_string(index) + " S5 L 10100";
            const auto encoded = serializeUdpSample(index,"S5 L 10100");
            equal(encoded,expected);
            const auto decoded = parseUdpSample(expected);
            equal(decoded.index,index);
            equal(decoded.payload,"S5 L 10100");
            equal(parseUdpSample(encoded).payload,decoded.payload);
        });
    }
    for (const auto& payload : std::vector<std::string>{" ","opaque\ntrailing",std::string("a\0b",3),std::string(5000,'x')}) {
        runner.run("UdpEnvelope_OpaquePayloadBytes_" + show(payload.size()), [=] {
            const auto encoded = serializeUdpSample(4,payload);
            equal(encoded,"U1 4 " + payload);
            const auto decoded = parseUdpSample(encoded);
            equal(decoded.index,std::size_t{4});
            equal(decoded.payload,std::string_view(payload));
            check(decoded.payload.data() == encoded.data()+5,"payload is a view into datagram");
        });
    }
    runner.run("UdpEnvelope_RejectsEmptyPayload", [] {
        throws<std::invalid_argument>([] { (void)serializeUdpSample(0,""); });
        throws<std::invalid_argument>([] { (void)parseUdpSample("U1 0 "); });
    });
    for (auto index : {std::size_t{5},std::size_t{10},std::numeric_limits<std::size_t>::max()}) {
        runner.run("UdpEnvelope_InvalidSerializedIndex_" + show(index), [=] {
            throws<std::invalid_argument>([=] { (void)serializeUdpSample(index,"x"); });
        });
    }
    for (const std::string input : {"", "U1", "U1 0", "U2 0 x", "u1 0 x", "U1 5 x", "U1 -1 x",
                                   "U1 10 x", "U1 a x", "U1  x", "U1\t0 x", "U1 0\tx", " U1 0 x", "U10 x"}) {
        runner.run("UdpEnvelope_InvalidEnvelope_" + show(input), [=] {
            throws<std::invalid_argument>([&] { (void)parseUdpSample(input); });
        });
    }
    runner.run("UdpEnvelope_MissingIndicesDiagnostic", [] {
        equal(missingUdpSamples({}),"; missing sample indices: 0,1,2,3,4");
        equal(missingUdpSamples({true,false,true,false,true}),"; missing sample indices: 1,3");
        equal(missingUdpSamples({true,true,true,true,true}),"; missing sample indices: none");
    });
}
