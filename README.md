# VisionEngine

A lightweight **C++ image processing tool** built with OpenCV that simulates how different animals perceive visual information.

The project is **CLI-based** and designed to be simple, fast, and easily extensible.

---

## 🚀 Features

* Multiple vision simulation modes:

  * 🐶 **Dog Vision** – reduced red sensitivity, blur, slight brightness boost
  * 🐱 **Cat Vision** – enhanced low-light perception with edge emphasis
  * 🐍 **Snake Vision** – thermal-style heatmap simulation
* Fast native processing using **OpenCV**
* Simple command-line interface
* Easily extendable architecture for adding new modes

---

## 🛠️ Setup

### 1. Install dependencies

On Debian/Ubuntu (including WSL):

```bash
sudo apt update
sudo apt install -y libopencv-dev pkg-config
```

---

### 2. Compile

```bash
g++ VisionEngine.cpp -o VisionEngine `pkg-config --cflags --libs opencv4`
```

---

## ▶️ Usage

```bash
./VisionEngine <image_path> --mode <mode>
```

### Example:

```bash
./VisionEngine images/input.jpg --mode dog-vision
```

---

## 🎨 Supported Modes

| Mode           | Description                                                      |
| -------------- | ---------------------------------------------------------------- |
| `dog-vision`   | Simulates dichromatic vision with blur and brightness adjustment |
| `cat-vision`   | Enhances low-light visibility and emphasizes edges               |
| `snake-vision` | Applies a heatmap to simulate thermal perception                 |

If no mode is specified, the default is `dog-vision`.

---

## 📁 Output

* Output image is saved **next to the input file**
* Filename format:

```text
<input_name>_<mode>.<ext>
```

### Example:

```text
image_dog-vision.jpg
image_cat-vision.jpg
image_snake-vision.jpg
```

---

## 🧱 Project Structure

```text
avision/
├── VisionEngine.cpp   # Core processing engine
├── VisionEngine       # Compiled executable (after build)
├── images/            # Input/output images
```

---

## ⚠️ Notes

* Input image must be a valid file readable by OpenCV
* If the image fails to load, the program will exit with an error
* Each run overwrites output only if filenames collide

---

## 🔮 Future Improvements

* Additional vision modes (infrared, UV approximation)
* Batch image processing
* Real-time webcam support
* AI-based perception models

---

## 📌 Summary

VisionEngine is a modular image processing engine focused on simulating different perception models.
It serves as a foundation for exploring both traditional computer vision and future AI-based enhancements.

---
