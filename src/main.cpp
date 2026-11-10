#include "httplib.h"
#include "assets.hpp"
#include "update_support.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <variant>
#include <cmath>
#include <thread>
#include <condition_variable>
#include <atomic>
using namespace std;
namespace fs = std::filesystem;
struct Error : runtime_error { using runtime_error::runtime_error; };
struct Token { string text; bool quoted=false; };
struct Node { int line=0; vector<Token> t; vector<Node> children; };
string filename;
[[noreturn]] void fail(int line, const string& s) { throw Error(filename+":"+to_string(line)+": "+s); }
string trim(string s) { auto a=s.find_first_not_of(" \r\n\t"); return a==string::npos?"":s.substr(a,s.find_last_not_of(" \r\n\t")-a+1); }
string esc(const string& s) { string r; for(char c:s) switch(c){case '&':r+="&amp;";break;case '<':r+="&lt;";break;case '>':r+="&gt;";break;case '"':r+="&quot;";break;case '\'':r+="&#39;";break;default:r+=c;} return r; }
fs::path utf8Path(const string& text) { u8string value;for(unsigned char c:text)value+=char8_t(c);return fs::path(value); }
string pathText(const fs::path& path) { auto value=path.u8string();return string(reinterpret_cast<const char*>(value.data()),value.size()); }
string readFile(const fs::path& p) { ifstream f(p,ios::binary); if(!f) throw Error("Cannot read "+pathText(p)); return string(istreambuf_iterator<char>(f),{}); }
void writeFile(const fs::path& p, const string& s) { ofstream f(p,ios::binary); if(!f || !(f<<s)) throw Error("Cannot write "+pathText(p)); f.close(); if(!f) throw Error("Cannot finish writing "+pathText(p)); }
vector<Token> lex(string s,int line) {
    vector<Token> ts; size_t i=0;
    while(i<s.size()) {
        if(isspace((unsigned char)s[i])){++i;continue;} if(s[i]=='#') break;
        Token t;
        if(s[i]=='"') { t.quoted=true; ++i; bool closed=false; while(i<s.size()) { char c=s[i++]; if(c=='"'){closed=true;break;} if(c=='\\'){if(i==s.size())fail(line,"Unfinished escape.");c=s[i++]; if(c=='n')c='\n';else if(c=='t')c='\t';else if(c!='"'&&c!='\\')fail(line,"Use \\n, \\t, \\\" or \\\\ inside text.");} t.text+=c;} if(!closed)fail(line,"Close this text with a double quote."); }
        else { while(i<s.size()&&!isspace((unsigned char)s[i])&&s[i]!=':'&&s[i]!='('&&s[i]!=')'&&s[i]!=','&&s[i]!='#') t.text+=s[i++]; if(t.text.empty()){t.text=string(1,s[i++]);} }
        ts.push_back(t);
    } return ts;
}
vector<Node> parse(const string& source) {
    struct Line{int indent;Node node;}; vector<Line> lines; istringstream in(source); string s; int n=0;
    while(getline(in,s)) {
        ++n;size_t i=0,columns=0;
        while(i<s.size()&&(s[i]==' '||s[i]=='\t')){columns+=s[i]=='\t'?4-columns%4:1;++i;}
        auto t=lex(s.substr(i),n);if(t.empty())continue;
        if(columns%4)fail(n,"Indent with four spaces or one tab per level.");
        if(!t.back().quoted&&t.back().text==":")t.pop_back();
        if(t.empty())fail(n,"Expected a command before ':'.");
        if(t[0].text=="navigation"&&(t.size()==1||(t.size()==2&&t[1].text=="bar")))t={{"nav",false}};
        lines.push_back({int(columns/4),{n,t,{}}});
    }
    size_t pos=0; function<vector<Node>(int)> block=[&](int depth){vector<Node> out;while(pos<lines.size()&&lines[pos].indent>=depth){if(lines[pos].indent!=depth)fail(lines[pos].node.line,"Indent only one level inside the previous command.");Node node=lines[pos++].node;if(pos<lines.size()&&lines[pos].indent>depth)node.children=block(depth+1);out.push_back(std::move(node));}return out;};
    return block(0);
}
using List=vector<string>;
using Value=variant<string,double,bool,List>;
using Env=map<string,Value>;
string str(const Value& v) {if(auto p=get_if<string>(&v))return *p;if(auto p=get_if<double>(&v)){ostringstream o;o<<setprecision(14)<<*p;return o.str();}if(auto p=get_if<bool>(&v))return *p?"true":"false";return to_string(get<List>(v).size());}
bool truth(const Value& v){if(auto p=get_if<bool>(&v))return *p;if(auto p=get_if<double>(&v))return *p!=0;if(auto p=get_if<List>(&v))return !p->empty();return !get<string>(v).empty();}
bool ident(const string& s){return !s.empty()&&(isalpha((unsigned char)s[0])||s[0]=='_')&&all_of(s.begin(),s.end(),[](unsigned char c){return isalnum(c)||c=='_';});}
struct Engine {
    vector<Node> roots; map<string,const Node*> pages,components; set<string> fragments; Env state; set<string> persistent; map<string,int> declarations; size_t revision=0; map<string,vector<string>> parameters; map<string,string> namedRoutes;
    explicit Engine(vector<Node> r):roots(std::move(r)) {
        for(auto& n:roots) {
            auto& t=n.t;
            if(t.size()<4||t[0].text!="set"||t[2].text!="to"||t[3].text!="new")continue;
            if(t.size()!=8||!ident(t[1].text)||t[4].text!="page"||t[5].text!="with"||t[6].text!="name"||!t[7].quoted)
                fail(n.line,"Write set home to new page with name \"Home\":");
            string symbol=t[1].text,title=t[7].text,route;bool suffix=false;vector<Node> content;
            for(auto& child:n.children) {
                if(child.t[0].text!="suffix") { content.push_back(std::move(child));continue; }
                if(suffix||child.t.size()!=3||child.t[1].text!=":"||!child.children.empty())fail(child.line,"Give the page one suffix: home, or suffix: / for the root.");
                suffix=true;route=child.t[2].text;if(route.empty())route="/";else if(route[0]!='/')route="/"+route;
            }
            if(!suffix)fail(n.line,"Add suffix: home or suffix: / inside the named page.");
            if(!namedRoutes.emplace(symbol,route).second)fail(n.line,"This page name is already declared.");
            n.t={{"page",false},{title,true},{route,false}};n.children=std::move(content);
        }
        for(auto& n:roots){auto& t=n.t;string k=t[0].text;
            if(k=="page"||k=="fragment") {if(t.size()!=3||!t[1].quoted||t[2].quoted||t[2].text.empty()||t[2].text[0]!='/')fail(n.line,"Write page \"Name\" /route.");string route=t[2].text;if(!regex_match(route,regex(R"(/([A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)*)?)")))fail(n.line,"Use / or a route like /my-page, without trailing slashes.");if(route.find_first_of("?#\\<>{}")!=string::npos||route.find("..")!=string::npos||route.rfind("/_tappy",0)==0||route.rfind("//",0)==0)fail(n.line,"Use a local URL path outside /_tappy.");if(!pages.emplace(route,&n).second)fail(n.line,"This page route is already used.");if(k=="fragment"){if(route=="/")fail(n.line,"Use a non-root route for a fragment.");fragments.insert(route);}}
            else if(k=="component") {if(t.size()<2||!ident(t[1].text)||!components.emplace(t[1].text,&n).second)fail(n.line,"Give each component a unique name.");
                vector<string> names;
                if(t.size()>2){if(t.size()<4||t[2].text!="("||t.back().text!=")")fail(n.line,"Write component Metric(label, value):");
                    for(size_t i=3;i+1<t.size();++i){if(!ident(t[i].text)||find(names.begin(),names.end(),t[i].text)!=names.end())fail(n.line,"Component parameters must have unique names.");names.push_back(t[i].text);if(i+2<t.size()){if(t[++i].text!=","||i+2==t.size())fail(n.line,"Separate component parameters with commas.");}}
                }parameters[t[1].text]=names;}
            else if(k=="remember"||k=="state"||k=="set"||k=="save") {if(t.size()<4||!ident(t[1].text)||(t[2].text!=(k=="set"?"to":"as"))||!n.children.empty())fail(n.line,"Write set count to 0 or save tasks as [].");string name=t[1].text;if(state.count(name)||namedRoutes.count(name))fail(n.line,"This name is already declared.");state[name]=expr(t,3,t.size(),state,n.line);declarations[name]=n.line;if(k=="remember"||k=="save")persistent.insert(name);}
            else fail(n.line,"At the top level use page, fragment, component, set or save.");
        }
        for(auto& [name,names]:parameters)for(auto& parameter:names)if(state.count(parameter)||namedRoutes.count(parameter))fail(components.at(name)->line,"Component parameters must not shadow global variables.");
        if(pages.size()==fragments.size())throw Error("Add at least one page, for example: page \"Home\" /");
    }
    Value lookup(string name,const Env& e,int line) const {
        auto it=e.find(name);if(it!=e.end())return it->second;
        auto page=namedRoutes.find(name);if(page!=namedRoutes.end())return page->second;
        if(name.ends_with(".value"))return lookup(name.substr(0,name.size()-6),e,line);
        if(name.ends_with(".count")){auto v=lookup(name.substr(0,name.size()-6),e,line);if(auto p=get_if<List>(&v))return double(p->size());fail(line,".count works on a list.");}
        fail(line,"I don't know '"+name+"'. Declare it with set/save, or name an input with as.");
    }
    string interpolate(const string& s,const Env& e,int line) const {string out;size_t p=0;while(p<s.size()){auto a=s.find('{',p);if(a==string::npos){out+=s.substr(p);break;}out+=s.substr(p,a-p);auto b=s.find('}',a);if(b==string::npos)fail(line,"Close the {variable} in this text.");out+=str(lookup(s.substr(a+1,b-a-1),e,line));p=b+1;}return out;}
    Value atom(const Token& t,const Env& e,int line) const {
        if(t.quoted)return interpolate(t.text,e,line);if(t.text=="[]")return List{};if(t.text=="true")return true;if(t.text=="false")return false;
        char* end=nullptr;double d=strtod(t.text.c_str(),&end);if(end!=t.text.c_str()&&*end=='\0'&&isfinite(d))return d;
        return lookup(t.text,e,line);
    }
    Value expr(const vector<Token>& source,size_t a,size_t b,const Env& e,int line) const {
        vector<Token> t;
        for(size_t i=a;i<b;++i){if(source[i].quoted){t.push_back(source[i]);continue;}
            string part;auto emit=[&]{if(!part.empty()){t.push_back({part,false});part.clear();}};
            const string& raw=source[i].text;
            for(size_t j=0;j<raw.size();++j){char c=raw[j];
                // Keep scientific-notation signs attached to the number.
                bool exponent=(c=='+'||c=='-')&&!part.empty()&&(part.back()=='e'||part.back()=='E')&&(isdigit((unsigned char)part[0])||part[0]=='.');
                if(string("+-*/()").find(c)!=string::npos&&!exponent){emit();t.push_back({string(1,c),false});}else part+=c;
            }emit();
        }
        size_t pos=0;int nesting=0;
        function<Value(int)> expression;
        function<Value()> primary=[&]() -> Value {
            if(++nesting>128)fail(line,"Expression nesting is too deep.");
            if(pos>=t.size())fail(line,"Expected a value.");
            Token token=t[pos++];Value v;
            if(!token.quoted&&(token.text=="+"||token.text=="-")){v=primary();auto number=get_if<double>(&v);if(!number)fail(line,"Unary + and - need a number.");if(token.text=="-")v=-*number;}
            else if(!token.quoted&&token.text=="("){v=expression(0);if(pos>=t.size()||t[pos].quoted||t[pos++].text!=")")fail(line,"Close the expression with ')'.");}
            else if(!token.quoted&&(token.text==")"||token.text=="*"||token.text=="/"))fail(line,"Expected a value.");
            else v=atom(token,e,line);
            --nesting;return v;
        };
        expression=[&](int minimum) -> Value {
            Value left=primary();
            while(pos<t.size()&&!t[pos].quoted){string op=t[pos].text;int precedence=(op=="+"||op=="-")?1:(op=="*"||op=="/")?2:-1;if(precedence<minimum)break;
                ++pos;Value right=expression(precedence+1);
                if(op=="+"&&(holds_alternative<string>(left)||holds_alternative<string>(right)))left=str(left)+str(right);
                else {auto x=get_if<double>(&left),y=get_if<double>(&right);if(!x||!y)fail(line,"Arithmetic needs numbers (or text + text).");if(op=="/"&&*y==0)fail(line,"Cannot divide by zero.");double result=op=="+"?*x+*y:op=="-"?*x-*y:op=="*"?*x * *y:*x / *y;if(!isfinite(result))fail(line,"This number is too large.");left=result;}
            }return left;
        };
        if(t.empty())fail(line,"Expected a value.");Value result=expression(0);if(pos!=t.size())fail(line,"Use +, -, * or / between values.");return result;
    }
    pair<string,Env> bind(const Node& n,const Env& e) const {
        const auto& t=n.t;
        if(t.size()<2||!components.count(t[1].text))fail(n.line,"Use a declared component name.");
        string name=t[1].text;const auto& names=parameters.at(name);vector<Value> args;
        if(t.size()>2){if(t.size()<4||t[2].text!="("||t.back().text!=")")fail(n.line,"Write use Metric(\"Label\", value).");
            size_t start=3;int depth=0;
            for(size_t i=3;i<t.size()-1;++i){if(!t[i].quoted&&t[i].text=="(")++depth;else if(!t[i].quoted&&t[i].text==")")--depth;
                if(!t[i].quoted&&t[i].text==","&&depth==0){args.push_back(expr(t,start,i,e,n.line));start=i+1;}}
            if(start<t.size()-1)args.push_back(expr(t,start,t.size()-1,e,n.line));else if(start!=3)fail(n.line,"Expected an argument after ','.");
        }
        if(args.size()!=names.size())fail(n.line,"Component '"+name+"' expects "+to_string(names.size())+" arguments.");
        Env bound;for(size_t i=0;i<names.size();++i)bound[names[i]]=args[i];return {name,bound};
    }
    bool condition(const Node& n,const Env& e) const {
        const auto& t=n.t;int depth=0;
        for(size_t i=1;i<t.size();++i){if(t[i].quoted)continue;if(t[i].text=="(")++depth;else if(t[i].text==")")--depth;
            string op=t[i].text;
            if(depth!=0||(op!="is"&&op!="=="&&op!="!="&&op!=">"&&op!="<"&&op!=">="&&op!="<="))continue;
            Value a=expr(t,1,i,e,n.line),b=expr(t,i+1,t.size(),e,n.line);
            if(op=="is"||op=="==")return a==b;if(op=="!=")return a!=b;
            auto x=get_if<double>(&a),y=get_if<double>(&b);if(!x||!y)fail(n.line,"Compare numbers with <, >, <= or >=.");
            if(op==">")return *x>*y;if(op=="<")return *x<*y;if(op==">=")return *x>=*y;return *x<=*y;
        }return truth(expr(t,1,t.size(),e,n.line));
    }

};
string token(){static random_device rd;ostringstream s;for(int i=0;i<4;++i)s<<hex<<setw(8)<<setfill('0')<<rd();return s.str();}
struct InputRule {string name,type="text";bool required=false;};
void gatherInputs(const Engine& app,const vector<Node>& nodes,map<string,InputRule>& result,set<string> seen={}) {
    for(auto& n:nodes){auto& t=n.t;string k=t[0].text;
        if((k=="input"||k=="textarea")&&t.size()>=4){InputRule r;r.name=t[3].text;for(size_t i=4;i<t.size();++i){if(t[i].text=="required")r.required=true;else if(t[i].text=="type"&&i+1<t.size())r.type=t[++i].text;}if(app.state.count(r.name)||app.namedRoutes.count(r.name)||result.count(r.name))fail(n.line,"Input names must be unique on a page and different from state names.");result[r.name]=r;}
        if(k=="use"&&t.size()>=2&&app.components.count(t[1].text)){if(!seen.insert(t[1].text).second)fail(n.line,"Components cannot include themselves.");gatherInputs(app,app.components.at(t[1].text)->children,result,seen);seen.erase(t[1].text);}
        gatherInputs(app,n.children,result,seen);
    }
}
struct Action {const Node* event;Env locals;string route;size_t revision;map<string,InputRule> inputs;};
struct Session {string csrf=token();map<string,Action> actions;};
const set<string> styles={"background","color","size","padding","rounded","gap","width","height","columns","center","bold","muted","id","htmx"};
const set<string> containers={"row","stack","grid","card","section","nav","footer"};
struct Renderer {
    Engine& app;Session& session;string route;bool staticMode=false;int depth=0;size_t budget=100000;set<string> expanding;map<string,InputRule> activeInputs;
    Renderer(Engine& a,Session& s,string r,bool st=false):app(a),session(s),route(std::move(r)),staticMode(st){}
    string text(const Node& n,size_t a,size_t b,const Env& e){return str(app.expr(n.t,a,b,e,n.line));}
    string actionAttrs(const Node& event,const Env& locals,const Node& owner) {
        if(staticMode)fail(event.line,"This page has server actions. Use serve; build exports static pages only.");
        map<string,InputRule> inputs;
        if(owner.t[0].text=="form")gatherInputs(app,owner.children,inputs);
        else if(!activeInputs.empty())inputs=activeInputs;
        else {gatherInputs(app,app.pages.at(route)->children,inputs);for(auto& [name,rule]:inputs)rule.required=false;}
        if(session.actions.size()>=20000)throw Error("Too many pending actions. Reload the page.");
        string id=token();session.actions[id]={&event,locals,route,app.revision,inputs};return " hx-post=\"/_tappy/action/"+id+"\" hx-target=\"#app\" hx-swap=\"outerHTML\"";
    }
    string style(const vector<Node>& nodes,const Env& e) {
        string out,attrs;set<string> seenAttrs;for(auto& n:nodes){string k=n.t[0].text;if(!styles.count(k))continue;
            if(k=="id"){if(n.t.size()!=2||!n.t[1].quoted||!ident(n.t[1].text))fail(n.line,"Write id \"name\" using a unique identifier.");if(n.t[1].text=="app")fail(n.line,"The id app is reserved for the runtime.");attrs+=" id=\""+esc(n.t[1].text)+"\"";continue;}
            if(k=="htmx"){
                static const set<string> allowed={"hx-get","hx-post","hx-put","hx-patch","hx-delete","hx-target","hx-swap","hx-trigger","hx-boost","hx-push-url","hx-replace-url","hx-select","hx-select-oob","hx-swap-oob","hx-sync","hx-include","hx-params","hx-vals","hx-headers","hx-confirm","hx-prompt","hx-indicator","hx-disabled-elt","hx-disable","hx-disinherit","hx-inherit","hx-encoding","hx-history","hx-history-elt","hx-preserve","hx-request","hx-validate"};
                if(n.t.size()!=3||!n.t[1].quoted||!n.t[2].quoted||!allowed.count(n.t[1].text))fail(n.line,"Write htmx \"hx-attribute\" \"value\". JavaScript evaluation and extensions are not supported.");
                string name=n.t[1].text,value=app.interpolate(n.t[2].text,e,n.line);if(!seenAttrs.insert(name).second)fail(n.line,"This HTMX attribute is already set on the element.");
                if(name=="hx-get"||name=="hx-post"||name=="hx-put"||name=="hx-patch"||name=="hx-delete"){if(value.empty()||value[0]!='/'||value.rfind("//",0)==0||value.find('\\')!=string::npos)fail(n.line,"HTMX requests must use a local /path.");}
                if(value.rfind("js:",0)==0||value.rfind("javascript:",0)==0)fail(n.line,"HTMX values cannot run JavaScript.");attrs+=" "+name+"=\""+esc(value)+"\"";continue;
            }
            if(k=="center"||k=="bold"||k=="muted"){if(n.t.size()!=1)fail(n.line,k+" takes no value.");out+=k=="center"?"text-align:center;":k=="bold"?"font-weight:700;":"color:#73738b;";continue;}
            string v=text(n,1,n.t.size(),e);if(v.find_first_of(";{}<>\\\"'")!=string::npos||v.find("url(")!=string::npos)fail(n.line,"Use a simple CSS value, without CSS rules or URLs.");
            if(k=="columns"){if(v.empty()||!all_of(v.begin(),v.end(),[](unsigned char c){return isdigit(c);})||v.size()>2||stoi(v)<1||stoi(v)>12)fail(n.line,"columns needs a number from 1 to 12.");out+="grid-template-columns:repeat("+v+",minmax(0,1fr));";continue;}
            map<string,string> props={{"size","font-size"},{"rounded","border-radius"},{"background","background"},{"color","color"},{"padding","padding"},{"gap","gap"},{"width","width"},{"height","height"}};
            if(k!="color"&&k!="background"&&!v.empty()&&all_of(v.begin(),v.end(),[](unsigned char c){return isdigit(c)||c=='.';}))v+="px";out+=props[k]+":"+v+";";
        }return attrs+(out.empty()?"":" style=\""+esc(out)+"\"");
    }
    const Node* event(const Node& n,const string& kind){const Node* found=nullptr;for(auto& c:n.children)if(c.t[0].text=="on"){if(c.t.size()!=2||c.t[1].text!=kind)fail(c.line,"Use on "+kind+" here.");if(found)fail(c.line,"Only one event block is allowed here.");found=&c;}return found;}
    string safeURL(string s,int line){if(s.rfind("/",0)==0&&s.rfind("//",0)!=0&&s.find('\\')==string::npos)return s;if(s.rfind("https://",0)==0||s.rfind("http://",0)==0)return s;fail(line,"Use a local /path or an http:// or https:// URL.");}
    string render(const vector<Node>& nodes,Env e,Env locals={}) {
        if(++depth>64)throw Error("Page nesting is too deep.");string out;bool previousIf=false,matched=false;
        for(auto& n:nodes){if(budget--==0)fail(n.line,"Page expansion is too large.");auto& t=n.t;string k=t[0].text;
            Env child=e;
            if(k=="else"){if(!previousIf||t.size()!=1)fail(n.line,"Put else immediately after if.");if(!matched)out+=render(n.children,e,locals);previousIf=false;continue;}
            previousIf=false;if(styles.count(k)||k=="on")continue;
            if(k=="if"){matched=app.condition(n,e);previousIf=true;if(matched)out+=render(n.children,e,locals);continue;}
            if(k=="for"){if(t.size()!=5||t[1].text!="each"||(!ident(t[2].text)||app.namedRoutes.count(t[2].text))||t[3].text!="in")fail(n.line,"Write for each task in tasks:");Value v=app.lookup(t[4].text,e,n.line);auto list=get_if<List>(&v);if(!list)fail(n.line,"Loop over a list declared as [].");for(size_t i=0;i<list->size();++i){Env next=e,l=locals;next[t[2].text]=l[t[2].text]=(*list)[i];next[t[2].text+".index"]=l[t[2].text+".index"]=double(i);out+=render(n.children,next,l);}continue;}
            if(k=="use"){auto [name,bound]=app.bind(n,e);Env scoped=e,captured=locals;for(auto& [key,value]:bound)scoped[key]=captured[key]=value;if(!expanding.insert(name).second)fail(n.line,"Components cannot include themselves.");out+=render(app.components.at(name)->children,scoped,captured);expanding.erase(name);continue;}
            auto savedInputs=activeInputs;string tag,body,attrs;bool leaf=false;
            if(containers.count(k)){if(t.size()!=1)fail(n.line,k+" takes no arguments.");tag=k=="nav"||k=="section"||k=="footer"?k:"div";attrs=" class=\""+k+"\"";}
            else if(k=="heading"||k=="subheading"||k=="text"){tag=k=="heading"?"h1":k=="subheading"?"h2":"p";body=esc(text(n,1,t.size(),e));}
            else if(k=="link"){if(t.size()!=3||!t[1].quoted)fail(n.line,"Write link \"Label\" /path.");tag="a";body=esc(text(n,1,2,e));string url=safeURL(t[2].quoted?app.interpolate(t[2].text,e,n.line):(app.namedRoutes.count(t[2].text)?app.namedRoutes.at(t[2].text):t[2].text),n.line);attrs=" href=\""+esc(url)+"\"";if(app.pages.count(url)&&!app.fragments.count(url))attrs+=" hx-get=\""+esc(url)+"\" hx-target=\"#app\" hx-swap=\"outerHTML\" hx-push-url=\"true\"";}
            else if(k=="button"){tag="button";body=esc(text(n,1,t.size(),e));auto ev=event(n,"click");attrs=ev?" type=\"button\""+actionAttrs(*ev,locals,n)+" hx-include=\"closest main\"":" type=\"submit\"";}
            else if(k=="form"){if(t.size()!=1)fail(n.line,"Write form: with inputs inside.");if(!activeInputs.empty())fail(n.line,"Forms cannot be nested.");tag="form";gatherInputs(app,n.children,activeInputs);auto ev=event(n,"submit");if(!ev)fail(n.line,"Add on submit: inside this form.");attrs=actionAttrs(*ev,locals,n)+" method=\"post\"";}
            else if(k=="input"||k=="textarea") {
                if(t.size()<4||!t[1].quoted||t[2].text!="as"||!ident(t[3].text))fail(n.line,"Write input \"Label\" as name [required] [type email].");
                string name=t[3].text,more;for(size_t j=4;j<t.size();++j){if(t[j].text=="required")more+=" required";else if((t[j].text=="type"||t[j].text=="placeholder")&&j+1<t.size()){string key=t[j++].text;string v=t[j].text;if(key=="type"&&v!="text"&&v!="email"&&v!="number"&&v!="password"&&v!="date")fail(n.line,"Supported input types: text, email, number, password, date.");more+=" "+key+"=\""+esc(v)+"\"";}else fail(n.line,"Unknown input option.");}
                if(!n.children.empty())fail(n.line,"Inputs cannot contain nested commands.");string label=esc(text(n,1,2,e));out+="<label>"+label+"<"+k+" name=\""+name+"\" id=\"input-"+name+"\""+more+(k=="input"?">":" ></textarea>")+"</label>";continue;
            }
            else if(k=="divider"){if(t.size()!=1||!n.children.empty())fail(n.line,"divider takes no arguments.");out+="<hr>";continue;}
            else if(k=="image"){if(t.size()!=3||!t[1].quoted||!t[2].quoted)fail(n.line,"Write image \"https://...\" \"Description\".");tag="img";attrs=" src=\""+esc(safeURL(text(n,1,2,e),n.line))+"\" alt=\""+esc(text(n,2,3,e))+"\"";leaf=true;}
            else fail(n.line,"Unknown page command '"+k+"'. Try heading, text, card, input, form or button.");
            if(k=="button"||k=="form"||k=="link")for(auto& c:n.children)if(c.t[0].text=="htmx"&&c.t.size()==3){string a=c.t[1].text;bool generated=k=="form"||(k=="button"&&event(n,"click"))||(k=="link"&&attrs.find(" hx-get=")!=string::npos);if(generated&&(a=="hx-get"||a=="hx-post"||a=="hx-put"||a=="hx-patch"||a=="hx-delete"||a=="hx-target"||a=="hx-swap"||a=="hx-push-url"||a=="hx-include"))fail(c.line,"This element already has runtime-managed HTMX behavior. Put custom HTMX attributes on a separate element.");}
            attrs+=style(n.children,e);out+="<"+tag+attrs+">"+body;if(!leaf)out+=render(n.children,e,locals)+"</"+tag+">";activeInputs=savedInputs;
        }--depth;return out;
    }
    string page(const string& error="") {
        Env e=app.state;string body=render(app.pages.at(route)->children,e);if(app.fragments.count(route))return body;
        return "<main id=\"app\""+style(app.pages.at(route)->children,e)+" hx-headers=\"{&quot;X-Tappy-CSRF&quot;:&quot;"+session.csrf+"&quot;}\" hx-history=\"false\"><title>"+esc(app.pages.at(route)->t[1].text)+"</title>"+(error.empty()?"":"<p role=\"alert\" class=\"tappy-error\">"+esc(error)+"</p>")+body+"</main>";
    }
};
struct Executor {
    Engine& app;Env locals;set<string> inputNames;string redirect;bool stopped=false;size_t budget=10000;
    Executor(Engine& a,Env l):app(a),locals(std::move(l)){}
    Env env()const{Env e=app.state;for(auto& [k,v]:locals)e[k]=v;return e;}
    void run(const vector<Node>& nodes,bool validate=false) {
        bool previous=false,matched=false;
        for(auto& n:nodes){if(stopped&&!validate)break;if(budget--==0)fail(n.line,"Too many actions in one request.");auto& t=n.t;string k=t[0].text;Env e=env();
            Env child=e;
            if(k=="else"){if(!previous||t.size()!=1)fail(n.line,"Put else immediately after if.");if(!matched||validate)run(n.children,validate);previous=false;continue;}previous=false;
            if(k=="if"){matched=app.condition(n,e);previous=true;if(matched||validate)run(n.children,validate);continue;}
            if(!n.children.empty())fail(n.line,"Only if/else can contain action blocks.");
            if(k=="set"){if(t.size()<4||t[2].text!="to"||!app.state.count(t[1].text))fail(n.line,"Write set declaredVariable to value.");Value value=app.expr(t,3,t.size(),e,n.line);if(value.index()!=app.state.at(t[1].text).index())fail(n.line,"Keep the same type when setting a variable.");if(!validate)app.state[t[1].text]=value;}
            else if(k=="add"){if(t.size()<4||t[t.size()-2].text!="to")fail(n.line,"Write add value to list.");string name=t.back().text;if(!app.state.count(name)||!holds_alternative<List>(app.state.at(name)))fail(n.line,"Add items to a declared list.");Value v=app.expr(t,1,t.size()-2,e,n.line);if(!holds_alternative<string>(v))fail(n.line,"MVP lists contain text items.");if(!validate){auto& list=get<List>(app.state[name]);if(list.size()>=10000)fail(n.line,"A list can contain at most 10,000 items.");list.push_back(get<string>(v));}}
            else if(k=="remove"){if(t.size()!=4||t[2].text!="from"||!app.state.count(t[3].text)||!holds_alternative<List>(app.state.at(t[3].text)))fail(n.line,"Write remove task from tasks (inside a loop), or remove index from list.");Value v=app.atom(t[1],e,n.line);double index=-1;if(holds_alternative<double>(v))index=get<double>(v);else if(e.count(t[1].text+".index"))index=get<double>(e.at(t[1].text+".index"));else fail(n.line,"Remove using a loop item or a numeric index.");if(!validate){auto& list=get<List>(app.state[t[3].text]);if(index<0||floor(index)!=index||index>=double(list.size()))fail(n.line,"That list item no longer exists. Refresh and try again.");list.erase(list.begin()+size_t(index));}}
            else if(k=="clear"){if(t.size()!=2)fail(n.line,"Write clear list.");if(app.state.count(t[1].text)&&holds_alternative<List>(app.state[t[1].text])){if(!validate)get<List>(app.state[t[1].text]).clear();}else if(inputNames.count(t[1].text)){if(!validate)locals[t[1].text]=string{};}else fail(n.line,"clear expects a declared list or a named form input.");}
            else if(k=="open"){if(t.size()!=2)fail(n.line,"Write open \"/route\".");string url=str(app.atom(t[1],e,n.line));if(!app.pages.count(url))fail(n.line,"open needs a declared page route.");if(!validate){redirect=url;stopped=true;}}
            else if(k=="stop"){if(t.size()!=1)fail(n.line,"stop takes no arguments.");if(!validate)stopped=true;}
            else fail(n.line,"Unknown action '"+k+"'. Use set, add, remove, clear, open, if or stop.");
        }
    }
};
void validate(Engine& app) {
    Session session;
    auto validationPages=app.pages;vector<Node> synthetic;synthetic.reserve(app.components.size());
    for(auto& [name,component]:app.components){if(!app.parameters.at(name).empty())continue;Node n;n.line=component->line;n.t={{"page",false},{name,true},{"/__check_"+name,false}};n.children=component->children;synthetic.push_back(std::move(n));}
    for(auto& n:synthetic)validationPages[n.t[2].text]=&n;
    for(auto& [route,page]:validationPages){bool temporary=!app.pages.count(route);if(temporary)app.pages[route]=page;map<string,InputRule> inputs;gatherInputs(app,page->children,inputs);Env base=app.state;for(auto& [name,r]:inputs)base[name]=string{};set<string> used;
        function<void(const vector<Node>&,Env,string)> walk=[&](const vector<Node>& nodes,Env e,string parent){bool prev=false;for(auto& n:nodes){string k=n.t[0].text;
            Env child=e;
            if(k=="else"){if(!prev||n.t.size()!=1)fail(n.line,"Put else immediately after if.");}prev=k=="if";
            if((parent=="heading"||parent=="subheading"||parent=="text"||parent=="link"||parent=="image")&&!styles.count(k))fail(n.line,"Only style or HTMX attributes belong inside this element.");
            if(parent=="button"&&k!="on"&&!styles.count(k))fail(n.line,"Inside a button use styles, HTMX attributes or on click.");
            if(k=="form"&&parent=="form")fail(n.line,"Forms cannot be nested.");
            if(k=="on"){if(app.fragments.count(route))fail(n.line,"Fragments are read-only views. Put actions on a page.");if(parent!="button"&&parent!="form")fail(n.line,"Put on click inside a button, or on submit inside a form.");if(n.t.size()!=2||n.t[1].text!=(parent=="button"?"click":"submit"))fail(n.line,"This event doesn't match its element.");Env locals=e;for(auto& [key,v]:app.state)locals.erase(key);Executor exec{app,locals};for(auto& [name,rule]:inputs){(void)rule;exec.inputNames.insert(name);}exec.run(n.children,true);continue;}
            if(styles.count(k)){if(parent=="page"||containers.count(parent)||parent=="heading"||parent=="subheading"||parent=="text"||parent=="button"||parent=="link"||parent=="form"||parent=="image"){}else fail(n.line,"Put style commands inside an element.");if(!n.children.empty())fail(n.line,"Style commands cannot contain a block.");}
            if(k=="if")app.condition(n,e);
            if(k=="for"){auto& t=n.t;if(t.size()!=5||t[1].text!="each"||(!ident(t[2].text)||app.namedRoutes.count(t[2].text))||t[3].text!="in")fail(n.line,"Write for each task in tasks:");auto v=app.lookup(t[4].text,e,n.line);if(!holds_alternative<List>(v))fail(n.line,"Loop over a list.");child[t[2].text]=string("Example");child[t[2].text+".index"]=0.0;}
            if(k=="use"){auto [componentName,bound]=app.bind(n,e);Env scoped=e;for(auto& [name,value]:bound)scoped[name]=value;if(!n.children.empty())fail(n.line,"use takes no nested commands.");if(!used.insert(n.t[1].text).second)fail(n.line,"Components cannot include themselves.");walk(app.components.at(componentName)->children,scoped,parent);used.erase(n.t[1].text);}
            // Render each node with dummy loop locals, so empty lists and false branches still get checked.
            if(k!="for"&&k!="if"&&k!="else"&&k!="use"&&k!="on"&&!styles.count(k)){Node copy=n;copy.children.erase(remove_if(copy.children.begin(),copy.children.end(),[](const Node& c){return c.t[0].text!="on"&&!styles.count(c.t[0].text);}),copy.children.end());Renderer r{app,session,route};r.render({copy},e);}
            walk(n.children,child,k);
        }};
        walk(page->children,base,"page");Renderer r{app,session,route};r.style(page->children,base);if(temporary)app.pages.erase(route);
    }
    // Reject unused components with invalid commands too, by validating through a temporary page.
    for(auto& [name,node]:app.components){(void)name;if(node->children.empty())fail(node->line,"A component needs content.");}
}
string document(const string& title,const string& body,const string& prefix="/_tappy/",const string& liveVersion="") {
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><meta name=\"htmx-config\" content='{\"allowEval\":false,\"allowScriptTags\":false,\"selfRequestsOnly\":true}'><title>"+esc(title)+"</title><link rel=\"stylesheet\" href=\""+prefix+"default.css\"><script src=\""+prefix+"htmx.min.js\" defer></script></head><body>"+body+(liveVersion.empty()?"":"<script src=\"/_tappy/live.js\" data-version=\""+liveVersion+"\" defer></script>")+"</body></html>";
}
fs::path dataPath(const fs::path& source){auto name=source.filename();name+=".state";return source.parent_path()/".tappy-data"/name;}
void save(const Engine& app,const fs::path& path){if(app.persistent.empty())return;fs::create_directories(path.parent_path());ostringstream o;o<<"TAPPY_STATE_1\n";for(auto& name:app.persistent){const Value& v=app.state.at(name);o<<quoted(name)<<' '<<v.index()<<' ';if(auto list=get_if<List>(&v)){o<<list->size();for(auto& item:*list)o<<' '<<quoted(item);}else o<<quoted(str(v));o<<'\n';}auto temp=path;temp+=".tmp";writeFile(temp,o.str());fs::rename(temp,path);}
void load(Engine& app,const fs::path& path){if(!fs::exists(path))return;istringstream in(readFile(path));string magic;getline(in,magic);if(magic!="TAPPY_STATE_1")throw Error("Unrecognized state file: "+pathText(path));string name;size_t type;while(in>>quoted(name)>>type){Value v;if(type==3){size_t count;if(!(in>>count)||count>10000)throw Error("Invalid saved list.");List list;for(size_t i=0;i<count;++i){string s;if(!(in>>quoted(s)))throw Error("Incomplete saved list.");list.push_back(s);}v=list;}else{string s;if(!(in>>quoted(s)))throw Error("Incomplete saved value.");if(type==0)v=s;else if(type==1){size_t consumed=0;double d=stod(s,&consumed);if(consumed!=s.size()||!isfinite(d))throw Error("Invalid saved number.");v=d;}else if(type==2){if(s!="true"&&s!="false")throw Error("Invalid saved boolean.");v=s=="true";}else throw Error("Unknown saved value type.");}if(app.persistent.count(name)){if(app.state.at(name).index()!=v.index())throw Error("Saved type changed for "+name+". Move the state file aside before changing its type.");app.state[name]=v;}}if(!in.eof())throw Error("Invalid state file: "+pathText(path));}
void security(httplib::Response& res){res.set_header("X-Content-Type-Options","nosniff");res.set_header("Referrer-Policy","same-origin");res.set_header("Cache-Control","no-store");res.set_header("Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' https: http:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");}
void serve(Engine& app,const fs::path& source,int port){
    load(app,dataPath(source));httplib::Server server;server.new_task_queue=[] { return new httplib::ThreadPool(16,128); };mutex lock;condition_variable changed;atomic<int> streams{0};map<string,Session> sessions;auto started=chrono::steady_clock::now();
    // One source watcher serves all browsers. Browser reload uses a persistent SSE connection.
    string lastSource=readFile(source),liveVersion=token(),lastReadError;
    auto refreshSource=[&] {
        string next;
        try { next=readFile(source); lastReadError.clear(); }
        catch(const exception& e) { if(lastReadError!=e.what()){lastReadError=e.what();cerr<<"Reload failed; keeping the last working version: "<<e.what()<<'\n'<<flush;}return; }
        if(next==lastSource)return;
        lastSource=next;
        try {
            Engine candidate(parse(next));validate(candidate);
            for(auto& [name,value]:candidate.state) {
                auto old=app.state.find(name);
                if(old!=app.state.end()&&old->second.index()==value.index())value=old->second;
            }
            // A valid default program must also render with the preserved live values.
            Session preview;
            for(auto& [route,node]:candidate.pages){(void)node;Renderer renderer{candidate,preview,route};renderer.page();preview.actions.clear();}
            candidate.revision=app.revision+1;
            save(candidate,dataPath(source));
            for(auto& [id,session]:sessions){(void)id;session.actions.clear();}
            app=std::move(candidate);
            liveVersion=token();changed.notify_all();
            cout<<"Reloaded "<<pathText(source.filename())<<" · preserved matching state variables\n"<<flush;
        } catch(const exception& e) {
            cerr<<"Reload failed; keeping the last working version: "<<e.what()<<'\n'<<flush;
        }
    };
    auto watchedTime=fs::last_write_time(source);auto watchedSize=fs::file_size(source);
    jthread watcher([&](stop_token stop) {
        while(!stop.stop_requested()) {
            this_thread::sleep_for(chrono::milliseconds(250));
            error_code error;auto time=fs::last_write_time(source,error);if(error)continue;
            auto size=fs::file_size(source,error);if(error||(time==watchedTime&&size==watchedSize))continue;
            watchedTime=time;watchedSize=size;lock_guard guard(lock);refreshSource();
        }
    });
    server.Get("/_tappy/live.js",[](const auto&,auto& res) {
        res.set_content(R"JS((()=>{const version=document.currentScript.dataset.version;const events=new EventSource('/_tappy/events?version='+encodeURIComponent(version));events.addEventListener('change',()=>{events.close();location.reload();});addEventListener('pagehide',()=>events.close(),{once:true});})();)JS","text/javascript; charset=utf-8");
    });
    server.Get("/_tappy/events",[&](const httplib::Request& req,httplib::Response& res) {
        if(streams.fetch_add(1)>=8) { --streams;res.status=503;res.set_header("Retry-After","10");return; }
        auto seen=make_shared<string>(req.has_param("version")?req.get_param_value("version"):"");
        auto first=make_shared<bool>(true);security(res);res.set_header("Cache-Control","no-cache");res.set_header("X-Accel-Buffering","no");
        res.set_chunked_content_provider("text/event-stream",[&,seen,first](size_t,httplib::DataSink& sink) {
            if(*first) { *first=false;const string initial="retry: 10000\n: connected\n\n";return sink.write(initial.data(),initial.size()); }
            unique_lock guard(lock);changed.wait_for(guard,chrono::seconds(25),[&]{return *seen!=liveVersion||!server.is_running();});
            if(!server.is_running())return false;
            bool update=*seen!=liveVersion;*seen=liveVersion;guard.unlock();
            string event=update?"event: change\ndata: reload\n\n":": keep-alive\n\n";
            return sink.write(event.data(),event.size());
        },[&](bool){--streams;});
    });
    server.Get("/_tappy/routes",[&](const httplib::Request&,httplib::Response& res) {
        lock_guard guard(lock);security(res);string body="[";bool first=true;
        for(auto& [route,node]:app.pages){(void)node;if(app.fragments.count(route))continue;if(!first)body+=",";first=false;body+="\""+route+"\"";}
        res.set_content(body+"]","application/json; charset=utf-8");
    });
    server.Get("/_tappy/reload",[&](const httplib::Request& req,httplib::Response& res){
        lock_guard guard(lock);security(res);res.status=204;
        if(req.has_param("version")&&req.get_param_value("version")!=liveVersion)res.set_header("HX-Refresh","true");
    });
    server.set_payload_max_length(64*1024);server.set_read_timeout(5,0);server.set_write_timeout(5,0);
    auto getSession=[&](const httplib::Request& req,httplib::Response& res)->Session&{string cookie=req.get_header_value("Cookie"),id;istringstream is(cookie);string bit;while(getline(is,bit,';')){bit=trim(bit);if(bit.rfind("tappy_session=",0)==0)id=bit.substr(14);}if(!sessions.count(id)){if(sessions.size()>=1000)sessions.erase(sessions.begin());id=token();sessions.emplace(id,Session{});res.set_header("Set-Cookie","tappy_session="+id+"; HttpOnly; SameSite=Strict; Path=/");}return sessions.at(id);};
    server.Get("/_tappy/htmx.min.js",[](const auto&,auto& res){res.set_content(string(htmx_js),"text/javascript; charset=utf-8");});
    server.Get("/_tappy/default.css",[](const auto&,auto& res){res.set_content(string(default_css),"text/css; charset=utf-8");});
    server.Post(R"(/_tappy/action/([a-f0-9]+))",[&](const httplib::Request& req,httplib::Response& res){lock_guard guard(lock);security(res);auto& session=getSession(req,res);if(req.get_header_value("X-Tappy-CSRF")!=session.csrf){res.status=403;res.set_content("Invalid form token. Reload the page.","text/plain");return;}auto it=session.actions.find(req.matches[1].str());if(it==session.actions.end()){res.status=409;res.set_header("HX-Refresh","true");res.set_content("This page is stale. Reload it before trying again.","text/plain");return;}Action action=it->second;
        if(action.revision!=app.revision){Renderer renderer{app,session,action.route};res.set_content(renderer.page("The shared data changed. Please try again using this refreshed page."),"text/html; charset=utf-8");return;}
        Env backup=app.state;size_t oldRevision=app.revision;
        try{auto inputs=action.inputs;Env locals=action.locals;
            for(auto& [name,rule]:inputs){string value=req.has_param(name)?req.get_param_value(name):"";if(value.size()>4096)throw Error("Input is too long (maximum 4096 bytes).");if(rule.required&&trim(value).empty())throw Error("Please fill in '"+name+"'.");if(rule.type=="email"&&!value.empty()&&(value.find('@')==string::npos||value.find('@')==0||value.back()=='@'))throw Error("Enter an email address for '"+name+"'.");locals[name]=value;}
            Executor exec{app,locals};for(auto& [name,rule]:inputs){(void)rule;exec.inputNames.insert(name);}exec.run(action.event->children);string route=exec.redirect.empty()?action.route:exec.redirect;
            // Render before persisting so a rendering error rolls back the entire mutation.
            ++app.revision;Renderer renderer{app,session,route};string body=renderer.page();save(app,dataPath(source));session.actions.erase(req.matches[1].str());if(!exec.redirect.empty())res.set_header("HX-Push-Url",route);res.set_content(body,"text/html; charset=utf-8");
        }catch(const exception& e){app.state=backup;app.revision=oldRevision;Renderer renderer{app,session,action.route};res.set_content(renderer.page(e.what()),"text/html; charset=utf-8");}
    });
    server.Get(R"(/.*)",[&](const httplib::Request& req,httplib::Response& res){lock_guard guard(lock);security(res);if(!app.pages.count(req.path)){res.status=404;res.set_content("Page not found.","text/plain");return;}auto& session=getSession(req,res);if(session.actions.size()>10000)session.actions.clear();try{Renderer renderer{app,session,req.path};string body=renderer.page();if(app.fragments.count(req.path)||(req.get_header_value("HX-Request")=="true"&&req.get_header_value("HX-History-Restore-Request")!="true"))res.set_content(body,"text/html; charset=utf-8");else res.set_content(document(app.pages.at(req.path)->t[1].text,body,"/_tappy/",liveVersion),"text/html; charset=utf-8");}catch(const exception& e){res.status=500;res.set_content(esc(e.what()),"text/plain");}});
    cout<<"Tappyweb · C++20 + HTMX\nhttp://127.0.0.1:"<<port<<"\n"<<app.pages.size()<<" pages · shared app state · "<<chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now()-started).count()<<" ms setup\nWatching source for changes. Press Ctrl+C to stop.\n"<<flush;
    if(!server.listen("127.0.0.1",port))throw Error("Cannot listen on port "+to_string(port)+". Try --port with a different number.");
}
int tappyMain(int argc,char** argv){try{
    if(int forwarded=tappy_update::forward(argc,argv);forwarded!=-1)return forwarded;
    if(argc>1&&string(argv[1])=="update") {
        auto helper=tappy_update::helper();if(!fs::exists(helper))throw Error("The update helper is missing. Install the latest package from GitHub.");
        return tappy_update::process({helper,"--force",tappy_update::version});
    }
    if(argc<2||string(argv[1])=="--help"){cout<<"Tappyweb 0.3 — a small, readable web language\n\n  tappy file.tappy [--port 3000]\n  tappy check file.tappy\n  tappy update\n  tappy serve file.tappy [--port 3000]\n  tappy build file.tappy [--out dist]\n\nbuild exports static pages; use serve for forms and state changes.\n";return 0;}string command=argv[1];if(command=="--version"){cout<<"tappy 0.3.0\n";return 0;}bool direct=utf8Path(command).extension()==".tappy";int sourceIndex=direct?1:2;if(direct)command="serve";if(command!="check"&&command!="serve"&&command!="build")throw Error("Unknown command. Use tappy file.tappy or run tappy --help.");if(argc<=sourceIndex)throw Error("Give a .tappy source file.");fs::path source=fs::absolute(utf8Path(argv[sourceIndex]));filename=pathText(source);int port=3000;fs::path out="dist";
    for(int i=sourceIndex+1;i<argc;++i){string opt=argv[i];if(i+1>=argc)throw Error("Missing value after "+opt);string value=argv[++i];if(opt=="--port"&&command=="serve"){if(value.empty()||!all_of(value.begin(),value.end(),[](unsigned char c){return isdigit(c);})||value.size()>5)throw Error("Port must be 1–65535.");port=stoi(value);if(port<1||port>65535)throw Error("Port must be 1–65535.");}else if(opt=="--out"&&command=="build")out=utf8Path(value);else throw Error("Unknown option: "+opt);}
    Engine app(parse(readFile(source)));validate(app);
    if(command=="check"){cout<<"OK · "<<app.pages.size()<<" pages, "<<app.components.size()<<" components, "<<app.state.size()<<" state variables\n";return 0;}
    if(command=="serve"){tappy_update::check_in_background();serve(app,source,port);return 0;}
    // Generate everything first; a dynamic-page error must not leave a partial export.
    map<fs::path,string> files;Session session;for(auto& [route,node]:app.pages){Renderer renderer{app,session,route,true};string body=renderer.page();fs::path rel=route=="/"?fs::path("index.html"):fs::path(route.substr(1))/"index.html";files[rel]=app.fragments.count(route)?body:document(node->t[1].text,body,"/_tappy/");}
    fs::create_directories(out/"_tappy");for(auto& [rel,html]:files){fs::create_directories((out/rel).parent_path());writeFile(out/rel,html);}writeFile(out/"_tappy/htmx.min.js",string(htmx_js));writeFile(out/"_tappy/default.css",string(default_css));cout<<"Built "<<files.size()<<" static pages in "<<pathText(fs::absolute(out))<<"\n";return 0;
}catch(const exception& e){cerr<<e.what()<<'\n';return 1;}}

#ifdef _WIN32
int wmain(int argc,wchar_t** wideArguments) {
    SetConsoleOutputCP(CP_UTF8);
    vector<string> values;vector<char*> arguments;
    for(int i=0;i<argc;++i){int length=WideCharToMultiByte(CP_UTF8,0,wideArguments[i],-1,nullptr,0,nullptr,nullptr);if(length<=0)return 1;string value(size_t(length),'\0');WideCharToMultiByte(CP_UTF8,0,wideArguments[i],-1,value.data(),length,nullptr,nullptr);value.pop_back();values.push_back(std::move(value));}
    for(auto& value:values)arguments.push_back(value.data());
    return tappyMain(argc,arguments.data());
}
#else
int main(int argc,char** argv) { return tappyMain(argc,argv); }
#endif
