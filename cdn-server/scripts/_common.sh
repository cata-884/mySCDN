#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[1]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

detect_terminal() {
    if command -v kitty &> /dev/null; then
        TERM_APP="kitty"
    elif command -v gnome-terminal &> /dev/null; then
        TERM_APP="gnome-terminal"
    elif command -v konsole &> /dev/null; then
        TERM_APP="konsole"
    elif command -v xterm &> /dev/null; then
        TERM_APP="xterm"
    else
        echo "Eroare: Niciun terminal compatibil găsit."
        exit 1
    fi
}

launch_in_terminal() {
    local title="$1"
    local cmd="$2"
    local full_cmd="$cmd; echo -e '\n--- STOP ---'; exec bash"

    case "$TERM_APP" in
        "kitty")
            kitty --title "$title" bash -c "$full_cmd" &
            ;;
        "gnome-terminal")
            gnome-terminal --title="$title" -- bash -c "$full_cmd" &
            ;;
        "konsole")
            konsole -p tabtitle="$title" -e bash -c "$full_cmd" &
            ;;
        "xterm")
            xterm -T "$title" -e bash -c "$full_cmd" &
            ;;
    esac
}
