# 🚗 Car Avoidance 3D Game  
https://github.com/user-attachments/assets/254aed8b-470d-400a-aa97-1df24b64e2ac

A fast-paced 3-lane obstacle-dodging game built with **C++**, **OpenGL**, **GLFW**, **GLM**, and **Assimp**.  
This project showcases real-time 3D rendering, smooth lane animations, first-person and third-person camera modes, dynamic obstacles, buildings, score progression, and FreeType text rendering.

> **Goal:** Survive as long as possible by switching lanes to avoid obstacles. Your speed increases automatically the longer you survive.

---

## 📸 Screenshots  
<img width="795" height="587" alt="image" src="https://github.com/user-attachments/assets/b1a4618f-0d33-4e43-bbd5-b3156f9f7ca4" />

3rd
<img width="796" height="596" alt="image" src="https://github.com/user-attachments/assets/8f5974eb-bbf0-46c2-94f1-0ea4166a1a10" />

1st
<img width="801" height="592" alt="image" src="https://github.com/user-attachments/assets/042f493d-7680-44e2-803c-146c719f7ecc" />


---

## 🎮 Features

### 🛣️ **Core Gameplay**
- Smooth 3-lane switching with car tilt animation  
- Dynamic obstacle spawning (cones, barrels, stop signs)  
- Distance-based scoring  
- Automatic speed increases as your score grows  
- Crash detection & Game Over screen

### 🏙️ **World & Environment**
- Infinite road generation (segment-based)  
- Procedurally spawned buildings on both sides  
- Grass terrain, footpaths, and red/white curbs  
- Supports repeating textures with Mipmaps  
- Foggy evening atmosphere for immersive feel

### 📷 **Camera Modes**
- **Third-Person**: classic behind-the-car view  
- **First-Person**: camera mounted inside the car with dynamic tilt  
- Toggle anytime using **C**

### 🔤 **UI & Text**
- FreeType-based text rendering  
- On-screen score, speed, distance  
- Main menu with controls and instructions  
- Game over screen with restart options

---

## 🕹️ Controls

| Key | Action |
|-----|--------|
| **A** | Move Left |
| **D** | Move Right |
| **C** | Toggle camera (First/Third Person) |
| **SPACE** | Start game |
| **ESC** | Back to menu / Quit (on menu) |
| **R** | Restart after Game Over |
| **M** | Back to Main Menu |
