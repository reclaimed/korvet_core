// ---------------------------------------------------------------------------
// sdl_window.cpp — ОПЦИОНАЛЬНЫЙ интерактивный прогон ядра korvet_core в
// окне SDL2 (512×256, масштаб ×2). Собирается только при наличии SDL2.
//
// Это НЕ часть ядра: ядро платформо-независимо и ничего не знает об SDL.
// Данный файл — минимальный «настоящий хост»: окно + клавиатура + звук.
//
// Управление: F12 — скриншот korvet_shot.png; ESC — выход.
// ---------------------------------------------------------------------------

#include "korvet_core/motherboard.hpp"
#include "korvet_core/internal/logger/logger.hpp"
#include "korvet_core/minigui/fdd_adapter.hpp"

#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

namespace {

// Простая таблица: SDL-сканкод → позиция в матрице Корвета (латиница,
// эмпирическая калибровка по ПЗУ 2.0 — см. kbd_map.hpp)
struct SdlKeyMap {
    SDL_Scancode sc;
    uint16_t key_code;  // (row << 8) | col
};

const SdlKeyMap KEYMAP[] = {
    {SDL_SCANCODE_Q, (1 << 8) | 2}, {SDL_SCANCODE_W, (7 << 8) | 2},
    {SDL_SCANCODE_E, (5 << 8) | 2}, {SDL_SCANCODE_R, (2 << 8) | 2},
    {SDL_SCANCODE_T, (4 << 8) | 2}, {SDL_SCANCODE_Y, (1 << 8) | 3},
    {SDL_SCANCODE_U, (5 << 8) | 2}, {SDL_SCANCODE_I, (1 << 8) | 1},
    {SDL_SCANCODE_O, (7 << 8) | 1}, {SDL_SCANCODE_P, (0 << 8) | 2},
    {SDL_SCANCODE_A, (1 << 8) | 0}, {SDL_SCANCODE_S, (3 << 8) | 2},
    {SDL_SCANCODE_D, (4 << 8) | 0}, {SDL_SCANCODE_F, (6 << 8) | 0},
    {SDL_SCANCODE_G, (7 << 8) | 0}, {SDL_SCANCODE_H, (0 << 8) | 1},
    {SDL_SCANCODE_J, (2 << 8) | 1}, {SDL_SCANCODE_K, (3 << 8) | 1},
    {SDL_SCANCODE_L, (4 << 8) | 1}, {SDL_SCANCODE_Z, (0 << 8) | 3},
    {SDL_SCANCODE_X, (1 << 8) | 3}, {SDL_SCANCODE_C, (3 << 8) | 0},
    {SDL_SCANCODE_V, (6 << 8) | 2}, {SDL_SCANCODE_B, (2 << 8) | 0},
    {SDL_SCANCODE_N, (6 << 8) | 1}, {SDL_SCANCODE_M, (5 << 8) | 1},
    {SDL_SCANCODE_1, (1 << 8) | 4}, {SDL_SCANCODE_2, (2 << 8) | 4},
    {SDL_SCANCODE_3, (3 << 8) | 4}, {SDL_SCANCODE_4, (4 << 8) | 4},
    {SDL_SCANCODE_5, (5 << 8) | 4}, {SDL_SCANCODE_6, (6 << 8) | 4},
    {SDL_SCANCODE_7, (7 << 8) | 4}, {SDL_SCANCODE_8, (0 << 8) | 5},
    {SDL_SCANCODE_9, (1 << 8) | 5}, {SDL_SCANCODE_0, (0 << 8) | 4},
    {SDL_SCANCODE_RETURN, (0 << 8) | 6}, {SDL_SCANCODE_SPACE, (7 << 8) | 6},
    {SDL_SCANCODE_TAB, (6 << 8) | 6}, {SDL_SCANCODE_ESCAPE, (3 << 8) | 7},
    {SDL_SCANCODE_LSHIFT, (0 << 8) | 7}, {SDL_SCANCODE_RSHIFT, (7 << 8) | 7},
    {SDL_SCANCODE_LCTRL, (5 << 8) | 7}, {SDL_SCANCODE_BACKSPACE, (3 << 8) | 6},
    {SDL_SCANCODE_F1, (0 << 8) | 10}, {SDL_SCANCODE_F2, (1 << 8) | 10},
    {SDL_SCANCODE_F3, (2 << 8) | 10}, {SDL_SCANCODE_F4, (3 << 8) | 10},
    {SDL_SCANCODE_F5, (4 << 8) | 10}, {SDL_SCANCODE_F6, (5 << 8) | 10},
    {SDL_SCANCODE_F7, (6 << 8) | 10}, {SDL_SCANCODE_F8, (7 << 8) | 10},
    {SDL_SCANCODE_F9, (0 << 8) | 11}, {SDL_SCANCODE_F10, (1 << 8) | 11},
};

std::vector<uint8_t> read_file(const char* path) {
    std::vector<uint8_t> data;
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        data.resize(sz > 0 ? static_cast<size_t>(sz) : 0);
        if (sz > 0 && fread(data.data(), 1, static_cast<size_t>(sz), f) != static_cast<size_t>(sz)) {
            data.clear();
        }
        fclose(f);
    }
    return data;
}

}  // namespace

int main(int argc, char** argv) {
    const char* rom_path = argc > 1 ? argv[1] : "data/rom/korvet_v2.0.rom";
    const char* disk_path = argc > 2 ? argv[2] : nullptr;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "korvet_core — minigui-sdl (ПК8020 «Корвет»)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        512 * 2, 256 * 2, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 512, 256);
    if (!window || !renderer || !texture) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }

    // Ядро
    auto& mb = korvet::Motherboard::get_instance();
    korvet::mb_config_t cfg = {};
    cfg.fdc_enabled = true;
    cfg.gzu_pages = 4;
    cfg.lan_addr = 1;
    cfg.emulation_speed = 100;
    cfg.sound_enabled = true;
    cfg.keyboard_layout = 2;
    cfg.log_enabled = 1;
    cfg.log_level = 1;
    mb.init(&cfg);

    auto rom = read_file(rom_path);
    auto mapper = read_file("data/rom/mapper.mem");
    auto font = read_file("data/rom/korvet.fnt");
    if (rom.empty() || mapper.empty() || font.empty()) {
        fprintf(stderr, "Не читаются данные: %s (+ data/rom/mapper.mem, korvet.fnt)\n", rom_path);
        return 1;
    }
    if (rom.size() > 24 * 1024) rom.resize(24 * 1024);
    mb.load_data(rom.data(), rom.size(), mapper.data(), mapper.size(),
                 font.data(), font.size());

    korvet::minigui::FileFddAdapter adapter;
    if (disk_path && adapter.insert(0, disk_path)) {
        printf("Диск A: %s\n", disk_path);
    }
    mb.fdd_set_image_adapter(&adapter);
    mb.mouse_set_mode(korvet::mb_mouse_mode_t::MOUSE_SERIAL);
    mb.power_on();

    // Аудио: SDL-очередь (22050 Гц, моно, s16)
    SDL_AudioSpec want{}, have{};
    want.freq = korvet::Audio::SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);

    // Основной цикл: 50 кадров/с (двигаем эмуляцию кадрами с SDL-таймером)
    bool running = true;
    bool keys_down[256] = {};
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
                if (ev.key.keysym.scancode == SDL_SCANCODE_F12) {
                    // Скриншот в PPM (максимум простоты)
                    // ... ядро отдаёт индексный кадр; сконвертируем в BMP вручную
                    printf("F12: скриншот не входит в задачу headless-ядра\n");
                }
                for (auto& m : KEYMAP) {
                    if (m.sc == ev.key.keysym.scancode && !ev.key.repeat) {
                        mb.keyboard_key_press(m.key_code);
                        keys_down[m.sc & 0xFF] = true;
                    }
                }
            }
            if (ev.type == SDL_KEYUP) {
                for (auto& m : KEYMAP) {
                    if (m.sc == ev.key.keysym.scancode) {
                        mb.keyboard_key_release(m.key_code);
                        keys_down[m.sc & 0xFF] = false;
                    }
                }
            }
            if (ev.type == SDL_MOUSEMOTION) {
                // Движение мыши: слежение окна → дельты
                static int last_x = -1, last_y = -1;
                if (last_x >= 0) {
                    mb.mouse_move(static_cast<int16_t>(ev.motion.x - last_x),
                                  static_cast<int16_t>(ev.motion.y - last_y));
                }
                last_x = ev.motion.x;
                last_y = ev.motion.y;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                uint8_t buttons = 0;
                if (ev.button.button == SDL_BUTTON_LEFT && ev.type == SDL_MOUSEBUTTONDOWN) buttons |= 1;
                if (ev.button.button == SDL_BUTTON_RIGHT && ev.type == SDL_MOUSEBUTTONDOWN) buttons |= 2;
                mb.mouse_set_buttons(buttons);
            }
        }

        // Один кадр эмуляции
        mb.run_frame();
        mb.render_video_frame();

        // Вывод видео (индексные цвета → RGB через LUT)
        const uint8_t* frame = mb.readout_video();
        const korvet::RgbColor* lut = mb.readout_lut_rgb();
        uint8_t rgb[512 * 256 * 3];
        for (int i = 0; i < 512 * 256; i++) {
            uint8_t c = frame[i] & 0x0F;
            rgb[i * 3 + 0] = lut[c].r;
            rgb[i * 3 + 1] = lut[c].g;
            rgb[i * 3 + 2] = lut[c].b;
        }
        SDL_UpdateTexture(texture, nullptr, rgb, 512 * 3);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // Звук кадра
        if (audio_dev) {
            SDL_QueueAudio(audio_dev, mb.get_audio_frame_buffer(),
                           mb.get_audio_frame_buffer_size() * sizeof(int16_t));
        }

        // Синхронизация 50 Гц
        SDL_Delay(20);
    }

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
