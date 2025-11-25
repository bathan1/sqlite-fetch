#include <vector>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>

extern "C" {

using jsoncons::json;

/*
 * walk:
 *   Given raw JSON text and a JSONPath expression,
 *   returns an array of JSON strings (each malloc'ed).
 *
 * Caller must:
 *   - free(each char*)
 *   - free(out_items)
 */
int walk(
    const char *json_buf,
    const char *path,
    char ***out_items,
    size_t *out_count
) {
    using jsoncons::jsonpath::json_query;
    using jsoncons::encode_json;

    try {
        json doc = json::parse(json_buf);
        json result = json_query(doc, path);

        std::vector<std::string> items;

        if (result.is_array()) {
            for (const auto& v : result.array_range()) {
                std::string s;
                encode_json(v, s);
                items.push_back(s);
            }
        } else {
            std::string s;
            encode_json(result, s);
            items.push_back(s);
        }

        size_t n = items.size();
        char **arr = (char **) malloc(sizeof(char*) * n);

        for (size_t i = 0; i < n; i++) {
            const std::string& s = items[i];
            arr[i] = (char *)malloc(s.size() + 1);
            memcpy(arr[i], s.c_str(), s.size() + 1);
        }

        *out_items = arr;
        *out_count = n;
        return 0;
    } catch (...) {
        *out_items = nullptr;
        *out_count = 0;
        return -1;
    }
}

} // extern "C"

