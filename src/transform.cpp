#define PUGIXML_NO_EXCEPTIONS
#define PUGIXML_HEADER_ONLY

#include <string>
#include "pugixml/src/pugixml.hpp"
#include "json/src/json.hpp"

using json = nlohmann::json;
using string = std::string;
using xquery = pugi::xpath_query;
using nodeset = pugi::xpath_node_set;

template<typename T>
void walk(T& doc, json& n, json& output, string key);

const string NUMBER = "number";
const string STRING = "string";

inline bool string_contains(string to_check, string prefix) {
    return to_check.size() >= prefix.size() && to_check.compare(0, prefix.size(), prefix) == 0;
}

inline string get_return_type(string& path) {
    if (string_contains(path, "count(") ||
        string_contains(path, "sum(") ||
        string_contains(path, "number(") ||
        string_contains(path, "ceiling(") ||
        string_contains(path, "floor(") ||
        string_contains(path, "round(")
    ) return NUMBER;
    return STRING;
}

template<typename T>
string seek_single_string(T& xnode, json& j) {
    string path = j;
    if (path.empty()) {
        return "";
    } else if (path.find("#") != std::string::npos) {
        return path.substr(1, path.size());
    } else {
        xquery query(path.c_str());
        return query.evaluate_string(xnode);
    }
}

template<typename T>
double seek_single_number(T& xnode, json& j) {
    string path = j;
    xquery query(path.c_str());
    return query.evaluate_number(xnode);
}

template<typename T>
json seek_array(T& doc, json& node) {
    string base_path = node[0];
    xquery q(base_path.c_str());
    pugi::xpath_node_set nodes = q.evaluate_node_set(doc);
    json tmp = json::array();

    for (size_t i = 0; i < nodes.size(); ++i) {
        pugi::xpath_node n = nodes[i];
        auto inner = node[1];

        if (inner.is_object()) {
            json obj = json({});
            for (json::iterator it = inner.begin(); it != inner.end(); ++it) {
                walk(n, it.value(), obj, it.key());
            }

            tmp.push_back(obj);
        } else if (inner.is_string()) {
            string path = inner;
            string type = get_return_type(path);
            if (type == NUMBER) {
                tmp.push_back(seek_single_number(n, inner));
            }
            if (type == STRING) {
                tmp.push_back(seek_single_string(n, inner));
            }
        }
    }

    return tmp;
}

template<typename T>
json seek_object(T& doc, json& node) {
    auto output = json({});
    for (json::iterator it = node.begin(); it != node.end(); ++it) {
        string key = it.key();
        walk(doc, *it, output, key);
    }
    return output;
}

template<typename T>
void walk(T& doc, json& n, json& output, string key) {
    if (n.is_array() ) {
        output[key] = seek_array(doc, n);
    } else if (n.is_object()) {
        output[key] = seek_object(doc, n);
    } else if (n.is_string()) {
        string path = n;
        string type = get_return_type(path);
        if (type == NUMBER) {
            output[key] = seek_single_number(doc, n);
        }
        if (type == STRING) {
            output[key] = seek_single_string(doc, n);
        }
    }
}

string transform(string xml, string fmt) {
    pugi::xml_document doc;
    if (!doc.load_string(xml.c_str())) {
        return "";
    }

    auto j = json::parse(fmt);
    json result;

    for (json::iterator it = j.begin(); it != j.end(); ++it) {
        string key = it.key();
        auto& node = j[key];
        walk(doc, node, result, key);
    }

    return result.dump();
}