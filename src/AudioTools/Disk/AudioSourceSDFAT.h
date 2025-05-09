#pragma once

#include <SPI.h>
#include <SdFat.h>

#include "AudioLogger.h"
#include "AudioTools/Disk/AudioSource.h"
#include "AudioToolsConfig.h"

#define USE_SDFAT
#include "AudioTools/Disk/SDDirect.h"

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#ifndef SD_FAT_TYPE
#define SD_FAT_TYPE 1
#endif
// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur. (40?)
#define SPI_CLOCK SD_SCK_MHZ(50)

#if SD_FAT_TYPE == 0
typedef SdFat AudioFs;
typedef File AudioFile;
#elif SD_FAT_TYPE == 1
typedef SdFat32 AudioFs;
typedef File32 AudioFile;
#elif SD_FAT_TYPE == 2
typedef SdExFat AudioFs;
typedef ExFile AudioFile;
#elif SD_FAT_TYPE == 3
typedef SdFs AudioFs;
typedef FsFile AudioFile;
#endif

namespace audio_tools {
/**
 * @brief ESP32 AudioSource for AudioPlayer using an SD card as data source.
 * This class is based on the Arduino SD implementation
 * Connect the SD card.
 * For UTF8 Support change SdFatConfig.h #define USE_UTF8_LONG_NAMES 1
 * @ingroup player
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioSourceSDFAT : public AudioSource {
 public:
  /// Default constructor
  AudioSourceSDFAT(const char *startFilePath = "/", const char *ext = ".mp3",
                   int chipSelect = PIN_CS, int speedMHz = 10,
                   int spi_mode = DEDICATED_SPI, bool setupIndex = true) {
    TRACED();
    LOGI("SD chipSelect: %d", chipSelect);
    LOGI("SD speedMHz: %d", speedMHz);
    LOGI("ext: %s", ext);
    p_cfg = new SdSpiConfig(chipSelect, spi_mode, SD_SCK_MHZ(speedMHz));
    owns_cfg = true;
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
  }

  /// Costructor with SdSpiConfig
  AudioSourceSDFAT(const char *startFilePath, const char *ext,
                   SdSpiConfig &config, bool setupIndex = true) {
    TRACED();
    p_cfg = &config;
    owns_cfg = false;
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
  }

  /// Constructor for providing an open FS
  AudioSourceSDFAT(AudioFs fs, const char *startFilePath="/", const char *ext="", bool setupIndex = true){
    TRACED();
    sd = fs;
    p_cfg = nullptr;
    owns_cfg = false;
    start_path = startFilePath;
    exension = ext;
    setup_index = setupIndex;
    is_sd_setup = true;
    // since we expect an open fs we do not close it
    is_close_sd = false;
  }

  virtual ~AudioSourceSDFAT() {
    end();
    if (owns_cfg) delete (p_cfg);
  }

  virtual void begin() override {
    TRACED();
    if (!is_sd_setup) {
      if (!sd.begin(*p_cfg)) {
        LOGE("sd.begin failed");
        return;
      }
      is_sd_setup = true;
    }
    idx.begin(start_path, exension, file_name_pattern);
    idx_pos = 0;
  }

  void end() {
    if (is_sd_setup) {
      TRACEI();
#ifdef ESP32
      if (is_close_sd) sd.end();
#endif
      is_sd_setup = false;
    }
  }

  virtual Stream *nextStream(int offset = 1) override {
    LOGI("nextStream: %d", offset);
    return selectStream(idx_pos + offset);
  }

  virtual Stream *selectStream(int index) override {
    LOGI("selectStream SDFAT: %d", index);
    if (index > -1) {  // avoid invalid position
      idx_pos = index;
    }
    return selectStream(idx[idx_pos]);
  }

  virtual Stream *selectStream(const char *path) override {
    file.close();
    if (path == nullptr) {
      LOGE("Filename is null")
      return nullptr;
    }

    // AudioFile new_file;
    if (!file.open(path, O_RDONLY)) {
      LOGE("Open error: '%s'", path);
    }

    LOGI("-> selectStream: %s", path);
    strncpy(file_name, path, MAX_FILE_LEN);
    // file = new_file;
    return &file;
  }

  /// Defines the regex filter criteria for selecting files. E.g. ".*Bob
  /// Dylan.*"
  void setFileFilter(const char *filter) { file_name_pattern = filter; }

  /// Provides the current index position
  int index() { return idx_pos; }

  /// provides the actual file name
  const char *toStr() { return file_name; }

  // provides default setting go to the next
  virtual bool isAutoNext() { return true; }

  /// Allows to "correct" the start path if not defined in the constructor
  virtual void setPath(const char *p) { start_path = p; }

  /// Provides the number of files (The max index is size()-1): WARNING this is
  /// very slow if you have a lot of files in many subdirectories
  long size() { return idx.size(); }

  /// provides access to the AudioFs object
  AudioFs &getAudioFs() { return sd; }

 protected:
  SdSpiConfig *p_cfg = nullptr;
  AudioFs sd;
  AudioFile file;
  SDDirect<AudioFs, AudioFile> idx{sd};
  size_t idx_pos = 0;
  char file_name[MAX_FILE_LEN];
  const char *exension = nullptr;
  const char *start_path = nullptr;
  const char *file_name_pattern = "*";
  int cs;
  bool setup_index = true;
  bool owns_cfg = false;
  bool is_sd_setup = false;
  bool is_close_sd = true;

  const char *getFileName(AudioFile &file) {
    static char name[MAX_FILE_LEN];
    file.getName(name, MAX_FILE_LEN);
    return name;
  }
};

}  // namespace audio_tools
