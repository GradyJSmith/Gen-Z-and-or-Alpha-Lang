/*
 * BrainRot Interpreter  v3.1
 * A dynamically-typed scripting language with Python-like semantics,
 * written in C++17. Saves/loads .aura files. Includes a built-in
 * terminal editor (no ncurses — raw ANSI/termios, Windows compatible).
 *
 * ═══════════════════════════════════════════════════════════════
 *  LANGUAGE REFERENCE
 * ═══════════════════════════════════════════════════════════════
 *
 *  ┌─────────────────┬───────────────┬─────────────────────────────────────────┐
 *  │  BrainRot       │  Python       │  Notes                                  │
 *  ├─────────────────┼───────────────┼─────────────────────────────────────────┤
 *  │  rizz x = ...   │  x = ...      │  declare / assign variable              │
 *  │  yap(...)       │  print(...)   │  print to stdout                        │
 *  │  slurp(...)     │  input(...)   │  read line from stdin                   │
 *  │  rizzler(...)   │  int(...)     │  cast to integer                        │
 *  │  unc            │  —            │  chaos: random type + value each time   │
 *  │  twin f(x) { }  │  def f(x):    │  define a function                      │
 *  │  ghost <expr>   │  return       │  return from a function                 │
 *  │  slay / nah     │  if / else    │  conditional                            │
 *  │  skibidi        │  while        │  while loop                             │
 *  │  sigma x in     │  for x in     │  for loop — sigma i in range(0, 10) { } │
 *  │  bail           │  break        │  break out of loop                      │
 *  │  ohio { } nah { }│ try/except   │  error handling                         │
 *  │  vibe check     │  assert       │  crash with msg if condition is falsy   │
 *  │  low-key        │  exit()       │  terminate process immediately          │
 *  ├─────────────────┼───────────────┼─────────────────────────────────────────┤
 *  │  bussin / cap   │  True / False │  boolean literals                       │
 *  │  npc            │  None         │  null value                             │
 *  │  fr  / no cap   │  == / !=      │  equality / inequality                  │
 *  │  based / ratio  │  >= / <=      │  comparisons                            │
 *  │  cooked / L     │  > / <        │  comparisons                            │
 *  │  and / or / not │               │  logical operators                      │
 *  │  + - * / %      │               │  arithmetic                             │
 *  └─────────────────┴───────────────┴─────────────────────────────────────────┘
 *
 *  FILE FORMAT
 *  ───────────
 *  Source files use the .aura extension (plain UTF-8 text).
 *
 *  USAGE
 *  ─────
 *  brainrot                        open editor (empty buffer)
 *  brainrot edit <file.aura>       open editor with existing file
 *  brainrot run  <file.aura>       run a source file directly
 *
 *  EDITOR KEYBINDINGS
 *  ──────────────────
 *  Arrow keys / Home / End    navigate
 *  Backspace / Delete         delete characters
 *  Enter                      new line
 *  Ctrl-S                     save to .aura
 *  Ctrl-R                     run current buffer
 *  Ctrl-N                     new file (prompts for name)
 *  Ctrl-Q                     quit (prompts if unsaved)
 *
 *  EXAMPLE — FizzBuzz
 *  ──────────────────
 *  rizz i = 1
 *  skibidi i ratio 100 {
 *      slay i % 15 fr 0 { yap("FizzBuzz") }
 *      nah slay i % 3 fr 0 { yap("Fizz") }
 *      nah slay i % 5 fr 0 { yap("Buzz") }
 *      nah { yap(i) }
 *      rizz i = i + 1
 *  }
 *
 *  EXAMPLE — Functions
 *  ────────────────────
 *  twin greet(name) {
 *      ghost "yo " + name + " what is good"
 *  }
 *  yap(greet("world"))
 *
 *  EXAMPLE — Ohio error handling
 *  ─────────────────────────────
 *  ohio {
 *      rizz x = 1 / 0
 *  } nah {
 *      yap("that was ohio fr")
 *  }
 */

// ── Platform detection ─────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
  #define BRAINROT_WINDOWS 1
  #include <conio.h>
  #include <windows.h>
#else
  #define BRAINROT_POSIX 1
  #include <sys/ioctl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// ═══════════════════════════════════════════════════════════════
//  RNG
// ═══════════════════════════════════════════════════════════════

static std::mt19937& rng() {
    static std::mt19937 gen(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return gen;
}

// ═══════════════════════════════════════════════════════════════
//  Forward declarations
// ═══════════════════════════════════════════════════════════════

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// ═══════════════════════════════════════════════════════════════
//  Value  — runtime type
// ═══════════════════════════════════════════════════════════════

struct Value {
    enum class Kind { Null, Bool, Int, Float, String };
    Kind        kind = Kind::Null;
    bool        b    = false;
    long long   i    = 0;
    double      f    = 0.0;
    std::string s;

    Value() = default;
    explicit Value(bool v)        : kind(Kind::Bool),   b(v)            {}
    explicit Value(long long v)   : kind(Kind::Int),    i(v)            {}
    explicit Value(double v)      : kind(Kind::Float),  f(v)            {}
    explicit Value(std::string v) : kind(Kind::String), s(std::move(v)) {}

    bool truthy() const {
        switch (kind) {
            case Kind::Null:   return false;
            case Kind::Bool:   return b;
            case Kind::Int:    return i != 0;
            case Kind::Float:  return f != 0.0;
            case Kind::String: return !s.empty();
        }
        return false;
    }

    std::string to_string() const {
        switch (kind) {
            case Kind::Null:   return "npc";
            case Kind::Bool:   return b ? "bussin" : "cap";
            case Kind::Int:    return std::to_string(i);
            case Kind::Float: { std::ostringstream o; o << f; return o.str(); }
            case Kind::String: return s;
        }
        return "";
    }

    double to_number() const {
        switch (kind) {
            case Kind::Null:   return 0.0;
            case Kind::Bool:   return b ? 1.0 : 0.0;
            case Kind::Int:    return static_cast<double>(i);
            case Kind::Float:  return f;
            case Kind::String:
                try { return std::stod(s); }
                catch (...) { throw std::runtime_error("Can't coerce \"" + s + "\" to number"); }
        }
        return 0.0;
    }

    bool is_numeric() const { return kind == Kind::Int || kind == Kind::Float; }

    // unc: random type AND content every single time
    static Value chaos() {
        static const std::vector<std::string> pool = {
            "skibidi", "rizz overload", "certified ohio moment", "no cap fr fr",
            "sigma grindset", "he is so cooked", "twin what are you doing",
            "bro fell off", "mewing arc activated", "fanum tax",
            "gyatt", "edge detected", "slay or get slayed", "the unc has arrived",
            "ratio + L + cap", "literally who asked", "touch grass immediately",
            "mid behavior", "W rizz detected", "glazing detected"
        };
        std::uniform_int_distribution<int> roll(0, 3);
        switch (roll(rng())) {
            case 0: { std::uniform_int_distribution<long long>  d(-1000, 1000); return Value(d(rng())); }
            case 1: { std::uniform_real_distribution<double>    d(-1.0,  1.0);  return Value(d(rng())); }
            case 2: { std::uniform_int_distribution<int>        d(0, 1);        return Value(static_cast<bool>(d(rng()))); }
            case 3: { std::uniform_int_distribution<std::size_t>d(0, pool.size()-1); return Value(pool[d(rng())]); }
        }
        return Value();
    }
};

static bool values_equal(const Value& a, const Value& b) {
    if (a.kind==Value::Kind::Null   && b.kind==Value::Kind::Null)   return true;
    if (a.kind==Value::Kind::Bool   && b.kind==Value::Kind::Bool)   return a.b == b.b;
    if (a.kind==Value::Kind::String && b.kind==Value::Kind::String) return a.s == b.s;
    if (a.is_numeric() && b.is_numeric()) return a.to_number() == b.to_number();
    return false;
}

// ═══════════════════════════════════════════════════════════════
//  Callable  (twin)
// ═══════════════════════════════════════════════════════════════

struct Callable {
    std::string              name;
    std::vector<std::string> params;
    std::vector<StmtPtr>     body;
};
using CallablePtr = std::shared_ptr<Callable>;

// ═══════════════════════════════════════════════════════════════
//  Lexer
// ═══════════════════════════════════════════════════════════════

enum class TT {
    Number, String, Ident,
    Rizz, Yap, Slurp, Rizzler, Unc,
    Twin, Ghost,
    Slay, Nah, Skibidi, Sigma, In, Range,
    Bail, Ohio, VibeCheck, LowKey,
    Bussin, Cap, Npc,
    Fr, NoCap, Based, Ratio, Cooked, L,
    And, Or, Not,
    Plus, Minus, Star, Slash, Percent, Assign,
    LParen, RParen, LBrace, RBrace,
    Comma, Semicolon, Newline, Eof,
};

struct Token { TT type; std::string lexeme; int line = 0; };

class Lexer {
public:
    explicit Lexer(std::string src) : src_(std::move(src)) {}

    std::vector<Token> tokenise() {
        while (!at_end()) { skip_trivia(); if (!at_end()) scan_token(); }
        emit(TT::Eof, "");
        return tokens_;
    }

private:
    std::string        src_;
    std::size_t        pos_  = 0;
    int                line_ = 1;
    std::vector<Token> tokens_;

    char peek(int off=0) const {
        auto p = pos_ + static_cast<std::size_t>(off < 0 ? 0 : off);
        return p < src_.size() ? src_[p] : '\0';
    }
    char advance() { char c=src_[pos_++]; if(c=='\n')++line_; return c; }
    bool at_end()  const { return pos_ >= src_.size(); }
    void emit(TT t, std::string l) { tokens_.push_back({t,std::move(l),line_}); }

    void skip_trivia() {
        while (!at_end()) {
            char c=peek();
            if(c==' '||c=='\r'||c=='\t') advance();
            else if(c=='#'){ while(!at_end()&&peek()!='\n') advance(); }
            else break;
        }
    }

    bool prev_is_value() const {
        if(tokens_.empty()) return false;
        auto t=tokens_.back().type;
        return t==TT::Number||t==TT::String||t==TT::Ident||
               t==TT::RParen||t==TT::Bussin||t==TT::Cap||t==TT::Npc||t==TT::Unc;
    }

    void scan_token() {
        char c=peek();
        if(c=='\n'){advance();emit(TT::Newline,"\\n");return;}
        if(c=='"'){scan_string();return;}
        if(std::isdigit(c)||(c=='-'&&std::isdigit(peek(1))&&!prev_is_value())){scan_number();return;}
        switch(c){
            case '+':advance();emit(TT::Plus,    "+");return;
            case '-':advance();emit(TT::Minus,   "-");return;
            case '*':advance();emit(TT::Star,    "*");return;
            case '/':advance();emit(TT::Slash,   "/");return;
            case '%':advance();emit(TT::Percent, "%");return;
            case '=':advance();emit(TT::Assign,  "=");return;
            case '(':advance();emit(TT::LParen,  "(");return;
            case ')':advance();emit(TT::RParen,  ")");return;
            case '{':advance();emit(TT::LBrace,  "{");return;
            case '}':advance();emit(TT::RBrace,  "}");return;
            case ',':advance();emit(TT::Comma,   ",");return;
            case ';':advance();emit(TT::Semicolon,";");return;
            default:break;
        }
        if(std::isalpha(c)||c=='_'){scan_ident();return;}
        std::cerr<<"[lexer] unknown char '"<<c<<"' line "<<line_<<"\n";
        advance();
    }

    void scan_string() {
        advance();
        std::string val;
        while(!at_end()&&peek()!='"'){
            char c=advance();
            if(c=='\\'){
                char e=advance();
                switch(e){case 'n':val+='\n';break;case 't':val+='\t';break;
                          case '"':val+='"';break;case '\\':val+='\\';break;default:val+=e;}
            } else val+=c;
        }
        if(at_end()) throw std::runtime_error("Unterminated string on line "+std::to_string(line_));
        advance();
        emit(TT::String,val);
    }

    void scan_number() {
        std::string n;
        if(peek()=='-') n+=advance();
        while(!at_end()&&std::isdigit(peek())) n+=advance();
        if(!at_end()&&peek()=='.'&&std::isdigit(peek(1))){
            n+=advance(); while(!at_end()&&std::isdigit(peek())) n+=advance();
        }
        emit(TT::Number,n);
    }

    void scan_ident() {
        std::string word;
        // allow hyphen inside identifiers so "low-key" is one token
        while(!at_end()&&(std::isalnum(peek())||peek()=='_'||peek()=='-')) word+=advance();

        // two-word keywords
        if(word=="no"){
            std::size_t save=pos_; int sl=line_;
            while(!at_end()&&peek()==' ') advance();
            std::string nxt; while(!at_end()&&std::isalpha(peek())) nxt+=advance();
            if(nxt=="cap"){emit(TT::NoCap,"no cap");return;}
            pos_=save;line_=sl;
        }
        if(word=="vibe"){
            std::size_t save=pos_; int sl=line_;
            while(!at_end()&&peek()==' ') advance();
            std::string nxt; while(!at_end()&&std::isalpha(peek())) nxt+=advance();
            if(nxt=="check"){emit(TT::VibeCheck,"vibe check");return;}
            pos_=save;line_=sl;
        }
        if(word=="low-key"){emit(TT::LowKey,"low-key");return;}

        static const std::map<std::string,TT> kw={
            {"rizz",TT::Rizz},{"yap",TT::Yap},{"slurp",TT::Slurp},
            {"rizzler",TT::Rizzler},{"unc",TT::Unc},
            {"twin",TT::Twin},{"ghost",TT::Ghost},
            {"slay",TT::Slay},{"nah",TT::Nah},
            {"skibidi",TT::Skibidi},{"sigma",TT::Sigma},
            {"in",TT::In},{"range",TT::Range},
            {"bail",TT::Bail},{"ohio",TT::Ohio},
            {"bussin",TT::Bussin},{"cap",TT::Cap},{"npc",TT::Npc},
            {"fr",TT::Fr},{"based",TT::Based},{"ratio",TT::Ratio},
            {"cooked",TT::Cooked},{"L",TT::L},
            {"and",TT::And},{"or",TT::Or},{"not",TT::Not},
        };
        auto it=kw.find(word);
        emit(it!=kw.end()?it->second:TT::Ident,word);
    }
};

// ═══════════════════════════════════════════════════════════════
//  AST nodes
// ═══════════════════════════════════════════════════════════════

struct LiteralExpr  { Value value; };
struct UncExpr      {};
struct IdentExpr    { std::string name; };
struct UnaryExpr    { std::string op; ExprPtr right; };
struct BinaryExpr   { std::string op; ExprPtr left; ExprPtr right; };
struct AssignExpr   { std::string name; ExprPtr value; };
struct CallExpr     { std::string callee; std::vector<ExprPtr> args; };

struct Expr { std::variant<LiteralExpr,UncExpr,IdentExpr,UnaryExpr,BinaryExpr,AssignExpr,CallExpr> node; };

struct ExprStmt    { ExprPtr expr; };
struct RizzStmt    { std::string name; ExprPtr init; };
struct TwinStmt    { CallablePtr fn; };
struct GhostStmt   { ExprPtr value; };
struct SlayStmt    { ExprPtr cond; std::vector<StmtPtr> then_b; std::vector<StmtPtr> else_b; };
struct SkibidiStmt { ExprPtr cond; std::vector<StmtPtr> body; };
struct SigmaStmt   { std::string var; ExprPtr start; ExprPtr end; std::vector<StmtPtr> body; };
struct BailStmt    {};
struct OhioStmt    { std::vector<StmtPtr> try_b; std::vector<StmtPtr> catch_b; };
struct VibeStmt    { ExprPtr cond; std::string msg; };
struct LowKeyStmt  {};

struct Stmt { std::variant<ExprStmt,RizzStmt,TwinStmt,GhostStmt,SlayStmt,
                            SkibidiStmt,SigmaStmt,BailStmt,OhioStmt,VibeStmt,LowKeyStmt> node; };

// ═══════════════════════════════════════════════════════════════
//  Parser
// ═══════════════════════════════════════════════════════════════

class Parser {
public:
    explicit Parser(std::vector<Token> t) : tokens_(std::move(t)) {}
    std::vector<StmtPtr> parse() {
        std::vector<StmtPtr> stmts;
        skip_sep();
        while(!check(TT::Eof)){stmts.push_back(statement());skip_sep();}
        return stmts;
    }
private:
    std::vector<Token> tokens_;
    std::size_t        pos_=0;

    const Token& peek(int off=0) const { return tokens_[std::min(pos_+off,tokens_.size()-1)]; }
    bool check(TT t,int off=0) const   { return peek(off).type==t; }
    const Token& advance()             { return tokens_[pos_++]; }
    void skip_sep() { while(check(TT::Newline)||check(TT::Semicolon)) advance(); }
    void skip_nl()  { while(check(TT::Newline)) advance(); }
    bool match(TT t){ if(check(t)){advance();return true;}return false; }

    const Token& expect(TT t,const std::string& msg){
        if(!check(t)) throw std::runtime_error(msg+" (got \""+peek().lexeme+"\" line "+std::to_string(peek().line)+")");
        return advance();
    }

    StmtPtr statement(){
        skip_sep();
        if(check(TT::Rizz))     return rizz_stmt();
        if(check(TT::Twin))     return twin_stmt();
        if(check(TT::Ghost))    return ghost_stmt();
        if(check(TT::Slay))     return slay_stmt();
        if(check(TT::Skibidi))  return skibidi_stmt();
        if(check(TT::Sigma))    return sigma_stmt();
        if(check(TT::Ohio))     return ohio_stmt();
        if(check(TT::VibeCheck))return vibe_stmt();
        if(check(TT::Bail))  {advance();skip_sep();auto s=std::make_unique<Stmt>();s->node=BailStmt{};return s;}
        if(check(TT::LowKey)){advance();skip_sep();auto s=std::make_unique<Stmt>();s->node=LowKeyStmt{};return s;}
        auto expr=expression(); skip_sep();
        auto s=std::make_unique<Stmt>(); s->node=ExprStmt{std::move(expr)}; return s;
    }

    StmtPtr rizz_stmt(){
        advance();
        std::string name=expect(TT::Ident,"Expected var name after 'rizz'").lexeme;
        expect(TT::Assign,"Expected '='");
        auto init=expression(); skip_sep();
        auto s=std::make_unique<Stmt>(); s->node=RizzStmt{name,std::move(init)}; return s;
    }

    StmtPtr twin_stmt(){
        advance();
        std::string name=expect(TT::Ident,"Expected function name").lexeme;
        expect(TT::LParen,"Expected '('");
        std::vector<std::string> params;
        if(!check(TT::RParen)){
            params.push_back(expect(TT::Ident,"Expected param name").lexeme);
            while(match(TT::Comma)) params.push_back(expect(TT::Ident,"Expected param name").lexeme);
        }
        expect(TT::RParen,"Expected ')'");
        auto body=block();
        auto fn=std::make_shared<Callable>(); fn->name=name; fn->params=std::move(params); fn->body=std::move(body);
        auto s=std::make_unique<Stmt>(); s->node=TwinStmt{fn}; return s;
    }

    StmtPtr ghost_stmt(){
        advance();
        ExprPtr val;
        if(!check(TT::Newline)&&!check(TT::Semicolon)&&!check(TT::RBrace)&&!check(TT::Eof))
            val=expression();
        else { auto e=std::make_unique<Expr>(); e->node=LiteralExpr{Value()}; val=std::move(e); }
        skip_sep();
        auto s=std::make_unique<Stmt>(); s->node=GhostStmt{std::move(val)}; return s;
    }

    StmtPtr slay_stmt(){
        advance();
        auto cond=expression(); auto then_b=block();
        std::vector<StmtPtr> else_b;
        skip_nl();
        if(match(TT::Nah)){
            skip_nl();
            if(check(TT::Slay)) else_b.push_back(slay_stmt());
            else else_b=block();
        }
        auto s=std::make_unique<Stmt>(); s->node=SlayStmt{std::move(cond),std::move(then_b),std::move(else_b)}; return s;
    }

    StmtPtr skibidi_stmt(){
        advance();
        auto cond=expression(); auto body=block();
        auto s=std::make_unique<Stmt>(); s->node=SkibidiStmt{std::move(cond),std::move(body)}; return s;
    }

    StmtPtr sigma_stmt(){
        advance();
        std::string var=expect(TT::Ident,"Expected loop var after 'sigma'").lexeme;
        expect(TT::In,"Expected 'in'");
        expect(TT::Range,"Expected 'range'");
        expect(TT::LParen,"Expected '('");
        auto start=expression(); expect(TT::Comma,"Expected ','"); auto end=expression();
        expect(TT::RParen,"Expected ')'");
        auto body=block();
        auto s=std::make_unique<Stmt>(); s->node=SigmaStmt{var,std::move(start),std::move(end),std::move(body)}; return s;
    }

    StmtPtr ohio_stmt(){
        advance(); auto try_b=block();
        skip_nl(); expect(TT::Nah,"Expected 'nah' after ohio block");
        auto catch_b=block();
        auto s=std::make_unique<Stmt>(); s->node=OhioStmt{std::move(try_b),std::move(catch_b)}; return s;
    }

    StmtPtr vibe_stmt(){
        advance(); auto cond=expression();
        std::string msg="vibe check failed fr";
        if(match(TT::Comma)&&check(TT::String)) msg=advance().lexeme;
        skip_sep();
        auto s=std::make_unique<Stmt>(); s->node=VibeStmt{std::move(cond),msg}; return s;
    }

    std::vector<StmtPtr> block(){
        expect(TT::LBrace,"Expected '{'");
        std::vector<StmtPtr> stmts; skip_sep();
        while(!check(TT::RBrace)&&!check(TT::Eof)){stmts.push_back(statement());skip_sep();}
        expect(TT::RBrace,"Expected '}'");
        return stmts;
    }

    // expressions
    ExprPtr expression() { return assign_expr(); }

    ExprPtr assign_expr(){
        if(check(TT::Ident)&&check(TT::Assign,1)){
            std::string name=advance().lexeme; advance();
            auto val=expression();
            auto e=std::make_unique<Expr>(); e->node=AssignExpr{name,std::move(val)}; return e;
        }
        return or_expr();
    }
    ExprPtr or_expr(){
        auto left=and_expr();
        while(check(TT::Or)){advance();auto right=and_expr();auto e=std::make_unique<Expr>();e->node=BinaryExpr{"or",std::move(left),std::move(right)};left=std::move(e);}
        return left;
    }
    ExprPtr and_expr(){
        auto left=not_expr();
        while(check(TT::And)){advance();auto right=not_expr();auto e=std::make_unique<Expr>();e->node=BinaryExpr{"and",std::move(left),std::move(right)};left=std::move(e);}
        return left;
    }
    ExprPtr not_expr(){
        if(check(TT::Not)){advance();auto right=not_expr();auto e=std::make_unique<Expr>();e->node=UnaryExpr{"not",std::move(right)};return e;}
        return comparison();
    }
    ExprPtr comparison(){
        auto left=additive();
        while(true){
            std::string op;
            if     (check(TT::Fr))     op="fr";
            else if(check(TT::NoCap))  op="no cap";
            else if(check(TT::Based))  op="based";
            else if(check(TT::Ratio))  op="ratio";
            else if(check(TT::Cooked)) op="cooked";
            else if(check(TT::L))      op="L";
            else break;
            advance(); auto right=additive();
            auto e=std::make_unique<Expr>(); e->node=BinaryExpr{op,std::move(left),std::move(right)}; left=std::move(e);
        }
        return left;
    }
    ExprPtr additive(){
        auto left=multiplicative();
        while(check(TT::Plus)||check(TT::Minus)){
            std::string op=advance().lexeme; auto right=multiplicative();
            auto e=std::make_unique<Expr>(); e->node=BinaryExpr{op,std::move(left),std::move(right)}; left=std::move(e);
        }
        return left;
    }
    ExprPtr multiplicative(){
        auto left=unary();
        while(check(TT::Star)||check(TT::Slash)||check(TT::Percent)){
            std::string op=advance().lexeme; auto right=unary();
            auto e=std::make_unique<Expr>(); e->node=BinaryExpr{op,std::move(left),std::move(right)}; left=std::move(e);
        }
        return left;
    }
    ExprPtr unary(){
        if(check(TT::Minus)){advance();auto right=unary();auto e=std::make_unique<Expr>();e->node=UnaryExpr{"-",std::move(right)};return e;}
        return primary();
    }
    ExprPtr primary(){
        if(check(TT::Number)){
            std::string lex=advance().lexeme; auto e=std::make_unique<Expr>();
            if(lex.find('.')!=std::string::npos) e->node=LiteralExpr{Value(std::stod(lex))};
            else e->node=LiteralExpr{Value(static_cast<long long>(std::stoll(lex)))};
            return e;
        }
        if(check(TT::String)){auto e=std::make_unique<Expr>();e->node=LiteralExpr{Value(advance().lexeme)};return e;}
        if(match(TT::Bussin)){auto e=std::make_unique<Expr>();e->node=LiteralExpr{Value(true)};return e;}
        if(match(TT::Cap))   {auto e=std::make_unique<Expr>();e->node=LiteralExpr{Value(false)};return e;}
        if(match(TT::Npc))   {auto e=std::make_unique<Expr>();e->node=LiteralExpr{Value()};return e;}
        if(match(TT::Unc))   {auto e=std::make_unique<Expr>();e->node=UncExpr{};return e;}

        if(check(TT::Yap)||check(TT::Slurp)||check(TT::Rizzler)){
            std::string callee=advance().lexeme;
            expect(TT::LParen,"Expected '('");
            std::vector<ExprPtr> args;
            if(!check(TT::RParen)){args.push_back(expression());while(match(TT::Comma))args.push_back(expression());}
            expect(TT::RParen,"Expected ')'");
            auto e=std::make_unique<Expr>(); e->node=CallExpr{callee,std::move(args)}; return e;
        }
        if(check(TT::Ident)){
            std::string name=advance().lexeme;
            if(check(TT::LParen)){
                advance();
                std::vector<ExprPtr> args;
                if(!check(TT::RParen)){args.push_back(expression());while(match(TT::Comma))args.push_back(expression());}
                expect(TT::RParen,"Expected ')'");
                auto e=std::make_unique<Expr>(); e->node=CallExpr{name,std::move(args)}; return e;
            }
            auto e=std::make_unique<Expr>(); e->node=IdentExpr{name}; return e;
        }
        if(match(TT::LParen)){auto inner=expression();expect(TT::RParen,"Expected ')'");return inner;}
        throw std::runtime_error("Unexpected token \""+peek().lexeme+"\" line "+std::to_string(peek().line));
    }
};

// ═══════════════════════════════════════════════════════════════
//  Environment
// ═══════════════════════════════════════════════════════════════

class Environment {
public:
    explicit Environment(Environment* p=nullptr):parent_(p){}
    void        define(const std::string& n,Value v)     { vars_[n]=std::move(v); }
    void        define_fn(const std::string& n,CallablePtr fn){ fns_[n]=std::move(fn); }
    Value       get(const std::string& n) const {
        auto it=vars_.find(n); if(it!=vars_.end()) return it->second;
        if(parent_) return parent_->get(n);
        throw std::runtime_error("Undefined variable: '"+n+"'");
    }
    void        set(const std::string& n,Value v){
        auto it=vars_.find(n); if(it!=vars_.end()){it->second=std::move(v);return;}
        if(parent_){parent_->set(n,std::move(v));return;}
        throw std::runtime_error("Undefined variable: '"+n+"'");
    }
    bool        has(const std::string& n) const { return vars_.count(n)||(parent_&&parent_->has(n)); }
    CallablePtr get_fn(const std::string& n) const {
        auto it=fns_.find(n); if(it!=fns_.end()) return it->second;
        if(parent_) return parent_->get_fn(n);
        return nullptr;
    }
private:
    std::map<std::string,Value>       vars_;
    std::map<std::string,CallablePtr> fns_;
    Environment*                      parent_;
};

// ═══════════════════════════════════════════════════════════════
//  Control flow signals
// ═══════════════════════════════════════════════════════════════

struct BailSignal   {};
struct GhostSignal  { Value value; };
struct LowKeySignal {};

// ═══════════════════════════════════════════════════════════════
//  Interpreter
// ═══════════════════════════════════════════════════════════════

class Interpreter {
public:
    void run(const std::vector<StmtPtr>& stmts){ for(auto& s:stmts) exec(*s,global_); }
private:
    Environment global_;

    void exec(const Stmt& s,Environment& env){ std::visit([&](auto& n){exec_node(n,env);},s.node); }
    void exec_block(const std::vector<StmtPtr>& stmts,Environment& env){ for(auto& s:stmts) exec(*s,env); }

    void exec_node(const ExprStmt& s,   Environment& env){ eval(*s.expr,env); }
    void exec_node(const RizzStmt& s,   Environment& env){ env.define(s.name,eval(*s.init,env)); }
    void exec_node(const TwinStmt& s,   Environment& env){ env.define_fn(s.fn->name,s.fn); }
    void exec_node(const GhostStmt& s,  Environment& env){ throw GhostSignal{eval(*s.value,env)}; }
    void exec_node(const BailStmt&,     Environment&    ){ throw BailSignal{}; }
    void exec_node(const LowKeyStmt&,   Environment&    ){ throw LowKeySignal{}; }

    void exec_node(const SlayStmt& s,Environment& env){
        if(eval(*s.cond,env).truthy()){Environment c(&env);exec_block(s.then_b,c);}
        else if(!s.else_b.empty()){Environment c(&env);exec_block(s.else_b,c);}
    }
    void exec_node(const SkibidiStmt& s,Environment& env){
        while(eval(*s.cond,env).truthy()){
            Environment c(&env);
            try{exec_block(s.body,c);}catch(BailSignal&){break;}
        }
    }
    void exec_node(const SigmaStmt& s,Environment& env){
        long long start=static_cast<long long>(eval(*s.start,env).to_number());
        long long end  =static_cast<long long>(eval(*s.end,  env).to_number());
        for(long long idx=start;idx<end;++idx){
            Environment c(&env); c.define(s.var,Value(idx));
            try{exec_block(s.body,c);}catch(BailSignal&){break;}
        }
    }
    void exec_node(const OhioStmt& s,Environment& env){
        try{ Environment c(&env); exec_block(s.try_b,c); }
        catch(BailSignal&) {throw;} catch(GhostSignal&){throw;} catch(LowKeySignal&){throw;}
        catch(...){ Environment c(&env); exec_block(s.catch_b,c); }
    }
    void exec_node(const VibeStmt& s,Environment& env){
        if(!eval(*s.cond,env).truthy())
            throw std::runtime_error("VIBE CHECK FAILED: "+s.msg);
    }

    Value eval(const Expr& e,Environment& env){ return std::visit([&](auto& n)->Value{return eval_node(n,env);},e.node); }

    Value eval_node(const LiteralExpr& e,Environment&)  { return e.value; }
    Value eval_node(const UncExpr&,      Environment&)  { return Value::chaos(); }
    Value eval_node(const IdentExpr& e,  Environment& env){ return env.get(e.name); }
    Value eval_node(const AssignExpr& e, Environment& env){
        Value v=eval(*e.value,env);
        if(env.has(e.name)) env.set(e.name,v); else env.define(e.name,v);
        return v;
    }
    Value eval_node(const UnaryExpr& e,Environment& env){
        Value r=eval(*e.right,env);
        if(e.op=="-"){ if(r.kind==Value::Kind::Int) return Value(-r.i); if(r.kind==Value::Kind::Float) return Value(-r.f); throw std::runtime_error("Unary '-' needs a number"); }
        if(e.op=="not") return Value(!r.truthy());
        throw std::runtime_error("Unknown unary: "+e.op);
    }
    Value eval_node(const BinaryExpr& e,Environment& env){
        if(e.op=="and"){auto l=eval(*e.left,env);return l.truthy()?eval(*e.right,env):l;}
        if(e.op=="or") {auto l=eval(*e.left,env);return l.truthy()?l:eval(*e.right,env);}
        Value l=eval(*e.left,env), r=eval(*e.right,env);
        if(e.op=="+"){
            if(l.kind==Value::Kind::String||r.kind==Value::Kind::String) return Value(l.to_string()+r.to_string());
            if(l.kind==Value::Kind::Float||r.kind==Value::Kind::Float) return Value(l.to_number()+r.to_number());
            return Value(l.i+r.i);
        }
        if(e.op=="-"){ if(l.kind==Value::Kind::Float||r.kind==Value::Kind::Float) return Value(l.to_number()-r.to_number()); return Value(l.i-r.i); }
        if(e.op=="*"){ if(l.kind==Value::Kind::Float||r.kind==Value::Kind::Float) return Value(l.to_number()*r.to_number()); return Value(l.i*r.i); }
        if(e.op=="/"){double d=r.to_number();if(d==0.0)throw std::runtime_error("Division by zero (so cooked)");if(l.kind==Value::Kind::Int&&r.kind==Value::Kind::Int)return Value(l.i/r.i);return Value(l.to_number()/d);}
        if(e.op=="%"){if(r.i==0)throw std::runtime_error("Modulo by zero (ratio'd)");return Value(l.i%r.i);}
        if(e.op=="fr")     return Value(values_equal(l,r));
        if(e.op=="no cap") return Value(!values_equal(l,r));
        if(e.op=="based")  return Value(l.to_number()>=r.to_number());
        if(e.op=="ratio")  return Value(l.to_number()<=r.to_number());
        if(e.op=="cooked") return Value(l.to_number()> r.to_number());
        if(e.op=="L")      return Value(l.to_number()< r.to_number());
        throw std::runtime_error("Unknown operator: "+e.op);
    }
    Value eval_node(const CallExpr& e,Environment& env){
        if(e.callee=="yap"){
            for(std::size_t i=0;i<e.args.size();++i){if(i)std::cout<<' ';std::cout<<eval(*e.args[i],env).to_string();}
            std::cout<<'\n'; return Value();
        }
        if(e.callee=="slurp"){
            if(!e.args.empty()) std::cout<<eval(*e.args[0],env).to_string();
            std::string line; std::getline(std::cin,line); return Value(line);
        }
        if(e.callee=="rizzler"){
            if(e.args.empty()) return Value(0LL);
            Value v=eval(*e.args[0],env);
            if(v.kind==Value::Kind::Int) return v;
            return Value(static_cast<long long>(v.to_number()));
        }
        CallablePtr fn=env.get_fn(e.callee);
        if(!fn) throw std::runtime_error("Undefined function: '"+e.callee+"'");
        if(e.args.size()!=fn->params.size())
            throw std::runtime_error("'"+e.callee+"' expected "+std::to_string(fn->params.size())+" args, got "+std::to_string(e.args.size()));
        Environment fn_env(&env);
        for(std::size_t i=0;i<fn->params.size();++i) fn_env.define(fn->params[i],eval(*e.args[i],env));
        try{ for(auto& s:fn->body) exec(*s,fn_env); }
        catch(GhostSignal& g){ return g.value; }
        return Value();
    }
};

// ═══════════════════════════════════════════════════════════════
//  run_source
// ═══════════════════════════════════════════════════════════════

static void run_source(const std::string& src){
    Lexer lex(src); auto tokens=lex.tokenise();
    Parser par(std::move(tokens)); auto stmts=par.parse();
    Interpreter interp; interp.run(stmts);
}

// ═══════════════════════════════════════════════════════════════
//  .aura file helpers
// ═══════════════════════════════════════════════════════════════

static std::string aura_path(const std::string& name){
    // case-insensitive check so "file.AURA" also works
    if(name.size()>=5){
        std::string tail=name.substr(name.size()-5);
        std::transform(tail.begin(),tail.end(),tail.begin(),[](unsigned char c){return std::tolower(c);});
        if(tail==".aura") return name;
    }
    return name+".aura";
}
static std::string load_aura(const std::string& path){
    std::ifstream f(path); if(!f) throw std::runtime_error("Cannot open: "+path);
    return std::string(std::istreambuf_iterator<char>(f),{});
}
static void save_aura(const std::string& path,const std::vector<std::string>& lines){
    std::ofstream f(path); if(!f) throw std::runtime_error("Cannot save: "+path);
    for(auto& l:lines) f<<l<<'\n';
}

// ═══════════════════════════════════════════════════════════════
//  Terminal layer  (POSIX + Windows)
// ═══════════════════════════════════════════════════════════════

namespace term {

static constexpr int KEY_UP    = 1000;
static constexpr int KEY_DOWN  = 1001;
static constexpr int KEY_RIGHT = 1002;
static constexpr int KEY_LEFT  = 1003;
static constexpr int KEY_HOME  = 1004;
static constexpr int KEY_END   = 1005;
static constexpr int KEY_DEL   = 1006;

#ifdef BRAINROT_POSIX
static struct termios orig_; static bool raw_=false;
void disable_raw(){ if(raw_){tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_);raw_=false;} }
void enable_raw(){
    tcgetattr(STDIN_FILENO,&orig_); atexit(disable_raw);
    struct termios r=orig_;
    r.c_iflag&=~(BRKINT|ICRNL|INPCK|ISTRIP|IXON);
    r.c_oflag&=~(OPOST); r.c_cflag|=(CS8);
    r.c_lflag&=~(ECHO|ICANON|IEXTEN|ISIG);
    r.c_cc[VMIN]=1; r.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&r); raw_=true;
}
std::pair<int,int> size(){ struct winsize w; ioctl(STDOUT_FILENO,TIOCGWINSZ,&w); return{w.ws_col,w.ws_row}; }
int read_key(){
    unsigned char c; if(read(STDIN_FILENO,&c,1)!=1) return -1;
    if(c=='\x1b'){
        unsigned char s[3]={};
        if(read(STDIN_FILENO,&s[0],1)!=1) return '\x1b';
        if(read(STDIN_FILENO,&s[1],1)!=1) return '\x1b';
        if(s[0]=='['){
            switch(s[1]){
                case 'A':return KEY_UP; case 'B':return KEY_DOWN;
                case 'C':return KEY_RIGHT; case 'D':return KEY_LEFT;
                case 'H':return KEY_HOME; case 'F':return KEY_END;
                case '3':{ unsigned char x; read(STDIN_FILENO,&x,1); if(x=='~') return KEY_DEL; }
            }
        }
        return '\x1b';
    }
    return c;
}
#endif

#ifdef BRAINROT_WINDOWS
void enable_raw(){ SetConsoleOutputCP(CP_UTF8); HANDLE h=GetStdHandle(STD_INPUT_HANDLE); DWORD m=0; GetConsoleMode(h,&m); SetConsoleMode(h,m&~(ENABLE_ECHO_INPUT|ENABLE_LINE_INPUT)); }
void disable_raw(){}
std::pair<int,int> size(){ CONSOLE_SCREEN_BUFFER_INFO i; GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&i); return{i.srWindow.Right-i.srWindow.Left+1,i.srWindow.Bottom-i.srWindow.Top+1}; }
int read_key(){ int c=_getch(); if(c==0||c==0xE0){ int x=_getch(); switch(x){case 72:return KEY_UP;case 80:return KEY_DOWN;case 77:return KEY_RIGHT;case 75:return KEY_LEFT;case 71:return KEY_HOME;case 79:return KEY_END;case 83:return KEY_DEL;} return -1;} return c; }
#endif

void flush(){ std::cout.flush(); }
void clear(){ std::cout<<"\x1b[2J\x1b[H"; }
void move(int r,int c){ std::cout<<"\x1b["<<r<<";"<<c<<"H"; }

// prompt_line: stay in raw mode and read the filename ourselves,
// character by character. This avoids every std::cin / cooked-mode
// conflict — we never leave raw mode at all.
std::string prompt_line(int r, int cols, const std::string& label){
    (void)cols;
    // Draw the prompt bar
    move(r,1);
    std::cout<<"\x1b[2K\x1b[7m "<<label<<"\x1b[0m ";
    flush();

    std::string in;
    while(true){
        int k = read_key();
        if(k == '\r' || k == '\n') break;           // Enter — done
        if(k == '\x1b') { in.clear(); break; }      // Escape — cancel
        if((k == 127 || k == '\b') && !in.empty()){ // Backspace
            in.pop_back();
            std::cout<<"\b \b";
            flush();
        } else if(k >= 32 && k < 127){              // Printable ASCII
            in += static_cast<char>(k);
            std::cout<<static_cast<char>(k);
            flush();
        }
        // ignore everything else (arrows, ctrl chords, etc.)
    }
    return in;
}

} // namespace term

// ═══════════════════════════════════════════════════════════════
//  Editor
// ═══════════════════════════════════════════════════════════════

class Editor {
public:
    explicit Editor(std::string filename="") : filename_(std::move(filename)) {
        if(!filename_.empty()){
            try{
                std::istringstream ss(load_aura(filename_));
                std::string line; while(std::getline(ss,line)) lines_.push_back(line);
            }catch(...){}
        }
        if(lines_.empty()) lines_.push_back("");
    }

    void run(){
        term::enable_raw(); dirty_=false;
        while(true){ draw(); int k=term::read_key(); if(!handle_key(k)) break; }
        term::disable_raw(); term::clear();
    }

private:
    std::string              filename_;
    std::vector<std::string> lines_;
    int cx_=0, cy_=0, sr_=0, sc_=0; // cursor + scroll
    bool dirty_=false;
    std::string status_;

    int tcols() const { return term::size().first; }
    int trows() const { return term::size().second; }
    int erows() const { return trows()-2; } // minus header + status bar

    void draw(){
        auto [cols,rows]=term::size();
        int er=erows();
        if(cy_<sr_) sr_=cy_;
        if(cy_>=sr_+er) sr_=cy_-er+1;
        if(cx_<sc_) sc_=cx_;
        if(cx_>=sc_+cols-6) sc_=cx_-(cols-6)+1;

        std::ostringstream b;

        // header
        b<<"\x1b[H\x1b[7m";
        std::string title="  .aura editor  |  "+(filename_.empty()?"[no name]":filename_);
        if(dirty_) title+="  [+]";
        std::string ctrl="  ^S save  ^R run  ^N new  ^Q quit  ";
        int gap=cols-(int)title.size()-(int)ctrl.size(); if(gap<1)gap=1;
        b<<title<<std::string(gap,' ')<<ctrl<<"\x1b[0m\r\n";

        // lines
        for(int r=0;r<er;++r){
            int li=sr_+r; b<<"\x1b[2K";
            if(li<(int)lines_.size()){
                char ln[16]; snprintf(ln,sizeof(ln),"%4d ",li+1);
                b<<"\x1b[90m"<<ln<<"\x1b[0m";
                const auto& line=lines_[li];
                int s=sc_, e=std::min((int)line.size(),s+cols-5);
                if(s<(int)line.size()) b<<line.substr(s,e-s);
            } else {
                b<<"\x1b[90m   ~\x1b[0m";
            }
            b<<"\r\n";
        }

        // status bar
        b<<"\x1b[7m\x1b[2K";
        std::string pos=" Ln "+std::to_string(cy_+1)+"  Col "+std::to_string(cx_+1)+" ";
        std::string msg=status_.empty()?"  stay sigma. yap something.":"  "+status_;
        int gap2=cols-(int)pos.size()-(int)msg.size(); if(gap2<0)gap2=0;
        b<<msg<<std::string(gap2,' ')<<pos<<"\x1b[0m";

        // place cursor
        int sr=(cy_-sr_)+2, sc=(cx_-sc_)+6;
        b<<"\x1b["<<sr<<";"<<sc<<"H";
        std::cout<<b.str(); term::flush();
    }

    bool handle_key(int k){
        status_.clear();
        if(k==('q'&0x1f)){ // Ctrl-Q
            if(dirty_){ status_="Unsaved changes! Ctrl-Q again to force quit."; dirty_=false; return true; }
            return false;
        }
        if(k==('s'&0x1f)){ do_save(); return true; }
        if(k==('r'&0x1f)){ do_run();  return true; }
        if(k==('n'&0x1f)){ do_new();  return true; }

        switch(k){
            case term::KEY_UP:    cy_=std::max(0,cy_-1); cx_=std::min(cx_,(int)lines_[cy_].size()); break;
            case term::KEY_DOWN:  cy_=std::min((int)lines_.size()-1,cy_+1); cx_=std::min(cx_,(int)lines_[cy_].size()); break;
            case term::KEY_LEFT:  if(cx_>0)--cx_; else if(cy_>0){--cy_;cx_=(int)lines_[cy_].size();} break;
            case term::KEY_RIGHT: if(cx_<(int)lines_[cy_].size())++cx_; else if(cy_<(int)lines_.size()-1){++cy_;cx_=0;} break;
            case term::KEY_HOME:  cx_=0; break;
            case term::KEY_END:   cx_=(int)lines_[cy_].size(); break;
            case '\r': case '\n': {
                std::string rest=lines_[cy_].substr(cx_);
                lines_[cy_]=lines_[cy_].substr(0,cx_);
                lines_.insert(lines_.begin()+cy_+1,rest);
                ++cy_; cx_=0; dirty_=true; break;
            }
            case 127: case '\b':
                if(cx_>0){lines_[cy_].erase(cx_-1,1);--cx_;dirty_=true;}
                else if(cy_>0){cx_=(int)lines_[cy_-1].size();lines_[cy_-1]+=lines_[cy_];lines_.erase(lines_.begin()+cy_);--cy_;dirty_=true;}
                break;
            case term::KEY_DEL:
                if(cx_<(int)lines_[cy_].size()){lines_[cy_].erase(cx_,1);dirty_=true;}
                else if(cy_+1<(int)lines_.size()){lines_[cy_]+=lines_[cy_+1];lines_.erase(lines_.begin()+cy_+1);dirty_=true;}
                break;
            default:
                if(k>=32&&k<127){lines_[cy_].insert(lines_[cy_].begin()+cx_,(char)k);++cx_;dirty_=true;}
                break;
        }
        return true;
    }

    void do_save(){
        if(filename_.empty()){
            filename_=term::prompt_line(trows(),tcols(),"Save as (.aura): ");
            if(filename_.empty()){status_="Save cancelled.";return;}
        }
        // FIX: only call aura_path once, and only when we actually need it —
        // not on every subsequent save (avoids double-extension if already set).
        filename_=aura_path(filename_);
        try{ save_aura(filename_,lines_); dirty_=false; status_="Saved to "+filename_+" — no cap."; }
        catch(std::exception& e){ status_=std::string("Save failed: ")+e.what(); }
    }

    void do_new(){
        std::string name=term::prompt_line(trows(),tcols(),"New file name (.aura): ");
        if(name.empty()){status_="Cancelled.";return;}
        filename_=aura_path(name); lines_={""}; cx_=cy_=sr_=sc_=0; dirty_=false;
        status_="New file: "+filename_;
    }

    void do_run(){
        std::string src; for(auto& l:lines_){src+=l;src+='\n';}
        term::disable_raw(); term::clear(); term::move(1,1);
        std::cout<<"\x1b[1m── BrainRot Running ── \x1b[0m\n\n"; term::flush();
        try{ run_source(src); }
        catch(LowKeySignal&){ std::cout<<"\n\x1b[90m[low-key — process ended]\x1b[0m\n"; }
        catch(std::exception& e){ std::cout<<"\n\x1b[31mError: "<<e.what()<<"\x1b[0m\n"; }
        std::cout<<"\n\x1b[90m── Press any key to return ──\x1b[0m\n"; term::flush();
        term::enable_raw(); term::read_key();
        status_="Run complete. back to grinding.";
    }
};

// ═══════════════════════════════════════════════════════════════
//  main
// ═══════════════════════════════════════════════════════════════

int main(int argc,char* argv[]){
    try{
        if(argc==3&&std::string(argv[1])=="run"){
            run_source(load_aura(aura_path(argv[2])));
        } else if(argc==3&&std::string(argv[1])=="edit"){
            Editor(aura_path(argv[2])).run();
        } else if(argc==1){
            Editor().run();
        } else {
            std::cerr<<"Usage:\n"
                     <<"  brainrot                   open editor (empty)\n"
                     <<"  brainrot edit <file.aura>  open editor with file\n"
                     <<"  brainrot run  <file.aura>  run a .aura source file\n";
            return 1;
        }
    } catch(LowKeySignal&){
        // clean exit
    } catch(std::exception& e){
        std::cerr<<"BrainRot error: "<<e.what()<<'\n'; return 1;
    }
    return 0;
}