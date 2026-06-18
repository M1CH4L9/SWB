# Instrukcja: jak odpalic, zapisac i oddac Projekt 3

Ten dokument opisuje **tylko** co zrobic krok po kroku na komputerze i na zajeciach. Nie tlumaczy teorii projektu.

---

## 1. Co musisz miec

### Programy (z listy od prowadzacego)

1. **STM32CubeIDE** (najnowsza wersja) - [https://www.st.com/stm32cubeide](https://www.st.com/stm32cubeide)
2. **Sterowniki ST-Link** (zazwyczaj instaluja sie z CubeIDE albo z Windows Update po podlaczeniu plytki)
3. Opcjonalnie **STM32CubeMX** (jest wbudowany w CubeIDE, osobno nie musisz)

### Sprzet

- Płytka **NUCLEO-G491RE** (STM32G491)
- Modul **TFT + dotyk** (ILI9341 + XPT2046)
- **Sonar HC-SR04**
- **Serwomechanizm**
- **Silnik krokowy** + sterownik (4 przewody IN1-IN4)
- Kabel **USB micro** do Nucleo (programowanie przez ST-Link)

---

## 2. Gdzie jest projekt

Folder projektu na Twoim dysku:

```
c:\Users\micha\Downloads\swb\Projekt 3\Lecture_10_TFT_TOUCH_SERVO_SONAR_STEP\
```

Wazne pliki:

| Plik / folder | Po co |
|---------------|-------|
| `Lecture_10_TFT_TOUCH_SERVO_SONAR_STEP.ioc` | Konfiguracja peryferiow (CubeMX) |
| `Core/Src/main.c` | Glowna logika |
| `Core/Src/*.c` | Moduly (GUI, skan, flash...) |
| `.project` | Nazwa projektu w CubeIDE: **Projekt 3** |

Raport Word:

```
c:\Users\micha\Downloads\swb\Projekt 3\Raport_Projekt3.docx
```

---

## 3. Import projektu do STM32CubeIDE

1. Uruchom **STM32CubeIDE**.
2. Wybierz workspace (folder gdzie trzymasz projekty) - np. `C:\STM32_workspace`.
3. Menu: **File → Import...**
4. Wybierz: **General → Existing Projects into Workspace** → Next.
5. W **Select root directory** kliknij **Browse** i wskaz:
   ```
   c:\Users\micha\Downloads\swb\Projekt 3\Lecture_10_TFT_TOUCH_SERVO_SONAR_STEP
   ```
6. Upewnij sie ze projekt **Projekt 3** jest zaznaczony.
7. Kliknij **Finish**.

Projekt pojawi sie w **Project Explorer** po lewej.

---

## 4. Podlaczenie plytki

1. Podlacz Nucleo kablem USB do komputera (port oznaczony **USB ST-LINK**).
2. Poczekaj az Windows zainstaluje sterownik (pierwszy raz moze chwile potrwac).
3. Podlacz moduly zgodnie z zajeciami (TFT, sonar, serwo, krokowiec na plytce rozszerzen).

Piny sa zdefiniowane w pliku `Core/Inc/main.h` (m.in. PB10 serwo, PC6/PC8 sonar, PB11-14 krokowiec, SPI1 TFT).

---

## 5. Kompilacja (build)

1. W **Project Explorer** kliknij prawym na **Projekt 3** → **Build Project**.
   - Albo skrot: **Ctrl + B**
2. Na dole w zakladce **Console** powinno byc: **Build Finished** i **0 errors**.
3. Jesli sa bledy - sprawdz czy wszystkie pliki `.c` w `Core/Src` sa widoczne w projekcie (CubeIDE zwykle dodaje je automatycznie).

---

## 6. Wgranie programu na plytke (flash)

1. Upewnij sie ze plytka jest podlaczona USB.
2. Kliknij zielona ikone **Run** (lub **Debug** - ikona robaka).
   - Skrot debug: **F11**
   - Skrot run: **Ctrl + F11**
3. Przy pierwszym uruchomieniu moze wyskoczyc okno **Debug configuration** - zostaw domyslne (ST-Link, NUCLEO-G491RE) i kliknij **OK**.
4. Program sie skompiluje (jesli trzeba) i wgra na MCU.
5. Jesli wszedles w tryb **Debug** - kliknij czerwony **Terminate** (kwadrat) zeby program normalnie dzialal bez debuggera zatrzymujacego procesor.

Po udanym flashu ekran TFT powinien pokazac ekran **KONFIGURACE**.

---

## 7. Pierwsze uruchomienie na sprzecie (kolejnosc)

1. **Ekran konfiguracji** - ustaw Min/Max serwa i czas skanu przyciskami +/-.
2. Wejdz w **KALIBR** - skalibruj wskaznik krokowy:
   - Dla kazdego kroku (LEVA, STRED, PRAVA) ustaw serwo automatycznie, dopasuj wskaznik przyciskami +/- i kliknij **ULOZIT**.
   - Po trzech punktach kliknij **ZPET**.
3. Kliknij **START** - przejdziesz do trybu skanu.
4. Postaw cos przed sonarem - zobaczysz paski i podswietlenie najblizszego celu.
5. **STOP** - powrot do konfiguracji.

---

## 8. Jesli cos nie dziala (szybkie fixy)

### Bialy / czarny ekran TFT

- Sprawdz zasilanie modulu i polaczenia SPI.
- Upewnij sie ze program sie wgral (brak bledu przy flash).

### Dotyk nie trafia w przyciski

- Otworz `Core/Src/xpt2046.c`.
- Zmien wartosci `TOUCH_RAW_X_MIN/MAX`, `TOUCH_RAW_Y_MIN/MAX` albo ustaw:
  - `TOUCH_INVERT_X` / `TOUCH_INVERT_Y` na `1`
- Zapisz, zbuduj ponownie, wgraj.

### Sonar nie mierzy

- Sprawdz kable TRIG (PC8) i ECHO (PC6).
- Upewnij sie ze nic nie zaslania sonaru zbyt blisko (< 2 cm).

---

## 9. Co spakowac do oddania (ZIP)

Stworz folder np. `TwojeNazwisko_Projekt3` i wrzuc:

| Element | Co dokladnie |
|---------|--------------|
| Projekt CubeIDE | Caly folder `Lecture_10_TFT_TOUCH_SERVO_SONAR_STEP` **BEZ** folderu `Debug/` (opcjonalnie tez bez `.settings/`) |
| Plik .ioc | `Lecture_10_TFT_TOUCH_SERVO_SONAR_STEP.ioc` (jest w folderze projektu) |
| Raport | `Raport_Projekt3.docx` |
| Zrzut ekranu | PNG ze ekranu skanu z widocznymi paskami (zrób na telefonie lub screenshot) |
| Film | Krotki film 30-60 s (ponizej) |

### Jak zrobic ZIP w Windows

1. Zaznacz folder z powyzszymi plikami.
2. Prawy przycisk → **Kompresuj do pliku ZIP** (lub **Wyślij do → Skompresowany folder**).
3. Nazwij np. `Kowalski_Projekt3.zip`.

---

## 10. Jak nagrac film do oddania

1. Ustaw telefon tak, zeby bylo widac **ekran TFT** i **sonar z serwem** (i najlepiej wskaznik).
2. Nagraj ok. **30-60 sekund**:
   - pokaz ekran konfiguracji (2-3 s),
   - kliknij START,
   - postaw reke / karton przed sonarem,
   - pokaz paski i podswietlenie celu,
   - pokaz ze wskaznik krokowy sie rusza (jesli skalibrowany).
3. Zapisz jako **MP4** (najlepiej).
4. Dolacz do ZIP albo wyslij osobno - zgodnie z wymaganiami prowadzacego.

---

## 11. Gdzie wyslac

**Uzupelnij to miejsce danymi od cwiczeniowca** (np. email, Moodle, Teams):

- Adres: `___________________________`
- Termin: `___________________________`
- Format nazwy pliku: `___________________________`

Jesli prowadzacy chce osobno raport i osobno kod - wyslij ten sam ZIP lub rozdziel zgodnie z instrukcja na zajeciach.

---

## 12. Checklist przed wyslaniem

- [ ] Projekt buduje sie bez bledow (0 errors)
- [ ] W ZIP jest plik `.ioc`
- [ ] W ZIP jest raport `.docx`
- [ ] Jest zrzut ekranu GUI
- [ ] Jest krotki film z dzialaniem
- [ ] Na zajeciach sprawdziles dzialanie na prawdziwej plytce

---

## 13. Szybka sciezka (TL;DR)

```
1. CubeIDE → Import → folder projektu
2. Podlacz Nucleo USB
3. Ctrl+B (build)
4. F11 lub Run (flash)
5. KALIBR → START → test
6. Spakuj projekt + raport + foto + film → wyslij
```

Powodzenia.
