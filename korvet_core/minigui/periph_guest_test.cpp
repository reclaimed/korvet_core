// ---------------------------------------------------------------------------
// periph_guest_test.cpp — верификация ИРПС/мыши/джойстика/LAN гостевым кодом.
//
// Загружает CP/M (ПЗУ SHCOMP), затем перехватывает PC на 8080-программу
// periph_prog.hpp (см. scripts/gen_periph_prog.py): она работает через
// реальные окна ввода-вывода 0FBxx — тот же путь, что и настоящее гостевое
// ПО (драйверы мыши, сетевые утилиты). Хост подаёт ввод (движение мыши,
// кнопки, джойстик, байт в LAN) и проверяет результаты в RAM.
//
// Два режима:
//   serial   — мышь Microsoft на ИРПС: 3-байтовая посылка через SERD/SERS
//   parallel — мышь ММ8031 (PPI3/ЭУ8040, слот ХР1) + джойстик МД8030 (ХР2)
//
// Использование:
//   periph_guest_test <rom> <disk> <serial|parallel> [shot.png]
// ---------------------------------------------------------------------------

#include "korvet_core/motherboard.hpp"
#include "periph_prog.hpp"
#include "png_writer.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <functional>

static FILE* g_disk = nullptr;
struct Adapter : public korvet::IFddImageAdapter {
    bool is_present(uint8_t) const override { return g_disk != nullptr; }
    bool is_write_protected(uint8_t) const override { return true; }
    bool read_sector(uint8_t, uint8_t track, uint8_t side, uint8_t sector, uint8_t* buf, size_t n) override {
        if (sector >= 5) return false;
        long pos = (long)track * 10 * 1024 + side * 5 * 1024 + sector * 1024;
        if (fseek(g_disk, pos, SEEK_SET) != 0) return false;
        return fread(buf, 1, n, g_disk) == n;
    }
    bool write_sector(uint8_t, uint8_t, uint8_t, uint8_t, const uint8_t*, size_t) override { return false; }
};

static std::vector<uint8_t> rd(const char* p) {
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
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Использование: periph_guest_test <rom> <disk> <serial|parallel> [shot.png]\n");
        return 2;
    }
    const bool mode_serial = strcmp(argv[3], "serial") == 0;
    const char* shot_path = argc > 4 ? argv[4] : nullptr;

    auto rom = rd(argv[1]);
    // маппер/шрифт лежат рядом с ПЗУ (data/rom/...) — считаем от пути ПЗУ,
    // чтобы инструмент работал из любого каталога (и под ctest).
    std::string romdir(argv[1]);
    size_t slash = romdir.find_last_of("/\\");
    romdir = slash == std::string::npos ? "." : romdir.substr(0, slash);
    auto mapper = rd((romdir + "/mapper.mem").c_str());
    auto font = rd((romdir + "/korvet.fnt").c_str());
    if (rom.empty() || mapper.empty() || font.empty()) {
        fprintf(stderr, "periph_guest_test: нет данных (ROM/маппер/шрифт)\n");
        return 1;
    }
    if (rom.size() > 24 * 1024) rom.resize(24 * 1024);

    auto& mb = korvet::Motherboard::get_instance();
    korvet::mb_config_t cfg = {};
    cfg.fdc_enabled = true;
    cfg.gzu_pages = 4;
    cfg.lan_addr = 5;            // НЕстандартный адрес РМУ — проверим LANADR
    cfg.emulation_speed = 100;
    cfg.sound_enabled = false;
    cfg.keyboard_layout = 2;
    cfg.log_enabled = 0;
    mb.init(&cfg);
    mb.load_data(rom.data(), rom.size(), mapper.data(), mapper.size(),
                 font.data(), font.size());
    g_disk = fopen(argv[2], "rb");
    if (!g_disk) { fprintf(stderr, "periph_guest_test: нет диска %s\n", argv[2]); return 1; }
    static Adapter adapter;
    mb.fdd_set_image_adapter(&adapter);
    mb.power_on();

    // Перехват байтов передачи (ИРПС наружу + LAN наружу)
    std::vector<uint8_t> serial_tx, lan_tx;
    mb.serial_set_byte_out_callback([&](uint8_t b) { serial_tx.push_back(b); });
    mb.lan_set_byte_out_callback([&](uint8_t b) { lan_tx.push_back(b); });

    // --- Загрузка CP/M ---
    for (int i = 0; i < 700; i++) mb.run_frame();
    printf("CP/M загружен (PC=%04X), перехват на гостевую программу...\n",
           mb.readout_pc());

    // --- Инъекция программы в TPA ---
    uint8_t* ram = mb.readout_ram();
    memcpy(ram + korvet::minigui::PERIPH_PROG_ADDR,
           korvet::minigui::PERIPH_PROG, korvet::minigui::PERIPH_PROG_SIZE);
    memset(ram + korvet::minigui::PERIPH_RES_ADDR, 0x00, 16);  // чистые результаты
    korvet::cpu_state_t st = mb.cpu_get_state();
    st.pc = korvet::minigui::PERIPH_PROG_ADDR;
    st.iff = 0;                  // DI: опрос без прерываний гостевой ОС
    mb.cpu_set_state(st);

    // --- Ввод по режиму ---
    if (mode_serial) {
        mb.mouse_set_mode(korvet::mb_mouse_mode_t::MOUSE_SERIAL);
        // +17, -3 с левой кнопкой («клавиша 1»): послка 6C 11 3D
        mb.mouse_set_buttons(0x01);
        mb.mouse_move(17, -3);
        printf("Режим SERIAL: мышь +17,-3, левая кнопка (ожидаются 6C 11 3D)\n");
    } else {
        mb.mouse_set_mode(korvet::mb_mouse_mode_t::MOUSE_PARALLEL);
        mb.joystick_set_connected(true);
        mb.joystick_set_direction(korvet::Motherboard::joystick_direction_t::JOY_RIGHT, true);
        mb.joystick_set_button(0, true);   // «Пуск1»
        // СЧХ = 8+5 = 13, СЧУ = 8-3 = 5 (ось Y инвертирована)
        mb.mouse_set_buttons(0x01);        // «Упр1»
        mb.mouse_move(5, 3);
        printf("Режим PARALLEL: ММ8031 +5,+3, Упр1; джойстик ВПРАВО+Пуск1\n");
    }

    // --- Прогон с подсадкой байта в LAN ---
    // Байт 0x5A подаётся дважды (кадры 5 и 100): первый — пока канал
    // «прозрачный», второй — после инициализации УСАПП программой.
    // Обе подсадки дают ОДНО значение: приём определяется по содержимому.
    for (int f = 1; f <= 150; f++) {
        if (f == 5 || f == 100) mb.lan_inject_byte(0x5A);
        mb.run_frame();
        if (f == 1 || f == 100) {
            st = mb.cpu_get_state();
            printf("  кадр %3d: PC=%04X (прогон до маркера)\n", f, st.pc);
        }
    }

    // --- Сбор результатов ---
    const uint8_t* R = ram + korvet::minigui::PERIPH_RES_ADDR;
    printf("\nРезультаты гостевой программы (RAM 0x0200):\n");
    printf("  RES+0..3  (ИРПС):   %02X %02X %02X %02X\n", R[0], R[1], R[2], R[3]);
    printf("  RES+4..5  (джойст): %02X %02X\n", R[4], R[5]);
    printf("  RES+6..8  (ММ8031): %02X %02X %02X\n", R[6], R[7], R[8]);
    printf("  RES+9..11 (LAN):    %02X %02X %02X\n", R[9], R[10], R[11]);
    printf("  RES+15    (маркер): %02X\n", R[15]);
    printf("  ИРПС наружу (%zu байт): ", serial_tx.size());
    for (uint8_t b : serial_tx) printf("%02X ", b);
    printf("\n  LAN наружу (%zu байт):   ", lan_tx.size());
    for (uint8_t b : lan_tx) printf("%02X ", b);
    printf("\n\n");

    // --- Проверки ---
    int failed = 0;
    auto check = [&](bool ok, const char* name) {
        printf("[GUEST] %-52s %s\n", name, ok ? "OK" : "FAIL");
        if (!ok) failed++;
    };

    // Маркер завершения: программа дошла до конца
    check(R[15] == 0xC3, "программа завершилась (маркер 0xC3)");

    if (mode_serial) {
        // Посылка мыши: b1=0x40|0x20(левая)|((0xFD&0xC0)>>4)=0x0C
        check(R[0] == 0x6C, "ИРПС: байт 1 = 6C (0x40|кн.|Y6-5|X6-5)");
        check(R[1] == 0x11, "ИРПС: байт 2 = 11 (dx=17, 6 бит)");
        check(R[2] == 0x3D, "ИРПС: байт 3 = 3D (dy=-3, 6 бит)");
        // Статус после опустошения: RRDY снят, TRDY|TXBE|DSR остаются
        check((R[3] & 0x02) == 0 && (R[3] & 0x85) == 0x85,
              "ИРПС: статус после чтения — RRDY снят, TRDY|TXBE|DSR");
    } else {
        // Джойстик: ВПРАВО (бит 3) и Пуск1 (бит 4) — активный НИЗКИЙ
        check((R[4] & 0x08) == 0 && (R[4] & 0x10) == 0 &&
              (R[4] & 0x04) != 0 && (R[4] & 0x20) != 0,
              "джойстик: Вправо+Пуск1 в PPI3-B (акт. низкий)");
        check(R[5] == 0xFF, "джойстик: Разрешение=1 -> 0xFF (штекер вынут)");
        // ММ8031: СЧХ = 13|0x20 (Упр1 акт. низкий), СЧУ = 5|0x20,
        // после импульса инициализации СЧХ = 8|0x20
        check(R[6] == (13 | 0x20), "ММ8031: СЧХ через PPI3 = 13|Упр1");
        check(R[7] == (5 | 0x20), "ММ8031: СЧУ через PPI3 = 5|Упр1");
        check(R[8] == (8 | 0x20), "ММ8031: сброс импульсом -> 1000b");
    }

    // LAN — в обоих режимах
    check((R[9] & 0xF0) == 0x50, "LAN: LANADR из PPI1-A = 5 (биты 4-7)");
    check((R[10] & 0x05) == 0x05, "LAN: LANS = TRDY|TXBE после инициализации");
    check(R[11] == 0x5A, "LAN: принятый байт из канала = 5A");
    check(serial_tx.size() == 2 && serial_tx[0] == 0xA5 && serial_tx[1] == 0x5A,
          "ИРПС: передача наружу A5 5A (byte_out колбэк)");
    check(lan_tx.size() == 2 && lan_tx[0] == 0x77 && lan_tx[1] == 0x99,
          "LAN: передача в канал 77 99 (byte_out колбэк)");

    printf("[GUEST] %s: %d неудач\n",
           failed == 0 ? "ВСЕ ГостЕВЫЕ ПРОВЕРКИ ПРОЙДЕНЫ" : "ЕСТЬ НЕУДАЧИ", failed);

    if (shot_path) {
        mb.render_video_frame();
        const uint8_t* frame = mb.readout_video();
        const korvet::RgbColor* lut = mb.readout_lut_rgb();
        std::vector<uint8_t> rgb(512 * 256 * 3);
        for (int i = 0; i < 512 * 256; i++) {
            uint8_t c = frame[i] & 0x0F;
            rgb[i * 3 + 0] = lut[c].r;
            rgb[i * 3 + 1] = lut[c].g;
            rgb[i * 3 + 2] = lut[c].b;
        }
        korvet::minigui::write_png_rgb(shot_path, rgb.data(), 512, 256);
    }
    return failed == 0 ? 0 : 1;
}
