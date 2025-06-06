# PRAF-Tech

![Praf Pinout](assets/pinout.png)

#### TO-DO

- [x] Repo initial
- [x] Components (Needs deep research for accuracy)
  - [x] ESP32 Micro-controller
  - [x] HC-SR04
  - [x] LED module 3x
  - [x] SD Card Module
  - [x] MAX98357a i2s amplifier module
  - [x] 3W 4-8ohm speaker
  - [x] PCM5102a
  - [x] 16x2 LCD i2c display
- [x] Device pinout diagram
- [x] ESP32 Dev
  - [x] Access dynamic location via IP from providers (uses ipinfo.iso for a more exact location)
  - [x] Fetches OpenWeatherMap current weather by location via net provider IP
  - [x] Uses Gemini AI api using http endpoints with api key
  - [x] Sync AI suggestions by current weather from OpenWeatherMap
  - [x] HC-SR04 ultrasonic sensor functions with ease
  - [x] Linked LEDs to three levels calc via HC-SRO4
  - [x] Integrate Twilio SMS (now httpSMS)
  - [x] Working supabase endpoint for fetching all registered numbers
  - [-] Deciding on weather to use Philsms or Twilio for sms (this is just sad)
- [x] Device workflow
  - [x] Integrate sensor for lvl detection
  - [x] Link LEDs to calc distance via sensor
  - [x] Lock and delays for LEDs for smoother and responsive alerts
  - [x] Read from sdcard for each flood level (uses text file as the MAX hasn't arrived yet)
  - [x] TTS
    - [x] Hard code speech for flood lvl
    - [x] AI suggestion
  - [x] LED effects
    - [x] Loading LED for AI and SMS button
  - [x] Sound effects
    - [x] AI suggestion
    - [x] Flood level
      - [x] Low
      - [x] Medium
      - [x] High
    - [x] Multi thread loading sounds for waiting AI suggestion (fetch the ai suggestions first instead of multithreading)
    - [x] Loading sound for AI and SMS button
    - [x] SMS notification
    - [x] New number registered
  - [x] Integrate OpenWeatherMap.org api
  - [x] IpInfo api for getting cords/location
  - [x] Link openweathermap api & ipinfo api for getting exact weather by location
  - [x] Gemini AI
    - [x] AI suggestion by location, weather and support
    - [-] Different prompts send to Gemini AI for each flood lvl (still thinking about it since the device is gonna be slower if I implement this)
    - [x] Connect to Google tts api
  - [x] SMS (twilio aint giving shit for free so will use httpSMS)
    - [x] Twilio is not actually usable in free tier so sadge
    - [x] httpSMS curl via powershell
    - [x] httpSMS arduino implementation
    - [x] Fetches numbers from supabase
    - [x] Store number entries to an array
    - [x] Live checks for number entries within supabase db
    - [x] Prevents checks when playing alerts & emitting sound from speaker
    - [x] Send sms to all numbers (bulk sms)
    - [x] Proper formatting for msgs
  - [x] Project Design
    - [x] Logo
    - [x] Praf character
    - [x] Sketch
    - [x] Pinout diagram
    - [x] Stickers
    - [x] Full project design (stickers and layout)
  - [+] Didn't update this as im getting done with the project so tons of missing info/todo here
  - [ ] Code optimization
    - [ ] Seperate functions and files
    - [ ] Clean unecessary comments, functions and definitions


