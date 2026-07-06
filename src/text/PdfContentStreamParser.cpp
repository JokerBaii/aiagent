/**
 * @file PdfContentStreamParser.cpp
 * @brief PDF 内容流文本解析实现。
 */

#include "cc/text/PdfContentStreamParser.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include <zlib.h>

namespace cc {
namespace {

constexpr std::size_t kMaxDecodedStreamBytes = static_cast<std::size_t>(4U) * 1024U * 1024U;
constexpr std::size_t kMaxExtractedTextBytes = static_cast<std::size_t>(1024U) * 1024U;

struct PdfStreamSlice {
    std::string_view dictionary;
    std::string_view bytes;
};

[[nodiscard]] bool hasPdfHeader(std::string_view bytes) {
    const auto headerWindow = bytes.substr(0U, std::min<std::size_t>(bytes.size(), 1024U));
    return headerWindow.find("%PDF") != std::string_view::npos;
}

[[nodiscard]] bool isWhitespace(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] int hexValue(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    if (value >= static_cast<unsigned char>('0') && value <= static_cast<unsigned char>('9')) {
        return static_cast<int>(value - static_cast<unsigned char>('0'));
    }
    if (value >= static_cast<unsigned char>('A') && value <= static_cast<unsigned char>('F')) {
        return static_cast<int>(value - static_cast<unsigned char>('A') + 10U);
    }
    if (value >= static_cast<unsigned char>('a') && value <= static_cast<unsigned char>('f')) {
        return static_cast<int>(value - static_cast<unsigned char>('a') + 10U);
    }
    return -1;
}

void appendUtf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        output.push_back(static_cast<char>(codepoint));
        return;
    }
    if (codepoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        return;
    }
    output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
    output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
    output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
}

[[nodiscard]] std::string decodeUtf16Be(const std::vector<unsigned char>& bytes) {
    std::string text;
    for (std::size_t index = 2U; index + 1U < bytes.size(); index += 2U) {
        const auto codepoint = (static_cast<std::uint32_t>(bytes.at(index)) << 8U) |
                               static_cast<std::uint32_t>(bytes.at(index + 1U));
        appendUtf8(text, codepoint);
    }
    return text;
}

[[nodiscard]] std::string bytesToText(const std::vector<unsigned char>& bytes) {
    if (bytes.size() >= 2U && bytes.at(0U) == 0xFEU && bytes.at(1U) == 0xFFU) {
        return decodeUtf16Be(bytes);
    }
    std::string text;
    text.reserve(bytes.size());
    for (const auto byte : bytes) {
        text.push_back(static_cast<char>(byte));
    }
    return text;
}

[[nodiscard]] std::string decodeLiteralString(std::string_view content, std::size_t& position) {
    std::vector<unsigned char> bytes;
    int depth = 1;
    ++position;
    while (position < content.size() && depth > 0) {
        const auto ch = content.at(position++);
        if (ch == '\\' && position < content.size()) {
            const auto escaped = content.at(position++);
            switch (escaped) {
            case 'n':
                bytes.push_back('\n');
                break;
            case 'r':
                bytes.push_back('\r');
                break;
            case 't':
                bytes.push_back('\t');
                break;
            case 'b':
                bytes.push_back('\b');
                break;
            case 'f':
                bytes.push_back('\f');
                break;
            case '(':
            case ')':
            case '\\':
                bytes.push_back(static_cast<unsigned char>(escaped));
                break;
            case '\n':
                break;
            case '\r':
                if (position < content.size() && content.at(position) == '\n') {
                    ++position;
                }
                break;
            default: {
                if (escaped < '0' || escaped > '7') {
                    bytes.push_back(static_cast<unsigned char>(escaped));
                    break;
                }
                int octal = escaped - '0';
                for (int count = 0; count < 2 && position < content.size(); ++count) {
                    const auto next = content.at(position);
                    if (next < '0' || next > '7') {
                        break;
                    }
                    octal = (octal * 8) + (next - '0');
                    ++position;
                }
                bytes.push_back(static_cast<unsigned char>(octal & 0xFF));
                break;
            }
            }
            continue;
        }
        if (ch == '(') {
            ++depth;
            bytes.push_back(static_cast<unsigned char>(ch));
            continue;
        }
        if (ch == ')') {
            --depth;
            if (depth > 0) {
                bytes.push_back(static_cast<unsigned char>(ch));
            }
            continue;
        }
        bytes.push_back(static_cast<unsigned char>(ch));
    }
    return bytesToText(bytes);
}

[[nodiscard]] std::string decodeHexString(std::string_view content, std::size_t& position) {
    std::vector<unsigned char> bytes;
    ++position;
    int highNibble = -1;
    while (position < content.size()) {
        const auto ch = content.at(position++);
        if (ch == '>') {
            break;
        }
        if (isWhitespace(ch)) {
            continue;
        }
        const auto value = hexValue(ch);
        if (value < 0) {
            continue;
        }
        if (highNibble < 0) {
            highNibble = value;
            continue;
        }
        bytes.push_back(static_cast<unsigned char>((highNibble << 4U) | value));
        highNibble = -1;
    }
    if (highNibble >= 0) {
        bytes.push_back(static_cast<unsigned char>(highNibble << 4U));
    }
    return bytesToText(bytes);
}

void appendExtractedText(std::string& output, const std::string& text) {
    if (text.empty() || output.size() >= kMaxExtractedTextBytes) {
        return;
    }
    if (!output.empty() && !isWhitespace(output.back())) {
        output.push_back(' ');
    }
    const auto remaining = kMaxExtractedTextBytes - output.size();
    output.append(text.substr(0U, remaining));
}

[[nodiscard]] std::string textFromContentStream(std::string_view content) {
    std::string output;
    bool inTextObject = false;
    for (std::size_t position = 0U; position < content.size();) {
        const auto ch = content.at(position);
        if (ch == '(' && inTextObject) {
            appendExtractedText(output, decodeLiteralString(content, position));
            continue;
        }
        if (ch == '<' && inTextObject && position + 1U < content.size() &&
            content.at(position + 1U) != '<') {
            appendExtractedText(output, decodeHexString(content, position));
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(ch)) != 0) {
            const auto start = position;
            while (position < content.size() &&
                   std::isalpha(static_cast<unsigned char>(content.at(position))) != 0) {
                ++position;
            }
            const auto word = content.substr(start, position - start);
            if (word == "BT") {
                inTextObject = true;
            } else if (word == "ET") {
                inTextObject = false;
                if (!output.empty() && output.back() != '\n') {
                    output.push_back('\n');
                }
            }
            continue;
        }
        ++position;
    }
    return output;
}

[[nodiscard]] std::string inflateFlateStream(std::string_view compressed) {
    if (compressed.empty() || compressed.size() > kMaxDecodedStreamBytes) {
        return {};
    }
    std::vector<unsigned char> input;
    input.reserve(compressed.size());
    for (const auto ch : compressed) {
        input.push_back(static_cast<unsigned char>(ch));
    }

    z_stream stream{};
    stream.next_in = input.data();
    stream.avail_in = static_cast<uInt>(input.size());
    if (inflateInit(&stream) != Z_OK) {
        return {};
    }

    std::array<unsigned char, 4096> buffer{};
    std::string output;
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = buffer.size() - static_cast<std::size_t>(stream.avail_out);
        if (produced > 0U) {
            for (std::size_t index = 0U; index < produced; ++index) {
                output.push_back(static_cast<char>(buffer.at(index)));
            }
        }
        if (output.size() > kMaxDecodedStreamBytes) {
            inflateEnd(&stream);
            return {};
        }
    }
    inflateEnd(&stream);
    return status == Z_STREAM_END ? output : std::string{};
}

[[nodiscard]] std::string decodeStream(PdfStreamSlice stream) {
    if (stream.dictionary.find("/FlateDecode") != std::string_view::npos) {
        return inflateFlateStream(stream.bytes);
    }
    return std::string{stream.bytes};
}

[[nodiscard]] std::size_t contentStartAfterStreamMarker(std::string_view pdfBytes,
                                                        std::size_t streamPosition) {
    auto contentStart = streamPosition + std::string_view{"stream"}.size();
    if (contentStart + 1U < pdfBytes.size() && pdfBytes.at(contentStart) == '\r' &&
        pdfBytes.at(contentStart + 1U) == '\n') {
        return contentStart + 2U;
    }
    if (contentStart < pdfBytes.size() &&
        (pdfBytes.at(contentStart) == '\n' || pdfBytes.at(contentStart) == '\r')) {
        return contentStart + 1U;
    }
    return contentStart;
}

} // namespace

std::string PdfContentStreamParser::extractText(std::string_view pdfBytes) const {
    if (!hasPdfHeader(pdfBytes)) {
        return {};
    }

    std::string text;
    std::size_t searchFrom = 0U;
    while (searchFrom < pdfBytes.size()) {
        const auto streamPosition = pdfBytes.find("stream", searchFrom);
        if (streamPosition == std::string_view::npos) {
            break;
        }
        const auto dictionaryEnd = pdfBytes.rfind(">>", streamPosition);
        const auto dictionaryStart = dictionaryEnd == std::string_view::npos
                                         ? std::string_view::npos
                                         : pdfBytes.rfind("<<", dictionaryEnd);
        const auto contentStart = contentStartAfterStreamMarker(pdfBytes, streamPosition);
        const auto streamEnd = pdfBytes.find("endstream", contentStart);
        if (streamEnd == std::string_view::npos) {
            break;
        }
        if (dictionaryStart != std::string_view::npos && dictionaryEnd != std::string_view::npos &&
            dictionaryStart < dictionaryEnd) {
            const auto dictionary =
                pdfBytes.substr(dictionaryStart, dictionaryEnd + 2U - dictionaryStart);
            appendExtractedText(
                text, textFromContentStream(decodeStream(
                          {.dictionary = dictionary,
                           .bytes = pdfBytes.substr(contentStart, streamEnd - contentStart)})));
        }
        searchFrom = streamEnd + std::string_view{"endstream"}.size();
    }
    return text;
}

} // namespace cc
