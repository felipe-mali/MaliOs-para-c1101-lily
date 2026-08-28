#include "MaliPortalStore.h"
#include "maliPortalTemplate.h"

#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace MaliPortalStore {
namespace {

constexpr const char *kDirectory = "/MaliOS/portals";
constexpr const char *kSelectionFile = "/MaliOS/portal_selected.txt";
constexpr const char *kDefaultName = "mali_lab.html";

SemaphoreHandle_t storeMutex = nullptr;

class Lock {
public:
    Lock() {
        if (!storeMutex) storeMutex = xSemaphoreCreateMutex();
        locked = storeMutex && xSemaphoreTake(storeMutex, pdMS_TO_TICKS(1000)) == pdTRUE;
    }
    ~Lock() {
        if (locked) xSemaphoreGive(storeMutex);
    }
    explicit operator bool() const { return locked; }

private:
    bool locked = false;
};

String pathFor(const String &name) { return String(kDirectory) + "/" + name; }

bool ensureDirectories(String &error) {
    if (!LittleFS.exists("/MaliOS") && !LittleFS.mkdir("/MaliOS")) {
        error = "Nao foi possivel criar /MaliOS";
        return false;
    }
    if (!LittleFS.exists(kDirectory) && !LittleFS.mkdir(kDirectory)) {
        error = "Nao foi possivel criar o diretorio de portais";
        return false;
    }
    return true;
}

bool writeText(const String &path, const String &content, String &error) {
    const String temporary = path + ".tmp";
    LittleFS.remove(temporary);
    File file = LittleFS.open(temporary, FILE_WRITE);
    if (!file) {
        error = "Nao foi possivel abrir o arquivo temporario";
        return false;
    }
    const size_t written = file.write(reinterpret_cast<const uint8_t *>(content.c_str()), content.length());
    file.flush();
    file.close();
    if (written != content.length()) {
        LittleFS.remove(temporary);
        error = "Falha ao gravar todo o conteudo";
        return false;
    }
    LittleFS.remove(path);
    if (!LittleFS.rename(temporary, path)) {
        LittleFS.remove(temporary);
        error = "Nao foi possivel concluir a gravacao";
        return false;
    }
    return true;
}

bool ensureUnlocked(String &error) {
    if (!ensureDirectories(error)) return false;
    const String defaultPath = pathFor(kDefaultName);
    if (!LittleFS.exists(defaultPath)) {
        File file = LittleFS.open(defaultPath, FILE_WRITE);
        if (!file) {
            error = "Nao foi possivel criar o template Mali Lab";
            return false;
        }
        const size_t written = file.write(mali_lab_html, mali_lab_html_size);
        file.close();
        if (written != mali_lab_html_size) {
            LittleFS.remove(defaultPath);
            error = "Falha ao criar o template Mali Lab";
            return false;
        }
    }
    if (!LittleFS.exists(kSelectionFile)) return writeText(kSelectionFile, kDefaultName, error);
    return true;
}

bool readSelectionUnlocked(String &name, String &error) {
    File file = LittleFS.open(kSelectionFile, FILE_READ);
    if (!file) {
        error = "Nao foi possivel ler a selecao do portal";
        return false;
    }
    name = file.readStringUntil('\n');
    name.trim();
    file.close();
    if (!isValidName(name) || !LittleFS.exists(pathFor(name))) {
        name = kDefaultName;
        return writeText(kSelectionFile, name, error);
    }
    return true;
}

} // namespace

bool isValidName(const String &name) {
    if (name.length() < 6 || name.length() > 48 || !name.endsWith(".html")) return false;
    if (name.indexOf("..") >= 0 || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return false;
    for (size_t index = 0; index < name.length(); ++index) {
        const char value = name[index];
        if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '_' || value == '-' || value == '.') {
            continue;
        }
        return false;
    }
    return true;
}

bool begin(String &error) {
    Lock lock;
    if (!lock) {
        error = "Portal Studio ocupado";
        return false;
    }
    return ensureUnlocked(error);
}

bool list(std::vector<TemplateInfo> &items, String &error) {
    Lock lock;
    if (!lock || !ensureUnlocked(error)) {
        if (error.isEmpty()) error = "Portal Studio ocupado";
        return false;
    }
    items.clear();
    String selectedName;
    if (!readSelectionUnlocked(selectedName, error)) return false;
    File directory = LittleFS.open(kDirectory);
    if (!directory || !directory.isDirectory()) {
        error = "Diretorio de templates indisponivel";
        return false;
    }
    File file = directory.openNextFile();
    while (file && items.size() < kMaxTemplates) {
        if (!file.isDirectory()) {
            String fullName = file.name();
            const int separator = fullName.lastIndexOf('/');
            const String name = separator >= 0 ? fullName.substring(separator + 1) : fullName;
            if (isValidName(name)) {
                items.push_back({name, static_cast<size_t>(file.size()), name == selectedName, name == kDefaultName});
            }
        }
        file.close();
        file = directory.openNextFile();
    }
    directory.close();
    return true;
}

bool read(const String &name, String &content, String &error) {
    if (!isValidName(name)) {
        error = "Nome de template invalido";
        return false;
    }
    Lock lock;
    if (!lock || !ensureUnlocked(error)) {
        if (error.isEmpty()) error = "Portal Studio ocupado";
        return false;
    }
    File file = LittleFS.open(pathFor(name), FILE_READ);
    if (!file) {
        error = "Template nao encontrado";
        return false;
    }
    if (file.size() > kMaxTemplateBytes) {
        file.close();
        error = "Template excede 24 KiB";
        return false;
    }
    content = file.readString();
    file.close();
    return true;
}

bool save(const String &name, const String &content, String &error) {
    if (!isValidName(name)) {
        error = "Use um nome .html com letras, numeros, _ ou -";
        return false;
    }
    if (content.isEmpty() || content.length() > kMaxTemplateBytes) {
        error = "O template deve possuir entre 1 byte e 24 KiB";
        return false;
    }
    Lock lock;
    if (!lock || !ensureUnlocked(error)) {
        if (error.isEmpty()) error = "Portal Studio ocupado";
        return false;
    }
    if (!LittleFS.exists(pathFor(name))) {
        File directory = LittleFS.open(kDirectory);
        size_t count = 0;
        File file = directory.openNextFile();
        while (file) {
            if (!file.isDirectory()) ++count;
            file.close();
            file = directory.openNextFile();
        }
        directory.close();
        if (count >= kMaxTemplates) {
            error = "Limite de 16 templates atingido";
            return false;
        }
    }
    return writeText(pathFor(name), content, error);
}

bool duplicate(const String &source, const String &destination, String &error) {
    if (!isValidName(source) || !isValidName(destination) || source == destination) {
        error = "Nomes de origem ou destino invalidos";
        return false;
    }
    String content;
    if (!read(source, content, error)) return false;
    if (LittleFS.exists(pathFor(destination))) {
        error = "Ja existe um template com esse nome";
        return false;
    }
    return save(destination, content, error);
}

bool remove(const String &name, String &error) {
    if (!isValidName(name) || name == kDefaultName) {
        error = "O template Mali Lab incorporado nao pode ser excluido";
        return false;
    }
    Lock lock;
    if (!lock || !ensureUnlocked(error)) {
        if (error.isEmpty()) error = "Portal Studio ocupado";
        return false;
    }
    String selectedName;
    if (!readSelectionUnlocked(selectedName, error)) return false;
    if (selectedName == name) {
        error = "Selecione outro template antes de excluir este";
        return false;
    }
    if (!LittleFS.exists(pathFor(name))) {
        error = "Template nao encontrado";
        return false;
    }
    if (!LittleFS.remove(pathFor(name))) {
        error = "Nao foi possivel excluir o template";
        return false;
    }
    return true;
}

bool select(const String &name, String &error) {
    if (!isValidName(name)) {
        error = "Nome de template invalido";
        return false;
    }
    Lock lock;
    if (!lock || !ensureUnlocked(error)) {
        if (error.isEmpty()) error = "Portal Studio ocupado";
        return false;
    }
    if (!LittleFS.exists(pathFor(name))) {
        error = "Template nao encontrado";
        return false;
    }
    return writeText(kSelectionFile, name, error);
}

bool selected(String &name, String &path, String &error) {
    Lock lock;
    if (!lock || !ensureUnlocked(error)) {
        if (error.isEmpty()) error = "Portal Studio ocupado";
        return false;
    }
    if (!readSelectionUnlocked(name, error)) return false;
    path = pathFor(name);
    return true;
}

} // namespace MaliPortalStore
