#include <iostream>
#include <string>
#include <mutex>

class Logger {
public:
    enum Level { ERROR_LVL, WARNING_LVL, INFO_LVL, VERBOSE_LVL };

    // Set the current log level
    static void setLogLevel(Level level) {
        std::lock_guard<std::mutex> lock(logMutex);
        currentLevel = level;
    }

    // Log a message if the level is equal to or higher than the current log level
    static void log(Level level, const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (level <= currentLevel) {
            std::cout << "[" << levelToString(level) << "] " << message << std::endl;
        }
    }

    // Stream-based logging
    template <typename T>
    Logger& operator<<(const T& msg) {
        std::lock_guard<std::mutex> lock(logMutex);
        buffer << msg;
        return *this;
    }
   /* Logger& operator<<(decltype(std::hex) manip) {
        buffer << manip; 
        return *this;
    }
    Logger& operator<<(decltype(std::dec) manip) {
        buffer << manip;
        return *this;
    }*/
    Logger& operator<<(decltype(std::endl<char,
        std::char_traits<char>>)) 
    {
        return *this << '\n';
    }

    // Flush the buffer to the output stream
    void flush(Level level) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (level <= currentLevel) {
            std::cout << buffer.str() << std::endl;
            buffer.str(""); // Clear the buffer
            buffer.clear(); // Clear any error flags
        }
    }

private:
    static std::string levelToString(Level level) {
        switch (level) {
            case ERROR_LVL: return "ERROR";
            case WARNING_LVL: return "WARNING";
            case INFO_LVL: return "INFO";
            default: return "UNKNOWN";
        }
    }

    static Level currentLevel;
    static std::mutex logMutex;
    std::ostringstream buffer;
};