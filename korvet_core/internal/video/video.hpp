#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <memory>

namespace korvet {

struct mb_config_t;
struct mb_video_state_t;

struct RgbColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

class Video {
public:
    static constexpr uint16_t GZU_PLANE_SIZE = 16 * 1024;
    static constexpr uint16_t ACZU_SIZE = 1024;
    static constexpr uint16_t LUT_SIZE = 16;
    static constexpr uint16_t SCREEN_WIDTH = 512;
    static constexpr uint16_t SCREEN_HEIGHT = 256;
    static constexpr uint32_t FRAMEBUFFER_SIZE = static_cast<uint32_t>(SCREEN_WIDTH) * SCREEN_HEIGHT;  // Fixed size calculation
    static constexpr uint32_t VBLANK_TICKS = 8170;  // T-states in vertical blanking period
    
    void init(const mb_config_t* config);
    void reset();
    void set_font(const uint8_t* data, size_t size);
    
    // GZU (графическое ЗУ)
    void gzu_write(uint16_t addr, uint8_t data);
    uint8_t gzu_read(uint16_t addr);
    void gzu_set_page(uint8_t page);
    
    // ACZU (алфавитно-цифровое ЗУ)
    void aczu_write(uint16_t addr, uint8_t data);
    uint8_t aczu_read(uint16_t addr);
    
    // LUT (палитра)
    void lut_write(uint8_t value);
    void lut_update_rgb();
    void set_lut_entry(uint8_t index, uint32_t rgb);
    uint32_t get_lut_entry(uint8_t index) const { return lut_[index]; }
    const RgbColor* get_lut_rgb() const { return lut_rgb_.data(); }
    
    // BW (ч/б) mode
    void set_bw_mode(bool enable) { bw_mode_ = enable; lut_update_rgb(); }
    bool get_bw_mode() const { return bw_mode_; }
    
    // Масштабирование яркости LUT (0-63 → 0-255)
    void set_lut_scale(float r, float g, float b) { lut_r_scale_ = r; lut_g_scale_ = g; lut_b_scale_ = b; lut_update_rgb(); }
    
    // Рендеринг
    void render_scanline(uint8_t y, uint8_t* buffer);
    void render_frame(uint8_t* framebuffer);
    bool is_frame_modified() const { return frame_modified_; }
    void clear_frame_modified() { frame_modified_ = false; }
    
    // Регистры
    void write_vireg(uint8_t value);
    void write_ncreg(uint8_t value);
    uint8_t get_ncreg() const { return ncreg_; }
    uint8_t get_vireg() const { return vireg_; }
    
    // Хук смены системного регистра (для Memory)
    void on_sysreg_change(uint8_t sysreg);
    
    // Состояние для отладки
    mb_video_state_t get_state() const;
    
    // Статус VBLANK для PPI
    bool is_in_vblank(uint32_t ticks) const { return ticks < VBLANK_TICKS; }
    uint8_t get_aczu_attr_read() const { return visst_; }
    
    // Геттеры/сеттеры для управления из PPI
    uint8_t get_gzu_read_page() const { return gzu_read_page_; }
    void set_gzu_read_page(uint8_t page) { gzu_read_page_ = page; }
    uint8_t get_gzu_show_page() const { return gzu_show_page_; }
    void set_gzu_show_page(uint8_t page) { gzu_show_page_ = page; }
    uint8_t get_aczu_attr_write() const { return aczu_attr_write_; }
    void set_aczu_attr_write_mode(uint8_t mode) { aczu_attr_write_ = mode; }
    bool get_aczu_wide_mode() const { return aczu_wide_mode_; }
    void set_aczu_wide_mode(bool wide) { aczu_wide_mode_ = wide; }
    bool get_aczu_second_font() const { return aczu_second_font_; }
    void set_aczu_second_font(bool second) { aczu_second_font_ = second; }
    uint8_t get_current_line() const { return current_line_; }

    // Флаг записи атрибута ACZU для бита 3 PPI1-A (VISTS)
    bool get_aczu_attr_written() const { return aczu_attr_written_; }
    void clear_aczu_attr_written() { aczu_attr_written_ = false; }

    // Отладочное управление
    void set_aczu_write_protect(bool v) { aczu_write_protect_ = v; }
    void set_lut_write_protect(bool v) { lut_write_protect_ = v; }
    void force_all_screen_update() { full_redraw_pending_ = true; }
    
    // Доступ к кадровому буферу (legacy API): возвращает back-буфер —
    // массив, в который render_frame() пишет последний отрендеренный кадр.
    // Хост обязан вызывать рендер (Motherboard::render_video_frame())
    // до чтения.
    uint8_t* get_framebuffer() { return back_framebuffer_.data(); }

    // === Double-buffering API (zero-copy для GUI) ===
    // Перед рендерингом кадра видео работает с back_framebuffer_ (back).
    // Перед emit в GUI вызывается swap_frame_buffers() — back становится front,
    // старый front становится back (и будет перезаписан следующим рендером).
    // GUI хранит shared_ptr на front и читает из него, пока эмуляция пишет
    // в новый back. Никаких гонок, никаких std::copy 128 КБ.
    //
    // Доступ к front-буферу для GUI (только для чтения, через shared_ptr).
    std::shared_ptr<const uint8_t[]> get_front_framebuffer() const;
    // Переключить front/back (вызывать в конце render_frame перед emit).
    void swap_frame_buffers();

    // Сырые данные для дампов
    const uint8_t* get_aczu_data() const { return aczu_.data(); }
    size_t get_aczu_size() const { return aczu_.size(); }
    const auto& get_gzu_planes() const { return gzu_planes_; }
    size_t get_gzu_planes_size() const { return sizeof(gzu_planes_); }
    const uint32_t* get_lut_data() const { return lut_.data(); }
    size_t get_lut_size() const { return lut_.size(); }

    // Raw bulk-запись для хостовых инструментов (загрузка дампов/файлов
    // в память видео). Никакой логики видеорегистров — прямая запись
    // в массивы + пометка «кадр изменён», чтобы следующий render_frame()
    // всё перерисовал.
    //
    // ACZU: сырой формат — чередование [символ, атрибут] × 1024 (2048
    // байт), ровно как в get_aczu_data()/read_phys("aczu").
    void write_aczu_raw(size_t offset, const uint8_t* data, size_t len);

    // GZU: сырой доступ к плоскостям. Раскладка страницы — чередующаяся:
    // 4 байта на адрес (плоскость 0, 1, 2 + паддинг), как в
    // get_gzu_planes()/read_phys("gzu"). Возвращает начало ВСЕГО массива
    // (4 страницы × 64 КБ подряд); страница p занимает байты p*65536 .. p*65536+65535.
    uint8_t* get_gzu_planes_writable() { return reinterpret_cast<uint8_t*>(gzu_planes_.data()); }

private:
    // GZU: 4 страницы × 3 плоскости × 16 КБ
    std::array<std::array<uint8_t, GZU_PLANE_SIZE * 4>, 4> gzu_planes_;  // 4 pages with 4 bytes per address (3 planes + padding)
    uint8_t gzu_read_page_;     // Для CPU
    uint8_t gzu_show_page_;     // Для отображения
    uint8_t gzu_color_mode_;    // COLORMD
    uint8_t gzu_read_mask_;     // RDMSK
    uint8_t gzu_write_mask_;    // WRMSK
    uint8_t gzu_write_bit_;     // WBIT
    uint8_t gzu_size_mask_;     // GZU page size mask (0x0F=4 pages, 0x00=1 page)
    
    // ACZU: 1 КБ
    std::array<uint8_t, ACZU_SIZE * 2> aczu_;  // With attribute bytes
    std::array<bool, ACZU_SIZE> aczu_char_dirty_;  // Per-character dirty flag
    uint8_t aczu_attr_write_;   // Режим записи атрибутов
    bool aczu_wide_mode_;       // 32 символа в строке
    bool aczu_second_font_;     // Альтернативный шрифт
    bool aczu_dirty_;
    
    // LUT: 16 цветов
    std::array<uint32_t, LUT_SIZE> lut_;
    std::array<RgbColor, LUT_SIZE> lut_rgb_;  // RGB conversion of LUT values
    bool bw_mode_ = false;  // Black & white mode
    float lut_r_scale_ = 255.0f;
    float lut_g_scale_ = 255.0f;
    float lut_b_scale_ = 255.0f;
    uint8_t ncreg_;  // NCREG register
    uint8_t vireg_;  // VIREG register
    uint8_t visst_;  // VISST register

    // Флаг записи атрибута ACZU для бита 3 PPI1-A (VISTS)
    // Устанавливается при записи в ACZU, сбрасывается при чтении ACZU
    bool aczu_attr_written_ = false;

    // Отладочные флаги
    bool aczu_write_protect_ = false;
    bool lut_write_protect_ = false;
    
    // Шрифт: файл знакогенератора — ДВА набора символов по 4096 байт
    // (основной + псевдографика; «в знакогенераторе находятся два
    // набора символов» — схема, константа FONT1=4 в описании
    // элементов). Широкий режим использует ТЖЕ байты через таблицу
    // развёртки WEXPAND — отдельный буфер 16 КБ не нужен (RP2040).
    static constexpr uint16_t FONT_SIZE = 16 * 256 * 2;  // 8192: 16 байт/символ × 256 × 2 набора
    std::array<uint8_t, FONT_SIZE> font_;

    // Развёртка ниббла для широкого режима: каждый бит дублируется
    // (пиксель длится два такта пиксельной частоты при MODSEL).
    // 16 байт вместо буфера широкого шрифта 16 КБ — экономия ОЗУ
    // для RP2040; результат побитово идентичен поэтапной развёртке.
    static constexpr uint8_t WEXPAND[16] = {
        0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
        0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF
    };

    bool frame_modified_;
    uint8_t current_line_;   ///< текущая линия развёртки (для отладки)
    
    // === Двойная буферизация кадра ===
    // back_framebuffer_ — пишется в render_frame() эмуляцией.
    // front_framebuffer_ (через shared_ptr) — отдаётся в GUI.
    // ФИКС B3: настоящий swap через 2 shared_ptr — без std::copy 128 КБ.
    // Каждый swap передаёт GUI указатель на готовый кадр и переиспользует
    // освободившийся буфер как новый back. Lock-free и без гонок.
    std::array<uint8_t, FRAMEBUFFER_SIZE> back_framebuffer_;
    std::shared_ptr<uint8_t[]> front_framebuffer_;
    std::shared_ptr<uint8_t[]> back_alloc_;       // аллоцированный back (на случай если GUI его ещё держит)
    
    // Обновление кадрового буфера ACZU
    void update_aczu_framebuffer();
    
    // Флаги обновления строк развёртки
    std::array<bool, 256> line_update_flags_;
    bool full_redraw_pending_;
    
    // Константы видеосистемы
    static constexpr uint8_t COLORMD = 0x80;
    static constexpr uint8_t RDMSK = 0x70;
    static constexpr uint8_t WRMSK = 0x0E;
    static constexpr uint8_t WBIT = 0x01;
    static constexpr uint8_t WSEL0 = 0xfd;
    static constexpr uint8_t WSEL1 = 0xfb;
    static constexpr uint8_t WSEL2 = 0xf7;
    static constexpr uint8_t RSEL0 = 0x10;
    static constexpr uint8_t RSEL1 = 0x20;
    static constexpr uint8_t RSEL2 = 0x40;
    
    // Базовый цвет палитры
    static constexpr uint8_t LUT_BASE_COLOR = 0x80;
    
    // Внутренний helper
    uint8_t lut_index_;
};

}  // namespace korvet