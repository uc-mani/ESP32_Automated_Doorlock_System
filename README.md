# ESP32_Automated_Doorlock_System
This project implements a secure, automated door lock system using LoRa communication and ESP32 microcontrollers. Designed for apartment complexes, the system unlocks doors remotely and keeps track of which units responded. It is optimized for low-power, long-range communication and scalable to hundreds of units.


## Features
  - Remote Door Unlocking using LoRa
  - LoRa Transmitter/Receiver Communication (Up to 200 apartments)
  - Unit & Building Identification using encoded unitAndBuilding codes
  - Unlock Acknowledgment sent from receiver to transmitter
  - Non-Responsive Unit Detection
  - Optimized Encoding (switch from strings to integers for efficiency)
  - Configurable Unit Mapping

## Project Structure
The system includes:

### Transmitter (Admin/Controller Side)
- Sends unlock command to all units
- Maintains a list of expected unitAndBuilding codes (e.g., 1011)
- Stores acknowledgments from receivers
- Compares to find non-responding units

### Receiver (Client Side per Apartment)
- Listens for unlock commands
- Cross-verifies incoming request
- Activates motor/relay to unlock door
- Sends acknowledgment back including its unitAndBuilding code

### Code Design
- unitAndBuilding format:
    XXXY → XXX = Unit, Y = Building
    Example: 1011 → Unit 101, Building 1

- Apartment Clusters:
  Building 1:
    Tower 1 → Units 101-104, 201-204, 301-304
    Tower 2 → Units 105-108, etc.

## Hardware Used
- ESP32
- LoRa SX1278 Modules
- Relay/Motor Driver (L298N or equivalent)
- DC Motor/Servo for locking mechanism
- Shunt resistor for current sensing (Optional safety feature)
