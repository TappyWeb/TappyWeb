// Per-user PATH updates without NSIS string-length limits or a shell subprocess.
#include <windows.h>
#include <string>
#include <vector>
using namespace std;
wstring normalized(wstring value) {
    if(value.size()>1 && value.front()==L'"' && value.back()==L'"') value=value.substr(1,value.size()-2);
    DWORD count=ExpandEnvironmentStringsW(value.c_str(),nullptr,0);
    if(count){wstring expanded(count,L'\0');ExpandEnvironmentStringsW(value.c_str(),expanded.data(),count);expanded.pop_back();value=expanded;}
    while(value.size()>3 && (value.back()==L'\\'||value.back()==L'/'))value.pop_back();
    return value;
}
bool same(const wstring& a,const wstring& b){auto x=normalized(a),y=normalized(b);return CompareStringOrdinal(x.c_str(),-1,y.c_str(),-1,TRUE)==CSTR_EQUAL;}
int wmain(int argc,wchar_t** argv){
    if(argc!=3 || (wstring(argv[1])!=L"add" && wstring(argv[1])!=L"remove"))return ERROR_INVALID_PARAMETER;
    HKEY key;LONG error=RegCreateKeyExW(HKEY_CURRENT_USER,L"Environment",0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE,nullptr,&key,nullptr);if(error)return int(error);
    DWORD type=REG_EXPAND_SZ,bytes=0;error=RegQueryValueExW(key,L"Path",nullptr,&type,nullptr,&bytes);
    if(error!=ERROR_SUCCESS&&error!=ERROR_FILE_NOT_FOUND){RegCloseKey(key);return int(error);}
    if(error==ERROR_SUCCESS && type!=REG_SZ && type!=REG_EXPAND_SZ){RegCloseKey(key);return ERROR_DATATYPE_MISMATCH;}
    wstring original;
    if(bytes){vector<wchar_t> data(bytes/sizeof(wchar_t)+1,L'\0');error=RegQueryValueExW(key,L"Path",nullptr,&type,reinterpret_cast<BYTE*>(data.data()),&bytes);if(error){RegCloseKey(key);return int(error);}original=data.data();}
    vector<wstring> entries;size_t start=0;bool found=false;wstring directory=argv[2];
    while(start<=original.size()){auto end=original.find(L';',start);auto item=original.substr(start,end==wstring::npos?wstring::npos:end-start);if(same(item,directory))found=true;entries.push_back(item);if(end==wstring::npos)break;start=end+1;}
    wstring result=original;
    if(wstring(argv[1])==L"add"){
        if(found){RegCloseKey(key);return 10;} // Already present: installer must not claim ownership.
        if(!result.empty()&&result.back()!=L';')result+=L';';result+=directory;
    }else{
        if(!found){RegCloseKey(key);return 0;}
        result.clear();bool first=true;for(auto& item:entries)if(!same(item,directory)){if(!first)result+=L';';result+=item;first=false;}
    }
    error=RegSetValueExW(key,L"Path",0,type,reinterpret_cast<const BYTE*>(result.c_str()),DWORD((result.size()+1)*sizeof(wchar_t)));RegCloseKey(key);
    if(error)return int(error);
    DWORD_PTR ignored;SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"Environment"),SMTO_ABORTIFHUNG,3000,&ignored);
    return 0;
}
