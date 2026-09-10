#pragma once
#include "history_journal.h"
#ifdef GSMART_FEATURE_FILESYSTEM
#include "esphome/components/storage/fileSystem.h"
#include <cstdio>
#include <cerrno>
#include <string>

namespace gsmart_history {
class FlashFiles:public Files {
 public:
  bool ready()const{return esphome::storage::fileSystem&&esphome::storage::fileSystem->isReady();}
  int size(int slot)override{
    if(!ready())return -2;const auto path=path_(slot);
#ifdef ESP32
    struct stat st{};if(stat(path.c_str(),&st)!=0)return errno==ENOENT?-1:-2;return st.st_size;
#else
    if(!LittleFS.exists(path.c_str()))return -1;
    auto f=LittleFS.open(path.c_str(),"r");if(!f)return -2;return f.size();
#endif
  }
  bool read(int slot,size_t offset,void*out,size_t length)override{
    if(!ready())return false;const auto path=path_(slot);
#ifdef ESP32
    auto *f=fopen(path.c_str(),"rb");if(!f)return false;
    bool ok=fseek(f,offset,SEEK_SET)==0&&fread(out,1,length,f)==length;fclose(f);return ok;
#else
    auto f=LittleFS.open(path.c_str(),"r");return f&&f.seek(offset)&&f.read(static_cast<uint8_t*>(out),length)==length;
#endif
  }
  bool append(int slot,const void*data,size_t length)override{return write_(path_(slot),data,length,true);}
  bool create(int slot,const void*data,size_t length)override{
    if(size(slot)!=-1)return false;
    const auto path=path_(slot),temp=path+".tmp";
    if(!write_(temp,data,length,false))return false;
#ifdef ESP32
    return rename(temp.c_str(),path.c_str())==0;
#else
    return LittleFS.rename(temp.c_str(),path.c_str());
#endif
  }
  bool truncate(int slot,size_t length)override{
    const auto path=path_(slot);
#ifdef ESP32
    auto*f=fopen(path.c_str(),"r+b");if(!f)return false;
    bool ok=ftruncate(fileno(f),length)==0&&fsync(fileno(f))==0;fclose(f);return ok;
#else
    auto f=LittleFS.open(path.c_str(),"r+");if(!f)return false;bool ok=f.truncate(length);f.flush();return ok;
#endif
  }
  bool erase(int slot)override{
    const auto path=path_(slot);
#ifdef ESP32
    return unlink(path.c_str())==0;
#else
    return LittleFS.remove(path.c_str());
#endif
  }
 private:
  static std::string path_(int slot){
    char name[40];
#ifdef ESP32
    const char*prefix="/fs";
#else
    const char*prefix="";
#endif
    if(slot<0)snprintf(name,sizeof(name),"%s/history-meta.bin",prefix);
    else snprintf(name,sizeof(name),"%s/history-%02d.bin",prefix,slot);
    return name;
  }
  bool write_(const std::string&path,const void*data,size_t length,bool append){
    if(!ready())return false;
#ifdef ESP32
    auto*f=fopen(path.c_str(),append?"ab":"wb");if(!f)return false;
    bool ok=fwrite(data,1,length,f)==length&&fflush(f)==0&&fsync(fileno(f))==0;
    return fclose(f)==0&&ok;
#else
    auto f=LittleFS.open(path.c_str(),append?"a":"w");if(!f)return false;
    bool ok=f.write(static_cast<const uint8_t*>(data),length)==length;f.flush();f.close();return ok;
#endif
  }
};
} // namespace gsmart_history
#endif
