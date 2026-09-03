/*
 * Clixon backend plugin: UCI hostname
 *
 * Maps ietf-system /system/hostname to UCI system.@system[0].hostname
 *
 * On commit:    writes hostname changes to UCI and applies immediately.
 * On statedata: populates system-state/platform from /etc/openwrt_release.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include <cligen/cligen.h>
#include <clixon/clixon.h>
#include <clixon/clixon_backend_transaction.h>

#define SYSTEM_NS "urn:ietf:params:xml:ns:yang:ietf-system"

/*
 * Run a shell command and log errors.
 */
static int
run_cmd(const char *cmd)
{
    int ret;

    clixon_log(NULL, LOG_DEBUG, "%s: executing: %s", __FUNCTION__, cmd);
    ret = system(cmd);
    if (ret != 0) {
        clixon_log(NULL, LOG_ERR, "%s: command failed (ret=%d): %s",
                   __FUNCTION__, ret, cmd);
        return -1;
    }
    return 0;
}

/*
 * Apply hostname change to OpenWrt via UCI.
 */
static int
apply_hostname(const char *hostname)
{
    char cmd[256];

    clixon_log(NULL, LOG_NOTICE, "%s: setting hostname to '%s'",
               __FUNCTION__, hostname);

    snprintf(cmd, sizeof(cmd),
             "uci set system.@system[0].hostname='%s'", hostname);
    if (run_cmd(cmd) < 0)
        return -1;

    if (run_cmd("uci commit system") < 0)
        return -1;

    /* Apply immediately to the running kernel */
    snprintf(cmd, sizeof(cmd),
             "echo '%s' > /proc/sys/kernel/hostname", hostname);
    if (run_cmd(cmd) < 0)
        return -1;

    return 0;
}

/*
 * Transaction commit callback.
 *
 * Called after successful validation when the candidate is committed
 * to running.  Extract /system/hostname and push it to UCI.
 */
static int
uci_hostname_commit(clixon_handle    h,
                    transaction_data td)
{
    cxobj      *target;
    cxobj      *hnode;
    const char *hostname;

    target = transaction_target(td);
    if (target == NULL)
        return 0;

    /* Find the hostname leaf; since only ietf-system is loaded, an
     * unqualified path is unambiguous. */
    hnode = xpath_first(target, NULL, "system/hostname");
    if (hnode == NULL)
        return 0;

    hostname = xml_body(hnode);
    if (hostname == NULL || strlen(hostname) == 0)
        return 0;

    if (apply_hostname(hostname) < 0) {
        clixon_log(NULL, LOG_ERR,
                   "%s: failed to apply hostname '%s'",
                   __FUNCTION__, hostname);
        return -1;
    }

    return 0;
}

/*
 * State data callback.
 *
 * Populates system-state/platform with live data from the router.
 */
static int
uci_hostname_statedata(clixon_handle h,
                       cvec         *nsc,
                       char         *xpath,
                       cxobj        *xtop)
{
    int    ret = -1;
    cxobj *xstate = NULL;
    FILE  *fp;
    char   buf[256];
    char   os_name[64]    = "OpenWrt";
    char   os_release[64] = "";
    char   os_version[128] = "";
    char   machine[64]    = "";

    /* Only respond to requests that include system-state */
    if (xpath && strstr(xpath, "system-state") == NULL &&
        strcmp(xpath, "/") != 0)
        return 0;

    /* Read OS info from /etc/openwrt_release */
    fp = fopen("/etc/openwrt_release", "r");
    if (fp) {
        while (fgets(buf, sizeof(buf), fp)) {
            if (strncmp(buf, "DISTRIB_ID=", 11) == 0)
                sscanf(buf + 11, "'%63[^']'", os_name);
            else if (strncmp(buf, "DISTRIB_RELEASE=", 16) == 0)
                sscanf(buf + 16, "'%63[^']'", os_release);
            else if (strncmp(buf, "DISTRIB_REVISION=", 17) == 0)
                sscanf(buf + 17, "'%127[^']'", os_version);
        }
        fclose(fp);
    }

    /* Read current kernel hostname */
    fp = fopen("/proc/sys/kernel/hostname", "r");
    if (fp) {
        if (fgets(machine, sizeof(machine), fp))
            machine[strcspn(machine, "\n")] = 0;
        fclose(fp);
    }

    /* Build state XML */
    if (clixon_xml_parse_va(YB_NONE, NULL, &xstate, NULL,
                            "<system-state xmlns=\"" SYSTEM_NS "\">"
                            "  <platform>"
                            "    <os-name>%s</os-name>"
                            "    <os-release>%s</os-release>"
                            "    <os-version>%s</os-version>"
                            "    <machine>%s</machine>"
                            "  </platform>"
                            "</system-state>",
                            os_name, os_release, os_version, machine) < 0)
        goto done;

    if (xml_addsub(xtop, xstate) < 0)
        goto done;

    ret = 0;
done:
    return ret;
}

/*
 * Plugin initialization — entry point called by clixon_backend.
 */
clixon_plugin_api *
clixon_plugin_init(clixon_handle h)
{
    static clixon_plugin_api api = {
        .ca_name         = "uci-hostname",
        .ca_init         = NULL,
        .ca_start        = NULL,
        .ca_exit         = NULL,
        .ca_trans_commit = uci_hostname_commit,
        .ca_statedata    = uci_hostname_statedata,
    };

    clixon_log(NULL, LOG_NOTICE, "%s: plugin loaded", __FUNCTION__);
    return &api;
}
