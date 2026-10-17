#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/native_installer_safety.c"
text = path.read_text(encoding="utf-8")

old_marker = '''    const char *source_root = getenv("LSM_SAFETY_SOURCE");
    if (!source_root) return EXIT_FAILURE;
    make_directories("/Programs", 0755);
    char executable[LSM_SAFETY_PATH_LEN];
    if (!realpath(argv[0], executable)) return EXIT_FAILURE;
    copy_regular_file(executable, "/Programs/system-monitor", 0755);

    char marker[LSM_SAFETY_PATH_LEN];
    if (!join_path(marker, sizeof(marker), source_root,
                   ".mock-installed-system-monitor"))
        return EXIT_FAILURE;
'''
new_marker = '''    make_directories("/Programs", 0755);
    char executable[LSM_SAFETY_PATH_LEN];
    if (!realpath(argv[0], executable)) return EXIT_FAILURE;
    copy_regular_file(executable, "/Programs/system-monitor", 0755);

    /* Keep fixture ownership state beside the isolated mock command set. This
     * remains deterministic across sudo's environment sanitisation and cannot
     * be confused with production package state. */
    char mock_directory[LSM_SAFETY_PATH_LEN];
    parent_path(argv[0], mock_directory, sizeof(mock_directory));
    char marker[LSM_SAFETY_PATH_LEN];
    if (!join_path(marker, sizeof(marker), mock_directory,
                   ".mock-installed-system-monitor"))
        return EXIT_FAILURE;
'''
if text.count(old_marker) != 1:
    raise SystemExit("Unexpected mock_dpkg marker block")
text = text.replace(old_marker, new_marker, 1)

start = text.index('static void cleanup_mock_install(')
end = text.index('\nstatic int dispatch_mock(', start)
new_cleanup = '''static void cleanup_mock_install(const char *mock_path)
{
    char marker[LSM_SAFETY_PATH_LEN];
    if (!join_path(marker, sizeof(marker), mock_path,
                   ".mock-installed-system-monitor") ||
        access(marker, F_OK) != 0)
        return;

    for (size_t index = 0U; index < 2U; index++) {
        const pid_t child = fork();
        if (child < 0) fail_errno("fork", NULL);
        if (child == 0) {
            if (index == 0U) {
                execl("/usr/bin/sudo", "/usr/bin/sudo", "-n", "--",
                      "/usr/bin/rm", "-f", "/Programs/system-monitor",
                      (char *)NULL);
            } else {
                execl("/usr/bin/sudo", "/usr/bin/sudo", "-n", "--",
                      "/usr/bin/rmdir", "/Programs", (char *)NULL);
            }
            _exit(126);
        }
        int status = 0;
        while (waitpid(child, &status, 0) < 0) {
            if (errno == EINTR) continue;
            fail_errno("waitpid", NULL);
        }
        if (index == 0U &&
            (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
            fputs("Failed to remove fixture-installed /Programs/system-monitor.\\n",
                  stderr);
            exit(EXIT_FAILURE);
        }
    }
    if (unlink(marker) != 0) fail_errno("unlink", marker);
}
'''
text = text[:start] + new_cleanup + text[end:]

old_call = '    cleanup_mock_install(source_root);\n'
if text.count(old_call) != 1:
    raise SystemExit("Unexpected fixture cleanup call")
text = text.replace(old_call, '    cleanup_mock_install(mock_path);\n', 1)

path.write_text(text, encoding="utf-8")
