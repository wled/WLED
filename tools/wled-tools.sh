#!/bin/bash

# =====================
# wled-toolbox.sh - A script for managing WLED devices
# =====================

# Color Definitions
GREEN="\e[32m"
RED="\e[31m"
BLUE="\e[34m"
YELLOW="\e[33m"
RESET="\e[0m"

# Path to backup directory
backup_dir="./"

# Firmware file (if provided)
firmware_file=""

# Logging function
log() {
    local category="$1"
    local color="$2"
    local text="$3"

    if [ "$quiet" = true ]; then
        return
    fi

    if [ -t 1 ]; then  # Check if output is a terminal
        echo -e "${color}[${category}]${RESET} ${text}"
    else
        echo "[${category}] ${text}"
    fi
}

# Generic curl handler function
curl_handler() {
    local command="$1"
    local hostname="$2"

    response=$($command -w "%{http_code}" -o /dev/null)
    curl_exit_code=$?

    if [ "$response" -ge 200 ] && [ "$response" -lt 300 ]; then
        return 0
    elif [ $curl_exit_code -ne 0 ]; then
        log "ERROR" "$RED" "Connection error during request to $hostname (curl exit code: $curl_exit_code)."
        return 1
    elif [ "$response" -ge 400 ]; then
        log "ERROR" "$RED" "Server error during request to $hostname (HTTP status code: $response)."
        return 2
    else
        log "ERROR" "$RED" "Unexpected response from $hostname (HTTP status code: $response)."
        return 3
    fi
}

# Print help message
show_help() {
    cat << EOF
Usage: wled-tools.sh [OPTIONS] COMMAND [ARGS...]

Options:
  -h, --help              Show this help message and exit.
  -v, --verbose           Enable verbose output for debugging.
  -t, --target <IP/Host>  Specify a single WLED device by IP address or hostname.
  -D, --discover          Discover multiple WLED devices using mDNS.
  -d, --directory <Path>  Specify a directory for saving backups (default: working directory).
  -f, --firmware <File>   Specify the firmware file for updating devices.
  -q, --quiet             Suppress logging output (also makes discover output hostnames only).

Commands:
  backup      Backup the current state of a WLED device or multiple discovered devices.
  update      Update the firmware of a WLED device or multiple discovered devices.
  discover    Discover WLED devices using mDNS and list their IP addresses and names.

EOF
}

# Discover devices using mDNS
discover_devices() {
    if ! command -v avahi-browse &> /dev/null; then
        log "ERROR" "$RED" "'avahi-browse' is required but not installed."
        exit 1
    fi

    mapfile -t hostnames < <(avahi-browse _wled._tcp --terminate -r -p | awk -F';' '/^=/ {print $7}')
    if [ "$quiet" = true ]; then
        for hostname in "${hostnames[@]}"; do
            echo "$hostname"
        done
    else
        printf "%s\n" "${hostnames[@]}" | sort -u
    fi
}

# Backup one device
backup_one() {
    local hostname="$1"

    log "INFO" "$YELLOW" "Backing up device config/presets: $hostname"

    mkdir -p "$backup_dir"

    local cfg_url="http://$hostname/cfg.json"
    local presets_url="http://$hostname/presets.json"
    local cfg_dest="${backup_dir}/${hostname}.cfg.json"
    local presets_dest="${backup_dir}/${hostname}.presets.json"

    # Write to ".tmp" files first, then move when success, to ensure we don't write partial files
    local curl_command_cfg="curl -s "$cfg_url" -o "$cfg_dest.tmp""
    local curl_command_presets="curl -s "$presets_url" -o "$presets_dest.tmp""

    curl_handler "$curl_command_cfg" "$hostname"
    curl_handler "$curl_command_presets" "$hostname"

    mv "$cfg_dest.tmp" "$cfg_dest"
    mv "$presets_dest.tmp" "$presets_dest"
}

# Update one device
update_one() {
    local hostname="$1"
    local firmware="$2"

    log "INFO" "$YELLOW" "Starting firmware update for device: $hostname"

    if [ -z "$firmware" ]; then
        log "ERROR" "$RED" "Firmware file not specified."
        exit 1
    fi

    local url="http://$hostname/update"
    local curl_command="curl -s -X POST -F "file=@$firmware" "$url""

    curl_handler "$curl_command" "$hostname"
}

# Command-line arguments processing
command=""
target=""
discover=false
quiet=false

if [ $# -eq 0 ]; then
    show_help
    exit 0
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            show_help
            exit 0
            ;;
        -v|--verbose)
            verbose=true
            shift
            ;;
        -t|--target)
            target="$2"
            shift 2
            ;;
        -D|--discover)
            discover=true
            shift
            ;;
        -d|--directory)
            backup_dir="$2"
            shift 2
            ;;
        -f|--firmware)
            firmware_file="$2"
            shift 2
            ;;
        -q|--quiet)
            quiet=true
            shift
            ;;
        backup|update|discover)
            command="$1"
            shift
            ;;
        *)
            log "ERROR" "$RED" "Unknown argument: $1"
            exit 1
            ;;
    esac

done

# Execute the appropriate command
case "$command" in
    discover)
        discover_devices
        ;;
    backup)
        if [ -n "$target" ]; then
            backup_one "$target"
        elif [ "$discover" = true ]; then
            for hostname in $(discover_devices); do
                backup_one "$hostname"
            done
        else
            log "ERROR" "$RED" "No target specified. Use --target or --discover."
            exit 1
        fi
        ;;
    update)
        if [ -n "$target" ]; then
            update_one "$target" "$firmware_file"
        elif [ "$discover" = true ]; then
            for hostname in $(discover_devices); do
                update_one "$hostname" "$firmware_file"
            done
        else
            log "ERROR" "$RED" "No target specified. Use --target or --discover."
            exit 1
        fi
        ;;
    *)
        show_help
        exit 1
        ;;
esac
