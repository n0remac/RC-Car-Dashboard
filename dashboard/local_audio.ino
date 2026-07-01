#include <LittleFS.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

// ----------------------
// Local uploaded audio
// ----------------------
static const char *LOCAL_AUDIO_PATH = "/sound.wav";
static const char *LOCAL_AUDIO_TEMP_PATH = "/sound.tmp";
static const char *LOCAL_AUDIO_BACKUP_PATH = "/sound.bak";
static const uint32_t LOCAL_AUDIO_MIN_SAMPLE_RATE = 8000;
static const uint32_t LOCAL_AUDIO_MAX_SAMPLE_RATE = 48000;

struct LocalAudioWavInfo {
  bool valid = false;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataLength = 0;
  String message = "No saved sound";
};

uint16_t readLocalAudioLe16(const uint8_t *data);
uint32_t readLocalAudioLe32(const uint8_t *data);
bool localAudioReadExact(File &file, uint8_t *buffer, size_t length);
bool localAudioFourCCEquals(const uint8_t *data, const char *value);
String formatLocalAudioBytes(size_t bytes);
String localAudioWavInfoLabel(const LocalAudioWavInfo &info);
LocalAudioWavInfo validateLocalAudioWavFile(const char *path);
void refreshLocalAudioFileState();
void stopLocalAudioPlayback(const String &status);
bool startLocalAudioPlayback(String &message);
bool replaceLocalAudioFile(String &error);
void failLocalAudioUpload(const String &message);
String localAudioResponseJson(bool ok, const String &message);
void sendLocalAudioResponse(int code, bool ok, const String &message);

bool prepareSpeakerI2sForLocalAudio(
  uint32_t sampleRate,
  uint8_t channels,
  uint8_t bitsPerSample
);

String speakerI2sStatusLabel();

bool localAudioStorageMounted = false;
String localAudioStorageStatus = "Not mounted";
String localAudioStatus = "Not ready";
LocalAudioWavInfo localAudioSavedInfo;
size_t localAudioSavedSize = 0;
bool localAudioPlaying = false;

File localAudioPlaybackFile;
WAVDecoder localAudioWavDecoder;
EncodedAudioStream localAudioDecoder;
StreamCopy localAudioCopier;

File localAudioUploadFile;
size_t localAudioUploadBytes = 0;
size_t localAudioUploadLimit = 0;
bool localAudioUploadFailed = false;
bool localAudioUploadFinished = false;
String localAudioUploadMessage = "No upload received";

uint16_t readLocalAudioLe16(const uint8_t *data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

uint32_t readLocalAudioLe32(const uint8_t *data) {
  return (uint32_t)data[0] |
    ((uint32_t)data[1] << 8) |
    ((uint32_t)data[2] << 16) |
    ((uint32_t)data[3] << 24);
}

bool localAudioReadExact(File &file, uint8_t *buffer, size_t length) {
  return file.read(buffer, length) == (int)length;
}

bool localAudioFourCCEquals(const uint8_t *data, const char *value) {
  return data[0] == value[0] &&
    data[1] == value[1] &&
    data[2] == value[2] &&
    data[3] == value[3];
}

String formatLocalAudioBytes(size_t bytes) {
  if (bytes < 1024) {
    return String((unsigned long)bytes) + " B";
  }
  if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0f, 1) + " KB";
  }
  return String(bytes / 1048576.0f, 2) + " MB";
}

String localAudioWavInfoLabel(const LocalAudioWavInfo &info) {
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

LocalAudioWavInfo validateLocalAudioWavFile(const char *path) {
  LocalAudioWavInfo info;

  if (!localAudioStorageMounted) {
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
  if (!localAudioReadExact(file, header, sizeof(header)) ||
      !localAudioFourCCEquals(header, "RIFF") ||
      !localAudioFourCCEquals(header + 8, "WAVE")) {
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
    if (!localAudioReadExact(file, chunkHeader, sizeof(chunkHeader))) {
      info.message = "Could not read WAV chunk";
      file.close();
      return info;
    }

    uint32_t chunkSize = readLocalAudioLe32(chunkHeader + 4);
    size_t chunkStart = file.position();
    if (chunkSize > (fileSize - chunkStart)) {
      info.message = "WAV chunk exceeds file size";
      file.close();
      return info;
    }
    size_t chunkEnd = chunkStart + chunkSize;

    if (localAudioFourCCEquals(chunkHeader, "fmt ")) {
      if (chunkSize < 16) {
        info.message = "WAV fmt chunk is too small";
        file.close();
        return info;
      }

      uint8_t fmt[16];
      if (!localAudioReadExact(file, fmt, sizeof(fmt))) {
        info.message = "Could not read WAV format";
        file.close();
        return info;
      }

      audioFormat = readLocalAudioLe16(fmt);
      info.channels = readLocalAudioLe16(fmt + 2);
      info.sampleRate = readLocalAudioLe32(fmt + 4);
      byteRate = readLocalAudioLe32(fmt + 8);
      blockAlign = readLocalAudioLe16(fmt + 12);
      info.bitsPerSample = readLocalAudioLe16(fmt + 14);
      foundFmt = true;
    } else if (localAudioFourCCEquals(chunkHeader, "data")) {
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
  if (info.sampleRate < LOCAL_AUDIO_MIN_SAMPLE_RATE ||
      info.sampleRate > LOCAL_AUDIO_MAX_SAMPLE_RATE) {
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

void refreshLocalAudioFileState() {
  localAudioSavedInfo = LocalAudioWavInfo();
  localAudioSavedSize = 0;

  if (!localAudioStorageMounted) {
    return;
  }

  if (!LittleFS.exists(LOCAL_AUDIO_PATH)) {
    localAudioSavedInfo.message = "No saved sound";
    return;
  }

  File file = LittleFS.open(LOCAL_AUDIO_PATH, "r");
  if (file) {
    localAudioSavedSize = file.size();
    file.close();
  }

  localAudioSavedInfo = validateLocalAudioWavFile(LOCAL_AUDIO_PATH);
}

void initLocalAudioStorage() {
  if (LittleFS.begin(true)) {
    localAudioStorageMounted = true;
    localAudioStorageStatus = "Ready";
    refreshLocalAudioFileState();
    localAudioStatus = localAudioSavedInfo.valid ? "Ready" : localAudioSavedInfo.message;
    Serial.print("LittleFS mounted: total=");
    Serial.print((unsigned long)localAudioStorageTotalBytes());
    Serial.print(" used=");
    Serial.println((unsigned long)localAudioStorageUsedBytes());
  } else {
    localAudioStorageMounted = false;
    localAudioStorageStatus = "Unavailable";
    localAudioStatus = "Storage unavailable";
    Serial.println("LittleFS mount failed; local audio storage unavailable");
  }
}

bool localAudioStorageReady() {
  return localAudioStorageMounted;
}

size_t localAudioStorageTotalBytes() {
  if (!localAudioStorageMounted) {
    return 0;
  }
  return LittleFS.totalBytes();
}

size_t localAudioStorageUsedBytes() {
  if (!localAudioStorageMounted) {
    return 0;
  }
  return LittleFS.usedBytes();
}

size_t localAudioStorageFreeBytes() {
  size_t total = localAudioStorageTotalBytes();
  size_t used = localAudioStorageUsedBytes();
  if (used >= total) {
    return 0;
  }
  return total - used;
}

bool localAudioFileSaved() {
  return localAudioStorageMounted && localAudioSavedInfo.valid;
}

size_t localAudioFileSize() {
  return localAudioSavedSize;
}

String localAudioFileInfoLabel() {
  if (!localAudioStorageMounted) {
    return "Storage unavailable";
  }
  if (!LittleFS.exists(LOCAL_AUDIO_PATH)) {
    return "No saved sound";
  }
  if (!localAudioSavedInfo.valid) {
    return localAudioSavedInfo.message;
  }
  return localAudioWavInfoLabel(localAudioSavedInfo);
}

String localAudioStatusLabel() {
  return localAudioStatus;
}

bool localAudioIsPlaying() {
  return localAudioPlaying;
}

void stopLocalAudioPlayback(const String &status) {
  if (localAudioPlaying) {
    localAudioCopier.end();
    localAudioDecoder.end();
    if (localAudioPlaybackFile) {
      localAudioPlaybackFile.close();
    }
    localAudioPlaying = false;
  }

  localAudioStatus = status;
}

bool startLocalAudioPlayback(String &message) {
  if (!localAudioStorageMounted) {
    localAudioStatus = "Storage unavailable";
    message = localAudioStatus;
    return false;
  }

  if (localAudioPlaying) {
    stopLocalAudioPlayback("Stopped");
  }

  refreshLocalAudioFileState();
  if (!localAudioSavedInfo.valid) {
    localAudioStatus = localAudioSavedInfo.message;
    message = localAudioStatus;
    return false;
  }

  if (!prepareSpeakerI2sForLocalAudio(
        localAudioSavedInfo.sampleRate,
        localAudioSavedInfo.channels,
        localAudioSavedInfo.bitsPerSample
      )) {
    localAudioStatus = String("Speaker unavailable: ") + speakerI2sStatusLabel();
    message = localAudioStatus;
    return false;
  }

  localAudioPlaybackFile = LittleFS.open(LOCAL_AUDIO_PATH, "r");
  if (!localAudioPlaybackFile) {
    localAudioStatus = "Could not open saved sound";
    message = localAudioStatus;
    return false;
  }

  localAudioDecoder.end();
  localAudioWavDecoder.setConvert8Bit(false);
  localAudioWavDecoder.setConvert24Bit(false);
  localAudioDecoder.setDecoder(&localAudioWavDecoder);
  localAudioDecoder.setStream(speakerI2s);
  if (!localAudioDecoder.begin()) {
    localAudioPlaybackFile.close();
    localAudioStatus = "WAV decoder failed";
    message = localAudioStatus;
    return false;
  }

  localAudioCopier.begin(localAudioDecoder, localAudioPlaybackFile);
  localAudioCopier.setDelayOnNoData(0);
  localAudioPlaying = true;
  localAudioStatus = "Playing";
  message = "Playing";
  return true;
}

void updateLocalAudioPlayback() {
  if (!localAudioPlaying) {
    return;
  }

  uint32_t start = millis();

  while (localAudioPlaying && millis() - start < 5) {
    size_t copied = localAudioCopier.copy();

    if (copied == 0) {
      if (!localAudioPlaybackFile.available()) {
        stopLocalAudioPlayback("Finished");
      }
      break;
    }
  }
}

bool replaceLocalAudioFile(String &error) {
  LittleFS.remove(LOCAL_AUDIO_BACKUP_PATH);
  bool hadExistingFile = LittleFS.exists(LOCAL_AUDIO_PATH);

  if (hadExistingFile && !LittleFS.rename(LOCAL_AUDIO_PATH, LOCAL_AUDIO_BACKUP_PATH)) {
    error = "Could not back up existing sound";
    return false;
  }

  if (!LittleFS.rename(LOCAL_AUDIO_TEMP_PATH, LOCAL_AUDIO_PATH)) {
    if (hadExistingFile && LittleFS.exists(LOCAL_AUDIO_BACKUP_PATH)) {
      LittleFS.rename(LOCAL_AUDIO_BACKUP_PATH, LOCAL_AUDIO_PATH);
    }
    error = "Could not save uploaded sound";
    return false;
  }

  if (hadExistingFile) {
    LittleFS.remove(LOCAL_AUDIO_BACKUP_PATH);
  }

  return true;
}

void failLocalAudioUpload(const String &message) {
  if (localAudioUploadFile) {
    localAudioUploadFile.close();
  }
  if (localAudioStorageMounted) {
    LittleFS.remove(LOCAL_AUDIO_TEMP_PATH);
  }
  localAudioUploadFailed = true;
  localAudioUploadFinished = true;
  localAudioUploadMessage = message;
  localAudioStatus = message;
  refreshLocalAudioFileState();
}

String localAudioResponseJson(bool ok, const String &message) {
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
  json += jsonEscape(localAudioStatusLabel());
  json += "\"";
  json += ",\"audio_playing\":";
  if (localAudioIsPlaying()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_file_saved\":";
  if (localAudioFileSaved()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_file_size\":";
  json += String((unsigned long)localAudioFileSize());
  json += ",\"audio_file_info\":\"";
  json += jsonEscape(localAudioFileInfoLabel());
  json += "\"";
  json += ",\"audio_storage_ready\":";
  if (localAudioStorageReady()) {
    json += "true";
  } else {
    json += "false";
  }
  json += ",\"audio_storage_total\":";
  json += String((unsigned long)localAudioStorageTotalBytes());
  json += ",\"audio_storage_used\":";
  json += String((unsigned long)localAudioStorageUsedBytes());
  json += ",\"audio_storage_free\":";
  json += String((unsigned long)localAudioStorageFreeBytes());
  json += "}";
  return json;
}

void sendLocalAudioResponse(int code, bool ok, const String &message) {
  server.send(code, "application/json", localAudioResponseJson(ok, message));
}

void handleLocalAudioUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (localAudioPlaying) {
      stopLocalAudioPlayback("Stopped");
    }

    localAudioUploadBytes = 0;
    localAudioUploadLimit = 0;
    localAudioUploadFailed = false;
    localAudioUploadFinished = false;
    localAudioUploadMessage = "Uploading";
    localAudioStatus = "Uploading";

    if (!localAudioStorageMounted) {
      failLocalAudioUpload("Storage unavailable");
      return;
    }

    String filename = upload.filename;
    filename.toLowerCase();
    if (!filename.endsWith(".wav")) {
      failLocalAudioUpload("Choose a .wav file");
      return;
    }

    LittleFS.remove(LOCAL_AUDIO_TEMP_PATH);
    localAudioUploadLimit = localAudioStorageFreeBytes();
    if (localAudioUploadLimit == 0) {
      failLocalAudioUpload("No free audio storage");
      return;
    }

    localAudioUploadFile = LittleFS.open(LOCAL_AUDIO_TEMP_PATH, "w");
    if (!localAudioUploadFile) {
      failLocalAudioUpload("Could not open upload file");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (localAudioUploadFailed) {
      return;
    }

    if ((localAudioUploadBytes + upload.currentSize) > localAudioUploadLimit) {
      failLocalAudioUpload("Upload exceeds free audio storage");
      return;
    }

    size_t written = localAudioUploadFile.write(upload.buf, upload.currentSize);
    localAudioUploadBytes += written;
    if (written != upload.currentSize) {
      failLocalAudioUpload("Could not write uploaded data");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (localAudioUploadFailed) {
      return;
    }

    if (localAudioUploadFile) {
      localAudioUploadFile.close();
    }

    if (localAudioUploadBytes == 0 || upload.totalSize == 0) {
      failLocalAudioUpload("Uploaded file is empty");
      return;
    }

    LocalAudioWavInfo uploadedInfo = validateLocalAudioWavFile(LOCAL_AUDIO_TEMP_PATH);
    if (!uploadedInfo.valid) {
      failLocalAudioUpload(uploadedInfo.message);
      return;
    }

    String replaceError;
    if (!replaceLocalAudioFile(replaceError)) {
      failLocalAudioUpload(replaceError);
      return;
    }

    localAudioSavedInfo = uploadedInfo;
    localAudioSavedSize = localAudioUploadBytes;
    localAudioUploadFailed = false;
    localAudioUploadFinished = true;
    localAudioUploadMessage = "Upload saved";
    localAudioStatus = "Ready";
    refreshLocalAudioFileState();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    failLocalAudioUpload("Upload aborted");
  }
}

void handleLocalAudioUploadComplete() {
  if (!localAudioUploadFinished) {
    sendLocalAudioResponse(400, false, "No upload received");
    return;
  }

  if (localAudioUploadFailed) {
    sendLocalAudioResponse(400, false, localAudioUploadMessage);
    return;
  }

  sendLocalAudioResponse(200, true, localAudioUploadMessage);
}

void handleLocalAudioPlay() {
  String message;
  if (startLocalAudioPlayback(message)) {
    sendLocalAudioResponse(200, true, message);
    return;
  }

  sendLocalAudioResponse(409, false, message);
}

void handleLocalAudioStop() {
  stopLocalAudioPlayback("Stopped");
  sendLocalAudioResponse(200, true, "Stopped");
}
