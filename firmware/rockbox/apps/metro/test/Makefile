# Tests host-side de la logica pura de Metro (sin Rockbox).
# Uso: make -C apps/metro/test

CC ?= cc
CFLAGS := -std=c99 -Wall -Wextra -Werror -O0 -g

BUILD := build

.PHONY: test clean

test: $(BUILD)/test_nav $(BUILD)/test_sync_marker $(BUILD)/test_device $(BUILD)/test_motion $(BUILD)/test_lrc $(BUILD)/test_artist_images $(BUILD)/test_fsutil $(BUILD)/test_lang $(BUILD)/test_volume $(BUILD)/test_firmware_families $(BUILD)/test_master_art $(BUILD)/test_marquee $(BUILD)/test_shared_settings $(BUILD)/test_textseg
	@echo "== test_nav =="
	@$(BUILD)/test_nav
	@echo "== test_sync_marker =="
	@$(BUILD)/test_sync_marker
	@echo "== test_device =="
	@$(BUILD)/test_device
	@echo "== test_motion =="
	@$(BUILD)/test_motion
	@echo "== test_lrc =="
	@$(BUILD)/test_lrc
	@echo "== test_artist_images =="
	@$(BUILD)/test_artist_images
	@echo "== test_fsutil =="
	@$(BUILD)/test_fsutil
	@echo "== test_lang =="
	@$(BUILD)/test_lang
	@echo "== test_volume =="
	@$(BUILD)/test_volume
	@echo "== test_firmware_families =="
	@$(BUILD)/test_firmware_families
	@echo "== test_master_art =="
	@$(BUILD)/test_master_art
	@echo "== test_marquee =="
	@$(BUILD)/test_marquee
	@echo "== test_shared_settings =="
	@$(BUILD)/test_shared_settings
	@echo "== test_textseg =="
	@$(BUILD)/test_textseg

$(BUILD)/test_marquee: test_marquee.c ../metro_marquee_cycle.c | $(BUILD)
	$(CC) $(CFLAGS) test_marquee.c ../metro_marquee_cycle.c -o $@

$(BUILD)/test_nav: test_nav.c ../metro_nav.c ../metro_nav.h | $(BUILD)
	$(CC) $(CFLAGS) test_nav.c ../metro_nav.c -o $@

$(BUILD)/test_sync_marker: test_sync_marker.c ../metro_sync_marker.c ../metro_sync_marker.h | $(BUILD)
	$(CC) $(CFLAGS) test_sync_marker.c ../metro_sync_marker.c -o $@

$(BUILD)/test_device: test_device.c ../metro_device_name.c ../metro_device_name.h | $(BUILD)
	$(CC) $(CFLAGS) test_device.c ../metro_device_name.c -o $@

$(BUILD)/test_motion: test_motion.c ../metro_motion.c ../metro_motion.h | $(BUILD)
	$(CC) $(CFLAGS) test_motion.c ../metro_motion.c -o $@

$(BUILD)/test_lrc: test_lrc.c ../metro_lrc.c ../metro_lrc.h | $(BUILD)
	$(CC) $(CFLAGS) test_lrc.c ../metro_lrc.c -o $@

$(BUILD)/test_artist_images: test_artist_images.c ../metro_artist_images.c ../metro_artist_images.h ../metro_artist_images_parse.c ../metro_artist_images_parse.h | $(BUILD)
	$(CC) $(CFLAGS) test_artist_images.c ../metro_artist_images.c ../metro_artist_images_parse.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

$(BUILD)/test_fsutil: test_fsutil.c ../metro_fsutil.h | $(BUILD)
	$(CC) $(CFLAGS) test_fsutil.c -o $@

$(BUILD)/test_lang: test_lang.c ../metro_lang.c ../metro_lang.h | $(BUILD)
	$(CC) $(CFLAGS) test_lang.c ../metro_lang.c -o $@

$(BUILD)/test_volume: test_volume.c ../metro_volume.c ../metro_volume.h | $(BUILD)
	$(CC) $(CFLAGS) test_volume.c ../metro_volume.c -o $@

$(BUILD)/test_firmware_families: test_firmware_families.c ../metro_firmware_families.c ../metro_firmware_families.h ../metro_lang.h | $(BUILD)
	$(CC) $(CFLAGS) test_firmware_families.c ../metro_firmware_families.c -o $@

$(BUILD)/test_master_art: test_master_art.c ../metro_master_art_format.c ../metro_master_art_format.h | $(BUILD)
	$(CC) $(CFLAGS) test_master_art.c ../metro_master_art_format.c -o $@

$(BUILD)/test_shared_settings: test_shared_settings.c ../metro_shared_settings.c ../metro_shared_settings.h ../metro_settings.h ../metro_theme.h ../metro_lang.h | $(BUILD)
	$(CC) $(CFLAGS) test_shared_settings.c ../metro_shared_settings.c -o $@

$(BUILD)/test_textseg: test_textseg.c ../metro_textseg.c ../metro_textseg.h | $(BUILD)
	$(CC) $(CFLAGS) test_textseg.c ../metro_textseg.c -o $@
