
# Log-Lock: Smart Access Control & Auditing System Prototype🛡️

<p align="center">
  <img src="https://energia.nu/img/energia_logo_2017.png](https://energia.nu/img/energia.png" width="300"/>
</p>

<p align="center">
  <strong>A real-time security and access control system developed entirely within the Energia framework.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C++-blue.svg" alt="Language: C++">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License: MIT">
</p>

---

![Log-Lock In Action](https://i.imgur.com/link_to_your_gif_or_image.gif)
*(Replace the link above with a GIF or image of your project in action)*

A functional prototype of a security system for critical assets and cabinets, developed for the **"Semiconductors in Industry 4.0" Hackathon**.

## 📜 Project Description

**Log-Lock** is a security solution that modernizes access control for secure compartments. It replaces traditional locks with a PIN authentication system, real-time sensor monitoring, and most importantly, an **immutable digital audit log**.

The system is designed to solve the problem of **inventory shrinkage** and the lack of traceability in industrial, logistics, and healthcare environments, offering enterprise-level security at an affordable cost for Small and Medium-sized Businesses (SMBs).

## ✨ Key Features

* **PIN Authentication:** Access via a virtual Numpad controlled by the joystick.
* **Intrusion Detection:** A light sensor triggers an alarm if light is detected inside the cabinet while the door is closed.
* **Door Status Monitoring:** Monitors whether the door is open or closed.
* **Access Log:** Stores and displays the last 5 access attempts (Success or Fail) with a timestamp on the screen.
* **Alert System:** Uses a buzzer and an RGB LED to provide audible and visual feedback on the system's status.
* **Motion Detection:** An accelerometer detects shocks or sudden movements that could indicate a forced entry attempt.
* **User Levels:** Supports a standard user password and an administrator password for advanced functions like changing the user password.

## 🛠️ Hardware & Software

### Hardware
* **Development Board:** Texas Instruments **MSP-EXP430F5529LP** (LaunchPad)
* **Sensor Board:** **BOOSTXL-EDUMKII** (Educational BoosterPack MK II)

### Software
* **IDE:** **Energia** (Version 1.0.1E0017 or similar)
* **Language:** The project is implemented in **C++** using the Energia framework, which is based on Arduino/Wiring.

## ⚙️ System Flow & State Logic

The system operates using a finite-state machine to ensure robust and predictable behavior.

* 🚀 **START STATE:** The system displays a welcome screen. A button press transitions it to the entry state.

* ⌨️ **ENTRY STATE:** A virtual Numpad is displayed. The user navigates with the joystick to enter a 3-digit PIN.

* 🤔 **VERIFY STATE:** The system compares the entered PIN against the saved passwords.
    * **If the user PIN is correct:** Access is granted, a success tone plays, and the system displays the **Event Log Stage**, showing the last 5 attempts with their timestamps.
    * **If the admin PIN is correct (and the system is not locked):** It transitions to the **Change Password State**.
    * **If the PIN is incorrect:** A failed attempt is logged. If attempts are exceeded, it transitions to the **Locked State**. Otherwise, it returns to the start state.

* ✏️ **CHANGE PASSWORD STATE:** The administrator enters a new 3-digit password for the standard user.

* 🚫 **LOCKED STATE:** After multiple failed attempts, the system locks down. Only the administrator PIN can unlock it.

## 🖼️ Custom Graphics Implementation

To display custom graphics like icons and logos without relying on an SD card, an image-to-array conversion technique was used.

1.  **Design:** An image is created in a pixel art editor (e.g., 40x40 pixels).
2.  **Conversion:** Using a script or an online tool, each pixel in the image is converted to its corresponding **RGB565 (16-bit)** color value.
3.  **Storage:** The thousands of color values are stored in a `const uint16_t` array within a header file (`.h`).
4.  **Rendering:** The main program includes this `.h` file. A function with two nested `for` loops iterates through the array and draws the image on the screen pixel by pixel using the `myScreen.point()` function.

This method allows for rich, custom graphical interfaces that are fast and entirely self-contained within the microcontroller's flash memory.

## 🚀 Future Improvements

* **Hardware RTC Integration:** The current timestamp is based on `millis()`. A key improvement is to use the **MSP430's built-in hardware Real-Time Clock (RTC)** to get persistent and highly accurate timestamps, even if the device loses power.
* **Wi-Fi Connectivity:** Integrate a **Wi-Fi module** (like an ESP8266) to enable "Log-Lock" to send access logs and intrusion alerts in real-time to a cloud platform or a mobile app, allowing for remote security management.

## 🔧 How to Use

1.  Open the `.ino` file in the Energia IDE.
2.  Ensure all `.h` files (like `change_icon.h`, `utils.h`) are in the same sketch folder.
3.  Go to `Tools > Board` and select your `MSP-EXP430F5529LP` board.
4.  Compile and upload the code.

## 👥 Authors

* **Ñequesitos**
    * *(Your Name)*
    * *(Team Member 2 Name)*
    * *(Team Member 3 Name)*
