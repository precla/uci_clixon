#!/bin/sh
#
# Deploy the uci_hostname plugin to an OpenWrt router (e.g. Banana Pi R3).
#
# Usage:
#   ./deploy.sh <router-ip>
#
# Prerequisites:
#   - clixon package installed on the router (apk add clixon)
#   - SSH access to the router as root
#   - The backend plugin (uci_hostname.so) must be cross-compiled first
#
# This script opens a single SSH master connection so you only enter
# the password once.  All subsequent ssh/scp calls reuse it.
#

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <router-ip>"
    exit 1
fi

ROUTER="root@$1"
DIR="$(cd "$(dirname "$0")" && pwd)"

# --- Shared SSH connection (password asked once) ---
SSH_SOCKET="/tmp/deploy-ssh-$1.sock"
SSH_OPTS="-o ControlMaster=auto -o ControlPath=$SSH_SOCKET -o ControlPersist=10m"
SSH="ssh $SSH_OPTS"
SCP="scp -O $SSH_OPTS"

cleanup() {
    ssh -O exit $SSH_OPTS "$ROUTER" 2>/dev/null || true
    rm -f "$SSH_SOCKET"
}
trap cleanup EXIT INT TERM

echo "=== Opening SSH master connection (enter password once) ==="
$SSH -o ControlPersist=10m "$ROUTER" true

echo "=== Stopping clixon backend (procd + kill) ==="
$SSH "$ROUTER" "[ -x /etc/init.d/clixon ] && /etc/init.d/clixon stop 2>/dev/null || true"
$SSH "$ROUTER" "[ -x /etc/init.d/clixon ] && /etc/init.d/clixon disable 2>/dev/null || true"
$SSH "$ROUTER" "kill \$(cat /var/run/clixon.pid 2>/dev/null) 2>/dev/null || true"

echo "=== Creating directories ==="
$SSH "$ROUTER" "mkdir -p /etc/clixon /usr/lib/clixon/clispec /usr/lib/clixon/backend /usr/share/clixon /var/clixon"

echo "=== Deploying YANG models ==="
$SCP "$DIR/yang/iana-crypt-hash@2014-08-06.yang" "$ROUTER:/usr/share/clixon/"
$SCP "$DIR/yang/ietf-system@2014-08-06.yang" "$ROUTER:/usr/share/clixon/"
$SCP "$DIR/yang/ietf-yang-types@2013-07-15.yang" "$ROUTER:/usr/share/clixon/"
$SCP "$DIR/yang/ietf-interfaces@2018-02-20.yang" "$ROUTER:/usr/share/clixon/"

echo "=== Deploying CLI spec ==="
$SCP "$DIR/clispec/system.cli" "$ROUTER:/usr/lib/clixon/clispec/"

echo "=== Deploying configuration ==="
$SCP "$DIR/config/clixon.xml" "$ROUTER:/etc/clixon/clixon.xml"

echo "=== Deploying backend plugin ==="
if [ -f "$DIR/backend/uci_hostname.so" ]; then
    $SCP "$DIR/backend/uci_hostname.so" "$ROUTER:/usr/lib/clixon/backend/"
else
    echo "ERROR: uci_hostname.so not found!"
    echo "Build it first with:"
    echo "  cd backend && make"
    exit 1
fi

echo "=== Clearing old datastore ==="
$SSH "$ROUTER" "rm -f /var/clixon/*"

echo "=== Verifying deployment ==="
$SSH "$ROUTER" "grep CLICON_YANG_MODULE_MAIN /etc/clixon/clixon.xml; \
                ls /usr/lib/clixon/clispec/; \
                ls /usr/lib/clixon/backend/; \
                ls /usr/share/clixon/ietf-system*"

echo "=== Starting clixon backend (daemon mode, not via procd) ==="
$SSH "$ROUTER" "clixon_backend -s init -l s"

sleep 1
echo "=== Backend status ==="
$SSH "$ROUTER" "ps w | grep -v grep | grep clixon_backend || echo 'NOT RUNNING - check: logread | tail -20'"

echo "=== Done ==="
echo "Connect with: ssh $1 clixon_cli"
echo "Try:  set system hostname my-router"
echo "Then: commit"
