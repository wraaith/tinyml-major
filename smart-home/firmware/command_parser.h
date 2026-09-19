/*
 * Command Parser Module
 * Parses BLE input into structured commands
 * 
 * Command Format: <CMD>:<PARAM1>,<PARAM2>\n
 * Examples:
 *   ON:1\n          → Turn ON appliance 1
 *   OFF:2\n         → Turn OFF appliance 2
 *   DIM:1,50\n      → Dim appliance 1 to 50%
 *   TOGGLE:3\n      → Toggle appliance 3
 *   STATUS\n        → Get status of all appliances
 *   HELP\n          → Show available commands
 *   SCENE:2\n       → Activate scene 2
 */

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>
#include <ctype.h>

// ============================================================================
// COMMAND ENUMERATION
// ============================================================================

enum CommandType {
  CMD_UNKNOWN,
  CMD_ON,
  CMD_OFF,
  CMD_DIM,
  CMD_TOGGLE,
  CMD_STATUS,
  CMD_HELP,
  CMD_SCENE,
  CMD_SCHEDULE,
  CMD_EMERGENCY_STOP
};

// ============================================================================
// PARSED COMMAND STRUCTURE
// ============================================================================

struct ParsedCommand {
  CommandType type;
  int applianceId;    // 1-4 for appliances, 1-N for scenes
  int intensity;      // 0-100 for dimming
  bool valid;         // Whether the command was parsed successfully
  char rawCommand[64]; // Original command string for logging
  
  ParsedCommand() : type(CMD_UNKNOWN), applianceId(-1), intensity(0), valid(false) {
    memset(rawCommand, 0, sizeof(rawCommand));
  }
};

// ============================================================================
// COMMAND PARSER CLASS
// ============================================================================

class CommandParser {
private:
  static const int MAX_CMD_LENGTH = 64;
  char buffer[MAX_CMD_LENGTH];
  int bufferIndex;
  
  // Helper: Convert string to uppercase
  void toUpperCase(char* str) {
    for (int i = 0; str[i]; i++) {
      str[i] = toupper(str[i]);
    }
  }
  
  // Helper: Parse integer from string position
  int parseInt(const char* str, int start) {
    int result = 0;
    int i = start;
    
    // Skip non-digits
    while (str[i] && !isdigit(str[i])) i++;
    
    // Parse digits
    while (str[i] && isdigit(str[i])) {
      result = result * 10 + (str[i] - '0');
      i++;
    }
    
    return result;
  }
  
public:
  CommandParser() : bufferIndex(0) {
    memset(buffer, 0, MAX_CMD_LENGTH);
  }
  
  // Initialize parser
  void begin() {
    reset();
    Serial.println("CommandParser: Initialized");
  }
  
  // Reset parser state
  void reset() {
    bufferIndex = 0;
    memset(buffer, 0, MAX_CMD_LENGTH);
  }
  
  // Process incoming character
  // Returns pointer to parsed command if complete, nullptr otherwise
  ParsedCommand* processChar(char c) {
    // Check for command terminator
    if (c == '\n' || c == '\r') {
      if (bufferIndex > 0) {
        buffer[bufferIndex] = '\0';
        return parseBuffer();
      }
      reset();
      return nullptr;
    }
    
    // Add character to buffer
    if (bufferIndex < MAX_CMD_LENGTH - 1) {
      buffer[bufferIndex++] = c;
    } else {
      // Buffer overflow - reset
      Serial.println("CommandParser: Buffer overflow");
      reset();
    }
    
    return nullptr;
  }
  
  // Parse the current buffer
  ParsedCommand* parseBuffer() {
    static ParsedCommand result;
    result = ParsedCommand(); // Reset
    
    // Save raw command
    strncpy(result.rawCommand, buffer, sizeof(result.rawCommand) - 1);
    
    // Create uppercase copy for parsing
    char cmdCopy[MAX_CMD_LENGTH];
    strncpy(cmdCopy, buffer, MAX_CMD_LENGTH - 1);
    cmdCopy[MAX_CMD_LENGTH - 1] = '\0';
    toUpperCase(cmdCopy);
    
    // Remove trailing whitespace
    int len = strlen(cmdCopy);
    while (len > 0 && (cmdCopy[len-1] == ' ' || cmdCopy[len-1] == '\t')) {
      cmdCopy[--len] = '\0';
    }
    
    // Parse command type
    if (strncmp(cmdCopy, "ON", 2) == 0) {
      result.type = CMD_ON;
      result.applianceId = parseInt(cmdCopy, 2);
      result.intensity = 100;
    }
    else if (strncmp(cmdCopy, "OFF", 3) == 0) {
      result.type = CMD_OFF;
      result.applianceId = parseInt(cmdCopy, 3);
      result.intensity = 0;
    }
    else if (strncmp(cmdCopy, "DIM", 3) == 0) {
      result.type = CMD_DIM;
      char* colon = strchr(cmdCopy, ':');
      if (colon) {
        result.applianceId = parseInt(cmdCopy, colon - cmdCopy);
        char* comma = strchr(colon, ',');
        if (comma) {
          result.intensity = parseInt(cmdCopy, comma - cmdCopy);
        }
      }
    }
    else if (strncmp(cmdCopy, "TOGGLE", 6) == 0) {
      result.type = CMD_TOGGLE;
      result.applianceId = parseInt(cmdCopy, 6);
    }
    else if (strncmp(cmdCopy, "STATUS", 6) == 0) {
      result.type = CMD_STATUS;
    }
    else if (strncmp(cmdCopy, "HELP", 4) == 0) {
      result.type = CMD_HELP;
    }
    else if (strncmp(cmdCopy, "SCENE", 5) == 0) {
      result.type = CMD_SCENE;
      result.applianceId = parseInt(cmdCopy, 5);
    }
    else if (strncmp(cmdCopy, "EMERGENCY", 9) == 0 || 
             strncmp(cmdCopy, "STOP", 4) == 0 ||
             strncmp(cmdCopy, "PANIC", 5) == 0) {
      result.type = CMD_EMERGENCY_STOP;
    }
    
    // Validate basic structure
    result.valid = (result.type != CMD_UNKNOWN);
    
    Serial.print("CommandParser: Parsed '");
    Serial.print(result.rawCommand);
    Serial.print("' → Type: ");
    Serial.println(result.type);
    
    return &result;
  }
};

#endif // COMMAND_PARSER_H