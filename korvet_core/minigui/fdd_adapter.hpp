#pragma once

// ---------------------------------------------------------------------------
// fdd_adapter.hpp — реализация IFddImageAdapter поверх std::FILE (host-код).
//
// Адаптер держит открытые файлы образов KDI (819 200 байт = 80 дорожек ×
// 2 стороны × 5 секторов × 1024 байта) и читает секторы по смещению:
//   позиция = track*10*1024 + side*5*1024 + (sector-1)*1024
// (раскладка формата КДИ). Ядро (Fdc) ничего не знает о файлах.
// ---------------------------------------------------------------------------

#include "../internal/fdc/fdc_image_adapter.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace korvet {
namespace minigui {

class FileFddAdapter : public IFddImageAdapter {
public:
    ~FileFddAdapter() override {
        for (auto& f : files_) {
            if (f) fclose(f);
        }
    }

    /// Вставить образ в привод (0..3). Возвращает false при ошибке файла.
    bool insert(uint8_t drive, const char* path, bool write_protect = false) {
        if (drive >= 4) return false;
        if (files_[drive]) {
            fclose(files_[drive]);
            files_[drive] = nullptr;
        }
        FILE* f = fopen(path, write_protect ? "rb" : "r+b");
        if (!f) {
            // Попытка только на чтение (файл может быть без прав на запись)
            f = fopen(path, "rb");
            write_protect_[drive] = true;
        } else {
            write_protect_[drive] = write_protect;
        }
        files_[drive] = f;
        return f != nullptr;
    }

    bool is_present(uint8_t drive) const override {
        return drive < 4 && files_[drive] != nullptr;
    }

    bool is_write_protected(uint8_t drive) const override {
        return drive >= 4 || write_protect_[drive];
    }

    bool read_sector(uint8_t drive, uint8_t track, uint8_t side,
                     uint8_t sector, uint8_t* buf, size_t buf_size) override {
        // ВНИМАНИЕ: ядро передаёт НУЛЕВОЙ номер сектора (0..4) — оно само
        // вычитает единицу из регистра ВГ93. Раскладка КДИ:
        //   позиция = track*10*1024 + side*5*1024 + sector*1024
        if (drive >= 4 || !files_[drive]) return false;
        if (buf_size < Fdc::SECTOR_SIZE) return false;
        if (track >= Fdc::TRACKS || side >= Fdc::SIDES ||
            sector >= Fdc::SECTORS_PER_TRACK) {
            return false;
        }
        long pos = static_cast<long>(track) * 10 * Fdc::SECTOR_SIZE +
                   static_cast<long>(side) * 5 * Fdc::SECTOR_SIZE +
                   static_cast<long>(sector) * Fdc::SECTOR_SIZE;
        if (fseek(files_[drive], pos, SEEK_SET) != 0) return false;
        return fread(buf, 1, Fdc::SECTOR_SIZE, files_[drive]) == Fdc::SECTOR_SIZE;
    }

    bool write_sector(uint8_t drive, uint8_t track, uint8_t side,
                      uint8_t sector, const uint8_t* buf, size_t buf_size) override {
        // sector — нулевой номер (см. read_sector)
        if (drive >= 4 || !files_[drive]) return false;
        if (buf_size < Fdc::SECTOR_SIZE) return false;
        if (track >= Fdc::TRACKS || side >= Fdc::SIDES ||
            sector >= Fdc::SECTORS_PER_TRACK) {
            return false;
        }
        long pos = static_cast<long>(track) * 10 * Fdc::SECTOR_SIZE +
                   static_cast<long>(side) * 5 * Fdc::SECTOR_SIZE +
                   static_cast<long>(sector) * Fdc::SECTOR_SIZE;
        if (fseek(files_[drive], pos, SEEK_SET) != 0) return false;
        return fwrite(buf, 1, Fdc::SECTOR_SIZE, files_[drive]) == Fdc::SECTOR_SIZE;
    }

private:
    FILE* files_[4] = {nullptr, nullptr, nullptr, nullptr};
    bool write_protect_[4] = {true, true, true, true};
};

}  // namespace minigui
}  // namespace korvet
