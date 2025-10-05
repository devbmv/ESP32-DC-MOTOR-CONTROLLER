#include <iostream>
#include <windows.h>
#include <conio.h>  // pentru kbhit()

using namespace std;

bool buttonState = false;

typedef void (*CallbackFunction)();
CallbackFunction onButtonPressed = nullptr;

void myCustomAction() {
    buttonState = !buttonState;
    cout << "Button toggled. State: " << (buttonState ? "ON" : "OFF") << endl;
}

void setButtonCallback(CallbackFunction cb) {
    onButtonPressed = cb;
}

int main() {
    setButtonCallback(myCustomAction);
    cout << "Press any key to simulate button press..." << endl;

    while (true) {
        if (_kbhit()) {  // dacă s-a apăsat o tastă
            _getch();    // citește tasta fără ENTER
           myCustomAction();
        }

        Sleep(1000);
        cout << "After sleep 1000..." << endl;
    }

    return 0;
}
