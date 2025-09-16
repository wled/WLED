#include "wled.h"

#if defined(ESP8266)
#include <ping.h>

/*
 * This usermod performs a ping request to the local IP address every 60 seconds. 
 * By this procedure the net services of WLED remains accessible in some problematic WLAN environments.
 * 
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/wled-dev/WLED/wiki/Add-own-functionality
 * 
 * v2 usermods are class inheritance based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 * 
 * Creating a usermod:
 * This file serves as an example. If you want to create a usermod, it is recommended to use usermod_v2_empty.h from the usermods folder as a template.
 * Please remember to rename the class and file to a descriptive name.
 * You may also use multiple .h and .cpp files.
 * 
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

class FixUnreachableNetServices : public Usermod
{
private:
  //Private class members. You can declare variables and functions only accessible to your usermod here
  unsigned long m_lastTime = 0;

  // declare required variables
  unsigned long m_pingDelayMs = 60000;
  unsigned long m_connectedWiFi = 0;
  ping_option m_pingOpt;
  unsigned int m_pingCount = 0;
  bool m_updateConfig = false;

public:
  //Functions called by WLED

  /**
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup()
  {
    //Serial.println("Hello from my usermod!");
  }

  /**
   * connected() is called every time the WiFi is (re)connected
   * Use it to initialize network interfaces
   */
  void connected()
  {
    //Serial.println("Connected to WiFi!");

    ++m_connectedWiFi;

    // initialize ping_options structure
    memset(&m_pingOpt, 0, sizeof(struct ping_option));
    m_pingOpt.count = 1;
    m_pingOpt.ip = WiFi.localIP();
  }

  /**
   * loop
   */
  void loop()
  {
    if (m_connectedWiFi > 0 && millis() - m_lastTime > m_pingDelayMs)
    {
      ping_start(&m_pingOpt);
      m_lastTime = millis();
      ++m_pingCount;
    }
    if (m_updateConfig)
    {
      serializeConfig();
      m_updateConfig = false;
    }
  }

  /**
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
   * Below it is shown how this could be used for e.g. a light sensor
   */
  void addToJsonInfo(JsonObject &root)
  {
    //this code adds "u":{"&#x26A1; Ping fix pings": m_pingCount} to the info object
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    String uiDomString = "&#x26A1; Ping fix pings<span style=\"display:block;padding-left:25px;\">\
Delay <input type=\"number\" min=\"5\" max=\"300\" value=\"";
    uiDomString += (unsigned long)(m_pingDelayMs / 1000);
    uiDomString += "\" onchange=\"requestJson({PingDelay:parseInt(this.value)});\">sec</span>";

    JsonArray infoArr = user.createNestedArray(uiDomString); //name
    infoArr.add(m_pingCount);                                              //value

    //this code adds "u":{"&#x26A1; Reconnects": m_connectedWiFi - 1} to the info object
    infoArr = user.createNestedArray("&#x26A1; Reconnects"); //name
    infoArr.add(m_connectedWiFi - 1);                        //value
  }

  /**
   * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  void addToJsonState(JsonObject &root)
  {
    root["PingDelay"] = (m_pingDelayMs/1000);
  }

  /**
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  void readFromJsonState(JsonObject &root)
  {
    if (root["PingDelay"] != nullptr)
    {
      m_pingDelayMs = (1000 * max(1UL, min(300UL, root["PingDelay"].as<unsigned long>())));
      m_updateConfig = true;
    }
  }

  /**
   * provide the changeable values
   */
  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject("FixUnreachableNetServices");
    top["PingDelayMs"] = m_pingDelayMs;
  }

  /**
   * restore the changeable values
   */
  bool readFromConfig(JsonObject &root)
  {
    JsonObject top = root["FixUnreachableNetServices"];
    if (top.isNull()) return false;
    m_pingDelayMs = top["PingDelayMs"] | m_pingDelayMs;
    m_pingDelayMs = max(5000UL, min(18000000UL, m_pingDelayMs));
    // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
    return true;
  }

  /**
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   * This could be used in the future for the system to determine whether your usermod is installed.
   */
  uint16_t getId()
  {
    return USERMOD_ID_FIXNETSERVICES;
  }
};

static FixUnreachableNetServices fix_unreachable_net_services;
REGISTER_USERMOD(fix_unreachable_net_services);

#else /* !ESP8266 */
#warning "Usermod FixUnreachableNetServices works only with ESP8266 builds"
#endif

