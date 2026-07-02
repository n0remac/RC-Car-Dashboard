#include <LittleFS.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

// ----------------------
// Browser uploaded audio
// ----------------------
static const char *BROWSER_AUDIO_PATH = "/sound.wav";
static const char *BROWSER_AUDIO_TEMP_PATH = "/sound.tmp";
static const char *BROWSER_AUDIO_BACKUP_PATH = "/sound.bak";
static const uint32_t BROWSER_AUDIO_MIN_SAMPLE_RATE = 8000;
static const uint32_t BROWSER_AUDIO_MAX_SAMPLE_RATE = 48000;

struct BrowserAudioWavInfo {
  bool valid = false;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataLength = 0;
  String message = "No saved sound";
};

uint16_t readBrowserAudioLe16(const uint8_t *data);
uint32_t readBrowserAudioLe32(const uint8_t *data);
bool browserAudioReadExact(File &file, uint8_t *buffer, size_t length);
bool browserAudioFourCCEquals(const uint8_t *data, const char *value);
String formatBrowserAudioBytes(size_t bytes);
String browserAudioWavInfoLabel(const BrowserAudioWavInfo &info);
BrowserAudioWavInfo validateBrowserAudioWavFile(const char *path);
void refreshBrowserAudioFileState();
void stopBrowserAudioPlayback(const String &status);
bool startBrowserAudioPlayback(String &message);
bool replaceBrowserAudioFile(String &error);
void failBrowserAudioUpload(const String &message);
String browserAudioResponseJson(bool ok, const String &message);
void sendBrowserAudioResponse(int code, bool ok, const String &message);

bool prepareSpeakerI2sForBrowserAudio(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
);

String speakerI2sStatusLabel();

bool browserAudioStorageMounted = false;
String browserAudioStorageStatus = "Not mounted";
String browserAudioStatus = "Not ready";
BrowserAudioWavInfo browserAudioSavedInfo;
size_t browserAudioSavedSize = 0;
bool browserAudioPlaying = false;

File browserAudioPlaybackFile;
WAVDecoder browserAudioWavDecoder;
EncodedAudioStream browserAudioDecoder;
StreamCopy browserAudioCopier;

File browserAudioUploadFile;
size_t browserAudioUploadBytes = 0;
size_t browserAudioUploadLimit = 0;
bool browserAudioUploadFailed = false;
bool browserAudioUploadFinished = false;
String browserAudioUploadMessage = "No upload received";

static const float BROWSER_AUDIO_VOLUME = 1.0f;
extern VolumeStream browserAudioVolume;

uint16_t readBrowserAudioLe16(const uint8_t *data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

uint32_t readBrowserAudioLe32(const uint8_t *data) {
  return (uint32_t)data[0] |
    ((uint32_t)data[1] << 8) |
    ((uint32_t)data[2] << 16) |
    ((uint32_t)data[3] << 24);
}

bool browserAudioReadExact(File &file, uint8_t *buffer, size_t length) {
  return file.read(buffer, length) == (int)length;
}

bool browserAudioFourCCEquals(const uint8_t *data, const char *value) {
  return data[0] == value[0] &&
    data[1] == value[1] &&
    data[2] == value[2] &&
    data[3] == value[3];
}

String formatBrowserAudioBytes(size_t bytes) {
  if (bytes < 1024) {
    return String((unsigned long)bytes) + " B";
  }
  if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0f, 1) + " KB";
  }
  return String(bytes / 1048576.0f, 2) + " MB";
}

String browserAudioWavInfoLabel(const BrowserAudioWavInfo &info) {
  if (!info.valid) {
    return info.message;
  }

  String label = String(info.sampleRate) + " Hz, ";
  label += String(info.channels);
  label += info.channels == 1 ? " channel, " : " channels, ";
  label += String(info.bitsPerSample);
  label += "-bit PCM";
  return label;
}

BrowserAudioWavInfo validateBrowserAudioWavFile(const char *path) {
  BrowserAudioWavInfo info;

  if (!browserAudioStorageMounted) {
    info.message = "Storage unavailable";
    return info;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    info.message = "File not found";
    return info;
  }

  size_t fileSize = file.size();
  if (fileSize < 44) {
    info.message = "WAV file is too small";
    file.close();
    return info;
  }

  uint8_t header[12];
  if (!browserAudioReadExact(file, header, sizeof(header)) ||
      !browserAudioFourCCEquals(header, "RIFF") ||
      !browserAudioFourCCEquals(header + 8, "WAVE")) {
    info.message = "File is not a RIFF/WAVE file";
    file.close();
    return info;
  }

  bool foundFmt = false;
  bool foundData = false;
  uint16_t audioFormat = 0;
  uint16_t blockAlign = 0;
  uint32_t byteRate = 0;

  while ((file.position() + 8) <= fileSize) {
    uint8_t chunkHeader[8];
    if (!browserAudioReadExact(file, chunkHeader, sizeof(chunkHeader))) {
      info.message = "Could not read WAV chunk";
      file.close();
      return info;
    }

    uint32_t chunkSize = readBrowserAudioLe32(chunkHeader + 4);
    size_t chunkStart = file.position();
    if (chunkSize > (fileSize - chunkStart)) {
      info.message = "WAV chunk exceeds file size";
      file.close();
      return info;
    }
    size_t chunkEnd = chunkStart + chunkSize;

    if (browserAudioFourCCEquals(chunkHeader, "fmt ")) {
      if (chunkSize < 16) {
        info.message = "WAV fmt chunk is too small";
        file.close();
        return info;
      }

      uint8_t fmt[16];
      if (!browserAudioReadExact(file, fmt, sizeof(fmt))) {
        info.message = "Could not read WAV format";
        file.close();
        return info;
      }

      audioFormat = readBrowserAudioLe16(fmt);
      info.channels = readBrowserAudioLe16(fmt + 2);
      info.sampleRate = readBrowserAudioLe32(fmt + 4);
      byteRate = readBrowserAudioLe32(fmt + 8);
      blockAlign = readBrowserAudioLe16(fmt + 12);
      info.bitsPerSample = readBrowserAudioLe16(fmt + 14);
      foundFmt = true;
    } else if (browserAudioFourCCEquals(chunkHeader, "data")) {
      info.dataLength = chunkSize;
      foundData = chunkSize > 0;
    }

    size_t nextChunk = chunkEnd + (chunkSize % 2);
    if (nextChunk > fileSize) {
      info.message = "WAV chunk padding exceeds file size";
      file.close();
      return info;
    }
    if (!file.seek(nextChunk)) {
      info.message = "Could not seek WAV chunk";
      file.close();
      return info;
    }

    if (foundFmt && foundData) {
      break;
    }
  }

  file.close();

  if (!foundFmt) {
    info.message = "WAV fmt chunk not found";
    return info;
  }
  if (!foundData) {
    info.message = "WAV data chunk not found";
    return info;
  }
  if (audioFormat != 1) {
    info.message = "Only PCM WAV files are supported";
    return info;
  }
  if (info.channels < 1 || info.channels > 2) {
    info.message = "Only mono or stereo WAV files are supported";
    return info;
  }
  if (info.bitsPerSample != 16) {
    info.message = "Only 16-bit PCM WAV files are supported";
    return info;
  }
  if (info.sampleRate < BROWSER_AUDIO_MIN_SAMPLE_RATE ||
      info.sampleRate > BROWSER_AUDIO_MAX_SAMPLE_RATE) {
    info.message = "WAV sample rate must be 8 kHz to 48 kHz";
    return info;
  }

  uint16_t expectedBlockAlign = info.channels * (info.bitsPerSample / 8);
  uint32_t expectedByteRate = info.sampleRate * expectedBlockAlign;
  if (blockAlign != expectedBlockAlign || byteRate != expectedByteRate) {
    info.message = "WAV byte rate or block alignment is invalid";
    return info;
  }

  info.valid = true;
  info.message = "Ready";
  return info;
}

void refreshBrowserAudioFileState() {
  browserAudioSavedInfo = BrowserAudioWavInfo();
  browserAudioSavedSize = 0;

  if (!browserAudioStorageMounted) {
    return;
  }

  if (!LittleFS.exists(BROWSER_AUDIO_PATH)) {
    browserAudioSavedInfo.message = "No saved sound";
    return;
  }

  File file = LittleFS.open(BROWSER_AUDIO_PATH, "r");
  if (file) {
    browserAudioSavedSize = file.size();
    file.close();
  }

  browserAudioSavedInfo = validateBrowserAudioWavFile(BROWSER_AUDIO_PATH);
}

void initBrowserAudioStorage() {
  if (LittleFS.begin(true)) {
    browserAudioStorageMounted = true;
    browserAudioStorageStatus = "Ready";
    refreshBrowserAudioFileState();
    browserAudioStatus = browserAudioSavedInfo.valid ? "Ready" : browserAudioSavedInfo.message;
    Serial.print("LittleFS mounted: total=");
    Serial.print((unsigned long)browserAudioStorageTotalBytes());
    Serial.print(" used=");
    Serial.println((unsigned long)browserAudioStorageUsedBytes());
  } else {
    browserAudioStorageMounted = false;
    browserAudioStorageStatus = "Unavailable";
    browserAudioStatus = "Storage unavailable";
    Serial.println("LittleFS mount failed; browser audio storage unavailable");
  }
}

bool browserAudioStorageReady() {
  return browserAudioStorageMounted;
}

size_t browserAudioStorageTotalBytes() {
  if (!browserAudioStorageMounted) {
    return 0;
  }
  return LittleFS.totalBytes();
}

size_t browserAudioStorageUsedBytes() {
  if (!browserAudioStorageMounted) {
    return 0;
  }
  return LittleFS.usedBytes();
}

size_t browserAudioStorageFreeBytes() {
  size_t total = browserAudioStorageTotalBytes();
  size_t used = browserAudioStorageUsedBytes();
  if (used >= total) {
    return 0;
  }
  return total - used;
}

bool browserAudioFileSaved() {
  return browserAudioStorageMounted && browserAudioSavedInfo.valid;
}

size_t browserAudioFileSize() {
  return browserAudioSavedSize;
}

String browserAudioFileInfoLabel() {
  if (!browserAudioStorageMounted) {
    return "Storage unavailable";
  }
  if (!LittleFS.exists(BROWSER_AUDIO_PATH)) {
    return "No saved sound";
  }
  if (!browserAudioSavedInfo.valid) {
    return browserAudioSavedInfo.message;
  }
  return browserAudioWavInfoLabel(browserAudioSavedInfo);
}

String browserAudioStatusLabel() {
  return browserAudioStatus;
}

bool browserAudioIsPlaying() {
  return browserAudioPlaying;
}

void stopBrowserAudioPlayback(const String &status) {
  if (browserAudioPlaying) {
    browserAudioCopier.end();
    browserAudioDecoder.end();
    if (browserAudioPlaybackFile) {
      browserAudioPlaybackFile.close();
    }
    browserAudioPlaying = false;
  }

  browserAudioStatus = status;
}

bool startBrowserAudioPlayback(String &message) {
  if (!browserAudioStorageMounted) {
    browserAudioStatus = "Storage unavailable";
    message = browserAudioStatus;
    return false;
  }

  if (browserAudioPlaying) {
    stopBrowserAudioPlayback("Stopped");
  }

  refreshBrowserAudioFileState();
  if (!browserAudioSavedInfo.valid) {
    browserAudioStatus = browserAudioSavedInfo.message;
    message = browserAudioStatus;
    return false;
  }

  if (!prepareSpeakerI2sForBrowserAudio(
        browserAudioSavedInfo.sampleRate,
        browserAudioSavedInfo.channels,
        browserAudioSavedInfo.bitsPerSample
      )) {
    browserAudioStatus = String("Speaker unavailable: ") + speakerI2sStatusLabel();
    message = browserAudioStatus;
    return false;
  }

  browserAudioPlaybackFile = LittleFS.open(BROWSER_AUDIO_PATH, "r");
  if (!browserAudioPlaybackFile) {
    browserAudioStatus = "Could not open saved sound";
    message = browserAudioStatus;
    return false;
  }

  browserAudioDecoder.end();
  browserAudioWavDecoder.setConvert8Bit(false);
  browserAudioWavDecoder.setConvert24Bit(false);
  browserAudioDecoder.setDecoder(&browserAudioWavDecoder);
  auto volumeCfg = browserAudioVolume.defaultConfig();
  volumeCfg.sample_rate = browserAudioSavedInfo.sampleRate;
  volumeCfg.channels = browserAudioSavedInfo.channels;
  volumeCfg.bits_per_sample = browserAudioSavedInfo.bitsPerSample;

  // Leave this false at first. Set true only if you need boost above 1.0.
  volumeCfg.allow_boost = true;

  browserAudioVolume.begin(volumeCfg);
  browserAudioVolume.setVolume(BROWSER_AUDIO_VOLUME);

  browserAudioDecoder.setStream(browserAudioVolume);
  if (!browserAudioDecoder.begin()) {
    browserAudioPlaybackFile.close();
    browserAudioStatus = "WAV decoder failed";
    message = browserAudioStatus;
    return false;
  }

  browserAudioCopier.begin(browserAudioDecoder, browserAudioPlaybackFile);
  browserAudioCopier.setDelayOnNoData(0);
  browserAudioPlaying = true;
  browserAudioStatus = "Playing";
  message = "Playing";
  return true;
}

void updateBrowserAudioPlayback() {
  if (!browserAudioPlaying) {
    return;
  }

  uint32_t start = millis();

  while (browserAudioPlaying && millis() - start < 5) {
    size_t copied = browserAudioCopier.copy();

    if (copied == 0) {
      if (!browserAudioPlaybackFile.available()) {
        stopBrowserAudioPlayback("Finished");
      }
      break;
    }
  }
}

bool replaceBrowserAudioFile(String &error) {
  LittleFS.remove(BROWSER_AUDIO_BACKUP_PATH);
  bool hadExistingFile = LittleFS.exists(BROWSER_AUDIO_PATH);

  if (hadExistingFile && !LittleFS.rename(BROWSER_AUDIO_PATH, BROWSER_AUDIO_BACKUP_PATH)) {
    error = "Could not back up existing sound";
    return false;
  }

  if (!LittleFS.rename(BROWSER_AUDIO_TEMP_PATH, BROWSER_AUDIO_PATH)) {
    if (hadExistingFile && LittleFS.exists(BROWSER_AUDIO_BACKUP_PATH)) {
      LittleFS.rename(BROWSER_AUDIO_BACKUP_PATH, BROWSER_AUDIO_PATH);
    }
    error = "Could not save uploaded sound";
    return false;
  }

  if (hadExistingFile) {
    LittleFS.remove(BROWSER_AUDIO_BACKUP_PATH);
  }

  return true;
}

void failBrowserAudioUpload(const String &message) {
  if (browserAudioUploadFile) {
    browserAudioUploadFile.close();
  }
  if (browserAudioStorageMounted) {
    LittleFS.remove(BROWSER_AUDIO_TEMP_PATH);
  }
  browserAudioUploadFailed = true;
  browserAudioUploadFinished = true;
  browserAudioUploadMessage = message;
  browserAudioStatus = message;
  refreshBrowserAudioFileState();
}

String browserAudioResponseJson(bool ok, const String &message) {
  String json = "{";
  json += "\"ok\":";
  if (ok) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"message\":\"";
  json += jsonEscape(message);
  json += "\"";
  json += ",\"audio_status\":\"";
  json += jsonEscape(browserAudioStatusLabel());
  json += "\"";
  json += ",\"audio_playing\":";
  if (browserAudioIsPlaying()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_file_saved\":";
  if (browserAudioFileSaved()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_file_size\":";
  json += String((unsigned long)browserAudioFileSize());
  json += ",\"audio_file_info\":\"";
  json += jsonEscape(browserAudioFileInfoLabel());
  json += "\"";
  json += ",\"audio_storage_ready\":";
  if (browserAudioStorageReady()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_storage_total\":";
  json += String((unsigned long)browserAudioStorageTotalBytes());
  json += ",\"audio_storage_used\":";
  json += String((unsigned long)browserAudioStorageUsedBytes());
  json += ",\"audio_storage_free\":";
  json += String((unsigned long)browserAudioStorageFreeBytes());
  json += "}";
  return json;
}

void sendBrowserAudioResponse(int code, bool ok, const String &message) {
  server.send(code, "application/json", browserAudioResponseJson(ok, message));
}

void handleBrowserAudioUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (browserAudioPlaying) {
      stopBrowserAudioPlayback("Stopped");
    }

    browserAudioUploadBytes = 0;
    browserAudioUploadLimit = 0;
    browserAudioUploadFailed = false;
    browserAudioUploadFinished = false;
    browserAudioUploadMessage = "Uploading";
    browserAudioStatus = "Uploading";

    if (!browserAudioStorageMounted) {
      failBrowserAudioUpload("Storage unavailable");
      return;
    }

    String filename = upload.filename;
    filename.toLowerCase();
    if (!filename.endsWith(".wav")) {
      failBrowserAudioUpload("Choose a .wav file");
      return;
    }

    LittleFS.remove(BROWSER_AUDIO_TEMP_PATH);
    browserAudioUploadLimit = browserAudioStorageFreeBytes();
    if (browserAudioUploadLimit == 0) {
      failBrowserAudioUpload("No free audio storage");
      return;
    }

    browserAudioUploadFile = LittleFS.open(BROWSER_AUDIO_TEMP_PATH, "w");
    if (!browserAudioUploadFile) {
      failBrowserAudioUpload("Could not open upload file");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (browserAudioUploadFailed) {
      return;
    }

    if ((browserAudioUploadBytes + upload.currentSize) > browserAudioUploadLimit) {
      failBrowserAudioUpload("Upload exceeds free audio storage");
      return;
    }

    size_t written = browserAudioUploadFile.write(upload.buf, upload.currentSize);
    browserAudioUploadBytes += written;
    if (written != upload.currentSize) {
      failBrowserAudioUpload("Could not write uploaded data");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (browserAudioUploadFailed) {
      return;
    }

    if (browserAudioUploadFile) {
      browserAudioUploadFile.close();
    }

    if (browserAudioUploadBytes == 0 || upload.totalSize == 0) {
      failBrowserAudioUpload("Uploaded file is empty");
      return;
    }

    BrowserAudioWavInfo uploadedInfo = validateBrowserAudioWavFile(BROWSER_AUDIO_TEMP_PATH);
    if (!uploadedInfo.valid) {
      failBrowserAudioUpload(uploadedInfo.message);
      return;
    }

    String replaceError;
    if (!replaceBrowserAudioFile(replaceError)) {
      failBrowserAudioUpload(replaceError);
      return;
    }

    browserAudioSavedInfo = uploadedInfo;
    browserAudioSavedSize = browserAudioUploadBytes;
    browserAudioUploadFailed = false;
    browserAudioUploadFinished = true;
    browserAudioUploadMessage = "Upload saved";
    browserAudioStatus = "Ready";
    refreshBrowserAudioFileState();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    failBrowserAudioUpload("Upload aborted");
  }
}

void handleBrowserAudioUploadComplete() {
  if (!browserAudioUploadFinished) {
    sendBrowserAudioResponse(400, false, "No upload received");
    return;
  }

  if (browserAudioUploadFailed) {
    sendBrowserAudioResponse(400, false, browserAudioUploadMessage);
    return;
  }

  sendBrowserAudioResponse(200, true, browserAudioUploadMessage);
}

void handleBrowserAudioPlay() {
  String message;
  if (startBrowserAudioPlayback(message)) {
    sendBrowserAudioResponse(200, true, message);
    return;
  }

  sendBrowserAudioResponse(409, false, message);
}

void handleBrowserAudioStop() {
  stopBrowserAudioPlayback("Stopped");
  sendBrowserAudioResponse(200, true, "Stopped");
}
