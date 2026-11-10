// Native per-user updater: HTTPS GitHub release metadata, SHA256-checked archives,
// versioned staging and an atomic pointer. Running servers are never replaced.
#include "../src/update_support.hpp"
#include "json.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <winhttp.h>
#include <bcrypt.h>
#else
#include <sys/file.h>
#endif
using namespace std;namespace fs=std::filesystem;using nlohmann::json;
string read(const fs::path& path){ifstream file(path,ios::binary);if(!file)throw runtime_error("Cannot read update file");return string(istreambuf_iterator<char>(file),{});}
#ifdef _WIN32
struct Handle { HINTERNET value=nullptr;~Handle(){if(value)WinHttpCloseHandle(value);}operator HINTERNET()const{return value;} };
wstring wide(const string& value){int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(count<=0)throw runtime_error("Invalid update URL");wstring out(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),count);return out;}
void download(const string& url,const fs::path& output,size_t limit) {
    auto address=wide(url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(address.c_str(),DWORD(address.size()),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS)throw runtime_error("Updates require HTTPS");
    Handle session{WinHttpOpen(L"Tappyweb-Updater/0.3",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value)throw runtime_error("Cannot initialize HTTPS");WinHttpSetTimeouts(session,5000,5000,10000,10000);
    wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    Handle connection{WinHttpConnect(session,host.c_str(),parts.nPort,0)};Handle request{WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!connection.value||!request.value)throw runtime_error("Cannot open HTTPS request");
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    if(!WinHttpSendRequest(request,L"Accept: application/vnd.github+json\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))throw runtime_error("GitHub could not be reached");
    DWORD code=0,size=sizeof(code);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX)||code!=200)throw runtime_error("GitHub returned HTTP "+to_string(code));
    ofstream file(output,ios::binary);char buffer[32768];size_t total=0;DWORD count=0;
    while(true){if(!WinHttpReadData(request,buffer,sizeof(buffer),&count))throw runtime_error("Incomplete download");if(!count)break;total+=count;if(total>limit)throw runtime_error("Update archive is too large");file.write(buffer,count);if(!file)throw runtime_error("Cannot write update archive");}
}
string sha256(const fs::path& path) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw runtime_error("SHA256 unavailable");
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);throw runtime_error("SHA256 unavailable");}
    ifstream file(path,ios::binary);vector<unsigned char> buffer(32768);bool ok=bool(file);
    while(file){file.read(reinterpret_cast<char*>(buffer.data()),buffer.size());auto count=file.gcount();if(count&&BCryptHashData(hash,buffer.data(),ULONG(count),0)<0)ok=false;}
    unsigned char digest[32];if(BCryptFinishHash(hash,digest,32,0)<0)ok=false;BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok)throw runtime_error("Cannot hash update archive");ostringstream out;for(unsigned char value:digest)out<<hex<<setw(2)<<setfill('0')<<int(value);return out.str();
}
void atomic_write(const fs::path& path,const string& text){auto temp=path;temp+=".tmp";{ofstream file(temp);file<<text;if(!file)throw runtime_error("Cannot write update pointer");}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw runtime_error("Cannot activate update");}
string psquote(const fs::path& path){auto raw=path.u8string();string value(reinterpret_cast<const char*>(raw.data()),raw.size()),out="'";for(char c:value){out+=c;if(c=='\'')out+=c;}return out+"'";}
void extract(const fs::path& archive,const fs::path& stage){string command="Expand-Archive -LiteralPath "+psquote(archive)+" -DestinationPath "+psquote(stage)+" -Force";wchar_t system[32768];if(!GetSystemDirectoryW(system,32768))throw runtime_error("System folder unavailable");if(tappy_update::process({fs::path(system)/L"WindowsPowerShell/v1.0/powershell.exe","-NoProfile","-NonInteractive","-Command",tappy_update::utf8(command)})!=0)throw runtime_error("Cannot unpack update");}
#else
void download(const string& url,const fs::path& output,size_t limit){if(tappy_update::process({"/usr/bin/curl","--fail","--location","--silent","--show-error","--proto","=https","--proto-redir","=https","--connect-timeout","5","--max-time","25","--max-filesize",to_string(limit),"--output",output,url})!=0)throw runtime_error("GitHub could not be reached");if(fs::file_size(output)>limit)throw runtime_error("Update archive is too large");}
string sha256(const fs::path& path){auto result=path;result+=".sha";pid_t pid=fork();if(pid==0){int out=open(result.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);if(out<0)_exit(1);dup2(out,1);close(out);execl("/usr/bin/shasum","shasum","-a","256",path.c_str(),nullptr);_exit(1);}int status=0;waitpid(pid,&status,0);if(!WIFEXITED(status)||WEXITSTATUS(status))throw runtime_error("Cannot hash update archive");string digest=read(result).substr(0,64);fs::remove(result);return digest;}
void atomic_write(const fs::path& path,const string& text){auto temp=path;temp+=".tmp";{ofstream file(temp);file<<text;if(!file)throw runtime_error("Cannot write update pointer");}fs::rename(temp,path);}
void extract(const fs::path& archive,const fs::path& stage){if(tappy_update::process({"/usr/bin/ditto","-x","-k",archive,stage})!=0)throw runtime_error("Cannot unpack update");}
#endif
string asset_name(const string& version){
#ifdef _WIN32
    return "Tappyweb-"+version+"-windows-x64.zip";
#else
    return "Tappyweb-"+version+"-macOS-universal.zip";
#endif
}
json select_asset(const json& release,const string& version){
    if(tappy_update::parts(version)[0]<0||release.value("draft",false)||release.value("prerelease",false))throw runtime_error("Invalid release version");
    string name=asset_name(version);
    for(auto& asset:release.at("assets"))if(asset.value("name","")==name){
        string url=asset.value("browser_download_url",""),digest=asset.value("digest","");
        string prefix="https://github.com/TappyWeb/TappyWeb/releases/download/v"+version+"/";
        if(url!=prefix+name||!regex_match(digest,regex("sha256:[a-f0-9]{64}")))throw runtime_error("Release has no valid GitHub SHA256 digest");return asset;
    }
    throw runtime_error("No update archive for this platform");
}
int main(int argc,char** argv){bool silent=argc>1&&string(argv[1])=="--background";
    try {
        if(argc>1&&string(argv[1])=="--self-test"){
            if(!(tappy_update::parts("0.10.0")>tappy_update::parts("0.9.9"))||tappy_update::parts("../../bad")[0]>=0)throw runtime_error("Version validation failed");
            auto test=fs::temp_directory_path()/"tappy-hash-self-test";{ofstream file(test);file<<"abc";}auto digest=sha256(test);fs::remove(test);if(digest!="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")throw runtime_error("SHA256 validation failed");
            json release={{"assets",json::array({{{"name",asset_name("1.2.3")},{"browser_download_url","https://github.com/TappyWeb/TappyWeb/releases/download/v1.2.3/"+asset_name("1.2.3")},{"digest","sha256:"+string(64,'a')}}})}};select_asset(release,"1.2.3");
            release["assets"][0]["browser_download_url"]="https://example.com/payload.zip";bool rejected=false;try{select_asset(release,"1.2.3");}catch(...){rejected=true;}if(!rejected)throw runtime_error("Unexpected update origin accepted");
            cout<<"Updater version, origin and SHA256 checks passed\n";return 0;
        }
        string current=argc>2?argv[2]:tappy_update::version;if(tappy_update::parts(current)[0]<0)throw runtime_error("Invalid current version");
        auto root=tappy_update::cache_root();if(root.empty())return 0;fs::create_directories(root);
#ifndef _WIN32
        fs::permissions(root,fs::perms::owner_all,fs::perm_options::replace);
        int lock=open((root/"update.lock").c_str(),O_CREAT|O_RDWR,0600);if(lock<0||flock(lock,LOCK_EX|LOCK_NB))return 0;
#else
        HANDLE lock=CreateFileW((root/L"update.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(lock==INVALID_HANDLE_VALUE)return 0;
#endif
        auto now=chrono::system_clock::to_time_t(chrono::system_clock::now());long long checked=0;{ifstream file(root/"checked.txt");file>>checked;}
        if(silent&&now>=checked&&now-checked<24*60*60)return 0;
        atomic_write(root/"checked.txt",to_string(now));auto metadata=root/"release.json";download("https://api.github.com/repos/TappyWeb/TappyWeb/releases/latest",metadata,1024*1024);
        json release=json::parse(read(metadata));string version=release.at("tag_name").get<string>();if(!version.empty()&&version[0]=='v')version.erase(0,1);
        if(tappy_update::parts(version)<=tappy_update::parts(current)){if(!silent)cout<<"Tappyweb is up to date ("<<current<<").\n";return 0;}
        auto asset=select_asset(release,version);auto archive=root/"download.zip";download(asset.at("browser_download_url"),archive,64*1024*1024);
        if("sha256:"+sha256(archive)!=asset.at("digest").get<string>())throw runtime_error("Update checksum mismatch; keeping current version");
        auto stage=root/(version+".staging");fs::remove_all(stage);fs::create_directory(stage);extract(archive,stage);
        auto runtime=stage/tappy_update::runtime_name();if(!fs::is_regular_file(runtime))throw runtime_error("Update contains no runtime");
#ifndef _WIN32
        fs::permissions(runtime,fs::perms::owner_all|fs::perms::group_read|fs::perms::group_exec,fs::perm_options::replace);
        fs::permissions(stage/"tappy-update",fs::perms::owner_all|fs::perms::group_read|fs::perms::group_exec,fs::perm_options::replace);
#endif
        auto destination=root/version;if(!fs::exists(destination))fs::rename(stage,destination);else fs::remove_all(stage);
        atomic_write(root/"current.txt",version);fs::remove(archive);
        if(!silent)cout<<"Tappyweb "<<version<<" is ready. The next command uses it; running servers are unchanged.\n";
        return 0;
    }catch(const exception& error){if(!silent)cerr<<"Update skipped: "<<error.what()<<'\n';return silent?0:1;}
}
