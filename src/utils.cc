#include <string>
#include <vector>

#include "utils.h"

std::vector<std::string> split(const std::string& str, const std::string& sep) {
    std::vector<std::string> result;
    std::size_t start = 0;
    std::size_t pos;

    while ((pos = str.find(sep, start)) != std::string::npos) {
        if (pos != start) {
            result.push_back(str.substr(start, pos - start));
        }
        start = pos + sep.size();
    }

    if (start < str.size()) {
        result.push_back(str.substr(start));
    }

    return result;
}
