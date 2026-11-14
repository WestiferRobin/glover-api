#include "test_support.hpp"
#include <cli/parser.hpp>
#include <nodes/primary.hpp>
#include <nodes/secondary.hpp>
#include <nodes/client.hpp>
#include <vector>

namespace {
using Args = std::vector<std::string>;
cli::ParsedCommand parse(Args args) {
    args.insert(args.begin(),"glove");
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    const int argc=static_cast<int>(argv.size());
    argv.push_back(nullptr);
    return cli::parse(argc,argv.data());
}
Args network(const std::string& command, const std::string& transport) {
    Args args{command,"--transport",transport,command == "client" ? "--bind" : "--peer","127.0.0.1","--port","9000"};
    if (command == "secondary") args.insert(args.end(),{"--hand","left"});
    if (command == "primary" && transport == "udp")
        args.insert(args.end(),{"--bind","127.0.0.1","--listen-port","9001"});
    return args;
}
Args with(Args args, const std::string& flag, const std::string& value) {
    for (std::size_t i=1;i+1<args.size();i+=2) {
        if (args[i]==flag) { args[i+1]=value; return args; }
    }
    args.insert(args.end(),{flag,value});
    return args;
}
Args without(Args args, const std::string& flag) {
    for (std::size_t i=1;i+1<args.size();i+=2) {
        if (args[i]==flag) { args.erase(args.begin()+i,args.begin()+i+2); return args; }
    }
    throw std::logic_error("Test setup could not find option " + flag);
}
void expectNode(const Args& args, const std::string& command) {
    const auto result=parse(args);
    test::equal(result.action,cli::Action::Node);
    test::check(result.node != nullptr,"parsed node exists");
    if (command=="primary") test::check(dynamic_cast<Primary*>(result.node.get()) != nullptr,"constructed Primary");
    else if (command=="secondary") test::check(dynamic_cast<Secondary*>(result.node.get()) != nullptr,"constructed Secondary");
    else test::check(dynamic_cast<Client*>(result.node.get()) != nullptr,"constructed Client");
    // Never call run(). Hand, timeout and submission defaults are private; their
    // runtime effects belong to later process tests, not new production getters.
}
}

void test::parser(Runner& runner) {
    for (const Args& args : {Args{},Args{"mock"},Args{"-h"},Args{"--help"}}) {
        runner.run("CliParser_LocalAction_"+show(args), [=] {
            const auto result=parse(args);
            equal(result.action,args.empty() || args[0]=="mock" ? cli::Action::Mock : cli::Action::Help);
            equal(result.node == nullptr,true);
        });
    }
    runner.run("CliParser_HelpContract", [] {
        std::ostringstream output; cli::printHelp(output);
        check(output.str().starts_with("Usage: glove [command] [network options]\n"),"help usage");
        for (const std::string text : {"mock", "primary", "secondary", "client", "http get", "http post", "--stay-open", "--upstream-transport"})
            check(output.str().find(text)!=std::string::npos,"help documents " + text);
        equal(output.str().find("demo-server"),std::string::npos);
    });
    for (const std::string command : {"primary","secondary","client"}) {
        runner.run("CliParser_BareNode_"+command, [=] { expectNode({command},command); });
        for (const std::string transport : {"tcp","udp","http"}) {
            runner.run("CliParser_NetworkNode_"+command+"_"+transport, [=] { expectNode(network(command,transport),command); });
            if (command != "client") {
                for (const std::string hand : {"left","right"}) {
                    runner.run("CliParser_ExplicitHand_"+command+"_"+transport+"_"+hand, [=] {
                        expectNode(with(network(command,transport),"--hand",hand),command);
                    });
                }
            }
        }
    }
    runner.run("CliParser_TcpRelay", [] {
        expectNode(with(with(network("primary","tcp"),"--bind","127.0.0.1"),"--listen-port","9001"),"primary");
    });
    for (const std::string inbound : {"tcp","udp"}) {
        runner.run("CliParser_HttpRelay_"+inbound, [=] {
            auto args=with(with(with(network("primary","http"),"--bind","127.0.0.1"),"--listen-port","9001"),"--upstream-transport",inbound);
            if (inbound=="udp") args=with(args,"--timeout-ms","1");
            expectNode(args,"primary");
        });
    }
    for (const std::string mode : {"combined","individual"}) {
        for (const std::string command : {"primary","client"}) {
            runner.run("CliParser_Submission_"+command+"_"+mode, [=] {
                expectNode(with(network(command,"http"),"--submission",mode),command);
            });
        }
    }
    for (const std::string value : {"true","false"}) {
        runner.run("CliParser_StayOpen_"+value, [=] { expectNode(with(network("client","http"),"--stay-open",value),"client"); });
    }
    for (const std::string port : {"1","65535"}) {
        runner.run("CliParser_PortBoundary_"+port, [=] { expectNode(with(network("client","tcp"),"--port",port),"client"); });
    }
    for (const std::string timeout : {"1","2147483647"}) {
        for (const std::string transport : {"udp","http"}) {
            runner.run("CliParser_TimeoutBoundary_"+transport+"_"+timeout, [=] {
                expectNode(with(network("client",transport),"--timeout-ms",timeout),"client");
            });
        }
    }
    const Args get{"http","get","--host","localhost","--port","8080","--path","/gloves?role=primary"};
    const Args post{"http","post","--host","localhost","--port","8080","--path","/gloves","--body","hello"};
    runner.run("CliParser_HttpGetConfig", [=] {
        const auto result=parse(get);
        equal(result.action,cli::Action::HttpGet); equal(result.node==nullptr,true);
        equal(result.http.address,"localhost"); equal(result.http.port,8080);
        equal(result.http.path,"/gloves?role=primary"); equal(result.http.body,"");
        equal(result.http.contentType,"text/plain");
    });
    for (const std::string body : {"hello","","--option-like"}) {
        runner.run("CliParser_HttpPostBody_"+show(body), [=] {
            auto args=post; args.back()=body;
            const auto result=parse(args);
            equal(result.action,cli::Action::HttpPost); equal(result.node==nullptr,true);
            equal(result.http.address,"localhost"); equal(result.http.port,8080);
            equal(result.http.path,"/gloves"); equal(result.http.body,body);
            equal(result.http.contentType,"text/plain");
        });
    }
    runner.run("CliParser_HttpExplicitContentType", [=] {
        auto args=post; args.insert(args.end(),{"--content-type","application/json"});
        equal(parse(args).http.contentType,"application/json");
    });
    for (const std::string unknown : {"unknown","ble","test","demo-server"}) {
        runner.run("CliParser_UnknownCommand_"+unknown, [=] {
            const auto result=parse({unknown});
            equal(result.action,cli::Action::UnknownCommand); equal(result.command,unknown);
            equal(result.node==nullptr,true);
        });
    }
    // Table entries are separate named cases, so a rejection failure cannot hide others.
    std::vector<std::pair<std::string,Args>> invalid{
        {"MockOptions",{"mock","extra"}}, {"HelpOptions",{"--help","extra"}},
        {"UnknownOptions",{"unknown","extra"}}, {"NodePositional",{"primary","extra"}},
        {"HttpMissingAction",{"http"}}, {"RemovedDemoServer",{"http","demo-server"}},
        {"HttpUnknownAction",{"http","delete"}}, {"HttpMissingConfig",{"http","get"}},
        {"NodeMissingValue",{"client","--transport"}}, {"OptionAsValue",{"client","--transport","--port"}}
    };
    for (const std::string command : {"primary","secondary","client"}) {
        const auto base=network(command,"tcp");
        for (const std::string flag : {"--transport","--port",command=="client" ? "--bind" : "--peer"})
            invalid.emplace_back("MissingRequired_"+command+"_"+flag,without(base,flag));
        invalid.emplace_back("UnknownFlag_"+command,with(base,"--unknown","x"));
        auto duplicate=base; duplicate.insert(duplicate.end(),{"--port","9002"});
        invalid.emplace_back("Duplicate_"+command,duplicate);
        invalid.emplace_back("InvalidTransport_"+command,with(base,"--transport","ble"));
        invalid.emplace_back("InvalidAddress_"+command,with(base,command=="client" ? "--bind" : "--peer","localhost"));
    }
    invalid.emplace_back("SecondaryMissingHand",without(network("secondary","tcp"),"--hand"));
    for (const std::string hand : {"middle","LEFT",""})
        invalid.emplace_back("InvalidHand_"+hand,with(network("secondary","tcp"),"--hand",hand));
    for (const std::string port : {"0","65536","-1","+1","1.5","9000x","","999999999999999999999"})
        invalid.emplace_back("InvalidPort_"+port,with(network("client","tcp"),"--port",port));
    for (const std::string address : {"::1","256.1.1.1","127.0.0","http://127.0.0.1",""})
        invalid.emplace_back("InvalidAddress_"+address,with(network("client","tcp"),"--bind",address));
    for (const std::string value : {"0","-1","2147483648","1.5","1x",""})
        invalid.emplace_back("InvalidTimeout_"+value,with(network("client","http"),"--timeout-ms",value));
    for (const std::string value : {"yes","1","TRUE",""})
        invalid.emplace_back("InvalidBoolean_"+value,with(network("client","http"),"--stay-open",value));
    invalid.emplace_back("InvalidSubmission",with(network("client","http"),"--submission","both"));
    invalid.emplace_back("SubmissionWithoutHttp",with(network("client","tcp"),"--submission","individual"));
    invalid.emplace_back("StayOpenWithoutHttp",with(network("client","udp"),"--stay-open","true"));
    invalid.emplace_back("SecondarySubmission",with(network("secondary","http"),"--submission","individual"));
    invalid.emplace_back("ClientHand",with(network("client","http"),"--hand","left"));
    invalid.emplace_back("TcpTimeout",with(network("client","tcp"),"--timeout-ms","100"));
    invalid.emplace_back("HttpSenderTimeout",with(network("primary","http"),"--timeout-ms","100"));
    invalid.emplace_back("SecondaryTimeout",with(network("secondary","udp"),"--timeout-ms","100"));
    invalid.emplace_back("PrimaryOnlyBind",with(network("primary","tcp"),"--bind","127.0.0.1"));
    invalid.emplace_back("PrimaryOnlyListen",with(network("primary","tcp"),"--listen-port","9001"));
    invalid.emplace_back("UdpNeedsInbound",without(without(network("primary","udp"),"--bind"),"--listen-port"));
    auto relay=with(with(network("primary","http"),"--bind","127.0.0.1"),"--listen-port","9001");
    invalid.emplace_back("HttpNeedsExplicitUpstream",relay);
    invalid.emplace_back("InvalidUpstream",with(relay,"--upstream-transport","http"));
    invalid.emplace_back("UpstreamNeedsEndpoints",with(network("primary","http"),"--upstream-transport","tcp"));
    invalid.emplace_back("UpstreamNeedsHttp",with(network("primary","udp"),"--upstream-transport","tcp"));
    invalid.emplace_back("RelayNeedsCombined",with(with(relay,"--upstream-transport","udp"),"--submission","individual"));
    invalid.emplace_back("TcpRelayTimeout",with(with(relay,"--upstream-transport","tcp"),"--timeout-ms","100"));
    invalid.emplace_back("InvalidListenPort",with(network("primary","udp"),"--listen-port","0"));
    invalid.emplace_back("InvalidInboundAddress",with(network("primary","udp"),"--bind","localhost"));
    for (const auto& [name,args] : invalid) {
        runner.run("CliParser_Rejects_"+name, [&args] {
            throws<std::invalid_argument>([&] { (void)parse(args); });
        });
    }
    // HTTP options begin after two positional tokens, unlike node options.
    for (const std::string flag : {"--host","--port","--path","--body"}) {
        runner.run("CliParser_HttpMissing_"+flag, [=] {
            auto args=post;
            for (std::size_t i=2;i<args.size();i+=2) {
                if (args[i]==flag) { args.erase(args.begin()+i,args.begin()+i+2); break; }
            }
            throws<std::invalid_argument>([&] { (void)parse(args); });
        });
    }
    for (const std::string flag : {"--host","--port","--path","--body"}) {
        runner.run("CliParser_HttpDuplicate_"+flag, [=] {
            auto args=post; args.insert(args.end(),{flag,"x"});
            throws<std::invalid_argument>([&] { (void)parse(args); });
        });
    }
    for (const std::string value : {"0","65536","-1","x","1.5"}) {
        runner.run("CliParser_HttpInvalidPort_"+value, [=] {
            auto args=get; args[5]=value;
            throws<std::invalid_argument>([&] { (void)parse(args); });
        });
    }
    for (const Args& tail : {Args{"--body","x"},Args{"--content-type","text/plain"},Args{"--unknown","x"},Args{"extra"}}) {
        runner.run("CliParser_HttpGetRejects_"+show(tail), [=] {
            auto args=get; args.insert(args.end(),tail.begin(),tail.end());
            throws<std::invalid_argument>([&] { (void)parse(args); });
        });
    }
    for (std::size_t position : {std::size_t{3},std::size_t{7}}) {
        runner.run("CliParser_HttpEmptyRequired_"+show(position), [=] {
            auto args=get; args[position]="";
            throws<std::invalid_argument>([&] { (void)parse(args); });
        });
    }
    runner.run("CliParser_HttpMissingValue", [=] {
        auto args=get; args.pop_back();
        throws<std::invalid_argument>([&] { (void)parse(args); });
    });
    runner.run("CliParser_HttpEmptyContentType", [=] {
        auto args=post; args.insert(args.end(),{"--content-type",""});
        throws<std::invalid_argument>([&] { (void)parse(args); });
    });
}
