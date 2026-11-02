// (TP HD fork) Hold A to sprint (tp_sprint.cpp): the option of the Switch tab's Controls.
#pragma once

namespace tp_sprint {
bool enabled();
void set_enabled(bool on);
bool keep_after_release();  // the sprint goes on after A lets go, until Link stops moving
void set_keep_after_release(bool on);
constexpr int kSpeedChoices = 3;
int speed_choice();
void set_speed_choice(int choice);
const char* speed_name(int choice);
}  // namespace tp_sprint
