#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

namespace MaliPortalStore {

constexpr size_t kMaxTemplateBytes = 24 * 1024;
constexpr size_t kMaxTemplates = 16;

struct TemplateInfo {
    String name;
    size_t size = 0;
    bool selected = false;
    bool builtIn = false;
};

// Templates are intentionally isolated from the general file manager under
// LittleFS:/MaliOS/portals. All public methods validate the basename before
// touching the filesystem.
bool begin(String &error);
bool list(std::vector<TemplateInfo> &items, String &error);
bool read(const String &name, String &content, String &error);
bool save(const String &name, const String &content, String &error);
bool duplicate(const String &source, const String &destination, String &error);
bool remove(const String &name, String &error);
bool select(const String &name, String &error);
bool selected(String &name, String &path, String &error);
bool isValidName(const String &name);

} // namespace MaliPortalStore
