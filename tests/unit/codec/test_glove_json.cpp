#include "test_support.hpp"
#include <codec/glove_json.hpp>
#include <nlohmann/json.hpp>
#include <vector>

namespace {
using Json = nlohmann::json;
Json individual() {
    return Json::parse(R"({"role":"secondary","hand":"left","index":0,"fingers":[1,0,1,0,0]})");
}
Json combined() {
    return Json::parse(R"({"role":"primary","hand":"right","index":0,"left":[1,0,1,0,0],"right":[0,1,0,0,1]})");
}
}

void test::gloveJson(Runner& runner) {
    using State = std::array<bool,FINGER_COUNT>;
    const State left{true,false,true,false,false}, right{false,true,false,false,true};
    const std::array<bool,PAIR_FINGER_COUNT> state{true,false,true,false,false,false,true,false,false,true};
    for (auto hand : {Hand::Left,Hand::Right}) {
        for (std::size_t index=0; index<5; ++index) {
            for (auto role : {GloveRole::Primary,GloveRole::Secondary}) {
                runner.run("GloveJson_Individual_"+show(hand)+"_"+show(index)+"_"+show(role), [=] {
                    auto expected = individual();
                    expected["role"] = role == GloveRole::Primary ? "primary" : "secondary";
                    expected["hand"] = hand == Hand::Left ? "left" : "right";
                    expected["index"] = index;
                    const auto parsed = glove_json::parse(expected.dump());
                    equal(parsed.role,role); equal(parsed.hand,hand); equal(parsed.index,index);
                    equal(parsed.combined,false); equal(parsed.fingers,left);
                    const auto wire = glove_json::individual(role,hand,index,left);
                    equal(Json::parse(wire).dump(),expected.dump());
                    equal(glove_json::parse(wire).fingers,left);
                });
            }
            runner.run("GloveJson_Combined_"+show(hand)+"_"+show(index), [=] {
                auto expected = combined();
                expected["hand"] = hand == Hand::Left ? "left" : "right";
                expected["index"] = index;
                const auto parsed = glove_json::parse(expected.dump());
                equal(parsed.role,GloveRole::Primary); equal(parsed.hand,hand); equal(parsed.index,index);
                equal(parsed.combined,true); equal(parsed.left,left); equal(parsed.right,right);
                const auto wire = glove_json::combinedSubmission(hand,index,state);
                equal(Json::parse(wire).dump(),expected.dump());
                equal(glove_json::parse(wire).right,right);
            });
        }
    }
    runner.run("GloveJson_InspectionSchemaIsNotSubmission", [=] {
        auto expected = combined(); expected.erase("role"); expected.erase("hand"); expected["index"]=4;
        const auto wire = glove_json::combined(4,state);
        equal(Json::parse(wire).dump(),expected.dump());
        throws<std::invalid_argument>([&] { (void)glove_json::parse(wire); });
    });
    runner.run("GloveJson_CombinedSerializerValidation", [=] {
        throws<std::invalid_argument>([&] { (void)glove_json::combinedSubmission(static_cast<Hand>(9),0,state); });
        throws<std::invalid_argument>([&] { (void)glove_json::combinedSubmission(Hand::Left,5,state); });
    });
    // Test schema validation on both shapes, separately from HTTP session conflicts.
    for (bool isCombined : {false,true}) {
        const auto base = isCombined ? combined() : individual();
        for (auto it=base.begin(); it!=base.end(); ++it) {
            const auto key=it.key();
            runner.run("GloveJson_MissingField_"+show(isCombined)+"_"+key, [=] {
                auto input=base; input.erase(key);
                throws<std::invalid_argument>([&] { (void)glove_json::parse(input.dump()); });
            });
            runner.run("GloveJson_NullField_"+show(isCombined)+"_"+key, [=] {
                auto input=base; input[key]=nullptr;
                throws<std::invalid_argument>([&] { (void)glove_json::parse(input.dump()); });
            });
        }
        for (const auto& value : std::vector<Json>{-1,5,100,0.0,1.5,true,"0",nullptr}) {
            runner.run("GloveJson_InvalidIndex_"+show(isCombined)+"_"+value.dump(), [=] {
                auto input=base; input["index"]=value;
                throws<std::invalid_argument>([&] { (void)glove_json::parse(input.dump()); });
            });
        }
        for (const std::string key : {"role","hand"}) {
            for (const auto& value : std::vector<Json>{"unknown","",1,true,nullptr}) {
                runner.run("GloveJson_InvalidIdentity_"+show(isCombined)+"_"+key+"_"+value.dump(), [=] {
                    auto input=base; input[key]=value;
                    throws<std::invalid_argument>([&] { (void)glove_json::parse(input.dump()); });
                });
            }
        }
        runner.run("GloveJson_UnknownAndMixedFields_"+show(isCombined), [=] {
            auto unknown=base; unknown["extra"]=1;
            throws<std::invalid_argument>([&] { (void)glove_json::parse(unknown.dump()); });
            auto mixed=base;
            mixed[isCombined ? "fingers" : "left"] = Json::array({0,0,0,0,0});
            throws<std::invalid_argument>([&] { (void)glove_json::parse(mixed.dump()); });
        });
        const std::vector<std::string> fields = isCombined ? std::vector<std::string>{"left","right"}
                                                         : std::vector<std::string>{"fingers"};
        for (const auto& field : fields) {
            for (const auto& invalid : std::vector<Json>{Json::array(),Json::array({1,0,0,0}),
                      Json::array({1,0,0,0,0,0}),"10000",nullptr,Json::object()}) {
                runner.run("GloveJson_InvalidArray_"+field+"_"+invalid.dump(), [=] {
                    auto input=base; input[field]=invalid;
                    throws<std::invalid_argument>([&] { (void)glove_json::parse(input.dump()); });
                });
            }
            for (const auto& value : std::vector<Json>{-1,2,true,false,1.0,0.0,"1",nullptr,Json::array({1})}) {
                runner.run("GloveJson_InvalidFinger_"+field+"_"+value.dump(), [=] {
                    // Exercise every position so validation cannot accidentally inspect only the first.
                    for (std::size_t i=0; i<5; ++i) {
                        auto input=base; input[field][i]=value;
                        throws<std::invalid_argument>([&] { (void)glove_json::parse(input.dump()); });
                    }
                });
            }
        }
    }
    runner.run("GloveJson_CombinedRequiresPrimary", [] {
        auto input=combined(); input["role"]="secondary";
        throws<std::invalid_argument>([&] { (void)glove_json::parse(input.dump()); });
    });
    const std::vector<std::string> malformed{
        "", "{", "{}", "[]", "null", "42", "true", "{\"role\":}",
        R"({"role":"secondary","role":"secondary","hand":"left","index":0,"fingers":[1,0,0,0,0]})",
        R"({"role":"primary","hand":"left","index":0,"left":[0,0,0,0,0],"right":[1,0,0,0,0],"right":[0,0,0,0,0]})",
        individual().dump()+" trailing",
        R"({"role":"secondary","hand":"left","index":0,"fingers":[{"nested":{"value":1}},0,0,0,0]})",
        std::string("{\"role\":\"")+char(0xff)+"\"}"};
    for (std::size_t i=0;i<malformed.size();++i) {
        runner.run("GloveJson_MalformedOrDuplicate_"+show(i), [&,i] {
            throws<std::invalid_argument>([&] { (void)glove_json::parse(malformed[i]); });
        });
    }
    for (const std::string value : {"application/json", " Application/JSON\t", "application/json;charset=utf-8",
              "application/json; charset=\"UTF-8\"", "APPLICATION/JSON ; CHARSET = utf-8"}) {
        runner.run("GloveJson_ContentTypeAccepted_"+show(value), [=] { equal(glove_json::isJsonContentType(value),true); });
    }
    for (const std::string value : {"", "text/plain", "application/jsonp", "application/json;", "application/json; charset=ascii",
              "application/json; other=utf-8", "application/json; charset=utf-8; charset=utf-8", "application/json\r\n", "application/json; charset"}) {
        runner.run("GloveJson_ContentTypeRejected_"+show(value), [=] { equal(glove_json::isJsonContentType(value),false); });
    }
    runner.run("GloveJson_NoRoleFilter", [] { equal(glove_json::roleFilter("/gloves").has_value(),false); });
    for (auto role : {GloveRole::Primary,GloveRole::Secondary}) {
        runner.run("GloveJson_ValidRoleFilter_"+show(role), [=] {
            const auto parsed=glove_json::roleFilter(role == GloveRole::Primary ? "/gloves?role=primary" : "/gloves?role=secondary");
            check(parsed.has_value(),"role filter exists"); equal(*parsed,role);
        });
    }
    runner.run("GloveJson_PercentEncodedRoleFilter", [] {
        const auto parsed=glove_json::roleFilter("/gloves?%72ole=pr%69mary");
        check(parsed.has_value(),"decoded role exists"); equal(*parsed,GloveRole::Primary);
    });
    for (const std::string query : {"", "role", "role=", "role=left", "role=Primary", "hand=left", "role=primary&role=secondary",
             "role=primary&", "role=primary&x=1", "role=primary=", "role=pri+mary", "%ro=primary", "role=%", "role=%GG", "role=primary%00"}) {
        runner.run("GloveJson_InvalidRoleFilter_"+show(query), [=] {
            throws<std::invalid_argument>([&] { (void)glove_json::roleFilter("/gloves?"+query); });
        });
    }
}
