#include <glib.h>
#include "version.h"

static void print_main_help     (const gchar *prg_name);

static void print_show_help     (const gchar *prg_name);

static void print_export_help   (const gchar *prg_name);


gboolean show_help (const gchar *prg_name,
                    const gchar *help_command)
{
    gboolean help_displayed = FALSE;
    if (g_strcmp0 (help_command, "-h") == 0 || g_strcmp0 (help_command, "--help") == 0 || g_strcmp0 (help_command, "help") == 0) {
        print_main_help (prg_name);
        help_displayed = TRUE;
    } else if (g_strcmp0 (help_command, "-v") == 0 || g_strcmp0 (help_command, "--version") == 0) {
        g_print ("%s v%s\n", PROJECT_NAME, PROJECT_VER);
        help_displayed = TRUE;
    } else if (g_strcmp0 (help_command, "--help-show") == 0 || g_strcmp0 (help_command, "help-show") == 0) {
        print_show_help (prg_name);
        help_displayed = TRUE;
    } else if (g_strcmp0 (help_command, "--help-export") == 0 || g_strcmp0 (help_command, "help-export") == 0) {
        print_export_help (prg_name);
        help_displayed = TRUE;
    } else if (help_command == NULL || g_utf8_strlen (help_command, -1) < 2) {
        print_main_help (prg_name);
        help_displayed = TRUE;
    }

    return help_displayed;
}


static void
print_main_help (const gchar *prg_name)
{
    g_print ("Usage:\n  %s <main option> [option 1] [option 2] ...\n", prg_name);
    g_print ("\n");
    g_print ("Help Options:\n");
    g_print ("  -h, --help\t\tShow this help\n");
    g_print ("  --help-show\t\tShow options\n");
    g_print ("  --help-export\t\tExport options\n");
    g_print ("\n");
    g_print ("Main Options:\n");
    g_print ("  -v, --version\t\t\t\tShow program version\n");
    g_print ("  show <-a ..> [-i ..] [-m] [-n]\tShow a token\n");
    g_print ("  list\t\t\t\t\tList all pairs of account and issuer\n");
    g_print ("  export <-t ..> [-d ..]\t\tExport data\n");
    g_print ("\n");
}


static void
print_show_help (const gchar *prg_name)
{
    g_print ("Usage:\n  %s show <-a ..> [-i ..] [-m]\n", prg_name);
    g_print ("\n");
    g_print ("Show Options:\n");
    g_print ("  -a, --account\t\tThe account name (mandatory)\n");
    g_print ("  -i, --issuer\t\tThe issuer name (optional)\n");
    g_print ("  -m, --match-exactly\tShow the token only if it matches exactly the account and/or the issuer (optional)\n");
    g_print ("  -n, --next\tShow also the next token, not only the current one (optional, valid only for TOTP)\n");
    g_print ("\n");
}


static void
print_export_help (const gchar *prg_name)
{
    g_print ("Usage:\n  %s export <-t> <andotp | freeotpplus | aegis> [-d ..]\n", prg_name);
    g_print ("\n");
    g_print ("Export Options:\n");
    g_print ("  -t, --type\t\tExport format. Must be either one of: andotp_plain, andotp_encrypted, freeotpplus, aegis\n");
    g_print ("  -d, --directory\tThe output directory where the exported file will be saved.\n");
    g_print ("\t\t\tIf nothing is specified OR flatpak is being used, the output folder will be the user's HOME directory.\n");
    g_print ("\n");
}
