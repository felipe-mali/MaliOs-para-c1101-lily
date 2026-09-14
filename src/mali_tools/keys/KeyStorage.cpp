#include "KeyStorage.h"
#include "KeyCodec.h"
#include <algorithm>
namespace MaliKeys {
namespace { constexpr size_t MAX_RECORDS = 256; }
String KeyStorage::path(const String &name) const { return String("/MaliKeys/") + name + ".mkey"; }
bool KeyStorage::prepare() { return fs.exists("/MaliKeys") || fs.mkdir("/MaliKeys"); }
bool KeyStorage::recover(const String &file) {
    String backup = file + ".bak";
    if (!fs.exists(backup)) return true;
    return fs.exists(file) ? fs.remove(backup) : fs.rename(backup, file);
}
StoreResult KeyStorage::list(std::vector<String> &names) {
    names.clear();
    if (!prepare()) return StoreResult::Unavailable;
    File directory = fs.open("/MaliKeys");
    if (!directory || !directory.isDirectory()) return StoreResult::Unavailable;
    File entry = directory.openNextFile();
    while (entry) {
        String name = entry.name(); bool folder = entry.isDirectory(); entry.close();
        name = name.substring(name.lastIndexOf('/') + 1);
        if (!folder && name.endsWith(".mkey.bak")) name.remove(name.length()-4);
        if (!folder && name.endsWith(".mkey")) {
            name.remove(name.length()-5);
            if (validName(name.c_str()) && std::find(names.begin(), names.end(), name)==names.end()) {
                if (names.size() >= MAX_RECORDS) return StoreResult::Full;
                names.push_back(name);
            }
        }
        entry = directory.openNextFile();
    }
    directory.close();
    // Recover after traversal: FAT iterators must not be invalidated by a rename.
    for (const auto &name : names) if (!recover(path(name))) return StoreResult::IoError;
    std::sort(names.begin(), names.end());
    return StoreResult::Ok;
}
StoreResult KeyStorage::load(const String &name, KeyProfile &profile) {
    if (!validName(name.c_str())) return StoreResult::Invalid;
    String file = path(name);
    if (!recover(file)) return StoreResult::IoError;
    if (!fs.exists(file)) return StoreResult::NotFound;
    File handle = fs.open(file, FILE_READ);
    if (!handle) return StoreResult::Unavailable;
    if (handle.size() != RECORD_SIZE) return StoreResult::Invalid;
    Record bytes{};
    if (handle.read(bytes.data(), bytes.size()) != bytes.size()) return StoreResult::IoError;
    handle.close();
    KeyProfile candidate;
    if (!decode(bytes, candidate)) return StoreResult::Invalid;
    // File identity remains authoritative when renamed through the existing file manager.
    name.toCharArray(candidate.name, sizeof(candidate.name));
    profile = candidate; return StoreResult::Ok;
}
StoreResult KeyStorage::save(const KeyProfile &p, bool replace) {
    Record bytes{};
    if (!encode(p, bytes)) return StoreResult::Invalid;
    if (!prepare()) return StoreResult::Unavailable;
    String file = path(p.name), temp = file + ".tmp", backup = file + ".bak";
    if (!recover(file)) return StoreResult::IoError;
    bool exists = fs.exists(file);
    if (exists && !replace) return StoreResult::Exists;
    if (!exists && replace) return StoreResult::NotFound;
    if (!exists) {
        std::vector<String> names;
        StoreResult result = list(names);
        if (result != StoreResult::Ok) return result;
        if (names.size() >= MAX_RECORDS) return StoreResult::Full;
    }
    // Truncate a leftover temporary file; FILE_WRITE differs between FS implementations.
    if (fs.exists(temp) && !fs.remove(temp)) return StoreResult::IoError;
    File handle = fs.open(temp, FILE_WRITE);
    if (!handle) return StoreResult::IoError;
    bool ok = handle.write(bytes.data(), bytes.size()) == bytes.size();
    handle.flush(); handle.close();
    if (!ok) { fs.remove(temp); return StoreResult::IoError; }
    // Read back before committing. Keep the old record until the new one is complete.
    Record verified{};
    handle = fs.open(temp, FILE_READ);
    ok = handle && handle.size() == bytes.size() && handle.read(verified.data(), verified.size()) == verified.size();
    handle.close();
    if (!ok || verified != bytes) { fs.remove(temp); return StoreResult::IoError; }
    if (exists && !fs.rename(file, backup)) { fs.remove(temp); return StoreResult::IoError; }
    if (!fs.rename(temp, file)) {
        if (exists) fs.rename(backup, file);
        fs.remove(temp); return StoreResult::IoError;
    }
    if (exists) fs.remove(backup); // A leftover backup is removed by recover on the next access.
    return StoreResult::Ok;
}
StoreResult KeyStorage::rename(const String &oldName, const String &newName) {
    if (!validName(oldName.c_str()) || !validName(newName.c_str())) return StoreResult::Invalid;
    if (oldName == newName) return StoreResult::Ok;
    if (!recover(path(oldName)) || !recover(path(newName))) return StoreResult::IoError;
    if (!fs.exists(path(oldName))) return StoreResult::NotFound;
    // FAT names are case insensitive: do not delete or overwrite the original for case-only renames.
    if (fs.exists(path(newName))) return StoreResult::Exists;
    return fs.rename(path(oldName), path(newName)) ? StoreResult::Ok : StoreResult::IoError;
}
StoreResult KeyStorage::remove(const String &name) {
    if (!validName(name.c_str())) return StoreResult::Invalid;
    if (!recover(path(name))) return StoreResult::IoError;
    if (!fs.exists(path(name))) return StoreResult::NotFound;
    return fs.remove(path(name)) ? StoreResult::Ok : StoreResult::IoError;
}
}
