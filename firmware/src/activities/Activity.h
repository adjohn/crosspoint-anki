#pragma once

#include <string>
#include <utility>
#include <Arduino.h>

class MappedInputManager;
class GfxRenderer;

class Activity {
public:
  // Navigation targets an activity can request; main.cpp performs the switch
  // after loop() returns so an activity is never deleted from inside its own loop().
  enum class NavTarget { None, MainMenu, DeckList, Review, SessionComplete, Upload, ExitApp };

  struct NavRequest {
    NavTarget target = NavTarget::None;
    String deckId;      // Review
    int reviewed = 0;   // SessionComplete
    int remaining = 0;  // SessionComplete
  };

protected:
  String name;
  GfxRenderer& renderer;
  MappedInputManager& input;

  void requestNav(NavTarget target, String deckId = "", int reviewed = 0, int remaining = 0) {
    navRequest.target = target;
    navRequest.deckId = std::move(deckId);
    navRequest.reviewed = reviewed;
    navRequest.remaining = remaining;
  }

private:
  NavRequest navRequest;

public:
  Activity(String name, GfxRenderer& renderer, MappedInputManager& input)
      : name(std::move(name)), renderer(renderer), input(input) {}
  virtual ~Activity() = default;
  virtual void onEnter() { Serial.printf("[%lu] [ACT] Entering activity: %s\n", millis(), name.c_str()); }
  virtual void onExit() { Serial.printf("[%lu] [ACT] Exiting activity: %s\n", millis(), name.c_str()); }
  virtual void loop() {}
  // Activities that must not be interrupted by auto-sleep (e.g. while the
  // WiFi upload server is running) return true; main.cpp checks each frame.
  virtual bool keepAwake() const { return false; }

  NavRequest consumeNavRequest() {
    NavRequest request = navRequest;
    navRequest = NavRequest();
    return request;
  }
};
