# Embedded Raspberry Pi Deployment Guide

This guide walks you through deploying HeatEx as a standalone embedded system on Raspberry Pi that boots directly into the application with a custom splash screen.

## Choosing Your Approach

### Option 1: Raspberry Pi OS Lite (RECOMMENDED - Easiest)
**What you get:** Minimal Linux (no desktop), boots straight to your app, ~500MB storage
- **Pros:** Easy setup, well-supported, package management with apt
- **Cons:** Still includes unused packages (200-300MB)
- **Setup time:** 1-2 hours
- **Best for:** Quick deployment, easier maintenance

### Option 2: Buildroot (Advanced - Truly Minimal)
**What you get:** Custom Linux with ONLY your app's dependencies, ~100-150MB storage
- **Pros:** Extremely minimal, fast boot (<5 seconds), complete control
- **Cons:** Complex initial setup, harder to update/maintain
- **Setup time:** 4-8 hours first time
- **Best for:** Production embedded systems, performance-critical applications

### Option 3: Yocto Project (Expert - Industrial)
**What you get:** Professional embedded Linux, customizable everything
- **Pros:** Industry standard, highly customizable, good for commercial products
- **Cons:** Steep learning curve, long build times
- **Setup time:** 1-2 days first time
- **Best for:** Commercial products, long-term support requirements

**This guide covers Options 1 and 2.** For Option 1, follow the steps below. For Option 2 (Buildroot), skip to the Buildroot section at the end.

## Hardware Requirements

- Raspberry Pi 4 (2GB+ RAM recommended)
- MicroSD card (16GB+ recommended)
- Display (HDMI)
- Power supply

## Step 1: Prepare Raspberry Pi OS

### 1.1 Flash Raspberry Pi OS Lite (64-bit)

Download and flash **Raspberry Pi OS Lite (64-bit)** to your SD card using Raspberry Pi Imager:
- Use the "Raspberry Pi OS Lite (64-bit)" option for a minimal installation
- Configure WiFi/SSH in the imager if needed for headless setup

### 1.2 Initial Boot and Update

```bash
# SSH into your Pi or use keyboard/monitor
sudo apt update
sudo apt upgrade -y
```

## Step 2: Install Dependencies

```bash
# Install build tools and Qt6 dependencies
sudo apt install -y \
    build-essential \
    cmake \
    git \
    qt6-base-dev \
    qt6-charts-dev \
    qt6-svg-dev \
    libqrencode-dev \
    qt6-wayland \
    libgles2-mesa-dev \
    libgbm-dev \
    libdrm-dev \
    plymouth \
    plymouth-themes
```

## Step 3: Build the Application

### 3.1 Transfer Source Code

Transfer your source code to the Pi (via git, scp, or USB):

```bash
# Example using git
cd ~
git clone <your-repo-url> heatex
cd heatex
```

Or if copying from your development machine:
```bash
# On your dev machine:
scp -r /home/victor/source/repos/tecspec-controls pi@<pi-ip>:~/heatex
```

### 3.2 Build BACnet Stack

```bash
cd ~/heatex/bacnet-stack
mkdir -p build
cd build
cmake ..
make -j4
cd ~/heatex
```

### 3.3 Build HeatEx Application

```bash
mkdir build
cd build
cmake ..
make -j4
```

The executable will be at `~/heatex/build/bin/heatex`

## Step 4: Install Application

```bash
# Create installation directory
sudo mkdir -p /opt/heatex

# Copy executable
sudo cp ~/heatex/build/bin/heatex /opt/heatex/

# Copy required assets to /opt/
sudo cp ~/heatex/heatex.png /opt/
sudo cp ~/heatex/alfa.svg /opt/

# Make executable
sudo chmod +x /opt/heatex/heatex
```

## Step 5: Custom Splash Screen

### 5.1 Create Splash Image

Create a custom splash screen image (1920x1080 PNG recommended):
- Use your company logo or custom design
- Save as `splash.png`

### 5.2 Install Splash Screen

```bash
# Copy splash image
sudo mkdir -p /opt/heatex/splash
sudo cp ~/heatex/alfa.svg /opt/heatex/splash/  # Or your custom splash.png

# Install framebuffer image viewer for simple splash
sudo apt install -y fbi
```

### 5.3 Alternative: Plymouth Boot Splash (Full Boot Animation)

For a more integrated boot splash:

```bash
# Install Plymouth
sudo apt install -y plymouth plymouth-themes

# Create custom Plymouth theme (optional)
# We'll configure this in Step 6
```

## Step 6: Configure Auto-Start

### 6.1 Create Systemd Service

Create `/etc/systemd/system/heatex.service`:

```ini
[Unit]
Description=HeatEx Monitoring Application
After=graphical.target
Wants=graphical.target

[Service]
Type=simple
User=root
Group=root
Environment="QT_QPA_PLATFORM=linuxfb"
Environment="QT_QPA_EGLFS_ALWAYS_SET_MODE=1"
Environment="QT_QPA_EGLFS_KMS_CONFIG=/etc/heatex-kms.json"
Environment="QT_QPA_EGLFS_HIDECURSOR=1"
Environment="QT_LOGGING_RULES=*.debug=false"
Environment="QT_QPA_FB_TTY=/dev/tty7"
WorkingDirectory=/opt/heatex
ExecStartPre=/bin/sleep 1
ExecStart=/opt/heatex/heatex
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=graphical.target
```

### 6.2 Create Qt EGLFS Configuration (Optional - only needed for EGLFS)

**Note:** This file is only needed if using `QT_QPA_PLATFORM=eglfs`. Since we're using linuxfb by default, you can skip this step unless your display supports KMS/DRM.

Create `/etc/heatex-kms.json`:

```json
{
  "device": "/dev/dri/card0",
  "outputs": [
    {
      "name": "DSI-1",
      "mode": "1280x800"
    }
  ]
}
```

Adjust the connector name and resolution to match your display:
- For HDMI displays: `"name": "HDMI-A-1"`, `"mode": "1920x1080"`
- For DSI displays: `"name": "DSI-1"`, `"mode": "1280x800"`

To find your display's connector name, run: `sudo modetest -M vc4 -c`

### 6.3 Enable the Service

```bash
sudo systemctl daemon-reload
sudo systemctl enable heatex.service

# Set system to use graphical.target (required even on Lite)
sudo systemctl set-default graphical.target
```

## Step 7: Configure Boot for Kiosk Mode

### 7.1 Disable Console Messages

Edit `/boot/firmware/cmdline.txt`:

```bash
sudo nano /boot/firmware/cmdline.txt
```

Replace the entire line with:
```
console=tty3 loglevel=3 logo.nologo vt.global_cursor_default=0 quiet splash plymouth.ignore-serial-consoles
```

This hides boot messages and the cursor.

### 7.2 Disable Login Prompts

```bash
# Disable getty on tty1
sudo systemctl disable getty@tty1.service
```

### 7.3 Configure Plymouth Splash

Edit `/boot/firmware/config.txt`:

```bash
sudo nano /boot/firmware/config.txt
```

Add at the end:
```
# Disable rainbow splash
disable_splash=1

# Framebuffer settings for smooth graphics
dtoverlay=vc4-kms-v3d
max_framebuffers=2
```

### 7.4 Set Plymouth Theme

```bash
# List available themes
plymouth-set-default-theme --list

# Set a theme (spinner is simple and clean)
sudo plymouth-set-default-theme spinner

# Update initramfs
sudo update-initramfs -u
```

## Step 8: Custom Boot Splash with Company Logo

### 8.1 Create Simple Splash Service (Before App Starts)

Create `/etc/systemd/system/splash-screen.service`:

```ini
[Unit]
Description=Show Splash Screen
DefaultDependencies=no
After=local-fs.target
Before=heatex.service

[Service]
Type=oneshot
ExecStartPre=/bin/sleep 2
ExecStartPre=/usr/bin/chvt 7
ExecStart=/usr/bin/fbi -T 7 -d /dev/fb0 --noverbose -a -t 5 /opt/heatex/splash/tecspec.png
ExecStartPost=/bin/sleep 5
ExecStartPost=/usr/bin/killall fbi

[Install]
WantedBy=graphical.target
```

Enable it:
```bash
sudo systemctl enable splash-screen.service
```

**Note:** Copy your splash screen image:
```bash
sudo mkdir -p /opt/heatex/splash
sudo cp ~/heatex/tecspec.png /opt/heatex/splash/
```

## Step 9: Optimize Boot Time (Optional)

### 9.1 Disable Unnecessary Services

```bash
# Disable services you don't need
sudo systemctl disable bluetooth.service
sudo systemctl disable hciuart.service
sudo systemctl disable avahi-daemon.service
sudo systemctl disable triggerhappy.service

# Remove unnecessary packages
sudo apt remove --purge -y triggerhappy
sudo apt autoremove -y
```

### 9.2 Reduce Boot Delay

Edit `/etc/systemd/system.conf`:
```bash
sudo nano /etc/systemd/system.conf
```

Uncomment and set:
```
DefaultTimeoutStartSec=10s
DefaultTimeoutStopSec=10s
```

## Step 10: Final Configuration

### 10.1 Hide Mouse Cursor

Install unclutter:
```bash
sudo apt install -y unclutter
```

Update `/etc/systemd/system/heatex.service` to include cursor hiding:

```ini
[Service]
...
ExecStartPre=/bin/sleep 2
ExecStartPre=/usr/bin/unclutter -idle 0 -root &
ExecStart=/opt/heatex/heatex
...
```

**Note:** The `QT_QPA_EGLFS_HIDECURSOR` environment variable only works with EGLFS, not linuxfb. Use unclutter for cursor hiding with linuxfb.

### 10.2 Disable Screen Blanking

Edit `/boot/firmware/cmdline.txt` and add:
```
consoleblank=0
```

## Step 11: Test and Deploy

### 11.1 Test the Service

```bash
# Test the service manually
sudo systemctl start heatex.service

# Check status
sudo systemctl status heatex.service

# View logs
sudo journalctl -u heatex.service -f
```

### 11.2 Reboot and Verify

```bash
sudo reboot
```

Your Pi should now:
1. Show minimal boot messages
2. Display your custom splash screen
3. Boot directly into the HeatEx application
4. No terminal or cursor visible

## Troubleshooting

### Screen size is weird
Try changing the QT scaling in src/config.cpp to 1.0 instead of 0.8

### Application Won't Start

Check logs:
```bash
sudo journalctl -u heatex.service -n 50
```

### Display Issues

Try different Qt platforms:
- `QT_QPA_PLATFORM=linuxfb` - Legacy framebuffer (recommended for most displays)
- `QT_QPA_PLATFORM=eglfs` - Modern KMS/DRM (for displays with DRM support)
- `QT_QPA_PLATFORM=wayland` - Wayland compositor (requires more setup)

**Common errors:**
- `qt.qpa.xcb: could not connect to display` - Set `QT_QPA_PLATFORM=linuxfb`
- `drmModeGetResources failed` - Use linuxfb instead of eglfs
- `no screens available` - Use linuxfb or check display driver configuration

### Performance Issues

- Ensure GPU memory is allocated: Edit `/boot/firmware/config.txt` and set `gpu_mem=256`
- Enable V3D driver: `dtoverlay=vc4-kms-v3d`

### Remote Access for Debugging

Keep SSH enabled during development:
```bash
sudo systemctl enable ssh
```

You can still SSH in even when the app is running fullscreen.

## Making it Read-Only (Advanced)

For production embedded systems, consider making the filesystem read-only:
- Use `raspi-config` to enable overlay filesystem
- Or use tools like `overlayroot`

This prevents SD card corruption and extends card life.

## Updates and Maintenance

To update the application:

```bash
# Stop the service
sudo systemctl stop heatex.service

# Replace the binary
sudo cp /path/to/new/heatex /opt/heatex/heatex

# Start the service
sudo systemctl start heatex.service
```

Or set up a remote update mechanism via SSH.

---

# OPTION 2: Buildroot - Truly Minimal Custom Linux

This approach creates a custom Linux image with ONLY what your app needs. Final image is ~100-150MB.

## Prerequisites (On Your Arch Linux Development Machine)

You'll build the image on your Arch PC, not on the Pi.

```bash
# Install dependencies (Arch Linux)
sudo pacman -S --needed \
    base-devel \
    ncurses \
    bc \
    rsync \
    unzip \
    wget \
    cpio \
    python \
    git
```

**Note:** Arch already includes most tools needed (sed, make, binutils, gcc, g++, patch, gzip, bzip2, perl, tar, file) in the `base-devel` group.

**Arch-specific considerations:**
- Buildroot works perfectly on Arch but occasionally newer toolchain versions can cause issues
- If you encounter build errors, check Buildroot's known issues or use their pre-built toolchain option
- The AUR package `buildroot` exists but building from source (below) is recommended for embedded work
- **IMPORTANT:** On Arch Linux, `make menuconfig` has ncurses compatibility issues. Use `make nconfig` instead throughout this guide.

## Step 1: Get Buildroot

```bash
cd ~
wget https://buildroot.org/downloads/buildroot-2025.08.3.tar.gz
tar xf buildroot-2025.08.3.tar.gz
cd buildroot-2025.08.3
```

**Arch note:** If you encounter locale-related build errors, ensure your locale is set:
```bash
# Check current locale
locale

# If needed, uncomment en_US.UTF-8 in /etc/locale.gen and run:
sudo locale-gen
export LC_ALL=en_US.UTF-8
```

## Step 2: Configure for Raspberry Pi 4

```bash
# Start with Raspberry Pi 4 (64-bit) base config
make raspberrypi4_64_defconfig

# Open configuration menu (use nconfig on Arch Linux)
make nconfig
```

**nconfig Navigation:**
- Arrow keys: Navigate menu
- Enter: Select/Enter submenu
- Space: Toggle option
- F6: Save configuration
- F9: Exit
- F1: Help

### In nconfig, configure the following:

**Target options:**
- Target Architecture: AArch64 (little endian)
- Target Architecture Variant: cortex-A72

**Toolchain:**
- C library: glibc (required for Qt)
- Enable C++ support: YES
- Enable WCHAR support: YES

**System configuration:**
- System hostname: `heatex`
- System banner: `Welcome to HeatEx`
- Init system: systemd (easier service management)
- Enable root login with password: YES (set a password)
- getty options → TTY port: tty1 → DISABLE (no console)

**Kernel:**
- Linux Kernel: YES
- Kernel version: Latest stable
- Kernel configuration: Use the default config for target

**Filesystem images:**
- ext2/3/4 root filesystem: YES
  - ext2/3/4 variant: ext4
  - exact size: 200M (adjust as needed)

**Target packages → Graphic libraries and applications:**

Navigate to `Target packages` → `Graphic libraries and applications (graphic/text)`:

- Qt6: YES
  - Qt6 base components: YES
    - Core module: YES (auto-selected)
    - GUI module: YES
    - Widgets module: YES
    - Network module: YES
  - Qt6 Charts: YES
  - Qt6 SVG: YES

- Mesa3D: YES
  - Gallium drivers: v3d (for Raspberry Pi)
  - OpenGL EGL: YES
  - OpenGL ES: YES

**Target packages → Libraries:**
- Graphics
  - libdrm: YES (with additional drivers)
  - libgbm: YES
- Other
  - libqrencode: YES

**Target packages → Networking applications:**
- openssh: YES (for remote access during development)

**Target packages → System tools:**
- systemd: YES (should already be selected from init system)

Save configuration and exit.

## Step 3: Add Your Application to Buildroot

### 3.1 Create Package Directory

```bash
cd ~/buildroot-2025.08.3
mkdir -p package/heatex
```

### 3.2 Create Package Config

Create `package/heatex/Config.in`:

```
config BR2_PACKAGE_HEATEX
    bool "heatex"
    depends on BR2_PACKAGE_QT6
    select BR2_PACKAGE_QT6BASE_WIDGETS
    select BR2_PACKAGE_QT6CHARTS
    select BR2_PACKAGE_QT6SVG
    select BR2_PACKAGE_LIBQRENCODE
    help
      HeatEx monitoring application with BACnet support.
```

### 3.3 Create Package Makefile

Create `package/heatex/heatex.mk`:

```makefile
################################################################################
#
# heatex
#
################################################################################

HEATEX_VERSION = 1.0
HEATEX_SITE = $(BR2_EXTERNAL_HEATEX_PATH)
HEATEX_SITE_METHOD = local
HEATEX_DEPENDENCIES = qt6base qt6charts qt6svg libqrencode
HEATEX_INSTALL_STAGING = NO
HEATEX_INSTALL_TARGET = YES

define HEATEX_BUILD_BACNET
    $(MAKE) -C $(@D)/bacnet-stack/build
endef

define HEATEX_BUILD_CMDS
    mkdir -p $(@D)/bacnet-stack/build
    cd $(@D)/bacnet-stack/build && \
        $(TARGET_CONFIGURE_OPTS) cmake .. && \
        $(TARGET_MAKE_ENV) $(MAKE)

    mkdir -p $(@D)/build
    cd $(@D)/build && \
        $(TARGET_CONFIGURE_OPTS) cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/usr \
            .. && \
        $(TARGET_MAKE_ENV) $(MAKE)
endef

define HEATEX_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/build/bin/heatex $(TARGET_DIR)/usr/bin/heatex
    $(INSTALL) -D -m 0644 $(@D)/alfa.svg $(TARGET_DIR)/opt/heatex/splash.svg
endef

define HEATEX_INSTALL_INIT_SYSTEMD
    $(INSTALL) -D -m 0644 $(BR2_EXTERNAL_HEATEX_PATH)/heatex.service \
        $(TARGET_DIR)/usr/lib/systemd/system/heatex.service
endef

$(eval $(generic-package))
```

### 3.4 Link Your Source Code

```bash
# Create buildroot external tree
mkdir -p ~/buildroot-external/package/heatex

# Copy your source (use your actual path)
cp -r /home/victor/source/repos/tecspec-controls ~/buildroot-external/heatex-src

# Create BR2_EXTERNAL structure
cat > ~/buildroot-external/external.mk << 'EOF'
include $(sort $(wildcard $(BR2_EXTERNAL_HEATEX_PATH)/package/*/*.mk))
EOF

cat > ~/buildroot-external/external.desc << 'EOF'
name: HEATEX
desc: HeatEx application external tree
EOF

cat > ~/buildroot-external/Config.in << 'EOF'
source "$BR2_EXTERNAL_HEATEX_PATH/package/heatex/Config.in"
EOF
```

### 3.5 Create systemd Service

Create `~/buildroot-external/heatex.service`:

```ini
[Unit]
Description=HeatEx Monitoring Application
After=multi-user.target

[Service]
Type=simple
Environment="QT_QPA_PLATFORM=linuxfb"
ExecStartPre=/bin/sleep 2
ExecStart=/usr/bin/heatex
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

## Step 4: Enable Your Package

```bash
cd ~/buildroot-2025.08.3
export BR2_EXTERNAL=~/buildroot-external
make nconfig
```

Navigate to: `External options` → Enable `heatex`

You should also verify/enable the following dependencies:
- **Target packages → Graphic libraries → Qt6 Charts** (enable this)
- **Target packages → Libraries → Other → libqrencode** (enable this)

Save (F6) and exit (F9).

## Step 5: Build

```bash
# This will take 1-4 hours on first build
make
```

Output will be in: `output/images/sdcard.img`

## Step 6: Flash to SD Card

```bash
# Find your SD card device (be careful!)
lsblk

# Flash the image (replace /dev/sdX with your SD card)
sudo dd if=output/images/sdcard.img of=/dev/sdX bs=4M status=progress
sync
```

**Arch tips:**
- You can also use `balenaEtcher` from the AUR for a GUI approach: `yay -S balena-etcher`
- Ensure you're in the `disk` or `storage` group to access block devices: `sudo usermod -aG disk $USER` (requires logout/login)
- For safer flashing, unmount all partitions first: `sudo umount /dev/sdX*`

## Step 7: Boot Your Pi

Insert the SD card and power on. Your Pi will:
1. Boot in 5-10 seconds
2. Show minimal boot messages (optional to hide)
3. Launch directly into your HeatEx app
4. Total system size: ~100-150MB

## Customization

### Hide Boot Messages

Edit `~/buildroot-2025.08.3/board/raspberrypi4-64/cmdline.txt`:

```
console=tty3 quiet loglevel=3 logo.nologo vt.global_cursor_default=0
```

Then rebuild:
```bash
make
```

### Add Custom Splash

In your package makefile, add splash display before app starts, or integrate into your Qt app's startup screen.

### Enable SSH for Debugging

SSH is enabled by default if you selected openssh. Access via:
```bash
ssh root@<pi-ip>
```

## Updating Your App

To update after code changes:

```bash
cd ~/buildroot-external/heatex-src
# Make your changes

cd ~/buildroot-2025.08.3
make heatex-rebuild
make

# Flash new image
sudo dd if=output/images/sdcard.img of=/dev/sdX bs=4M
```

## Advantages of Buildroot Approach

- **Tiny footprint:** ~100MB vs ~500MB for Raspbian Lite
- **Fast boot:** 5-10 seconds to application
- **Secure:** Only includes what you need, smaller attack surface
- **Reproducible:** Same build every time
- **Professional:** Industry-standard approach for embedded products

## Disadvantages

- **No package manager:** Can't apt install things on the Pi
- **Complex updates:** Need to rebuild entire image for changes
- **Initial setup time:** First build takes hours
- **Learning curve:** Need to understand Buildroot basics

---

## Summary: Which Option Should You Choose?

**Choose Raspberry Pi OS Lite if:**
- You want to get running quickly (today)
- You might need to install additional packages later
- You're comfortable with standard Linux administration
- 500MB storage is acceptable

**Choose Buildroot if:**
- You want truly minimal (100MB)
- You need fast boot times (<10 seconds)
- This is a production/commercial product
- You won't be frequently modifying the system
- You're willing to invest time in setup

Both approaches will give you a system that boots straight to your app with no visible terminal or desktop.
