#pragma once

// On-screen keyboard for naming a new project.
namespace keyboard {

enum class Action { None, Cancel, Create };

void enter(const char* prompt = "NAME YOUR PROJECT", const char* submit = "CREATE", int maxLength = 20);
Action touchDown(int x, int y);
// The name typed so far (upper case, at most 20 characters).
const char* name();
// Shows a one-line message under the name field (e.g. "NAME ALREADY EXISTS").
void showMessage(const char* text);

}  // namespace keyboard
