#include "wled.h"

#include "DeadlineUsermod.h"
#include "DeadlineTrophy.h"

static DeadlineUsermod deadlineUsermod;
REGISTER_USERMOD(deadlineUsermod);

void DeadlineUsermod::readRgbValues(bool printDebug)
{
  // it seems that in the strip _pixels[], the segment opacity is cared for, but overall brightness is independent.
  // this is confusing, so let's implement it as flexible as we can for now, don't know what's good yet. sad.
  fract8 masterFader = static_cast<fract8>(bri);

  int s;
  // it seems that we can only use strip.getPixelColor(i), not just seg->getPixelColor or bus->getPixelColor()
  // that means, we loop over the whole matrix and skip the gaps there.
  for (size_t i=0; i < strip.getLengthTotal(); i++) {
    size_t index = strip.getMappedPixelIndex(i);
    if (index >= DeadlineTrophy::N_LEDS_TOTAL) {
        continue;
    }
    uint32_t color = strip.getPixelColor(i);
    uint8_t r = R(color);
    uint8_t g = G(color);
    uint8_t b = B(color);
    // development made use of these brightness-scaling options, but not needed to change anymore
    if (applyMasterBrightnessToRgbValues) {
        if (applyMasterBrightnessWithScaleVideo) {
            r = scale8_video(r, masterFader);
            g = scale8_video(g, masterFader);
            b = scale8_video(b, masterFader);
        } else {
            r = scale8(r, masterFader);
            g = scale8(g, masterFader);
            b = scale8(b, masterFader);
        }
    } else {
        // if the strip is off, also make the packets dark.
        r = bri == 0 ? 0 : r;
        g = bri == 0 ? 0 : g;
        b = bri == 0 ? 0 : b;
    }
    s = 3 * index;
    rgbValues[s++] = r;
    rgbValues[s++] = g;
    rgbValues[s++] = b;
  }

  if (printDebug) {
    for (int s = 0; s < DeadlineUsermod::N_RGB_VALUES; s+=3) {
        DEBUG_PRINTF("[QM_DEBUG_RGB] LED %d: (%d, %d, %d)\n", s / 3, rgbValues[s], rgbValues[s+1], rgbValues[s+2]);
    }
    DEBUG_PRINTF("[QM_DEBUG_RGB] was scaled? %d; with video-scaling? %d; Master Brightness %d\n",
                 applyMasterBrightnessToRgbValues, applyMasterBrightnessWithScaleVideo, masterFader
    );
  }
}

void DeadlineUsermod::sendTrophyUdp()
{
  // Cannot start the sender in setup() because the network stuff is then not initialized yet.
  if (!udpSenderConnected) {
    if (udpSenderPort == 0) {
      return;
    }
    udpSenderConnected = udpSender.begin(udpSenderPort);

    DEBUG_PRINTF("[DEADLINE_TROPHY] UDP Sender Port: %d - Connected? %d\n", udpSenderPort, udpSenderConnected);

    if (!udpSenderConnected) {
      return;
    }
  }
  else if (udpSenderPort == 0) {
    udpSender.stop();
  }

  bool doLog = doDebugLogUdp || doOneVerboseDebugLogUdp;

  if (!udpSenderEnabled) {
    return;
  }

  // udpSenderIntervalSec must be set > 0. else the ESP32 sender queue can get clogged.
  if (sendUdpInSec > 0.) {
    sendUdpInSec -= elapsedSec;
    return;
  }
  sendUdpInSec += udpSenderIntervalSec;

  // QM Note: Using DRGB protocol, i.e. limited to 490
  // This is enough for the DL Trophy (172 LEDs).
  // If this is decoupled and more are needed -> use DNRGB
  const int packetSize = 2 + DeadlineUsermod::N_RGB_VALUES;
  udpPacket[0] = 2; // protocol index (2 = DRGB, 4 = DNRGB)
  udpPacket[1] = 255; // timeout in ms = forever
  memcpy(udpPacket + 2, rgbValues, DeadlineUsermod::N_RGB_VALUES);

  udpSender.beginPacket(udpSenderIp, udpSenderPort);
  udpSender.write(udpPacket, packetSize);
  udpSender.endPacket();

  if (doLog) {
    DEBUG_PRINTF("[SENT UDP PACKAGE] to %s:%d, size %d\n", udpSenderIp.toString().c_str(), udpSenderPort, packetSize);
  }

  if (doOneVerboseDebugLogUdp) {
    for (int i=0; i < DeadlineTrophy::N_LEDS_TOTAL; i++) {
        int s = 2 + 3*i;
        DEBUG_PRINTF("[DEBUG-UDP] index %d, color [%d, %d, %d]\n", i, udpPacket[s], udpPacket[s+1], udpPacket[s+2]);
    }
  }

  doOneVerboseDebugLogUdp = false;
}

bool DeadlineUsermod::sendLiveview(AsyncWebSocketClient *wsc)
{
    const int HEADER_VALUES = 4;
    // somewhat duplicated from ws.cpp sendLiveLedsWs(), but distinction is more manageable that way
    AsyncWebSocketBuffer wsBuf(HEADER_VALUES + N_RGB_VALUES);
    if (!wsBuf) return false; //out of memory
    uint8_t* buffer = reinterpret_cast<uint8_t*>(wsBuf.data());
    if (!buffer) return false; //out of memory

    buffer[0] = 'L';
    buffer[1] = 2;
    buffer[2] = Segment::maxWidth;
    buffer[3] = Segment::maxHeight;
    memcpy(buffer + HEADER_VALUES, rgbValues, N_RGB_VALUES);

    wsc->binary(std::move(wsBuf));
    return true;
}

bool DeadlineUsermod::parseNotifyPacket(const uint8_t *udpIn) {
    // version is on 11 because at least _this_ matches the common WLED packets (as well as the first two bytes)
    int version = udpIn[11];
    int subVersion = udpIn[12];
    if (version != DeadlineUsermod::customPacketVersion) {
        return false;
    }
    if (subVersion != 0) {
        DEBUG_PRINTF("[DEADLINE_TROPHY] Got a UDP package of our version %d but unknown subversion %d\n", version, subVersion);
        return false;
    }

    // this is our definition of "subversion 0"
    bool doReset = udpIn[2] == 1;
    bool applyBrightness = udpIn[3] == 1;
    uint8_t brightness = udpIn[4];
    uint8_t applySegmentOpacityTo = udpIn[5];
    uint8_t segmentOpacity = udpIn[6];
    uint8_t applyFxIndexTo = udpIn[7];
    uint8_t fxIndex = udpIn[8];
    uint8_t applyFxSpeedTo = udpIn[9];
    uint8_t fxSpeed = udpIn[10];

    bool changesToSegments =
        applySegmentOpacityTo > 0 ||
        applyFxIndexTo > 0 ||
        applyFxSpeedTo > 0;

    if (!(doReset || changesToSegments || applyBrightness)) {
        return false;
    }

    if (applyBrightness) {
        bri = brightness;
        turnOnAtBoot = bri > 0;
    }
    if (changesToSegments) {
        if (currentPlaylist >= 0) {
            unloadPlaylist();
        }
        strip.suspend();
        strip.waitForIt();
        for (int s = 0; s < strip.getSegmentsNum(); s++) {
            uint8_t isAffected = 1 << s;
            bool applySegmentOpacity = applySegmentOpacityTo & isAffected;
            bool applyFxIndex = applyFxIndexTo & isAffected;
            bool applyFxSpeed = applyFxSpeedTo & isAffected;
            Segment& seg = strip.getSegment(s);
            seg.on = true;
            seg.freeze = false;
            if (applySegmentOpacity) {
                seg.setOpacity(segmentOpacity);
            }
            if (applyFxIndex) {
                seg.setMode(fxIndex, true);
            }
            if (applyFxSpeed) {
                seg.speed = fxSpeed;
            }
        }
        strip.trigger();
        strip.resume();
    }

    if (doReset) {
        strip.resetTimebase();
        strip.restartRuntime();
    }

    return true;
}


void DeadlineUsermod::addToJsonState(JsonObject& obj)
{
    JsonObject usermod = obj[FPSTR("DL")];
    if (usermod.isNull()) {
    usermod = obj.createNestedObject(FPSTR("DL"));
    }
    usermod["xBri"] = applyMasterBrightnessToRgbValues;
    usermod["xVid"] = applyMasterBrightnessWithScaleVideo;
}

void DeadlineUsermod::readFromJsonState(JsonObject& obj)
{
    JsonObject usermod = obj[FPSTR("DL")];
    DEBUG_PRINTF("[QM_DEBUG] JSON null? %d\n", usermod.isNull());
    if (!usermod.isNull()) {
        auto xBri = usermod[FPSTR("xBri")];
        bool aBri = xBri.is<bool>() ? xBri.as<bool>()
            : applyMasterBrightnessToRgbValues;
        auto xVid = usermod[FPSTR("xVid")];
        bool aVid = xVid.is<bool>() ? xVid.as<bool>()
            : applyMasterBrightnessWithScaleVideo;
        DEBUG_PRINTF("[QM_DEBUG] readFromJsonState %d %d / %d %d\n",
                     applyMasterBrightnessToRgbValues, applyMasterBrightnessWithScaleVideo, aBri, aVid);
        if (aBri != applyMasterBrightnessToRgbValues || aVid != applyMasterBrightnessWithScaleVideo) {
            applyMasterBrightnessToRgbValues = aBri;
            applyMasterBrightnessWithScaleVideo = aVid;
            readRgbValues(true);
        }
    }
}

void DeadlineUsermod::addToConfig(JsonObject& doc)
{
    JsonObject top = doc.createNestedObject(F("DeadlineTrophy"));
    // fun thing, fillUMPins() will look for entries in these "pin" arrays. deal with it.
    auto pins = top.createNestedArray("pin");
    pins.add(PIN_LOGOTHERM);
    pins.add(inputVoltagePin());

    auto limV = top.createNestedObject("minVoltage");
    limV[F("threshold")] = limit_inputVoltageThreshold;
    limV[F("attenuate")] = attenuateByV_attackFactorWhenCritical;
    limV[F("release")] = attenuateByV_releasePerSecond;
    auto limT = top.createNestedObject("maxTemp");
    limT[F("critical")] = limit_criticalTempDegC;
    limT[F("attenuate")] = attenuateByT_attackFactorWhenCritical,
    limT[F("release")] = attenuateByT_releasePerSecond;

    // Where the Simulator listens for UDP messages
    auto sim = top.createNestedObject("simulator");
    sim["ip"] = static_cast<uint32_t>(udpSenderIp);
    sim["port"] = udpSenderPort;
}

void DeadlineUsermod::appendConfigData(Print& settingsScript)
{
    settingsScript.print(F("let ipInputs = d.getElementsByName('DeadlineTrophy:simulator:ip');"));
    auto ip = static_cast<uint32_t>(udpSenderIp);
    settingsScript.printf_P(
        "ipInputs[1].insertAdjacentHTML('afterend', \""
            "<input name='DeadlineTrophy:simulator:ipOct1' type='number' class='s' min='0' max='255' value='%d' required>."
            "<input name='DeadlineTrophy:simulator:ipOct2' type='number' class='s' min='0' max='255' value='%d' required>."
            "<input name='DeadlineTrophy:simulator:ipOct3' type='number' class='s' min='0' max='255' value='%d' required>."
            "<input name='DeadlineTrophy:simulator:ipOct4' type='number' class='s' min='0' max='255' value='%d' required>\");\n"
        "ipInputs.forEach(e => {e.style.display = 'none';});\n",
        ip & 0xFF,
        (ip>>8) & 0xFF,
        (ip>>16) & 0xFF,
        (ip>>24) & 0xFF
    );
    settingsScript.print(F("addInfo('DeadlineTrophy:simulator:port',1,'0 = off');"));

    settingsScript.print(F("addInfo('DeadlineTrophy:pin[]',0,'Logo Temperature','LogoTherm');"));
    settingsScript.print(F("addInfo('DeadlineTrophy:pin[]',1,'Common Voltage','VCC');"));

    settingsScript.print(F("addInfo('DeadlineTrophy:minVoltage:threshold',1,'required minimum V<sub>CC</sub>');"));
    settingsScript.print(F("addInfo('DeadlineTrophy:minVoltage:attenuate',1,'dim by factor when below threshold');"));
    settingsScript.print(F("addInfo('DeadlineTrophy:minVoltage:release',1,'slowly go back to 1 (perSec) if safe');"));

    settingsScript.print(F("addInfo('DeadlineTrophy:maxTemp:critical',1,'critical Temperature in °C');"));
    settingsScript.print(F("addInfo('DeadlineTrophy:maxTemp:attenuate',1,'dim by factor when above critical');"));
    settingsScript.print(F("addInfo('DeadlineTrophy:maxTemp:release',1,'slowly go back to 1 (perSec) if safe');"));

    // qm210: was hard in xml.cpp at first, but I guess this belongs rather here.
    settingsScript.print(F("/* USERMOD DEADLINE */ "));
    settingsScript.print(F("d.DEADLINE_TROPHY_MOD = true;"));
    settingsScript.print(F("d.DEADLINE_VALUES = "));
    settingsScript.print(buildSocketJson());
    settingsScript.print(F(";"));
    settingsScript.print(F("/* END USERMOD DEADLINE */ "));
}

bool DeadlineUsermod::readFromConfig(JsonObject& root)
{
    JsonObject top = root[FPSTR("DeadlineTrophy")];
    if (top.isNull()) {
        DEBUG_PRINTLN(F("DeadlineTrophy: No config found. (Using defaults.)"));
        return false;
    }

    JsonObject sim = top["simulator"];
    if (!sim.isNull()) {
        udpSenderPort = sim["port"];

        if (sim.containsKey("ipOct1")) {
            // this is the WebUI implementation
            udpSenderIp = IPAddress(
                atoi(sim["ipOct1"]),
                atoi(sim["ipOct2"]),
                atoi(sim["ipOct3"]),
                atoi(sim["ipOct4"])
            );
        } else {
            // this is the persisted key
            udpSenderIp = IPAddress(static_cast<uint32_t>(sim["ip"]));
        }
    }

    JsonObject limV = top["minVoltage"];
    if (!limV.isNull()) {
        limit_inputVoltageThreshold = limV[F("threshold")] | limit_inputVoltageThreshold;
        attenuateByV_attackFactorWhenCritical = limV[F("attenuate")] | attenuateByV_attackFactorWhenCritical;
        attenuateByV_releasePerSecond = limV[F("release")] | attenuateByV_releasePerSecond;
    }
    JsonObject limT = top["maxTemp"];
    if (!limT.isNull()) {
        limit_criticalTempDegC =limT[F("critical")] | limit_criticalTempDegC;
        attenuateByT_attackFactorWhenCritical = limT[F("attenuate")] | attenuateByT_attackFactorWhenCritical;
        attenuateByT_releasePerSecond = limT[F("release")] | attenuateByT_releasePerSecond;
    }

    return true;
}
