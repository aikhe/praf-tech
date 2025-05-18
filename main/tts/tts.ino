/**
 * @file tts.ino
 * @brief Decode MP3 stream from Google TTS (mobile endpoint) in ≤200‑char chunks
 *        and output to PCM5102A via I2S
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include <WiFi.h>

// PCM5102A I2S pin configuration
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC  26

// WiFi credentials
const char* ssid     = "TK-gacura";
const char* password = "gisaniel924";

// Google TTS URL template
Str queryTemplate("http://translate.google.com/translate_tts"
                  "?ie=UTF-8&tl=%1&client=tw-ob&ttsspeed=%2&q=%3");

URLStream url(ssid, password);
I2SStream i2s;
EncodedAudioStream dec(&i2s, new MP3DecoderHelix());
StreamCopy copier(dec, url);

/// Build a single‐chunk TTS URL
String makeTTSUrl(const String& chunk, const char* lang = "tl", const char* speed = "1") {
  Str q = queryTemplate;
  q.replace("%1", lang);
  q.replace("%2", speed);
  Str enc(chunk.c_str());
  enc.urlEncode();
  q.replace("%3", enc.c_str());
  return String(q.c_str());
}

/// Split `text` into chunks of max `maxLen`, breaking at spaces when possible
void splitText(const String& text, size_t maxLen, std::vector<String>& outChunks) {
  size_t start = 0, len = text.length();
  while (start < len) {
    // Determine end idx
    size_t end = start + maxLen;
    if (end >= len) {
      end = len;
    } else {
      // backtrack to last space
      size_t lastSpace = text.lastIndexOf(' ', end);
      if (lastSpace > start) end = lastSpace;
    }
    outChunks.push_back(text.substring(start, end));
    // skip any spaces at beginning of next chunk
    start = end;
    while (start < len && text.charAt(start) == ' ') start++;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Configure I2S → PCM5102A
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck         = I2S_BCLK;
  cfg.pin_ws          = I2S_LRC;
  cfg.pin_data        = I2S_DOUT;
  cfg.bits_per_sample = 16;
  cfg.channels        = 2;
  cfg.sample_rate     = 44100;
  cfg.channel_format  = I2SChannelSelect::Stereo;

  Serial.println("Initializing I2S...");
  i2s.begin(cfg);

  Serial.println("Initializing decoder...");
  dec.begin();

  // Your long TTS text
  String fullText =
    "PRAF Technology Weather Update: Maulap ang kalangitan sa Caloocan, "
    "32.3°C (parang 39.3°C ang init) at 73% ang humidity, kaya mataas ang "
    "posibilidad ng pagbaha. Mag-ingat at iwasan ang pagtungo sa mga mabababang "
    "lugar. Laging magdala ng payong o kapote!";

  // Split into ≤200‐char chunks
  std::vector<String> chunks;
  splitText(fullText, 200, chunks);
  Serial.printf("Text divided into %u chunk(s)\n\n", chunks.size());

  // Stream each chunk in order
  for (size_t i = 0; i < chunks.size(); ++i) {
    String urlStr = makeTTSUrl(chunks[i], "tl", "1");
    Serial.printf("Chunk %u URL:\n%s\n", i + 1, urlStr.c_str());

    // Start streaming this chunk
    url.begin(urlStr.c_str(), "audio/mp3");
    Serial.printf("Playing chunk %u/%u...\n", i + 1, chunks.size());

    // Copy until this chunk’s MP3 stream ends
    while (copier.copy() > 0) {
      // keep pumping audio
    }
    Serial.printf("Chunk %u done.\n\n", i + 1);
  }

  Serial.println("All chunks played!");
}

void loop() {
  // Nothing to do here—everything’s handled in setup
}
