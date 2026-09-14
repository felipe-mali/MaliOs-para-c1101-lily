#pragma once
#include "KeyProfile.h"
#include <FS.h>
#include <vector>
namespace MaliKeys {
enum class StoreResult { Ok, Unavailable, Invalid, NotFound, Exists, Full, IoError };
class KeyStorage {
    FS &fs;
    String path(const String &name) const;
    bool prepare();
    bool recover(const String &file);
public:
    explicit KeyStorage(FS &filesystem) : fs(filesystem) {}
    StoreResult list(std::vector<String> &names);
    StoreResult load(const String &name, KeyProfile &profile);
    StoreResult save(const KeyProfile &profile, bool replace = false);
    StoreResult rename(const String &oldName, const String &newName);
    StoreResult remove(const String &name);
};
}
