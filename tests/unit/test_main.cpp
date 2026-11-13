#include "test_support.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: test-unit ABSOLUTE_FIXTURE_DIRECTORY\n";
        return 1;
    }
    try {
        const std::filesystem::path fixtures(argv[1]);
        if (!fixtures.is_absolute() || !std::filesystem::is_directory(fixtures))
            throw std::invalid_argument("Fixture directory must exist and be absolute");
        test::Runner runner(fixtures);
        test::glove(runner);
        test::pair(runner);
        test::mock(runner);
        test::stateCodec(runner);
        test::udpEnvelope(runner);
        test::framing(runner);
        test::gloveJson(runner);
        test::parser(runner);
        return runner.finish();
    } catch (const std::exception& error) {
        std::cerr << "Unit runner error: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "Unit runner error: unexpected non-standard exception\n";
    }
    return 1;
}
