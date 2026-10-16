#pragma once

// ---------------------------------------------------------------------------
// wav_writer.hpp — простой WAV-писатель (16 бит, моно) для дампа звука.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cstdio>
#include <vector>

namespace korvet {
namespace minigui {

class WavWriter {
public:
    bool open(const char* path, uint32_t sample_rate) {
        close();
        f_ = fopen(path, "wb");
        if (!f_) return false;
        sample_rate_ = sample_rate;
        // Резерв под заголовок (44 байта) — перезапишем в close()
        const uint8_t header[44] = {0};
        if (fwrite(header, 1, sizeof(header), f_) != sizeof(header)) {
            fclose(f_);
            f_ = nullptr;
            return false;
        }
        return true;
    }

    void write_samples(const int16_t* samples, size_t count) {
        if (!f_) return;
        if (fwrite(samples, sizeof(int16_t), count, f_) != count) {
            error_ = true;
        }
        data_bytes_ += static_cast<uint32_t>(count * sizeof(int16_t));
    }

    bool close() {
        if (!f_) return true;
        bool ok = !error_;
        if (fseek(f_, 0, SEEK_SET) == 0) {
            const uint32_t byte_rate = sample_rate_ * 2;
            const uint8_t header[44] = {
                'R', 'I', 'F', 'F',
                static_cast<uint8_t>((36 + data_bytes_) & 0xFF),
                static_cast<uint8_t>(((36 + data_bytes_) >> 8) & 0xFF),
                static_cast<uint8_t>(((36 + data_bytes_) >> 16) & 0xFF),
                static_cast<uint8_t>(((36 + data_bytes_) >> 24) & 0xFF),
                'W', 'A', 'V', 'E',
                'f', 'm', 't', ' ',
                16, 0, 0, 0,          // размер fmt-чанка
                1, 0,                  // PCM
                1, 0,                  // моно
                static_cast<uint8_t>(sample_rate_ & 0xFF),
                static_cast<uint8_t>((sample_rate_ >> 8) & 0xFF),
                static_cast<uint8_t>((sample_rate_ >> 16) & 0xFF),
                static_cast<uint8_t>((sample_rate_ >> 24) & 0xFF),
                static_cast<uint8_t>(byte_rate & 0xFF),
                static_cast<uint8_t>((byte_rate >> 8) & 0xFF),
                static_cast<uint8_t>((byte_rate >> 16) & 0xFF),
                static_cast<uint8_t>((byte_rate >> 24) & 0xFF),
                2, 0,                  // блок (2 байта)
                16, 0,                 // бит на сэмпл
                'd', 'a', 't', 'a',
                static_cast<uint8_t>(data_bytes_ & 0xFF),
                static_cast<uint8_t>((data_bytes_ >> 8) & 0xFF),
                static_cast<uint8_t>((data_bytes_ >> 16) & 0xFF),
                static_cast<uint8_t>((data_bytes_ >> 24) & 0xFF)};
            if (fwrite(header, 1, sizeof(header), f_) != sizeof(header)) {
                ok = false;
            }
        } else {
            ok = false;
        }
        fclose(f_);
        f_ = nullptr;
        data_bytes_ = 0;
        error_ = false;
        return ok;
    }

    ~WavWriter() { close(); }

    bool is_open() const { return f_ != nullptr; }

private:
    FILE* f_ = nullptr;
    uint32_t sample_rate_ = 22050;
    uint32_t data_bytes_ = 0;
    bool error_ = false;
};

}  // namespace minigui
}  // namespace korvet
