#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path


def replace_once(text, old, new, path):
    if old not in text:
        raise SystemExit(f"expected text not found in {path}: {old[:80]!r}")
    if text.count(old) != 1:
        raise SystemExit(f"expected exactly one match in {path}: {old[:80]!r}")
    return text.replace(old, new, 1)


def function_block(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == '{':
            depth += 1
        elif text[index] == '}':
            depth -= 1
            if depth == 0:
                end = index + 1
                while end < len(text) and text[end] == '\n':
                    end += 1
                return start, end, text[start:end]
    raise SystemExit(f"unterminated function: {signature}")


for name in ('src/application_catalog.c',
             'support/tests/application_catalog_smoke.c'):
    path = Path(name)
    source = path.read_text()
    source = source.replace('#include "app_internal.h"\n', '')
    path.write_text(source)

path = Path('src/app_internal.h')
source = path.read_text()
marker = ('/** Widgets and graph state associated with one side-pane device '
          'entry. */\ntypedef struct {')
source = replace_once(
    source, marker,
    '/** Widgets and graph state associated with one side-pane device entry. */\n'
    'typedef struct LsmDevicePage {', str(path))
path.write_text(source)

path = Path('src/performance_present.h')
source = path.read_text()
source = replace_once(
    source, '#include "app_internal.h"\n',
    '#include "app.h"\n\ntypedef struct LsmDevicePage LsmDevicePage;\n',
    str(path))
path.write_text(source)

path = Path('CMakeLists.txt')
source = path.read_text()
old = '''    else()\n        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")\n        execute_process(\n            COMMAND "${LSM_GIT_EXECUTABLE}" clone --depth 1 --branch\n                    "${INFILTRATR_COMMON_TAG}" "${INFILTRATR_COMMON_URL}"\n                    "${INFILTRATR_COMMON_DIR}"\n            RESULT_VARIABLE LSM_COMMON_FETCH_RESULT)\n    endif()\n'''
new = '''    else()\n        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")\n        execute_process(\n            COMMAND "${LSM_GIT_EXECUTABLE}" clone "${INFILTRATR_COMMON_URL}"\n                    "${INFILTRATR_COMMON_DIR}"\n            RESULT_VARIABLE LSM_COMMON_FETCH_RESULT)\n        if(LSM_COMMON_FETCH_RESULT EQUAL 0)\n            execute_process(\n                COMMAND "${LSM_GIT_EXECUTABLE}" -C "${INFILTRATR_COMMON_DIR}"\n                        checkout --detach "${INFILTRATR_COMMON_EXPECTED_COMMIT}"\n                RESULT_VARIABLE LSM_COMMON_FETCH_RESULT)\n        endif()\n    endif()\n'''
source = replace_once(source, old, new, str(path))
path.write_text(source)

path = Path('Makefile')
source = path.read_text()
old = '''\t\telse \\\n\t\t\tmkdir -p "$(dir $(INFILTRATR_COMMON_DIR))"; \\\n\t\t\tgit clone --depth 1 --branch "$(INFILTRATR_COMMON_TAG)" \\\n\t\t\t\t"$(INFILTRATR_COMMON_URL)" "$(INFILTRATR_COMMON_DIR)"; \\\n\t\tfi; \\\n'''
new = '''\t\telse \\\n\t\t\tmkdir -p "$(dir $(INFILTRATR_COMMON_DIR))"; \\\n\t\t\tgit clone "$(INFILTRATR_COMMON_URL)" "$(INFILTRATR_COMMON_DIR)"; \\\n\t\t\tgit -C "$(INFILTRATR_COMMON_DIR)" checkout --detach \\\n\t\t\t\t"$(INFILTRATR_COMMON_COMMIT)"; \\\n\t\tfi; \\\n'''
source = replace_once(source, old, new, str(path))
path.write_text(source)

# Split menus/actions from window/navigation policy without changing behaviour.
path = Path('src/app_shell.c')
original = path.read_text()
menu_marker = ('/* Menu callbacks contain presentation policy only; feature '
               'modules own data. */')
css_marker = 'void lsm_app_shell_apply_css(void)'
menu_start = original.index(menu_marker)
css_start = original.index(css_marker)
menu_region = original[menu_start:css_start]
shell_region = original[css_start:]

compact_start, compact_end, compact = function_block(
    menu_region, 'void lsm_app_shell_apply_compact_summary(LsmApp *app)')
menu_region = menu_region[:compact_start] + menu_region[compact_end:]
menu_region = menu_region.replace(
    'static void on_save_snapshot(GtkMenuItem *item, gpointer user_data)',
    'void lsm_app_menu_save_snapshot(GtkMenuItem *item, gpointer user_data)', 1)
menu_region = menu_region.replace(
    'static void on_refresh(GtkMenuItem *item, gpointer user_data)',
    'void lsm_app_menu_refresh(GtkMenuItem *item, gpointer user_data)', 1)
menu_region = menu_region.replace(
    'G_CALLBACK(on_save_snapshot)', 'G_CALLBACK(lsm_app_menu_save_snapshot)')
menu_region = menu_region.replace(
    'G_CALLBACK(on_refresh)', 'G_CALLBACK(lsm_app_menu_refresh)')
menu_region = menu_region.replace(
    'GtkWidget *lsm_app_shell_build_menu(LsmApp *app)',
    'GtkWidget *lsm_app_menu_build(LsmApp *app)', 1)

menu_header = '''// SPDX-License-Identifier: GPL-3.0-or-later\n/**\n * @file app_menu.c\n * @brief Global menu construction and user-invoked application actions.\n *\n * @author Shannon Smith\n * @copyright Copyright (c) 2026 Shannon Smith\n * @license GPL-3.0-or-later\n */\n#include "app_menu.h"\n#include "app_internal.h"\n#include "app_runtime.h"\n#include "app_shell.h"\n\n#include "details_page.h"\n#include "help.h"\n#include "process_export.h"\n#include "process_inspector.h"\n#include "project_info.h"\n#include "preferences.h"\n#include "system_snapshot.h"\n#include "task_launcher.h"\n#include "ui_helpers.h"\n\n#include <stdbool.h>\n#include <stdio.h>\n\n'''
Path('src/app_menu.c').write_text(menu_header + menu_region)

shell_header = '''// SPDX-License-Identifier: GPL-3.0-or-later\n/**\n * @file app_shell.c\n * @brief Global window state, navigation, keyboard policy and shell styling.\n *\n * @author Shannon Smith\n * @copyright Copyright (c) 2026 Shannon Smith\n * @license GPL-3.0-or-later\n */\n#include "app_shell.h"\n#include "app_internal.h"\n#include "app_menu.h"\n#include "app_runtime.h"\n\n#include "details_page.h"\n#include "filesystems.h"\n#include "history.h"\n#include "performance.h"\n#include "preferences.h"\n#include "process_export.h"\n#include "processes_ui.h"\n#include "services.h"\n#include "startup.h"\n#include "users.h"\n\n'''
shell_region = shell_region.replace(
    'on_refresh(NULL, app);', 'lsm_app_menu_refresh(NULL, app);')
shell_region = shell_region.replace(
    'on_save_snapshot(NULL, app);', 'lsm_app_menu_save_snapshot(NULL, app);')
path.write_text(shell_header + compact + shell_region)

Path('src/app_menu.h').write_text('''// SPDX-License-Identifier: GPL-3.0-or-later\n/**\n * @file app_menu.h\n * @brief Internal menu and user-action coordination API.\n *\n * @author Shannon Smith\n * @copyright Copyright (c) 2026 Shannon Smith\n * @license GPL-3.0-or-later\n */\n#ifndef LINUX_SYSTEM_MONITOR_APP_MENU_H\n#define LINUX_SYSTEM_MONITOR_APP_MENU_H\n\n#include "app.h"\n\nGtkWidget *lsm_app_menu_build(LsmApp *app);\nvoid lsm_app_menu_refresh(GtkMenuItem *item, gpointer user_data);\nvoid lsm_app_menu_save_snapshot(GtkMenuItem *item, gpointer user_data);\n\n#endif\n''')

path = Path('src/app_shell.h')
source = path.read_text()
start = source.index('/**\n * Build the global menu bar and bind application actions.')
end = source.index('/** Install the application CSS provider', start)
source = source[:start] + source[end:]
path.write_text(source)

path = Path('src/app.c')
source = path.read_text()
source = replace_once(source, '#include "app_internal.h"\n',
                      '#include "app_internal.h"\n#include "app_menu.h"\n',
                      str(path))
source = replace_once(source, 'lsm_app_shell_build_menu(app)',
                      'lsm_app_menu_build(app)', str(path))
path.write_text(source)

path = Path('support/sources.txt')
source = path.read_text()
source = replace_once(source, 'app.c\napp_runtime.c\napp_shell.c\n',
                      'app.c\napp_runtime.c\napp_menu.c\napp_shell.c\n',
                      str(path))
path.write_text(source)
