# InkBlink (po polsku)

Otwartoźródłowy firmware czytnika e-booków dla płytki **Elecrow CrowPanel ESP32 2,13" e-paper (DIE01021S, wersja V1.2)**.

Książki (`.txt` i EPUB) wgrywasz przez własną sieć WiFi czytnika, bez karty SD i bez internetu. Obsługa polskich liter, strona WWW, statystyki czytania, dzielenie wyrazów, wiele czcionek i opcji wygody czytania.

**Stan: beta.** Rozwijane i sprawdzane na jednej płytce V1.2. Logika jest pokryta testami na komputerze, ale działanie ekranu, baterii i zegara sprawdzane jest ręcznie.

## Szybki start

1. Arduino IDE 2.x + rdzeń ESP32 3.x, biblioteki `Adafruit GFX Library` i `U8g2_for_Adafruit_GFX`.
2. Otwórz `firmware/inkblink/inkblink.ino`: płytka *ESP32S3 Dev Module*, Flash 8 MB, PSRAM OPI, USB CDC On Boot: Disabled, plik `partitions.csv` obok szkicu.
3. Wgraj. Jeśli po pierwszym wgraniu płytka się zapętla, włącz raz „Erase All Flash Before Sketch Upload”.
4. Książki: **Menu → Upload**, połącz się z siecią `InkBlink-XXXX` (kod QR lub hasło z ekranu), otwórz `http://192.168.4.1` i przeciągnij pliki `.txt` / `.epub`.

Obsługa przycisków i ekranów: [docs/USER_GUIDE.md](docs/USER_GUIDE.md) (po angielsku). Testy: `make -C tests test`.

Licencja własnego kodu: MIT. Sterownik ekranu pochodzi od Elecrow i jest w repozytorium za ich zgodą (nie jest na licencji MIT). Szczegóły i licencje pozostałych komponentów: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
