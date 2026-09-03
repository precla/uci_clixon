/*
 * uci_interfaces_cli.c - Clixon CLI plugin for ietf-interfaces
 *
 * Provides the custom expand function `expand_ifs()` used in system.cli:
 *   set interfaces interface <name:string expand_ifs()>
 *
 * The function enumerates the network interfaces present on the system
 * (from /sys/class/net) and returns them as completion candidates so that
 * pressing `?` after "set interfaces interface" lists all available
 * interfaces on the router.
 *
 * Cross-compiled for OpenWrt (aarch64) - see Makefile.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

#include <cligen/cligen.h>
#include <clixon/clixon.h>

/*
 * Expand function called from the cligen spec via `expand_ifs()`.
 *
 * Arguments follow the clixon expand convention:
 *   h         Clixon client handle (unused here)
 *   name      Name of the variable being expanded
 *   cvv       Volume of previous variables (unused)
 *   argv      Volume of previous commands (unused)
 *   commands  Vector to append the completion values to
 *   helptexts Vector to append the corresponding help texts to
 *
 * @see expand_dbvar in clixon apps/cli/cli_show.c
 */
int
expand_ifs(void        *h,
           char        *name,
           cvec        *cvv,
           cvec        *argv,
           cvec        *commands,
           cvec        *helptexts)
{
    DIR           *dir;
    struct dirent *de;

    (void)h;
    (void)name;
    (void)cvv;
    (void)argv;

    if ((dir = opendir("/sys/class/net")) == NULL)
        return -1;

    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.')
            continue;               /* skip ., .. and hidden */
        if (cvec_add_string(commands, 0, de->d_name) < 0) {
            closedir(dir);
            return -1;
        }
        if (helptexts != NULL &&
            cvec_add_string(helptexts, 0, "Network interface") < 0) {
            closedir(dir);
            return -1;
        }
    }
    closedir(dir);
    return 0;
}

/* Plugin init - required entry point for all clixon plugins.
 * See CLIXON_PLUGIN_INIT in clixon_plugin.h
 */
clixon_plugin_api *
clixon_plugin_init(clixon_handle h)
{
    static clixon_plugin_api api;

    (void)h;
    memset(&api, 0, sizeof(api));
    strncpy(api.ca_name, "uci_interfaces_cli", sizeof(api.ca_name) - 1);

    return &api;
}