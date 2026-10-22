# SnailPulse — defence study guide (Orbita 2026)

How to use this:
1. Read **§1–2** until you can say them without notes.
2. Go block by block in **§3** with the schematic open.
3. Rehearse **§6** out loud in Russian.

All the formulas are in [`SnailPulse_Math.md`](SnailPulse_Math.md). Known weaknesses and fixes are in [`Schematic_Review.md`](Schematic_Review.md).

---

## 1. The mission in 60 seconds

**EN.** We fly a living garden snail (*Cornu aspersum*) to ~24 km and **measure its heartbeat during the flight**, without touching or hurting it. An infrared sensor glued to the shell above the heart sees the heart move. An analog band-pass amplifier extracts that tiny signal, and the satellite's flight controller counts the beats and radios them to the ground every 2 seconds. A heater keeps the box at ~20 °C, with a hardware cutoff that works even if the software crashes. The bigger goal (Mission 1) is snail mucus as a regenerative biomaterial for space medicine. SnailPulse proves the animal stayed alive and shows how stressed it was.

**RU.** Мы отправляем живую садовую улитку (*Cornu aspersum*) на высоту около 24 км и **измеряем её сердцебиение во время полёта**, не травмируя её. ИК-датчик, приклеенный к раковине над сердцем, «видит» движение сердца. Аналоговый полосовой усилитель выделяет этот очень слабый сигнал, а полётный контроллер считает удары и каждые 2 секунды передаёт данные на Землю по радио. Нагреватель держит в боксе около 20 °C, а аппаратная защита от перегрева работает, даже если программа зависнет. Общая цель миссии — слизь улитки как регенеративный биоматериал для космической медицины. SnailPulse доказывает, что животное выжило, и показывает уровень его стресса.

---

## 2. System at a glance

```
 snail shell                 SnailPulse board (perfboard 76x76)                  IntroSat platform
┌───────────┐  J1 cable   ┌──────────────────────────────────────────┐ J6  ┌──────────────────────────┐
│ TCRT5000  │────────────▶│ A: RV3 → HPF 0.16 Hz → ×101, LPF 3.4 Hz ──┼────▶│ PB0 ADC   STM32H750 FCM │
│ (IR LED + │             │ B: 1.65 V reference (divider + buffer)    │     │ PB1 ADC   - beat detect │
│ phototr.) │             │ C: NTC1 divider ──────────────────────────┼────▶│ PC6 PWM   - PI heater   │
└───────────┘  J4 cable   │    NTC2 → LM393 (40 °C) ─┐                │◀────┤ I2C4      - packets     │
 NTC1, NTC2 on heater ───▶│    PWM → R11 → GATE ◀────┘→ IRLZ44N ──────┼──┐  │ UART1 → comm board LoRa │
 heater 3×150 Ω ◀─J3──────│ D: J7 SHT21, J8 BMP280, J9 MAX30100 (I2C4)│  │  └──────────────────────────┘
                          └──────────────────────────────────────────┘  └── J2 XT30 14.8 V from power module X8
```

| Signal | FC pin | Function on IntroBus_L |
|---|---|---|
| HB_ADC | PB0 | ADC |
| NTC1_ADC | PB1 | ADC |
| PWM | PC6 | timer (HRTIM_CHA1 / TIM3) |
| SCL / SDA | PD12 / PD13 | I2C4 (the payload bus; I2C1 is the service bus, so we must not use it) |
| 5 V / 3V3 | +5V_IS / +3V3_IS | payload power, switched on with jumpers on X6 |
| 14.8 V | X8 XT30 | unregulated battery, 6 A max |

---

## 3. Every part, what it does and why this value

### Block A — heartbeat sensor and amplifier

| Part | Value | What it does | Why this part / value | If asked "what if…" |
|---|---|---|---|---|
| **U1 TCRT5000** | — | Reflective IR sensor: 950 nm LED plus a phototransistor in one package, with a daylight filter | Cheap and local (electron.uz). Reflective IR through the shell is the established method for invertebrate hearts (CNY70 in Burnett et al. 2013; same principle) | Sensor slips → signal disappears. Firmware flags NO-SIGNAL and quality 0. We still see it in the waveform |
| R1 | 220 Ω | LED current limit | (5 − 1.25)/220 = 17 mA, 30 % of max: enough light, little heat | More current = stronger signal but more heating near the snail |
| RV3 | 50 k trim | Phototransistor load: converts light current into voltage | The shell's reflectivity is unknown in advance, so it is tuned on the real snail to ~1 V DC | Too big → saturates at ~2.9 V; too small → weak signal |
| C11 | 1 µF ceramic | High-pass with R15, blocks the DC level | 0.16 Hz corner = 9.5 bpm. Ceramic (non-polar) because DC across it can be either sign | Bigger C passes slower hearts but settles more slowly (τ = RC) |
| R15 | 1 MΩ | High-pass resistor to VREF; re-centres the signal at 1.65 V | Large R allows a small C for a low corner. Fine with a CMOS input (1 pA bias) | — |
| **U2A MCP6002** | — | Non-inverting amplifier | Rail-to-rail input and output, runs on 3.3 V, 1 MHz bandwidth, ~100 µA. A classic LM358 can't swing to the 3.3 V rail | — |
| R16 / R17 | 1 k / 100 k | Gain setting | G = 1 + R17/R16 = 101 (40 dB) | Gain too high → op-amp offset × gain could saturate the output |
| C2 | 470 nF | Low-pass in the feedback | 3.39 Hz corner. Removes noise and 50 Hz pick-up from the 2 m cable | — |
| R3 / C3 | 1 k / 100 nF | ADC input filter and protection | 1.6 kHz. C3 supplies charge to the ADC sampler; R3 limits fault current | — |

### Block B — 1.65 V reference
| Part | Value | Why |
|---|---|---|
| R6 / R7 | 10 k / 10 k | 3.3 V / 2 = 1.65 V, the middle of the ADC range, so the AC signal can swing ± |
| C4 | 10 µF | Filters supply noise from the divider (3.2 Hz corner) |
| U2B | MCP6002 half B as a buffer | Low output impedance: R15 and R16 can't pull VREF around. It uses the second half of a chip we already have, so no extra part |
| C5, C10 | 100 nF + 10 µF | Decoupling. 100 nF for fast spikes right at the chip pins; 10 µF bulk for slower dips on the 3.3 V line |

### Block C — heater switch (14.8 V)
| Part | Value | Why |
|---|---|---|
| R8–R10 | 3 × 150 Ω, 5 W | 50 Ω total → 4.4 W at 14.8 V. Spreads heat, and still works if one resistor opens |
| **Q1 IRLZ44N** | logic-level N-MOSFET | Fully on at a 3 V gate (the "L" = logic level). R_DS(on) ≈ 0.035 Ω → only 3 mW loss. Low-side switch: the source sits at ground, so a 3.3 V MCU can drive it directly |
| R11 | 1 k | Gate resistor: limits current spikes into the gate capacitance, and lets the LM393 override the MCU pin |
| R12 | 10 k | Gate pull-down: the heater is OFF while the MCU boots, resets or is unplugged |
| C6 | 10 µF 50 V | Local reservoir for PWM current pulses on the 14.8 V line |
| NT1 | net tie | Joins heater ground and signal ground at **one point**, so 0.3 A of heater current doesn't flow through the analog ground and add noise to a millivolt-level heartbeat |
| J2 | XT30 | Same connector as the power module's X8 output; rated well above 0.3 A |

### Block C — thermistors and hardware overheat cutoff
| Part | Value | Why |
|---|---|---|
| TH1 + R13 + C8 | NTC 10 k, 10 k, 100 nF | Heater plate temperature → PB1 for firmware control. 10 k + 10 k gives ½ of 3.3 V at 25 °C, the most sensitive point of the divider |
| TH2 + R18 + C7 | same | A **separate** sensor only for the hardware cutoff, so one failed sensor can't break both protections |
| RV2 | 10 k trim | Threshold ≈ 1.14 V = 40 °C on the plate |
| **U5 LM393** | comparator, open-collector | Pulls GATE low when too hot. Works with **no software**. Runs on 5 V because its inputs only work up to V_CC − 1.5 V |
| U5B | unused half | Inputs tied to GND, output not connected. Unused inputs must never float |
| C9 | 100 nF | LM393 decoupling |

### Block D — connectors and modules
| Part | Why |
|---|---|
| J1 JST-XH 4-pin, 2 m 4-core phone cable | Locking connector (vibration at burst and parachute opening). The sensor sits on the snail while the board sits elsewhere |
| J4 JST-XH 4-pin | Two thermistors, each with its own ground return |
| J5 3-pin | 5 V, 3.3 V, GND from the FC |
| J6 5-pin | 2 analog, 1 PWM, I²C pair to the FC |
| J3 2-pin | heater output (+14.8 V and the switched low side — see review fix 1) |
| **J7 SHT21 (GY-21)** | Box temperature ±0.3 °C and humidity ±2 % RH. Humidity matters: snails dry out, and dehydration lowers heart rate |
| **J8 BMP280** | Box pressure, to check the box stays sealed (300–1100 hPa). SDO → GND sets address 0x76 |
| **J9 MAX30100** | Backup optical pulse sensor (I²C, address 0x57). If the TCRT5000 path fails we still have optical data |

### The flight controller side (organizers' platform)
- **STM32H750VBT6**: Cortex-M7 at 480 MHz with 16-bit ADCs, programmed through the Arduino IDE (stm32duino).
- **IntroStratLib**: the organizers' library for the board sensors (LSM6DS3 accel/gyro 0x6A, LIS3MDL magnetometer 0x1C, MS5611 barometer 0x77, LM75A temperature 0x4A).
- **Radio**: UART1 → communication board → **Ebyte E32-433T30D** LoRa, 410–441 MHz, up to 30 dBm (1 W), 2.4 kbit/s air rate. Data goes out in 58-byte packets, and the board answers ACK 0x06 after each block.
- **microSD** over SDMMC2. A full-rate copy of everything we measure is kept there.

---

## 4. Firmware in 5 points (details in `firmware/README.md`)

1. **Timer interrupt at 50 Hz** reads the ADC, so heartbeat timing is exact even while the SD card or I²C is slow.
2. **Beat detector**: smoothing, baseline removal and an adaptive threshold (40 % of the envelope, with hysteresis). The refractory period stops double counting. The rate is the **median of the last 5 intervals**.
3. **Heater PI loop** on box air temperature (SHT21), with a 35 °C limit on the plate (NTC1), sensor-failure fallbacks, and the LM393 at 40 °C as the last line of defence.
4. **Binary packets** with sync bytes, sequence number and CRC-16. Each fits in one 58-byte LoRa packet. Link use is 18 % of 2.4 kbit/s. The waveform is sent too, so the ground can recount beats independently.
5. **Watchdog 4 s, SD logging, status packet** with error counters. We found and worked around a bug in the organizers' MS5611 code: below 20 °C its compensation is wrong.

---

## 5. Honest weaknesses (say these before the jury finds them)

- *Rev A schematic had 3 wiring errors* (heater connector, sensor cable, NTC2 label). We caught them in review and fixed them before assembly (see `Schematic_Review.md`).
- *The comparator has no hysteresis*, so near 40 °C it can chatter. That's harmless at 0.3 A, and it acts like a proportional limiter.
- *An open TH2 wire disables the hardware cutoff.* The software limit on NTC1 still works. Next revision: use the spare comparator half as an open-wire detector.
- *Op-amp offset × 101* shifts the resting output by up to ±0.45 V. We handle it in firmware (baseline tracking).
- *The heater power is fixed by the battery voltage* (3.3–5.6 W over the discharge curve). Insulation has to be designed for that.
- *One IR sensor = one point of failure*, so the MAX30100 is the backup.
- *Next revision:* a payload MCU on our own board (modularity, short analog wires, independent heater safety). We chose not to for this flight: fewer parts and less risk.

---

## 6. Likely jury questions — answers in Russian

### Биология / идея
1. **Почему улитка?** — Слизь *Cornu aspersum* изучается как регенеративный биоматериал (заживление ран, костная и хрящевая ткань). Улитка выносит холод и обезвоживание, впадая в спячку, мала и не требует сложного жизнеобеспечения. А частота сердечных сокращений — простой неинвазивный показатель стресса и того, что животное живо.
2. **Как вы видите сердце через раковину?** — ИК-светодиод светит через тонкую раковину; при каждом сокращении сердца меняется количество отражённого/поглощённого ИК-излучения. Фототранзистор переводит изменение света в изменение тока, а резистор RV3 — в напряжение. Этот метод (CAPMON, датчик CNY70) давно используется для моллюсков и ракообразных.
3. **Какая частота сердцебиения у улитки?** — Порядка десятков ударов в минуту. Сильно зависит от температуры и активности: у молодых улиток в тепле около 100, в холоде, спячке и при обезвоживании гораздо ниже. Поэтому полоса фильтра 0,16–3,4 Гц (≈10–200 уд/мин), а программа принимает 3–150 уд/мин.
4. **Не повредит ли улитке ИК-свет или нагрев?** — Ток светодиода 17 мА, мощность около 20 мВт, длина волны 950 нм (не ультрафиолет). Нагреватель ограничен программно (35 °C на пластине) и аппаратно (40 °C), а в боксе поддерживаем 20 °C.
5. **Какой у вас контроль?** — Наземная улитка в таком же боксе с той же записью пульса: сравниваем пульс до полёта, во время и после.

### Аналоговая часть
6. **Зачем фильтр высоких частот?** — Постоянная составляющая от датчика (≈1 В) в сотни раз больше полезного сигнала. Если её усилить в 101 раз, ОУ уйдёт в насыщение. C11 и R15 убирают постоянную составляющую: f = 1/(2πRC) = 0,16 Гц.
7. **Почему R15 подключён к VREF, а не к земле?** — Чтобы сигнал был отцентрован на 1,65 В, в середине диапазона АЦП 0–3,3 В, и мог колебаться в обе стороны при однополярном питании.
8. **Как считается коэффициент усиления?** — Неинвертирующий усилитель: K = 1 + R17/R16 = 1 + 100k/1k = 101, это 40 дБ.
9. **Зачем C2 параллельно R17?** — Это ФНЧ: f = 1/(2π·100k·470n) = 3,4 Гц. Убирает шум и наводку 50 Гц от двухметрового кабеля. На высоких частотах усиление стремится к 1, поэтому дальше стоит RC-фильтр R3/C3 перед АЦП.
10. **Почему MCP6002, а не LM358?** — MCP6002 — rail-to-rail по входу и выходу, работает от 3,3 В, входной ток порядка пикоампер (подходит для резистора 1 МОм). LM358 при питании 3,3 В не может выдать напряжение до верхней шины.
11. **Зачем буфер на VREF?** — Делитель 10k/10k имеет выходное сопротивление 5 кОм. Если нагрузить его резисторами R15/R16, опорное напряжение будет «плавать». Повторитель на второй половине ОУ даёт низкое выходное сопротивление.
12. **Какая чувствительность системы?** — АЦП 12 бит: 3,3 В / 4096 = 0,8 мВ на отсчёт; с учётом усиления 101 — около 8 мкВ на выходе датчика.
13. **Что будет со смещением нуля ОУ?** — Смещение до ±4,5 мВ умножается на 101, то есть до ±0,45 В на выходе. Это в пределах диапазона АЦП, а программа отслеживает базовую линию, поэтому не важно.

### Нагреватель и защита
14. **Как работает нагреватель?** — Три резистора по 150 Ом параллельно, 50 Ом: при 14,8 В это 0,3 А и 4,4 Вт. Включается N-канальным MOSFET IRLZ44N по нижнему плечу, ШИМ с вывода PC6.
15. **Почему IRLZ44N?** — Это logic-level MOSFET: полностью открывается уже при 3–4 В на затворе. Сопротивление около 0,035 Ом, потери около 3 мВт, радиатор не нужен.
16. **Зачем R11 и R12?** — R12 (10 кОм) держит затвор на земле при загрузке и сбросе МК, чтобы нагреватель не включился сам. R11 (1 кОм) ограничивает ток в затвор и позволяет компаратору «перебороть» вывод МК.
17. **Что если программа зависнет с включённым нагревателем?** — Аппаратная защита: отдельный термистор TH2 и компаратор LM393. При 40 °C выход с открытым коллектором притягивает затвор к земле независимо от МК. Плюс сторожевой таймер (watchdog) перезагружает МК через 4 с.
18. **Почему LM393 питается от 5 В?** — Диапазон входных синфазных напряжений LM393 — от 0 до Vпит − 1,5 В. При 3,3 В это только до 1,8 В, а напряжение с термистора на холоде доходит до 3 В.
19. **Почему два термистора?** — Независимость: один для программного регулирования (PB1), другой только для аппаратной защиты. Отказ одного не отключает обе защиты.
20. **Как пересчитать напряжение в температуру?** — Делитель: R_NTC = 10k·U/(3,3−U). Уравнение с коэффициентом B: 1/T = 1/298,15 + ln(R/10k)/B, B ≈ 3950. При 40 °C: 5,3 кОм и 1,14 В.
21. **Хватит ли мощности нагревателя?** — Тепловой баланс: P = ΔT / R_th. Чтобы держать +20 °C при −40 °C снаружи на 4,4 Вт, нужна теплоизоляция не хуже 13,6 К/Вт. Поэтому бокс утеплён, а в разреженном воздухе почти нет конвекции.
22. **Зачем NetTie?** — Разделение земель: силовой ток нагревателя не течёт по аналоговой земле и не создаёт помех в милливольтовом сигнале пульса. Земли соединяются в одной точке.

### Программа и радио
23. **Как вычисляется пульс?** — Оцифровка 50 Гц по прерыванию таймера, сглаживание, вычитание медленной базовой линии, адаптивный порог 40 % от амплитуды с гистерезисом, рефрактерный период. Частота — по медиане последних пяти интервалов: медиана устойчива к одному пропущенному или лишнему удару.
24. **Почему 50 Гц?** — Максимальная частота сигнала около 3,4 Гц после фильтра. По теореме Котельникова нужно больше 6,8 Гц; 50 Гц дают запас и точность измерения интервала ±20 мс.
25. **Какой формат пакетов?** — Бинарный: синхробайты AA 55, тип, номер пакета, длина, данные, CRC-16. Каждый пакет не больше 58 байт, то есть ровно один LoRa-пакет модуля E32. Строки не передаём: это лишние байты и потеря точности.
26. **Какая загрузка радиоканала?** — Около 54 байт/с, то есть 435 бит/с из 2400, около 18 %. Есть запас на повторы и помехи.
27. **Что если пакет потеряется?** — По номеру пакета на Земле видно потери. Всё пишется на microSD с полной частотой, так что после поиска спутника данные восстанавливаются.
28. **Зачем передавать саму форму сигнала?** — Чтобы на Земле пересчитать удары независимо и проверить бортовой алгоритм. 20 точек за 2 секунды (10 Гц) в каждом пакете.
29. **Почему не использовали MS5611 из библиотеки организаторов?** — Мы нашли ошибку: при температуре ниже 20 °C коррекция второго порядка смешивает единицы (°C и 0,01 °C) и знак T2, и температура получается неверной на десятки и сотни градусов. Мы использовали формулы из даташита и проверили их на примере из даташита. Также формула высоты верна только до 11 км, поэтому считаем по стандартной атмосфере (ISA): на 30 мбар это 23,8 км, а не 21,6 км.
30. **Что такое watchdog и зачем?** — Независимый таймер: если программа не сбрасывает его 4 секунды (зависла шина I²C или SD-карта), микроконтроллер перезагружается. Причину сброса передаём в статусном пакете.

### Почему так, а не иначе
31. **Почему нет своего микроконтроллера на плате полезной нагрузки?** — У полётного контроллера уже есть 16-битный АЦП, таймеры и шина I²C4 на IntroBus_L. Второй МК — вторая прошивка, ещё один канал связи и больше точек отказа. Для следующей версии (CanSat/IREC) мы планируем свой МК: модульность и независимое управление нагревом.
32. **Почему не только MAX30100?** — Он рассчитан на палец человека, на кровоток с гемоглобином. У улитки гемолимфа с гемоцианином, и сигнал другой. TCRT5000 с нашим усилителем позволяет настроить усиление и полосу под медленное сердце, а MAX30100 — резерв.
33. **Почему макетная плата, а не печатная?** — Сроки: на заказ ПП нужны недели. Компоненты в DIP-корпусах на панельках, соединения пайкой, разъёмы с защёлкой, покрытие цапон-лаком от конденсата.
34. **Как защищены от вибрации и конденсата?** — Разъёмы JST-XH с защёлкой, провода закреплены каптоновым скотчем, плата покрыта лаком, микросхемы в панельках.
35. **Что бы вы улучшили?** — Гистерезис компаратора; обнаружение обрыва термистора на второй половине LM393; печатная плата; свой МК; более толстая изоляция; калибровка датчика на улитках до полёта.

---

## 7. Russian ↔ English terms

| English | Русский |
|---|---|
| schematic | принципиальная схема |
| op-amp | операционный усилитель (ОУ) |
| comparator | компаратор |
| gain | коэффициент усиления |
| high-pass / low-pass filter | фильтр высоких / низких частот (ФВЧ / ФНЧ) |
| band-pass | полосовой фильтр |
| cutoff frequency | частота среза |
| time constant | постоянная времени |
| voltage divider | делитель напряжения |
| reference voltage | опорное напряжение |
| buffer / voltage follower | буфер / повторитель напряжения |
| offset voltage | напряжение смещения нуля |
| rail-to-rail | rail-to-rail («от шины до шины») |
| decoupling capacitor | блокировочный (развязывающий) конденсатор |
| pull-up / pull-down resistor | подтягивающий резистор к питанию / к земле |
| open collector | открытый коллектор |
| hysteresis | гистерезис |
| MOSFET, gate / drain / source | полевой транзистор, затвор / сток / исток |
| low-side switch | ключ в нижнем плече |
| PWM, duty cycle | ШИМ, коэффициент заполнения |
| thermistor (NTC) | терморезистор (термистор) с отрицательным ТКС |
| phototransistor | фототранзистор |
| ADC / resolution / sampling rate | АЦП / разрядность / частота дискретизации |
| interrupt | прерывание |
| watchdog | сторожевой таймер |
| checksum (CRC) | контрольная сумма |
| packet / frame | пакет / кадр |
| air data rate | скорость передачи в эфире |
| link budget | энергетический бюджет радиолинии |
| ground station | наземная станция |
| heart rate / beats per minute | частота сердечных сокращений (ЧСС) / удары в минуту |
| inter-beat interval | межударный интервал |
| median | медиана |
| thermal resistance | тепловое сопротивление |
| conformal coating | защитное лаковое покрытие |
| net tie, star ground | соединение земель в одной точке («звезда») |

---

## 8. Sources

**Organizers / platform**
- IntroSat.Platform short guide (EN and RU) and the Orbita mission documents — in `resources/`
- IntroStratLib (organizers' sensor library): https://github.com/Obu-IntroSat/IntroStratLib-Public
- STM32H750VB: https://www.st.com/en/microcontrollers-microprocessors/stm32h750vb.html
- Ebyte E32-433T30D datasheet (link from the guide): https://www.cdebyte.com/pdf-down.aspx?id=1462
- MS5611-01BA03 datasheet (TE Connectivity), linked from the IntroSat guide; precise altitude application note (AMSYS): https://www.amsys-sensor.com/downloads/notes/ms5611-precise-altitude-measurement-with-a-pressure-sensor-module-amsys-509e.pdf

**Components** (read at least the first page and the "absolute maximum ratings" of each)
- TCRT5000 (Vishay): https://www.vishay.com/docs/83760/tcrt5000.pdf
- CNY70 (Vishay; the sensor used in the snail and mussel papers): https://www.vishay.com/docs/83751/cny70.pdf
- MCP6002 (Microchip): http://ww1.microchip.com/downloads/en/DeviceDoc/21733j.pdf
- LM393 (TI): https://www.ti.com/lit/ds/symlink/lm393.pdf
- IRLZ44N (Infineon / International Rectifier) — datasheet on infineon.com, search "IRLZ44N"
- SHT21 (Sensirion): https://sensirion.com/products/catalog/SHT21
- BMP280 (Bosch Sensortec): https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280/
- MAX30100 (Analog Devices / Maxim): https://www.analog.com/en/products/max30100.html
- MF58 10 k NTC — B value on the seller's page; we assume B = 3950

**Science — heartbeat through the shell, snail physiology**
- Burnett N.P. et al. (2013). *An improved noninvasive method for measuring heartbeat of intertidal animals.* Limnology & Oceanography: Methods 11:91–100. https://doi.org/10.4319/lom.2013.11.91 — CNY70 over the heart, band-pass amplifier ≈ 0.2–2.2 Hz. Closest to our design: read this one.
- Depledge M.H. & Andersen B.B. (1990). *A computer-aided physiological monitoring system for continuous, long-term recording of cardiac activity in selected invertebrates.* Comparative Biochemistry and Physiology A 96:473–477 — the original IR heartbeat method (CAPMON).
- Biannic M. et al. (1994). *Heart-rate in relation to dehydration in the snail Helix aspersa.* Comp. Biochem. Physiol. A 108(1):65–67 — heart rate falls to ~63 % during dehydration.
- Toader-Williams A. & Buicu O. (2011). Bull. UASVM Cluj-Napoca, Animal Science and Biotechnologies 68(1-2) — 97–102 bpm in young *Cornu aspersum* at high temperature. https://doi.org/10.15835/buasvmcn-asb:68:1-2:6715
- Bailey S.E.R. & Lazaridou-Dimitriadou M. (1991). *Inverse temperature acclimation of heart rate in hibernating land snails.* J. Comp. Physiol. B 160:677–681.
- Methods in Ecology & Evolution blog (2025), "Peering through the shell: tracking stress with heartbeats": https://methodsblog.com/2025/10/28/peering-through-the-shell-tracking-stress-with-heartbeats/

**Theory refreshers** — any electronics textbook chapter on RC filters, non-inverting amplifiers, comparators and MOSFET switches (for example *The Art of Electronics*, chapters 1, 2.4, 4, 3.5). The ISA atmosphere is the U.S. Standard Atmosphere 1976 / ICAO Doc 7488.
