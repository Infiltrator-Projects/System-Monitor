#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/native_installer_safety.c"
text = path.read_text(encoding="utf-8")

old_dpkg = '''static int mock_dpkg(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--print-architecture") == 0) {
        puts("amd64");
        return EXIT_SUCCESS;
    }
    append_command_log("dpkg", argc, argv);
    if (argc != 3 || strcmp(argv[1], "--install") != 0 ||
        access(argv[2], R_OK) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
'''
new_dpkg = '''static int mock_dpkg(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--print-architecture") == 0) {
        puts("amd64");
        return EXIT_SUCCESS;
    }
    append_command_log("dpkg", argc, argv);
    if (argc != 3 || strcmp(argv[1], "--install") != 0 ||
        access(argv[2], R_OK) != 0)
        return EXIT_FAILURE;

    /* A successful package install now materialises the GUI executable in the
     * native program namespace. The safety fixture must model that filesystem
     * effect before the production installer performs its post-install check. */
    struct stat installed_status;
    if (lstat("/Programs/system-monitor", &installed_status) == 0)
        return EXIT_SUCCESS;
    if (errno != ENOENT) return EXIT_FAILURE;

    const char *source_root = getenv("LSM_SAFETY_SOURCE");
    if (!source_root) return EXIT_FAILURE;
    make_directories("/Programs", 0755);
    char executable[LSM_SAFETY_PATH_LEN];
    if (!realpath(argv[0], executable)) return EXIT_FAILURE;
    copy_regular_file(executable, "/Programs/system-monitor", 0755);

    char marker[LSM_SAFETY_PATH_LEN];
    if (!join_path(marker, sizeof(marker), source_root,
                   ".mock-installed-system-monitor"))
        return EXIT_FAILURE;
    FILE *marker_file = fopen(marker, "wx");
    if (!marker_file) return EXIT_FAILURE;
    if (fputs("fixture-created\\n", marker_file) == EOF ||
        fclose(marker_file) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
'''
if text.count(old_dpkg) != 1:
    raise SystemExit("Unexpected mock_dpkg source; refusing broad edit")
text = text.replace(old_dpkg, new_dpkg, 1)

old_sudo = '''static int mock_sudo(int argc, char **argv)
{
    append_command_log("sudo", argc, argv);
    if (argc != 5 || strcmp(argv[1], "--") != 0 ||
        strcmp(base_name(argv[2]), "dpkg") != 0 ||
        strcmp(argv[3], "--install") != 0)
        return EXIT_FAILURE;
    execv(argv[2], &argv[2]);
    return EXIT_FAILURE;
}
'''
new_sudo = '''static int mock_sudo(int argc, char **argv)
{
    append_command_log("sudo", argc, argv);
    if (argc != 5 || strcmp(argv[1], "--") != 0 ||
        strcmp(base_name(argv[2]), "dpkg") != 0 ||
        strcmp(argv[3], "--install") != 0)
        return EXIT_FAILURE;

    /* Run only the fixture dpkg through the host's non-interactive sudo. This
     * preserves the exact production privilege boundary while allowing the
     * mock package manager to create the root-owned /Programs payload. */
    char real_sudo[] = "/usr/bin/sudo";
    char non_interactive[] = "-n";
    char separator[] = "--";
    if (access(real_sudo, X_OK) != 0) return EXIT_FAILURE;
    char *arguments[] = {
        real_sudo, non_interactive, separator,
        argv[2], argv[3], argv[4], NULL
    };
    execv(real_sudo, arguments);
    return EXIT_FAILURE;
}

static void cleanup_mock_install(const char *source_root)
{
    char marker[LSM_SAFETY_PATH_LEN];
    if (!join_path(marker, sizeof(marker), source_root,
                   ".mock-installed-system-monitor") ||
        access(marker, F_OK) != 0)
        return;

    const char *commands[][4] = {
        {"/usr/bin/rm", "-f", "/Programs/system-monitor", NULL},
        {"/usr/bin/rmdir", "/Programs", NULL, NULL}
    };
    for (size_t index = 0U; index < 2U; index++) {
        const pid_t child = fork();
        if (child < 0) fail_errno("fork", NULL);
        if (child == 0) {
            char *arguments[8];
            size_t count = 0U;
            arguments[count++] = "/usr/bin/sudo";
            arguments[count++] = "-n";
            arguments[count++] = "--";
            for (size_t part = 0U; commands[index][part]; part++)
                arguments[count++] = (char *)commands[index][part];
            arguments[count] = NULL;
            execv("/usr/bin/sudo", arguments);
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
if text.count(old_sudo) != 1:
    raise SystemExit("Unexpected mock_sudo source; refusing broad edit")
text = text.replace(old_sudo, new_sudo, 1)

old_run = '''    const int install_result = run_installer(builder, source_root, home, search_path,
                                             log_path, run_user, run_group, false);
    if (install_result != 0) {
'''
new_run = '''    const int install_result = run_installer(builder, source_root, home, search_path,
                                             log_path, run_user, run_group, false);
    cleanup_mock_install(source_root);
    if (install_result != 0) {
'''
if text.count(old_run) != 1:
    raise SystemExit("Unexpected installer execution source; refusing broad edit")
text = text.replace(old_run, new_run, 1)

path.write_text(text, encoding="utf-8")
