// ---------------------------------------------------------------------------
// guest_test.cpp — прогон гостевого ПО на korvet_core (инструмент проверки).
//
// Сценарий: загрузка ПЗУ/дискеты, ввод команды CP/M, прогон N кадров,
// дамп текста экрана (KOI8→UTF8) и скриншот PNG. Вызывается из run_guest.sh
// для каждой программы (PROFFER, SAUCER, KLAD, JUMPER, CHESS, ABRIS, FX...).
//
// Использование:
//   guest_test <rom> <disk> <команда> <кадров_после_команды> <скриншот.png>
// ---------------------------------------------------------------------------

#include "korvet_core/motherboard.hpp"
#include "png_writer.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

static FILE* g_disk = nullptr;
struct Adapter : public korvet::IFddImageAdapter {
    bool is_present(uint8_t) const override { return g_disk != nullptr; }
    bool is_write_protected(uint8_t) const override { return true; }
    bool read_sector(uint8_t, uint8_t track, uint8_t side, uint8_t sector, uint8_t* buf, size_t n) override {
        // sector — НУЛЕВОЙ номер (ядро вычитает 1 из регистра ВГ93)
        if (sector >= 5) return false;
        long pos = (long)track * 10 * 1024 + side * 5 * 1024 + sector * 1024;
        if (fseek(g_disk, pos, SEEK_SET) != 0) return false;
        return fread(buf, 1, n, g_disk) == n;
    }
    bool write_sector(uint8_t, uint8_t, uint8_t, uint8_t, const uint8_t*, size_t) override { return false; }
};

// Эмпирическая раскладка ПЗУ 2.0 (латиница; калибровка вводом)
struct KeyPos { char ch; int row, col; };
static const KeyPos KEYPOS[] = {
    {'@',0,0},{'A',1,0},{'B',2,0},{'C',3,0},{'D',4,0},{'E',5,0},{'F',6,0},{'G',7,0},
    {'H',0,1},{'I',1,1},{'J',2,1},{'K',3,1},{'L',4,1},{'M',5,1},{'N',6,1},{'O',7,1},
    {'P',0,2},{'Q',1,2},{'R',2,2},{'S',3,2},{'T',4,2},{'U',5,2},{'V',6,2},{'W',7,2},
    {'X',0,3},{'Y',1,3},{'Z',2,3},{'[',3,3},{'\\',4,3},{']',5,3},{'^',6,3},{'_',7,3},
    {'0',0,4},{'1',1,4},{'2',2,4},{'3',3,4},{'4',4,4},{'5',5,4},{'6',6,4},{'7',7,4},
    {'8',0,5},{'9',1,5},{':',2,5},{';',3,5},{',',4,5},{'-',5,5},{'.',6,5},{'/',7,5},
    {' ',7,6},
};

static void type_line(korvet::Motherboard& mb, const char* text) {
    for (const char* p = text; *p; p++) {
        if (*p == '~') {  // '~' = ENTER
            mb.keyboard_key_press((0 << 8) | 6);
            for (int k = 0; k < 14; k++) mb.run_frame();
            mb.keyboard_key_release((0 << 8) | 6);
            for (int k = 0; k < 8; k++) mb.run_frame();
            continue;
        }
        if (*p == '^') {  // '^' = SPACE (альтернативный разделитель)
            p++;
            continue;
        }
        for (auto& e : KEYPOS) {
            if (e.ch == *p) {
                mb.keyboard_key_press((e.row << 8) | e.col);
                for (int k = 0; k < 12; k++) mb.run_frame();
                mb.keyboard_key_release((e.row << 8) | e.col);
                for (int k = 0; k < 5; k++) mb.run_frame();
                break;
            }
        }
    }
}

static void press_space(korvet::Motherboard& mb, int frames = 20) {
    mb.keyboard_key_press((7 << 8) | 6);
    for (int k = 0; k < frames; k++) mb.run_frame();
    mb.keyboard_key_release((7 << 8) | 6);
    for (int k = 0; k < 6; k++) mb.run_frame();
}

static void dump_screen(korvet::Motherboard& mb) {
    mb.render_video_frame();
    const uint8_t* aczu = mb.readout_aczu();
    for (int row = 0; row < 16; row++) {
        std::string out;
        for (int col = 0; col < 64; col++) {
            uint8_t ch = aczu[(row * 64 + col) * 2];
            out += (char)(ch ? ch : ' ');
        }
        size_t end = out.find_last_not_of(' ');
        if (end == std::string::npos) continue;
        printf("%2d| ", row);
        for (char c : out.substr(0, end + 1)) {
            unsigned char b = (unsigned char)c;
            if (b < 0x80) putchar(b);
            else printf("<%02X>", b);
        }
        printf("\n");
    }
}

static bool save_shot(korvet::Motherboard& mb, const char* path) {
    const uint8_t* frame = mb.readout_video();
    const korvet::RgbColor* lut = mb.readout_lut_rgb();
    std::vector<uint8_t> rgb(512 * 256 * 3);
    for (int i = 0; i < 512 * 256; i++) {
        uint8_t c = frame[i] & 0x0F;
        rgb[i * 3 + 0] = lut[c].r;
        rgb[i * 3 + 1] = lut[c].g;
        rgb[i * 3 + 2] = lut[c].b;
    }
    return korvet::minigui::write_png_rgb(path, rgb.data(), 512, 256);
}

int main(int argc, char** argv) {
    if (argc < 6) {
        printf("Использование: guest_test <rom> <disk|-> <команда(~=ENTER)> <кадров> <shot.png> [boot_frames]\n");
        return 2;
    }
    const char* rom_path = argv[1];
    const char* disk_path = strcmp(argv[2], "-") != 0 ? argv[2] : nullptr;
    const char* command = argv[3];
    int run_frames = atoi(argv[4]);
    const char* shot_path = argv[5];
    int boot_frames = argc > 6 ? atoi(argv[6]) : 1600;

    auto rd = [](const char* p) {
        std::vector<uint8_t> d;
        FILE* f = fopen(p, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long s = ftell(f);
            fseek(f, 0, SEEK_SET);
            d.resize(s > 0 ? s : 0);
            if (s > 0 && fread(d.data(), 1, s, f) != (size_t)s) d.clear();
            fclose(f);
        }
        return d;
    };

    auto rom = rd(rom_path);
    auto mapper = rd("data/rom/mapper.mem");
    auto font = rd("data/rom/korvet.fnt");
    if (rom.empty() || mapper.empty() || font.empty()) {
        fprintf(stderr, "guest_test: не читаются данные (ROM/маппер/шрифт)\n");
        return 1;
    }
    if (rom.size() > 24 * 1024) rom.resize(24 * 1024);

    auto& mb = korvet::Motherboard::get_instance();
    korvet::mb_config_t cfg = {};
    cfg.fdc_enabled = true;
    cfg.gzu_pages = 4;
    cfg.lan_addr = 1;
    cfg.emulation_speed = 100;
    cfg.sound_enabled = true;
    cfg.keyboard_layout = 2;
    cfg.log_enabled = 0;
    mb.init(&cfg);
    mb.load_data(rom.data(), rom.size(), mapper.data(), mapper.size(),
                 font.data(), font.size());
    if (disk_path) {
        g_disk = fopen(disk_path, "rb");
        if (!g_disk) { fprintf(stderr, "guest_test: не читается диск %s\n", disk_path); return 1; }
    }
    static Adapter adapter;
    mb.fdd_set_image_adapter(&adapter);
    mb.power_on();

    // Загрузка
    for (int i = 0; i < boot_frames; i++) mb.run_frame();
    printf("=== ПОСЛЕ ЗАГРУЗКИ (%d кадров, PC=%04X) ===\n", boot_frames, mb.readout_pc());
    dump_screen(mb);

    // Ввод команды
    if (command && strcmp(command, "-") != 0) {
        type_line(mb, command);
        printf("=== ПОСЛЕ ВВОДА '%s' ===\n", command);
        dump_screen(mb);
    }

    // Прогон
    for (int i = 0; i < run_frames; i++) mb.run_frame();
    printf("=== ФИНАЛ (%d кадров, PC=%04X) ===\n", run_frames, mb.readout_pc());
    dump_screen(mb);

    if (shot_path && !save_shot(mb, shot_path)) {
        fprintf(stderr, "guest_test: не пишется скриншот %s\n", shot_path);
    }
    return 0;
}
