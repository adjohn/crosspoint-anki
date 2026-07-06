// Host-side test shim for Arduino's String (WString.h).
// Minimal std::string-backed implementation: just enough API for
// SM2.cpp, Card.h, Progress.h and ArduinoJson's generic string adapters
// (c_str()/length() for reading, operator=(const char*)/concat() for the
// ::String serialization writer). Test-only; never compiled on-device.
#pragma once

#include <string>
#include <cstring>

class String {
public:
    String() = default;
    String(const char* s) : buf(s ? s : "") {}
    String(const std::string& s) : buf(s) {}
    String(char c) : buf(1, c) {}
    explicit String(int v) : buf(std::to_string(v)) {}
    explicit String(unsigned int v) : buf(std::to_string(v)) {}
    explicit String(long v) : buf(std::to_string(v)) {}
    explicit String(unsigned long v) : buf(std::to_string(v)) {}

    String& operator=(const char* s) {
        buf = s ? s : "";
        return *this;
    }

    const char* c_str() const { return buf.c_str(); }
    size_t length() const { return buf.size(); }
    bool isEmpty() const { return buf.empty(); }

    // ArduinoJson's Writer<::String> appends via concat(const char*)
    bool concat(const char* s) {
        if (!s) return false;
        buf += s;
        return true;
    }

    String operator+(const String& rhs) const { return String(buf + rhs.buf); }
    String operator+(const char* rhs) const { return String(buf + (rhs ? rhs : "")); }
    friend String operator+(const char* lhs, const String& rhs) {
        return String(std::string(lhs ? lhs : "") + rhs.buf);
    }

    bool operator==(const String& rhs) const { return buf == rhs.buf; }
    bool operator==(const char* rhs) const { return buf == (rhs ? rhs : ""); }
    bool operator!=(const String& rhs) const { return buf != rhs.buf; }
    bool operator!=(const char* rhs) const { return !(*this == rhs); }
    bool operator<(const String& rhs) const { return buf < rhs.buf; }  // std::map key

private:
    std::string buf;
};
