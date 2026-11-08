/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_WHOCONVERSATION_H
#define PLAYERBOTS_WHOCONVERSATION_H

#include <array>
#include <cctype>
#include <string>
#include <string_view>

// ExternalEventHelper::ParseChatCommand matches the longest leading trigger
// name, so a Party sentence such as "who starts this quest?" reaches the
// "who" trigger with the parameter "starts this quest?". This predicate
// recognises that natural-language form so the who action can decline it.
//
// It is deliberately narrow. These remain who commands:
//   who                      (spec/gear summary)
//   who <skill>              (exact skill name, e.g. "who mining")
//   who <item link or name>  (trade query; any |Hitem: link always counts)
// Only a parameter whose FIRST word continues a sentence ("who starts ...",
// "who is ...", "who drops ...") is treated as conversation.
// Mirrored by mod-llm-chatter (LLMChatterCommandPolicy.h, chatter_group.py);
// both are tested against tools/tests/data/who_routing_cases.json there.
namespace WhoConversation
{
inline std::string ToLowerAscii(std::string_view text)
{
    std::string out(text);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

inline bool IsConversationalParam(std::string_view param)
{
    static constexpr std::array<std::string_view, 34> kSentenceWords = {
        "starts", "start", "drops", "drop", "dropped", "is", "are", "was", "were", "knows", "know",
        "has", "have", "had", "can", "could", "wants", "want", "needs", "need", "got", "gets",
        "did", "does", "do", "will", "would", "should", "else", "pulled", "pulls", "killed",
        "gives", "sells"};

    size_t const begin = param.find_first_not_of(" \t");
    if (begin == std::string_view::npos)
        return false;
    std::string const lower = ToLowerAscii(param.substr(begin));
    if (lower.find("|hitem:") != std::string::npos || lower.find("|hquest:") != std::string::npos)
        return false;
    size_t end = 0;
    while (end < lower.size() && std::isalpha(static_cast<unsigned char>(lower[end])))
        ++end;
    std::string_view const first(lower.data(), end);
    for (std::string_view word : kSentenceWords)
        if (first == word)
            return true;
    return false;
}
} // namespace WhoConversation

#endif
