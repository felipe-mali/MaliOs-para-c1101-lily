#pragma once
// In-memory adapter for executing the real DeviceStore on a desktop compiler.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#define FILE_READ "r"
#define FILE_WRITE "w"
class String:public std::string {
public:
    using std::string::string;
    String(const std::string &s):std::string(s){}
    bool endsWith(const char *s)const{size_t n=strlen(s);return size()>=n&&compare(size()-n,n,s)==0;}
};
struct MemoryFs {
    struct Node {bool dir=false;std::vector<uint8_t> bytes;};
    std::map<std::string,std::shared_ptr<Node>> files;
    bool failCommit=false,failWrites=false;
};
class File {
    std::shared_ptr<MemoryFs> fs;
    std::shared_ptr<MemoryFs::Node> node;
    std::string path;
    size_t cursor=0,child=0;
public:
    File()=default;
    File(std::shared_ptr<MemoryFs> f,const std::string &p):fs(f),path(p){auto i=fs->files.find(path);if(i!=fs->files.end())node=i->second;}
    explicit operator bool()const{return bool(node);}
    bool isDirectory()const{return node&&node->dir;}
    size_t size()const{return node?node->bytes.size():0;}
    const char *name()const{return path.c_str();}
    size_t read(uint8_t *out,size_t n){if(!node)return 0;n=std::min(n,node->bytes.size()-cursor);memcpy(out,node->bytes.data()+cursor,n);cursor+=n;return n;}
    size_t write(const uint8_t *data,size_t n){if(!node||fs->failWrites)return 0;node->bytes.insert(node->bytes.end(),data,data+n);return n;}
    void flush(){}
    void close(){node.reset();}
    File openNextFile(){size_t i=0;if(!isDirectory())return {};for(auto &entry:fs->files){if(entry.first.rfind(path+"/",0)==0){if(i++==child){++child;return File(fs,entry.first);}}}return {};}
};
class FS {
public:
    std::shared_ptr<MemoryFs> memory=std::make_shared<MemoryFs>();
    bool exists(const String &p){return memory->files.count(p)>0;}
    bool mkdir(const String &p){if(exists(p))return false;auto n=std::make_shared<MemoryFs::Node>();n->dir=true;memory->files[p]=n;return true;}
    bool remove(const String &p){return memory->files.erase(p)>0;}
    bool rename(const String &from,const String &to){if(!exists(from)||exists(to)||(memory->failCommit&&from.endsWith(".tmp")))return false;memory->files[to]=memory->files[from];memory->files.erase(from);return true;}
    File open(const String &p,const char *mode=FILE_READ){if(!strcmp(mode,FILE_WRITE)){auto n=std::make_shared<MemoryFs::Node>();memory->files[p]=n;}return File(memory,p);}
};
