# Post-build: writes dist/MouseJiggler-CardputerADV-<version>.bin, a single merged image
# (bootloader + partitions + boot_app0 + app) that Launcher / M5Burner / web flashers accept.
import os

Import("env")  # noqa: F821  (provided by PlatformIO)


def merge_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    dist_dir = os.path.join(project_dir, "dist")
    os.makedirs(dist_dir, exist_ok=True)

    version = "dev"
    for flag in env.get("CPPDEFINES", []):
        if isinstance(flag, tuple) and flag[0] == "FW_VERSION":
            version = str(flag[1]).strip('\\"')

    app = os.path.join(build_dir, "firmware.bin")
    merged = os.path.join(dist_dir, f"MouseJiggler-CardputerADV-{version}.bin")

    # FLASH_EXTRA_IMAGES holds (offset, path) for bootloader, partitions and boot_app0.
    images = [(off, env.subst(path)) for off, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET") or "0x10000", app))
    args = []
    for off, path in sorted(images, key=lambda i: int(i[0], 16)):
        args += [off, f'"{path}"']

    cmd = " ".join([
        '"$PYTHONEXE"', '"$OBJCOPY"',
        "--chip", "esp32s3", "merge_bin",
        "-o", f'"{merged}"',
        "--flash_mode", "keep", "--flash_freq", "keep",
        "--flash_size", env.BoardConfig().get("upload.flash_size", "8MB"),
        *args,
    ])
    env.Execute(cmd)
    print(f"Merged image: {merged}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)  # noqa: F821
