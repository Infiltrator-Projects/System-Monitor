// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Minimal GTK 3 declarations used only by `make gui-syntax-check` on hosts
 * without GTK development headers. Production builds use the distribution's
 * gtk+-3.0 headers through pkg-config.
 */
#ifndef LSM_MINIMAL_GTK3_H
#define LSM_MINIMAL_GTK3_H
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int gboolean;
typedef int gint;
typedef unsigned int guint;
typedef long glong;
typedef unsigned long gulong;
typedef int64_t gint64;
typedef float gfloat;
typedef uint64_t guint64;
typedef size_t gsize;
typedef ptrdiff_t gssize;
typedef uint8_t guint8;
typedef uint32_t guint32;
typedef char gchar;
typedef void *gpointer;
typedef const void *gconstpointer;
typedef unsigned long GType;
typedef unsigned int GQuark;
typedef int GApplicationFlags;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define G_SOURCE_CONTINUE TRUE
#define G_SOURCE_REMOVE FALSE
#define G_TIME_SPAN_SECOND ((gint64)1000000)
#define G_APPLICATION_FLAGS_NONE 0
#define G_APPLICATION_DEFAULT_FLAGS 0
#define GLIB_CHECK_VERSION(major, minor, micro) 1
#define GDK_CURRENT_TIME 0
#define GDK_BUTTON_PRESS 4
#define GDK_BUTTON_PRESS_MASK (1 << 8)
#define G_N_ELEMENTS(arr) (sizeof(arr)/sizeof((arr)[0]))
#define G_GNUC_PRINTF(a,b) __attribute__((format(printf,a,b)))
#define G_CALLBACK(f) ((GCallback)(f))
#define G_OBJECT(o) ((GObject *)(o))
#define G_APPLICATION(o) ((GApplication *)(o))
#define G_TASK(o) ((GTask *)(o))
#define GINT_TO_POINTER(i) ((gpointer)(intptr_t)(i))
#define GPOINTER_TO_INT(p) ((gint)(intptr_t)(p))
#define GUINT_TO_POINTER(u) ((gpointer)(uintptr_t)(u))
#define GPOINTER_TO_UINT(p) ((guint)(uintptr_t)(p))
#define g_new0(type,n) ((type*)calloc((n), sizeof(type)))

#define G_TYPE_BOOLEAN ((GType)20)
#define G_TYPE_INT ((GType)24)
#define G_TYPE_INT64 ((GType)40)
#define G_TYPE_UINT ((GType)28)
#define G_TYPE_UINT64 ((GType)44)
#define G_TYPE_DOUBLE ((GType)60)
#define G_TYPE_STRING ((GType)64)
#define GDK_SHIFT_MASK (1U << 0)
#define GDK_CONTROL_MASK (1U << 2)
#define GDK_MOD1_MASK (1U << 3)
#define GDK_WINDOW_STATE_MAXIMIZED (1U << 2)
#define GDK_KEY_F5 0xffc2U
#define GDK_KEY_f 0x066U
#define GDK_KEY_F 0x046U
#define GDK_KEY_c 0x063U
#define GDK_KEY_C 0x043U
#define GDK_KEY_s 0x073U
#define GDK_KEY_S 0x053U
#define GDK_KEY_1 0x031U
#define GDK_KEY_2 0x032U
#define GDK_KEY_3 0x033U
#define GDK_KEY_4 0x034U
#define GDK_KEY_5 0x035U
#define GDK_KEY_6 0x036U
#define GDK_KEY_7 0x037U
#define GDK_KEY_8 0x038U
#define GDK_KEY_space 0x020U
#define GDK_KEY_Return 0xff0dU
#define GDK_KEY_KP_Enter 0xff8dU
#define GDK_KEY_Delete 0xffffU

typedef void (*GCallback)(void);
typedef gboolean (*GSourceFunc)(gpointer);
typedef void (*GDestroyNotify)(gpointer);
typedef void (*GSpawnChildSetupFunc)(gpointer);
typedef enum {
    G_SPAWN_DEFAULT = 0,
    G_SPAWN_LEAVE_DESCRIPTORS_OPEN = 1 << 0,
    G_SPAWN_DO_NOT_REAP_CHILD = 1 << 1,
    G_SPAWN_SEARCH_PATH = 1 << 2
} GSpawnFlags;

typedef struct _GObject GObject;
typedef struct _GApplication GApplication;
typedef struct _GError { GQuark domain; gint code; gchar *message; } GError;
typedef struct _GPtrArray { gpointer *pdata; guint len; } GPtrArray;
typedef struct _GString { gchar *str; gsize len; gsize allocated_len; } GString;
typedef struct _GHashTable GHashTable;
typedef struct { gpointer dummy[8]; } GHashTableIter;
typedef struct _GKeyFile GKeyFile;
typedef struct _GVariant GVariant;
typedef struct _GVariantIter { gpointer dummy[8]; } GVariantIter;
typedef struct _GVariantBuilder GVariantBuilder;
typedef struct _GVariantType GVariantType;
typedef struct _GDBusConnection GDBusConnection;
typedef struct _GCancellable GCancellable;
typedef struct _GThread GThread;
typedef struct { gpointer dummy[2]; } GMutex;
typedef struct { gpointer dummy[2]; } GCond;
typedef struct _GAsyncResult GAsyncResult;
typedef struct _GTask GTask;
typedef void (*GAsyncReadyCallback)(GObject *, GAsyncResult *, gpointer);
typedef void (*GTaskThreadFunc)(GTask *, gpointer, gpointer, GCancellable *);
typedef struct _GSList { gpointer data; struct _GSList *next; } GSList;
typedef struct _GList {
    gpointer data;
    struct _GList *next;
    struct _GList *prev;
} GList;

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkApplication GtkApplication;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkBox GtkBox;
typedef struct _GtkButton GtkButton;
typedef struct _GtkContainer GtkContainer;
typedef struct _GtkGrid GtkGrid;
typedef struct _GtkLabel GtkLabel;
typedef struct _GtkMenuItem GtkMenuItem;
typedef struct _GtkCheckMenuItem GtkCheckMenuItem;
typedef struct _GtkRadioMenuItem GtkRadioMenuItem;
typedef struct _GtkMenuShell GtkMenuShell;
typedef struct _GtkNotebook GtkNotebook;
typedef struct _GtkPaned GtkPaned;
typedef struct _GtkScrolledWindow GtkScrolledWindow;
typedef struct _GtkAdjustment GtkAdjustment;
typedef struct _GtkStack GtkStack;
typedef struct _GtkStatusbar GtkStatusbar;
typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GtkComboBox GtkComboBox;
typedef struct _GtkComboBoxText GtkComboBoxText;
typedef struct _GtkClipboard GtkClipboard;
typedef struct _GtkMenu GtkMenu;
typedef struct _GtkEntry GtkEntry;
typedef struct _GtkEditable GtkEditable;
typedef struct _GtkDialog GtkDialog;
typedef struct _GtkMessageDialog GtkMessageDialog;
typedef struct _GtkFileChooser GtkFileChooser;
typedef struct _GtkFileFilter GtkFileFilter;
typedef struct _GtkCssProvider GtkCssProvider;
typedef struct _GtkStyleProvider GtkStyleProvider;
typedef struct _GtkStyleContext GtkStyleContext;
typedef struct _GtkDrawingArea GtkDrawingArea;
typedef struct _GtkCellRenderer GtkCellRenderer;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;
typedef struct _GtkTreeModel GtkTreeModel;
typedef struct _GtkTreeModelFilter GtkTreeModelFilter;
typedef struct _GtkTreeModelSort GtkTreeModelSort;
typedef struct _GtkTreeSortable GtkTreeSortable;
typedef struct _GtkTreeSelection GtkTreeSelection;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkListStore GtkListStore;
typedef struct _GtkTreeStore GtkTreeStore;
typedef struct _GtkTextView GtkTextView;
typedef struct _GtkTextBuffer GtkTextBuffer;
typedef struct _GdkScreen GdkScreen;
typedef struct _GdkEventButton {
    int type; void *window; int8_t send_event; guint32 time; double x, y;
    void *axes; unsigned int state; unsigned int button; void *device; double x_root, y_root;
} GdkEventButton;
typedef struct _GdkEventConfigure {
    int type; void *window; int8_t send_event; int x, y, width, height;
} GdkEventConfigure;
typedef struct _GdkEventKey {
    int type; void *window; int8_t send_event; guint32 time;
    unsigned int state; unsigned int keyval;
} GdkEventKey;
typedef struct _GdkEventWindowState {
    int type; void *window; int8_t send_event;
    unsigned int changed_mask;
    unsigned int new_window_state;
} GdkEventWindowState;
typedef union _GdkEvent { int type; GdkEventButton button; } GdkEvent;
typedef void *GdkAtom;
typedef struct _PangoAttrList PangoAttrList;
typedef struct _PangoAttribute PangoAttribute;
typedef struct _cairo cairo_t;

typedef struct _GtkAllocation { gint x, y, width, height; } GtkAllocation;
typedef struct _GdkRGBA { double red, green, blue, alpha; } GdkRGBA;
typedef struct _GtkTreeIter { gint stamp; gpointer user_data, user_data2, user_data3; } GtkTreeIter;
typedef struct _GtkTextIter {
    gpointer dummy1, dummy2; gint dummy3, dummy4, dummy5, dummy6, dummy7, dummy8;
    gpointer dummy9, dummy10; gint dummy11, dummy12, dummy13; gpointer dummy14;
} GtkTextIter;

typedef gboolean (*GtkTreeModelFilterVisibleFunc)(GtkTreeModel *, GtkTreeIter *, gpointer);
typedef void (*GtkTreeCellDataFunc)(GtkTreeViewColumn *, GtkCellRenderer *, GtkTreeModel *, GtkTreeIter *, gpointer);
typedef void (*GtkTreeViewMappingFunc)(GtkTreeView *, GtkTreePath *, gpointer);

typedef enum { GTK_ORIENTATION_HORIZONTAL=0, GTK_ORIENTATION_VERTICAL=1 } GtkOrientation;
typedef enum { GTK_ALIGN_FILL=0, GTK_ALIGN_START=1, GTK_ALIGN_END=2, GTK_ALIGN_CENTER=3, GTK_ALIGN_BASELINE=4 } GtkAlign;
typedef enum { GTK_POLICY_ALWAYS=0, GTK_POLICY_AUTOMATIC=1, GTK_POLICY_NEVER=2, GTK_POLICY_EXTERNAL=3 } GtkPolicyType;
typedef enum { GTK_POS_LEFT=0, GTK_POS_RIGHT=1, GTK_POS_TOP=2, GTK_POS_BOTTOM=3 } GtkPositionType;
typedef enum { GTK_WINDOW_TOPLEVEL=0, GTK_WINDOW_POPUP=1 } GtkWindowType;
typedef enum { GTK_WIN_POS_NONE=0, GTK_WIN_POS_CENTER=1, GTK_WIN_POS_MOUSE=2, GTK_WIN_POS_CENTER_ALWAYS=3, GTK_WIN_POS_CENTER_ON_PARENT=4 } GtkWindowPosition;
typedef enum { GTK_DIALOG_MODAL=1<<0, GTK_DIALOG_DESTROY_WITH_PARENT=1<<1, GTK_DIALOG_USE_HEADER_BAR=1<<2 } GtkDialogFlags;
typedef enum { GTK_MESSAGE_INFO=0, GTK_MESSAGE_WARNING=1, GTK_MESSAGE_QUESTION=2, GTK_MESSAGE_ERROR=3, GTK_MESSAGE_OTHER=4 } GtkMessageType;
typedef enum { GTK_BUTTONS_NONE=0, GTK_BUTTONS_OK=1, GTK_BUTTONS_CLOSE=2, GTK_BUTTONS_CANCEL=3, GTK_BUTTONS_YES_NO=4, GTK_BUTTONS_OK_CANCEL=5 } GtkButtonsType;
typedef enum { GTK_FILE_CHOOSER_ACTION_OPEN=0, GTK_FILE_CHOOSER_ACTION_SAVE=1, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER=2, GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER=3 } GtkFileChooserAction;
typedef enum { GTK_STACK_TRANSITION_TYPE_NONE=0, GTK_STACK_TRANSITION_TYPE_CROSSFADE=1 } GtkStackTransitionType;
typedef enum { GTK_STATE_FLAG_NORMAL=0 } GtkStateFlags;
typedef enum {
    GTK_SORT_ASCENDING=0,
    GTK_SORT_DESCENDING=1
} GtkSortType;
typedef enum {
    GTK_TREE_VIEW_COLUMN_GROW_ONLY=0,
    GTK_TREE_VIEW_COLUMN_AUTOSIZE=1,
    GTK_TREE_VIEW_COLUMN_FIXED=2
} GtkTreeViewColumnSizing;
typedef enum { G_BUS_TYPE_STARTER=-1, G_BUS_TYPE_NONE=0, G_BUS_TYPE_SYSTEM=1, G_BUS_TYPE_SESSION=2 } GBusType;
typedef enum { G_DBUS_CALL_FLAGS_NONE=0 } GDBusCallFlags;
typedef enum { G_KEY_FILE_NONE=0, G_KEY_FILE_KEEP_COMMENTS=1<<0, G_KEY_FILE_KEEP_TRANSLATIONS=1<<1 } GKeyFileFlags;
#define G_FILE_ERROR (g_file_error_quark())
#define G_VARIANT_TYPE(s) ((const GVariantType *)(s))
typedef enum { GTK_LICENSE_UNKNOWN=0, GTK_LICENSE_CUSTOM=1, GTK_LICENSE_GPL_2_0=2, GTK_LICENSE_GPL_3_0=3, GTK_LICENSE_LGPL_2_1=4, GTK_LICENSE_LGPL_3_0=5, GTK_LICENSE_BSD=6 } GtkLicense;
typedef enum { PANGO_ELLIPSIZE_NONE=0, PANGO_ELLIPSIZE_START=1, PANGO_ELLIPSIZE_MIDDLE=2, PANGO_ELLIPSIZE_END=3 } PangoEllipsizeMode;
typedef enum { GTK_WRAP_NONE=0, GTK_WRAP_CHAR=1, GTK_WRAP_WORD=2, GTK_WRAP_WORD_CHAR=3 } GtkWrapMode;
#define PANGO_WEIGHT_BOLD 700
#define PANGO_SCALE 1024
#define GTK_STYLE_PROVIDER_PRIORITY_APPLICATION 600
#define GTK_RESPONSE_NONE -1
#define GTK_RESPONSE_REJECT -2
#define GTK_RESPONSE_ACCEPT -3
#define GTK_RESPONSE_DELETE_EVENT -4
#define GTK_RESPONSE_OK -5
#define GTK_RESPONSE_CANCEL -6
#define GTK_RESPONSE_CLOSE -7
#define GTK_RESPONSE_YES -8
#define GTK_RESPONSE_NO -9

#define GTK_WINDOW(o) ((GtkWindow*)(o))
#define GTK_BOX(o) ((GtkBox*)(o))
#define GTK_BUTTON(o) ((GtkButton*)(o))
#define GTK_CONTAINER(o) ((GtkContainer*)(o))
#define GTK_GRID(o) ((GtkGrid*)(o))
#define GTK_LABEL(o) ((GtkLabel*)(o))
#define GTK_MENU_ITEM(o) ((GtkMenuItem*)(o))
#define GTK_CHECK_MENU_ITEM(o) ((GtkCheckMenuItem*)(o))
#define GTK_RADIO_MENU_ITEM(o) ((GtkRadioMenuItem*)(o))
#define GTK_MENU_SHELL(o) ((GtkMenuShell*)(o))
#define GTK_NOTEBOOK(o) ((GtkNotebook*)(o))
#define GTK_PANED(o) ((GtkPaned*)(o))
#define GTK_SCROLLED_WINDOW(o) ((GtkScrolledWindow*)(o))
#define GTK_STACK(o) ((GtkStack*)(o))
#define GTK_STATUSBAR(o) ((GtkStatusbar*)(o))
#define GTK_TOGGLE_BUTTON(o) ((GtkToggleButton*)(o))
#define GTK_COMBO_BOX(o) ((GtkComboBox*)(o))
#define GTK_COMBO_BOX_TEXT(o) ((GtkComboBoxText*)(o))
#define GTK_MENU(o) ((GtkMenu*)(o))
#define GTK_ENTRY(o) ((GtkEntry*)(o))
#define GTK_DIALOG(o) ((GtkDialog*)(o))
#define GTK_MESSAGE_DIALOG(o) ((GtkMessageDialog*)(o))
#define GTK_FILE_CHOOSER(o) ((GtkFileChooser*)(o))
#define GTK_STYLE_PROVIDER(o) ((GtkStyleProvider*)(o))
#define GTK_TEXT_VIEW(o) ((GtkTextView*)(o))
#define GTK_TREE_VIEW(o) ((GtkTreeView*)(o))
#define GTK_TREE_MODEL(o) ((GtkTreeModel*)(o))
#define GTK_TREE_MODEL_FILTER(o) ((GtkTreeModelFilter*)(o))
#define GTK_TREE_MODEL_SORT(o) ((GtkTreeModelSort*)(o))
#define GTK_TREE_SORTABLE(o) ((GtkTreeSortable*)(o))

/* GLib */
void g_application_quit(GApplication *application);
int g_application_run(GApplication *application, int argc, char **argv);
gchar *g_build_filename(const gchar *first, ...);
void g_error_free(GError *error);
gboolean g_file_get_contents(const gchar *filename, gchar **contents, gsize *length, GError **error);
gboolean g_file_set_contents(const gchar *filename, const gchar *contents, gssize length, GError **error);
gchar *g_filename_to_uri(const gchar *filename, const gchar *hostname, GError **error);
void g_free(gpointer mem);
void g_list_free(GList *list);
gchar *g_path_get_dirname(const gchar *file_name);
const gchar *g_get_home_dir(void);
const gchar *g_get_user_config_dir(void);
gchar *g_markup_printf_escaped(const gchar *format, ...);
int g_mkdir_with_parents(const gchar *pathname, int mode);
gpointer g_object_get_data(gpointer object, const gchar *key);
void g_object_set(gpointer object, const gchar *first_property_name, ...);
void g_object_set_data(gpointer object, const gchar *key, gpointer data);
gpointer g_object_ref(gpointer object);
void g_object_unref(gpointer object);
#define g_clear_object(object_ptr) do { \
    if (*(object_ptr)) { g_object_unref(*(object_ptr)); *(object_ptr) = NULL; } \
} while (0)
void g_ptr_array_add(GPtrArray *array, gpointer data);
gpointer *g_ptr_array_free(GPtrArray *array, gboolean free_segment);
GPtrArray *g_ptr_array_new(void);
GPtrArray *g_ptr_array_new_with_free_func(GDestroyNotify free_func);
void g_ptr_array_set_size(GPtrArray *array, gint length);
void g_ptr_array_sort(GPtrArray *array,
                      gint (*compare_func)(gconstpointer, gconstpointer));
guint g_str_hash(gconstpointer v);
gboolean g_str_equal(gconstpointer v1, gconstpointer v2);
GHashTable *g_hash_table_new(guint (*hash_func)(gconstpointer), gboolean (*key_equal_func)(gconstpointer,gconstpointer));
GHashTable *g_hash_table_new_full(guint (*hash_func)(gconstpointer), gboolean (*key_equal_func)(gconstpointer,gconstpointer), GDestroyNotify key_destroy_func, GDestroyNotify value_destroy_func);
void g_hash_table_destroy(GHashTable *hash_table);
gboolean g_hash_table_add(GHashTable *hash_table, gpointer key);
void g_hash_table_insert(GHashTable *hash_table, gpointer key, gpointer value);
void g_hash_table_replace(GHashTable *hash_table, gpointer key, gpointer value);
gpointer g_hash_table_lookup(GHashTable *hash_table, gconstpointer key);
gboolean g_hash_table_contains(GHashTable *hash_table, gconstpointer key);
void g_hash_table_iter_init(GHashTableIter *iter, GHashTable *hash_table);
gboolean g_hash_table_iter_next(GHashTableIter *iter, gpointer *key, gpointer *value);
guint g_hash_table_foreach_remove(GHashTable *hash_table, gboolean (*func)(gpointer,gpointer,gpointer), gpointer user_data);
void g_hash_table_remove_all(GHashTable *hash_table);
guint g_direct_hash(gconstpointer v);
gboolean g_direct_equal(gconstpointer v1, gconstpointer v2);
#define g_ptr_array_index(array,index) ((array)->pdata[(index)])
gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GDestroyNotify destroy_data, int connect_flags);
#define g_signal_connect(instance,signal,handler,data) g_signal_connect_data((instance),(signal),G_CALLBACK(handler),(data),NULL,0)
gboolean g_source_remove(guint tag); guint g_idle_add(GSourceFunc function, gpointer data);
gchar *g_strdup(const gchar *str);
gchar *g_strdup_printf(const gchar *format, ...);
guint g_strv_length(gchar **str_array);
double g_ascii_strtod(const gchar *nptr, gchar **endptr);
guint64 g_ascii_strtoull(const gchar *nptr, gchar **endptr, guint base);
gint64 g_ascii_strtoll(const gchar *nptr, gchar **endptr, guint base);
gchar *g_strdup_vprintf(const gchar *format, va_list args);
const gchar *g_strerror(gint errnum);
void g_strfreev(gchar **str_array);
GString *g_string_append_printf(GString *string, const gchar *format, ...);
GString *g_string_append(GString *string, const gchar *value);
gchar *g_string_free(GString *string, gboolean free_segment);
GString *g_string_new(const gchar *init);
gsize g_strlcpy(gchar *dest, const gchar *src, gsize dest_size);
gchar **g_strsplit(const gchar *string, const gchar *delimiter, gint max_tokens);
gboolean g_shell_parse_argv(const gchar *command_line, gint *argcp,
                            gchar ***argvp, GError **error);
gboolean g_spawn_async(const gchar *working_directory, gchar **argv,
                       gchar **envp, GSpawnFlags flags,
                       GSpawnChildSetupFunc child_setup, gpointer user_data,
                       gint *child_pid, GError **error);
guint g_timeout_add(guint interval, GSourceFunc function, gpointer data);
guint g_timeout_add_seconds(guint interval, GSourceFunc function, gpointer data);
gchar *g_utf8_casefold(const gchar *str, gssize len);


static inline gboolean lsm_g_ascii_isalnum(gchar c) {
    unsigned char u = (unsigned char)c;
    return (u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z');
}
static inline gchar *lsm_g_strstrip(gchar *string) {
    if (!string) return NULL;
    gchar *start = string;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' || *start == '\f' || *start == '\v') start++;
    if (start != string) { gchar *d = string; while ((*d++ = *start++)) {} }
    gsize len = 0; while (string[len]) len++;
    while (len && (string[len-1] == ' ' || string[len-1] == '\t' || string[len-1] == '\n' || string[len-1] == '\r' || string[len-1] == '\f' || string[len-1] == '\v')) string[--len] = '\0';
    return string;
}
#define g_ascii_isalnum(c) lsm_g_ascii_isalnum(c)
#define g_strstrip(s) lsm_g_strstrip(s)

GQuark g_file_error_quark(void);
gint g_file_error_from_errno(gint err_no);
void g_set_error(GError **err, GQuark domain, gint code, const gchar *format, ...);
void g_clear_error(GError **err);
GKeyFile *g_key_file_new(void);
void g_key_file_free(GKeyFile *key_file);
gboolean g_key_file_load_from_file(GKeyFile *key_file, const gchar *file, GKeyFileFlags flags, GError **error);
gchar *g_key_file_get_locale_string(GKeyFile *key_file, const gchar *group_name, const gchar *key, const gchar *locale, GError **error);
gboolean g_key_file_get_boolean(GKeyFile *key_file, const gchar *group_name, const gchar *key, GError **error);
void g_key_file_set_boolean(GKeyFile *key_file, const gchar *group_name, const gchar *key, gboolean value);
gchar *g_key_file_to_data(GKeyFile *key_file, gsize *length, GError **error);
gboolean g_once_init_enter(volatile void *location);
void g_once_init_leave(volatile void *location, gsize result);
void g_mutex_init(GMutex *mutex);
void g_mutex_lock(GMutex *mutex);
void g_mutex_unlock(GMutex *mutex);
void g_cond_init(GCond *condition);
gboolean g_cond_wait_until(GCond *condition, GMutex *mutex, gint64 end_time);
void g_cond_broadcast(GCond *condition);
GThread *g_thread_new(const gchar *name, gpointer (*function)(gpointer), gpointer data);
gpointer g_thread_join(GThread *thread);
gint64 g_get_monotonic_time(void);
GDBusConnection *g_bus_get_sync(GBusType bus_type, GCancellable *cancellable, GError **error);
GVariant *g_dbus_connection_call_sync(GDBusConnection *connection, const gchar *bus_name,
    const gchar *object_path, const gchar *interface_name, const gchar *method_name,
    GVariant *parameters, const void *reply_type, GDBusCallFlags flags,
    gint timeout_msec, GCancellable *cancellable, GError **error);
GCancellable *g_cancellable_new(void);
void g_cancellable_cancel(GCancellable *cancellable);
gboolean g_cancellable_is_cancelled(GCancellable *cancellable);
GTask *g_task_new(GObject *source_object, GCancellable *cancellable,
                  GAsyncReadyCallback callback, gpointer callback_data);
void g_task_set_task_data(GTask *task, gpointer task_data,
                          GDestroyNotify task_data_destroy);
void g_task_run_in_thread(GTask *task, GTaskThreadFunc task_func);
void g_task_return_pointer(GTask *task, gpointer result,
                           GDestroyNotify result_destroy);
gpointer g_task_propagate_pointer(GTask *task, GError **error);
gboolean g_main_context_iteration(gpointer context, gboolean may_block);
GVariant *g_variant_parse(const GVariantType *type, const gchar *text, const gchar *limit, const gchar **endptr, GError **error);
GVariant *g_variant_new(const gchar *format_string, ...);
GVariant *g_variant_ref_sink(GVariant *value);
GVariant *g_variant_new_string(const gchar *string);
GVariant *g_variant_new_boolean(gboolean value);
GVariant *g_variant_new_uint32(guint32 value);
GVariant *g_variant_new_uint64(guint64 value);
GVariantBuilder *g_variant_builder_new(const GVariantType *type);
void g_variant_builder_add(GVariantBuilder *builder, const gchar *format_string, ...);
GVariant *g_variant_builder_end(GVariantBuilder *builder);
void g_variant_builder_unref(GVariantBuilder *builder);
void g_variant_get(GVariant *value, const gchar *format_string, ...);
GVariant *g_variant_get_child_value(GVariant *value, gsize index_);
gboolean g_variant_lookup(GVariant *dictionary, const gchar *key, const gchar *format_string, ...);
GVariant *g_variant_lookup_value(GVariant *dictionary, const gchar *key, const void *expected_type);
gconstpointer g_variant_get_fixed_array(GVariant *value, gsize *n_elements, gsize element_size);
const gchar *g_variant_get_string(GVariant *value, gsize *length);
gboolean g_variant_get_boolean(GVariant *value);
guint32 g_variant_get_uint32(GVariant *value);
guint64 g_variant_get_uint64(GVariant *value);
gboolean g_variant_is_of_type(GVariant *value, const GVariantType *type);
void g_variant_iter_init(GVariantIter *iter, GVariant *value);
gboolean g_variant_iter_next(GVariantIter *iter, const gchar *format_string, ...);
GVariantIter *g_variant_iter_new(GVariant *value);
gboolean g_variant_iter_loop(GVariantIter *iter, const gchar *format_string, ...);
void g_variant_iter_free(GVariantIter *iter);
void g_variant_unref(GVariant *value);

/* GDK/Pango/Cairo */
gboolean gdk_rgba_parse(GdkRGBA *rgba, const gchar *spec);
GdkScreen *gdk_screen_get_default(void);
GdkAtom gdk_atom_intern_static_string(const gchar *atom_name);
PangoAttrList *pango_attr_list_new(void);
void pango_attr_list_insert(PangoAttrList *list, PangoAttribute *attr);
void pango_attr_list_unref(PangoAttrList *list);
PangoAttribute *pango_attr_size_new(gint size);
PangoAttribute *pango_attr_weight_new(int weight);
void cairo_new_path(cairo_t *cr); void cairo_move_to(cairo_t *cr,double x,double y); void cairo_line_to(cairo_t *cr,double x,double y); void cairo_close_path(cairo_t *cr); void cairo_fill(cairo_t *cr); void cairo_stroke(cairo_t *cr); void cairo_rectangle(cairo_t *cr,double x,double y,double width,double height); void cairo_set_dash(cairo_t *cr,const double *dashes,int num_dashes,double offset); void cairo_set_line_width(cairo_t *cr,double width); void cairo_set_source_rgba(cairo_t *cr,double red,double green,double blue,double alpha);

/* GTK */
GtkApplication *gtk_application_new(const gchar *application_id, int flags);
GtkWidget *gtk_application_window_new(GtkApplication *application);
GtkWidget *gtk_box_new(GtkOrientation orientation, gint spacing);
void gtk_box_pack_start(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding);
void gtk_box_pack_end(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding);
GtkWidget *gtk_button_new(void); GtkWidget *gtk_button_new_with_label(const gchar *label); void gtk_button_set_label(GtkButton*, const gchar*);
GtkWidget *gtk_check_button_new_with_label(const gchar *label);
GtkCellRenderer *gtk_cell_renderer_pixbuf_new(void);
GtkCellRenderer *gtk_cell_renderer_text_new(void);
GtkClipboard *gtk_clipboard_get(GdkAtom selection); void gtk_clipboard_set_text(GtkClipboard*,const gchar*,gint);
gboolean gtk_check_menu_item_get_active(GtkCheckMenuItem*); GtkWidget *gtk_check_menu_item_new_with_mnemonic(const gchar*); GtkWidget *gtk_check_menu_item_new_with_label(const gchar*); void gtk_check_menu_item_set_active(GtkCheckMenuItem*,gboolean);
GtkWidget *gtk_combo_box_text_new(void); void gtk_combo_box_text_append_text(GtkComboBoxText*,const gchar*); void gtk_combo_box_text_remove_all(GtkComboBoxText*); gint gtk_combo_box_get_active(GtkComboBox*); void gtk_combo_box_set_active(GtkComboBox*,gint);
void gtk_container_add(GtkContainer*,GtkWidget*); void gtk_container_set_border_width(GtkContainer*,guint);
gboolean gtk_css_provider_load_from_data(GtkCssProvider*,const gchar*,gssize,GError**); GtkCssProvider *gtk_css_provider_new(void);
void gtk_dialog_add_buttons(GtkDialog*,const gchar*,...); GtkWidget *gtk_dialog_get_content_area(GtkDialog*); GtkWidget *gtk_dialog_new_with_buttons(const gchar*,GtkWindow*,GtkDialogFlags,const gchar*,...); gint gtk_dialog_run(GtkDialog*);
GtkWidget *gtk_drawing_area_new(void);
GtkWidget *gtk_entry_new(void); const gchar *gtk_entry_get_text(GtkEntry*); void gtk_entry_set_text(GtkEntry*,const gchar*); void gtk_entry_set_placeholder_text(GtkEntry*,const gchar*);
void gtk_file_chooser_add_filter(GtkFileChooser*,GtkFileFilter*); GtkWidget *gtk_file_chooser_dialog_new(const gchar*,GtkWindow*,GtkFileChooserAction,const gchar*,...); gchar *gtk_file_chooser_get_filename(GtkFileChooser*); gboolean gtk_file_chooser_set_current_folder(GtkFileChooser*,const gchar*); void gtk_file_chooser_set_current_name(GtkFileChooser*,const gchar*); void gtk_file_chooser_set_do_overwrite_confirmation(GtkFileChooser*,gboolean);
void gtk_file_filter_add_pattern(GtkFileFilter*,const gchar*); GtkFileFilter *gtk_file_filter_new(void); void gtk_file_filter_set_name(GtkFileFilter*,const gchar*);
GtkWidget *gtk_frame_new(const gchar*);
void gtk_grid_attach(GtkGrid*,GtkWidget*,gint,gint,gint,gint); GtkWidget *gtk_grid_new(void); void gtk_grid_set_column_spacing(GtkGrid*,guint); void gtk_grid_set_row_spacing(GtkGrid*,guint);
GtkWidget *gtk_label_new(const gchar*); void gtk_label_set_selectable(GtkLabel*,gboolean); void gtk_label_set_attributes(GtkLabel*,PangoAttrList*); void gtk_label_set_ellipsize(GtkLabel*,PangoEllipsizeMode); void gtk_label_set_line_wrap(GtkLabel*,gboolean); void gtk_label_set_markup(GtkLabel*,const gchar*); void gtk_label_set_text(GtkLabel*,const gchar*); const gchar *gtk_label_get_text(GtkLabel*);
void gtk_list_store_append(GtkListStore*,GtkTreeIter*); void gtk_list_store_clear(GtkListStore*); GtkListStore *gtk_list_store_new(gint n_columns,...); void gtk_list_store_set(GtkListStore*,GtkTreeIter*,...);
GtkWidget *gtk_menu_bar_new(void); void gtk_menu_popup_at_pointer(GtkMenu*,const GdkEvent*); GtkWidget *gtk_menu_item_new_with_label(const gchar*); GtkWidget *gtk_menu_item_new_with_mnemonic(const gchar*); void gtk_menu_item_set_label(GtkMenuItem*,const gchar*); void gtk_menu_item_set_submenu(GtkMenuItem*,GtkWidget*); GtkWidget *gtk_menu_new(void); void gtk_menu_shell_append(GtkMenuShell*,GtkWidget*);
void gtk_message_dialog_format_secondary_text(GtkMessageDialog*,const gchar*,...); GtkWidget *gtk_message_dialog_new(GtkWindow*,GtkDialogFlags,GtkMessageType,GtkButtonsType,const gchar*,...);
gint gtk_notebook_append_page(GtkNotebook*,GtkWidget*,GtkWidget*); gint gtk_notebook_get_current_page(GtkNotebook*); GtkWidget *gtk_notebook_new(void); void gtk_notebook_set_current_page(GtkNotebook*,gint); void gtk_notebook_set_tab_pos(GtkNotebook*,GtkPositionType);
GtkWidget *gtk_paned_new(GtkOrientation); void gtk_paned_pack1(GtkPaned*,GtkWidget*,gboolean,gboolean); void gtk_paned_pack2(GtkPaned*,GtkWidget*,gboolean,gboolean); void gtk_paned_set_position(GtkPaned*,gint);
GSList *gtk_radio_menu_item_get_group(GtkRadioMenuItem*); GtkWidget *gtk_radio_menu_item_new_with_label(GSList*,const gchar*); GtkWidget *gtk_radio_menu_item_new_with_label_from_widget(GtkRadioMenuItem*,const gchar*);
GtkWidget *gtk_scrolled_window_new(gpointer,gpointer); void gtk_scrolled_window_set_policy(GtkScrolledWindow*,GtkPolicyType,GtkPolicyType); void gtk_scrolled_window_set_propagate_natural_width(GtkScrolledWindow*,gboolean); void gtk_scrolled_window_set_propagate_natural_height(GtkScrolledWindow*,gboolean); GtkAdjustment *gtk_scrolled_window_get_vadjustment(GtkScrolledWindow*); double gtk_adjustment_get_value(GtkAdjustment*); void gtk_adjustment_set_value(GtkAdjustment*,double);
GtkWidget *gtk_search_entry_new(void); GtkWidget *gtk_separator_menu_item_new(void); GtkWidget *gtk_separator_new(GtkOrientation);
void gtk_show_about_dialog(GtkWindow*,const gchar*,...); gboolean gtk_show_uri_on_window(GtkWindow*,const gchar*,guint32,GError**);
void gtk_stack_add_named(GtkStack*,GtkWidget*,const gchar*); const gchar *gtk_stack_get_visible_child_name(GtkStack*); GtkWidget *gtk_stack_new(void); void gtk_stack_set_homogeneous(GtkStack*,gboolean); void gtk_stack_set_transition_duration(GtkStack*,guint); void gtk_stack_set_transition_type(GtkStack*,GtkStackTransitionType); void gtk_stack_set_visible_child_name(GtkStack*,const gchar*);
GtkWidget *gtk_statusbar_new(void); guint gtk_statusbar_push(GtkStatusbar*,guint,const gchar*);
void gtk_style_context_add_provider_for_screen(GdkScreen*,GtkStyleProvider*,guint); gboolean gtk_style_context_lookup_color(GtkStyleContext*,const gchar*,GdkRGBA*); void gtk_style_context_get_background_color(GtkStyleContext*,GtkStateFlags,GdkRGBA*);
void gtk_text_buffer_get_bounds(GtkTextBuffer*,GtkTextIter*,GtkTextIter*); gchar *gtk_text_buffer_get_text(GtkTextBuffer*,const GtkTextIter*,const GtkTextIter*,gboolean); void gtk_text_buffer_set_text(GtkTextBuffer*,const gchar*,gint); GtkTextBuffer *gtk_text_view_get_buffer(GtkTextView*); GtkWidget *gtk_text_view_new(void); void gtk_text_view_set_monospace(GtkTextView*,gboolean); void gtk_text_view_set_wrap_mode(GtkTextView*,GtkWrapMode); void gtk_text_view_set_editable(GtkTextView*,gboolean); void gtk_text_view_set_cursor_visible(GtkTextView*,gboolean);
gboolean gtk_toggle_button_get_active(GtkToggleButton*); GtkWidget *gtk_toggle_button_new(void); GtkWidget *gtk_toggle_button_new_with_label(const gchar*); void gtk_toggle_button_set_active(GtkToggleButton*,gboolean);
GtkTreePath *gtk_tree_model_filter_convert_child_path_to_path(GtkTreeModelFilter*,GtkTreePath*); GtkTreeModel *gtk_tree_model_filter_new(GtkTreeModel*,GtkTreePath*); void gtk_tree_model_filter_refilter(GtkTreeModelFilter*); void gtk_tree_model_filter_set_visible_func(GtkTreeModelFilter*,GtkTreeModelFilterVisibleFunc,gpointer,GDestroyNotify);
void gtk_tree_model_get(GtkTreeModel*,GtkTreeIter*,...); gboolean gtk_tree_model_get_iter(GtkTreeModel*,GtkTreeIter*,GtkTreePath*); gboolean gtk_tree_model_get_iter_first(GtkTreeModel*,GtkTreeIter*); gboolean gtk_tree_model_iter_next(GtkTreeModel*,GtkTreeIter*); gboolean gtk_tree_model_iter_children(GtkTreeModel*,GtkTreeIter*,GtkTreeIter*); GtkTreePath *gtk_tree_model_get_path(GtkTreeModel*,GtkTreeIter*);
GtkTreePath *gtk_tree_model_sort_convert_child_path_to_path(GtkTreeModelSort*,GtkTreePath*); GtkTreeModel *gtk_tree_model_sort_new_with_model(GtkTreeModel*);
void gtk_tree_path_free(GtkTreePath*); gboolean gtk_tree_selection_get_selected(GtkTreeSelection*,GtkTreeModel**,GtkTreeIter*); void gtk_tree_selection_select_path(GtkTreeSelection*,GtkTreePath*);
GtkTreeModel *gtk_tree_view_get_model(GtkTreeView*); GList *gtk_tree_view_get_columns(GtkTreeView*); gboolean gtk_tree_view_get_path_at_pos(GtkTreeView*,gint,gint,GtkTreePath**,GtkTreeViewColumn**,gint*,gint*); void gtk_tree_view_expand_all(GtkTreeView*); void gtk_tree_view_map_expanded_rows(GtkTreeView*,GtkTreeViewMappingFunc,gpointer); gboolean gtk_tree_view_expand_row(GtkTreeView*,GtkTreePath*,gboolean); void gtk_tree_view_move_column_after(GtkTreeView*,GtkTreeViewColumn*,GtkTreeViewColumn*); void gtk_tree_view_scroll_to_cell(GtkTreeView*,GtkTreePath*,GtkTreeViewColumn*,gboolean,gfloat,gfloat); void gtk_tree_view_set_enable_tree_lines(GtkTreeView*,gboolean); void gtk_tree_view_set_show_expanders(GtkTreeView*,gboolean);
gint gtk_tree_view_append_column(GtkTreeView*,GtkTreeViewColumn*); gint gtk_tree_view_column_get_width(GtkTreeViewColumn*); GtkTreeViewColumn *gtk_tree_view_column_new(void); GtkTreeViewColumn *gtk_tree_view_column_new_with_attributes(const gchar*,GtkCellRenderer*,const gchar*,gint,...); void gtk_tree_view_column_pack_start(GtkTreeViewColumn*,GtkCellRenderer*,gboolean); void gtk_tree_view_column_set_cell_data_func(GtkTreeViewColumn*,GtkCellRenderer*,GtkTreeCellDataFunc,gpointer,GDestroyNotify); void gtk_tree_view_column_set_expand(GtkTreeViewColumn*,gboolean); void gtk_tree_view_column_set_fixed_width(GtkTreeViewColumn*,gint); void gtk_tree_view_column_set_min_width(GtkTreeViewColumn*,gint); void gtk_tree_view_column_set_resizable(GtkTreeViewColumn*,gboolean); void gtk_tree_view_column_set_sizing(GtkTreeViewColumn*,GtkTreeViewColumnSizing); void gtk_tree_view_column_set_sort_column_id(GtkTreeViewColumn*,gint);
gboolean gtk_tree_sortable_get_sort_column_id(GtkTreeSortable*,gint*,GtkSortType*); void gtk_tree_sortable_set_sort_column_id(GtkTreeSortable*,gint,GtkSortType); void gtk_tree_view_column_set_title(GtkTreeViewColumn*,const gchar*); void gtk_tree_view_column_set_visible(GtkTreeViewColumn*,gboolean); gboolean gtk_tree_view_column_get_visible(GtkTreeViewColumn*);
GtkTreeStore *gtk_tree_store_new(gint n_columns,...); void gtk_tree_store_append(GtkTreeStore*,GtkTreeIter*,GtkTreeIter*); void gtk_tree_store_clear(GtkTreeStore*); void gtk_tree_store_set(GtkTreeStore*,GtkTreeIter*,...);
GtkTreeSelection *gtk_tree_view_get_selection(GtkTreeView*); GtkWidget *gtk_tree_view_new_with_model(GtkTreeModel*); void gtk_tree_view_set_enable_search(GtkTreeView*,gboolean); void gtk_tree_view_set_headers_clickable(GtkTreeView*,gboolean);
void gtk_widget_add_events(GtkWidget*,gint); void gtk_widget_destroy(GtkWidget*); void gtk_widget_get_allocation(GtkWidget*,GtkAllocation*); GtkWidget *gtk_widget_get_parent(GtkWidget*); void gtk_widget_queue_resize(GtkWidget*); gboolean gtk_widget_get_mapped(GtkWidget*); GtkStyleContext *gtk_widget_get_style_context(GtkWidget*); void gtk_widget_grab_focus(GtkWidget*); void gtk_widget_queue_draw(GtkWidget*); void gtk_widget_set_halign(GtkWidget*,GtkAlign); void gtk_widget_set_valign(GtkWidget*,GtkAlign); void gtk_widget_set_hexpand(GtkWidget*,gboolean); void gtk_widget_set_margin_bottom(GtkWidget*,gint); void gtk_widget_set_margin_start(GtkWidget*,gint); void gtk_widget_set_margin_top(GtkWidget*,gint); void gtk_widget_set_name(GtkWidget*,const gchar*); void gtk_widget_set_no_show_all(GtkWidget*,gboolean); void gtk_widget_set_sensitive(GtkWidget*,gboolean); void gtk_widget_set_size_request(GtkWidget*,gint,gint); void gtk_widget_set_tooltip_text(GtkWidget*,const gchar*); void gtk_widget_set_vexpand(GtkWidget*,gboolean); void gtk_widget_set_visible(GtkWidget*,gboolean); void gtk_widget_show_all(GtkWidget*);
GtkWidget *gtk_window_get_focus(GtkWindow*); void gtk_window_get_size(GtkWindow*,gint*,gint*); gboolean gtk_window_is_maximized(GtkWindow*); void gtk_window_maximize(GtkWindow*); void gtk_window_unmaximize(GtkWindow*); void gtk_window_resize(GtkWindow*,gint,gint); void gtk_window_set_keep_above(GtkWindow*,gboolean); GtkWidget *gtk_window_new(GtkWindowType); void gtk_window_present(GtkWindow*); void gtk_window_set_default_size(GtkWindow*,gint,gint); void gtk_window_set_icon_name(GtkWindow*,const gchar*); void gtk_window_set_position(GtkWindow*,GtkWindowPosition); void gtk_window_set_title(GtkWindow*,const gchar*); void gtk_window_set_transient_for(GtkWindow*,GtkWindow*); void gtk_window_set_destroy_with_parent(GtkWindow*,gboolean);

#ifdef __cplusplus
}
#endif
#endif
