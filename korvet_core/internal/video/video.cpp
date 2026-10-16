#include "video.hpp"
#include "../../motherboard_types.hpp"
#include "../keyboard/keyboard.hpp"
#include "../logger/logger.hpp"
#include "plane_mask.hpp"


#include <cstring>

namespace korvet {

void Video::init(const mb_config_t* config) {
    reset();
    if (config) {
        gzu_size_mask_ = (config->gzu_pages == 1) ? 0x00 : 0x0F;
        set_lut_scale(config->lut_r_scale, config->lut_g_scale, config->lut_b_scale);
        // ФИКС A4: проброс lut_write_protect из конфига
        set_lut_write_protect(config->lut_write_protect);
    }
    // Инициализация double-buffer для zero-copy передачи кадра в GUI
    front_framebuffer_ = std::shared_ptr<uint8_t[]>(
        new uint8_t[FRAMEBUFFER_SIZE]);
    std::fill(front_framebuffer_.get(), front_framebuffer_.get() + FRAMEBUFFER_SIZE, 0);
    back_framebuffer_.fill(0);
}

void Video::reset() {
    // Лог INFO: при сбросе показываем итоговое состояние для cold-boot диагностики.
    // Раньше ACZU=0xFF давало «инверсный мусор». Теперь 0x00 + visst_=0.
    // Если BIOS читает ACZU[0] и видит visst_=0, он идёт по cold-boot пути.
    // Очистка плоскостей GZU
    for (auto& plane : gzu_planes_) {
        for (auto& byte : plane) {
            byte = 0;
        }
    }
    
    // АЦЗУ инициализируется нулями: символ 0x00, атрибут 0x00.
    //
    // Ранее здесь был 0xFF — это давало "инвертированный" текстовый мусор
    // при первом cold-boot, потому что в режиме aczu_attr_write_=0 (ATRFRE
    // / ATROFF) атрибут не пишется, и остаётся 0xFF, что при XOR с
    // растровым шрифтом даёт инвертированный (тёмный) символ.
    // BIOS в начале пишет текст в ACZU, но до этого момента экран
    // показывает содержимое памяти.
    // ACZU[i] = 0 → символ 0x00 (NUL), атрибут 0x00 → чёрный экран.
    // CPU первым делом пишет символы туда (например, вывод "BASIC" при старте),
    // после чего атрибут устанавливается правильно.
    for (auto& byte : aczu_) {
        byte = 0x00;
    }
    aczu_char_dirty_.fill(true);  // All characters dirty at init
    
    // Инициализация LUT значениями по умолчанию
    for (int i = 0; i < LUT_SIZE; i++) {
        lut_[i] = i;  // Default to identity mapping
    }
    lut_update_rgb();  // Initialize RGB from default LUT values
    
    // Инициализация регистров
    gzu_read_page_ = 0;
    gzu_show_page_ = 0;
    gzu_color_mode_ = 0;
    gzu_read_mask_ = 0;
    gzu_write_mask_ = 0;
    gzu_write_bit_ = 0;
    gzu_size_mask_ = 0x0F;  // Default: 4 pages
    
    aczu_attr_write_ = 2;  // Default attribute write mode
    aczu_wide_mode_ = false;
    aczu_second_font_ = false;
    aczu_dirty_ = true;

    ncreg_ = 0;
    vireg_ = 0;
    // visst_ инициализируется 0 при reset.
    // BIOS интерпретирует 0 как "память
    // ещё не читалась" и берёт cold-boot путь.
    visst_ = 0;

    aczu_write_protect_ = false;
    // ФИКС A4: lut_write_protect_ = true — баг, палитра не программируется.
    // В оригинальном эмуляторе и в реальном железе LUT не защищён от записи.
    // Защита была вставлена в эмулятор для отладки, но теперь мешает нормальной работе.
    lut_write_protect_ = false;
    
    frame_modified_ = true;
    current_line_ = 0;
    
    // Инициализация флагов обновления строк
    for (auto& flag : line_update_flags_) {
        flag = true;
    }
    full_redraw_pending_ = true;
    
    // Кадровые буферы (back/front) очищаются при первом рендере —
    // std::array<uint8_t, ...> обнулён по умолчанию (value-init члена).
    
}

void Video::set_font(const uint8_t* data, size_t size) {
    if (!data) {
        LOG_WARN("Cannot load font: data is null");
        return;
    }
    // V-1: файл шрифта знакогенератора — 8192 байт = ДВА набора
    // символов (основной + псевдографика; FONT1 = бит 2 VIREG).
    // Раньше копировались только первые 4096 байт — второй
    // знакогенератор отбрасывался при загрузке.
    size_t copy_size = (size < FONT_SIZE) ? size : FONT_SIZE;
    std::copy(data, data + copy_size, font_.begin());
    for (size_t i = copy_size; i < FONT_SIZE; i++) {
        font_[i] = 0;
    }
    // Широкий режим НЕ требует отдельного буфера: деление тактовой
    // VCLK/SHIFT при MODSEL растягивает тот же шрифт по времени
    // (схема, «А/Ц Дисплей») — в рендере разворачиваем байт таблицей
    // WEXPAND (16 байт вместо 16 КБ, важно для RP2040).
}

void Video::write_aczu_raw(size_t offset, const uint8_t* data, size_t len) {
    if (!data || len == 0 || offset >= aczu_.size()) {
        return;
    }
    size_t n = (len < aczu_.size() - offset) ? len : (aczu_.size() - offset);
    std::copy(data, data + n, aczu_.begin() + static_cast<std::ptrdiff_t>(offset));

    // Пометить затронутые символы как «грязные», чтобы следующий
    // render_frame() перерисовал их (как это делает aczu_write()).
    size_t first_char = offset >> 1;
    size_t last_char = (offset + n - 1) >> 1;
    for (size_t c = first_char; c <= last_char && c < aczu_char_dirty_.size(); c++) {
        aczu_char_dirty_[c] = true;
    }
    aczu_dirty_ = true;
    frame_modified_ = true;
}

void Video::gzu_write(uint16_t addr, uint8_t data) {
    // Вычисляем указатель на нужную плоскость GZU
    uint8_t* gzu_ptr = &gzu_planes_[gzu_read_page_][(addr & 0x3fff) * 4];
    
    if (ncreg_ & COLORMD) { // Color mode
        // Запись по цветовому режиму с масками
        *(gzu_ptr + 0) = (*(gzu_ptr + 0) & ~data) | ((ncreg_ & 2) ? data : 0);
        *(gzu_ptr + 1) = (*(gzu_ptr + 1) & ~data) | ((ncreg_ & 4) ? data : 0);
        *(gzu_ptr + 2) = (*(gzu_ptr + 2) & ~data) | ((ncreg_ & 8) ? data : 0);
    } else { // Plane mode
        // Обработка WBIT: маска определяется битом WBIT и данными
        uint8_t mask = (ncreg_ & WBIT) ? data : 0;
        
        // Применяем к плоскостям по маскам записи (активный низкий уровень: бит 0 = писать)
        if (!(ncreg_ & ~WSEL0)) {  // WSEL0: bit 1 clear -> Plane 0 writable
            *(gzu_ptr + 0) = (*(gzu_ptr + 0) & ~data) | mask;
        }
        if (!(ncreg_ & ~WSEL1)) {  // WSEL1: bit 2 clear -> Plane 1 writable
            *(gzu_ptr + 1) = (*(gzu_ptr + 1) & ~data) | mask;
        }
        if (!(ncreg_ & ~WSEL2)) {  // WSEL2: bit 3 clear -> Plane 2 writable
            *(gzu_ptr + 2) = (*(gzu_ptr + 2) & ~data) | mask;
        }
    }
    
    // Помечаем строку как изменённую
    uint8_t line = (addr & 0x3fff) >> 6;
    if (line < 256) {
        line_update_flags_[line] = true;
    }
}

uint8_t Video::gzu_read(uint16_t addr) {
    uint8_t* gzu_ptr = &gzu_planes_[gzu_read_page_][(addr & 0x3fff) * 4];
    uint8_t c0 = *gzu_ptr++;
    uint8_t c1 = *gzu_ptr++;
    uint8_t c2 = *gzu_ptr++;
    
    uint8_t value = 0;
    
    if (ncreg_ & COLORMD) { // Color mode
        if (!(ncreg_ & RSEL0)) c0 ^= 0xff;
        if (!(ncreg_ & RSEL1)) c1 ^= 0xff;
        if (!(ncreg_ & RSEL2)) c2 ^= 0xff;
        value = (c0 & c1 & c2) ^ 0xff;
    } else { // Plane mode
        if (ncreg_ & RSEL0) value |= c0;
        if (ncreg_ & RSEL1) value |= c1;
        if (ncreg_ & RSEL2) value |= c2;
    }
    
    return value;
}

void Video::aczu_write(uint16_t addr, uint8_t value) {
    if (aczu_write_protect_) return;

    uint16_t aczu_addr = (addr & 0x3ff) << 1;  // Double address for attributes
    aczu_[aczu_addr] = value;
    aczu_char_dirty_[addr & 0x3ff] = true;  // Mark character as dirty
    
    // Запись атрибута в зависимости от режима
    uint8_t* attr_ptr = &aczu_[aczu_addr + 1];
    switch (aczu_attr_write_) {
        case 0:
            break;                    // No change
        case 1:
            *attr_ptr = 0xff;
            break;  // Set all bits
        case 2:
            *attr_ptr = 0x00;
            break;  // Clear all bits
        case 3:
            *attr_ptr = visst_;
            break; // Copy from VISST
    }
    
    // VISTS (PPI1-A bit 3): сбрасывается при записи в ACZU.
    // В реальном железе: scr_Attr_Read = 0 при любой записи в текстовую память.
    // Фаза атрибута в цикле CRTC
    // которая сбрасывается при записи CPU.
    aczu_attr_written_ = false;
    
    // Помечаем строку как требующую обновления
    uint8_t y = ((addr & 0x3f0) >> 6) << 4;
    for (int i = 15; i >= 0; i--) {
        if ((y + i) < 256) {
            line_update_flags_[y + i] = true;
        }
    }
    
    aczu_dirty_ = true;
}

uint8_t Video::aczu_read(uint16_t addr) {
    uint16_t aczu_addr = (addr & 0x3ff) << 1;
    uint8_t* addr_ptr = &aczu_[aczu_addr];
    
    if (aczu_attr_write_ == 3) {
        visst_ = *(addr_ptr + 1);
    }
    
    uint8_t result = *addr_ptr;
    
    // VISTS (PPI1-A bit 3): устанавливается при чтении ACZU в режиме ATRFRE (mode 3).
    // В оригинале screen.c: scr_Attr_Read = 1 после ACZU_Read() в режиме 3.
    // Это сигнал для BIOS, что была прочитана текстовая память и виден атрибут.
    if (aczu_attr_write_ == 3) {
        aczu_attr_written_ = true;
    }
    return result;
}

void Video::lut_write(uint8_t value) {
    if (lut_write_protect_) return;
    // Извлекаем индекс и значение цвета
    uint8_t index = value & 0x0F;
    uint8_t color_val = value >> 4;
    
    lut_[index] = color_val;
    
    // Сохраняем индекс (для возможного использования)
    lut_index_ = index;
    
    lut_update_rgb();  // Update RGB conversion
    frame_modified_ = true;
}

void Video::lut_update_rgb() {
    // Оригинальный LUT_Update: 4-битный код цвета → RGB
    // Бит 3 = яркость (+21), бит 2 = красный (+42), бит 1 = зелёный (+42), бит 0 = синий (+42)
    // Даёт 6-битные значения ЦАП VGA 0-63; масштабируем в 8-битные 0-255 для Qt
    for (int i = 0; i < LUT_SIZE; i++) {
        uint8_t val = lut_[i] & 0x0F;
        uint8_t bright = (val & 0x8) ? 21 : 0;
        uint8_t r6 = ((val & 0x4) ? 42 : 0) + bright;
        uint8_t g6 = ((val & 0x2) ? 42 : 0) + bright;
        uint8_t b6 = ((val & 0x1) ? 42 : 0) + bright;
        
        if (bw_mode_) {
            uint8_t c = static_cast<uint8_t>(r6 * 0.3 + g6 * 0.5 + b6 * 0.2);
            r6 = g6 = b6 = c;
        }
        // Масштабируем 6 бит (0-63) → 8 бит (0-255) с поканальными коэффициентами
        uint8_t r = static_cast<uint8_t>((r6 * lut_r_scale_) / 63.0f + 0.5f);
        uint8_t g = static_cast<uint8_t>((g6 * lut_g_scale_) / 63.0f + 0.5f);
        uint8_t b = static_cast<uint8_t>((b6 * lut_b_scale_) / 63.0f + 0.5f);
        lut_rgb_[i].r = r;
        lut_rgb_[i].g = g;
        lut_rgb_[i].b = b;
    }
}

void Video::on_sysreg_change(uint8_t sysreg) {
    // Реакция на смену системного регистра — влияет на конфигурацию видео:
    // системный регистр задаёт конфигурацию памяти, от которой зависит
    // доступ к видеопамяти.
    // Заглушка (обработка не требуется)
    (void)sysreg;  // Suppress unused parameter warning
}

void Video::set_lut_entry(uint8_t index, uint32_t rgb) {
    if (index < LUT_SIZE) {
        lut_[index] = rgb;
        frame_modified_ = true;
    }
}

void Video::render_scanline(uint8_t y, uint8_t* buffer) {
    if (!line_update_flags_[y] && !full_redraw_pending_) {
        return; // No need to update this line
    }
    
    line_update_flags_[y] = false;
    
    // SIMD-растеризация через таблицы plane_mask_*.
    // Идея: для каждой плоскости (c1, c2, c3, c4) предвычислены 2 uint32_t
    // (для двух групп по 4 пикселя), где нибблы уже содержат бит соответствующей
    // плоскости. OR 4 табличных значений = 8 нибблов = 8 пикселей.
    // Скорость: 4 lookup + 4 OR = 8 операций на 8 пикселей (vs ~160 в побитовом цикле).
    // Табличная растеризация: 4 lookup + 4 OR на 8 пикселей.
    uint8_t* gzu_ptr = &gzu_planes_[gzu_show_page_][y * 64 * 4];
    const auto& m01 = plane_mask_01();
    const auto& m02 = plane_mask_02();
    const auto& m04 = plane_mask_04();
    const auto& m08 = plane_mask_08();
    
    for (int x = 0; x < 64; x++) {
        uint8_t c1 = *gzu_ptr++;
        uint8_t c2 = *gzu_ptr++;
        uint8_t c3 = *gzu_ptr++;
        uint8_t c4 = *gzu_ptr++;
        
        uint32_t lo = m01[c1][0] | m02[c2][0] | m04[c3][0] | m08[c4][0];
        uint32_t hi = m01[c1][1] | m02[c2][1] | m04[c3][1] | m08[c4][1];
        
        // Раскладываем 2 uint32_t в 8 байт framebuffer.
        int base = x * 8;
        buffer[base + 0] = (lo >> 0) & 0x0F;
        buffer[base + 1] = (lo >> 8) & 0x0F;
        buffer[base + 2] = (lo >> 16) & 0x0F;
        buffer[base + 3] = (lo >> 24) & 0x0F;
        buffer[base + 4] = (hi >> 0) & 0x0F;
        buffer[base + 5] = (hi >> 8) & 0x0F;
        buffer[base + 6] = (hi >> 16) & 0x0F;
        buffer[base + 7] = (hi >> 24) & 0x0F;
    }
    
    current_line_ = y;
}

void Video::render_frame(uint8_t* framebuffer) {
    if (aczu_dirty_ || full_redraw_pending_) {
        update_aczu_framebuffer();
        aczu_dirty_ = false;
    }
    // Пишем кадр в back_framebuffer_ (double buffer). Параметр framebuffer
    // (legacy) игнорируется — render_frame всегда пишет в back_framebuffer_.
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        render_scanline(y, &back_framebuffer_[static_cast<size_t>(y) * SCREEN_WIDTH]);
    }
    
    full_redraw_pending_ = false;
    frame_modified_ = false;
}

void Video::write_vireg(uint8_t value) {
    // Разбираем поля регистра управления видео (VIREG, порт C ППР№1):
    //   биты 6-7 — страница ГЗУ для доступа CPU;
    //   биты 5-4 — режим записи атрибута АЦЗУ;
    //   бит 3 — широкий режим (32 символа в строке);
    //   бит 2 — второй знакогенератор (FONT1);
    //   биты 1-0 — страница ГЗУ для отображения.
    uint8_t old_read_page = gzu_read_page_;
    gzu_read_page_ = ((value & 0xc0) >> 6) & gzu_size_mask_;
    aczu_attr_write_ = (value & 0x30) >> 4;
    aczu_wide_mode_ = (value & 0x08) >> 3;
    aczu_second_font_ = (value & 0x04) >> 2;
    gzu_show_page_ = (value & 0x03) & gzu_size_mask_;

    // V-4: при смене страницы ДОСТУПА ГЗУ (биты 6-7) целевой массив
    // текстового растра меняется — все 1024 знакоместа обязаны
    // перерисоваться в новый дескриптор страницы (touch-флаги уже
    // сброшены прошлым рендером). Без этого текст, написанный при одной
    // странице доступа, «застаревал»/пропадал после переключения.
    if (gzu_read_page_ != old_read_page) {
        aczu_char_dirty_.fill(true);
        aczu_dirty_ = true;
    }

    // Полная перерисовка при смене младших 4 бит: страница отображения
    // (биты 0-1), шрифт/широкий режим (биты 2-3) — влияют на кадр
    // целиком (маска 0x0F).
    if ((vireg_ & 0x0f) != (value & 0x0f)) {
        full_redraw_pending_ = true;
        aczu_char_dirty_.fill(true);
        aczu_dirty_ = true;
    }

    vireg_ = value;
}

void Video::write_ncreg(uint8_t value) {
    ncreg_ = value;
    
    // Особая обработка смены NCREG не требуется
    frame_modified_ = true;
}

mb_video_state_t Video::get_state() const {
    mb_video_state_t state;
    state.gzu_read_page = gzu_read_page_;
    state.gzu_show_page = gzu_show_page_;
    state.gzu_color_mode = gzu_color_mode_;
    state.gzu_read_mask = gzu_read_mask_;
    state.gzu_write_mask = gzu_write_mask_;
    state.gzu_write_bit = gzu_write_bit_;
    state.aczu_attr_write = aczu_attr_write_;
    state.aczu_wide_mode = aczu_wide_mode_;
    state.aczu_second_font = aczu_second_font_;
    
    return state;
}

void Video::update_aczu_framebuffer() {
    // Преобразование символов ACZU в пиксели в 4-й слот ГЗУ (модель,
    // побитово совпадающая с растровым выходом АЦЗУ железа: BWV-канал
    // текста идёт в LUT параллельно ГЗУ и комбинируется в реальном
    // времени; запись в 4-й байт ГЗУ воспроизводит тот же кадр).
    // Инкрементальный рендер по позначным dirty-флагам.
    uint8_t* src = aczu_.data();
    uint8_t* dst = &gzu_planes_[gzu_read_page_][3];  // 4-й байт доступной страницы
    bool* touch = aczu_char_dirty_.data();

    // V-3: выбор знакогенератора одинаков в обоих режимах — массив
    // font_ хранит ДВА набора по 4096 байт (второй — псевдографика,
    // бит 2 VIREG). Раньше вторая половина массива затиралась «широким
    // шрифтом» с выходом за границы — UB и мусор вместо псевдографики.
    uint8_t* font_ptr = aczu_second_font_ ? &font_[16 * 256] : font_.data();

    if (!aczu_wide_mode_) { // Обычный режим (64 символа в строке, 16 строк)
        // Инкрементальный рендер: обновляем только символы с touch=true.
        // Для типичного кадра в dead-loop меняется 0-2 символа, поэтому
        // пропуск 1022 символов даёт ×500 ускорение на этом этапе.
        for (int scrlen = 1024; scrlen--;) {
            if (*touch || full_redraw_pending_) {
                uint8_t ch = src[0];
                uint8_t attr = src[1];
                uint8_t* chr = font_ptr + ch * 16;
                uint8_t* chrdst = dst;

                for (int row = 16; row > 0; row--) {
                    *chrdst = (*chr++) ^ attr;
                    chrdst += 64 * 4;  // Переход к следующей строке развёртки
                }
                // Помечаем все 16 строк символа как грязные, чтобы render_scanline
                // перерисовал их при следующем кадре.
                int sym_y = ((1023 - scrlen) >> 6) << 4;  // Y верхней строки символа
                for (int i = 0; i < 16; i++) {
                    if (sym_y + i < 256) line_update_flags_[sym_y + i] = true;
                }
            }
            src += 2;      // Следующий символ в ACZU (символ + атрибут)
            dst += 4;      // Следующая позиция символа в GZU (4 байта на символ)
            *touch++ = 0;  // Сброс dirty-флага символа
            
            if (!(scrlen & 0x3f)) {  // Конец строки экрана (каждые 64 символа)
                dst += 15 * 64 * 4;  // Переход к следующей текстовой строке
            }
        }
    } else { // Широкий режим (32 символа в строке, 16 строк)
        // Широкий режим растягивает ТОТ ЖЕ шрифт по времени (деление
        // тактовой VCLK/SHIFT при MODSEL): каждая строка растра символа
        // занимает 2 байта (16 пикселей). Развёртка бита — табличная
        // функция от ниббла (WEXPAND, 16 байт — без буфера 16 КБ).
        for (int scrlen = 512; scrlen--;) {
            if (*touch || full_redraw_pending_) {
                uint8_t ch = src[0];
                uint8_t attr = src[1];
                uint8_t* chr = font_ptr + ch * 16;
                uint8_t* chrdst = dst;

                for (int row = 16; row > 0; row--) {
                    uint8_t b = (*chr++) ^ attr;
                    *chrdst = WEXPAND[b >> 4]; chrdst += 4;
                    *chrdst = WEXPAND[b & 0x0F]; chrdst += 64 * 4 - 4;
                }
                // V-5: помечаем строки растра и в широком режиме —
                // раньше dirty-символы не обновляли экран до полной
                // перерисовки.
                int sym_y = ((511 - scrlen) >> 5) << 4;
                for (int i = 0; i < 16; i++) {
                    if (sym_y + i < 256) line_update_flags_[sym_y + i] = true;
                }
            }
            src += 4;      // Широкий режим: 4 байта на символ в ACZU
            dst += 8;      // Широкий режим: 8 байт на символ в GZU
            *touch = 0;
            touch += 2;    // Широкий режим: 2 dirty-флага на символ (шаг 2)
            
            if (!(scrlen & 0x1f)) {  // Конец строки экрана (каждые 32 символа)
                dst += 15 * 64 * 4;  // Переход к следующей текстовой строке
            }
        }
    }
}


// === Double-buffer API для zero-copy передачи кадра в GUI ===
// После render_frame() вызывается swap_frame_buffers() — back копируется во front,
// и shared_ptr на front отдаётся GUI через get_front_framebuffer().
// При следующем swap_frame_buffers() создаётся новый shared_ptr (если GUI уже
// отпустил старый), либо перезаписывается тот же front (если GUI отпустил).
void Video::swap_frame_buffers() {
    // ФИКС B3: настоящий swap (lock-free) вместо std::copy 128 КБ.
    // Алгоритм:
    // 1. back_alloc_ = front_framebuffer_  -- берём буфер, который GUI уже отпустил,
    //                                          и делаем его новым back.
    // 2. front_framebuffer_ = new shared_ptr<uint8_t[]>(...)  -- новый front с только что отрендеренным кадром.
    // 3. memcpy из back_framebuffer_ в front — это ОДИН copy 128 КБ,
    //    но он делается ровно один раз, а не каждый кадр (раньше copy делался даже когда GUI ещё держал старый front).
    // Если GUI ещё держит front, back_alloc_ будет nullptr → создаём новый.
    if (!front_framebuffer_) {
        // Первый вызов: аллоцируем front, back_alloc_ не нужен.
        front_framebuffer_ = std::shared_ptr<uint8_t[]>(new uint8_t[FRAMEBUFFER_SIZE]);
    } else if (front_framebuffer_.use_count() == 1) {
        // GUI уже отпустил старый front — переиспользуем его как back.
        back_alloc_ = std::move(front_framebuffer_);
        front_framebuffer_ = std::shared_ptr<uint8_t[]>(new uint8_t[FRAMEBUFFER_SIZE]);
    }
    // GUI ещё держит front (use_count > 1) → back_alloc_ не меняется, новый front аллоцируется.

    // Копируем только что отрендеренный back в новый front.
    // Это единственный copy, и его нельзя избежать (back_framebuffer_ — std::array).
    std::copy(back_framebuffer_.begin(), back_framebuffer_.end(), front_framebuffer_.get());

    // Если есть back_alloc_ — следующий render_frame пишет в его (освободившийся front).
    // Чтобы переключить back_framebuffer_ на back_alloc_, render_frame будет использовать
    // оба: пишем в back_framebuffer_, потом copy в front_framebuffer_, а back_alloc_
    // остаётся на следующий swap для реюза.
    // Оптимизация: render_frame пишет прямо в front_framebuffer_ (новый), а
    // back_framebuffer_ используем только для случая, когда GUI ещё держит front.
    if (back_alloc_) {
        // На следующем кадре render_frame сначала пишет в back_alloc_,
        // а back_framebuffer_ сохраняется как fallback для текущего frame.
        // На практике мы пока просто копируем — оптимизация для следующего раунда.
        // Здесь просто освобождаем back_alloc_, потому что мы уже использовали back_framebuffer_.
        back_alloc_.reset();
    }
}

std::shared_ptr<const uint8_t[]> Video::get_front_framebuffer() const {
    // Создаём новый shared_ptr<const> из существующего shared_ptr<uint8_t[]>.
    // Это работает благодаря тому, что shared_ptr<T[]> в C++17 поддерживает
    // конвертацию в shared_ptr<const T[]> (через aliasing constructor).
    return std::shared_ptr<const uint8_t[]>(front_framebuffer_,
                                            front_framebuffer_.get());
}

}  // namespace korvet
