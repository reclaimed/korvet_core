#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_periph_prog.py — ассемблер гостевой тест-программы 8080 для проверки
ИРПС/мыши/джойстика/LAN наkorvet_core. Генерирует periph_prog.hpp.

Программа выполняется НАД реальной шиной эмулируемой машины (LDA/STA по
окнам 0FBxx), т.е. проходит весь путь: CPU -> шина -> memory.cpp ->
периферия -> мультиплексор PPI3/ЭУ8040. Результаты — в RAM по адресу
RES (0x0200), читает хост через readout_ram().
"""

# --- мини-ассемблер 8080 (двухпроходный, только нужные команды) ---
OPCODES = {
    'MVI A': (0x3E, 1), 'STA': (0x32, 2), 'LDA': (0x3A, 2),
    'LXI B': (0x01, 2), 'ANI': (0xE6, 1), 'JNZ': (0xC2, 2),
    'JMP': (0xC3, 2), 'DCX B': (0x0B, 0), 'MOV A,B': (0x78, 0),
    'ORA C': (0xB1, 0), 'DI': (0xF3, 0), 'NOP': (0x00, 0),
}

def pyint(s):
    """'4EH' -> 0x4E, '0FFFFH' -> 0xFFFF, 'RES+0' -> выражение Python."""
    s = s.strip()
    out = []
    for tok in s.replace('+', ' + ').split():
        if tok.endswith('H'):
            try:
                tok = str(int(tok[:-1], 16))
            except ValueError:
                pass
        out.append(tok)
    return ' '.join(out)

def assemble(asm, org=0x0100):
    lines = []
    for raw in asm.strip().splitlines():
        line = raw.split(';')[0].strip()
        if not line:
            continue
        if line.upper().startswith('ORG'):
            org = int(line.split()[1].rstrip('Hh'), 16)
            continue
        if ':' in line and not line.startswith(('MVI', 'LDA', 'STA', 'LXI',
                                                'ANI', 'JNZ', 'JMP', 'DCX',
                                                'MOV', 'ORA', 'DI', 'NOP')):
            label, rest = line.split(':', 1)
            lines.append((label.strip(), rest.strip()))
        else:
            lines.append((None, line))
    # проход 1: адреса (с учётом ORG)
    addr = org
    labels = {}
    parsed = []
    for label, stmt in lines:
        if label is not None:
            labels[label] = addr
        if not stmt:
            parsed.append((addr, None, None))
            continue
        op = None
        for cand, (opc, sz) in OPCODES.items():
            if stmt.startswith(cand):
                op = (cand, opc, sz)
                break
        assert op is not None, f"неизвестная команда: {stmt!r}"
        args = stmt[len(op[0]):].strip().lstrip(',').strip()
        parsed.append((addr, op, args))
        addr += 1 + op[2]
    # проход 2: кодирование
    out = bytearray()
    for a, op, args in parsed:
        if op is None:
            continue
        cand, opc, sz = op
        out.append(opc)
        if sz == 0:
            assert args == '', f"лишний операнд: {stmt!r}"
            continue
        parts = [p.strip() for p in args.split(',')] if args else []
        try:
            if sz == 1:
                val = eval(pyint(parts[0]), {}, dict(labels, **SYMS))  # noqa: S307
                out.append(val & 0xFF)
            else:
                val = eval(pyint(args), {}, dict(labels, **SYMS))      # noqa: S307
                out.append(val & 0xFF)
                out.append((val >> 8) & 0xFF)
        except Exception as e:
            raise SystemExit(f"ошибка кодирования {stmt!r} (args={args!r}, "
                             f"pyint={pyint(args)!r}): {e}")
    return bytes(out), labels

# --- символы (адреса регистров Корвета и результаты) ---
SYMS = {
    'SERD':  0xFB10, 'SERC': 0xFB11,          # ИРПС: данные / управление-статус
    'LAND':  0xFB20, 'LANC': 0xFB21,          # ЛВС:  данные / управление-статус
    'PPI1A': 0xFB38,                          # PPI1-A: LANADR (биты 4-7)
    'PPI3B': 0xFB09, 'PPI3C': 0xFB0A, 'PPI3CTL': 0xFB0B,  # PPI3: ЭУ8040
    'RES':   0x0200,                          # область результатов в TPA
}

ASM = r"""
        ORG 0100H                  ; TPA base — программа грузится сюда
        DI                      ; прерывания гостевой ОС не мешают опросу
        ; === ИРПС: детерминированная инициализация 8251A ===
        ; «4E, 37, 37» сходится в (режим=4E, команда=37) из ЛЮБОГО состояния
        ; автомата слово-режима/команды (см. periph_guest_test.cpp)
        MVI A,4EH              ; слово режима: 16x, 8 бит, 1 стоповый
        STA SERC
        MVI A,37H              ; TXEN|RXEN|CLERR|DTR|RTS
        STA SERC
        MVI A,37H
        STA SERC
        ; === приём 3-байтовой посылки мыши (опрос RRDY) ===
        LXI B,0FFFFH           ; таймаут ~71 кадр
poll1:  LDA SERC               ; SERS
        ANI 02H                ; маска RRDY
        JNZ ser_read
        DCX B
        MOV A,B
        ORA C
        JNZ poll1
        MVI A,0EEH             ; таймаут — маркер ошибки
        STA RES+0
        STA RES+1
        STA RES+2
        STA RES+3
        JMP serial_tx
ser_read:
        LDA SERD               ; байт 1 посылки
        STA RES+0
        LDA SERD               ; байт 2
        STA RES+1
        LDA SERD               ; байт 3
        STA RES+2
        LDA SERC               ; статус после опустошения очереди
        STA RES+3
serial_tx:
        MVI A,0A5H             ; передача наружу (SERINT2/TXEN)
        STA SERD
        MVI A,5AH
        STA SERD
        ; === джойстик МД8030: слот ХР2 мультиплексора ЭУ8040 ===
        MVI A,82H              ; PPI3: порт B — ВВОД, режим 0
        STA PPI3CTL
        MVI A,40H              ; «Разрешение»=0, слот ХР2 (джойстик)
        STA PPI3C
        LDA PPI3B
        STA RES+4              ; биты направлений/кнопок (активный низкий)
        MVI A,0C0H             ; «Разрешение»=1 — штекер «вынут»
        STA PPI3C
        LDA PPI3B
        STA RES+5              ; ожидание 0xFF
        ; === параллельная мышь ММ8031: слот ХР1 ===
        MVI A,00H              ; слот ХР1, «Синхр»=0 -> СЧХ
        STA PPI3C
        LDA PPI3B
        STA RES+6
        MVI A,10H              ; «Синхр»=1 -> СЧУ
        STA PPI3C
        LDA PPI3B
        STA RES+7
        ; импульс «инициализация» (фронт бита 3): счётчики в 1000b
        MVI A,00H
        STA PPI3C
        MVI A,08H
        STA PPI3C
        MVI A,00H
        STA PPI3C
        LDA PPI3B
        STA RES+8              ; после сброса: 8|кнопки
        ; === ЛВС ===
        LDA PPI1A
        STA RES+9              ; LANADR в битах 4-7
        MVI A,5EH              ; LANMD (16x, 8 бит, контроль, 1 стоп)
        STA LANC
        MVI A,37H
        STA LANC
        MVI A,37H
        STA LANC               ; детерминированная инициализация (см. выше)
        LDA LANC               ; LANS — статус после инициализации
        STA RES+10
        MVI A,77H              ; передача двух байтов в канал
        STA LAND
        MVI A,99H
        STA LAND
        ; === приём байта из канала (опрос RRDY) ===
        LXI B,0FFFFH
lan_poll:
        LDA LANC
        ANI 02H
        JNZ lan_read
        DCX B
        MOV A,B
        ORA C
        JNZ lan_poll
        MVI A,0EEH
        STA RES+11
        JMP fin
lan_read:
        LDA LAND
        STA RES+11
fin:
        MVI A,0C3H             ; маркер завершения
        STA RES+15
fin2:   JMP fin2
"""

def main():
    code, labels = assemble(ASM)
    assert len(code) < 0x100, "программа не влезает в 0x0100..0x01FF"
    lines = []
    for i in range(0, len(code), 12):
        chunk = ', '.join(f'0x{b:02X}' for b in code[i:i+12])
        lines.append(f'    {chunk},')
    body = '\n'.join(lines).rstrip(',') + '\n'
    hpp = f"""// ---------------------------------------------------------------------------
// periph_prog.hpp — гостевая тест-программа 8080 (сгенерирована
// scripts/gen_periph_prog.py — НЕ править руками, править генератор).
//
// Проверяет подсистемы ввода-вывода ИЗНУТРИ эмулируемой машины:
//   RES+0..3  — приём 3-байтовой посылки мыши по ИРПС (SERD/SERS)
//   RES+4..5  — чтение джойстика МД8030 через PPI3/ЭУ8040 (слот ХР2)
//   RES+6..8  — счётчики мыши ММ8031 через PPI3 (слот ХР1, бит «Синхр»)
//   RES+9     — LANADR из PPI1-A (биты 4-7)
//   RES+10    — статус LANS после инициализации 8251A ЛВС
//   RES+11    — принятый байт ЛВС (опрос RRDY)
//   RES+15    — маркер завершения 0xC3
// Программа грузится в TPA по адресу 0x0100 (под CP/M), результаты
// кладёт в 0x0200..0x020F. Прерывания запрещены (DI) — чистый опрос.
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <cstddef>

namespace korvet::minigui {{

static const uint8_t PERIPH_PROG[] = {{
{body}}};
static const size_t PERIPH_PROG_SIZE = sizeof(PERIPH_PROG);
static const uint16_t PERIPH_PROG_ADDR = 0x0100;   // TPA base
static const uint16_t PERIPH_RES_ADDR = 0x0200;    // RES (см. генератор)

}}  // namespace korvet::minigui
"""
    out_path = '/home/z/my-project/korvet_core/korvet_core/minigui/periph_prog.hpp'
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(hpp)
    print(f"OK: {out_path} — {len(code)} байт, метки: {labels}")

if __name__ == '__main__':
    main()
