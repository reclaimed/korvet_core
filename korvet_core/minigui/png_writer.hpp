#pragma once

// ---------------------------------------------------------------------------
// png_writer.hpp — минимальный PNG-энкодер (RGB, без зависимостей кроме zlib).
//
// Предназначен для скриншотов видеокадра Корвета (512×256, индексные цвета
// 0-15 → RGB через LUT) в утилитах хоста (minigui). Полноценный GUI-хост
// волен использовать собственный кодек.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <vector>
#include <zlib.h>

namespace korvet {
namespace minigui {

/// Записать PNG-файл: raw-пиксели RGB (3 байта на пиксель, строки сверху
/// вниз, без паддинга). Возвращает true при успехе.
inline bool write_png_rgb(const char* path, const uint8_t* rgb,
                          int width, int height) {
    if (!path || !rgb || width <= 0 || height <= 0) return false;

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = false;

    // Заголовок PNG: сигнатура + IHDR
    const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const uint8_t ihdr[13] = {
        static_cast<uint8_t>((width >> 24) & 0xFF),
        static_cast<uint8_t>((width >> 16) & 0xFF),
        static_cast<uint8_t>((width >> 8) & 0xFF),
        static_cast<uint8_t>(width & 0xFF),
        static_cast<uint8_t>((height >> 24) & 0xFF),
        static_cast<uint8_t>((height >> 16) & 0xFF),
        static_cast<uint8_t>((height >> 8) & 0xFF),
        static_cast<uint8_t>(height & 0xFF),
        8,   // бит на канал
        2,   // цвет: RGB
        0, 0, 0  // сжатие/фильтр/чередование
    };

    auto write_chunk = [&](const char* type, const uint8_t* data, uint32_t len) -> bool {
        uint8_t len_be[4] = {
            static_cast<uint8_t>((len >> 24) & 0xFF),
            static_cast<uint8_t>((len >> 16) & 0xFF),
            static_cast<uint8_t>((len >> 8) & 0xFF),
            static_cast<uint8_t>(len & 0xFF)};
        if (fwrite(len_be, 1, 4, f) != 4) return false;
        if (fwrite(type, 1, 4, f) != 4) return false;
        if (len && fwrite(data, 1, len, f) != len) return false;

        uint32_t crc = crc32(0, reinterpret_cast<const Bytef*>(type), 4);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data), len);
        uint8_t crc_be[4] = {
            static_cast<uint8_t>((crc >> 24) & 0xFF),
            static_cast<uint8_t>((crc >> 16) & 0xFF),
            static_cast<uint8_t>((crc >> 8) & 0xFF),
            static_cast<uint8_t>(crc & 0xFF)};
        return fwrite(crc_be, 1, 4, f) == 4;
    };

    do {
        if (fwrite(signature, 1, 8, f) != 8) break;
        if (!write_chunk("IHDR", ihdr, sizeof(ihdr))) break;

        // Сырые данные со строковым фильтром 0 (None): каждая строка
        // предваряется байтом фильтра.
        const size_t stride = 1 + static_cast<size_t>(width) * 3;
        std::vector<uint8_t> raw(static_cast<size_t>(height) * stride);
        for (int y = 0; y < height; y++) {
            raw[static_cast<size_t>(y) * stride] = 0;
            memcpy(raw.data() + static_cast<size_t>(y) * stride + 1,
                   rgb + static_cast<size_t>(y) * width * 3,
                   static_cast<size_t>(width) * 3);
        }

        // Сжатие zlib (уровень 6)
        uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
        std::vector<uint8_t> compressed(compressed_size);
        if (compress2(compressed.data(), &compressed_size, raw.data(),
                      static_cast<uLong>(raw.size()), 6) != Z_OK) {
            break;
        }
        compressed.resize(compressed_size);

        if (!write_chunk("IDAT", compressed.data(), static_cast<uint32_t>(compressed_size))) break;
        if (!write_chunk("IEND", nullptr, 0)) break;
        ok = true;
    } while (false);

    fclose(f);
    return ok;
}

}  // namespace minigui
}  // namespace korvet
