# Host tests for the InkBlink firmware: they compile the sketch with g++ on a PC against
# stand-ins for the Arduino/ESP32 libraries (tests/stubs) and a stand-in e-paper driver,
# so no hardware and no Elecrow driver files are needed.
#
#   make test          run everything that does not need the real driver
#   make test-driver   also run the tests that need the real e-paper driver files
#                      (they are part of the repository, see firmware/inkblink/driver/README.md)
#   make clean
#
# Needs: g++ (C++17), python3, node >= 18.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -w
SK       := ../firmware/inkblink
B        := build
RUN      := $(B)/run
ELECROW  := EPD.cpp EPD.h EPDfont.h EPD_Init.cpp EPD_Init.h spi.cpp spi.h

STUB_INC := -I$(B)/sketch -Istubs
STUB_SRC := common.cpp stubs/EPD_stub.cpp

CPP_TESTS := test_core test_ui_features test_fonts_margins test_clock_settings \
             test_hyphenation_layout test_toc_chapters test_glyph_widths \
             test_battery test_battery_estimate test_menu_shortcuts test_arduino_prototypes test_no_u8g2

.PHONY: test test-cpp test-js test-meta test-strict test-driver clean dirs fixtures
test: test-strict test-cpp test-js test-meta
	@echo; echo "ALL TESTS PASSED"

dirs:
	@mkdir -p $(RUN)/fs $(RUN)/png $(B)/fixtures $(B)/samples

# --- sketch copy without any Elecrow files (so stubs are always used) ---
$(B)/sketch/.stamp: $(wildcard $(SK)/*.ino $(SK)/*.h) | dirs
	@rm -rf $(B)/sketch; mkdir -p $(B)/sketch
	@cp $(SK)/*.ino $(SK)/*.h $(B)/sketch/
	@cd $(B)/sketch && rm -f $(ELECROW)
	@touch $@

$(B)/sketch_nou8g2/.stamp: $(B)/sketch/.stamp
	@rm -rf $(B)/sketch_nou8g2; cp -r $(B)/sketch $(B)/sketch_nou8g2
	@sed -i 's/#define USE_U8G2 1/#define USE_U8G2 0/' $(B)/sketch_nou8g2/inkblink.ino
	@grep -q '#define USE_U8G2 0' $(B)/sketch_nou8g2/inkblink.ino
	@touch $@

$(B)/proto.ino: $(B)/sketch/.stamp arduino_proto.py
	@python3 arduino_proto.py $(B)/sketch/inkblink.ino $@ > /dev/null

# --- test samples / fixtures ---
fixtures: dirs
	@python3 ../tools/epub/make_samples.py $(B)/samples > /dev/null
	@node ../tools/epub/epub2txt.mjs $(B)/samples/big.epub $(B)/fixtures Dluga_podroz > /dev/null

# --- C++ tests ---
$(B)/%: %.cpp $(B)/sketch/.stamp $(STUB_SRC)
	$(CXX) $(CXXFLAGS) $(STUB_INC) $< $(STUB_SRC) -o $@

$(B)/test_battery: test_battery.cpp $(B)/sketch/.stamp $(STUB_SRC)
	$(CXX) $(CXXFLAGS) -DPIN_BAT_ADC=3 $(STUB_INC) $< $(STUB_SRC) -o $@

$(B)/test_no_u8g2: test_no_u8g2.cpp $(B)/sketch_nou8g2/.stamp $(STUB_SRC)
	$(CXX) $(CXXFLAGS) -I$(B)/sketch_nou8g2 -Istubs $< $(STUB_SRC) -o $@

$(B)/test_arduino_prototypes: test_arduino_prototypes.cpp $(B)/proto.ino $(STUB_SRC)
	$(CXX) $(CXXFLAGS) -I$(B) -I$(B)/sketch -Istubs $< $(STUB_SRC) -o $@

test-cpp: dirs fixtures $(addprefix $(B)/,$(CPP_TESTS))
	@set -e; for t in $(CPP_TESTS); do echo "--- $$t"; ./$(B)/$$t | grep -aE "====|FAIL" || true; ./$(B)/$$t > $(RUN)/$$t.log 2>&1 || { echo "FAILED: $$t (see $(RUN)/$$t.log)"; exit 1; }; done

# --- JavaScript tests (EPUB converter and Books page script) ---
test-js: dirs fixtures
	@python3 ../tools/sync_webui_epub.py --check
	@node ../tools/epub/test_epub.mjs $(B)/samples | tail -1
	@node ../tools/epub/test_books_page.mjs $(B)/samples | tail -1

# Narrowing in brace initialisers is an error on ESP32 core 3.x but silent with -w (core 2.x
# default), so check the sketch for it separately, without -w.
test-strict: $(B)/sketch/.stamp
	@$(CXX) -std=c++17 -fsyntax-only -Werror=narrowing $(STUB_INC) test_core.cpp 2>&1 | grep -A3 "narrowing" ; \
	  if $(CXX) -std=c++17 -fsyntax-only -Werror=narrowing $(STUB_INC) test_core.cpp > /dev/null 2>&1; then echo "==== STRICT (narrowing): OK ===="; else echo "FAILED: narrowing conversion (see above)"; exit 1; fi

test-meta:
	@python3 check_version.py

# --- tests that need the real Elecrow driver files in firmware/inkblink/ ---
test-driver: dirs fixtures
	@for f in $(ELECROW); do test -f $(SK)/$$f || { echo "missing driver file $(SK)/$$f - see firmware/inkblink/driver/README.md"; exit 1; }; done
	@rm -rf $(B)/sketch_real; mkdir -p $(B)/sketch_real; cp $(SK)/*.ino $(SK)/*.h $(SK)/*.cpp $(B)/sketch_real/
	$(CXX) $(CXXFLAGS) -Idriver -I$(B)/sketch_real -Istubs driver/test_fast_refresh.cpp common.cpp driver/spi_trace.cpp $(B)/sketch_real/EPD.cpp $(B)/sketch_real/EPD_Init.cpp -o $(B)/test_fast_refresh
	$(CXX) $(CXXFLAGS) -Idriver -I$(B)/sketch_real -Istubs test_menu_shortcuts.cpp common.cpp driver/spi_trace.cpp $(B)/sketch_real/EPD.cpp $(B)/sketch_real/EPD_Init.cpp -o $(B)/test_menu_shortcuts_driver
	@./$(B)/test_fast_refresh | grep -aE "====|FAIL"
	@./$(B)/test_menu_shortcuts_driver | grep -aE "====|FAIL"

clean:
	rm -rf $(B)
