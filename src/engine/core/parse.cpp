#include "parse.h"
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <atomic>
#include "ast_node.h"

using namespace std;
using namespace ldt;

// 辅助函数：根据 Token 类型智能转换为合适的 Attribute 类型
static Attribute tokenToAttribute(const ldt::Token& tok) {
    if (tok.type == ldt::TokenType::Number) {
        // 判断是整数还是浮点数
        const string& text = tok.text;
        // 如果数字包含单位（例如 '%' 或 'px' 等字母），不要当作纯数字解析，保留为字符串
        bool hasUnit = false;
        if (text.find('%') != string::npos) hasUnit = true;
        if (!text.empty() && isalpha((unsigned char)text.back())) hasUnit = true;
        if (hasUnit) {
            return Attribute(text);
        }
        if (text.find('.') != string::npos || text.find('e') != string::npos || text.find('E') != string::npos) {
            // 包含小数点或科学计数法 -> float
            return Attribute(std::stof(text));
        } else {
            // 纯整数 -> int
            return Attribute(std::stoi(text));
        }
    }
    else if (tok.type == TokenType::String) {
        // 字符串（去除引号）
        string str = tok.text;
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
            return Attribute(str.substr(1, str.size() - 2));
        }
        return Attribute(str);
    }
    else {
        // Identifier 或其他 -> 先尝试识别布尔值 true/false（不区分大小写），否则保留为字符串（如 "auto", "100%"）
        string txt = tok.text;
        if (txt == "true") return Attribute(true);
        if (txt == "false") return Attribute(false);
        return Attribute(txt);
    }
}

// (Stable id generation moved to resolved_tree.cpp)

// ----------------- TokenStream 实现 -----------------
ldt::Token ldt::TokenStream::eofTok = { ldt::TokenType::Eof, "", 0, 0 };

ldt::TokenStream::TokenStream(const string& s)
    : src(s), charPos(0), line(1), col(1), cur(0)
{
    modeStack.push_back(LexerMode::Default);
}

const ldt::Token& TokenStream::peek(size_t lookahead) {
    ensureTokens(cur + lookahead + 1);
    if (cur + lookahead < tokens.size()) return tokens[cur + lookahead];
    return eofTok;
}

Token TokenStream::next() {
    ensureTokens(cur + 1);
    if (cur < tokens.size()) return tokens[cur++];
    return eofTok;
}

bool TokenStream::eof() {
    return peek().type == TokenType::Eof;
}

void TokenStream::pushLexer(LexerMode m) { modeStack.push_back(m); }
void TokenStream::popLexer() { if (modeStack.size() > 1) modeStack.pop_back(); }

void TokenStream::ensureTokens(size_t need) {
    while (tokens.size() < need) {
        tokens.push_back(lexNext());
    }
}

void TokenStream::advanceChar() {
    if (charPos < src.size()) {
        if (src[charPos] == '\n') { line++; col = 1; }
        else col++;
        charPos++;
    }
}
void TokenStream::skipWhitespace() {
    while (charPos < src.size()) {
        if (isspace((unsigned char)src[charPos])) { advanceChar(); continue; }
        if (src[charPos] == '/' && charPos + 1 < src.size() && src[charPos + 1] == '/') {
            charPos += 2; col += 2;
            while (charPos < src.size() && src[charPos] != '\n') { charPos++; col++; }
            if (charPos < src.size() && src[charPos] == '\n') advanceChar();
            continue;
        }
        if (src[charPos] == '/' && charPos + 1 < src.size() && src[charPos + 1] == '*') {
            charPos += 2; col += 2;
            while (charPos < src.size()) {
                if (charPos + 1 < src.size() && src[charPos] == '*' && src[charPos + 1] == '/') {
                    charPos += 2; col += 2; break;
                }
                if (src[charPos] == '\n') { line++; col = 1; charPos++; }
                else { charPos++; col++; }
            }
            continue;
        }
        break;
    }
}
bool TokenStream::peekIsDigit() const {
    return (charPos + 1 < src.size()) && isdigit((unsigned char)src[charPos + 1]);
}
bool TokenStream::isSymbolChar(char c) const {
    static const string syms = R"(@{}()[]=:,.;)"; // 与之前保持一致（# 放到 style lexer 处理）
    return syms.find(c) != string::npos;
}

Token TokenStream::lexNext() {
    switch (modeStack.back()) {
    case LexerMode::Style:  return lexNextStyle();
    case LexerMode::Layout: return lexNextLayout();
    default:                return lexNextDefault();
    }
}

Token TokenStream::lexNextDefault() {
    skipWhitespace();
    size_t startLine = line, startCol = col;
    if (charPos >= src.size()) return Token{ TokenType::Eof, "", startLine, startCol };
    char c = src[charPos];
    if (c == '@') {
        size_t start = charPos;
        advanceChar(); // '@'
        while (charPos < src.size() && isalpha((unsigned char)src[charPos])) advanceChar();
        return { TokenType::Keyword, src.substr(start, charPos - start), startLine, startCol };
    }
    if (c == '.') {
        size_t start = charPos;
        advanceChar();
        while (charPos < src.size() && (isalnum((unsigned char)src[charPos]) || src[charPos] == '_' || src[charPos] == '-')) advanceChar();
        return { TokenType::Identifier, src.substr(start, charPos - start), startLine, startCol };
    }
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = charPos;
        while (charPos < src.size() && (isalnum((unsigned char)src[charPos]) || src[charPos] == '_' || src[charPos] == '-')) advanceChar();
        return { TokenType::Identifier, src.substr(start, charPos - start), startLine, startCol };
    }
    if (c == '"') {
        advanceChar();
        std::string val;
        while (charPos < src.size() && src[charPos] != '"') {
            if (src[charPos] == '\\' && charPos + 1 < src.size()) {
                char nextC = src[charPos + 1];
                if (nextC == 'n') val += '\n';
                else if (nextC == 'r') val += '\r';
                else if (nextC == 't') val += '\t';
                else if (nextC == '"') val += '"';
                else if (nextC == '\\') val += '\\';
                else val += nextC;
                charPos += 2; col += 2;
            }
            else {
                val += src[charPos];
                advanceChar();
            }
        }
        if (charPos < src.size() && src[charPos] == '"') advanceChar();
        return { TokenType::String, val, startLine, startCol };
    }
    if (isdigit((unsigned char)c) || (c == '-' && peekIsDigit())) {
        size_t start = charPos;
        if (src[charPos] == '-') advanceChar();
        while (charPos < src.size() && (isdigit((unsigned char)src[charPos]) || src[charPos] == '.')) advanceChar();
        if (charPos < src.size() && (isalpha((unsigned char)src[charPos]) || src[charPos] == '%')) {
            while (charPos < src.size() && (isalnum((unsigned char)src[charPos]) || src[charPos] == '%')) advanceChar();
        }
        return { TokenType::Number, src.substr(start, charPos - start), startLine, startCol };
    }
    if (isSymbolChar(c)) {
        string s(1, c);
        advanceChar();
        return { TokenType::Symbol, s, startLine, startCol };
    }
    string s(1, c);
    advanceChar();
    return { TokenType::Symbol, s, startLine, startCol };
}

Token TokenStream::lexNextStyle() {
    skipWhitespace();
    size_t startLine = line, startCol = col;
    if (charPos >= src.size()) return Token{ TokenType::Eof, "", startLine, startCol };
    char c = src[charPos];
    if (c == '@') {
        size_t start = charPos;
        advanceChar();
        while (charPos < src.size() && isalpha((unsigned char)src[charPos])) advanceChar();
        return { TokenType::Keyword, src.substr(start, charPos - start), startLine, startCol };
    }
    // 支持 .classname 选择器
    if (c == '.') {
        size_t start = charPos;
        advanceChar();
        while (charPos < src.size() && (isalnum((unsigned char)src[charPos]) || src[charPos] == '_' || src[charPos] == '-')) advanceChar();
        return { TokenType::Identifier, src.substr(start, charPos - start), startLine, startCol };
    }
    if (c == '#') {
        size_t start = charPos;
        advanceChar();
        // 支持类似 .classname 的规则，允许字母数字、下划线和连字符
        while (charPos < src.size() && (isalnum((unsigned char)src[charPos]) || src[charPos] == '_' || src[charPos] == '-')) advanceChar();
        return { TokenType::Identifier, src.substr(start, charPos - start), startLine, startCol };
    }
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = charPos;
        while (charPos < src.size() && (isalnum((unsigned char)src[charPos]) || src[charPos] == '_' || src[charPos] == '-')) advanceChar();
        return { TokenType::Identifier, src.substr(start, charPos - start), startLine, startCol };
    }
    if (c == '"') {
        advanceChar();
        std::string val;
        while (charPos < src.size() && src[charPos] != '"') {
            if (src[charPos] == '\\' && charPos + 1 < src.size()) {
                char nextC = src[charPos + 1];
                if (nextC == 'n') val += '\n';
                else if (nextC == 'r') val += '\r';
                else if (nextC == 't') val += '\t';
                else if (nextC == '"') val += '"';
                else if (nextC == '\\') val += '\\';
                else val += nextC;
                charPos += 2; col += 2;
            }
            else {
                val += src[charPos];
                advanceChar();
            }
        }
        if (charPos < src.size() && src[charPos] == '"') advanceChar();
        return { TokenType::String, val, startLine, startCol };
    }
    if (isdigit((unsigned char)c) || (c == '-' && peekIsDigit())) {
        size_t start = charPos;
        if (src[charPos] == '-') advanceChar();
        while (charPos < src.size() && (isdigit((unsigned char)src[charPos]) || src[charPos] == '.')) advanceChar();
        if (charPos < src.size() && (isalpha((unsigned char)src[charPos]) || src[charPos] == '%')) {
            while (charPos < src.size() && (isalnum((unsigned char)src[charPos]) || src[charPos] == '%')) advanceChar();
        }
        return { TokenType::Number, src.substr(start, charPos - start), startLine, startCol };
    }
    if (isSymbolChar(c)) {
        string s(1, c);
        advanceChar();
        return { TokenType::Symbol, s, startLine, startCol };
    }
    string s(1, c);
    advanceChar();
    return { TokenType::Symbol, s, startLine, startCol };
}

Token TokenStream::lexNextLayout() {
    // Layout 选择器应该支持与 style 类似的 id/class 语法（如 #id 和 .class），
    // 因此直接复用 style 词法器的处理逻辑以保持一致性。
    return lexNextStyle();
}

// ----------------- LexerGuard -----------------
LexerGuard::LexerGuard(TokenStream& s, TokenStream::LexerMode m) : ts(s) { ts.pushLexer(m); }
LexerGuard::~LexerGuard() { ts.popLexer(); }

// ----------------- ASTPrinter -----------------
void ASTPrinter::visit(shared_ptr<ASTNode> node, int depth) {
    printIndent(depth);
    cout << node->type;
    if (!node->attrs.empty()) {
        cout << " { ";
        bool first = true;
        for (auto& kv : node->attrs) {
            if (!first) cout << ", ";
            cout << kv.first << ":";
            printAttr(kv.second);
            first = false;
        }
        cout << " }";
    }
    cout << "\n";
    for (auto& c : node->children) visit(c, depth + 1);
}
void ASTPrinter::printIndent(int d) {
    for (int i = 0; i < d; i++) cout << "  ";
}
void ASTPrinter::printAttr(const Attribute& a) {
    if (a.isNull()) { cout << "null"; return; }
    if (a.isBool()) { cout << (a.as<bool>() ? "true" : "false"); return; }
    if (a.isInt()) { cout << a.as<int>(); return; }
    if (a.isFloat()) { cout << a.as<float>(); return; }
    if (a.isString()) { cout << '"' << a.as<std::string>() << '"'; return; }
    if (a.isArray()) {
        const auto* arr = a.asArrayPtr();
        cout << "[";
        for (size_t i = 0; i < arr->size(); ++i) {
            if (i) cout << ", ";
            cout << (*arr)[i];
        }
        cout << "]";
        return;
    }
    if (a.isObject()) {
        const auto* obj = a.asObjectPtr();
        cout << "{";
        bool first = true;
        for (const auto& kv : *obj) {
            if (!first) cout << ", ";
            cout << kv.first << ": ";
            printAttr(*kv.second);
            first = false;
        }
        cout << "}";
        return;
    }
    cout << "<unknown>";
}

// ----------------- ParserRegistry -----------------
ParserRegistry& ParserRegistry::instance() {
    static ParserRegistry inst;
    return inst;
}
void ParserRegistry::registerParser(const string& name, shared_ptr<IParser> p) {
    parsers.push_back(p);
}
shared_ptr<IParser> ParserRegistry::findMatching(TokenStream& ts) const {
    for (auto& p : parsers) {
        if (p->match(const_cast<TokenStream&>(ts))) return p;
    }
    return nullptr;
}

// ----------------- 小工具函数 -----------------
static const Token& ldt::tokAt(const vector<Token>& toks, size_t idx) {
    if (idx < toks.size()) return toks[idx];
    static Token eofTok{ TokenType::Eof, "" };
    return eofTok;
}
static bool ldt::isSym(const Token& t, const string& s) {
    return t.type == TokenType::Symbol && t.text == s;
}
static bool ldt::isId(const Token& t, const string& s) {
    return t.type == TokenType::Identifier && t.text == s;
}

// ----------------- ImportParser -----------------
bool ImportParser::match(TokenStream& ts) const {
    return ts.peek().type == TokenType::Keyword && ts.peek().text == "@import";
}
shared_ptr<ASTNode> ImportParser::parse(TokenStream& ts) const {
    ts.next(); // skip @import
    if (!isSym(ts.peek(), "(")) throw runtime_error("Expected '(' after @import");
    ts.next();
    if (ts.peek().type != TokenType::String) throw runtime_error("Expected string in @import()");
    string path = ts.next().text;
    if (!isSym(ts.peek(), ")")) throw runtime_error("Expected ')' after @import string");
    ts.next();
    auto n = ASTNode::make("Import");
    n->attrs["path"] = Attribute(path);
    return n;
}

// ----------------- StyleParser -----------------
bool StyleParser::match(TokenStream& ts) const {
    return ts.peek().type == TokenType::Keyword && ts.peek().text == "@style";
}
shared_ptr<ASTNode> StyleParser::parse(TokenStream& ts) const {
    ts.next(); // skip @style
    LexerGuard g(ts, TokenStream::LexerMode::Style); // 切换到 style 专用词法器
    if (!isSym(ts.peek(), "{")) throw runtime_error("Expected '{' after @style");
    ts.next();
    auto root = ASTNode::make("Style");
    while (!isSym(ts.peek(), "}")) {
        auto peek = ts.peek();
        string selector;
        // 拼接 selector token：仅在两个连续 token 都是 word 类（Identifier/Number/String）时插入空格，
        // 否则直接连接（避免将 ':' 和伪类名分开，或在 ',' 周围产生多余空格）。
        Token prevTok{TokenType::Eof, ""};
        while (!isSym(ts.peek(), "{") && !ts.eof()) {
            Token cur = ts.next();
            auto isWord = [](const Token& tk){ return tk.type == TokenType::Identifier || tk.type == TokenType::Number || tk.type == TokenType::String; };
            if (!selector.empty() && isWord(prevTok) && isWord(cur)) selector += " ";
            selector += cur.text;
            prevTok = cur;
        }
        if (selector.empty()) throw runtime_error("Expected selector in @style");
        
        if (!isSym(ts.peek(), "{")) throw runtime_error("Expected '{' after selector in @style");
        ts.next();
        auto rule = ASTNode::make("StyleRule");
        rule->attrs["selector"] = Attribute(selector);
        while (!isSym(ts.peek(), "}")) {
            if (ts.peek().type != TokenType::Identifier) throw runtime_error("Expected key in style rule");
            string key = ts.next().text;
            if (!isSym(ts.peek(), ":")) throw runtime_error("Expected ':' in style rule");
            ts.next();
            
            // 收集值的所有 token，然后智能转换
            vector<Token> valueToks;
            while (!isSym(ts.peek(), ";") && !isSym(ts.peek(), "}")) {
                const Token& t = ts.peek();
                if (t.type == TokenType::Eof) break;
                valueToks.push_back(t);
                ts.next();
            }
            
            // 如果只有一个 token，使用智能转换
            if (valueToks.size() == 1) {
                rule->attrs[key] = tokenToAttribute(valueToks[0]);
            }
            // 多个 token，拼接为字符串（如 "10 20"）
            else {
                ostringstream valoss;
                for (size_t i = 0; i < valueToks.size(); ++i) {
                    if (i > 0) valoss << " ";
                    valoss << valueToks[i].text;
                }
                rule->attrs[key] = Attribute(valoss.str());
            }
            
            if (isSym(ts.peek(), ";")) ts.next();
        }
        ts.next(); // skip closing '}'
        root->children.push_back(rule);
    }
    ts.next(); // skip closing '}' of @style
    return root;
}

// ----------------- LayoutParser -----------------
bool LayoutParser::match(TokenStream& ts) const {
    return ts.peek().type == TokenType::Keyword && ts.peek().text == "@layout";
}
shared_ptr<ASTNode> LayoutParser::parse(TokenStream& ts) const {
    ts.next(); // skip @layout
    LexerGuard g(ts, TokenStream::LexerMode::Layout); // 切换到 layout 专用词法器（当前与默认相同）
    if (!isSym(ts.peek(), "{")) throw runtime_error("Expected '{' after @layout");
    ts.next();
    auto root = ASTNode::make("Layout");
    while (!isSym(ts.peek(), "}")) {
        auto l = ts.peek();
        if (ts.peek().type != TokenType::Identifier) {
            throw runtime_error("Expected selector in @layout");
        }
        string selector = ts.next().text;
        if (!isSym(ts.peek(), "{")) throw runtime_error("Expected '{' after selector in @layout");
        ts.next();
        auto rule = ASTNode::make("LayoutRule");
        rule->attrs["selector"] = Attribute(selector);
        while (!isSym(ts.peek(), "}")) {
            if (ts.peek().type != TokenType::Identifier) throw runtime_error("Expected key in layout rule");
            string key = ts.next().text;
            if (!isSym(ts.peek(), ":")) throw runtime_error("Expected ':' in layout rule");
            ts.next();
            
            // 根据 token 类型智能转换
            Token valueTok = ts.next();
            rule->attrs[key] = tokenToAttribute(valueTok);
            
            if (isSym(ts.peek(), ";")) ts.next();
        }
        ts.next(); // skip '}'
        root->children.push_back(rule);
    }
    ts.next(); // skip '}' of @layout
    return root;
}

// ----------------- ContentParser -----------------
bool ContentParser::match(TokenStream& ts) const {
    return ts.peek().type == TokenType::Identifier;
}
shared_ptr<ASTNode> ContentParser::parse(TokenStream& ts) const {
    if (ts.peek().type != TokenType::Identifier) throw runtime_error("Expected element type");
    string elemType = ts.next().text;
    string elemId;
    if (isSym(ts.peek(), ":")) {
        ts.next();
        auto l = ts.peek();
        if (ts.peek().type != TokenType::Identifier) {
            throw runtime_error("Expected id after ':'");
        }
        elemId = ts.next().text;
    }
    auto node = ASTNode::make(elemType);
    // 用户显式提供 id 时，存入 attrs["id"] 作为“用户 id”。
    if (!elemId.empty()) node->attrs["id"] = Attribute(elemId);

    if (isSym(ts.peek(), "(")) {
        ts.next(); // skip '('
        parseAttrList(ts, node);
        if (!isSym(ts.peek(), ")")) throw runtime_error("Expected ')' after attr list");
        ts.next();
    }
    if (isSym(ts.peek(), "{")) {
        ts.next(); // skip '{'
        while (!isSym(ts.peek(), "}")) {
            if (ts.peek().type == TokenType::Identifier) {
                auto child = parse(ts);
                child->parent = node;
                node->children.push_back(child);
                if (isSym(ts.peek(), ",")) ts.next();
            }
            else if (isSym(ts.peek(), ",")) {
                ts.next();
            }
            else {
                throw runtime_error("Unexpected token in children block: " + ts.peek().text);
            }
        }
        ts.next(); // skip '}'
    }
    return node;
}

void ContentParser::parseAttrList(TokenStream& ts, shared_ptr<ASTNode>& node) const {
    while (!isSym(ts.peek(), ")")) {
        if (ts.peek().type != TokenType::Identifier) throw runtime_error("Expected attribute key");
        string key = ts.next().text;
        if (!isSym(ts.peek(), "=")) throw runtime_error("Expected '=' in attr");
        ts.next();
        
        if (ts.peek().type == TokenType::String || ts.peek().type == TokenType::Number || ts.peek().type == TokenType::Identifier) {
            // 单个值，使用智能转换
            Token valueTok = ts.next();
            if (key == "class") {
                parseClass(*node, valueTok.text);
                node->attrs[key] = tokenToAttribute(valueTok); // 保持原有属性也存储
            } else {
                node->attrs[key] = tokenToAttribute(valueTok);
            }
        }
        else if (isSym(ts.peek(), "[")) {
            ts.next();
            vector<string> items;
            while (!isSym(ts.peek(), "]")) {
                if (ts.peek().type == TokenType::Number || ts.peek().type == TokenType::Identifier || ts.peek().type == TokenType::String) {
                    items.push_back(ts.next().text);
                    if (isSym(ts.peek(), ",")) ts.next();
                }
                else {
                    throw runtime_error("Invalid array item in attr");
                }
            }
            if (!isSym(ts.peek(), "]")) throw runtime_error("Expected closing ']' in attr array");
            ts.next(); // skip ']'
            bool allNum = true;
            std::vector<float> nums;
            for (auto &it : items) {
                try {
                    nums.push_back(std::stof(it));
                } catch (...) { allNum = false; break; }
            }
            if (allNum) node->attrs[key] = Attribute(nums);
            else {
                ostringstream oss;
                for (size_t i = 0; i < items.size(); ++i) {
                    if (i) oss << ",";
                    oss << items[i];
                }
                node->attrs[key] = Attribute(oss.str());
            }
        }
        else {
            throw runtime_error("Unsupported attr value type: " + ts.peek().text);
        }
        if (isSym(ts.peek(), ",")) ts.next();
        else break;
    }
}

void ContentParser::parseClass(ASTNode& node, const std::string& classStr) const {
    std::vector<std::string> classList;
    std::istringstream iss(classStr);
    std::string cls;
    while (iss >> cls) {
        classList.push_back(cls);
    }
    node.setClassList(classList);
}

// ------------------- 顶层 LDTParser -------------------
shared_ptr<ASTNode> LDTParser::parseRoot(const string& input) const {
    TokenStream ts(input);
    auto root = ASTNode::make("Root");
    while (!ts.eof()) {
        if (isSym(ts.peek(), ";") || isSym(ts.peek(), ",")) { ts.next(); continue; }
        auto parser = ParserRegistry::instance().findMatching(ts);
        if (!parser) {
            ContentParser fallback;
            if (fallback.match(ts)) {
                auto node = fallback.parse(ts);
                root->children.push_back(node);
                continue;
            }
            throw runtime_error("Unknown or unsupported syntax near token: " + ts.peek().text);
        }
        // 在调用 parser->parse 前记录当前 token，以便将以 Keyword (@...) 开头的块单独存放到 root->meta
        Token curTok = ts.peek();
        auto node = parser->parse(ts);
        if (curTok.type == TokenType::Keyword && !curTok.text.empty()) {
            // 使用关键字文本作为 meta 键，例如 "@style"
            root->addMeta(curTok.text, node);
        } else {
            root->children.push_back(node);
        }
    }
    // Stable ids are now assigned when building ResolvedTree.

    return root;
}

AstSnippet LDTParser::parse(const string& input) const {
    return AstSnippet::FromParsedRoot(parseRoot(input));
}
