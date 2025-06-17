#pragma once

#include <lang/core.hpp>

#include <vector>

LANG_NAMESPACE_BEGIN

enum class TokenType {
    Invalid = 0,

    Identifier, // foo
    String,     // "foo"
    Number,     // 123.4
    Puncuation, // +=/*...
};

constexpr auto InvalidTokenIndex = ~0u;

struct Token
{
    TokenType type = TokenType::Invalid;
    std::string_view raw;
    union {
        struct {
            bool is_at;
            std::string_view value;
        } identifier;
        struct {
            bool is_raw;
            std::string_view value;
        } string;
        struct {
            u32 base;
            std::string_view raw;
        } number;
        struct {
            u8 value;
            u32 matching = InvalidTokenIndex;
        } punctuation;
    };
};

constexpr auto InvalidToken = Token { .type = TokenType::Invalid };

struct TokenList
{
    std::vector<Token> tokens;

    struct TokenIterator begin();
    struct TokenIterator end();
};

struct TokenIterator
{
    TokenList* list;
    u32 current;

    bool is_at_end() const { return current >= list->tokens.size(); }
    u32 index() const { return is_at_end() ? InvalidTokenIndex : current; }

    Token advance() { return is_at_end() ? InvalidToken : list->tokens[current++]; }

    Token peek() const    { return is_at_end() ? InvalidToken : list->tokens[current];   }
    Token peek_next() const
    {
        auto next = current + 1;
        return next >= list->tokens.size() ? InvalidToken : list->tokens[next];
    }

    TokenIterator matching_pair_for(Token token) const
    {
        if (token.type != TokenType::Puncuation) return list->end();
        auto i = token.punctuation.matching;
        return TokenIterator{list, i};
    }

    operator bool() const noexcept
    {
        return !is_at_end();
    }

    bool operator==(const TokenIterator&& other) const
    {
        return index() == other.index();
    }

    Token operator*() const
    {
        return list->tokens[current];
    }

    Token* operator->() const
    {
        return &list->tokens[current];
    }

    TokenIterator& operator++()
    {
        current++;
        return *this;
    }
};

inline
TokenIterator TokenList::begin()
{
    return TokenIterator{this, 0};
}

inline
TokenIterator TokenList::end()
{
    return TokenIterator{this, u32(tokens.size())};
}

TokenList scan(std::string_view);

LANG_NAMESPACE_END
