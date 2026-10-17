#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/native_installer_safety.c"
text = path.read_text(encoding="utf-8")
old = '''    char mock_directory[LSM_SAFETY_PATH_LEN];
    parent_path(argv[0], mock_directory, sizeof(mock_directory));
    char marker[LSM_SAFETY_PATH_LEN];
'''
new = '''    char mock_directory[LSM_SAFETY_PATH_LEN];
    const size_t mock_path_length = strlen(argv[0]);
    if (mock_path_length == 0U || mock_path_length >= sizeof(mock_directory))
        return EXIT_FAILURE;
    memcpy(mock_directory, argv[0], mock_path_length + 1U);
    char *mock_slash = strrchr(mock_directory, '/');
    if (!mock_slash) {
        memcpy(mock_directory, ".", 2U);
    } else if (mock_slash == mock_directory) {
        mock_slash[1] = '\0';
    } else {
        *mock_slash = '\0';
    }
    char marker[LSM_SAFETY_PATH_LEN];
'''
if text.count(old) != 1:
    raise SystemExit("Unexpected mock directory block")
text = text.replace(old, new, 1)
old_call = '    cleanup_mock_install(mock_path);\n'
if text.count(old_call) != 1:
    raise SystemExit("Unexpected cleanup call")
text = text.replace(old_call, '    cleanup_mock_install(mock_bin);\n', 1)
path.write_text(text, encoding="utf-8")
