#include "test_support.hpp"
#include <transport/tcp/detail/framing.hpp>
#include <vector>

void test::framing(Runner& runner) {
    const std::string wire = "one\ntwo\n\nthree\n";
    // Every split includes empty chunks and boundaries inside/after LF frames.
    for (std::size_t split=0; split<=wire.size(); ++split) {
        runner.run("TcpFraming_EveryTwoChunkSplit_" + show(split), [=] {
            tcp_detail::LineFramer framer;
            std::vector<std::string> frames;
            const auto collect = [&](const std::string& frame) { frames.push_back(frame); };
            framer.consume(std::string_view(wire).substr(0,split),collect);
            framer.consume(std::string_view(wire).substr(split),collect);
            framer.finish();
            equal(frames,(std::vector<std::string>{"one","two","","three"}));
        });
    }
    runner.run("TcpFraming_ByteChunksPreserveCRAndNUL", [] {
        tcp_detail::LineFramer framer;
        std::vector<std::string> frames;
        const auto collect = [&](const std::string& frame) { frames.push_back(frame); };
        const std::string wire("a\r\nb\0c\n",7);
        for (char byte : wire) framer.consume(std::string_view(&byte,1),collect);
        framer.finish();
        equal(frames,(std::vector<std::string>{"a\r",std::string("b\0c",3)}));
    });
    runner.run("TcpFraming_UnterminatedEOFAndRetainedBuffer", [] {
        tcp_detail::LineFramer framer;
        std::vector<std::string> frames;
        const auto collect = [&](const std::string& frame) { frames.push_back(frame); };
        framer.consume("first\npar",collect);
        equal(frames,(std::vector<std::string>{"first"}));
        throws<std::runtime_error>([&] { framer.finish(); });
        framer.consume("tial\nnext\n",collect);
        framer.finish();
        equal(frames,(std::vector<std::string>{"first","partial","next"}));
    });
    runner.run("TcpFraming_EmptyInputAndEmptyFrames", [] {
        tcp_detail::LineFramer framer;
        std::vector<std::string> frames;
        const auto collect = [&](const std::string& frame) { frames.push_back(frame); };
        framer.consume("",collect);
        framer.finish();
        equal(frames.size(),std::size_t{0});
        framer.consume("\n\n",collect);
        framer.finish();
        equal(frames,(std::vector<std::string>{"",""}));
    });
    runner.run("TcpFraming_MaximumLengthResetsAfterLF", [] {
        tcp_detail::LineFramer framer;
        std::vector<std::string> frames;
        const auto collect = [&](const std::string& frame) { frames.push_back(frame); };
        const std::string maximum(TcpServer::MAX_MESSAGE_BYTES,'x');
        framer.consume(maximum,collect);
        equal(frames.size(),std::size_t{0});
        throws<std::runtime_error>([&] { framer.finish(); });
        framer.consume("\n" + maximum + "\n",collect);
        framer.finish();
        equal(frames,(std::vector<std::string>{maximum,maximum}));
    });
    for (bool fragmented : {false,true}) {
        runner.run("TcpFraming_RejectsOversize_" + show(fragmented), [=] {
            tcp_detail::LineFramer framer;
            std::size_t callbacks=0;
            const auto collect = [&](const std::string&) { ++callbacks; };
            if (fragmented) framer.consume(std::string(TcpServer::MAX_MESSAGE_BYTES,'x'),collect);
            throws<std::length_error>([&] {
                framer.consume(fragmented ? "x" : std::string(TcpServer::MAX_MESSAGE_BYTES+1,'x')+"\n",collect);
            });
            equal(callbacks,std::size_t{0});
        });
    }
}
