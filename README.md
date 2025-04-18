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
- [x] Device pinout diagram
- [ ] ESP32 Dev
  - [x] Access dynamic location via IP from providers (uses ipinfo.iso for a more exact location)
  - [x] Fetches OpenWeatherMap current weather by location via net provider IP
  - [x] Uses Gemini AI api using http endpoints with api key
  - [x] Sync AI suggestions by current weather from OpenWeatherMap
  - [x] HC-SR04 ultrasonic sensor functions with ease
  - [x] Linked LEDs to three levels calc via HC-SRO4
  - [x] Integrate Twilio SMS
  - [x] Working supabase endpoint for fetching all registered numbers
  - [ ] Deciding on weather to use Philsms or Twilio for sms
  - [ ] 
- [ ] Device workflow
  - [ ] Integrate sensor for lvl detection
  - [ ] Link LEDs to calc distance via sensor
  - [ ] Read from sdcard for each flood level (uses text file as the MAX hasn't arrived yet)
  - [ ] Hard code text for flood lvl
  - [ ] Different prompts send to Gemini AI for each flood lvl
  - [ ] SMS for each resident numbers
  - [ ] Lock SMS to two number
    - [ ] One for receiving alerts for each flood level and ai suggestions
    - [ ] Other one for getting the location and daily weather update w/ai suggestions for the current weather and possibilty of flooding
  - [ ] Proper formatting for msgs
