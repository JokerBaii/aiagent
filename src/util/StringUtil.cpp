/**
 * @file StringUtil.cpp
 * @brief 小型字符串工具实现。
 */

#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace cc::util {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string trim(std::string value) {
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
                    return !isSpace(static_cast<unsigned char>(ch));
                }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](char ch) { return !isSpace(static_cast<unsigned char>(ch)); })
                    .base(),
                value.end());
    return value;
}

bool contains(std::string_view text, std::string_view needle) {
    return text.find(needle) != std::string_view::npos;
}

bool containsLower(const std::string& text, const std::string& needle) {
    return contains(lowerAscii(text), lowerAscii(needle));
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string join(const std::vector<std::string>& values, std::string_view separator) {
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0U) {
            output << separator;
        }
        output << values[index];
    }
    return output.str();
}

} // namespace cc::util
