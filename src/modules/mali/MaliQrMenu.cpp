#include "MaliQrMenu.h"

#include "core/display.h"
#include "core/wifi/webInterface.h"
#include "modules/mali/MaliQrStore.h"
#include "modules/others/qrcode_menu.h"

namespace {

void showNoQrMessage(const char *message) {
    displayInfo(message);
    delay(1000);
}

void openCreateMenu() {
    while (true) {
        std::vector<Option> createOptions = {
            {"PIX",              pix_qrcode             },
            {"Texto",            display_custom_qrcode  },
            {"Salvar e exibir",  save_and_display_qrcode},
            {"Voltar",           []() {}                },
        };

        const int selected = loopOptions(createOptions, MENU_TYPE_SUBMENU, "Criar QR");
        if (selected < 0 || selected == static_cast<int>(createOptions.size()) - 1) return;
    }
}

void openFavoritesMenu() {
    MaliQrStore::begin();

    while (true) {
        const size_t count = MaliQrStore::favoriteCount();
        if (count == 0) {
            showNoQrMessage("Nenhum favorito salvo");
            return;
        }

        std::vector<Option> favoriteOptions;
        favoriteOptions.reserve(count + 1);
        for (size_t index = 0; index < count; ++index) {
            MaliQrStore::FavoriteEntry favorite;
            if (!MaliQrStore::favoriteAt(index, favorite)) continue;

            favoriteOptions.emplace_back(favorite.name, [payload = favorite.payload]() {
                qrcode_display(payload);
            });
        }
        favoriteOptions.emplace_back("Voltar", []() {});

        const int selected = loopOptions(favoriteOptions, MENU_TYPE_SUBMENU, "Favoritos QR");
        if (selected < 0 || selected == static_cast<int>(favoriteOptions.size()) - 1) return;
    }
}

void showLastQr() {
    MaliQrStore::begin();
    MaliQrStore::HistoryEntry last;
    if (!MaliQrStore::lastHistory(last) || last.payload.isEmpty()) {
        showNoQrMessage("Nenhum QR no historico");
        return;
    }

    qrcode_display(last.payload);
}

} // namespace

namespace MaliQrMenu {

void open() {
    while (true) {
        std::vector<Option> qrOptions = {
            {"Criar",       openCreateMenu  },
            {"Favoritos",   openFavoritesMenu},
            {"Ultimo QR",   showLastQr      },
            {"WebUI",       loopOptionsWebUi},
            {"Voltar",      []() {}         },
        };

        const int selected = loopOptions(qrOptions, MENU_TYPE_SUBMENU, "QR");
        if (selected < 0 || selected == static_cast<int>(qrOptions.size()) - 1) return;
    }
}

} // namespace MaliQrMenu
