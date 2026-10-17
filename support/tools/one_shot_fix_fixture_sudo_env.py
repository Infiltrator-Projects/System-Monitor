#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/native_installer_safety.c"
text = path.read_text(encoding="utf-8")
old = '''    char real_sudo[] = "/usr/bin/sudo";
    char non_interactive[] = "-n";
    char separator[] = "--";
    if (access(real_sudo, X_OK) != 0) return EXIT_FAILURE;
    char *arguments[] = {
        real_sudo, non_interactive, separator,
        argv[2], argv[3], argv[4], NULL
    };
    execv(real_sudo, arguments);
'''
new = '''    char real_sudo[] = "/usr/bin/sudo";
    char non_interactive[] = "-n";
    char separator[] = "--";
    char env_program[] = "/usr/bin/env";
    const char *log_path = getenv("LSM_SAFETY_LOG");
    if (access(real_sudo, X_OK) != 0 || access(env_program, X_OK) != 0 ||
        !log_path)
        return EXIT_FAILURE;
    char log_assignment[LSM_SAFETY_PATH_LEN + 32U];
    const int log_written = snprintf(log_assignment, sizeof(log_assignment),
                                     "LSM_SAFETY_LOG=%s", log_path);
    if (log_written < 0 || (size_t)log_written >= sizeof(log_assignment))
        return EXIT_FAILURE;
    char *arguments[] = {
        real_sudo, non_interactive, separator, env_program, log_assignment,
        argv[2], argv[3], argv[4], NULL
    };
    execv(real_sudo, arguments);
'''
if text.count(old) != 1:
    raise SystemExit("Unexpected mock_sudo execution block")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
