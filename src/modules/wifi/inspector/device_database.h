#pragma once
#include "device_model.h"
#include <FS.h>
namespace WifiInspector {
class DeviceStore {
    FS &fs;
    String filename;
    bool recover();
public:
    DeviceStore(FS &storage,const Network &network);
    // Missing files are an empty database, malformed files are never overwritten implicitly.
    bool load(Database &database);
    bool save(Database &database);
    bool erase();
    const String &path() const { return filename; }
};
}
