#include "pixel_paint_app.h"

#include "core/display.h"
#include "core/sd_functions.h"

namespace {
constexpr uint8_t CANVAS_WIDTH = 16;
constexpr uint8_t CANVAS_HEIGHT = 16;
constexpr uint16_t PIXEL_COUNT = CANVAS_WIDTH * CANVAS_HEIGHT;
constexpr uint8_t BITMAP_SIZE = PIXEL_COUNT / 8;
constexpr char PAINT_DIR[] = "/MaliPaint";
constexpr char PAINT_FILE[] = "/MaliPaint/paint_001.dat";
constexpr char EXPORT_FILE[] = "/MaliPaint/paint_001.h";

struct PaintState {
    uint8_t bitmap[BITMAP_SIZE] = {0};
    uint16_t cursor = 0;
    bool eraseMode = false;
    bool dirty = false;
};

struct CanvasLayout {
    int x;
    int y;
    int cell;
    int panelX;
};

bool pixelAt(const PaintState &state, uint16_t index) {
    return state.bitmap[index / 8] & static_cast<uint8_t>(1U << (7 - (index % 8)));
}

void setPixel(PaintState &state, uint16_t index, bool enabled) {
    const uint8_t mask = static_cast<uint8_t>(1U << (7 - (index % 8)));
    if (enabled) state.bitmap[index / 8] |= mask;
    else state.bitmap[index / 8] &= static_cast<uint8_t>(~mask);
}

void clearPainting(PaintState &state, bool resetTools) {
    memset(state.bitmap, 0, sizeof(state.bitmap));
    state.dirty = true;
    if (resetTools) {
        state.cursor = 0;
        state.eraseMode = false;
    }
}

CanvasLayout calculateLayout() {
    constexpr int top = 28;
    int byHeight = (tftHeight - top - 12) / CANVAS_HEIGHT;
    int byWidth = ((tftWidth * 3) / 5) / CANVAS_WIDTH;
    int cell = byHeight < byWidth ? byHeight : byWidth;
    if (cell < 2) cell = 2;

    const int canvasWidth = cell * CANVAS_WIDTH;
    return {6, top, cell, 6 + canvasWidth + 8};
}

void drawCell(const PaintState &state, const CanvasLayout &layout, uint16_t index, bool cursor) {
    const int col = index % CANVAS_WIDTH;
    const int row = index / CANVAS_WIDTH;
    const int x = layout.x + col * layout.cell;
    const int y = layout.y + row * layout.cell;
    const uint16_t fillColor = pixelAt(state, index) ? bruceConfig.priColor : bruceConfig.bgColor;

    tft.fillRect(x + 1, y + 1, layout.cell - 1, layout.cell - 1, fillColor);
    tft.drawRect(x, y, layout.cell, layout.cell, cursor ? bruceConfig.priColor : bruceConfig.secColor);
    if (cursor && pixelAt(state, index) && layout.cell > 3) {
        tft.drawRect(x + 1, y + 1, layout.cell - 2, layout.cell - 2, bruceConfig.secColor);
    }
}

void drawPanel(const PaintState &state, const CanvasLayout &layout) {
    const int panelWidth = tftWidth - layout.panelX - 4;
    if (panelWidth <= 0) return;

    tft.fillRect(layout.panelX, 28, panelWidth, tftHeight - 40, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString("Ferramenta", layout.panelX, 32, 1);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString(state.eraseMode ? "Apagar" : "Pintar", layout.panelX, 44, 1);

    const int col = state.cursor % CANVAS_WIDTH;
    const int row = state.cursor / CANVAS_WIDTH;
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString("Cursor", layout.panelX, 64, 1);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString(String(col + 1) + "," + String(row + 1), layout.panelX, 76, 1);

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(state.dirty ? "Alterado" : "Salvo", layout.panelX, 96, 1);
    tft.drawString("SEL aplica", layout.panelX, 116, 1);
    tft.drawString("BACK menu", layout.panelX, 128, 1);
}

void drawPainting(const PaintState &state, const CanvasLayout &layout) {
    drawMainBorderWithTitle("Mini Pixel Paint");
    for (uint16_t i = 0; i < PIXEL_COUNT; ++i) drawCell(state, layout, i, i == state.cursor);
    drawPanel(state, layout);
}

bool prepareStorage(FS *&fs) {
    if (!getFsStorage(fs) || fs == nullptr) {
        displayError("Armazenamento indisponivel", true);
        return false;
    }
    if (!fs->exists(PAINT_DIR) && !fs->mkdir(PAINT_DIR)) {
        displayError("Falha ao criar /MaliPaint", true);
        return false;
    }
    return true;
}

bool savePainting(PaintState &state) {
    FS *fs = nullptr;
    if (!prepareStorage(fs)) return false;

    File file = fs->open(PAINT_FILE, FILE_WRITE);
    if (!file) {
        displayError("Falha ao abrir arquivo", true);
        return false;
    }

    const uint8_t header[4] = {'M', 'P', '1', '6'};
    const bool ok = file.write(header, sizeof(header)) == sizeof(header) &&
                    file.write(state.bitmap, sizeof(state.bitmap)) == sizeof(state.bitmap);
    file.close();

    if (!ok) {
        displayError("Falha ao salvar desenho", true);
        return false;
    }

    state.dirty = false;
    displaySuccess("Salvo em /MaliPaint/paint_001.dat", true);
    return true;
}

bool loadPainting(PaintState &state) {
    FS *fs = nullptr;
    if (!prepareStorage(fs)) return false;
    if (!fs->exists(PAINT_FILE)) {
        displayError("Nenhum desenho salvo", true);
        return false;
    }

    File file = fs->open(PAINT_FILE, FILE_READ);
    if (!file || file.size() < static_cast<size_t>(4 + BITMAP_SIZE)) {
        if (file) file.close();
        displayError("Arquivo de desenho invalido", true);
        return false;
    }

    uint8_t header[4];
    const bool validHeader = file.read(header, sizeof(header)) == sizeof(header) && header[0] == 'M' &&
                             header[1] == 'P' && header[2] == '1' && header[3] == '6';
    const bool validBitmap = validHeader && file.read(state.bitmap, sizeof(state.bitmap)) == sizeof(state.bitmap);
    file.close();

    if (!validBitmap) {
        displayError("Arquivo de desenho invalido", true);
        return false;
    }

    state.dirty = false;
    displaySuccess("Desenho carregado", true);
    return true;
}

bool exportBitmap(const PaintState &state) {
    FS *fs = nullptr;
    if (!prepareStorage(fs)) return false;

    File file = fs->open(EXPORT_FILE, FILE_WRITE);
    if (!file) {
        displayError("Falha ao criar bitmap", true);
        return false;
    }

    file.println("// Mini Pixel Paint - 16x16 monocromatico");
    file.println("const unsigned char mali_paint_16x16[32] = {");
    for (uint8_t i = 0; i < BITMAP_SIZE; ++i) {
        file.print("  0x");
        if (state.bitmap[i] < 0x10) file.print('0');
        file.print(state.bitmap[i], HEX);
        file.println(i + 1 < BITMAP_SIZE ? "," : "");
    }
    file.println("};");
    file.close();

    displaySuccess("Exportado em /MaliPaint/paint_001.h", true);
    return true;
}

bool actionsMenu(PaintState &state) {
    bool leave = false;
    std::vector<Option> actions = {
        {"Pintar", [&]() { state.eraseMode = false; }},
        {"Apagar", [&]() { state.eraseMode = true; }},
        {"Limpar", [&]() { clearPainting(state, false); }},
        {"Salvar", [&]() { savePainting(state); }},
        {"Carregar", [&]() { loadPainting(state); }},
        {"Novo", [&]() { clearPainting(state, true); }},
        {"Exportar bitmap", [&]() { exportBitmap(state); }},
        {"Sair", [&]() { leave = true; }},
        {"Voltar", []() {}},
    };

    loopOptions(actions, MENU_TYPE_SUBMENU, "Pixel Paint");
    return leave;
}

uint16_t wrapCursor(int32_t cursor) {
    cursor %= PIXEL_COUNT;
    if (cursor < 0) cursor += PIXEL_COUNT;
    return static_cast<uint16_t>(cursor);
}
} // namespace

void pixel_paint_app() {
    PaintState state;
    const CanvasLayout layout = calculateLayout();

#ifdef HAS_ENCODER
    RotaryNetSteps = 0;
#endif
    check(PrevPress);
    check(NextPress);
    drawPainting(state, layout);

    while (!returnToMenu) {
        if (check(EscPress)) {
            if (actionsMenu(state)) break;
            drawPainting(state, layout);
            continue;
        }

        const uint16_t previousCursor = state.cursor;
        bool moved = false;

#ifdef HAS_ENCODER
        const int32_t rotarySteps = drainRotarySteps();
        if (rotarySteps != 0) {
            check(PrevPress);
            check(NextPress);
            state.cursor = wrapCursor(static_cast<int32_t>(state.cursor) - rotarySteps);
            moved = true;
        } else
#endif
        {
            if (check(PrevPress)) {
                state.cursor = wrapCursor(static_cast<int32_t>(state.cursor) - 1);
                moved = true;
            }
            if (check(NextPress)) {
                state.cursor = wrapCursor(static_cast<int32_t>(state.cursor) + 1);
                moved = true;
            }
            if (check(UpPress)) {
                state.cursor = wrapCursor(static_cast<int32_t>(state.cursor) - CANVAS_WIDTH);
                moved = true;
            }
            if (check(DownPress)) {
                state.cursor = wrapCursor(static_cast<int32_t>(state.cursor) + CANVAS_WIDTH);
                moved = true;
            }
        }

        if (moved) {
            drawCell(state, layout, previousCursor, false);
            drawCell(state, layout, state.cursor, true);
            drawPanel(state, layout);
        }

        if (check(SelPress)) {
            const bool newValue = !state.eraseMode;
            if (pixelAt(state, state.cursor) != newValue) {
                setPixel(state, state.cursor, newValue);
                state.dirty = true;
                drawCell(state, layout, state.cursor, true);
                drawPanel(state, layout);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

#ifdef HAS_ENCODER
    RotaryNetSteps = 0;
#endif
    tft.fillScreen(bruceConfig.bgColor);
}
