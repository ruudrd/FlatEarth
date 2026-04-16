## 🌍 FlatEarth

FlatEarth is an ESP32-powered project that brings real-time satellite imagery of Earth to your desk. By fetching data from NOAA's GOES satellites, this project turns a small circular LCD into a "portal" from space, showing the latest planetary views.

<img src="media/FlatEarth2.png" width="400">

-----

## 🛠 Supported Hardware

This project currently supports two primary hardware configurations:

  * **Waveshare 1.46" Touch LCD (B):** An ESP32-S3 based round display ([Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.46B)).
  * **Generic GC9A01 Display:** Any ESP32 board paired with a 1.28-inch 240x240 IPS LCD using the GC9A01 driver.

-----

## 🛰 Satellite Data Sources

The project pulls imagery from the following sources:

  * **GOES-18 / GOES-19:** High-resolution Full Disk imagery provided by NOAA.
      * **Data Source:** [NOAA STAR Satellite Center](https://www.star.nesdis.noaa.gov/GOES/index.php)
  * **Meteosat (EUMETSAT):** Coverage for Europe and Africa is currently **under development**.
      * **Data Source:** [EUMETSAT User Portal](https://user.eumetsat.int/data)
  * **Elektro-L & DSCOVR EPIC:** Support for these sources is currently **experimental**.

-----

## ⚙️ Configuration & Setup

### 1\. ImageKit.io Proxy

Currently, raw satellite images are too large for an ESP32 to process directly. We use [ImageKit.io](https://imagekit.io/) as a real-time image transformation proxy to resize and crop the images before they reach the device.

1.  Create a free account on **ImageKit.io**.
2.  Go to **External Storage** and click **Add New**.
3.  Set the **Origin Type** to `Web Folder`.
4.  Set the **Base URL** to: `https://cdn.star.nesdis.noaa.gov`
5.  Note your **URL-endpoint** (e.g., `ik.imagekit.io/your_id/GOES`).

Imagekit Settings<img src="media/ImageKitSettings.png" width="400">

### 2\. Firmware Setup

1.  Navigate to the `include/` directory.
2.  Rename `renameto_secrets.h` to `secrets.h`.
3.  Update the file with your WiFi credentials and your ImageKit URL:

<!-- end list -->

```cpp
#define WIFI_SSID "Your_SSID"
#define WIFI_PASS "Your_Password"
#define IMAGEKIT_ENDPOINT "https://ik.imagekit.io/your_id/"
```

-----

## 📸 Media

Device in Action<img src="media/FlatEarth.png" width="400">

-----

## 🏗 To-Do List

  - [ ] **On-Device Processing:** Implement direct downloading and resizing on the ESP32 to remove the dependency on ImageKit.
  - [ ] **SD Card Support:** Add functionality to cache images on an SD card for faster loading and offline display.
  - [ ] **Satellite Expansion:** Complete integration for Meteosat (EUMETSAT) data.
  - [ ] **Stability:** Finalize experimental support for Elektro-L and EPIC.
  - [ ] **Satellite Selection:** Implement a method to toggle between available satellite sources.

-----

## 🤖 Note on Development

> **Disclaimer:** This Fun little thing was developed with extensive assistance from AI to streamline coding and documentation. While AI helped bridge the technical gaps, the core vision, hardware integration, and overall execution remain human-driven endeavors.