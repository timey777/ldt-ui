#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "attribute.h"
#include "ast_snippet.h"
#include "ldt_export.h"
struct ASTNode;
/*
  简要说明：
  - 无状态：Lexer 和 Parser 都不保留外部状态；每次 parse 都从输入字符串重新 token 化并驱动解析器。
  - 可扩展：实现 IParser 接口并在 ParserRegistry 注册即可新增语法块，比如 @event{}、@script{}。
  - 设计模式：Singleton (ParserRegistry), Strategy (IParser variants), Visitor (AST traversal), Factory (registry returns parser).
*/
namespace ldt {
    // ------------------- Token & Lazy TokenStream （按需词法，支持多种 LexerMode） -------------------
    enum class TokenType {
        Identifier,
        String,
        Number,
        Symbol,
        Keyword,   // e.g. @style, @layout, @import
        Eof
    };

    struct Token {
        TokenType type;
        std::string text;
        size_t line = 1, col = 1;
    };

    class TokenStream {
    public:
        enum class LexerMode { Default, Style, Layout };
        explicit TokenStream(const std::string& s);

        // peek lookahead-th token (0=current). Will lex tokens on demand.
        const Token& peek(size_t lookahead = 0);
        // consume and return next token
        Token next();
        // helper to check EOF quickly
        bool eof();

        // 控制词法器模式（栈式，便于嵌套切换）
        void pushLexer(LexerMode m);
        void popLexer();

    private:
        std::string src;
        size_t charPos;
        size_t line, col;
        std::vector<Token> tokens;
        size_t cur;
        static Token eofTok;

        std::vector<LexerMode> modeStack;

        void ensureTokens(size_t need);

        void advanceChar();
        void skipWhitespace();
        bool peekIsDigit() const;
        bool isSymbolChar(char c) const;

        // 按当前模式分发
        Token lexNext();

        // 各模式词法器声明（实现移至 cpp）
        Token lexNextDefault();
        Token lexNextStyle();
        Token lexNextLayout();
    };

    // eof token 定义在 cpp 中

    // RAII 辅助，用于在解析器中临时切换词法器并在退出时恢复
    struct LexerGuard {
        TokenStream& ts;
        LexerGuard(TokenStream& s, TokenStream::LexerMode m);
        ~LexerGuard();
    };


    // Visitor 接口
    class LDT_API IVisitor {
    public:
        virtual ~IVisitor() = default;
        virtual void visit(std::shared_ptr<ASTNode> node, int depth) = 0;
    };

    // AST 打印实现（Visitor）
    class LDT_API ASTPrinter : public IVisitor {
    public:
        void visit(std::shared_ptr<ASTNode> node, int depth) override;
    private:
        void printIndent(int d);
        void printAttr(const Attribute& a);
    };

    // ------------------- Parser 抽象与 Registry (Singleton) -------------------
    // 修改 IParser 接口：使用 TokenStream（peek/next）而不是 std::vector+index
    class IParser {
    public:
        virtual ~IParser() = default;
        // 判断是否能在当前 TokenStream 的当前位置匹配当前语法块（无状态）
        virtual bool match(TokenStream& ts) const = 0;
        // 解析，从当前流位置开始，返回 ASTNode（会消费相应的 token）
        virtual std::shared_ptr<ASTNode> parse(TokenStream& ts) const = 0;
    };

    class ParserRegistry {
    public:
        static ParserRegistry& instance();
        void registerParser(const std::string& name, std::shared_ptr<IParser> p);
        // 找第一个 match 的 parser（顺序敏感 —— 注册顺序可以控制优先级）
        std::shared_ptr<IParser> findMatching(TokenStream& ts) const;
    private:
        ParserRegistry() = default;
        std::vector<std::shared_ptr<IParser>> parsers;
    };

    // ------------------- 小工具：token 游标操作 -------------------
    static const Token& tokAt(const std::vector<Token>& toks, size_t idx);
    static bool isSym(const Token& t, const std::string& s);
    static bool isId(const Token& t, const std::string& s);

    // ------------------- 各具体 Parser 声明 （实现移至 cpp） -------------------

    // @import("s.ldtstyle")
    class ImportParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    };

    // @style { ... }  // CSS-like简化版本： selector { key: value; ... }
    class StyleParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    };

    // @layout { ... }  // very similar structure to style
    class LayoutParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    };

    // ContentParser：解析类似 panel:Main ( ... ) { ... }
    class ContentParser : public IParser {
    public:
        bool match(TokenStream& ts) const override;
        std::shared_ptr<ASTNode> parse(TokenStream& ts) const override;
    private:
        void parseAttrList(TokenStream& ts, std::shared_ptr<ASTNode>& node) const;
        void parseClass(ASTNode& node, const std::string& classStr) const;
    };

    // ------------------- 顶层 LDTParser -------------------
    class LDT_API LDTParser {
    public:
        AstSnippet parse(const std::string& input) const;

    private:
        std::shared_ptr<ASTNode> parseRoot(const std::string& input) const;
    };
}