#include "KeyGaugeStore.h"
#include "core/sd_functions.h"
#include <globals.h>
#include <cstdlib>
#include <cstring>
#include <freertos/semphr.h>
namespace KeyGauge {
namespace {
StaticSemaphore_t mutexControl;
SemaphoreHandle_t mutex = xSemaphoreCreateMutexStatic(&mutexControl);
struct Lock {
    bool held;
    Lock() : held(mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {}
    ~Lock() { if (held) xSemaphoreGive(mutex); }
};
const char *directory = "/MaliTools/KeyGauge";
FS &filesystem() { return sdcardMounted ? static_cast<FS &>(SD) : static_cast<FS &>(LittleFS); }
bool prepare(FS &fs) {
    return (fs.exists("/MaliTools") || fs.mkdir("/MaliTools")) &&
           (fs.exists(directory) || fs.mkdir(directory));
}
String pathFor(const String &name) { return String(directory) + "/" + name + ".mkg"; }
bool recover(FS &fs, const String &path) {
    String backup = path + ".bak";
    if (!fs.exists(backup)) return true;
    // A missing target means replacement stopped before commit. Restore the original.
    if (!fs.exists(path)) return fs.rename(backup, path);
    return fs.remove(backup);
}
bool number(const String &s, int lo, int hi, int &result) {
    if (!s.length()) return false;
    for (unsigned i = 0; i < s.length(); ++i) if (s[i] < '0' || s[i] > '9') return false;
    long value = strtol(s.c_str(), nullptr, 10);
    if (value < lo || value > hi) return false;
    result = value;
    return true;
}
bool readFile(FS &fs, const String &path, KeyGaugeProfile &p) {
    File f = fs.open(path, FILE_READ);
    if (!f || f.size() > 1024) return false;
    String header = f.readStringUntil('\n'); header.trim();
    if (header != "MKG1") return false;
    KeyGaugeProfile candidate;
    unsigned seen = 0;
    String levels;
    while (f.available()) {
        String row = f.readStringUntil('\n'); row.trim();
        if (!row.length()) continue;
        int eq = row.indexOf('=');
        if (eq < 1) return false;
        String key = row.substring(0, eq), value = row.substring(eq+1);
        unsigned bit = 0; int n = 0;
        if (key == "name") {
            bit = 1; if (!value.length() || value.length() >= sizeof(candidate.name)) return false;
            value.toCharArray(candidate.name, sizeof(candidate.name));
        } else if (key == "points") {
            bit = 2; if (!number(value, 4, 10, n)) return false; candidate.points = n;
        } else if (key == "levels") { bit = 4; levels = value; }
        else if (key == "thickness") {
            bit = 8; if (!number(value, 1, 10, n)) return false; candidate.thickness = n;
        } else if (key == "width") {
            bit = 16; if (!number(value, 50, 100, n)) return false; candidate.width = n;
        } else return false;
        if (seen & bit) return false;
        seen |= bit;
    }
    if ((seen & 7) != 7) return false;
    int offset = 0;
    for (int i = 0; i < candidate.points; ++i) {
        int comma = levels.indexOf(',', offset), n;
        if ((i < candidate.points - 1) != (comma >= 0)) return false;
        if (!number(levels.substring(offset, comma < 0 ? levels.length() : comma), 0, 9, n)) return false;
        candidate.gaugePoints[i].level = n; offset = comma + 1;
    }
    p = candidate; return true;
}

}
bool validName(const String &name) {
    if (!name.length() || name.length() >= sizeof(KeyGaugeProfile::name)) return false;
    for (unsigned i = 0; i < name.length(); ++i) {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}
bool validProfile(const KeyGaugeProfile &p) {
    if (strnlen(p.name, sizeof(p.name)) == sizeof(p.name) || !validName(p.name) ||
        p.points < 4 || p.points > 10 || p.thickness < 1 || p.thickness > 10 || p.width < 50 || p.width > 100) return false;
    for (int i = 0; i < p.points; ++i) if (p.gaugePoints[i].level > 9) return false;
    return true;
}
int listProfiles(std::vector<String> &names, String &nextName) {
    Lock lock; if (!lock.held) return 503;
    FS &fs = filesystem();
    if (!prepare(fs)) return 503;
    File dir = fs.open(directory);
    if (!dir || !dir.isDirectory()) return 503;
    names.clear();
    File file = dir.openNextFile();
    while (file) {
        String name = file.name(); bool folder = file.isDirectory(); file.close();
        name = name.substring(name.lastIndexOf('/') + 1);
        if (!folder && name.endsWith(".mkg.bak")) {
            name.remove(name.length() - 4);
        }
        if (!folder && name.endsWith(".mkg")) {
            name.remove(name.length() - 4);
            if (validName(name)) {
                if (!recover(fs, pathFor(name))) return 503;
                bool duplicate = false;
                for (const auto &existing : names) if (existing == name) duplicate = true;
                if (duplicate) { file = dir.openNextFile(); continue; }
                if (names.size() >= 256) return 507;
                names.push_back(name);
            }
        }
        file = dir.openNextFile();
    }
    nextName = "";
    for (int i = 1; i <= 9999; ++i) {
        char name[32]; snprintf(name, sizeof(name), "PROFILE_%03d", i);
        if (!fs.exists(pathFor(name))) { nextName = name; break; }
    }
    return nextName.length() ? 200 : 507;
}
int readProfile(const String &name, KeyGaugeProfile &p) {
    if (!validName(name)) return 400;
    Lock lock; if (!lock.held) return 503;
    FS &fs = filesystem();
    if (!recover(fs, pathFor(name))) return 503;
    if (!fs.exists(pathFor(name))) return 404;
    KeyGaugeProfile candidate;
    if (!readFile(fs, pathFor(name), candidate) || !validProfile(candidate)) return 422;
    // File identity is authoritative, including files renamed by the file manager.
    name.toCharArray(candidate.name, sizeof(candidate.name));
    p = candidate; return 200;
}
int writeProfile(const KeyGaugeProfile &p, bool replace) {
    if (!validProfile(p)) return 400;
    Lock lock; if (!lock.held) return 503;
    FS &fs = filesystem(); if (!prepare(fs)) return 503;
    String path = pathFor(p.name);
    if (!recover(fs, path)) return 503;
    if (fs.exists(path) && !replace) return 409;
    if (!fs.exists(path) && replace) return 404;
    if (!replace) {
        File dir = fs.open(directory);
        if (!dir || !dir.isDirectory()) return 503;
        size_t count = 0;
        File entry = dir.openNextFile();
        while (entry) {
            if (!entry.isDirectory() && String(entry.name()).endsWith(".mkg")) ++count;
            entry.close();
            if (count >= 256) return 507;
            entry = dir.openNextFile();
        }
    }
    String content = "MKG1\nname=" + String(p.name) + "\npoints=" + String(p.points) +
        "\nthickness=" + String(p.thickness) + "\nwidth=" + String(p.width) + "\nlevels=";
    for (int i = 0; i < p.points; ++i) { if (i) content += ','; content += String(p.gaugePoints[i].level); }
    content += '\n';
    String temp = path + ".tmp";
    File f = fs.open(temp, FILE_WRITE);
    if (!f) return 507;
    bool ok = f.print(content) == content.length(); f.flush(); f.close();
    if (!ok) { fs.remove(temp); return 507; }
    String backup = path + ".bak";
    // FAT cannot rename over an existing destination. Keep the old file until commit.
    if (replace && !fs.rename(path, backup)) { fs.remove(temp); return 507; }
    if (!fs.rename(temp, path)) {
        if (replace) fs.rename(backup, path);
        fs.remove(temp); return 507;
    }
    if (replace) fs.remove(backup);
    return 200;
}
int deleteProfile(const String &name) {
    if (!validName(name)) return 400;
    Lock lock; if (!lock.held) return 503;
    FS &fs = filesystem(); String path = pathFor(name);
    if (!recover(fs, path)) return 503;
    if (!fs.exists(path)) return 404;
    return fs.remove(path) ? 200 : 507;
}
}
