#pragma once
#include "update_support.hpp"
#include <regex>
#include <sstream>
#include <chrono>
#include <iostream>
#include <functional>
#ifdef _WIN32
#include <winhttp.h>
#endif
namespace tappy_theme {
namespace fs=std::filesystem;
constexpr size_t limit=2*1024*1024;
inline std::string normalize(std::string value) {
    static const std::regex github(R"(^https://github\.com/([^/]+)/([^/]+)/blob/([^?#]+)$)");
    std::smatch match;if(std::regex_match(value,match,github))value="https://raw.githubusercontent.com/"+match[1].str()+"/"+match[2].str()+"/"+match[3].str();
    if(value.empty()||value.find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Give theme a CSS path or HTTPS URL.");
    if(value.find("://")!=std::string::npos&&value.rfind("https://",0)!=0)throw std::runtime_error("Remote themes require an HTTPS URL.");
    if(value.rfind("https://",0)==0){auto host=value.substr(8,value.find_first_of("/?#",8)-8);if(host.empty()||host.find('@')!=std::string::npos||value.find_first_of(" \t")!=std::string::npos)throw std::runtime_error("Use a public HTTPS theme URL without embedded credentials.");}
    return value;
}
inline std::string read(const fs::path& path) {
    if(fs::file_size(path)>limit)throw std::runtime_error("Theme CSS must be smaller than 2 MB.");
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read theme: "+path.string());
    std::string css{std::istreambuf_iterator<char>(file),{}};
    auto start=css.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
    if(css.find('\0')!=std::string::npos||(start!=std::string::npos&&css[start]=='<'))throw std::runtime_error("Theme link returned HTML or binary data. Use a raw CSS file.");
    return css;
}
inline void fetch(const std::string& url,const fs::path& output) {
#ifdef _WIN32
    struct Handle { HINTERNET value;~Handle(){if(value)WinHttpCloseHandle(value);}operator HINTERNET()const{return value;} };
    auto address=tappy_update::utf8(url).wstring();URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(address.c_str(),DWORD(address.size()),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.dwUserNameLength||parts.dwPasswordLength)throw std::runtime_error("Invalid theme URL.");
    Handle session{WinHttpOpen(L"Tappyweb-Themes/0.3",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value)throw std::runtime_error("Cannot initialize theme HTTPS.");WinHttpSetTimeouts(session,5000,5000,10000,10000);
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    Handle connection{WinHttpConnect(session,host.c_str(),parts.nPort,0)};if(!connection.value)throw std::runtime_error("Cannot connect to theme host.");
    Handle request{WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.value)throw std::runtime_error("Cannot open theme URL.");
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    if(!WinHttpSendRequest(request,L"Accept: text/css, text/plain;q=0.9\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw std::runtime_error("Cannot download theme.");
    DWORD code=0,size=sizeof(code);WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX);if(code!=200)throw std::runtime_error("Theme returned HTTP "+std::to_string(code));
    std::ofstream file(output,std::ios::binary);char buffer[32768];size_t total=0;DWORD count=0;
    while(true){if(!WinHttpReadData(request,buffer,sizeof(buffer),&count))throw std::runtime_error("Incomplete theme download.");if(!count)break;total+=count;if(total>limit)throw std::runtime_error("Theme CSS must be smaller than 2 MB.");file.write(buffer,count);if(!file)throw std::runtime_error("Cannot cache theme.");}
#else
    if(tappy_update::process({"/usr/bin/curl","--disable","--fail","--location","--silent","--proto","=https","--proto-redir","=https","--connect-timeout","5","--max-time","20","--max-filesize",std::to_string(limit),"--output",output,"--url",url})!=0)throw std::runtime_error("Cannot download theme. Check its raw CSS URL.");
#endif
}
// Relative remote asset URLs keep their meaning after CSS is served locally.
inline std::string absolute(const std::string& value,const std::string& base) {
    if(value.empty()||value[0]=='#'||value.find(':')!=std::string::npos)return value;
    auto slash=base.find('/',8);auto origin=base.substr(0,slash);
    if(value.rfind("//",0)==0)return "https:"+value;
    if(value[0]=='/')return origin+value;
    auto clean=base.substr(0,base.find_first_of("?#"));if(clean.find('/',8)==std::string::npos)return clean+"/"+value;return clean.substr(0,clean.rfind('/')+1)+value;
}
inline std::string remote_assets(const std::string& css,const std::string& base) {
    // Match comments and ordinary strings first so their contents remain untouched.
    static const std::regex references(R"CSS(/\*[\s\S]*?\*/|url\(\s*(?:"([^"\\]*)"|'([^'\\]*)'|([^\s)'"\\]+))\s*\)|@import\s+"([^"\\]*)"|@import\s+'([^'\\]*)'|"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')CSS",std::regex::icase);
    std::string out;size_t end=0;
    for(std::sregex_iterator it(css.begin(),css.end(),references),last;it!=last;++it){auto m=*it;out+=css.substr(end,size_t(m.position())-end);std::string piece=m.str();for(size_t i=1;i<m.size();++i)if(m[i].matched){piece.replace(size_t(m.position(i)-m.position()),size_t(m.length(i)),absolute(m[i].str(),base));break;}out+=piece;end=size_t(m.position()+m.length());}
    return out+css.substr(end);
}
struct Theme {
    std::string source,css;fs::path local;fs::file_time_type modified{};uintmax_t size=0;
    bool refresh() {if(local.empty())return false;auto next=fs::last_write_time(local);auto bytes=fs::file_size(local);if(next==modified&&bytes==size)return false;modified=next;size=bytes;auto value=read(local);if(value==css)return false;css=std::move(value);return true;}
    void load(const std::string& value,const fs::path& page, const std::function<void(const std::string&,const fs::path&)>& download=fetch, fs::path cache={}) {
        source=normalize(value);local.clear();css.clear();
        if(source.rfind("https://",0)!=0){local=page.parent_path()/tappy_update::utf8(source);css=read(local);modified=fs::last_write_time(local);size=fs::file_size(local);return;}
        auto root=cache.empty()?tappy_update::cache_root().parent_path()/"Themes":cache;fs::create_directories(root);
        uint64_t hash=14695981039346656037ULL;for(unsigned char c:source){hash^=c;hash*=1099511628211ULL;}
        std::ostringstream key;key<<std::hex<<hash;auto cached=root/(key.str()+".css"),record=root/(key.str()+".url");
        auto temp=root/(key.str()+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp");
        try {download(source,temp);css=read(temp);{std::ofstream file(cached,std::ios::binary);file<<css;}{std::ofstream file(record);file<<source;}fs::remove(temp);}
        catch(const std::exception&){std::error_code error;fs::remove(temp,error);std::ifstream file(record);std::string previous{std::istreambuf_iterator<char>(file),{}};if(previous!=source)throw;css=read(cached);std::cerr<<"Theme host unavailable; using cached CSS.\n";}
        css=remote_assets(css,source);
    }
};
}
