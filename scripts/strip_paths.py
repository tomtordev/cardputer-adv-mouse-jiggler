# Pre-build: keep local install paths (and with them the user name) out of __FILE__ strings
# embedded in the firmware. Both slash styles are mapped because Windows paths reach GCC in either form.
Import("env")  # noqa: F821  (provided by PlatformIO)

for key, alias in (("$PROJECT_PACKAGES_DIR", "pio/packages"), ("$PROJECT_CORE_DIR", "pio"), ("$PROJECT_DIR", "project")):
    path = env.subst(key)  # noqa: F821
    for variant in {path, path.replace("\\", "/")}:
        env.Append(CCFLAGS=[f"-fmacro-prefix-map={variant}={alias}"])  # noqa: F821
