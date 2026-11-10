#include "../src/themes.hpp"
#include <iostream>
void require(bool result){if(!result)throw std::runtime_error("Theme check failed");}
int main(){
 namespace fs=std::filesystem;using namespace tappy_theme;
 auto folder=fs::temp_directory_path()/("tappy-themes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder);
 try {
  require(normalize("https://github.com/team/themes/blob/main/theme.css")=="https://raw.githubusercontent.com/team/themes/main/theme.css");
  auto css=remote_assets("/* url(fake.png) */ .card { background: url('../images/bg.png'); content: \"url(fake.png)\"; } @import \"fonts.css\";", "https://example.com/themes/dark/theme.css");
  require(css.find("https://example.com/themes/dark/../images/bg.png")!=std::string::npos);
  require(css.find("/* url(fake.png) */")!=std::string::npos);
  require(css.find("content: \"url(fake.png)\"")!=std::string::npos);
  require(css.find("https://example.com/themes/dark/fonts.css")!=std::string::npos);
  require(absolute("image.png","https://example.com")=="https://example.com/image.png");
  bool credentials=false;try{normalize("https://user:password@example.com/theme.css");}catch(...){credentials=true;}require(credentials);
  int calls=0;Theme theme;
  auto download=[&](const std::string&,const fs::path& output){++calls;std::ofstream file(output);file<<":root { --tappy-accent: red; }";};
  theme.load("https://example.com/theme.css",folder/"page.tappy",download,folder/"cache");require(calls==1&&theme.css.find("red")!=std::string::npos);
  auto offline=[](const std::string&,const fs::path&){throw std::runtime_error("Offline");};
  theme.load("https://example.com/theme.css",folder/"page.tappy",offline,folder/"cache");require(theme.css.find("red")!=std::string::npos);
  bool rejected=false;try{theme.load("https://example.com/other.css",folder/"page.tappy",offline,folder/"cache");}catch(...){rejected=true;}require(rejected);
  auto html=[](const std::string&,const fs::path& output){std::ofstream file(output);file<<"<!doctype html>";};
  rejected=false;try{theme.load("https://example.com/html.css",folder/"page.tappy",html,folder/"cache");}catch(...){rejected=true;}require(rejected);
  fs::remove_all(folder);std::cout<<"Theme URL, CSS assets, cache fallback and HTML rejection checks passed\n";
 }catch(...){fs::remove_all(folder);throw;}
}
